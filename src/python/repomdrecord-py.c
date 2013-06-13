/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013  Tomas Mlcoch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <Python.h>
#include <assert.h>
#include <stddef.h>

#include "repomdrecord-py.h"
#include "exception-py.h"
#include "typeconversion.h"
#include "contentstat-py.h"

typedef struct {
    PyObject_HEAD
    cr_RepomdRecord *record;
} _RepomdRecordObject;

PyObject *
Object_FromRepomdRecord(cr_RepomdRecord *rec)
{
    PyObject *py_rec;

    if (!rec) {
        PyErr_SetString(PyExc_ValueError, "Expected a cr_RepomdRecord pointer not NULL.");
        return NULL;
    }

    py_rec = PyObject_CallObject((PyObject *) &RepomdRecord_Type, NULL);
    cr_repomd_record_free(((_RepomdRecordObject *)py_rec)->record);
    ((_RepomdRecordObject *)py_rec)->record = rec;

    return py_rec;
}

cr_RepomdRecord *
RepomdRecord_FromPyObject(PyObject *o)
{
    if (!RepomdRecordObject_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Expected a RepomdRecord object.");
        return NULL;
    }
    return ((_RepomdRecordObject *)o)->record;
}

static int
check_RepomdRecordStatus(const _RepomdRecordObject *self)
{
    assert(self != NULL);
    assert(RepomdRecordObject_Check(self));
    if (self->record == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c RepomdRecord object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
repomdrecord_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(args);
    CR_UNUSED(kwds);
    _RepomdRecordObject *self = (_RepomdRecordObject *)type->tp_alloc(type, 0);
    if (self) {
        self->record = NULL;
    }
    return (PyObject *)self;
}

static int
repomdrecord_init(_RepomdRecordObject *self, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(kwds);

    char *type = NULL, *path = NULL;

    if (!PyArg_ParseTuple(args, "|zz:repomdrecord_init", &type, &path))
        return -1;

    /* Free all previous resources when reinitialization */
    if (self->record)
        cr_repomd_record_free(self->record);

    /* Init */
    self->record = cr_repomd_record_new(type, path);
    if (self->record == NULL) {
        PyErr_SetString(CrErr_Exception, "RepomdRecord initialization failed");
        return -1;
    }

    return 0;
}

static void
repomdrecord_dealloc(_RepomdRecordObject *self)
{
    if (self->record)
        cr_repomd_record_free(self->record);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
repomdrecord_repr(_RepomdRecordObject *self)
{
    CR_UNUSED(self);
    if (self->record->type)
        return PyString_FromFormat("<createrepo_c.RepomdRecord %s object>",
                                   self->record->type);
    else
        return PyString_FromFormat("<createrepo_c.RepomdRecord object>");
}

/* RepomdRecord methods */

static PyObject *
copy_repomdrecord(_RepomdRecordObject *self, void *nothing)
{
    CR_UNUSED(nothing);
    if (check_RepomdRecordStatus(self))
        return NULL;
    return Object_FromRepomdRecord(cr_repomd_record_copy(self->record));
}

static PyObject *
fill(_RepomdRecordObject *self, PyObject *args)
{
    int checksum_type;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "i:fill", &checksum_type))
        return NULL;

    if (check_RepomdRecordStatus(self))
        return NULL;

    cr_repomd_record_fill(self->record, checksum_type, &err);
    if (err) {
        nice_exception(&err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
compress_and_fill(_RepomdRecordObject *self, PyObject *args)
{
    int checksum_type, compression_type;
    PyObject *compressed_repomdrecord;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "O!ii:compress_and_fill",
                          &RepomdRecord_Type,
                          &compressed_repomdrecord,
                          &checksum_type,
                          &compression_type))
        return NULL;

    if (check_RepomdRecordStatus(self))
        return NULL;

    cr_repomd_record_compress_and_fill(self->record,
                                       RepomdRecord_FromPyObject(compressed_repomdrecord),
                                       checksum_type,
                                       compression_type,
                                       &err);
    if (err) {
        nice_exception(&err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
rename_file(_RepomdRecordObject *self, void *nothing)
{
    GError *err = NULL;

    CR_UNUSED(nothing);

    cr_repomd_record_rename_file(self->record, &err);
    if (err) {
        nice_exception(&err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
load_contentstat(_RepomdRecordObject *self, PyObject *args)
{
    PyObject *contentstat;

    if (!PyArg_ParseTuple(args, "O!:load_contentstat",
                          &ContentStat_Type,
                          &contentstat))
        return NULL;

    if (check_RepomdRecordStatus(self))
        return NULL;

    cr_repomd_record_load_contentstat(self->record,
                                      ContentStat_FromPyObject(contentstat));
    Py_RETURN_NONE;
}

static struct PyMethodDef repomdrecord_methods[] = {
    {"copy", (PyCFunction)copy_repomdrecord, METH_NOARGS, NULL},
    {"fill", (PyCFunction)fill, METH_VARARGS, NULL},
    {"compress_and_fill", (PyCFunction)compress_and_fill, METH_VARARGS, NULL},
    {"rename_file", (PyCFunction)rename_file, METH_NOARGS, NULL},
    {"load_contentstat", (PyCFunction)load_contentstat, METH_VARARGS, NULL},
    {NULL} /* sentinel */
};

/* getsetters */

#define OFFSET(member) (void *) offsetof(cr_RepomdRecord, member)

static PyObject *
get_num(_RepomdRecordObject *self, void *member_offset)
{
    if (check_RepomdRecordStatus(self))
        return NULL;
    cr_RepomdRecord *rec = self->record;
    gint64 val = *((gint64 *) ((size_t)rec + (size_t) member_offset));
    return PyLong_FromLongLong((long long) val);
}

static PyObject *
get_int(_RepomdRecordObject *self, void *member_offset)
{
    if (check_RepomdRecordStatus(self))
        return NULL;
    cr_RepomdRecord *rec = self->record;
    gint64 val = *((int *) ((size_t)rec + (size_t) member_offset));
    return PyLong_FromLongLong((long long) val);
}

static PyObject *
get_str(_RepomdRecordObject *self, void *member_offset)
{
    if (check_RepomdRecordStatus(self))
        return NULL;
    cr_RepomdRecord *rec = self->record;
    char *str = *((char **) ((size_t) rec + (size_t) member_offset));
    if (str == NULL)
        Py_RETURN_NONE;
    return PyString_FromString(str);
}

static int
set_num(_RepomdRecordObject *self, PyObject *value, void *member_offset)
{
    gint64 val;
    if (check_RepomdRecordStatus(self))
        return -1;
    if (PyLong_Check(value)) {
        val = (gint64) PyLong_AsLong(value);
    } else if (PyInt_Check(value)) {
        val = (gint64) PyInt_AS_LONG(value);
    } else {
        PyErr_SetString(PyExc_TypeError, "Number expected!");
        return -1;
    }
    cr_RepomdRecord *rec = self->record;
    *((gint64 *) ((size_t) rec + (size_t) member_offset)) = val;
    return 0;
}

static int
set_int(_RepomdRecordObject *self, PyObject *value, void *member_offset)
{
    long val;
    if (check_RepomdRecordStatus(self))
        return -1;
    if (PyLong_Check(value)) {
        val = PyLong_AsLong(value);
    } else if (PyInt_Check(value)) {
        val = PyInt_AS_LONG(value);
    } else {
        PyErr_SetString(PyExc_TypeError, "Number expected!");
        return -1;
    }
    cr_RepomdRecord *rec = self->record;
    *((int *) ((size_t) rec + (size_t) member_offset)) = (int) val;
    return 0;
}

static int
set_str(_RepomdRecordObject *self, PyObject *value, void *member_offset)
{
    if (check_RepomdRecordStatus(self))
        return -1;
    if (!PyString_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_TypeError, "String or None expected!");
        return -1;
    }
    cr_RepomdRecord *rec = self->record;
    char *str = cr_safe_string_chunk_insert(rec->chunk,
                                            PyObject_ToStrOrNull(value));
    *((char **) ((size_t) rec + (size_t) member_offset)) = str;
    return 0;
}

static PyGetSetDef repomdrecord_getsetters[] = {
    {"type",                (getter)get_str, (setter)set_str, NULL, OFFSET(type)},
    {"location_real",       (getter)get_str, (setter)set_str, NULL, OFFSET(location_real)},
    {"location_href",       (getter)get_str, (setter)set_str, NULL, OFFSET(location_href)},
    {"location_base",       (getter)get_str, (setter)set_str, NULL, OFFSET(location_base)},
    {"checksum",            (getter)get_str, (setter)set_str, NULL, OFFSET(checksum)},
    {"checksum_type",       (getter)get_str, (setter)set_str, NULL, OFFSET(checksum_type)},
    {"checksum_open",       (getter)get_str, (setter)set_str, NULL, OFFSET(checksum_open)},
    {"checksum_open_type",  (getter)get_str, (setter)set_str, NULL, OFFSET(checksum_open_type)},
    {"timestamp",           (getter)get_num, (setter)set_num, NULL, OFFSET(timestamp)},
    {"size",                (getter)get_num, (setter)set_num, NULL, OFFSET(size)},
    {"size_open",           (getter)get_num, (setter)set_num, NULL, OFFSET(size_open)},
    {"db_ver",              (getter)get_int, (setter)set_int, NULL, OFFSET(db_ver)},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */

PyTypeObject RepomdRecord_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "createrepo_c.RepomdRecord",    /* tp_name */
    sizeof(_RepomdRecordObject),    /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor) repomdrecord_dealloc, /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    (reprfunc) repomdrecord_repr,   /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /* tp_flags */
    "RepomdRecord object",          /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    0,                              /* tp_iternext */
    repomdrecord_methods,           /* tp_methods */
    0,                              /* tp_members */
    repomdrecord_getsetters,        /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc) repomdrecord_init,   /* tp_init */
    0,                              /* tp_alloc */
    repomdrecord_new,               /* tp_new */
    0,                              /* tp_free */
    0,                              /* tp_is_gc */
};
