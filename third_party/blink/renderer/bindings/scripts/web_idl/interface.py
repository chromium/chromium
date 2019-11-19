# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .attribute import Attribute
from .code_generator_info import CodeGeneratorInfo
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExposure
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithOwner
from .constant import Constant
from .constructor import Constructor
from .constructor import ConstructorGroup
from .exposure import Exposure
from .idl_type import IdlType
from .ir_map import IRMap
from .make_copy import make_copy
from .operation import Operation
from .operation import OperationGroup
from .reference import RefById
from .user_defined_type import UserDefinedType


class Interface(UserDefinedType, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithExposure, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-interfaces"""

    class IR(IRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     is_partial,
                     is_mixin,
                     inherited=None,
                     attributes=None,
                     constants=None,
                     constructors=None,
                     operations=None,
                     stringifier=None,
                     iterable=None,
                     maplike=None,
                     setlike=None,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            assert isinstance(is_partial, bool)
            assert isinstance(is_mixin, bool)
            assert inherited is None or isinstance(inherited, RefById)
            assert attributes is None or isinstance(attributes, (list, tuple))
            assert constants is None or isinstance(constants, (list, tuple))
            assert constructors is None or isinstance(constructors,
                                                      (list, tuple))
            assert operations is None or isinstance(operations, (list, tuple))
            assert stringifier is None or isinstance(stringifier,
                                                     Stringifier.IR)
            assert iterable is None or isinstance(iterable, Iterable)
            assert maplike is None or isinstance(maplike, Maplike)
            assert setlike is None or isinstance(setlike, Setlike)

            attributes = attributes or []
            constants = constants or []
            constructors = constructors or []
            operations = operations or []
            assert all(
                isinstance(attribute, Attribute.IR)
                for attribute in attributes)
            assert all(
                isinstance(constant, Constant.IR) for constant in constants)
            assert all(
                isinstance(constructor, Constructor.IR)
                for constructor in constructors)
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

            kind = None
            if is_partial:
                if is_mixin:
                    kind = IRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN
                else:
                    kind = IRMap.IR.Kind.PARTIAL_INTERFACE
            else:
                if is_mixin:
                    kind = IRMap.IR.Kind.INTERFACE_MIXIN
                else:
                    kind = IRMap.IR.Kind.INTERFACE
            IRMap.IR.__init__(self, identifier=identifier, kind=kind)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithComponent.__init__(self, component=component)
            WithDebugInfo.__init__(self, debug_info)

            self.is_partial = is_partial
            self.is_mixin = is_mixin
            self.inherited = inherited
            self.attributes = list(attributes)
            self.constants = list(constants)
            self.constructors = list(constructors)
            self.constructor_groups = []
            self.operations = list(operations)
            self.operation_groups = []
            self.stringifier = stringifier
            self.iterable = iterable
            self.maplike = maplike
            self.setlike = setlike

        def iter_all_members(self):
            for attribute in self.attributes:
                yield attribute
            for constant in self.constants:
                yield constant
            for constructor in self.constructors:
                yield constructor
            for operation in self.operations:
                yield operation

    def __init__(self, ir):
        assert isinstance(ir, Interface.IR)
        assert not ir.is_partial

        ir = make_copy(ir)
        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, ir.extended_attributes)
        WithCodeGeneratorInfo.__init__(
            self, CodeGeneratorInfo(ir.code_generator_info))
        WithExposure.__init__(self, Exposure(ir.exposure))
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info)

        self._is_mixin = ir.is_mixin
        self._inherited = ir.inherited
        self._attributes = tuple([
            Attribute(attribute_ir, owner=self)
            for attribute_ir in ir.attributes
        ])
        self._constants = tuple([
            Constant(constant_ir, owner=self) for constant_ir in ir.constants
        ])
        self._constructors = tuple([
            Constructor(constructor_ir, owner=self)
            for constructor_ir in ir.constructors
        ])
        self._constructor_groups = tuple([
            ConstructorGroup(
                constructor_group_ir,
                filter(
                    lambda x: x.identifier == constructor_group_ir.identifier,
                    self._constructors),
                owner=self) for constructor_group_ir in ir.constructor_groups
        ])
        assert len(self._constructor_groups) <= 1
        self._operations = tuple([
            Operation(operation_ir, owner=self)
            for operation_ir in ir.operations
        ])
        self._operation_groups = tuple([
            OperationGroup(
                operation_group_ir,
                filter(lambda x: x.identifier == operation_group_ir.identifier,
                       self._operations),
                owner=self) for operation_group_ir in ir.operation_groups
        ])
        self._stringifier = None
        if ir.stringifier:
            operations = filter(lambda x: x.is_stringifier, self._operations)
            assert len(operations) == 1
            attributes = [None]
            if ir.stringifier.attribute:
                attr_id = ir.stringifier.attribute.identifier
                attributes = filter(lambda x: x.identifier == attr_id,
                                    self._attributes)
            assert len(attributes) == 1
            self._stringifier = Stringifier(
                ir.stringifier,
                operation=operations[0],
                attribute=attributes[0],
                owner=self)
        self._iterable = ir.iterable
        self._maplike = ir.maplike
        self._setlike = ir.setlike

    @property
    def is_mixin(self):
        """Returns True if this is a mixin interface."""
        return self._is_mixin

    @property
    def inherited(self):
        """Returns the inherited interface or None."""
        return self._inherited.target_object if self._inherited else None

    @property
    def attributes(self):
        """
        Returns attributes, including [Unforgeable] attributes in ancestors.
        """
        return self._attributes

    @property
    def constants(self):
        """Returns constants."""
        return self._constants

    @property
    def constructors(self):
        """Returns constructors."""
        return self._constructors

    @property
    def constructor_groups(self):
        """
        Returns groups of constructors.

        Constructors are grouped as operations are. There is 0 or 1 group.
        """
        return self._constructor_groups

    @property
    def operations(self):
        """
        Returns all operations, including special operations without an
        identifier, as well as [Unforgeable] operations in ancestors.
        """
        return self._operations

    @property
    def operation_groups(self):
        """
        Returns groups of overloaded operations, including [Unforgeable]
        operations in ancestors.

        All operations that have an identifier are grouped by identifier, thus
        it's possible that there is a single operation in a certain operation
        group.  If an operation doesn't have an identifier, i.e. if it's a
        merely special operation, then the operation doesn't appear in any
        operation group.
        """
        return self._operation_groups

    @property
    def named_constructor(self):
        """Returns a named constructor or None."""
        assert False, "Not implemented yet."

    @property
    def exposed_interfaces(self):
        """
        Returns a tuple of interfaces that are exposed to this interface, if
        this is a global interface.  Returns None otherwise.
        """
        assert False, "Not implemented yet."

    # Special operations
    @property
    def indexed_property_handler(self):
        """
        Returns a set of handlers (getter/setter/deleter) for the indexed
        property.
        @return IndexedPropertyHandler?
        """
        # TODO: Include anonymous handlers of ancestors. https://crbug.com/695972
        assert False, "Not implemented yet."

    @property
    def named_property_handler(self):
        """
        Returns a set of handlers (getter/setter/deleter) for the named
        property.
        @return NamedPropertyHandler?
        """
        # TODO: Include anonymous handlers of ancestors. https://crbug.com/695972
        assert False, "Not implemented yet."

    @property
    def stringifier(self):
        """Returns a Stringifier or None."""
        return self._stringifier

    @property
    def iterable(self):
        """Returns an Iterable or None."""
        return self._iterable

    @property
    def maplike(self):
        """Returns a Maplike or None."""
        return self._maplike

    @property
    def setlike(self):
        """Returns a Setlike or None."""
        return self._setlike

    # UserDefinedType overrides
    @property
    def is_interface(self):
        return True


class Stringifier(WithOwner, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-stringifiers"""

    class IR(WithDebugInfo):
        def __init__(self, operation=None, attribute=None, debug_info=None):
            assert isinstance(operation, Operation.IR)
            assert attribute is None or isinstance(attribute, Attribute.IR)

            WithDebugInfo.__init__(self, debug_info)

            self.operation = operation
            self.attribute = attribute

    def __init__(self, ir, operation, attribute, owner):
        assert isinstance(ir, Stringifier.IR)
        assert isinstance(operation, Operation)
        assert attribute is None or isinstance(attribute, Attribute)

        WithOwner.__init__(self, owner)
        WithDebugInfo.__init__(self, ir.debug_info)

        self._operation = operation
        self._attribute = attribute

    @property
    def operation(self):
        return self._operation

    @property
    def attribute(self):
        return self._attribute


class Iterable(WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-iterable"""

    def __init__(self,
                 key_type=None,
                 value_type=None,
                 debug_info=None):
        assert key_type is None or isinstance(key_type, IdlType)
        # iterable is declared in either form of
        #     iterable<value_type>
        #     iterable<key_type, value_type>
        # thus |value_type| can't be None.  However, we put it after |key_type|
        # to be consistent with the format of IDL.
        assert isinstance(value_type, IdlType), "value_type must be specified"

        WithDebugInfo.__init__(self, debug_info)

        self._key_type = key_type
        self._value_type = value_type

    @property
    def key_type(self):
        """Returns the key type or None."""
        return self._key_type

    @property
    def value_type(self):
        """Returns the value type."""
        return self._value_type


class Maplike(WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-maplike"""

    def __init__(self,
                 key_type,
                 value_type,
                 is_readonly=False,
                 debug_info=None):
        assert isinstance(key_type, IdlType)
        assert isinstance(value_type, IdlType)
        assert isinstance(is_readonly, bool)

        WithDebugInfo.__init__(self, debug_info)

        self._key_type = key_type
        self._value_type = value_type
        self._is_readonly = is_readonly

    @property
    def key_type(self):
        """
        Returns its key type.
        @return IdlType
        """
        return self._key_type

    @property
    def value_type(self):
        """
        Returns its value type.
        @return IdlType
        """
        return self._value_type

    @property
    def is_readonly(self):
        """
        Returns True if it's readonly.
        @return bool
        """
        return self._is_readonly


class Setlike(WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-setlike"""

    def __init__(self,
                 value_type,
                 is_readonly=False,
                 debug_info=None):
        assert isinstance(value_type, IdlType)
        assert isinstance(is_readonly, bool)

        WithDebugInfo.__init__(self, debug_info)

        self._value_type = value_type
        self._is_readonly = is_readonly

    @property
    def value_type(self):
        """
        Returns its value type.
        @return IdlType
        """
        return self._value_type

    @property
    def is_readonly(self):
        """
        Returns True if it's readonly.
        @return bool
        """
        return self._is_readonly


class IndexedPropertyHandler(object):
    @property
    def getter(self):
        """
        Returns an Operation for indexed property getter.
        @return Operation?
        """
        assert False, "Not implemented yet."

    @property
    def setter(self):
        """
        Returns an Operation for indexed property setter.
        @return Operation?
        """
        assert False, "Not implemented yet."

    @property
    def deleter(self):
        """
        Returns an Operation for indexed property deleter.
        @return Operation?
        """
        assert False, "Not implemented yet."


class NamedPropertyHandler(object):
    @property
    def getter(self):
        """
        Returns an Operation for named property getter.
        @return Operation?
        """
        assert False, "Not implemented yet."

    @property
    def setter(self):
        """
        Returns an Operation for named property setter.
        @return Operation?
        """
        assert False, "Not implemented yet."

    @property
    def deleter(self):
        """
        Returns an Operation for named property deleter.
        @return Operation?
        """
        assert False, "Not implemented yet."
