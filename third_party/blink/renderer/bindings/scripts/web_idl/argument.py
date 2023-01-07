# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithIdentifier
from .composition_parts import WithOwner
from .idl_type import IdlType
from .literal_constant import LiteralConstant
from .make_copy import make_copy


class Argument(WithIdentifier, WithOwner):
    class IR(WithIdentifier):
        def __init__(self, identifier, index, idl_type, default_value=None):
            assert isinstance(index, int)
            assert isinstance(idl_type, IdlType)
            assert (default_value is None
                    or isinstance(default_value, LiteralConstant))

            WithIdentifier.__init__(self, identifier)

            self.index = index
            self.idl_type = idl_type
            self.default_value = default_value

    def __init__(self, ir, owner):
        assert isinstance(ir, Argument.IR)

        ir = make_copy(ir)
        WithIdentifier.__init__(self, ir.identifier)
        WithOwner.__init__(self, owner)

        self._index = ir.index
        self._idl_type = ir.idl_type
        self._default_value = ir.default_value

    @property
    def index(self):
        """Returns the argument index."""
        return self._index

    @property
    def idl_type(self):
        """Returns the type of the argument."""
        return self._idl_type

    @property
    def is_optional(self):
        """Returns True if this is an optional argument."""
        return self.idl_type.is_optional

    @property
    def is_variadic(self):
        """Returns True if this is a variadic argument."""
        return self.idl_type.is_variadic

    @property
    def optionality(self):
        """Returns the optionality value."""
        return self.idl_type.optionality

    @property
    def default_value(self):
        """Returns the default value or None."""
        return self._default_value
