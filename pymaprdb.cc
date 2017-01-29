#include <Python.h>
#include "structmember.h"
#include <stdio.h>
#include <unistd.h>
#include <hbase/hbase.h>
#include <pthread.h>
#include <string.h>
#include <vector>


#if defined( WIN64 ) || defined( _WIN64 ) || defined( __WIN64__ ) || defined(_WIN32)
#define __WINDOWS__
#endif

#define CHECK_RC_RETURN(rc)          \
    do {                               \
        if (rc) {                        \
            printf("%s:%d Call failed: %d\n", __PRETTY_FUNCTION__, __LINE__, rc); \
        }                                \
    } while (0);


static PyObject *SpamError;

typedef struct {
    // This is a macro, correct with no semi colon, which initializes fields to make it usable as a PyObject
    // Why not define first and last as char * ? Is there any benefit over each way?
    PyObject_HEAD
    PyObject *first;
    PyObject *last;
    int number;
    char *secret;
} Foo;

static void Foo_dealloc(Foo *self) {
    //dispose of your owned references
    //Py_XDECREF is sued because first/last could be NULL
    Py_XDECREF(self->first);
    Py_XDECREF(self->last);
    //call the class tp_free function to clean up the type itself.
    // Note how the Type is PyObject * insteaed of FooType * because the object may be a subclass
    self->ob_type->tp_free((PyObject *) self);

    // Note how there is no XDECREF on self->number
}

static PyObject *Foo_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    // Hm this isn't printing out?
    // Ok Foo_new isn't being called for some reason
    printf("In foo_new\n");
    Foo *self;// == NULL;
    // to_alloc allocates memory
    self = (Foo *)type->tp_alloc(type, 0);
    // One reason to implement a new method is to assure the initial values of instance variables
    // Here we are ensuring they initial values of first and last are not NULL.
    // If we don't care, we ould have used PyType_GenericNew() as the new method, which sets everything to NULL...
    if (self != NULL) {
        printf("in neww self is not null");
        self->first = PyString_FromString("");
        if (self->first == NULL) {
            Py_DECREF(self);
            return NULL;
        }

        self->last = PyString_FromString("");
        if (self->last == NULL) {
            Py_DECREF(self);
            return NULL;
        }

        self->number = 0;
    }


    // What about self->secret ?

    if (self->first == NULL) {
        printf("in new self first is null\n");
    } else {
        printf("in new self first is not null\n");
    }

    return (PyObject *) self;
}

static int Foo_init(Foo *self, PyObject *args, PyObject *kwargs) {
    //char *name;
    printf("In foo_init\n");
    PyObject *first, *last, *tmp;
    // Note how we can use &self->number, but not &self->first
    if (!PyArg_ParseTuple(args, "SSi", &first, &last, &self->number)) {
        //return NULL;
        return -1;
    }
    // What is the point of tmp?
    // The docs say we should always reassign members before decrementing their reference counts

    if (last) {
        tmp = self->last;
        Py_INCREF(last);
        self->last = last;
        Py_DECREF(tmp);
    }

    if (first) {
        tmp = self->first;
        Py_INCREF(first);
        self->first = first;
        //This was changed to DECREF from XDECREF once the get_first/last were set
        // This is because the get_first/last guarantee that it isn't null
        // but it caused a segmentation fault wtf?
        // Ok that was because the new method wasn't working bug
        Py_DECREF(tmp);
    }


    // Should I incref this?
    self->secret = "secret lol";
    printf("Finished foo_init");
    return 0;
}

/*
import spam
spam.Foo('a','b',5)
*/


// Make data available to Python
static PyMemberDef Foo_members[] = {
    //{"first", T_OBJECT_EX, offsetof(Foo, first), 0, "first name"},
    //{"last", T_OBJECT_EX, offsetof(Foo, last), 0, "last name"},
    {"number", T_INT, offsetof(Foo, number), 0, "number"},
    {NULL}
};

static PyObject *Foo_get_first(Foo *self, void *closure) {
    Py_INCREF(self->first);
    return self->first;
}

static int Foo_set_first(Foo *self, PyObject *value, void *closure) {
    printf("IN foo_set_first\n");
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the first attribute");
        return -1;
    }

    if (!PyString_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "The first attribute value must be a string");
        return -1;
    }

    Py_DECREF(self->first);
    Py_INCREF(value);
    self->first = value;
    printf("finished foo_set_first\n");
    return 0;
}

static PyObject *Foo_get_last(Foo *self, void *closure) {
    Py_INCREF(self->last);
    return self->last;
}

static int Foo_set_last(Foo *self, PyObject *value, void *closure) {
    printf("IN foo_set_last\n");
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the last attribute");
        return -1;
    }

    if (!PyString_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "The last attribute must be a string");
        return -1;
    }

    Py_DECREF(self->last);
    Py_INCREF(value);
    self->last = value;
    printf("finished foo_set_last\n");
    return 0;
}

static PyGetSetDef Foo_getseters[] = {
    {"first", (getter) Foo_get_first, (setter) Foo_set_first, "first name", NULL},
    {"last", (getter) Foo_get_last, (setter) Foo_set_last, "last name", NULL},
    {NULL}
};

static PyObject *Foo_square(Foo *self) {
    return Py_BuildValue("i", self->number * self->number);
}

static PyObject * Foo_name(Foo *self) {
    static PyObject *format = NULL;

    PyObject *args, *result;

    // We have to check for NULL, because they can be deleted, in which case they are set to NULL.
    // It would be better to prevent deletion of these attributes and to restrict the attribute values to strings.
    if (format == NULL) {
        format = PyString_FromString("%s %s");
        if (format == NULL) {
            return NULL;
        }
    }
    /*
    // These checks can be removed after adding the getter/setter that guarentees it cannot be null
    if (self->first == NULL) {
        PyErr_SetString(PyExc_AttributeError, "first");
        return NULL;
    }

    if (self->last == NULL) {
        PyErr_SetString(PyExc_AttributeError, "last");
        return NULL;
    }
    */

    args = Py_BuildValue("OO", self->first, self->last);
    if (args == NULL) {
        return NULL;
    }

    result = PyString_Format(format, args);
    // What is the difference between XDECREF and DECREF?
    // Use XDECREF if something can be null, DECREF if it is guarenteed to not be null
    Py_DECREF(args);

    return result;
}

// Make methods available
static PyMethodDef Foo_methods[] = {
    {"square", (PyCFunction) Foo_square, METH_VARARGS, "squares an int"},
    // METH_NOARGS indicates that this method should not be passed any arguments
    {"name", (PyCFunction) Foo_name, METH_NOARGS, "Returns the full name"},
    {NULL}
};

// Declare the type components
static PyTypeObject FooType = {
   PyObject_HEAD_INIT(NULL)
   0,                         /* ob_size */
   "spam.Foo",               /* tp_name */
   sizeof(Foo),         /* tp_basicsize */
   0,                         /* tp_itemsize */
   (destructor)Foo_dealloc, /* tp_dealloc */
   0,                         /* tp_print */
   0,                         /* tp_getattr */
   0,                         /* tp_setattr */
   0,                         /* tp_compare */
   0,                         /* tp_repr */
   0,                         /* tp_as_number */
   0,                         /* tp_as_sequence */
   0,                         /* tp_as_mapping */
   0,                         /* tp_hash */
   0,                         /* tp_call */
   0,                         /* tp_str */
   0,                         /* tp_getattro */
   0,                         /* tp_setattro */
   0,                         /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "Foo object",        /* tp_doc */
   0,                         /* tp_traverse */
   0,                         /* tp_clear */
   0,                         /* tp_richcompare */
   0,                         /* tp_weaklistoffset */
   0,                         /* tp_iter */
   0,                         /* tp_iternext */
   Foo_methods,         /* tp_methods */
   Foo_members,         /* tp_members */
   Foo_getseters,                         /* tp_getset */
   0,                         /* tp_base */
   0,                         /* tp_dict */
   0,                         /* tp_descr_get */
   0,                         /* tp_descr_set */
   0,                         /* tp_dictoffset */
   (initproc)Foo_init,  /* tp_init */
   0,                         /* tp_alloc */
   Foo_new,                         /* tp_new */
};










static const char *cldbs = "hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222";
static const char *tableName = "/app/SubscriptionBillingPlatform/testInteractive";

/*
static const char *family1 = "Id";
static const char *col1_1  = "I";
static const char *family2 = "Name";
static const char *col2_1  = "First";
static const char *col2_2  = "Last";
static const char *family3 = "Address";
static const char *col3_1  = "City";
*/
static char *hbase_fqcolumn(char *family, char *column) {
    // +1 for null terminator, +1 for colon
    char *fq = (char *) malloc(1 + 1 + strlen(family) + strlen(column));
    strcpy(fq, family);
    fq[strlen(family)] = ':';
    fq[strlen(family) + 1] = '\0';
    // strcat will replace the last null terminator before writing, then add a null terminator
    strcat(fq, column);
    return fq;
}

struct RowBuffer {
    std::vector<char *> allocedBufs;

    RowBuffer() {
        allocedBufs.clear();
    }

    ~RowBuffer() {
        while (allocedBufs.size() > 0) {
            char *buf = allocedBufs.back();
            allocedBufs.pop_back();
            delete [] buf;
        }
    }

    char *getBuffer(uint32_t size) {
        char *newAlloc = new char[size];
        allocedBufs.push_back(newAlloc);
        return newAlloc;
    }
    PyObject *ret;
    PyObject *rets;
};



/*
import spam
connection = spam._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.is_open()
connection.open()
connection.is_open()
connection.close()
connection.is_open()
*/



typedef struct {
    PyObject_HEAD
    PyObject *cldbs;
    // Add an is_open boolean
    bool is_open;
    hb_connection_t conn;
    hb_client_t client;
    hb_admin_t admin;
    RowBuffer *rowBuf;
} Connection;

static void Connection_dealloc(Connection *self) {
    Py_XDECREF(self->cldbs);

    //hb_admin_destroy(self->admin, admin_disconnection_callback)

    self->ob_type->tp_free((PyObject *) self);



    // I don't think I need to Py_XDECREF on conn and client?
}

// I'm going to skip Connection_new
// remember to FooType.tp_new = PyType_GenericNew;


static int Connection_init(Connection *self, PyObject *args, PyObject *kwargs) {
    PyObject *cldbs, *tmp;

    // Add an is_open boolean
    if (!PyArg_ParseTuple(args, "S", &cldbs)) {
        return -1;
    }

    if (cldbs) {
        tmp = self->cldbs;
        Py_INCREF(cldbs);
        self->cldbs = cldbs;
        Py_XDECREF(tmp);
    }
    // Todo make this optional, and then find it from /opt/mapr/conf



    return 0;
}

static PyMemberDef Connection_members[] = {
    {"cldbs", T_OBJECT_EX, offsetof(Connection, cldbs), 0, "The cldbs connection string"},
    {NULL}
};

static PyObject *Connection_open(Connection *self) {
    self->rowBuf = new RowBuffer();
    if (!self->is_open) {
        int err = 0;
        err = hb_connection_create(PyString_AsString(self->cldbs), NULL, &self->conn);
        printf("err hb_connection_create %i\n", err);

        err = hb_client_create(self->conn, &self->client);
        printf("err hb_client_create %i\n", err);

        //Add an is_open boolean
        self->is_open = true;

        err = hb_admin_create(self->conn, &self->admin);
        CHECK_RC_RETURN(err);

    }
    Py_RETURN_NONE;

}

static void cl_dsc_cb(int32_t err, hb_client_t client, void *connection) {
    // Perhaps I could add a is_client_open boolean to connection ?
}

static PyObject *Connection_close(Connection *self) {
    hb_client_destroy(self->client, cl_dsc_cb, self);
    hb_connection_destroy(self->conn);
    self->is_open = false;
    Py_RETURN_NONE;
}

static PyObject *Connection_is_open(Connection *self) {
    if (self->is_open) {
        return Py_True;
    }
    return Py_False;
}

/*
static PyObject *Connection_create_table(Connection *self, PyObject *args) {
    char *table_name;
    PyObject *dict;
    if (!PyArg_ParseTuple(args, "sO!", &table_name, PyDict_Type, &dict)) {
        return NULL;
    }
    int number_of_families;
    hb_columndesc families[];
    int err = hb_admin_table_create(self->admin, NULL, table_name, families, num_families);
}
*/

static PyMethodDef Connection_methods[] = {
    {"open", (PyCFunction) Connection_open, METH_NOARGS, "Opens the connection"},
    {"close", (PyCFunction) Connection_close, METH_NOARGS, "Closes the connection"},
    {"is_open", (PyCFunction) Connection_is_open, METH_NOARGS,"Checks if the connection is open"},
    {NULL},
};

// Declare the type components
static PyTypeObject ConnectionType = {
   PyObject_HEAD_INIT(NULL)
   0,                         /* ob_size */
   "spam._connection",               /* tp_name */
   sizeof(Connection),         /* tp_basicsize */
   0,                         /* tp_itemsize */
   (destructor)Connection_dealloc, /* tp_dealloc */
   0,                         /* tp_print */
   0,                         /* tp_getattr */
   0,                         /* tp_setattr */
   0,                         /* tp_compare */
   0,                         /* tp_repr */
   0,                         /* tp_as_number */
   0,                         /* tp_as_sequence */
   0,                         /* tp_as_mapping */
   0,                         /* tp_hash */
   0,                         /* tp_call */
   0,                         /* tp_str */
   0,                         /* tp_getattro */
   0,                         /* tp_setattro */
   0,                         /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "Connection object",        /* tp_doc */
   0,                         /* tp_traverse */
   0,                         /* tp_clear */
   0,                         /* tp_richcompare */
   0,                         /* tp_weaklistoffset */
   0,                         /* tp_iter */
   0,                         /* tp_iternext */
   Connection_methods,         /* tp_methods */
   Connection_members,         /* tp_members */
   0,                         /* tp_getset */
   0,                         /* tp_base */
   0,                         /* tp_dict */
   0,                         /* tp_descr_get */
   0,                         /* tp_descr_set */
   0,                         /* tp_dictoffset */
   (initproc)Connection_init,  /* tp_init */
   0,                         /* tp_alloc */
   0,                         /* tp_new */
};

/*
import spam
connection = spam._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = spam._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.row('row-000')
connection.close()
table.row('row-000')
*/

typedef struct {
    PyObject_HEAD
    Connection *connection;
    // Do I need to INCREF/DECREF this since I am exposing it to the python layer?
    // Is it better or worse taht this is char * instead of PyObject * ?
    char *table_name;

    // TODO need a better way to manage this, also probably need to INCREF??
    PyObject *ret;
    PyObject *rets;

    // TODO this is pretty scary dont multi thread until you understand this
    pthread_mutex_t mutex;
    uint64_t count;
} Table;

struct CallBackBuffer {
    RowBuffer *rowBuf;
    Table *table;
    CallBackBuffer(Table *t, RowBuffer *r) {
        table = t;
        rowBuf = r;
    }
    ~CallBackBuffer() {
        delete rowBuf;
    }
};

static CallBackBuffer *CallBackBuffer_create(Table *table, RowBuffer *rowBuf) {
    CallBackBuffer *call_back_buffer = (CallBackBuffer *) malloc(sizeof(CallBackBuffer));
    call_back_buffer->table = table;
    call_back_buffer->rowBuf = rowBuf;
    return call_back_buffer;
}

static void Table_dealloc(Table *self) {
    Py_XDECREF(self->connection);
}

/*
import spam
connection = spam._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = spam._table(connection, '/app/SubscriptionBillingPlatform/testInteracasdfasdtive')
*/
static int Table_init(Table *self, PyObject *args, PyObject *kwargs) {
    Connection *connection, *tmp;
    char *table_name = NULL;

    if (!PyArg_ParseTuple(args, "O!s", &ConnectionType ,&connection, &table_name)) {
        return -1;
    }

    // TODO Verify if table is valid?

    tmp = self->connection;
    Py_INCREF(connection);
    self->connection = connection;
    Py_XDECREF(connection);

    self->table_name = table_name;

    int err = hb_admin_table_exists(self->connection->admin, NULL, self->table_name);
    CHECK_RC_RETURN(err);

    if (err != 0) {
        // I guess I have to return -1 nothing else to cause the correct failure
        PyErr_SetString(PyExc_ValueError, "Table does not exist\n");
        //return NULL; // return err;
        return -1;
    }

    self->mutex = PTHREAD_MUTEX_INITIALIZER;
    self->count = 0;



    return 0;
}

static PyMemberDef Table_members[] = {
    {"table_name", T_STRING, offsetof(Table, table_name), 0, "The name of the MapRDB table"},
    {NULL}
};


static PyObject *read_result(hb_result_t result, PyObject *dict) {
    if (!result) {
        Py_RETURN_NONE;
    }

    size_t cellCount = 0;
    hb_result_get_cell_count(result, &cellCount);

    // Probably take this as a parameter
    //PyObject *dict = PyDict_New();

    for (size_t i = 0; i < cellCount; ++i) {
        const hb_cell_t *cell;
        hb_result_get_cell_at(result, i, &cell);
        if (dict) {
            PyDict_SetItem(dict, Py_BuildValue("s", hbase_fqcolumn((char *)cell->family, (char *)cell->qualifier)), Py_BuildValue("s",(char *)cell->value));
        }
        printf("%s:%s = %s\t", cell->family, cell->qualifier, cell->value);
    }

    if (cellCount == 0) {
        printf("----- NO CELLS -----");
    }
    return dict;
}


static void row_callback(int32_t err, hb_client_t client, hb_get_t get, hb_result_t result, void *extra) {

    Table *table = (Table *) extra;
    if (result) {
        const byte_t *key;
        size_t keyLen;
        hb_result_get_key(result, &key, &keyLen);
        printf("Row: %s\t", (char *)key);
        PyObject *dict = PyDict_New();
        read_result(result, dict);
        printf("\n");
        Table *table = (Table *)extra;
        table->ret = dict;
        hb_result_destroy(result);
    } else {
        return;
    }

    table->count = 1;
    hb_get_destroy(get);

    /*
    // TODO add this?
    if (extra) {
        RowBuffer *rowBuf = (RowBuffer *)extra;
        printf("In extra, rowkey is %s\n", rowBuf->allocedBufs.back());
        //rowBuf->ret = dict;
        delete rowBuf;
    }
    */
}

static PyObject *Table_row(Table *self, PyObject *args) {
    char *row_key;
    if (!PyArg_ParseTuple(args, "s", &row_key)) {
        return NULL;
    }
    if (!self->connection->is_open) {
        Connection_open(self->connection);
    }

    int err = 0;

    hb_get_t get;
    err = hb_get_create((const byte_t *)row_key, strlen(row_key) + 1, &get);
    CHECK_RC_RETURN(err);

    //err = hb_get_set_table(get, tableName, strlen(tableName));
    err = hb_get_set_table(get, self->table_name, strlen(self->table_name));
    CHECK_RC_RETURN(err);


    self->count = 0;
    //err = hb_get_send(client, get, get_send_cb, rowBuf);
    err = hb_get_send(self->connection->client, get, row_callback, self);
    CHECK_RC_RETURN(err);

    int wait = 0;
    while (self->count != 1) {
        sleep(0.1);
        wait += 1;
        if (wait == 20) {
            //PyErr_SetString(SpamError, "Library error");
            //return NULL;
        }
    }

    return self->ret;


}


void client_flush_callback(int32_t err, hb_client_t client, void *ctx) {
    printf("Client flush callback invoked: %d\n", err);
}

static void *split(char *fq, char* arr[]) {
    int i = 0;
    // Initialize family to length + null pointer - 1 for the colon
    char *family = (char *) malloc(sizeof(char) * strlen(fq));
    for (i = 0; i < strlen(fq) && fq[i] != '\0'; i++) {
        if (fq[i] != ':') {
            family[i] = fq[i];
        } else {
            break;
        }
    }
    family[i] = '\0';

    // This works with 1+ or without 1+ ...
    char *qualifier = (char *) malloc(1 + sizeof(char) * (strlen(fq) - i));
    int qualifier_index = 0;
    for (i=i + 1; i < strlen(fq) && fq[i] != '\0'; i++) {
        qualifier[qualifier_index] = fq[i];
        qualifier_index += 1;
    }
    qualifier[qualifier_index] = '\0';

    arr[0] = family;
    arr[1] = qualifier;

    //printf("arr[0] is %s\n", arr[0]);
    //printf("arr[1] is %s\n", arr[1]);

}



void put_callback(int err, hb_client_t client, hb_mutation_t mutation, hb_result_t result, void *extra) {
    // TODO Check types.h for the HBase error codes
    if (err != 0) {
        printf("PUT CALLBACK called err = %d\n", err);

    }
    //printf("in put_callback\n");
    //printf("Going to do if result lol\n");

    //Table *table = (Table *) extra;
    CallBackBuffer *call_back_buffer = (CallBackBuffer *) extra;


    hb_mutation_destroy(mutation);
    pthread_mutex_lock(&call_back_buffer->table->mutex);
    //table->count++;
    call_back_buffer->table->count++;
    pthread_mutex_unlock(&call_back_buffer->table->mutex);
    //printf("after lock thing\n");
    if (result) {
        //printf("we have a result\n");
        const byte_t *key;
        size_t keyLen;
        //printf("before hb_result_get_key\n");
        hb_result_get_key(result, &key, &keyLen);
        //printf("after hb_result_get_key\n");
        //printf("Row: %s\t", (char *)key);
        read_result(result, NULL);
        //printf("\n");
        hb_result_destroy(result);
    }

    // Ya this is important to do for puts lol
    //if (extra) {
        //RowBuffer *rowBuf = (RowBuffer *)extra;
        //CallBackBuffer *call_back_buffer = (CallBackBuffer *) extra;
        //printf("before delete buffer\n");
        delete call_back_buffer; //->rowBuf;
        //printf("after delete buffer\n");
        //free(call_back_buffer);
    //}

}

void create_dummy_cell(hb_cell_t **cell,
                      const char *r, size_t rLen,
                      const char *f, size_t fLen,
                      const char *q, size_t qLen,
                      const char *v, size_t vLen) {
    hb_cell_t *cellPtr = new hb_cell_t();

    cellPtr->row = (byte_t *)r;
    cellPtr->row_len = rLen;

    cellPtr->family = (byte_t *)f;
    cellPtr->family_len = fLen;

    cellPtr->qualifier = (byte_t *)q;
    cellPtr->qualifier_len = qLen;

    cellPtr->value = (byte_t *)v;
    cellPtr->value_len = vLen;

    *cell = cellPtr;
}


/*
import spam
connection = spam._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = spam._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.row('row-000')
table.put('snoop', {'Name:a':'a','Name:foo':'bar'})
*/

static int make_put(Table *self, RowBuffer *rowBuf, const char *row_key, PyObject *dict, hb_put_t *hb_put) {
    int err;
    //hb_put_t *hb_put = (hb_put_t *) malloc(sizeof(hb_put_t));
    //printf("Before hb_put_create\n");
    err = hb_put_create((byte_t *)row_key, strlen(row_key) + 1, hb_put);
    //printf("After hb_put_create\n");
    CHECK_RC_RETURN(err);
    //printf("hb_put_create was %i\n", err);

    PyObject *fq, *value;
    Py_ssize_t pos = 0;
    hb_cell_t *cell;
    while (PyDict_Next(dict, &pos, &fq, &value)) {
        char *arr[2];
        split(PyString_AsString(fq), arr);
        //printf("family is %s\n", arr[0]);
        //printf("qualifier is %s\n", arr[1]);
        // TODO Have to make sure to free this memory lol
        char *family = rowBuf->getBuffer(1024);
        char *qualifier = rowBuf->getBuffer(1024);
        char *v = rowBuf->getBuffer(1024);

        strcpy(family, arr[0]);
        strcpy(qualifier, arr[1]);
        strcpy(v, PyString_AsString(value));

        //printf("family is %s\n", family);
        //printf("qualifier is %s\n", qualifier);
        //printf("v is %s\n", v);

        //printf("creating dummy cell\n");
        create_dummy_cell(&cell, row_key, strlen(row_key), family, strlen(family) + 1, qualifier, strlen(qualifier) + 1, v, strlen(v) + 1);
        //printf("put add cell\n");
        err = hb_put_add_cell(*hb_put, cell);
        //printf("put add cell error %i\n", err);
        delete cell;
        CHECK_RC_RETURN(err);
        //printf("RC for put add cell was %i\n", err);
    }

    //printf("hb_mutation set table\n");
    err = hb_mutation_set_table((hb_mutation_t)*hb_put, self->table_name, strlen(self->table_name));
    CHECK_RC_RETURN(err);
    //printf("RC for muttaion set table was %i\n", err);

    return err;
}

static PyObject *Table_put(Table *self, PyObject *args) {
    char *row_key;
    PyObject *dict;

    if (!PyArg_ParseTuple(args, "sO!", &row_key, &PyDict_Type, &dict)) {
        return NULL;
    }

    int err = 0;

    RowBuffer *rowBuf = new RowBuffer();
    //CallBackBuffer *call_back_buffer = CallBackBuffer_create(self, rowBuf);
    CallBackBuffer *call_back_buffer = new CallBackBuffer(self, rowBuf);

    hb_put_t hb_put;
    err = make_put(self, rowBuf, row_key, dict, &hb_put);
    printf("After make_put in table_put\n");
    CHECK_RC_RETURN(err);

    err = hb_mutation_send(self->connection->client, (hb_mutation_t)hb_put, put_callback, call_back_buffer);
    CHECK_RC_RETURN(err);
    printf("RC for mutation send was %i\n", err);


    self->count = 0;
    hb_client_flush(self->connection->client, client_flush_callback, NULL);
    printf("Waiting for all callbacks to return ...\n");

    //uint64_t locCount;
    //do {
    //    sleep (1);
    //    pthread_mutex_lock(&mutex);
    //    locCount = count;
    //    pthread_mutex_unlock(&mutex);
    //} while (locCount < numRows);

    int wait = 0;

    while (self->count != 1) {
        sleep(0.1);
        wait++;
        //printf("wait is %i\n",wait);
        if (wait == 20) {
            //printf("wait is %i\n",wait);
            //PyErr_SetString(SpamError, "Library error");
            //return NULL;
        }

    }

    //free(hb_put);

    Py_RETURN_NONE;
}

void scan_callback(int32_t err, hb_scanner_t scan, hb_result_t *results, size_t numResults, void *extra) {
    printf("In sn_cb\n");
    Table *table = (Table *) extra;
    if (numResults > 0) {
        printf("bnefore rowbuff = extra\n");

        PyObject *dict;
        printf("befoer loop\n");
        for (uint32_t r = 0; r < numResults; ++r) {
            printf("looping\n");
            const byte_t *key;
            size_t keyLen;
            hb_result_get_key(results[r], &key, &keyLen);
            printf("Row: %s\t", (char *)key);
            dict = PyDict_New();
            printf("before reading result into dict\n");
            read_result(results[r], dict);
            printf("before append\n");
            // Do I need to INCREF the result of Py_BuildValue?
            // Should I do that ! with the type?
            PyList_Append(table->rets, Py_BuildValue("sO",(char *)key, dict));
            printf("\n");
            printf("before destroy\n");
            hb_result_destroy(results[r]);
        }

        hb_scanner_next(scan, scan_callback, table);
    } else {
        printf(" ----- NO MORE RESULTS -----\n");
        hb_scanner_destroy(scan, NULL, NULL);
        sleep(0.1);
        pthread_mutex_lock(&table->mutex);
        table->count = 1;
        pthread_mutex_unlock(&table->mutex);
    }
}

/*
import spam
connection = spam._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = spam._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.row('row-000')
table.scan()
*/

static PyObject *Table_scan(Table *self, PyObject *args) {
    char *start = NULL;
    char *stop = NULL;

    if (!PyArg_ParseTuple(args, "|ss", &start, &stop)) {
        return NULL;
    }

    self->rets = PyList_New(0);

    int err = 0;

    hb_scanner_t scan;
    err = hb_scanner_create(self->connection->client, &scan);
    CHECK_RC_RETURN(err);
    printf("RC for scanner create was %i\n", err);

    err = hb_scanner_set_table(scan, self->table_name, strlen(self->table_name));
    CHECK_RC_RETURN(err);
    printf("RC for set table was %i\n", err);

    err = hb_scanner_set_num_versions(scan, 1);
    CHECK_RC_RETURN(err);
    printf("RC for num versions  was %i\n", err);

    if (start) {
        // Do I need strlen + 1 ?
        err = hb_scanner_set_start_row(scan, (byte_t *) start, strlen(start));
        CHECK_RC_RETURN(err);
        printf("RC for start row  was %i\n", err);
    }
    if (stop) {
        // do I need strlen + 1 ?
        err = hb_scanner_set_end_row(scan, (byte_t *) stop, strlen(stop));
        CHECK_RC_RETURN(err);
        printf("RC for stop row  was %i\n", err);
    }

    // Does it optimize if I set this higher?
    err = hb_scanner_set_num_max_rows(scan, 1);
    CHECK_RC_RETURN(err);
    printf("RC for set num max rows  was %i\n", err);

    self->count = 0;
    err = hb_scanner_next(scan, scan_callback, self);
    CHECK_RC_RETURN(err);
    printf("RC for scanner next  was %i\n", err);

    int wait = 0;
    while (self->count == 0) {
        sleep(0.1);
        wait += 1;
    }

    return self->rets;


}

void delete_callback(int err, hb_client_t client, hb_mutation_t mutation, hb_result_t result, void *extra) {
    // Not sure if I can just use the put_callback or if I should change them
    if (err != 0) {
        printf("PUT CALLBACK called err = %d\n", err);
    }

    //Table *table = (Table *) extra;
    CallBackBuffer *call_back_buffer = (CallBackBuffer *) extra;


    hb_mutation_destroy(mutation);
    pthread_mutex_lock(&call_back_buffer->table->mutex);
    //count ++;
    call_back_buffer->table->count++;
    pthread_mutex_unlock(&call_back_buffer->table->mutex);
    if (result) {
        const byte_t *key;
        size_t keyLen;
        hb_result_get_key(result, &key, &keyLen);
        printf("Row: %s\t", (char *)key);
        read_result(result, NULL);
        printf("\n");
        hb_result_destroy(result);
    }

    delete call_back_buffer;
    /*
    // Ya this is important to do for puts lol
    if (extra) {
        RowBuffer *rowBuf = (RowBuffer *)extra;
        delete rowBuf;
    }
    */
}

/*
import spam
connection = spam._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = spam._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.row('hello1')
table.delete('hello1')
*/

static int make_delete(Table *self, char *row_key, hb_delete_t *hb_delete) {
    int err = 0;

    err = hb_delete_create((byte_t *)row_key, strlen(row_key) + 1, hb_delete);
    CHECK_RC_RETURN(err);

    err = hb_mutation_set_table((hb_mutation_t)*hb_delete, self->table_name, strlen(self->table_name));
    CHECK_RC_RETURN(err);

    return err;
}

static PyObject *Table_delete(Table *self, PyObject *args) {
    char *row_key;
    if (!PyArg_ParseTuple(args, "s", &row_key)) {
        return NULL;
    }
    if (!self->connection->is_open) {
        Connection_open(self->connection);
    }

    int err = 0;
    /*
    hb_delete_t hb_delete;

    err = hb_delete_create((byte_t *)row_key, strlen(row_key) + 1, &hb_delete);
    CHECK_RC_RETURN(err);

    err = hb_mutation_set_table((hb_mutation_t)hb_delete, self->table_name, strlen(self->table_name));
    CHECK_RC_RETURN(err);
    printf("RC for muttaion set table was %i\n", err);
    */

    hb_delete_t hb_delete;
    err = make_delete(self, row_key, &hb_delete);

    CHECK_RC_RETURN(err);
    printf("RC for muttaion set table was %i\n", err);

    RowBuffer *rowBuf = new RowBuffer();
    //CallBackBuffer *call_back_buffer = CallBackBuffer_create(self, rowBuf);
    CallBackBuffer *call_back_buffer = new CallBackBuffer(self, rowBuf);

    err = hb_mutation_send(self->connection->client, (hb_mutation_t)hb_delete, delete_callback, call_back_buffer);
    CHECK_RC_RETURN(err);
    printf("RC for mutation send was %i\n", err);

    self->count = 0;
    hb_client_flush(self->connection->client, client_flush_callback, NULL);
    printf("Waiting for all callbacks to return ...\n");

    int wait = 0;

    while (self->count != 1) {
        sleep(0.1);
        wait++;
        //printf("wait is %i\n",wait);
        if (wait == 20) {
            //printf("wait is %i\n",wait);
            //PyErr_SetString(SpamError, "Library error");
            //return NULL;
        }

    }

    Py_RETURN_NONE;
}


/*
import spam
connection = spam._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = spam._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.batch([('put', 'hello{}'.format(i), {'Name:bar':'bar{}'.format(i)}) for i in range(1000000)])
#table.scan()


import spam
connection = spam._connection("hdnprd-c01-r03-01:7222,hdnprd-c01-r04-01:7222,hdnprd-c01-r05-01:7222")
connection.open()

table = spam._table(connection, '/app/SubscriptionBillingPlatform/testInteractive')
table.batch([('delete', 'hello{}'.format(i), {'Name:bar':'bar{}'.format(i)}) for i in range(1000000)])

*/

static PyObject *Table_batch(Table *self, PyObject *args) {
    PyObject *actions;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &actions)) {
        return NULL;
    }

    self->count = 0;

    int err;

    PyObject *tuple;
    Py_ssize_t i;
    int number_of_actions = PyList_Size(actions);
    for (i = 0; i < number_of_actions; i++) {
        tuple = PyList_GetItem(actions, i);
        //printf("got tuple\n");
        char *mutation_type = PyString_AsString(PyTuple_GetItem(tuple, 0));
        //printf("got mutation_type\n");
        //printf("mutation type is %s\n", mutation_type);
        if (strcmp(mutation_type, "put") == 0) {
            //printf("Its a put");
            RowBuffer *rowBuf = new RowBuffer();
            //CallBackBuffer *call_back_buffer = CallBackBuffer_create(self, rowBuf);
            CallBackBuffer *call_back_buffer = new CallBackBuffer(self, rowBuf);
            //In particular, all functions whose function it is to create a new object, such as PyInt_FromLong() and Py_BuildValue(), pass ownership to the receiver.
            char *row_key = PyString_AsString(PyTuple_GetItem(tuple, 1));
            PyObject *dict = PyTuple_GetItem(tuple, 2);
            // do I need to increment dict?
            hb_put_t hb_put;
            err = make_put(self, rowBuf, row_key, dict, &hb_put);
            CHECK_RC_RETURN(err)
            err = hb_mutation_send(self->connection->client, (hb_mutation_t)hb_put, put_callback, call_back_buffer);
            // TODO ADD the hb_put to the call back buffer and free it!
            CHECK_RC_RETURN(err);
            //printf("hb mutation send was %i\n",err);
        } else if (strcmp(mutation_type, "delete") == 0) {
            //printf("its a delete");
            RowBuffer *rowBuf = new RowBuffer();
            CallBackBuffer *call_back_buffer = new CallBackBuffer(self, rowBuf);
            char *row_key = PyString_AsString(PyTuple_GetItem(tuple, 1));
            hb_delete_t hb_delete;
            err = make_delete(self, row_key, &hb_delete);
            CHECK_RC_RETURN(err);
            err = hb_mutation_send(self->connection->client, (hb_mutation_t)hb_delete, delete_callback, call_back_buffer);
            CHECK_RC_RETURN(err);
        }
    }

    printf("done with loop going to flush\n");

    //self->count = 0;
    hb_client_flush(self->connection->client, client_flush_callback, NULL);
    printf("Waiting for all callbacks to return ...\n");

    long wait = 0;

    while (self->count < number_of_actions) {
        sleep(0.1);
        wait++;
        //printf("wait is %i\n",wait);
        if (wait > 10000000) {
            //printf("wait is %i\n",wait);
            //printf("Count is %i\n", self->count);
            printf("count is %i wait is %ld\n", self->count, wait);
            PyErr_SetString(SpamError, "Library error");
            return NULL;

        }

    }
    printf("wait was %ld\n",wait);

    Py_RETURN_NONE;
}

static PyMethodDef Table_methods[] = {
    {"row", (PyCFunction) Table_row, METH_VARARGS, "Gets one row"},
    {"put", (PyCFunction) Table_put, METH_VARARGS, "Puts one row"},
    {"scan", (PyCFunction) Table_scan, METH_VARARGS, "Scans the table"},
    {"delete", (PyCFunction) Table_delete, METH_VARARGS, "Deletes one row"},
    {"batch", (PyCFunction) Table_batch, METH_VARARGS, "sends a batch"},
    {NULL}
};

// Declare the type components
static PyTypeObject TableType = {
   PyObject_HEAD_INIT(NULL)
   0,                         /* ob_size */
   "spam._table",               /* tp_name */
   sizeof(Table),         /* tp_basicsize */
   0,                         /* tp_itemsize */
   (destructor)Table_dealloc, /* tp_dealloc */
   0,                         /* tp_print */
   0,                         /* tp_getattr */
   0,                         /* tp_setattr */
   0,                         /* tp_compare */
   0,                         /* tp_repr */
   0,                         /* tp_as_number */
   0,                         /* tp_as_sequence */
   0,                         /* tp_as_mapping */
   0,                         /* tp_hash */
   0,                         /* tp_call */
   0,                         /* tp_str */
   0,                         /* tp_getattro */
   0,                         /* tp_setattro */
   0,                         /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "Connection object",        /* tp_doc */
   0,                         /* tp_traverse */
   0,                         /* tp_clear */
   0,                         /* tp_richcompare */
   0,                         /* tp_weaklistoffset */
   0,                         /* tp_iter */
   0,                         /* tp_iternext */
   Table_methods,         /* tp_methods */
   Table_members,         /* tp_members */
   0,                         /* tp_getset */
   0,                         /* tp_base */
   0,                         /* tp_dict */
   0,                         /* tp_descr_get */
   0,                         /* tp_descr_set */
   0,                         /* tp_dictoffset */
   (initproc)Table_init,  /* tp_init */
   0,                         /* tp_alloc */
   PyType_GenericNew,                         /* tp_new */
};

/*
struct Connection {
    hb_connection_t conn;
    hb_client_t client;
    RowBuffer *rowBuf;

    Connection() {
        rowBuf = new RowBuffer();
        int err = 0;
        err = hb_connection_create(cldbs, NULL, &conn);
        CHECK_RC_RETURN(err);

        err = hb_client_create(conn, &client);
        CHECK_RC_RETURN(err);
    }

    ~Connection() {
        hb_client_destroy(client, cl_dsc_cb, rowBuf);
        hb_connection_destroy(conn);
    }

};
*/

/*
static void read_result(hb_result_t result) {
    if (!result) {
        return;
    }

    size_t cellCount = 0;
    hb_result_get_cell_count(result, &cellCount);

    for (size_t i = 0; i < cellCount; ++i) {
        const hb_cell_t *cell;
        hb_result_get_cell_at(result, i, &cell);
        printf("%s:%s = %s\t", cell->family, cell->qualifier, cell->value);
    }

    if (cellCount == 0) {
        printf("----- NO CELLS -----");
    }
}
*/










/*
static PyObject *pymaprdb_get(Connection *connection,const char *table_name, char *row_key) {
    printf("Inside pymaprdb_get\n");
    //RowBuffer *rowBuf = new RowBuffer();
    //char *rk = rowBuf->getBuffer(1024);
    char *rk = connection->rowBuf->getBuffer(1024);
    //char rk[1024];
    // Both of those work^ I wonder why they use the row buffer?

    //printf("Enter key to get: ");
    //scanf("%s", rk);
    strcpy(rk, row_key);
    printf("In test_get after strcpy, rowkey is %s\n", connection->rowBuf->allocedBufs.back());


    //hb_connection_t conn;
    int err = 0;


    //if (!user) {
        //err = hb_connection_create(cldbs, NULL, &conn);
        //CHECK_RC_RETURN(err);
    //} else {
    //    err = hb_connection_create_as_user(cldbs, NULL, user, &conn);
    //    CHECK_RC_RETURN(err);
    //}

    //hb_client_t client;

    //err = hb_client_create(conn, &client);
    //CHECK_RC_RETURN(err);

    hb_get_t get;
    err = hb_get_create((const byte_t *)rk, strlen(rk) + 1, &get);
    CHECK_RC_RETURN(err);

    //err = hb_get_set_table(get, tableName, strlen(tableName));
    err = hb_get_set_table(get, table_name, strlen(table_name));
    CHECK_RC_RETURN(err);


    count = 0;
    //err = hb_get_send(client, get, get_send_cb, rowBuf);
    err = hb_get_send(connection->client, get, get_send_cb, connection->rowBuf);
    CHECK_RC_RETURN(err);

    while (count != 1) { sleep(0.1); }

    hb_client_destroy(connection->client, cl_dsc_cb, connection);

    //sleep(1);
    //hb_connection_destroy(conn);

    printf("Done with test_get\n");
    return connection->rowBuf->ret;
}
*/





// The C function always has self and args
// for Module functions, self is NULL; for a method, self is the object
static PyObject *spam_system(PyObject *self, PyObject *args)
{
    const char *command;
    int sts;
    //PyArg_ParseTuple converts the python arguments to C values
    // It returns if all arguments are valid
    if (!PyArg_ParseTuple(args, "s", &command))
        // Returning NULL throws an exception
        return NULL;
    sts = system(command);
    if (sts < 0) {
        // Note how this sets the exception, and THEN returns null!
        PyErr_SetString(SpamError, "System command failed");
        return NULL;
    }
    return PyLong_FromLong(sts);
}


static PyObject *lol(PyObject *self, PyObject *args) {
    printf("Noob\n");
    // This is how to write a void method in python
    Py_RETURN_NONE;
}

static void noob(char *row_key) {
    printf("you are a noob");
    char rk[100];
    printf("Before segmentation fault");
    strcpy(rk, row_key);
    printf("After segmentation fault");
}
/*
static PyObject *get(PyObject *self, PyObject *args) {
    char *row_key;
    if (!PyArg_ParseTuple(args, "s", &row_key)) {
        return NULL;
    }
    Connection *connection = new Connection();
    printf("hai I am %s\n", row_key);
    printf("before test_get\n");
    PyObject *lol = pymaprdb_get(connection, tableName, row_key);
    printf("done with foo\n");
    delete connection;
    //noob(row_key);
    return lol;
}
*/
/*
import spam
spam.put('hai', {'Name:First': 'Matthew'})
*/


/*
import spam
spam.scan()
*/

/*


static PyObject *scan(PyObject *self, PyObject *args) {
    char *start = NULL;
    char *stop = NULL;

    RowBuffer *rowBuf = new RowBuffer();
    rowBuf->rets = PyList_New(0);

    if (!PyArg_ParseTuple(args, "|ss", &start, &stop)) {
        return NULL;
    }

    hb_connection_t conn;
    int err = 0;
    err = hb_connection_create(cldbs, NULL, &conn);
    CHECK_RC_RETURN(err);
    printf("RC for conn create was %i\n", err);
    hb_client_t client;
    err = hb_client_create(conn, &client);
    CHECK_RC_RETURN(err);
    printf("RC for client create was %i\n", err);

    hb_scanner_t scan;
    err = hb_scanner_create(client, &scan);
    CHECK_RC_RETURN(err);
    printf("RC for scanner create was %i\n", err);

    err = hb_scanner_set_table(scan, tableName, strlen(tableName));
    CHECK_RC_RETURN(err);
    printf("RC for set table was %i\n", err);

    err = hb_scanner_set_num_versions(scan, 1);
    CHECK_RC_RETURN(err);
    printf("RC for num versions  was %i\n", err);

    if (start) {
        // Do I need strlen + 1 ?
        err = hb_scanner_set_start_row(scan, (byte_t *) start, strlen(start));
        CHECK_RC_RETURN(err);
        printf("RC for start row  was %i\n", err);
    }
    if (stop) {
        err = hb_scanner_set_end_row(scan, (byte_t *) stop, strlen(stop));
        CHECK_RC_RETURN(err);
        printf("RC for stop row  was %i\n", err);
    }

    // Does it optimize if I set this higher?
    err = hb_scanner_set_num_max_rows(scan, 1);
    CHECK_RC_RETURN(err);
    printf("RC for set num max rows  was %i\n", err);

    count = 0;
    err = hb_scanner_next(scan, sn_cb, rowBuf);
    CHECK_RC_RETURN(err);
    printf("RC for scanner next  was %i\n", err);

    while (count == 0) { sleep(0.1); }

    err = hb_client_destroy(client, cl_dsc_cb, NULL);
    CHECK_RC_RETURN(err);
    printf("RC for client destroy  was %i\n", err);


    err = hb_connection_destroy(conn);
    CHECK_RC_RETURN(err);
    printf("RC for connection destroy  was %i\n", err);

    return rowBuf->rets;
}
*/

static PyObject *build_int(PyObject *self, PyObject *args) {
    return Py_BuildValue("i", 123);
}

static PyObject *build_dict(PyObject *self, PyObject *args) {
    return Py_BuildValue("{s:i}", "name", 123);
}

static PyObject *add_to_dict(PyObject *self, PyObject *args) {
    PyObject *key;
    PyObject *value;
    PyObject *dict;

    if (!PyArg_ParseTuple(args, "OOO", &dict, &key, &value)) {
        return NULL;
    }

    printf("Parsed successfully\n");

    PyDict_SetItem(dict, key, value);

    Py_RETURN_NONE;
}

static PyObject *print_dict(PyObject *self, PyObject *args) {
    PyObject *dict;

    if (!PyArg_ParseTuple(args, "O", &dict)) {
        return NULL;
    }

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(dict, &pos, &key, &value)) {
        //PyString_AsString converts a PyObject to char * (and assumes it is actually a char * not some other data type)

        printf("key is %s\n", PyString_AsString(key));
        printf("value is %s\n", PyString_AsString(value));
    }

    Py_RETURN_NONE;

}

static PyObject *build_list(PyObject *self, PyObject *args) {
    int num;
    if (!PyArg_ParseTuple(args, "i", &num)) {
        return NULL;
    }
    printf("num is %i\n", num);
    PyObject *list = PyList_New(0);
    int i = 0;
    for (i = 0; i < num; i++) {
        PyObject *val = Py_BuildValue("s", "hai");
        PyList_Append(list, val);
        // This doesn't seem to help?
        Py_DECREF(val);
    }

    return list;
}

static PyObject *super_dict(PyObject *self, PyObject *args) {
    char *f1;
    char *k1;
    char *v1;
    char *f2;
    char *k2;
    char *v2;

    if (!PyArg_ParseTuple(args, "ssssss", &f1, &k1, &v1, &f2, &k2, &v2)) {
        return NULL;
    }
    printf("f1 is %s\n", f1);
    printf("k1 is %s\n", k1);
    printf("v1 is %s\n", v1);
    printf("f2 is %s\n", f2);
    printf("k2 is %s\n", k2);
    printf("v2 is %s\n", v2);
    /*
    char *first = (char *) malloc(1 + 1 + strlen(f1) + strlen(f2));
    strcpy(first, f1);
    first[strlen(f1)] = ':';
    strcat(first, k1);
    */

    // somehow take args as a tuple
    PyObject *dict = PyDict_New();

    char *first = hbase_fqcolumn(f1, k1);
    char *second = hbase_fqcolumn(f2, k2);

    printf("First is %s\n", first);
    printf("Second is %s\n", second);

    PyDict_SetItem(dict, Py_BuildValue("s", first), Py_BuildValue("s", v1));
    PyDict_SetItem(dict, Py_BuildValue("s", second), Py_BuildValue("s", v2));

    return dict;
}

static PyObject *print_list(PyObject *self, PyObject *args) {
    //PyListObject seems to suck, it isn't accepted by PyList_Size for example
    PyObject *actions;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &actions)) {
        return NULL;
    }

    //http://effbot.org/zone/python-capi-sequences.htm
    // This guy recommends PySequence_Fast api
    PyObject *value;
    Py_ssize_t i;
    for (i = 0; i < PyList_Size(actions); i++) {
        value = PyList_GetItem(actions, i);
        printf("value is %s\n", PyString_AsString(value));
    }

    Py_RETURN_NONE;
}

/*
import spam
spam.print_list_t([('put', 'row1', {'a':'b'}), ('delete', 'row2')])
*/
static PyObject *print_list_t(PyObject *self, PyObject *args) {
    //PyListObject seems to suck, it isn't accepted by PyList_Size for example
    PyObject *actions;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &actions)) {
        return NULL;
    }

    //http://effbot.org/zone/python-capi-sequences.htm
    // This guy recommends PySequence_Fast api

    PyObject *tuple;
    Py_ssize_t i;
    for (i = 0; i < PyList_Size(actions); i++) {
        tuple = PyList_GetItem(actions, i);
        printf("got tuple\n");
        char *mutation_type = PyString_AsString(PyTuple_GetItem(tuple, 0));
        printf("got mutation_type\n");
        printf("mutation type is %s\n", mutation_type);
        if (strcmp(mutation_type, "put") == 0) {
            printf("Its a put");
        } else if (strcmp(mutation_type, "delete") == 0) {
            printf("its a delete");
        }
    }

    Py_RETURN_NONE;
}

/*
import string
import spam
spam.print_list([c for c in string.letters])
*/
static PyObject *print_list_fast(PyObject *self, PyObject *args) {
    //http://effbot.org/zone/python-capi-sequences.htm
    // This guy says the PySqeunce_Fast api is faster
    // hm later on he says You can also use the PyList API (dead link), but that only works for lists, and is only marginally faster than the PySequence_Fast API.
    PyObject *actions;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &actions)) {
        return NULL;
    }

    PyObject *seq;
    int i, len;

    PyObject *value;

    seq = PySequence_Fast(actions, "expected a sequence");
    len = PySequence_Size(actions);

    for (i = 0; i < len; i++) {
        value = PySequence_Fast_GET_ITEM(seq, i);
        printf("Value is %s\n", PyString_AsString(value));
    }

    Py_RETURN_NONE;


}




/*
lol = spam.build_dict()
print lol
spam.add_to_dict(lol, 'hai', 'bai')

lol = spam.


import spam
spam.super_dict('f', 'k1', 'v1', 'f2', 'k2', 'v2')

*/
/*
static PyObject *foo(PyObject *self, PyObject *args) {
    int lol = pymaprdb_get(NULL);
    Py_RETURN_NONE;
}
*/

static PyMethodDef SpamMethods[] = {
    {"system",  spam_system, METH_VARARGS, "Execute a shell command."},
    {"lol", lol, METH_VARARGS, "your a lol"},
    //{"get", get, METH_VARARGS, "gets a row given a rowkey"},
    //{"put", put, METH_VARARGS, "puts a row and dict"},
    //{"scan", scan, METH_VARARGS, "scans"},
    {"build_int", build_int, METH_VARARGS, "build an int"},
    {"build_dict", build_dict, METH_VARARGS, "build a dict"},
    {"add_to_dict", add_to_dict, METH_VARARGS, "add to dict"},
    {"super_dict", super_dict, METH_VARARGS, "super dict"},
    {"print_dict", print_dict, METH_VARARGS, "print dict"},
    {"build_list", build_list, METH_VARARGS, "build list"},
    {"print_list", print_list, METH_VARARGS, "prints a list"},
    {"print_list_fast", print_list_fast, METH_VARARGS, "prints a list using the fast api"},
    {"print_list_t", print_list_t, METH_VARARGS, "pritns a list of tuples"},
    {NULL, NULL, 0, NULL}
};

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initspam(void)
{
    PyObject *m;

    m = Py_InitModule("spam", SpamMethods);
    if (m == NULL) {
        return;
    }

    // Fill in some slots in the type and make it ready
    // I suppose I use this if I don't write my own new mthod?
    //FooType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&FooType) < 0) {
        return;
    }

    if (PyType_Ready(&ConnectionType) < 0) {
        return;
    }

    if (PyType_Ready(&TableType) < 0) {
        return;
    }


    // no tp_new here because its in the FooType
    Py_INCREF(&FooType);
    PyModule_AddObject(m, "Foo", (PyObject *) &FooType);

    // Add the type to the module
    // failing to add this tp_new will result in: TypeError: cannot create 'spam._connection' instances
    ConnectionType.tp_new = PyType_GenericNew;
    Py_INCREF(&ConnectionType);
    PyModule_AddObject(m, "_connection", (PyObject *) &ConnectionType);

    //TableType.tp_new = PyType_GenericNew;
    Py_INCREF(&TableType);
    PyModule_AddObject(m, "_table", (PyObject *) &TableType);

    SpamError = PyErr_NewException("spam.error", NULL, NULL);
    Py_INCREF(SpamError);
    PyModule_AddObject(m, "error", SpamError);
}

int
main(int argc, char *argv[])
{

    Py_SetProgramName(argv[0]);


    Py_Initialize();


    initspam();
}