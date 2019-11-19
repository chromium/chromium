# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# pylint: disable=relative-import

"""Blink IDL Intermediate Representation (IR) classes.

Classes are primarily constructors, which build an IdlDefinitions object
(and various contained objects) from an AST (produced by blink_idl_parser).

IR stores typedefs and they are resolved by the code generator.

Typedef resolution uses some auxiliary classes and OOP techniques to make this
a generic call. See TypedefResolver class in code_generator_v8.py.

Class hierarchy (mostly containment, '<' for inheritance):

IdlDefinitions
    IdlCallbackFunction < TypedObject
    IdlEnum :: FIXME: remove, just use a dict for enums
    IdlInterface
        IdlAttribute < TypedObject
        IdlConstant < TypedObject
        IdlLiteral
        IdlOperation < TypedObject
            IdlArgument < TypedObject
        IdlStringifier
        IdlIterable < IdlIterableOrMaplikeOrSetlike
        IdlMaplike < IdlIterableOrMaplikeOrSetlike
        IdlSetlike < IdlIterableOrMaplikeOrSetlike

TypedObject :: Object with one or more attributes that is a type.

IdlArgument is 'picklable', as it is stored in interfaces_info.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import abc

from idl_types import IdlAnnotatedType
from idl_types import IdlFrozenArrayType
from idl_types import IdlNullableType
from idl_types import IdlRecordType
from idl_types import IdlSequenceType
from idl_types import IdlType
from idl_types import IdlUnionType

SPECIAL_KEYWORD_LIST = ['GETTER', 'SETTER', 'DELETER']


################################################################################
# TypedObject
################################################################################

class TypedObject(object):
    """Object with a type, such as an Attribute or Operation (return value).

    The type can be an actual type, or can be a typedef, which must be resolved
    by the TypedefResolver before passing data to the code generator.
    """
    __metaclass__ = abc.ABCMeta
    idl_type_attributes = ('idl_type',)


################################################################################
# Definitions (main container class)
################################################################################

class IdlDefinitions(object):
    def __init__(self, node):
        """Args: node: AST root node, class == 'File'"""
        self.callback_functions = {}
        self.dictionaries = {}
        self.enumerations = {}
        self.includes = []
        self.interfaces = {}
        self.first_name = None
        self.typedefs = {}

        node_class = node.GetClass()
        if node_class != 'File':
            raise ValueError('Unrecognized node class: %s' % node_class)

        children = node.GetChildren()
        for child in children:
            child_class = child.GetClass()
            if child_class == 'Interface':
                interface = IdlInterface(child)
                self.interfaces[interface.name] = interface
                if not self.first_name:
                    self.first_name = interface.name
            elif child_class == 'Typedef':
                typedef = IdlTypedef(child)
                self.typedefs[typedef.name] = typedef
            elif child_class == 'Enum':
                enumeration = IdlEnum(child)
                self.enumerations[enumeration.name] = enumeration
            elif child_class == 'Callback':
                callback_function = IdlCallbackFunction(child)
                self.callback_functions[callback_function.name] = callback_function
            elif child_class == 'Includes':
                self.includes.append(IdlIncludes(child))
            elif child_class == 'Dictionary':
                dictionary = IdlDictionary(child)
                self.dictionaries[dictionary.name] = dictionary
                if not self.first_name:
                    self.first_name = dictionary.name
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)

    def accept(self, visitor):
        visitor.visit_definitions(self)
        for interface in self.interfaces.itervalues():
            interface.accept(visitor)
        for callback_function in self.callback_functions.itervalues():
            callback_function.accept(visitor)
        for dictionary in self.dictionaries.itervalues():
            dictionary.accept(visitor)
        for enumeration in self.enumerations.itervalues():
            enumeration.accept(visitor)
        for include in self.includes:
            include.accept(visitor)
        for typedef in self.typedefs.itervalues():
            typedef.accept(visitor)

    def update(self, other):
        """Update with additional IdlDefinitions."""
        for interface_name, new_interface in other.interfaces.iteritems():
            if not new_interface.is_partial:
                # Add as new interface
                self.interfaces[interface_name] = new_interface
                continue

            # Merge partial to existing interface
            try:
                self.interfaces[interface_name].merge(new_interface)
            except KeyError:
                raise Exception('Tried to merge partial interface for {0}, '
                                'but no existing interface by that name'
                                .format(interface_name))

            # Merge callbacks and enumerations
            self.enumerations.update(other.enumerations)
            self.callback_functions.update(other.callback_functions)


################################################################################
# Callback Functions
################################################################################

class IdlCallbackFunction(TypedObject):
    def __init__(self, node):
        children = node.GetChildren()
        num_children = len(children)
        if num_children < 2 or num_children > 3:
            raise ValueError('Expected 2 or 3 children, got %s' % num_children)
        type_node = children[0]
        arguments_node = children[1]
        if num_children == 3:
            ext_attributes_node = children[2]
            self.extended_attributes = (
                ext_attributes_node_to_extended_attributes(ext_attributes_node))
        else:
            self.extended_attributes = {}
        arguments_node_class = arguments_node.GetClass()
        if arguments_node_class != 'Arguments':
            raise ValueError('Expected Arguments node, got %s' % arguments_node_class)

        self.name = node.GetName()
        self.idl_type = type_node_to_type(type_node)
        self.arguments = arguments_node_to_arguments(arguments_node)

    def accept(self, visitor):
        visitor.visit_callback_function(self)
        for argument in self.arguments:
            argument.accept(visitor)


################################################################################
# Dictionary
################################################################################

class IdlDictionary(object):
    def __init__(self, node):
        self.extended_attributes = {}
        self.is_partial = bool(node.GetProperty('PARTIAL'))
        self.name = node.GetName()
        self.members = []
        self.parent = None
        for child in node.GetChildren():
            child_class = child.GetClass()
            if child_class == 'Inherit':
                self.parent = child.GetName()
            elif child_class == 'Key':
                self.members.append(IdlDictionaryMember(child))
            elif child_class == 'ExtAttributes':
                self.extended_attributes = (
                    ext_attributes_node_to_extended_attributes(child))
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)

    def accept(self, visitor):
        visitor.visit_dictionary(self)
        for member in self.members:
            member.accept(visitor)


class IdlDictionaryMember(TypedObject):
    def __init__(self, node):
        self.default_value = None
        self.extended_attributes = {}
        self.idl_type = None
        self.is_required = bool(node.GetProperty('REQUIRED'))
        self.name = node.GetName()
        for child in node.GetChildren():
            child_class = child.GetClass()
            if child_class == 'Type':
                self.idl_type = type_node_to_type(child)
            elif child_class == 'Default':
                self.default_value = default_node_to_idl_literal(child)
            elif child_class == 'ExtAttributes':
                self.extended_attributes = (
                    ext_attributes_node_to_extended_attributes(child))
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)

    def accept(self, visitor):
        visitor.visit_dictionary_member(self)


################################################################################
# Enumerations
################################################################################

class IdlEnum(object):
    def __init__(self, node):
        self.name = node.GetName()
        self.values = []
        for child in node.GetChildren():
            self.values.append(child.GetName())

    def accept(self, visitor):
        visitor.visit_enumeration(self)


################################################################################
# Typedefs
################################################################################

class IdlTypedef(object):
    idl_type_attributes = ('idl_type',)

    def __init__(self, node):
        self.name = node.GetName()
        self.idl_type = typedef_node_to_type(node)

    def accept(self, visitor):
        visitor.visit_typedef(self)


################################################################################
# Interfaces
################################################################################

class IdlInterface(object):
    def __init__(self, node):
        self.attributes = []
        self.constants = []
        self.constructors = []
        self.custom_constructors = []
        self.extended_attributes = {}
        self.operations = []
        self.parent = None
        self.stringifier = None
        self.iterable = None
        self.has_indexed_elements = False
        self.has_named_property_getter = False
        self.maplike = None
        self.setlike = None
        self.original_interface = None
        self.partial_interfaces = []

        self.is_callback = bool(node.GetProperty('CALLBACK'))
        self.is_partial = bool(node.GetProperty('PARTIAL'))
        self.is_mixin = bool(node.GetProperty('MIXIN'))
        self.name = node.GetName()
        self.idl_type = IdlType(self.name)

        has_indexed_property_getter = False
        has_integer_typed_length = False

        def is_blacklisted_attribute_type(idl_type):
            return idl_type.is_callback_function or \
                idl_type.is_dictionary or \
                idl_type.is_record_type or \
                idl_type.is_sequence_type

        children = node.GetChildren()
        for child in children:
            child_class = child.GetClass()
            if child_class == 'Attribute':
                attr = IdlAttribute(child)
                if is_blacklisted_attribute_type(attr.idl_type):
                    raise ValueError('Type "%s" cannot be used as an attribute.' % attr.idl_type)
                if attr.idl_type.is_integer_type and attr.name == 'length':
                    has_integer_typed_length = True
                self.attributes.append(attr)
            elif child_class == 'Const':
                self.constants.append(IdlConstant(child))
            elif child_class == 'ExtAttributes':
                extended_attributes = ext_attributes_node_to_extended_attributes(child)
                self.constructors, self.custom_constructors = (
                    extended_attributes_to_constructors(extended_attributes))
                clear_constructor_attributes(extended_attributes)
                self.extended_attributes = extended_attributes
            elif child_class == 'Operation':
                op = IdlOperation(child)
                if 'getter' in op.specials:
                    if str(op.arguments[0].idl_type) == 'unsigned long':
                        has_indexed_property_getter = True
                    elif str(op.arguments[0].idl_type) == 'DOMString':
                        self.has_named_property_getter = True
                self.operations.append(op)
            elif child_class == 'Inherit':
                self.parent = child.GetName()
            elif child_class == 'Stringifier':
                self.stringifier = IdlStringifier(child)
                self.process_stringifier()
            elif child_class == 'Iterable':
                self.iterable = IdlIterable(child)
            elif child_class == 'Maplike':
                self.maplike = IdlMaplike(child)
            elif child_class == 'Setlike':
                self.setlike = IdlSetlike(child)
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)

        if len(filter(None, [self.iterable, self.maplike, self.setlike])) > 1:
            raise ValueError('Interface can only have one of iterable<>, maplike<> and setlike<>.')

        # TODO(rakuco): This validation logic should be in v8_interface according to bashi@.
        # At the moment, doing so does not work because several IDL files are partial Window
        # interface definitions, and interface_dependency_resolver.py doesn't seem to have any logic
        # to prevent these partial interfaces from resetting has_named_property to False.
        if 'LegacyUnenumerableNamedProperties' in self.extended_attributes and \
           not self.has_named_property_getter:
            raise ValueError('[LegacyUnenumerableNamedProperties] can be used only in interfaces '
                             'that support named properties.')

        if has_integer_typed_length and has_indexed_property_getter:
            self.has_indexed_elements = True
        else:
            if self.iterable is not None and self.iterable.key_type is None:
                raise ValueError('Value iterators (iterable<V>) must be accompanied by an indexed '
                                 'property getter and an integer-typed length attribute.')

        if 'Unforgeable' in self.extended_attributes:
            raise ValueError('[Unforgeable] cannot appear on interfaces.')

    def accept(self, visitor):
        visitor.visit_interface(self)
        for attribute in self.attributes:
            attribute.accept(visitor)
        for constant in self.constants:
            constant.accept(visitor)
        for constructor in self.constructors:
            constructor.accept(visitor)
        for custom_constructor in self.custom_constructors:
            custom_constructor.accept(visitor)
        for operation in self.operations:
            operation.accept(visitor)
        if self.iterable:
            self.iterable.accept(visitor)
        elif self.maplike:
            self.maplike.accept(visitor)
        elif self.setlike:
            self.setlike.accept(visitor)

    def process_stringifier(self):
        """Add the stringifier's attribute or named operation child, if it has
        one, as a regular attribute/operation of this interface."""
        if self.stringifier.attribute:
            self.attributes.append(self.stringifier.attribute)
        elif self.stringifier.operation:
            self.operations.append(self.stringifier.operation)

    def merge(self, other):
        """Merge in another interface's members (e.g., partial interface)"""
        self.attributes.extend(other.attributes)
        self.constants.extend(other.constants)
        self.operations.extend(other.operations)
        if self.stringifier is None:
            self.stringifier = other.stringifier


################################################################################
# Attributes
################################################################################

class IdlAttribute(TypedObject):
    def __init__(self, node=None):
        self.is_read_only = bool(node.GetProperty('READONLY')) if node else False
        self.is_static = bool(node.GetProperty('STATIC')) if node else False
        self.name = node.GetName() if node else None
        self.idl_type = None
        self.extended_attributes = {}
        # In what interface the attribute is (originally) defined when the
        # attribute is inherited from an ancestor interface.
        self.defined_in = None

        if node:
            children = node.GetChildren()
            for child in children:
                child_class = child.GetClass()
                if child_class == 'Type':
                    self.idl_type = type_node_to_type(child)
                elif child_class == 'ExtAttributes':
                    self.extended_attributes = ext_attributes_node_to_extended_attributes(child)
                else:
                    raise ValueError('Unrecognized node class: %s' % child_class)

        if 'Unforgeable' in self.extended_attributes and self.is_static:
            raise ValueError('[Unforgeable] cannot appear on static attributes.')

    def accept(self, visitor):
        visitor.visit_attribute(self)


################################################################################
# Constants
################################################################################

class IdlConstant(TypedObject):
    def __init__(self, node):
        children = node.GetChildren()
        num_children = len(children)
        if num_children < 2 or num_children > 3:
            raise ValueError('Expected 2 or 3 children, got %s' % num_children)
        type_node = children[0]
        value_node = children[1]
        value_node_class = value_node.GetClass()
        if value_node_class != 'Value':
            raise ValueError('Expected Value node, got %s' % value_node_class)

        self.name = node.GetName()
        # ConstType is more limited than Type, so subtree is smaller and
        # we don't use the full type_node_to_type function.
        self.idl_type = type_node_inner_to_type(type_node)
        self.value = value_node.GetProperty('VALUE')
        # In what interface the attribute is (originally) defined when the
        # attribute is inherited from an ancestor interface.
        self.defined_in = None

        if num_children == 3:
            ext_attributes_node = children[2]
            self.extended_attributes = ext_attributes_node_to_extended_attributes(ext_attributes_node)
        else:
            self.extended_attributes = {}

    def accept(self, visitor):
        visitor.visit_constant(self)


################################################################################
# Literals
################################################################################

class IdlLiteral(object):
    def __init__(self, idl_type, value):
        self.idl_type = idl_type
        self.value = value
        self.is_null = False

    def __str__(self):
        if self.idl_type == 'DOMString':
            if self.value:
                return '"%s"' % self.value
            else:
                return 'WTF::g_empty_string'
        if self.idl_type == 'integer':
            return '%d' % self.value
        if self.idl_type == 'float':
            return '%g' % self.value
        if self.idl_type == 'boolean':
            return 'true' if self.value else 'false'
        if self.idl_type == 'dictionary':
            return self.value
        raise ValueError('Unsupported literal type: %s' % self.idl_type)


class IdlLiteralNull(IdlLiteral):
    def __init__(self):
        self.idl_type = 'NULL'
        self.value = None
        self.is_null = True

    def __str__(self):
        return 'nullptr'


def default_node_to_idl_literal(node):
    idl_type = node.GetProperty('TYPE')
    value = node.GetProperty('VALUE')
    if idl_type == 'DOMString':
        if '"' in value or '\\' in value:
            raise ValueError('Unsupported string value: %r' % value)
        return IdlLiteral(idl_type, value)
    if idl_type == 'integer':
        return IdlLiteral(idl_type, int(value, base=0))
    if idl_type == 'float':
        return IdlLiteral(idl_type, float(value))
    if idl_type in ['boolean', 'sequence']:
        return IdlLiteral(idl_type, value)
    if idl_type == 'NULL':
        return IdlLiteralNull()
    if idl_type == 'dictionary':
        return IdlLiteral(idl_type, value)
    raise ValueError('Unrecognized default value type: %s' % idl_type)


################################################################################
# Operations
################################################################################

class IdlOperation(TypedObject):
    def __init__(self, node=None):
        self.arguments = []
        self.extended_attributes = {}
        self.specials = []
        self.is_constructor = False
        self.idl_type = None
        self.is_static = False
        # In what interface the attribute is (originally) defined when the
        # attribute is inherited from an ancestor interface.
        self.defined_in = None

        if not node:
            return

        self.name = node.GetName()

        self.is_static = bool(node.GetProperty('STATIC'))
        property_dictionary = node.GetProperties()
        for special_keyword in SPECIAL_KEYWORD_LIST:
            if special_keyword in property_dictionary:
                self.specials.append(special_keyword.lower())

        children = node.GetChildren()
        for child in children:
            child_class = child.GetClass()
            if child_class == 'Arguments':
                self.arguments = arguments_node_to_arguments(child)
            elif child_class == 'Type':
                self.idl_type = type_node_to_type(child)
            elif child_class == 'ExtAttributes':
                self.extended_attributes = ext_attributes_node_to_extended_attributes(child)
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)

        if 'Unforgeable' in self.extended_attributes and self.is_static:
            raise ValueError('[Unforgeable] cannot appear on static operations.')

    @classmethod
    def constructor_from_arguments_node(cls, name, arguments_node):
        constructor = cls()
        constructor.name = name
        constructor.arguments = arguments_node_to_arguments(arguments_node)
        constructor.is_constructor = True
        return constructor

    def accept(self, visitor):
        visitor.visit_operation(self)
        for argument in self.arguments:
            argument.accept(visitor)


################################################################################
# Arguments
################################################################################

class IdlArgument(TypedObject):
    def __init__(self, node=None):
        self.extended_attributes = {}
        self.idl_type = None
        self.is_optional = False  # syntax: (optional T)
        self.is_variadic = False  # syntax: (T...)
        self.default_value = None

        if not node:
            return

        self.is_optional = node.GetProperty('OPTIONAL')
        self.name = node.GetName()

        children = node.GetChildren()
        for child in children:
            child_class = child.GetClass()
            if child_class == 'Type':
                self.idl_type = type_node_to_type(child)
            elif child_class == 'ExtAttributes':
                self.extended_attributes = ext_attributes_node_to_extended_attributes(child)
            elif child_class == 'Argument':
                child_name = child.GetName()
                if child_name != '...':
                    raise ValueError('Unrecognized Argument node; expected "...", got "%s"' % child_name)
                self.is_variadic = bool(child.GetProperty('ELLIPSIS'))
            elif child_class == 'Default':
                self.default_value = default_node_to_idl_literal(child)
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)

    def accept(self, visitor):
        visitor.visit_argument(self)


def arguments_node_to_arguments(node):
    # [Constructor] and [CustomConstructor] without arguments (the bare form)
    # have None instead of an arguments node, but have the same meaning as using
    # an empty argument list, [Constructor()], so special-case this.
    # http://www.w3.org/TR/WebIDL/#Constructor
    if node is None:
        return []
    return [IdlArgument(argument_node) for argument_node in node.GetChildren()]


################################################################################
# Stringifiers
################################################################################

class IdlStringifier(object):
    def __init__(self, node):
        self.attribute = None
        self.operation = None
        self.extended_attributes = {}

        for child in node.GetChildren():
            child_class = child.GetClass()
            if child_class == 'Attribute':
                self.attribute = IdlAttribute(child)
            elif child_class == 'Operation':
                operation = IdlOperation(child)
                if operation.name:
                    self.operation = operation
            elif child_class == 'ExtAttributes':
                self.extended_attributes = ext_attributes_node_to_extended_attributes(child)
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)

        # Copy the stringifier's extended attributes (such as [Unforgable]) onto
        # the underlying attribute or operation, if there is one.
        if self.attribute or self.operation:
            (self.attribute or self.operation).extended_attributes.update(
                self.extended_attributes)


################################################################################
# Iterable, Maplike, Setlike
################################################################################

class IdlIterableOrMaplikeOrSetlike(TypedObject):
    def __init__(self, node):
        self.extended_attributes = {}
        self.type_children = []

        for child in node.GetChildren():
            child_class = child.GetClass()
            if child_class == 'ExtAttributes':
                self.extended_attributes = ext_attributes_node_to_extended_attributes(child)
            elif child_class == 'Type':
                self.type_children.append(child)
            else:
                raise ValueError('Unrecognized node class: %s' % child_class)


class IdlIterable(IdlIterableOrMaplikeOrSetlike):
    idl_type_attributes = ('key_type', 'value_type')

    def __init__(self, node):
        super(IdlIterable, self).__init__(node)

        if len(self.type_children) == 1:
            self.key_type = None
            self.value_type = type_node_to_type(self.type_children[0])
        elif len(self.type_children) == 2:
            self.key_type = type_node_to_type(self.type_children[0])
            self.value_type = type_node_to_type(self.type_children[1])
        else:
            raise ValueError('Unexpected number of type children: %d' % len(self.type_children))
        del self.type_children

    def accept(self, visitor):
        visitor.visit_iterable(self)


class IdlMaplike(IdlIterableOrMaplikeOrSetlike):
    idl_type_attributes = ('key_type', 'value_type')

    def __init__(self, node):
        super(IdlMaplike, self).__init__(node)

        self.is_read_only = bool(node.GetProperty('READONLY'))

        if len(self.type_children) == 2:
            self.key_type = type_node_to_type(self.type_children[0])
            self.value_type = type_node_to_type(self.type_children[1])
        else:
            raise ValueError('Unexpected number of children: %d' % len(self.type_children))
        del self.type_children

    def accept(self, visitor):
        visitor.visit_maplike(self)


class IdlSetlike(IdlIterableOrMaplikeOrSetlike):
    idl_type_attributes = ('value_type',)

    def __init__(self, node):
        super(IdlSetlike, self).__init__(node)

        self.is_read_only = bool(node.GetProperty('READONLY'))

        if len(self.type_children) == 1:
            self.value_type = type_node_to_type(self.type_children[0])
        else:
            raise ValueError('Unexpected number of children: %d' % len(self.type_children))
        del self.type_children

    def accept(self, visitor):
        visitor.visit_setlike(self)


################################################################################
# Includes statements
################################################################################

class IdlIncludes(object):
    def __init__(self, node):
        self.interface = node.GetName()
        self.mixin = node.GetProperty('REFERENCE')

    def accept(self, visitor):
        visitor.visit_include(self)


################################################################################
# Extended attributes
################################################################################

class Exposure:
    """An Exposure holds one Exposed or RuntimeEnabled condition.
    Each exposure has two properties: exposed and runtime_enabled.
    Exposure(e, r) corresponds to [Exposed(e r)]. Exposure(e) corresponds to
    [Exposed=e].
    """
    def __init__(self, exposed, runtime_enabled=None):
        self.exposed = exposed
        self.runtime_enabled = runtime_enabled


def ext_attributes_node_to_extended_attributes(node):
    """
    Returns:
      Dictionary of {ExtAttributeName: ExtAttributeValue}.
      Value is usually a string, with these exceptions:
      Constructors: value is a list of Arguments nodes, corresponding to
        possible signatures of the constructor.
      CustomConstructors: value is a list of Arguments nodes, corresponding to
        possible signatures of the custom constructor.
      NamedConstructor: value is a Call node, corresponding to the single
        signature of the named constructor.
    """
    # Primarily just make a dictionary from the children.
    # The only complexity is handling various types of constructors:
    # Constructors and Custom Constructors can have duplicate entries due to
    # overloading, and thus are stored in temporary lists.
    # However, Named Constructors cannot be overloaded, and thus do not have
    # a list.
    # FIXME: move Constructor logic into separate function, instead of modifying
    #        extended attributes in-place.
    constructors = []
    custom_constructors = []
    extended_attributes = {}

    def child_node(extended_attribute_node):
        children = extended_attribute_node.GetChildren()
        if not children:
            return None
        if len(children) > 1:
            raise ValueError('ExtAttributes node with %s children, expected at most 1' % len(children))
        return children[0]

    extended_attribute_node_list = node.GetChildren()
    for extended_attribute_node in extended_attribute_node_list:
        name = extended_attribute_node.GetName()
        child = child_node(extended_attribute_node)
        child_class = child and child.GetClass()
        if name == 'Constructor':
            if child_class and child_class != 'Arguments':
                raise ValueError('Constructor only supports Arguments as child, but has child of class: %s' % child_class)
            constructors.append(child)
        elif name == 'CustomConstructor':
            if child_class and child_class != 'Arguments':
                raise ValueError('[CustomConstructor] only supports Arguments as child, but has child of class: %s' % child_class)
            custom_constructors.append(child)
        elif name == 'NamedConstructor':
            if child_class and child_class != 'Call':
                raise ValueError('[NamedConstructor] only supports Call as child, but has child of class: %s' % child_class)
            extended_attributes[name] = child
        elif name == 'Exposed':
            if child_class and child_class != 'Arguments':
                raise ValueError('[Exposed] only supports Arguments as child, but has child of class: %s' % child_class)
            exposures = []
            if child_class == 'Arguments':
                exposures = [Exposure(exposed=str(arg.idl_type),
                                      runtime_enabled=arg.name)
                             for arg in arguments_node_to_arguments(child)]
            else:
                value = extended_attribute_node.GetProperty('VALUE')
                if type(value) is str:
                    exposures = [Exposure(exposed=value)]
                else:
                    exposures = [Exposure(exposed=v) for v in value]
            extended_attributes[name] = exposures
        elif child:
            raise ValueError('ExtAttributes node with unexpected children: %s' % name)
        else:
            value = extended_attribute_node.GetProperty('VALUE')
            extended_attributes[name] = value

    # Store constructors and custom constructors in special list attributes,
    # which are deleted later. Note plural in key.
    if constructors:
        extended_attributes['Constructors'] = constructors
    if custom_constructors:
        extended_attributes['CustomConstructors'] = custom_constructors

    return extended_attributes


def extended_attributes_to_constructors(extended_attributes):
    """Returns constructors and custom_constructors (lists of IdlOperations).

    Auxiliary function for IdlInterface.__init__.
    """

    constructor_list = extended_attributes.get('Constructors', [])
    constructors = [
        IdlOperation.constructor_from_arguments_node('Constructor', arguments_node)
        for arguments_node in constructor_list]

    custom_constructor_list = extended_attributes.get('CustomConstructors', [])
    custom_constructors = [
        IdlOperation.constructor_from_arguments_node('CustomConstructor', arguments_node)
        for arguments_node in custom_constructor_list]

    if 'NamedConstructor' in extended_attributes:
        # FIXME: support overloaded named constructors, and make homogeneous
        name = 'NamedConstructor'
        call_node = extended_attributes['NamedConstructor']
        extended_attributes['NamedConstructor'] = call_node.GetName()
        children = call_node.GetChildren()
        if len(children) != 1:
            raise ValueError('NamedConstructor node expects 1 child, got %s.' % len(children))
        arguments_node = children[0]
        named_constructor = IdlOperation.constructor_from_arguments_node('NamedConstructor', arguments_node)
        # FIXME: should return named_constructor separately; appended for Perl
        constructors.append(named_constructor)

    return constructors, custom_constructors


def clear_constructor_attributes(extended_attributes):
    # Deletes Constructor*s* (plural), sets Constructor (singular)
    if 'Constructors' in extended_attributes:
        del extended_attributes['Constructors']
        extended_attributes['Constructor'] = None
    if 'CustomConstructors' in extended_attributes:
        del extended_attributes['CustomConstructors']
        extended_attributes['CustomConstructor'] = None


################################################################################
# Types
################################################################################

def type_node_to_type(node):
    children = node.GetChildren()
    if len(children) != 1 and len(children) != 2:
        raise ValueError('Type node expects 1 or 2 child(ren), got %d.' % len(children))

    base_type = type_node_inner_to_type(children[0])
    if len(children) == 2:
        extended_attributes = ext_attributes_node_to_extended_attributes(children[1])
        base_type = IdlAnnotatedType(base_type, extended_attributes)

    if node.GetProperty('NULLABLE'):
        base_type = IdlNullableType(base_type)

    return base_type


def type_node_inner_to_type(node):
    node_class = node.GetClass()
    # Note Type*r*ef, not Typedef, meaning the type is an identifier, thus
    # either a typedef shorthand (but not a Typedef declaration itself) or an
    # interface type. We do not distinguish these, and just use the type name.
    if node_class in ['PrimitiveType', 'StringType', 'Typeref']:
        # unrestricted syntax: unrestricted double | unrestricted float
        is_unrestricted = bool(node.GetProperty('UNRESTRICTED'))
        return IdlType(node.GetName(), is_unrestricted=is_unrestricted)
    elif node_class == 'Any':
        return IdlType('any')
    elif node_class in ['Sequence', 'FrozenArray']:
        return sequence_node_to_type(node)
    elif node_class == 'UnionType':
        return union_type_node_to_idl_union_type(node)
    elif node_class == 'Promise':
        return IdlType('Promise')
    elif node_class == 'Record':
        return record_node_to_type(node)
    raise ValueError('Unrecognized node class: %s' % node_class)


def record_node_to_type(node):
    children = node.GetChildren()
    if len(children) != 2:
        raise ValueError('record<K,V> node expects exactly 2 children, got %d' % (len(children)))
    key_child = children[0]
    value_child = children[1]
    if key_child.GetClass() != 'StringType':
        raise ValueError('Keys in record<K,V> nodes must be string types.')
    if value_child.GetClass() != 'Type':
        raise ValueError('Unrecognized node class for record<K,V> value: %s' % value_child.GetClass())
    return IdlRecordType(IdlType(key_child.GetName()), type_node_to_type(value_child))


def sequence_node_to_type(node):
    children = node.GetChildren()
    class_name = node.GetClass()
    if len(children) != 1:
        raise ValueError('%s node expects exactly 1 child, got %s' % (class_name, len(children)))
    sequence_child = children[0]
    sequence_child_class = sequence_child.GetClass()
    if sequence_child_class != 'Type':
        raise ValueError('Unrecognized node class: %s' % sequence_child_class)
    element_type = type_node_to_type(sequence_child)
    if class_name == 'Sequence':
        sequence_type = IdlSequenceType(element_type)
    elif class_name == 'FrozenArray':
        sequence_type = IdlFrozenArrayType(element_type)
    else:
        raise ValueError('Unexpected node: %s' % class_name)
    if node.GetProperty('NULLABLE'):
        return IdlNullableType(sequence_type)
    return sequence_type


def typedef_node_to_type(node):
    children = node.GetChildren()
    if len(children) != 1:
        raise ValueError('Typedef node with %s children, expected 1' % len(children))
    child = children[0]
    child_class = child.GetClass()
    if child_class != 'Type':
        raise ValueError('Unrecognized node class: %s' % child_class)
    return type_node_to_type(child)


def union_type_node_to_idl_union_type(node):
    member_types = [type_node_to_type(member_type_node)
                    for member_type_node in node.GetChildren()]
    return IdlUnionType(member_types)


################################################################################
# Visitor
################################################################################

class Visitor(object):
    """Abstract visitor class for IDL definitions traverse."""

    def visit_definitions(self, definitions):
        pass

    def visit_typed_object(self, typed_object):
        pass

    def visit_callback_function(self, callback_function):
        self.visit_typed_object(callback_function)

    def visit_dictionary(self, dictionary):
        pass

    def visit_dictionary_member(self, member):
        self.visit_typed_object(member)

    def visit_enumeration(self, enumeration):
        pass

    def visit_include(self, include):
        pass

    def visit_interface(self, interface):
        pass

    def visit_typedef(self, typedef):
        self.visit_typed_object(typedef)

    def visit_attribute(self, attribute):
        self.visit_typed_object(attribute)

    def visit_constant(self, constant):
        self.visit_typed_object(constant)

    def visit_operation(self, operation):
        self.visit_typed_object(operation)

    def visit_argument(self, argument):
        self.visit_typed_object(argument)

    def visit_iterable(self, iterable):
        self.visit_typed_object(iterable)

    def visit_maplike(self, maplike):
        self.visit_typed_object(maplike)

    def visit_setlike(self, setlike):
        self.visit_typed_object(setlike)
