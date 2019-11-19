# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .argument import Argument
from .ast_group import AstGroup
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
from .interface import Interface
from .interface import Iterable
from .interface import Maplike
from .interface import Setlike
from .interface import Stringifier
from .literal_constant import LiteralConstant
from .namespace import Namespace
from .operation import Operation
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
        asts_per_component = AstGroup.read_from_file(filepath)
        component = Component(asts_per_component.component)
        builder = _IRBuilder(
            component=component,
            create_ref_to_idl_def=create_ref_to_idl_def,
            idl_type_factory=idl_type_factory)

        for file_node in asts_per_component:
            assert file_node.GetClass() == 'File'
            for top_level_node in file_node.GetChildren():
                register_ir(builder.build_top_level_def(top_level_node))


class _IRBuilder(object):
    def __init__(self, component, create_ref_to_idl_def, idl_type_factory):
        """
        Args:
            component: A Component to which the built IRs are associated.
            create_ref_to_idl_def: A callback function that creates a reference
                to an IDL definition from the given identifier.
            idl_type_factory: All IdlType instances will be created through this
                factory.
        """
        assert callable(create_ref_to_idl_def)
        assert isinstance(idl_type_factory, IdlTypeFactory)

        self._component = component
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
        return build_functions[node.GetClass()](node)

    # Builder functions for top-level definitions

    def _build_interface(self, node):
        if node.GetProperty('CALLBACK'):
            return self._build_callback_interface(node)

        child_nodes = list(node.GetChildren())
        inherited = self._take_inheritance(child_nodes)
        stringifier = self._take_stringifier(child_nodes)
        iterable = self._take_iterable(child_nodes)
        maplike = self._take_maplike(child_nodes)
        setlike = self._take_setlike(child_nodes)
        extended_attributes = self._take_extended_attributes(child_nodes)

        identifier = Identifier(node.GetName())
        members = [
            self._build_interface_member(
                child, interface_identifier=identifier)
            for child in child_nodes
        ]
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
                if member.identifier:
                    operations.append(member)
            else:
                assert False

        if stringifier:
            operations.append(stringifier.operation)
            if stringifier.attribute:
                attributes.append(stringifier.attribute)
        # TODO(peria): Create indexed/named property handlers from |operations|.

        return Interface.IR(
            identifier=identifier,
            is_partial=bool(node.GetProperty('PARTIAL')),
            is_mixin=bool(node.GetProperty('MIXIN')),
            inherited=inherited,
            attributes=attributes,
            constants=constants,
            constructors=constructors,
            operations=operations,
            stringifier=stringifier,
            iterable=iterable,
            maplike=maplike,
            setlike=setlike,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=self._build_debug_info(node))

    def _build_namespace(self, node):
        child_nodes = list(node.GetChildren())
        extended_attributes = self._take_extended_attributes(child_nodes)

        members = map(self._build_interface_member, child_nodes)
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
            assert len(child_nodes) == 0
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
            assert interface_identifier is not None
            child_nodes = list(node.GetChildren())
            arguments = self._take_arguments(child_nodes)
            extended_attributes = self._take_extended_attributes(
                child_nodes) or fallback_extended_attributes
            assert len(child_nodes) == 0
            return_type = self._idl_type_factory.reference_type(
                interface_identifier)
            return Constructor.IR(
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
            assert len(child_nodes) == 0
            return Operation.IR(
                identifier=Identifier(node.GetName()),
                arguments=arguments,
                return_type=return_type,
                is_static=bool(node.GetProperty('STATIC')),
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

    def _build_dictionary(self, node):
        child_nodes = list(node.GetChildren())
        inherited = self._take_inheritance(child_nodes)
        extended_attributes = self._take_extended_attributes(child_nodes)
        own_members = map(self._build_dictionary_member, child_nodes)

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
        assert len(child_nodes) == 0

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
        members = map(self._build_interface_member, child_nodes)
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
        assert len(child_nodes) == 0
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
        assert len(child_nodes) == 0

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
            assert len(child_nodes) == 0
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

            # Drop constructors as they do not fit in ExtendedAttribute which
            # doesn't support IdlType.
            if key in ('Constructor', 'CustomConstructor', 'NamedConstructor'):
                return None

            child_nodes = node.GetChildren()
            if child_nodes:
                assert len(child_nodes) == 1
                assert child_nodes[0].GetClass() == 'Arguments'
                arguments = map(build_extattr_argument,
                                child_nodes[0].GetChildren())

            return ExtendedAttribute(
                key=key, values=values, arguments=arguments)

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
            filter(None, map(build_extended_attribute, node.GetChildren())))

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

    def _build_iterable(self, node):
        assert node.GetClass() == 'Iterable'
        types = map(self._build_type, node.GetChildren())
        if len(types) == 1:
            types.insert(0, None)
        assert len(types) == 2
        return Iterable(
            key_type=types[0],
            value_type=types[1],
            debug_info=self._build_debug_info(node))

    def _build_literal_constant(self, node):
        assert len(node.GetChildren()) == 0

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
            value = long(value_token, base=0)
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
            assert False, 'Unknown literal type: {}'.format(type_token)

        return LiteralConstant(idl_type=idl_type, value=value, literal=literal)

    def _build_maplike(self, node):
        assert node.GetClass() == 'Maplike'
        types = map(self._build_type, node.GetChildren())
        assert len(types) == 2
        return Maplike(
            key_type=types[0],
            value_type=types[1],
            is_readonly=bool(node.GetProperty('READONLY')),
            debug_info=self._build_debug_info(node))

    def _build_setlike(self, node):
        assert node.GetClass() == 'Setlike'
        assert len(node.GetChildren()) == 1
        return Setlike(
            value_type=self._build_type(node.GetChildren()[0]),
            is_readonly=bool(node.GetProperty('READONLY')),
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

        return Stringifier.IR(
            operation=operation,
            attribute=attribute,
            debug_info=self._build_debug_info(node))

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

        def build_promise_type(node, extended_attributes):
            assert len(node.GetChildren()) == 1
            return self._idl_type_factory.promise_type(
                result_type=self._build_type(node.GetChildren()[0]),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_union_type(node, extended_attributes):
            return self._idl_type_factory.union_type(
                member_types=map(self._build_type, node.GetChildren()),
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
                assert node.GetClass() == 'Any'
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

        build_functions = {
            'Any': build_simple_type,
            'FrozenArray': build_frozen_array_type,
            'PrimitiveType': build_simple_type,
            'Promise': build_promise_type,
            'Record': build_record_type,
            'Sequence': build_sequence_type,
            'StringType': build_simple_type,
            'Typeref': build_reference_type,
            'UnionType': build_union_type,
        }
        return build_functions[body_node.GetClass()](
            body_node, extended_attributes=extended_attributes)

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

    def _take_iterable(self, node_list):
        return self._take_and_build('Iterable', self._build_iterable,
                                    node_list)

    def _take_maplike(self, node_list):
        return self._take_and_build('Maplike', self._build_maplike, node_list)

    def _take_setlike(self, node_list):
        return self._take_and_build('Setlike', self._build_setlike, node_list)

    def _take_stringifier(self, node_list):
        return self._take_and_build('Stringifier', self._build_stringifier,
                                    node_list)

    def _take_type(self,
                   node_list,
                   is_optional=False,
                   is_variadic=False,
                   extended_attributes=None):
        return self._take_and_build(
            'Type',
            self._build_type,
            node_list,
            is_optional=is_optional,
            is_variadic=is_variadic,
            extended_attributes=extended_attributes)
