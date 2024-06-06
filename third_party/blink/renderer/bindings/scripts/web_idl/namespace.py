# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools

from .attribute import Attribute
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


class Namespace(UserDefinedType, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithExposure, WithComponent, WithDebugInfo):
    """https://webidl.spec.whatwg.org/#idl-namespaces"""

    class IR(IRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     is_partial,
                     attributes=None,
                     constants=None,
                     operations=None,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            assert isinstance(is_partial, bool)
            assert attributes is None or isinstance(attributes, (list, tuple))
            assert constants is None or isinstance(constants, (list, tuple))
            assert operations is None or isinstance(operations, (list, tuple))

            attributes = attributes or []
            constants = constants or []
            operations = operations or []
            assert all(
                isinstance(attribute, Attribute.IR) and attribute.is_readonly
                and attribute.is_static for attribute in attributes)
            assert all(
                isinstance(constant, Constant.IR) for constant in constants)
            assert all(
                isinstance(operation, Operation.IR) and operation.identifier
                and operation.is_static for operation in operations)

            kind = (IRMap.IR.Kind.PARTIAL_NAMESPACE
                    if is_partial else IRMap.IR.Kind.NAMESPACE)
            IRMap.IR.__init__(self, identifier=identifier, kind=kind)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.is_partial = is_partial
            self.is_mixin = False
            self.attributes = list(attributes)
            self.constants = list(constants)
            self.constructors = []
            self.constructor_groups = []
            self.legacy_factory_functions = []
            self.legacy_factory_function_groups = []
            self.operations = list(operations)
            self.operation_groups = []

            self.inherited = None
            self.direct_subclasses = []
            self.subclasses = []
            self.tag = None
            self.max_subclass_tag = None

        def iter_all_members(self):
            list_of_members = [
                self.attributes,
                self.constants,
                self.operations,
            ]
            return itertools.chain(*list_of_members)

        def iter_all_overload_groups(self):
            return iter(self.operation_groups)

    def __init__(self, ir):
        assert isinstance(ir, Namespace.IR)
        assert not ir.is_partial

        ir = make_copy(ir)
        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, ir, readonly=True)
        WithCodeGeneratorInfo.__init__(self, ir, readonly=True)
        WithExposure.__init__(self, ir, readonly=True)
        WithComponent.__init__(self, ir, readonly=True)
        WithDebugInfo.__init__(self, ir)

        self._attributes = tuple([
            Attribute(attribute_ir, owner=self)
            for attribute_ir in ir.attributes
        ])
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
    def inherited(self):
        """Returns the inherited namespace or None."""
        return None

    @property
    def subclasses(self):
        """Returns the list of the derived namespaces."""
        return ()

    @property
    def attributes(self):
        """Returns attributes."""
        return self._attributes

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
        """Returns a list of OperationGroups."""
        return self._operation_groups

    @property
    def exposed_constructs(self):
        """Returns exposed constructs."""
        return ()

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
    def is_namespace(self):
        return True
