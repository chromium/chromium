# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExposure
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithIdentifier
from .composition_parts import WithOwner
from .composition_parts import WithOwnerMixin
from .idl_type import IdlType
from .literal_constant import LiteralConstant
from .make_copy import make_copy


class Constant(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
               WithExposure, WithOwner, WithOwnerMixin, WithComponent,
               WithDebugInfo):
    """https://webidl.spec.whatwg.org/#idl-constants"""

    class IR(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithOwnerMixin, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     idl_type,
                     value,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            assert isinstance(idl_type, IdlType)
            assert isinstance(value, LiteralConstant)

            WithIdentifier.__init__(self, identifier)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithOwnerMixin.__init__(self)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.idl_type = idl_type
            self.value = value

    def __init__(self, ir, owner):
        assert isinstance(ir, Constant.IR)

        ir = make_copy(ir)
        WithIdentifier.__init__(self, ir)
        WithExtendedAttributes.__init__(self, ir, readonly=True)
        WithCodeGeneratorInfo.__init__(self, ir, readonly=True)
        WithExposure.__init__(self, ir, readonly=True)
        WithOwner.__init__(self, owner)
        WithOwnerMixin.__init__(self, ir)
        WithComponent.__init__(self, ir, readonly=True)
        WithDebugInfo.__init__(self, ir)

        self._idl_type = ir.idl_type
        self._value = ir.value

    @property
    def idl_type(self):
        """Returns the type"""
        return self._idl_type

    @property
    def is_static(self):
        return True

    @property
    def is_readonly(self):
        return True

    @property
    def value(self):
        """Returns the value."""
        return self._value
