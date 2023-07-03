# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .idl_type import IdlType


class LiteralConstant(object):
    """
    Represents a literal constant.

    Literal constants are used as:
    - constant values of IDL constants
    - default values of IDL operation's arguments
    - default values of IDL dictionary members
    """

    def __init__(self, idl_type=None, value=None, literal=None):
        assert isinstance(idl_type, IdlType)
        assert isinstance(literal, str)

        self._idl_type = idl_type
        self._value = value
        self._literal = literal

    @property
    def idl_type(self):
        """
        Returns the type of the literal constant.

        The types of the following literals are represented as follows.
        - null: any?
        - []: sequence<any>
        - {}: object
        - true / false: boolean
        - INTEGER_NUMERICS: long
        - FLOATING_POINTS: double
        - STRING: DOMString
        """
        return self._idl_type

    @property
    def value(self):
        """
        Returns the value as a Python value.

        The values of the following literals are represented as follows.
        - null: None
        - []: list()
        - {}: dict()
        - true / false: True / False
        - INTEGER_NUMERICS: an instance of int
        - FLOATING_POINTS: an instance of float
        - STRING: an instance of str
        """
        return self._value

    @property
    def literal(self):
        """Returns the literal representation."""
        return self._literal

    def is_type_compatible_with(self, idl_type):
        """Returns True if |idl_type| can store this value."""
        assert isinstance(idl_type, IdlType)

        idl_type = idl_type.unwrap(nullable=False)

        if idl_type.is_any:
            return True

        if self.idl_type.is_nullable:
            return idl_type.does_include_nullable_type

        idl_type = idl_type.unwrap()

        if idl_type.is_union:
            return any(
                self.is_type_compatible_with(member_type)
                for member_type in idl_type.flattened_member_types)

        if self.idl_type.is_sequence:
            return idl_type.is_sequence
        if self.idl_type.is_object:
            return idl_type.is_dictionary
        if self.idl_type.is_boolean:
            return idl_type.is_boolean
        if self.idl_type.is_integer:
            return idl_type.is_numeric
        if self.idl_type.is_floating_point_numeric:
            return idl_type.is_floating_point_numeric
        if self.idl_type.is_string:
            return idl_type.is_string or idl_type.is_enumeration
