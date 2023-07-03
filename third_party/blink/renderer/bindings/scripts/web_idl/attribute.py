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
from .make_copy import make_copy


class Attribute(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithExposure, WithOwner, WithOwnerMixin, WithComponent,
                WithDebugInfo):
    """https://webidl.spec.whatwg.org/#idl-attributes"""

    class IR(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithOwnerMixin, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     idl_type,
                     is_static=False,
                     is_readonly=False,
                     does_inherit_getter=False,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            assert isinstance(idl_type, IdlType)
            assert isinstance(is_static, bool)
            assert isinstance(is_readonly, bool)
            assert isinstance(does_inherit_getter, bool)

            WithIdentifier.__init__(self, identifier)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithOwnerMixin.__init__(self)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.idl_type = idl_type
            self.is_static = is_static
            self.is_readonly = is_readonly
            self.does_inherit_getter = does_inherit_getter

    def __init__(self, ir, owner):
        assert isinstance(ir, Attribute.IR)

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
        self._is_static = ir.is_static
        self._is_readonly = ir.is_readonly
        self._does_inherit_getter = ir.does_inherit_getter

    @property
    def idl_type(self):
        """Returns the type."""
        return self._idl_type

    @property
    def is_static(self):
        """Returns True if this attriute is static."""
        return self._is_static

    @property
    def is_readonly(self):
        """Returns True if this attribute is read only."""
        return self._is_readonly

    @property
    def does_inherit_getter(self):
        """
        Returns True if this attribute inherits its getter.
        https://webidl.spec.whatwg.org/#dfn-inherit-getter
        """
        return self._does_inherit_getter
