# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .argument import Argument
from .ast_group import AstGroup
from .async_iterator import AsyncIterator
from .attribute import Attribute
from .callback_function import CallbackFunction
from .callback_interface import CallbackInterface
from .composition_parts import Component
from .composition_parts import DebugInfo
from .composition_parts import Identifier
from .composition_parts import Location
from .constant import Constant
from .constructor import Constructor
from .dictionary import Dictionary
from .dictionary import DictionaryMember
from .enumeration import Enumeration
from .extended_attribute import ExtendedAttribute
from .extended_attribute import ExtendedAttributes
from .idl_type import IdlTypeFactory
from .includes import Includes
from .interface import AsyncIterable
from .interface import Interface
from .interface import Iterable
from .interface import Maplike
from .interface import Setlike
from .literal_constant import LiteralConstant
from .make_copy import make_copy
from .namespace import Namespace
from .operation import Operation
from .sync_iterator import SyncIterator
from .typedef import Typedef


def load_and_register_idl_definitions(filepaths, register_ir,
                                      create_ref_to_idl_def, idl_type_factory):
    """
    Reads ASTs from |filepaths| and builds IRs from ASTs.

    Args:
        filepaths: Paths to pickle files that store AST nodes.
        register_ir: A callback function that registers the argument as an
            IR.
        create_ref_to_idl_def: A callback function that creates a reference
            to an IDL definition from the given identifier.
        idl_type_factory: All IdlType instances will be created through this
            factory.
    """
    assert callable(register_ir)

    for filepath in filepaths:
        asts = AstGroup.read_from_file(filepath)
        builder = _IRBuilder(
            component=Component(asts.component),
            for_testing=asts.for_testing,
            create_ref_to_idl_def=create_ref_to_idl_def,
            idl_type_factory=idl_type_factory)

        for file_node in asts:
            assert file_node.GetClass() == 'File'
            for top_level_node in file_node.GetChildren():
                register_ir(builder.build_top_level_def(top_level_node))


class _IRBuilder(object):
    def __init__(self, component, for_testing, create_ref_to_idl_def,
                 idl_type_factory):
        """
        Args:
            component: A Component to which the built IRs are associated.
            for_testing: True if the IDL definitions are meant for testing
                purpose only.
            create_ref_to_idl_def: A callback function that creates a reference
                to an IDL definition from the given identifier.
            idl_type_factory: All IdlType instances will be created through this
                factory.
        """
        assert isinstance(component, Component)
        assert isinstance(for_testing, bool)
        assert callable(create_ref_to_idl_def)
        assert isinstance(idl_type_factory, IdlTypeFactory)

        self._component = component
        self._for_testing = for_testing
        self._create_ref_to_idl_def = create_ref_to_idl_def
        self._idl_type_factory = idl_type_factory

    def build_top_level_def(self, node):
        build_functions = {
            'Callback': self._build_callback_function,
            'Dictionary': self._build_dictionary,
            'Enum': self._build_enumeration,
            'Includes': self._build_includes,
            'Interface': self._build_interface,
            'Namespace': self._build_namespace,
            'Typedef': self._build_typedef,
        }
        ir = build_functions[node.GetClass()](node)
        ir.code_generator_info.set_for_testing(self._for_testing)
        return ir

    # Builder functions for top-level definitions

    def _build_interface(self, node):
        if node.GetProperty('CALLBACK'):
            return self._build_callback_interface(node)

        identifier = Identifier(node.GetName())
        child_nodes = list(node.GetChildren())
        inherited = self._take_inheritance(child_nodes)
        stringifier_members = self._take_stringifier(child_nodes)
        async_iterable = self._take_async_iterable(
            child_nodes, interface_identifier=identifier)
        iterable = self._take_iterable(child_nodes,
                                       interface_identifier=identifier)
        maplike = self._take_maplike(
            child_nodes, interface_identifier=identifier)
        setlike = self._take_setlike(
            child_nodes, interface_identifier=identifier)
        extended_attributes = self._take_extended_attributes(child_nodes)

        members = [
            self._build_interface_member(
                child, interface_identifier=identifier)
            for child in child_nodes
        ]
        if stringifier_members:
            members.extend(filter(None, stringifier_members))
        attributes = []
        constants = []
        constructors = []
        operations = []
        for member in members:
            if isinstance(member, Attribute.IR):
                attributes.append(member)
            elif isinstance(member, Constant.IR):
                constants.append(member)
            elif isinstance(member, Constructor.IR):
                constructors.append(member)
            elif isinstance(member, Operation.IR):
                operations.append(member)
            else:
                assert False

        legacy_factory_functions = self._build_legacy_factory_function(node)

        return Interface.IR(identifier=identifier,
                            is_partial=bool(node.GetProperty('PARTIAL')),
                            is_mixin=bool(node.GetProperty('MIXIN')),
                            inherited=inherited,
                            attributes=attributes,
                            constants=constants,
                            constructors=constructors,
                            legacy_factory_functions=legacy_factory_functions,
                            operations=operations,
                            async_iterable=async_iterable,
                            iterable=iterable,
                            maplike=maplike,
                            setlike=setlike,
                            extended_attributes=extended_attributes,
                            component=self._component,
                            debug_info=self._build_debug_info(node))

    def _build_namespace(self, node):
        child_nodes = list(node.GetChildren())
        extended_attributes = self._take_extended_attributes(child_nodes)

        members = list(map(self._build_interface_member, child_nodes))
        attributes = []
        constants = []
        operations = []
        for member in members:
            if isinstance(member, Attribute.IR):
                member.is_static = True
                attributes.append(member)
            elif isinstance(member, Constant.IR):
                constants.append(member)
            elif isinstance(member, Operation.IR):
                member.is_static = True
                operations.append(member)
            else:
                assert False

        return Namespace.IR(
            identifier=Identifier(node.GetName()),
            is_partial=bool(node.GetProperty('PARTIAL')),
            attributes=attributes,
            constants=constants,
            operations=operations,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=self._build_debug_info(node))

    def _build_interface_member(self,
                                node,
                                fallback_extended_attributes=None,
                                interface_identifier=None):
        def build_attribute(node):
            child_nodes = list(node.GetChildren())
            idl_type = self._take_type(child_nodes)
            extended_attributes = self._take_extended_attributes(
                child_nodes) or fallback_extended_attributes
            assert not child_nodes
            return Attribute.IR(
                identifier=Identifier(node.GetName()),
                idl_type=idl_type,
                is_static=bool(node.GetProperty('STATIC')),
                is_readonly=bool(node.GetProperty('READONLY')),
                does_inherit_getter=bool(node.GetProperty('INHERIT')),
                extended_attributes=extended_attributes,
                component=self._component,
                debug_info=self._build_debug_info(node))

        def build_constant(node):
            child_nodes = list(node.GetChildren())
            value = self._take_constant_value(child_nodes)
            extended_attributes = self._take_extended_attributes(
                child_nodes) or fallback_extended_attributes
            assert len(child_nodes) == 1, child_nodes[0].GetClass()
            # idl_parser doesn't produce a 'Type' node for the type of a
            # constant, hence we need to skip one level.
            idl_type = self._build_type_internal(child_nodes)
            return Constant.IR(
                identifier=Identifier(node.GetName()),
                idl_type=idl_type,
                value=value,
                extended_attributes=extended_attributes,
                component=self._component,
                debug_info=self._build_debug_info(node))

        def build_constructor(node):
            assert isinstance(interface_identifier, Identifier)
            child_nodes = list(node.GetChildren())
            arguments = self._take_arguments(child_nodes)
            extended_attributes = self._take_extended_attributes(
                child_nodes) or fallback_extended_attributes
            assert not child_nodes
            return_type = self._idl_type_factory.reference_type(
                interface_identifier)
            return Constructor.IR(
                identifier=None,
                arguments=arguments,
                return_type=return_type,
                extended_attributes=extended_attributes,
                component=self._component,
                debug_info=self._build_debug_info(node))

        def build_operation(node):
            child_nodes = list(node.GetChildren())
            arguments = self._take_arguments(child_nodes)
            return_type = self._take_type(child_nodes)
            extended_attributes = self._take_extended_attributes(
                child_nodes) or fallback_extended_attributes
            assert not child_nodes
            return Operation.IR(
                identifier=Identifier(node.GetName()),
                arguments=arguments,
                return_type=return_type,
                is_static=bool(node.GetProperty('STATIC')),
                is_getter=bool(node.GetProperty('GETTER')),
                is_setter=bool(node.GetProperty('SETTER')),
                is_deleter=bool(node.GetProperty('DELETER')),
                extended_attributes=extended_attributes,
                component=self._component,
                debug_info=self._build_debug_info(node))

        build_functions = {
            'Attribute': build_attribute,
            'Const': build_constant,
            'Constructor': build_constructor,
            'Operation': build_operation,
        }
        return build_functions[node.GetClass()](node)

    def _build_legacy_factory_function(self, node):
        assert node.GetClass() == 'Interface'
        legacy_factory_functions = []

        for child in node.GetChildren():
            if child.GetClass() == 'ExtAttributes':
                interface_ext_attrs = child.GetChildren()
                break
        else:
            return legacy_factory_functions

        for ext_attr in interface_ext_attrs:
            if ext_attr.GetName() != 'LegacyFactoryFunction':
                continue
            call_node = ext_attr.GetChildren()[0]
            assert call_node.GetClass() == 'Call'
            child_nodes = list(call_node.GetChildren())
            arguments = self._take_arguments(child_nodes)
            return_type = self._idl_type_factory.reference_type(
                Identifier(node.GetName()))
            assert not child_nodes
            legacy_factory_functions.append(
                Constructor.IR(identifier=Identifier(call_node.GetName()),
                               arguments=arguments,
                               return_type=return_type,
                               component=self._component,
                               debug_info=self._build_debug_info(node)))

        return legacy_factory_functions

    def _build_dictionary(self, node):
        child_nodes = list(node.GetChildren())
        inherited = self._take_inheritance(child_nodes)
        extended_attributes = self._take_extended_attributes(child_nodes)
        own_members = list(map(self._build_dictionary_member, child_nodes))

        return Dictionary.IR(
            identifier=Identifier(node.GetName()),
            is_partial=bool(node.GetProperty('PARTIAL')),
            inherited=inherited,
            own_members=own_members,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=self._build_debug_info(node))

    def _build_dictionary_member(self, node):
        assert node.GetClass() == 'Key'

        child_nodes = list(node.GetChildren())
        is_required = bool(node.GetProperty('REQUIRED'))
        idl_type = self._take_type(child_nodes, is_optional=(not is_required))
        default_value = self._take_default_value(child_nodes)
        extended_attributes = self._take_extended_attributes(child_nodes)
        assert not child_nodes

        return DictionaryMember.IR(
            identifier=Identifier(node.GetName()),
            idl_type=idl_type,
            default_value=default_value,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=self._build_debug_info(node))

    def _build_callback_interface(self, node):
        assert node.GetProperty('CALLBACK')

        child_nodes = list(node.GetChildren())
        extended_attributes = self._take_extended_attributes(child_nodes)
        members = list(map(self._build_interface_member, child_nodes))
        constants = []
        operations = []
        for member in members:
            if isinstance(member, Constant.IR):
                constants.append(member)
            elif isinstance(member, Operation.IR):
                operations.append(member)
            else:
                assert False

        return CallbackInterface.IR(
            identifier=Identifier(node.GetName()),
            constants=constants,
            operations=operations,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=self._build_debug_info(node))

    def _build_callback_function(self, node):
        child_nodes = list(node.GetChildren())
        arguments = self._take_arguments(child_nodes)
        return_type = self._take_type(child_nodes)
        extended_attributes = self._take_extended_attributes(child_nodes)
        assert not child_nodes
        return CallbackFunction.IR(
            identifier=Identifier(node.GetName()),
            arguments=arguments,
            return_type=return_type,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=self._build_debug_info(node))

    def _build_enumeration(self, node):
        child_nodes = list(node.GetChildren())
        extended_attributes = self._take_extended_attributes(child_nodes)
        assert all(child.GetClass() == 'EnumItem' for child in child_nodes)
        values = [child.GetName() for child in child_nodes]
        return Enumeration.IR(
            identifier=Identifier(node.GetName()),
            values=values,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=self._build_debug_info(node))

    def _build_typedef(self, node):
        child_nodes = list(node.GetChildren())
        idl_type = self._take_type(child_nodes)
        assert not child_nodes

        return Typedef.IR(
            identifier=Identifier(node.GetName()),
            idl_type=idl_type,
            component=self._component,
            debug_info=self._build_debug_info(node))

    def _build_includes(self, node):
        return Includes.IR(
            interface_identifier=Identifier(node.GetName()),
            mixin_identifier=Identifier(node.GetProperty('REFERENCE')),
            component=self._component,
            debug_info=self._build_debug_info(node))

    # Helper functions sorted alphabetically

    def _build_arguments(self, node):
        def build_argument(node, index):
            assert node.GetClass() == 'Argument'
            child_nodes = list(node.GetChildren())
            is_optional = bool(node.GetProperty('OPTIONAL'))
            is_variadic = bool(self._take_is_variadic_argument(child_nodes))
            # The parser may place extended attributes on arguments, but they
            # should be applied to types.
            extended_attributes = self._take_extended_attributes(child_nodes)
            idl_type = self._take_type(
                child_nodes,
                is_optional=is_optional,
                is_variadic=is_variadic,
                extended_attributes=extended_attributes)
            default_value = self._take_default_value(child_nodes)
            assert not child_nodes
            return Argument.IR(
                identifier=Identifier(node.GetName()),
                index=index,
                idl_type=idl_type,
                default_value=default_value)

        assert node.GetClass() == 'Arguments'
        return [
            build_argument(node, i)
            for i, node in enumerate(node.GetChildren())
        ]

    def _build_async_iterable(self, node, interface_identifier):
        assert node.GetClass() == 'AsyncIterable'
        assert isinstance(interface_identifier, Identifier)
        child_nodes = list(node.GetChildren())
        arguments = self._take_arguments(child_nodes)
        extended_attributes = self._take_extended_attributes(child_nodes)
        types = list(map(self._build_type, child_nodes))
        assert len(types) == 1 or len(types) == 2
        iter_ops = self._create_async_iterable_operations(
            node, interface_identifier, arguments, extended_attributes)
        if len(types) == 1:  # value iterator
            key_type, value_type = (None, types[0])
            iter_ops[Identifier('values')].is_async_iterator = True
            operations = [iter_ops[Identifier('values')]]
        else:  # pair iterator
            key_type, value_type = types
            iter_ops[Identifier('entries')].is_async_iterator = True
            operations = list(iter_ops.values())
        return AsyncIterable.IR(key_type=key_type,
                                value_type=value_type,
                                operations=operations,
                                arguments=arguments,
                                extended_attributes=extended_attributes,
                                debug_info=self._build_debug_info(node))

    def _build_constant_value(self, node):
        assert node.GetClass() == 'Value'
        return self._build_literal_constant(node)

    def _build_debug_info(self, node):
        return DebugInfo(
            location=Location(
                filepath=node.GetProperty('FILENAME'),
                line_number=node.GetProperty('LINENO'),
                position=node.GetProperty('POSITION')))

    def _build_default_value(self, node):
        assert node.GetClass() == 'Default'
        return self._build_literal_constant(node)

    def _build_extended_attributes(self, node):
        def build_extended_attribute(node):
            key = node.GetName()
            values = node.GetProperty('VALUE', default=None)
            arguments = None
            name = None

            child_nodes = node.GetChildren()
            if child_nodes:
                assert len(child_nodes) == 1
                child = child_nodes[0]
                if child.GetClass() == 'Arguments':
                    arguments = list(
                        map(build_extattr_argument, child.GetChildren()))
                elif child.GetClass() == 'Call':
                    assert len(child.GetChildren()) == 1
                    grand_child = child.GetChildren()[0]
                    assert grand_child.GetClass() == 'Arguments'
                    # ExtendedAttribute is not designed to represent an
                    # operation, especially a complicated argument list.
                    # Discard the arguments.
                    arguments = ()
                    name = child.GetName()
                else:
                    assert False

            return ExtendedAttribute(
                key=key, values=values, arguments=arguments, name=name)

        def build_extattr_argument(node):
            assert node.GetClass() == 'Argument'

            child_nodes = node.GetChildren()
            assert len(child_nodes) == 1
            assert child_nodes[0].GetClass() == 'Type'

            type_node = child_nodes[0]
            type_children = type_node.GetChildren()
            assert len(type_children) == 1

            return (type_children[0].GetName(), node.GetName())

        assert node.GetClass() == 'ExtAttributes'
        return ExtendedAttributes(
            list(
                filter(None, map(build_extended_attribute,
                                 node.GetChildren()))))

    def _build_inheritance(self, node):
        assert node.GetClass() == 'Inherit'
        return self._create_ref_to_idl_def(
            Identifier(node.GetName()), self._build_debug_info(node))

    def _build_is_variadic_argument(self, node):
        # idl_parser produces the following tree to indicate an argument is
        # variadic.
        #   Arguments
        #     := [Argument, Argument, ...]
        #   Argument
        #     := [Type, Argument(Name='...')]  # Argument inside Argument
        assert node.GetClass() == 'Argument'
        assert node.GetName() == '...'
        return True

    def _build_iterable(self, node, interface_identifier):
        assert node.GetClass() == 'Iterable'
        assert isinstance(interface_identifier, Identifier)
        child_nodes = list(node.GetChildren())
        extended_attributes = self._take_extended_attributes(child_nodes)
        types = list(map(self._build_type, child_nodes))
        assert len(types) == 1 or len(types) == 2
        if len(types) == 1:  # value iterator
            key_type, value_type = (None, types[0])
            operations = None
        else:  # pair iterator
            key_type, value_type = types
            iter_ops = self._create_iterable_operations(
                node, interface_identifier, extended_attributes)
            iter_ops[Identifier('entries')].is_iterator = True
            operations = list(iter_ops.values())
        return Iterable.IR(key_type=key_type,
                           value_type=value_type,
                           operations=operations,
                           extended_attributes=extended_attributes,
                           debug_info=self._build_debug_info(node))

    def _build_literal_constant(self, node):
        assert not node.GetChildren()

        type_token = node.GetProperty('TYPE')
        value_token = node.GetProperty('VALUE')

        debug_info = self._build_debug_info(node)
        factory = self._idl_type_factory

        if type_token == 'NULL':
            idl_type = factory.nullable_type(
                inner_type=factory.simple_type(
                    name='any', debug_info=debug_info),
                debug_info=debug_info)
            assert value_token == 'NULL'
            value = None
            literal = 'null'
        elif type_token == 'boolean':
            idl_type = factory.simple_type(
                name='boolean', debug_info=debug_info)
            assert isinstance(value_token, bool)
            value = value_token
            literal = 'true' if value else 'false'
        elif type_token == 'integer':
            idl_type = factory.simple_type(name='long', debug_info=debug_info)
            assert isinstance(value_token, str)
            value = int(value_token, base=0)
            literal = value_token
        elif type_token == 'float':
            idl_type = factory.simple_type(
                name='double', debug_info=debug_info)
            assert isinstance(value_token, str)
            value = float(value_token)
            literal = value_token
        elif type_token == 'DOMString':
            idl_type = factory.simple_type(
                name='DOMString', debug_info=debug_info)
            assert isinstance(value_token, str)
            value = value_token
            literal = '"{}"'.format(value)
        elif type_token == 'sequence':
            idl_type = factory.sequence_type(
                element_type=factory.simple_type(
                    name='any', debug_info=debug_info),
                debug_info=debug_info)
            assert value_token == '[]'
            value = []
            literal = '[]'
        elif type_token == 'dictionary':
            idl_type = factory.simple_type(
                name='object', debug_info=debug_info)
            assert value_token == '{}'
            value = dict()
            literal = '{}'
        else:
            assert False, "Unknown literal type: {}".format(type_token)

        return LiteralConstant(idl_type=idl_type, value=value, literal=literal)

    def _build_maplike(self, node, interface_identifier):
        assert node.GetClass() == 'Maplike'
        assert isinstance(interface_identifier, Identifier)
        types = list(map(self._build_type, node.GetChildren()))
        assert len(types) == 2
        key_type, value_type = types
        is_readonly = bool(node.GetProperty('READONLY'))
        attributes = [
            self._create_attribute(
                Identifier('size'),
                'unsigned long',
                is_readonly=True,
                node=node),
        ]
        iter_map = self._create_iterable_operations(node, interface_identifier)
        iter_map[Identifier('entries')].is_iterator = True
        iter_ops = list(iter_map.values())
        read_ops = [
            self._create_operation(Identifier('get'),
                                   arguments=self._create_arguments([
                                       (Identifier('key'), key_type),
                                   ]),
                                   return_type='any',
                                   extended_attributes={
                                       'CallWith': 'ScriptState',
                                       'RaisesException': None,
                                       'ImplementedAs': 'getForBinding',
                                   },
                                   node=node),
            self._create_operation(Identifier('has'),
                                   arguments=self._create_arguments([
                                       (Identifier('key'), key_type),
                                   ]),
                                   return_type='boolean',
                                   extended_attributes={
                                       'CallWith': 'ScriptState',
                                       'RaisesException': None,
                                       'ImplementedAs': 'hasForBinding',
                                   },
                                   node=node),
        ]
        write_ops = [
            self._create_operation(
                Identifier('set'),
                arguments=self._create_arguments([
                    (Identifier('key'), key_type),
                    (Identifier('value'), value_type),
                ]),
                return_type=interface_identifier,
                extended_attributes={
                    'CallWith': 'ScriptState',
                    'RaisesException': None,
                    'ImplementedAs': 'setForBinding',
                },
                node=node),
            self._create_operation(
                Identifier('delete'),
                arguments=self._create_arguments([
                    (Identifier('key'), key_type),
                ]),
                return_type='boolean',
                extended_attributes={
                    'CallWith': 'ScriptState',
                    'RaisesException': None,
                    'ImplementedAs': 'deleteForBinding',
                },
                node=node),
            self._create_operation(
                Identifier('clear'),
                extended_attributes={
                    'CallWith': 'ScriptState',
                    'RaisesException': None,
                    'ImplementedAs': 'clearForBinding',
                },
                node=node),
        ]
        for op in write_ops:
            op.is_optionally_defined = True
        if is_readonly:
            operations = iter_ops + read_ops
        else:
            operations = iter_ops + read_ops + write_ops
        return Maplike.IR(
            key_type=key_type,
            value_type=value_type,
            is_readonly=is_readonly,
            attributes=attributes,
            operations=operations,
            debug_info=self._build_debug_info(node))

    def _build_setlike(self, node, interface_identifier):
        assert node.GetClass() == 'Setlike'
        assert isinstance(interface_identifier, Identifier)
        types = list(map(self._build_type, node.GetChildren()))
        assert len(types) == 1
        value_type = types[0]
        is_readonly = bool(node.GetProperty('READONLY'))
        attributes = [
            self._create_attribute(
                Identifier('size'),
                'unsigned long',
                is_readonly=True,
                node=node),
        ]
        iter_map = self._create_iterable_operations(node, interface_identifier)
        iter_map[Identifier('values')].is_iterator = True
        iter_ops = list(iter_map.values())
        read_ops = [
            self._create_operation(
                Identifier('has'),
                arguments=self._create_arguments([
                    (Identifier('value'), value_type),
                ]),
                return_type='boolean',
                extended_attributes={
                    'CallWith': 'ScriptState',
                    'RaisesException': None,
                    'ImplementedAs': 'hasForBinding',
                },
                node=node),
        ]
        write_ops = [
            self._create_operation(
                Identifier('add'),
                arguments=self._create_arguments([
                    (Identifier('value'), value_type),
                ]),
                return_type=interface_identifier,
                extended_attributes={
                    'CallWith': 'ScriptState',
                    'RaisesException': None,
                    'ImplementedAs': 'addForBinding',
                },
                node=node),
            self._create_operation(
                Identifier('delete'),
                arguments=self._create_arguments([
                    (Identifier('value'), value_type),
                ]),
                return_type='boolean',
                extended_attributes={
                    'CallWith': 'ScriptState',
                    'RaisesException': None,
                    'ImplementedAs': 'deleteForBinding',
                },
                node=node),
            self._create_operation(
                Identifier('clear'),
                extended_attributes={
                    'CallWith': 'ScriptState',
                    'RaisesException': None,
                    'ImplementedAs': 'clearForBinding',
                },
                node=node),
        ]
        for op in write_ops:
            op.is_optionally_defined = True
        if is_readonly:
            operations = iter_ops + read_ops
        else:
            operations = iter_ops + read_ops + write_ops
        return Setlike.IR(
            value_type=value_type,
            is_readonly=is_readonly,
            attributes=attributes,
            operations=operations,
            debug_info=self._build_debug_info(node))

    def _build_stringifier(self, node):
        # There are three forms of stringifier declaration;
        #   a. [ExtAttrs] stringifier;
        #   b. [ExtAttrs] stringifier DOMString foo();
        #   c. [ExtAttrs] stringifier attribute DOMString bar;
        # and we apply [ExtAttrs] to an operation in cases a and b, or to an
        # attribute in case c.

        assert node.GetClass() == 'Stringifier'
        child_nodes = node.GetChildren()
        extended_attributes = self._take_extended_attributes(child_nodes)
        assert len(child_nodes) <= 1

        member = None
        if len(child_nodes) == 1:
            member = self._build_interface_member(
                child_nodes[0],
                fallback_extended_attributes=extended_attributes)
            extended_attributes = None
        operation = member if isinstance(member, Operation.IR) else None
        attribute = member if isinstance(member, Attribute.IR) else None

        if operation is None:
            return_type = self._idl_type_factory.simple_type(
                name='DOMString', debug_info=self._build_debug_info(node))
            operation = Operation.IR(
                identifier=Identifier(''),
                arguments=[],
                return_type=return_type,
                extended_attributes=extended_attributes,
                component=self._component,
                debug_info=self._build_debug_info(node))
        operation.is_stringifier = True
        if attribute:
            operation.stringifier_attribute = attribute.identifier
            return (operation, attribute)
        else:
            return (operation, )

    def _build_type(self,
                    node,
                    is_optional=False,
                    is_variadic=False,
                    extended_attributes=None):
        assert node.GetClass() == 'Type'
        assert not (is_optional and is_variadic)
        idl_type = self._build_type_internal(
            node.GetChildren(),
            is_optional=is_optional,
            extended_attributes=extended_attributes)
        if node.GetProperty('NULLABLE'):
            idl_type = self._idl_type_factory.nullable_type(
                idl_type,
                is_optional=is_optional,
                debug_info=self._build_debug_info(node))
        if is_variadic:
            idl_type = self._idl_type_factory.variadic_type(
                idl_type, debug_info=self._build_debug_info(node))
        return idl_type

    def _build_type_internal(self,
                             nodes,
                             is_optional=False,
                             extended_attributes=None):
        """
        Args:
            nodes: The child nodes of a 'Type' node.
        """

        def build_frozen_array_type(node, extended_attributes):
            assert len(node.GetChildren()) == 1
            return self._idl_type_factory.frozen_array_type(
                element_type=self._build_type(node.GetChildren()[0]),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_observable_array_type(node, extended_attributes):
            assert len(node.GetChildren()) == 1
            return self._idl_type_factory.observable_array_type(
                element_type=self._build_type(node.GetChildren()[0]),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_promise_type(node, extended_attributes):
            assert len(node.GetChildren()) == 1
            return self._idl_type_factory.promise_type(
                result_type=self._build_type(node.GetChildren()[0]),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_union_type(node, extended_attributes):
            return self._idl_type_factory.union_type(
                member_types=list(map(self._build_type, node.GetChildren())),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_record_type(node, extended_attributes):
            key_node, value_node = node.GetChildren()
            return self._idl_type_factory.record_type(
                # idl_parser doesn't produce a 'Type' node for the key type,
                # hence we need to skip one level.
                key_type=self._build_type_internal([key_node]),
                value_type=self._build_type(value_node),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_reference_type(node, extended_attributes):
            return self._idl_type_factory.reference_type(
                Identifier(node.GetName()),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_sequence_type(node, extended_attributes):
            return self._idl_type_factory.sequence_type(
                element_type=self._build_type(node.GetChildren()[0]),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_simple_type(node, extended_attributes):
            name = node.GetName()
            if name is None:
                assert node.GetClass() in ('Any', 'Undefined')
                name = node.GetClass().lower()
            if node.GetProperty('UNRESTRICTED'):
                name = 'unrestricted {}'.format(name)
            return self._idl_type_factory.simple_type(
                name=name,
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        type_nodes = list(nodes)
        ext_attrs1 = extended_attributes
        ext_attrs2 = self._take_extended_attributes(type_nodes)
        if ext_attrs1 and ext_attrs2:
            extended_attributes = ExtendedAttributes(
                list(ext_attrs1) + list(ext_attrs2))
        else:
            extended_attributes = ext_attrs1 or ext_attrs2
        assert len(type_nodes) == 1
        body_node = type_nodes[0]

        buffer_source_types = set([
            'ArrayBuffer',
            'ArrayBufferView',  # Blink-specific ArrayBufferView definition
            'DataView',
            'Int8Array',
            'Int16Array',
            'Int32Array',
            'BigInt64Array',
            'Uint8Array',
            'Uint16Array',
            'Uint32Array',
            'BigUint64Array',
            'Uint8ClampedArray',
            'Float32Array',
            'Float64Array',
        ])
        if body_node.GetName() in buffer_source_types:
            return build_simple_type(
                body_node, extended_attributes=extended_attributes)

        build_functions = {
            'Any': build_simple_type,
            'FrozenArray': build_frozen_array_type,
            'ObservableArray': build_observable_array_type,
            'PrimitiveType': build_simple_type,
            'Promise': build_promise_type,
            'Record': build_record_type,
            'Sequence': build_sequence_type,
            'StringType': build_simple_type,
            'Typeref': build_reference_type,
            'Undefined': build_simple_type,
            'UnionType': build_union_type,
        }
        return build_functions[body_node.GetClass()](
            body_node, extended_attributes=extended_attributes)

    def _create_arguments(self, args):
        """
        Constructs a list of new instances of Argument.

        Args:
            args: A list of argument parameters.  Each argument parameter is
                a list of argument identifier, type name in str, and optional
                default value in str.
        """
        assert isinstance(args, (list, tuple))

        arguments = []
        index = 0
        for arg in args:
            assert isinstance(arg, (list, tuple))
            assert len(arg) == 2 or len(arg) == 3

            identifier = arg[0]
            if isinstance(arg[1], str):
                idl_type = self._create_type(
                    arg[1], is_optional=(len(arg) == 3))
            else:
                idl_type = arg[1]

            default_value = None
            if len(arg) == 3:
                default_value = self._create_literal_constant(arg[2])

            arguments.append(
                Argument.IR(
                    identifier,
                    index=index,
                    idl_type=idl_type,
                    default_value=default_value))

            index += 1

        return arguments

    def _create_async_iterable_operations(self, node, interface_identifier,
                                          arguments, extended_attributes):
        """
        Constructs a set of async iterable operations.

        https://webidl.spec.whatwg.org/#define-the-asynchronous-iteration-methods
        """
        def make_ext_attrs(key_values):
            return ExtendedAttributes(
                list(extended_attributes or []) +
                list(self._create_extended_attributes(key_values)))

        return {
            Identifier('entries'):
            self._create_operation(
                Identifier('entries'),
                arguments=make_copy(arguments),
                return_type=AsyncIterator.identifier_for(interface_identifier),
                extended_attributes=make_ext_attrs({
                    'CallWith':
                    'ScriptState',
                    'RaisesException':
                    None,
                    'ImplementedAs':
                    'entriesForBinding',
                }),
                node=node),
            Identifier('keys'):
            self._create_operation(
                Identifier('keys'),
                arguments=make_copy(arguments),
                return_type=AsyncIterator.identifier_for(interface_identifier),
                extended_attributes=make_ext_attrs({
                    'CallWith':
                    'ScriptState',
                    'RaisesException':
                    None,
                    'ImplementedAs':
                    'keysForBinding',
                }),
                node=node),
            Identifier('values'):
            self._create_operation(
                Identifier('values'),
                arguments=make_copy(arguments),
                return_type=AsyncIterator.identifier_for(interface_identifier),
                extended_attributes=make_ext_attrs({
                    'CallWith':
                    'ScriptState',
                    'RaisesException':
                    None,
                    'ImplementedAs':
                    'valuesForBinding',
                }),
                node=node),
        }

    def _create_attribute(self,
                          identifier,
                          idl_type,
                          is_readonly=False,
                          extended_attributes=None,
                          node=None):
        """Constructs a new Attribute.IR from simple parameters."""
        if isinstance(idl_type, str):
            idl_type = self._create_type(idl_type)
        if isinstance(extended_attributes, dict):
            extended_attributes = self._create_extended_attributes(
                extended_attributes)
        debug_info = self._build_debug_info(node) if node else None

        return Attribute.IR(
            identifier,
            idl_type=idl_type,
            is_readonly=is_readonly,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=debug_info)

    def _create_extended_attributes(self, key_values):
        """
        Constructs a new ExtendedAttributes from a dict of key and values.
        """
        assert isinstance(key_values, dict)

        return ExtendedAttributes([
            ExtendedAttribute(key=key, values=values)
            for key, values in key_values.items()
        ])

    def _create_iterable_operations(self,
                                    node,
                                    interface_identifier,
                                    extended_attributes=None):
        """
        Constructs a set of iterable operations.

        https://webidl.spec.whatwg.org/#define-the-iteration-methods
        """
        def make_ext_attrs(key_values):
            return ExtendedAttributes(
                list(extended_attributes or []) +
                list(self._create_extended_attributes(key_values)))

        return {
            Identifier('forEach'):
            self._create_operation(Identifier('forEach'),
                                   arguments=self._create_arguments([
                                       (Identifier('callback'),
                                        Identifier('ForEachIteratorCallback')),
                                       (Identifier('thisArg'), 'any', 'null'),
                                   ]),
                                   extended_attributes=make_ext_attrs({
                                       'CallWith':
                                       ('ScriptState', 'ThisValue'),
                                       'RaisesException':
                                       None,
                                       'ImplementedAs':
                                       'forEachForBinding',
                                   }),
                                   node=node),
            Identifier('entries'):
            self._create_operation(
                Identifier('entries'),
                return_type=SyncIterator.identifier_for(interface_identifier),
                extended_attributes=make_ext_attrs({
                    'CallWith':
                    'ScriptState',
                    'RaisesException':
                    None,
                    'ImplementedAs':
                    'entriesForBinding',
                }),
                node=node),
            Identifier('keys'):
            self._create_operation(
                Identifier('keys'),
                return_type=SyncIterator.identifier_for(interface_identifier),
                extended_attributes=make_ext_attrs({
                    'CallWith':
                    'ScriptState',
                    'RaisesException':
                    None,
                    'ImplementedAs':
                    'keysForBinding',
                }),
                node=node),
            Identifier('values'):
            self._create_operation(
                Identifier('values'),
                return_type=SyncIterator.identifier_for(interface_identifier),
                extended_attributes=make_ext_attrs({
                    'CallWith':
                    'ScriptState',
                    'RaisesException':
                    None,
                    'ImplementedAs':
                    'valuesForBinding',
                }),
                node=node),
        }

    def _create_literal_constant(self, token):
        factory = self._idl_type_factory
        if token == 'null':
            return LiteralConstant(
                idl_type=factory.nullable_type(
                    inner_type=factory.simple_type(name='any')),
                value=None,
                literal='null')
        else:
            assert False

    def _create_operation(self,
                          identifier,
                          arguments=None,
                          return_type=None,
                          extended_attributes=None,
                          node=None):
        """Constructs a new Operation.IR from simple parameters."""
        if not return_type:
            return_type = self._create_type('undefined')
        elif isinstance(return_type, str):
            return_type = self._create_type(return_type)
        if isinstance(extended_attributes, dict):
            extended_attributes = self._create_extended_attributes(
                extended_attributes)
        debug_info = self._build_debug_info(node) if node else None

        return Operation.IR(
            identifier,
            arguments=(arguments or []),
            return_type=return_type,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=debug_info)

    def _create_type(self, keyword_or_identifier, **kwargs):
        """Constructs a new IdlType from a type keyword or identifier."""
        name = keyword_or_identifier
        if isinstance(name, Identifier):
            return self._idl_type_factory.reference_type(name, **kwargs)
        elif isinstance(name, str):
            return self._idl_type_factory.simple_type(name, **kwargs)
        else:
            assert False

    def _take_and_build(self, node_class, build_func, node_list, **kwargs):
        """
        Takes a node of |node_class| from |node_list| if any, and then builds
        and returns an IR.  The processed node is removed from |node_list|.
        Returns None if not found.
        """
        for node in node_list:
            if node.GetClass() == node_class:
                node_list.remove(node)
                return build_func(node, **kwargs)
        return None

    def _take_arguments(self, node_list):
        return self._take_and_build('Arguments', self._build_arguments,
                                    node_list)

    def _take_async_iterable(self, node_list, **kwargs):
        return self._take_and_build('AsyncIterable',
                                    self._build_async_iterable, node_list,
                                    **kwargs)

    def _take_constant_value(self, node_list):
        return self._take_and_build('Value', self._build_constant_value,
                                    node_list)

    def _take_default_value(self, node_list):
        return self._take_and_build('Default', self._build_default_value,
                                    node_list)

    def _take_extended_attributes(self, node_list):
        return self._take_and_build('ExtAttributes',
                                    self._build_extended_attributes, node_list)

    def _take_inheritance(self, node_list):
        return self._take_and_build('Inherit', self._build_inheritance,
                                    node_list)

    def _take_is_variadic_argument(self, node_list):
        return self._take_and_build(
            'Argument', self._build_is_variadic_argument, node_list)

    def _take_iterable(self, node_list, **kwargs):
        return self._take_and_build('Iterable', self._build_iterable,
                                    node_list, **kwargs)

    def _take_maplike(self, node_list, **kwargs):
        return self._take_and_build('Maplike', self._build_maplike, node_list,
                                    **kwargs)

    def _take_setlike(self, node_list, **kwargs):
        return self._take_and_build('Setlike', self._build_setlike, node_list,
                                    **kwargs)

    def _take_stringifier(self, node_list):
        return self._take_and_build('Stringifier', self._build_stringifier,
                                    node_list)

    def _take_type(self, node_list, **kwargs):
        return self._take_and_build('Type', self._build_type, node_list,
                                    **kwargs)
