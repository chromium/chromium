# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import file_io
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
        CALLBACK_FUNCTION = 'callback function'
        CALLBACK_INTERFACE = 'callback interface'
        DICTIONARY = 'dictionary'
        ENUMERATION = 'enumeration'
        INTERFACE = 'interface'
        INTERFACE_MIXIN = 'interface mixin'
        NAMESPACE = 'namespace'
        TYPEDEF = 'typedef'
        UNION = 'union'

        _ALL_ENTRIES = (
            CALLBACK_FUNCTION,
            CALLBACK_INTERFACE,
            DICTIONARY,
            ENUMERATION,
            INTERFACE,
            INTERFACE_MIXIN,
            NAMESPACE,
            TYPEDEF,
            UNION,
        )

        @classmethod
        def itervalues(cls):
            return cls._ALL_ENTRIES.__iter__()

    def __init__(self):
        self._defs = {}
        for kind in DatabaseBody.Kind.itervalues():
            self._defs[kind] = {}

    def register(self, kind, user_defined_type):
        assert isinstance(user_defined_type, (Typedef, Union, UserDefinedType))
        assert kind in DatabaseBody.Kind.itervalues()
        try:
            self.find_by_identifier(user_defined_type.identifier)
            assert False, user_defined_type.identifier
        except KeyError:
            pass
        self._defs[kind][user_defined_type.identifier] = user_defined_type

    def find_by_identifier(self, identifier):
        for defs_per_kind in self._defs.itervalues():
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
    def dictionaries(self):
        """Returns all dictionaries."""
        return self._view_by_kind(Database._Kind.DICTIONARY)

    @property
    def namespaces(self):
        """Returns all namespaces."""
        return self._view_by_kind(Database._Kind.NAMESPACE)

    @property
    def callback_functions(self):
        """Returns all callback functions."""
        return self._view_by_kind(Database._Kind.CALLBACK_FUNCTION)

    @property
    def callback_interfaces(self):
        """Returns all callback interfaces."""
        return self._view_by_kind(Database._Kind.CALLBACK_INTERFACE)

    @property
    def typedefs(self):
        """Returns all typedef definitions."""
        return self._view_by_kind(Database._Kind.TYPEDEF)

    @property
    def union_types(self):
        """Returns all union type definitions."""
        return self._view_by_kind(Database._Kind.UNION)

    def _view_by_kind(self, kind):
        return self._impl.find_by_kind(kind).viewvalues()
