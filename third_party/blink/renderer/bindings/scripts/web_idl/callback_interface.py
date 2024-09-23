# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools

from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExposure
from .composition_parts import WithExtendedAttributes
from .constant import Constant
from .ir_map import IRMap
from .make_copy import make_copy
from .operation import Operation
from .operation import OperationGroup
from .user_defined_type import UserDefinedType


class CallbackInterface(UserDefinedType, WithExtendedAttributes,
                        WithCodeGeneratorInfo, WithExposure, WithComponent,
                        WithDebugInfo):
    """https://webidl.spec.whatwg.org/#idl-interfaces"""

    class IR(IRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     constants=None,
                     operations=None,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            if constants is None:
                constants = []
            if operations is None:
                operations = []
            assert isinstance(constants, (list, tuple))
            assert isinstance(operations, (list, tuple))
            assert all(
                isinstance(constant, Constant.IR) for constant in constants)
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

            IRMap.IR.__init__(
                self,
                identifier=identifier,
                kind=IRMap.IR.Kind.CALLBACK_INTERFACE)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.attributes = []
            self.constants = constants
            self.constructors = []
            self.constructor_groups = []
            self.legacy_factory_functions = []
            self.legacy_factory_function_groups = []
            self.operations = operations
            self.operation_groups = []

            self.inherited = None
            self.direct_subclasses = []
            self.subclasses = []
            self.tag = None
            self.max_subclass_tag = None

        def iter_all_members(self):
            list_of_members = [
                self.constants,
                self.operations,
            ]
            return itertools.chain(*list_of_members)

        def iter_all_overload_groups(self):
            return iter(self.operation_groups)

    def __init__(self, ir):
        assert isinstance(ir, CallbackInterface.IR)

        ir = make_copy(ir)
        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, ir, readonly=True)
        WithCodeGeneratorInfo.__init__(self, ir, readonly=True)
        WithExposure.__init__(self, ir, readonly=True)
        WithComponent.__init__(self, ir, readonly=True)
        WithDebugInfo.__init__(self, ir)
        self._constants = tuple([
            Constant(constant_ir, owner=self) for constant_ir in ir.constants
        ])
        self._operations = tuple([
            Operation(operation_ir, owner=self)
            for operation_ir in ir.operations
        ])
        self._operation_groups = tuple([
            OperationGroup(operation_group_ir,
                           list(
                               filter(
                                   lambda x: x.identifier == operation_group_ir
                                   .identifier, self._operations)),
                           owner=self)
            for operation_group_ir in ir.operation_groups
        ])
        self._tag = ir.tag
        self._max_subclass_tag = ir.max_subclass_tag

    @property
    def attributes(self):
        """Returns attributes."""
        return ()

    @property
    def constants(self):
        """Returns constants."""
        return self._constants

    @property
    def constructors(self):
        """Returns constructors."""
        return ()

    @property
    def constructor_groups(self):
        """Returns groups of constructors."""
        return ()

    @property
    def legacy_factory_functions(self):
        """Returns legacy factory functions."""
        return ()

    @property
    def legacy_factory_function_groups(self):
        """Returns groups of overloaded legacy factory functions."""
        return ()

    @property
    def operations(self):
        """Returns operations."""
        return self._operations

    @property
    def operation_groups(self):
        """Returns groups of overloaded operations."""
        return self._operation_groups

    @property
    def tag(self):
        """Returns a tag integer or None."""
        return self._tag

    @property
    def max_subclass_tag(self):
        """Returns a tag integer or None."""
        return self._max_subclass_tag

    # UserDefinedType overrides
    @property
    def is_callback_interface(self):
        return True
