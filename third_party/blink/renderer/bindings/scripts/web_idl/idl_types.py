# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .utilities import assert_no_extra_args


# Details of each type is described in
# https://heycam.github.io/webidl/#idl-types

class TypeBase(object):
    """
    TypeBase is a base class for all classes for IDL types.
    """

    @property
    def type_name(self):
        assert False, 'type_name() is not implemented'


class AnyType(TypeBase):

    @property
    def type_name(self):
        return 'Any'


class PrimitiveType(TypeBase):
    """
    PrimitiveType represents either of integer types, float types, or 'boolean'.
    * integer types: byte, octet, (unsigned) short, (unsigned) long, (unsigned) long long
    * float types: (unrestricted) float, (unrestricted) double
    @param string name             : the name of a primitive type
    @param bool   is_nullable      : True if the type is nullable (optional)
    @param bool   is_clamp         : True if the type has [Clamp] annotation (optional)
    @param bool   is_enforce_range : True if the type has [EnforceRange] annotation (optional)
    """

    _INTEGER_TYPES = frozenset([
        'byte', 'octet', 'short', 'unsigned short', 'long', 'unsigned long', 'long long', 'unsigned long long'
    ])
    _FLOAT_TYPES = frozenset([
        'float', 'unrestricted float', 'double', 'unrestricted double'
    ])
    _PRIMITIVE_TYPES = _INTEGER_TYPES | _FLOAT_TYPES | frozenset(['boolean'])

    def __init__(self, **kwargs):
        self._name = kwargs.pop('name')
        self._is_nullable = kwargs.pop('is_nullable', False)
        self._is_clamp = kwargs.pop('is_clamp', False)
        self._is_enforce_range = kwargs.pop('is_enforce_range', False)
        assert_no_extra_args(kwargs)

        if self._name not in PrimitiveType._PRIMITIVE_TYPES:
            raise ValueError('Unknown type name: %s' % self._name)
        if self.is_clamp and self.is_enforce_range:
            raise ValueError('[Clamp] and [EnforceRange] cannot be associated together')
        if (self.is_clamp or self.is_enforce_range) and not self.is_integer_type:
            raise ValueError('[Clamp] or [EnforceRange] cannot be associated with %s' % self._name)

    @property
    def type_name(self):
        return ''.join([word.capitalize() for word in self._name.split(' ')])

    @property
    def is_nullable(self):
        return self._is_nullable

    @property
    def is_clamp(self):
        return self._is_clamp

    @property
    def is_enforce_range(self):
        return self._is_enforce_range

    @property
    def is_integer_type(self):
        return self._name in PrimitiveType._INTEGER_TYPES

    @property
    def is_float_type(self):
        return self._name in PrimitiveType._FLOAT_TYPES

    @property
    def is_numeric_type(self):
        return self.is_integer_type or self.is_float_type


class StringType(TypeBase):
    """
    StringType represents a string type.
    @param StringType.Type        string_type   : a type of string
    @param bool                   is_nullable   : True if the string is nullable (optional)
    @param StringType.TreatNullAs treat_null_as : argument of an extended attribute [TreatNullAs] (optional)
    """
    STRING_TYPES = ('DOMString', 'ByteString', 'USVString')
    TREAT_NULL_AS = ('EmptyString',)

    def __init__(self, **kwargs):
        self._string_type = kwargs.pop('string_type')
        self._is_nullable = kwargs.pop('is_nullable', False)
        self._treat_null_as = kwargs.pop('treat_null_as', None)
        assert_no_extra_args(kwargs)

        if self._string_type not in StringType.STRING_TYPES:
            raise ValueError('Unknown string type: %s' % self._string_type)
        if self.treat_null_as and self.treat_null_as not in StringType.TREAT_NULL_AS:
            raise ValueError('Unknown TreatAsNull parameter: %s' % self.treat_null_as)

    @property
    def type_name(self):
        if self._string_type == 'DOMString':
            return 'String'
        return self._string_type

    @property
    def is_nullable(self):
        return self._is_nullable

    @property
    def treat_null_as(self):
        return self._treat_null_as


class ObjectType(TypeBase):
    """
    ObjectType represents 'object' type in Web IDL spec.
    @param bool is_nullable : True if the type is nullable (optional)
    """

    def __init__(self, **kwargs):
        self._is_nullable = kwargs.pop('is_nullable', False)
        assert_no_extra_args(kwargs)

    @property
    def type_name(self):
        return 'Object'

    @property
    def is_nullable(self):
        return self._is_nullable


class SequenceType(TypeBase):
    """
    SequenceType represents a sequence type 'sequence<T>' in Web IDL spec.
    @param TypeBase element_type : Type of element T
    @param bool     is_nullable  : True if the type is nullable (optional)
    """

    def __init__(self, **kwargs):
        self._element_type = kwargs.pop('element_type')
        self._is_nullable = kwargs.pop('is_nullable', False)
        assert_no_extra_args(kwargs)

        if not isinstance(self.element_type, TypeBase):
            raise ValueError('element_type must be an instance of TypeBase inheritances')

    @property
    def type_name(self):
        return self.element_type.type_name + 'Sequence'

    @property
    def is_nullable(self):
        return self._is_nullable

    @property
    def element_type(self):
        return self._element_type


class RecordType(TypeBase):
    """
    RecordType represents a record type 'record<K, V>' in Web IDL spec.
    @param StringType key_type    : Type of key K
    @param TypeBase   value_type  : Type of value V
    @param bool       is_nullable : True if the record type is nullable (optional)
    """

    def __init__(self, **kwargs):
        self._key_type = kwargs.pop('key_type')
        self._value_type = kwargs.pop('value_type')
        self._is_nullable = kwargs.pop('is_nullable', False)
        assert_no_extra_args(kwargs)

        if type(self.key_type) != StringType:
            raise ValueError('key_type parameter must be an instance of StringType.')
        if not isinstance(self.value_type, TypeBase):
            raise ValueError('value_type parameter must be an instance of TypeBase inheritances.')

    @property
    def type_name(self):
        return self.key_type.type_name + self.value_type.type_name + 'Record'

    @property
    def key_type(self):
        return self._key_type

    @property
    def value_type(self):
        return self._value_type

    @property
    def is_nullable(self):
        return self._is_nullable


class PromiseType(TypeBase):
    """
    PromiseType represents a promise type 'promise<T>' in Web IDL spec.
    @param TypeBase result_type : Type of the promise's result V
    """

    def __init__(self, **kwargs):
        self._result_type = kwargs.pop('result_type')
        assert_no_extra_args(kwargs)

    @property
    def type_name(self):
        return self.result_type.type_name + 'Promise'

    @property
    def result_type(self):
        return self._result_type


class UnionType(TypeBase):
    """
    UnionType represents a union type in Web IDL spec.
    @param [TypeBase] member_types : List of member types
    @param bool       is_nullable  : True if the type is nullable (optional)
    """

    def __init__(self, **kwargs):
        def count_nullable_member_types():
            number = 0
            for member in self.member_types:
                if type(member) == UnionType:
                    number = number + member.number_of_nullable_member_types
                elif type(member) not in (AnyType, PromiseType) and member.is_nullable:
                    number = number + 1
            return number

        self._member_types = tuple(kwargs.pop('member_types'))
        self._is_nullable = kwargs.pop('is_nullable', False)
        self._number_of_nullable_member_types = count_nullable_member_types()  # pylint: disable=invalid-name
        assert_no_extra_args(kwargs)

        if len(self.member_types) < 2:
            raise ValueError('Union type must have 2 or more member types, but got %d.' % len(self.member_types))
        if any(type(member) == AnyType for member in self.member_types):
            raise ValueError('any type must not be used as a union member type.')
        if self.number_of_nullable_member_types > 1:
            raise ValueError('The number of nullable member types of a union type must be 0 or 1, but %d' %
                             self.number_of_nullable_member_types)

    @property
    def type_name(self):
        return 'Or'.join(member.type_name for member in self.member_types)

    @property
    def member_types(self):
        return self._member_types

    @property
    def is_nullable(self):
        return self._is_nullable

    @property
    def number_of_nullable_member_types(self):
        return self._number_of_nullable_member_types

    @property
    def flattened_member_types(self):
        flattened = set()
        # TODO(peria): In spec, we have to remove type annotations and nullable flags.
        for member in self.member_types:
            if type(member) != UnionType:
                flattened.add(member)
            else:
                flattened.update(member.flattened_member_types)
        return flattened


class FrozenArrayType(TypeBase):
    """
    FrozenArrayType represents a frozen array type 'FrozenArray<T>' in Web IDL.
    @param TypeBase element_type : Type of element T
    @param bool     is_nullable  : True if the type is nullable (optional)
    """

    def __init__(self, **kwargs):
        self._element_type = kwargs.pop('element_type')
        self._is_nullable = kwargs.pop('is_nullable', False)
        assert_no_extra_args(kwargs)

    @property
    def type_name(self):
        return self.element_type.type_name + 'Array'

    @property
    def element_type(self):
        return self._element_type

    @property
    def is_nullable(self):
        return self._is_nullable


class VoidType(TypeBase):

    @property
    def type_name(self):
        return 'Void'


class TypePlaceHolder(TypeBase):
    """
    TypePlaceHolder is a pseudo type as a place holder of types which use identifers;
    interface types, dictionary types, enumeration types, and callback function types.
    Because it is not guaranteed that we have a definition of target defintions when
    we meet the type used, we use this class as a place holder.
    All place holders will be replaced with references after all the defintions in
    all components are collected.
    @param string identifier  : the identifier of a named definition to refer
    @param bool   is_nullable : True if the type is nullable (optional)
    """

    def __init__(self, **kwargs):
        self._identifier = kwargs.pop('identifier')
        self._is_nullable = kwargs.pop('is_nullable', False)
        assert_no_extra_args(kwargs)

    @property
    def type_name(self):
        return self._identifier

    @property
    def is_nullable(self):
        return self._is_nullable
