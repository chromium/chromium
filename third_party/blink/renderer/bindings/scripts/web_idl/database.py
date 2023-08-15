# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import file_io
from .observable_array import ObservableArray
from .typedef import Typedef
from .union import Union
from .user_defined_type import UserDefinedType


class DatabaseBody(object):
    """
    Database class is a public class to provide read access only, while
    DatabaseBody class is an internal class within web_idl module to support
    both of read and write access in addition to construction of Database
    instances.

    |self._defs| is the storage of IDL definitions in the final shape (not IR),
    in the form of:

        { kind1 : { identifier_a: def_a, identifier_b: def_b, ... },
          kind2 : { ... },
          ...
        }
    """

    class Kind(object):
        ASYNC_ITERATOR = 'async iterator'
        CALLBACK_FUNCTION = 'callback function'
        CALLBACK_INTERFACE = 'callback interface'
        DICTIONARY = 'dictionary'
        ENUMERATION = 'enumeration'
        INTERFACE = 'interface'
        INTERFACE_MIXIN = 'interface mixin'
        NAMESPACE = 'namespace'
        OBSERVABLE_ARRAY = 'observable array'
        SYNC_ITERATOR = 'sync iterator'
        TYPEDEF = 'typedef'
        UNION = 'union'

        _ALL_ENTRIES = (
            ASYNC_ITERATOR,
            CALLBACK_FUNCTION,
            CALLBACK_INTERFACE,
            DICTIONARY,
            ENUMERATION,
            INTERFACE,
            INTERFACE_MIXIN,
            NAMESPACE,
            OBSERVABLE_ARRAY,
            SYNC_ITERATOR,
            TYPEDEF,
            UNION,
        )

        @classmethod
        def values(cls):
            return cls._ALL_ENTRIES.__iter__()

    def __init__(self):
        self._defs = {}
        for kind in DatabaseBody.Kind.values():
            self._defs[kind] = {}

    def register(self, kind, user_defined_type):
        assert isinstance(user_defined_type,
                          (ObservableArray, Typedef, Union, UserDefinedType))
        assert kind in DatabaseBody.Kind.values()
        try:
            self.find_by_identifier(user_defined_type.identifier)
            assert False, user_defined_type.identifier
        except KeyError:
            pass
        self._defs[kind][user_defined_type.identifier] = user_defined_type

    def find_by_identifier(self, identifier):
        for defs_per_kind in self._defs.values():
            if identifier in defs_per_kind:
                return defs_per_kind[identifier]
        raise KeyError(identifier)

    def find_by_kind(self, kind):
        return self._defs[kind]


class Database(object):
    """
    Database class is an entry point for the clients of web_idl module.  All the
    data about IDL definitions will be retrieved from this database.

    Database class provides read access only.
    """

    _Kind = DatabaseBody.Kind

    def __init__(self, database_body):
        assert isinstance(database_body, DatabaseBody)
        self._impl = database_body

    @staticmethod
    def read_from_file(filepath):
        database = file_io.read_pickle_file(filepath)
        assert isinstance(database, Database)
        return database

    def write_to_file(self, filepath):
        return file_io.write_pickle_file_if_changed(filepath, self)

    def find(self, identifier):
        """
        Returns the IDL definition specified with |identifier|.  Raises KeyError
        if not found.
        """
        return self._impl.find_by_identifier(identifier)

    @property
    def async_iterators(self):
        """Returns all async iterators."""
        return self._view_by_kind(Database._Kind.ASYNC_ITERATOR)

    @property
    def callback_functions(self):
        """Returns all callback functions."""
        return self._view_by_kind(Database._Kind.CALLBACK_FUNCTION)

    @property
    def callback_interfaces(self):
        """Returns all callback interfaces."""
        return self._view_by_kind(Database._Kind.CALLBACK_INTERFACE)

    @property
    def dictionaries(self):
        """Returns all dictionaries."""
        return self._view_by_kind(Database._Kind.DICTIONARY)

    @property
    def enumerations(self):
        """Returns all enumerations."""
        return self._view_by_kind(Database._Kind.ENUMERATION)

    @property
    def interfaces(self):
        """
        Returns all interfaces.

        Callback interfaces and mixins are not included.
        """
        return self._view_by_kind(Database._Kind.INTERFACE)

    @property
    def interface_mixins(self):
        """Returns all interface mixins."""
        return self._view_by_kind(Database._Kind.INTERFACE_MIXIN)

    @property
    def namespaces(self):
        """Returns all namespaces."""
        return self._view_by_kind(Database._Kind.NAMESPACE)

    @property
    def observable_arrays(self):
        """Returns all observable arrays."""
        return self._view_by_kind(Database._Kind.OBSERVABLE_ARRAY)

    @property
    def sync_iterators(self):
        """Returns all sync iterators."""
        return self._view_by_kind(Database._Kind.SYNC_ITERATOR)

    @property
    def typedefs(self):
        """Returns all typedef definitions."""
        return self._view_by_kind(Database._Kind.TYPEDEF)

    @property
    def union_types(self):
        """Returns all union type definitions."""
        return self._view_by_kind(Database._Kind.UNION)

    def _view_by_kind(self, kind):
        return list(self._impl.find_by_kind(kind).values())
