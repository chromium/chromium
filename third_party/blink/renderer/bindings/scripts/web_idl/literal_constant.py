# Copyright 2019 The Chromium Authors. All rights reserved.
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
        - INTEGER_NUMERICS: an instance of long
        - FLOATING_POINTS: an instance of float
        - STRING: an instance of str
        """
        return self._value

    @property
    def literal(self):
        """Returns the literal representation."""
        return self._literal
