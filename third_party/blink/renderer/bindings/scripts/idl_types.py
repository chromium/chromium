# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""IDL type handling.

Classes:
IdlTypeBase
 IdlType
 IdlUnionType
 IdlArrayOrSequenceType
  IdlSequenceType
  IdlFrozenArrayType
 IdlNullableType
 IdlAnnotatedType

IdlTypes are picklable because we store them in interfaces_info.
"""

from collections import defaultdict

################################################################################
# IDL types
################################################################################

INTEGER_TYPES = frozenset([
    # http://www.w3.org/TR/WebIDL/#dfn-integer-type
    'byte',
    'octet',
    'short',
    'unsigned short',
    # int and unsigned are not IDL types
    'long',
    'unsigned long',
    'long long',
    'unsigned long long',
])
NUMERIC_TYPES = (
    INTEGER_TYPES | frozenset([
        # http://www.w3.org/TR/WebIDL/#dfn-numeric-type
        'float',
        'unrestricted float',
        'double',
        'unrestricted double',
    ]))
# http://www.w3.org/TR/WebIDL/#dfn-primitive-type
PRIMITIVE_TYPES = (frozenset(['boolean']) | NUMERIC_TYPES)
BASIC_TYPES = (
    PRIMITIVE_TYPES | frozenset([
        # Built-in, non-composite, non-object data types
        # http://heycam.github.io/webidl/#idl-types
        'DOMString',
        'ByteString',
        'USVString',
        # http://heycam.github.io/webidl/#idl-types
        'void',
    ]))
TYPE_NAMES = {
    # http://heycam.github.io/webidl/#dfn-type-name
    'any': 'Any',
    'boolean': 'Boolean',
    'byte': 'Byte',
    'octet': 'Octet',
    'short': 'Short',
    'unsigned short': 'UnsignedShort',
    'long': 'Long',
    'unsigned long': 'UnsignedLong',
    'long long': 'LongLong',
    'unsigned long long': 'UnsignedLongLong',
    'float': 'Float',
    'unrestricted float': 'UnrestrictedFloat',
    'double': 'Double',
    'unrestricted double': 'UnrestrictedDouble',
    'DOMString': 'String',
    'ByteString': 'ByteString',
    'USVString': 'USVString',
    'object': 'Object',
}

STRING_TYPES = frozenset([
    # http://heycam.github.io/webidl/#es-interface-call (step 10.11)
    # (Interface object [[Call]] method's string types.)
    'String',
    'ByteString',
    'USVString',
])

EXTENDED_ATTRIBUTES_APPLICABLE_TO_TYPES = frozenset([
    'AllowShared',
    'Clamp',
    'EnforceRange',
    'StringContext',
    'TreatNullAs',
])

################################################################################
# Inheritance
################################################################################

ancestors = defaultdict(list)  # interface_name -> ancestors


def inherits_interface(interface_name, ancestor_name):
    return (interface_name == ancestor_name
            or ancestor_name in ancestors[interface_name])


def set_ancestors(new_ancestors):
    ancestors.update(new_ancestors)


class IdlTypeBase(object):
    """Base class for IdlType, IdlUnionType, IdlArrayOrSequenceType
    and IdlNullableType.
    """

    def __str__(self):
        raise NotImplementedError('__str__() should be defined in subclasses')

    def __getattr__(self, name):
        # Default undefined attributes to None (analogous to Jinja variables).
        # This allows us to not define default properties in the base class, and
        # allows us to relay __getattr__ in IdlNullableType to the inner type.
        return None

    def resolve_typedefs(self, typedefs):
        raise NotImplementedError(
            'resolve_typedefs should be defined in subclasses')

    def idl_types(self):
        """A generator which yields IdlTypes which are referenced from |self|,
        including itself."""
        yield self


################################################################################
# IdlType
################################################################################


class IdlType(IdlTypeBase):
    # FIXME: incorporate Nullable, etc.
    # to support types like short?[] vs. short[]?, instead of treating these
    # as orthogonal properties (via flags).
    callback_functions = {}
    callback_interfaces = set()
    dictionaries = set()
    enums = {}  # name -> values

    def __init__(self, base_type, is_unrestricted=False):
        super(IdlType, self).__init__()
        if is_unrestricted:
            self.base_type = 'unrestricted %s' % base_type
        else:
            self.base_type = base_type

    def __str__(self):
        return self.base_type

    def __getstate__(self):
        return {
            'base_type': self.base_type,
        }

    def __setstate__(self, state):
        self.base_type = state['base_type']

    @property
    def is_basic_type(self):
        return self.base_type in BASIC_TYPES

    @property
    def is_callback_function(self):  # pylint: disable=C0103
        return self.base_type in IdlType.callback_functions

    @property
    def is_custom_callback_function(self):
        entry = IdlType.callback_functions.get(self.base_type)
        callback_function = entry.get('callback_function')
        if not callback_function:
            return False
        return 'Custom' in callback_function.extended_attributes

    @property
    def is_callback_interface(self):
        return self.base_type in IdlType.callback_interfaces

    @property
    def is_dictionary(self):
        return self.base_type in IdlType.dictionaries

    @property
    def is_enum(self):
        # FIXME: add an IdlEnumType class and a resolve_enums step
        # at end of IdlDefinitions constructor
        return self.name in IdlType.enums

    @property
    def enum_values(self):
        return IdlType.enums.get(self.name)

    @property
    def enum_type(self):
        return self.name if self.is_enum else None

    @property
    def is_integer_type(self):
        return self.base_type in INTEGER_TYPES

    @property
    def is_void(self):
        return self.base_type == 'void'

    @property
    def is_numeric_type(self):
        return self.base_type in NUMERIC_TYPES

    @property
    def is_primitive_type(self):
        return self.base_type in PRIMITIVE_TYPES

    @property
    def is_interface_type(self):
        # Anything that is not another type is an interface type.
        # http://www.w3.org/TR/WebIDL/#idl-types
        # http://www.w3.org/TR/WebIDL/#idl-interface
        # In C++ these are RefPtr types.
        return not (self.is_basic_type or self.is_callback_function
                    or self.is_dictionary or self.is_enum or self.name == 'Any'
                    or self.name == 'Object' or self.name == 'Promise'
                    )  # Promise will be basic in future

    @property
    def is_string_type(self):
        return self.name in STRING_TYPES

    @property
    def name(self):
        """Return type name

        http://heycam.github.io/webidl/#dfn-type-name
        """
        base_type = self.base_type
        return TYPE_NAMES.get(base_type, base_type)

    @classmethod
    def set_callback_functions(cls, new_callback_functions):
        cls.callback_functions.update(new_callback_functions)

    @classmethod
    def set_callback_interfaces(cls, new_callback_interfaces):
        cls.callback_interfaces.update(new_callback_interfaces)

    @classmethod
    def set_dictionaries(cls, new_dictionaries):
        cls.dictionaries.update(new_dictionaries)

    @classmethod
    def set_enums(cls, new_enums):
        cls.enums.update(new_enums)

    def resolve_typedefs(self, typedefs):
        base_type = self.base_type
        if base_type in typedefs:
            resolved_type = typedefs[base_type]
            if resolved_type.base_type in typedefs:
                raise ValueError("We can't typedef a typedef'ed type.")
            # For the case that the resolved type contains other typedef'ed
            # type(s).
            return resolved_type.resolve_typedefs(typedefs)
        return self


################################################################################
# IdlUnionType
################################################################################


class IdlUnionType(IdlTypeBase):
    # http://heycam.github.io/webidl/#idl-union
    # IdlUnionType has __hash__() and __eq__() methods because they are stored
    # in sets.
    def __init__(self, member_types):
        super(IdlUnionType, self).__init__()
        self.member_types = member_types

    def __str__(self):
        return '(' + ' or '.join(
            str(member_type) for member_type in self.member_types) + ')'

    def __hash__(self):
        return hash(self.name)

    def __eq__(self, rhs):
        return self.name == rhs.name

    def __getstate__(self):
        return {
            'member_types': self.member_types,
        }

    def __setstate__(self, state):
        self.member_types = state['member_types']

    @property
    def flattened_member_types(self):
        """Returns the set of the union's flattened member types.

        https://heycam.github.io/webidl/#dfn-flattened-union-member-types
        """
        # We cannot use a set directly because each member is an
        # IdlTypeBase-derived class, and comparing two objects of the
        # same type is not the same as comparing their names.
        # In other words:
        #   x = IdlType('ByteString')
        #   y = IdlType('ByteString')
        #   x == y  # False
        #   x.name == y.name  # True
        # |flattened_members|'s keys are type names, the values are type
        # |objects|.
        # We assume we can use two IDL objects of the same type interchangeably.
        flattened_members = {}
        for member in self.member_types:
            if member.is_nullable:
                member = member.inner_type
            if member.is_union_type:
                for inner_member in member.flattened_member_types:
                    flattened_members[inner_member.name] = inner_member
            else:
                flattened_members[member.name] = member
        return set(flattened_members.values())

    @property
    def number_of_nullable_member_types(self):
        """Returns the union's number of nullable types.

        http://heycam.github.io/webidl/#dfn-number-of-nullable-member-types
        """
        count = 0
        for member in self.member_types:
            if member.is_nullable:
                count += 1
                member = member.inner_type
            if member.is_union_type:
                count += member.number_of_nullable_member_types
        return count

    @property
    def is_union_type(self):
        return True

    def single_matching_member_type(self, predicate):
        matching_types = list(filter(predicate, self.flattened_member_types))
        if len(matching_types) > 1:
            raise ValueError('%s is ambiguous.' % self.name)
        return matching_types[0] if matching_types else None

    @property
    def string_member_type(self):
        return self.single_matching_member_type(
            lambda member_type: (member_type.is_string_type or member_type.is_enum)
        )

    @property
    def numeric_member_type(self):
        return self.single_matching_member_type(
            lambda member_type: member_type.is_numeric_type)

    @property
    def boolean_member_type(self):
        return self.single_matching_member_type(
            lambda member_type: member_type.base_type == 'boolean')

    @property
    def sequence_member_type(self):
        return self.single_matching_member_type(
            lambda member_type: member_type.is_sequence_type)

    @property
    def dictionary_member_type(self):
        return self.single_matching_member_type(
            lambda member_type: member_type.is_dictionary)

    @property
    def as_union_type(self):
        # Note: Use this to "look through" a possible IdlNullableType wrapper.
        return self

    @property
    def name(self):
        """Return type name (or inner type name if nullable)

        http://heycam.github.io/webidl/#dfn-type-name
        """
        return 'Or'.join(member_type.name for member_type in self.member_types)

    def resolve_typedefs(self, typedefs):
        self.member_types = [
            member_type.resolve_typedefs(typedefs)
            for member_type in self.member_types
        ]
        return self

    def idl_types(self):
        yield self
        for member_type in self.member_types:
            for idl_type in member_type.idl_types():
                yield idl_type


################################################################################
# IdlArrayOrSequenceType, IdlSequenceType, IdlFrozenArrayType
################################################################################


# TODO(bashi): Rename this like "IdlArrayTypeBase" or something.
class IdlArrayOrSequenceType(IdlTypeBase):
    """Base class for array-like types."""

    def __init__(self, element_type):
        super(IdlArrayOrSequenceType, self).__init__()
        self.element_type = element_type

    def __getstate__(self):
        return {
            'element_type': self.element_type,
        }

    def __setstate__(self, state):
        self.element_type = state['element_type']

    def resolve_typedefs(self, typedefs):
        self.element_type = self.element_type.resolve_typedefs(typedefs)
        return self

    @property
    def is_array_or_sequence_type(self):
        return True

    @property
    def is_sequence_type(self):
        return False

    @property
    def is_frozen_array(self):
        return False

    @property
    def enum_values(self):
        return self.element_type.enum_values

    @property
    def enum_type(self):
        return self.element_type.enum_type

    def idl_types(self):
        yield self
        for idl_type in self.element_type.idl_types():
            yield idl_type


class IdlSequenceType(IdlArrayOrSequenceType):
    def __init__(self, element_type):
        super(IdlSequenceType, self).__init__(element_type)

    def __str__(self):
        return 'sequence<%s>' % self.element_type

    @property
    def name(self):
        return self.element_type.name + 'Sequence'

    @property
    def is_sequence_type(self):
        return True


class IdlFrozenArrayType(IdlArrayOrSequenceType):
    def __init__(self, element_type):
        super(IdlFrozenArrayType, self).__init__(element_type)

    def __str__(self):
        return 'FrozenArray<%s>' % self.element_type

    @property
    def name(self):
        return self.element_type.name + 'Array'

    @property
    def is_frozen_array(self):
        return True


################################################################################
# IdlRecordType
################################################################################


class IdlRecordType(IdlTypeBase):
    def __init__(self, key_type, value_type):
        super(IdlRecordType, self).__init__()
        self.key_type = key_type
        self.value_type = value_type

    def __str__(self):
        return 'record<%s, %s>' % (self.key_type, self.value_type)

    def __getstate__(self):
        return {
            'key_type': self.key_type,
            'value_type': self.value_type,
        }

    def __setstate__(self, state):
        self.key_type = state['key_type']
        self.value_type = state['value_type']

    def idl_types(self):
        yield self
        for idl_type in self.key_type.idl_types():
            yield idl_type
        for idl_type in self.value_type.idl_types():
            yield idl_type

    def resolve_typedefs(self, typedefs):
        self.key_type = self.key_type.resolve_typedefs(typedefs)
        self.value_type = self.value_type.resolve_typedefs(typedefs)
        return self

    @property
    def is_record_type(self):
        return True

    @property
    def name(self):
        return self.key_type.name + self.value_type.name + 'Record'


################################################################################
# IdlNullableType
################################################################################


# https://heycam.github.io/webidl/#idl-nullable-type
class IdlNullableType(IdlTypeBase):
    def __init__(self, inner_type):
        super(IdlNullableType, self).__init__()
        if inner_type.name == 'Any':
            raise ValueError('Inner type of nullable type must not be any.')
        if inner_type.name == 'Promise':
            raise ValueError(
                'Inner type of nullable type must not be a promise.')
        if inner_type.is_nullable:
            raise ValueError(
                'Inner type of nullable type must not be a nullable type.')
        if inner_type.is_union_type:
            if inner_type.number_of_nullable_member_types > 0:
                raise ValueError(
                    'Inner type of nullable type must not be a union type that '
                    'itself includes a nullable type.')
            if any(member.is_dictionary
                   for member in inner_type.flattened_member_types):
                raise ValueError(
                    'Inner type of nullable type must not be a union type that '
                    'has a dictionary type as its members.')

        self.inner_type = inner_type

    def __str__(self):
        # FIXME: Dictionary::ConversionContext::setConversionType can't
        # handle the '?' in nullable types (passes nullability separately).
        # Update that function to handle nullability from the type name,
        # simplifying its signature.
        # return str(self.inner_type) + '?'
        return str(self.inner_type)

    def __getattr__(self, name):
        return getattr(self.inner_type, name)

    def __getstate__(self):
        return {
            'inner_type': self.inner_type,
        }

    def __setstate__(self, state):
        self.inner_type = state['inner_type']

    @property
    def is_nullable(self):
        return True

    @property
    def name(self):
        return self.inner_type.name + 'OrNull'

    @property
    def enum_values(self):
        # Nullable enums are handled by preprending a None value to the list of
        # enum values. This None value is converted to nullptr on the C++ side,
        # which matches the JavaScript 'null' in the enum parsing code.
        inner_values = self.inner_type.enum_values
        if inner_values:
            return [None] + inner_values
        return None

    def resolve_typedefs(self, typedefs):
        self.inner_type = self.inner_type.resolve_typedefs(typedefs)
        return self

    def idl_types(self):
        yield self
        for idl_type in self.inner_type.idl_types():
            yield idl_type


################################################################################
# IdlAnnotatedType
################################################################################


class IdlAnnotatedType(IdlTypeBase):
    """IdlAnnoatedType represents an IDL type with extended attributes.
    [Clamp], [EnforceRange], [StringContext], and [TreatNullAs] are applicable
    to types.
    https://heycam.github.io/webidl/#idl-annotated-types
    """

    def __init__(self, inner_type, extended_attributes):
        super(IdlAnnotatedType, self).__init__()
        self.inner_type = inner_type
        self.extended_attributes = extended_attributes

        if any(key not in EXTENDED_ATTRIBUTES_APPLICABLE_TO_TYPES
               for key in extended_attributes):
            raise ValueError(
                'Extended attributes not applicable to types: %s' % self)

        if ('StringContext' in extended_attributes
                and inner_type.base_type not in ['DOMString', 'USVString']):
            raise ValueError(
                'StringContext is only applicable to string types.')

    def __str__(self):
        annotation = ', '.join(
            (key + ('' if val is None else '=' + val))
            for key, val in self.extended_attributes.items())
        return '[%s] %s' % (annotation, str(self.inner_type))

    def __getattr__(self, name):
        return getattr(self.inner_type, name)

    def __getstate__(self):
        return {
            'inner_type': self.inner_type,
            'extended_attributes': self.extended_attributes,
        }

    def __setstate__(self, state):
        self.inner_type = state['inner_type']
        self.extended_attributes = state['extended_attributes']

    @property
    def is_annotated_type(self):
        return True

    @property
    def has_string_context(self):
        return 'StringContext' in self.extended_attributes

    @property
    def name(self):
        annotation = ''.join(
            (key + ('' if val is None else val))
            for key, val in sorted(self.extended_attributes.items()))
        return self.inner_type.name + annotation

    def resolve_typedefs(self, typedefs):
        self.inner_type = self.inner_type.resolve_typedefs(typedefs)
        return self

    def idl_types(self):
        yield self
        yield self.inner_type
