# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExtendedAttributes
from .ir_map import IRMap
from .make_copy import make_copy
from .user_defined_type import UserDefinedType


class Enumeration(UserDefinedType, WithExtendedAttributes,
                  WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
    """https://webidl.spec.whatwg.org/#idl-enums"""

    class IR(IRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     values,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            assert isinstance(values, (list, tuple))
            assert all(isinstance(value, str) for value in values)

            IRMap.IR.__init__(
                self, identifier=identifier, kind=IRMap.IR.Kind.ENUMERATION)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.values = list(values)

    def __init__(self, ir):
        assert isinstance(ir, Enumeration.IR)

        ir = make_copy(ir)
        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, ir, readonly=True)
        WithCodeGeneratorInfo.__init__(self, ir, readonly=True)
        WithComponent.__init__(self, ir, readonly=True)
        WithDebugInfo.__init__(self, ir)

        self._values = tuple(ir.values)

    @property
    def values(self):
        """Returns the list of enum values."""
        return self._values

    # UserDefinedType overrides
    @property
    def is_enumeration(self):
        return True
