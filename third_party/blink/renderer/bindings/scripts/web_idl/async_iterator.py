# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import Identifier
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExposure
from .composition_parts import WithExtendedAttributes
from .idl_type import IdlType
from .ir_map import IRMap
from .make_copy import make_copy
from .operation import Operation
from .operation import OperationGroup
from .reference import RefById
from .user_defined_type import UserDefinedType


class AsyncIterator(UserDefinedType, WithExtendedAttributes,
                    WithCodeGeneratorInfo, WithExposure, WithComponent,
                    WithDebugInfo):
    """
    Represents an async iterator type for 'asynchronous default iterator
    objects' [1], which exists for every interface that has a 'async iterator'
    [2].

    [1] https://webidl.spec.whatwg.org/#es-default-asynchronous-iterator-object
    [2] https://webidl.spec.whatwg.org/#es-asynchronous-iterable
    """
    @staticmethod
    def identifier_for(interface_identifier):
        """
        Returns the identifier of the asynchronous iterator type of the given
        interface type.
        """
        assert isinstance(interface_identifier, Identifier)
        return Identifier('AsyncIterator_{}'.format(interface_identifier))

    class IR(IRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithComponent, WithDebugInfo):
        def __init__(self,
                     interface,
                     component,
                     key_type=None,
                     value_type=None,
                     operations=None):
            assert isinstance(interface, RefById)
            assert key_type is None or isinstance(key_type, IdlType)
            assert isinstance(value_type, IdlType)
            assert isinstance(operations, (list, tuple))
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

            identifier = AsyncIterator.identifier_for(interface.identifier)

            IRMap.IR.__init__(self, identifier, IRMap.IR.Kind.ASYNC_ITERATOR)
            WithExtendedAttributes.__init__(self)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self)

            self.interface = interface
            self.key_type = key_type
            self.value_type = value_type
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
            return list(self.operations)

        def iter_all_overload_groups(self):
            return list(self.operation_groups)

    def __init__(self, ir):
        assert isinstance(ir, AsyncIterator.IR)

        ir = make_copy(ir)
        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, ir, readonly=True)
        WithCodeGeneratorInfo.__init__(self, ir, readonly=True)
        WithExposure.__init__(self, ir, readonly=True)
        WithComponent.__init__(self, ir, readonly=True)
        WithDebugInfo.__init__(self, ir)

        self._interface = ir.interface
        self._key_type = ir.key_type
        self._value_type = ir.value_type
        self._operations = tuple([
            Operation(operation_ir, owner=self)
            for operation_ir in ir.operations
        ])
        self._operation_groups = tuple([
            OperationGroup(
                group_ir,
                list(
                    filter(lambda x: x.identifier == group_ir.identifier,
                           self._operations)),
                owner=self) for group_ir in ir.operation_groups
        ])
        self._tag = ir.tag
        self._max_subclass_tag = ir.max_subclass_tag

    @property
    def interface(self):
        """Returns the interface that defines this async iterator."""
        return self._interface

    @property
    def key_type(self):
        """Returns the key type or None."""
        return self._key_type

    @property
    def value_type(self):
        """Returns the value type."""
        return self._value_type

    @property
    def inherited(self):
        # Just to be compatible with web_idl.Interface.
        return None

    @property
    def subclasses(self):
        # Just to be compatible with web_idl.Interface.
        return ()

    @property
    def attributes(self):
        """Returns attributes."""
        return ()

    @property
    def constants(self):
        """Returns constants."""
        return ()

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
    def is_async_iterator(self):
        return True
