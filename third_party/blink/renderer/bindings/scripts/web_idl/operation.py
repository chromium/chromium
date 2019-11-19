# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .argument import Argument
from .code_generator_info import CodeGeneratorInfo
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExposure
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithOwner
from .exposure import Exposure
from .function_like import FunctionLike
from .idl_type import IdlType
from .make_copy import make_copy
from .overload_group import OverloadGroup


class Operation(FunctionLike, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithExposure, WithOwner, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-operations"""

    class IR(FunctionLike.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     arguments,
                     return_type,
                     is_static=False,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            FunctionLike.IR.__init__(
                self,
                identifier=identifier,
                arguments=arguments,
                return_type=return_type,
                is_static=is_static)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithComponent.__init__(self, component=component)
            WithDebugInfo.__init__(self, debug_info)

            self.is_stringifier = False

    def __init__(self, ir, owner):
        assert isinstance(ir, Operation.IR)

        FunctionLike.__init__(self, ir)
        WithExtendedAttributes.__init__(self, ir.extended_attributes)
        WithCodeGeneratorInfo.__init__(
            self, CodeGeneratorInfo(ir.code_generator_info))
        WithExposure.__init__(self, Exposure(ir.exposure))
        WithOwner.__init__(self, owner)
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info)

        self._is_stringifier = ir.is_stringifier

    @property
    def is_stringifier(self):
        return self._is_stringifier


class OperationGroup(OverloadGroup, WithCodeGeneratorInfo, WithExposure,
                     WithOwner, WithDebugInfo):
    """
    Represents a group of operations with the same identifier.

    The number of operations in this group may be 1 or 2+.  In the latter case,
    the operations are overloaded.
    """

    class IR(OverloadGroup.IR, WithCodeGeneratorInfo, WithExposure,
             WithDebugInfo):
        def __init__(self,
                     operations,
                     code_generator_info=None,
                     debug_info=None):
            OverloadGroup.IR.__init__(self, operations)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithExposure.__init__(self)
            WithDebugInfo.__init__(self, debug_info)

    def __init__(self, ir, operations, owner):
        assert isinstance(ir, OperationGroup.IR)
        assert isinstance(operations, (list, tuple))
        assert all(
            isinstance(operation, Operation) for operation in operations)
        assert all(
            operation.identifier == ir.identifier for operation in operations)

        ir = make_copy(ir)
        OverloadGroup.__init__(self, functions=operations)
        WithCodeGeneratorInfo.__init__(
            self, CodeGeneratorInfo(ir.code_generator_info))
        WithExposure.__init__(self, Exposure(ir.exposure))
        WithOwner.__init__(self, owner)
        WithDebugInfo.__init__(self, ir.debug_info)
