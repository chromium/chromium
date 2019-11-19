# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
import functools

from blinkbuild.name_style_converter import NameStyleConverter

from .composition_parts import WithDebugInfo
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithIdentifier
from .extended_attribute import ExtendedAttributes
from .reference import RefById
from .reference import RefByIdFactory
from .typedef import Typedef
from .user_defined_type import UserDefinedType

# The implementation class hierarchy of IdlType
#
# IdlType
# + SimpleType
# + ReferenceType
# + DefinitionType
# + TypedefType
# + _ArrayLikeType
# | + SequenceType
# | + FrozenArrayType
# | + VariadicType
# + RecordType
# + PromiseType
# + UnionType
# + NullableType

_IDL_TYPE_PASS_KEY = object()


class IdlTypeFactory(object):
    """
    Creates a group of instances of IdlType, over which you can iterate later.

    There are two phases; instantiation phase and iteration phase.  The factory
    is initially in the instantiation phase and you can create instances of
    IdlType.  Once it enters to the iteration phase (through the first attempt
    to iterate), you can no longer create a new instance.
    """

    def __init__(self):
        self._idl_types = []
        # Factory to initialize instances of ReferenceType.
        attrs_to_be_proxied = (
            set(RefById.get_all_attributes(IdlType)).difference(
                # attributes not to be proxied
                set(('debug_info', 'extended_attributes', 'is_optional'))))
        self._ref_by_id_factory = RefByIdFactory(
            target_attrs_with_priority=attrs_to_be_proxied)
        # |_is_frozen| is initially False and you can create new instances of
        # IdlType.  The first invocation of |for_each| freezes the factory and
        # you can no longer create a new instance of IdlType.
        self._is_frozen = False

    def for_each(self, callback):
        """
        Applies |callback| to all the instances of IdlType created by this
        factory.

        Instantiation of IdlType is no longer possible.

        Args:
            callback: A callable that takes an IdlType as only the argument.
                Return value is not used.
        """
        assert callable(callback)
        self._is_frozen = True
        for idl_type in self._idl_types:
            callback(idl_type)

    def for_each_reference(self, callback):
        """
        Applies |callback| to all the instances of IdlType that is referencing
        to another IdlType.

        Instantiation of referencing IdlType is no longer possible, but it's
        still possible to instantiate other IdlTypes.

        Args:
            callback: A callable that takes an IdlType as only the argument.
                Return value is not used.
        """
        self._ref_by_id_factory.for_each(callback)

    def simple_type(self, *args, **kwargs):
        return self._create(SimpleType, args, kwargs)

    def reference_type(self, *args, **kwargs):
        assert 'ref_by_id_factory' not in kwargs
        kwargs['ref_by_id_factory'] = self._ref_by_id_factory
        return self._create(ReferenceType, args, kwargs)

    def definition_type(self, *args, **kwargs):
        return self._create(DefinitionType, args, kwargs)

    def typedef_type(self, *args, **kwargs):
        return self._create(TypedefType, args, kwargs)

    def sequence_type(self, *args, **kwargs):
        return self._create(SequenceType, args, kwargs)

    def frozen_array_type(self, *args, **kwargs):
        return self._create(FrozenArrayType, args, kwargs)

    def variadic_type(self, *args, **kwargs):
        return self._create(VariadicType, args, kwargs)

    def record_type(self, *args, **kwargs):
        return self._create(RecordType, args, kwargs)

    def promise_type(self, *args, **kwargs):
        return self._create(PromiseType, args, kwargs)

    def union_type(self, *args, **kwargs):
        return self._create(UnionType, args, kwargs)

    def nullable_type(self, *args, **kwargs):
        return self._create(NullableType, args, kwargs)

    def _create(self, idl_type_concrete_class, args, kwargs):
        assert not self._is_frozen
        idl_type = idl_type_concrete_class(
            *args, pass_key=_IDL_TYPE_PASS_KEY, **kwargs)
        self._idl_types.append(idl_type)
        return idl_type


class IdlType(WithExtendedAttributes, WithDebugInfo):
    """
    Represents a 'type' in Web IDL.

    IdlType is an interface of types in Web IDL, and also provides all the
    information that is necessary for type conversions.  For example, given the
    conversion rules of ECMAScript bindings, you can produce a type converter
    between Blink types and V8 types with using an IdlType.

    Note that IdlType is designed to _not_ include knowledge about a certain
    language bindings (such as ECMAScript bindings), thus it's out of scope for
    IdlType to tell whether IDL dictionary type accepts ES null value or not.

    Nullable type and typedef are implemented as if they're a container type
    like record type and promise type.
    """

    def __init__(self,
                 is_optional=False,
                 extended_attributes=None,
                 debug_info=None,
                 pass_key=None):
        assert isinstance(is_optional, bool)
        assert pass_key is _IDL_TYPE_PASS_KEY
        WithExtendedAttributes.__init__(self, extended_attributes)
        WithDebugInfo.__init__(self, debug_info)
        self._is_optional = is_optional

    def __eq__(self, other):
        """Returns True if |self| and |other| represent the equivalent type."""
        return (self.__class__ == other.__class__
                and ExtendedAttributes.equals(self.extended_attributes,
                                              other.extended_attributes)
                and self.is_optional == other.is_optional)

    def __ne__(self, other):
        return not self == other

    def __hash__(self):
        raise exceptions.NotImplementedError()

    def make_copy(self, memo):
        return self

    @property
    def syntactic_form(self):
        """
        Returns a text representation of the type in the form of Web IDL syntax.
        """
        raise exceptions.NotImplementedError()

    @property
    def type_name(self):
        """
        Returns the type name.
        https://heycam.github.io/webidl/#dfn-type-name
        Note that a type name is not necessarily unique.
        """
        raise exceptions.NotImplementedError()

    @property
    def keyword_typename(self):
        """
        Returns the keyword name of the type if this is a simple built-in type,
        e.g. "any", "boolean", "unsigned long long", "void", etc.  Otherwise,
        returns None.
        """
        return None

    def apply_to_all_composing_elements(self, callback):
        """
        Applies |callback| to all instances of IdlType of which this IdlType
        consists, including |self|.

        In case of x.apply_to_all_composing_elements(callback), |callback| will
        be recursively called back on x, x.inner_type, x.element_type,
        x.result_type.original_type, etc. if any.
        """
        callback(self)

    def unwrap(self, nullable=None, typedef=None):
        """
        Returns the body part of the actual type, i.e. returns the interesting
        part of this type.

        Args:
            nullable:
            typedef:
                All these arguments take tri-state value: True, False, or None.
                True unwraps that type, False stops unwrapping that type.  All
                of specified arguments' values must be consistent, and mixture
                of True and False is not allowed.  Unspecified arguments are
                automatically set to the opposite value.  If no argument is
                specified, unwraps all types.
        """
        switches = {
            'nullable': nullable,
            'typedef': typedef,
        }

        value_counts = {None: 0, False: 0, True: 0}
        for value in switches.itervalues():
            assert value is None or isinstance(value, bool)
            value_counts[value] += 1
        assert value_counts[False] == 0 or value_counts[True] == 0, (
            "Specify only True or False arguments.  Unspecified arguments are "
            "automatically set to the opposite value.")
        default = value_counts[True] == 0
        for arg, value in switches.iteritems():
            if value is None:
                switches[arg] = default

        return self._unwrap(switches)

    @property
    def does_include_nullable_type(self):
        """
        Returns True if |self| includes a nulllable type.
        https://heycam.github.io/webidl/#dfn-includes-a-nullable-type
        @return bool
        """
        return False

    @property
    def is_numeric(self):
        """
        Returns True if this is an integer type or floating point number type.
        """
        return False

    @property
    def is_integer(self):
        """Returns True if this is an integer type."""
        return False

    @property
    def is_boolean(self):
        """Returns True if this is a boolean type."""
        return False

    @property
    def is_string(self):
        """Returns True if this is a DOMString, ByteString, or USVString."""
        return False

    @property
    def is_object(self):
        """
        Returns True if this is exactly type 'object'.

        Note that this method doesn't return True for an interface or dictionary
        type, or type 'any'.
        """
        return False

    @property
    def is_symbol(self):
        """Returns True if this is type 'symbol'."""
        return False

    @property
    def is_any(self):
        """Returns True if this is type 'any'."""
        return False

    @property
    def is_void(self):
        """Returns True if this is type 'void'."""
        return False

    @property
    def is_interface(self):
        """Returns True if this is an interface type."""
        return False

    @property
    def is_dictionary(self):
        """Returns True if this is a dictionary type."""
        return False

    @property
    def is_enumeration(self):
        """Returns True if this is an enumeration type."""
        return False

    @property
    def is_callback_interface(self):
        """Returns True if this is a callback interface type."""
        return False

    @property
    def is_callback_function(self):
        """Returns True if this is a callback function type."""
        return False

    @property
    def is_typedef(self):
        """
        Returns True if this is a typedef.

        Despite that 'typedef' in Web IDL is not a type, IdlType treats typedefs
        as type-wrapping-type just like nullable type and promise type.  You can
        access the typedef'ed type through |original_type|.
        """
        return False

    @property
    def is_sequence(self):
        """Returns True if this is a sequence type."""
        return False

    @property
    def is_frozen_array(self):
        """Returns True if this is a froen array type."""
        return False

    @property
    def is_record(self):
        """Returns True if this is a record type."""
        return False

    @property
    def is_promise(self):
        """Returns True if this is a promise type."""
        return False

    @property
    def is_union(self):
        """Returns True if this is a union type."""
        return False

    @property
    def is_nullable(self):
        """
        Returns True if this is a nullable type.

        NOTE: If |self| is a union type which includes a nullable type, this
        returns False, because |self| itself is not a nullable type.  Use
        |does_include_nullable_type| in such a case.
        """
        return False

    @property
    def is_annotated(self):
        """Returns True if this is annotated."""
        return bool(self.extended_attributes)

    @property
    def is_optional(self):
        """
        Returns True if this type is used for a non-required dictionary member
        or an optional argument.
        """
        return self._is_optional

    @property
    def is_variadic(self):
        """
        Returns True if this represents variadic arguments' type.

        Variadic argument type is represented as a type-wrapping-type like
        sequence type.  You can access the type of each argument through
        |element_type|.
        """
        return False

    @property
    def original_type(self):
        """Returns the typedef'ed type."""
        return None

    @property
    def element_type(self):
        """
        Returns the element type if |is_sequence|, |is_frozen_array|, or
        |is_variadic|.
        """
        return None

    @property
    def key_type(self):
        """Returns the key type if |is_record|."""
        return None

    @property
    def value_type(self):
        """Returns the value type if |is_record|."""
        return None

    @property
    def result_type(self):
        """Returns the result type if |is_promise|."""
        return None

    @property
    def member_types(self):
        """Returns member types if |is_union|."""
        return None

    @property
    def flattened_member_types(self):
        """
        Returns a set of flattened member types if |is_union|.
        https://heycam.github.io/webidl/#dfn-flattened-union-member-types

        Note that this is not simple flattening, and a nullable type will be
        unwrapped.  Annotated types are always unwrapped but you can access it
        through extended_attributes in IdlType.  Typedef is unwrapped.
        """
        return None

    @property
    def inner_type(self):
        """Returns the inner type of type IdlType if |is_nullable|."""
        return None

    @property
    def type_definition_object(self):
        """
        Returns an object that represents a spec-author-defined type or None.

        Note that a returned object is not an IdlType.  In case of interface,
        a returned object is an instance of Interface.
        """
        return None

    def _format_syntactic_form(self, syntactic_form_inner):
        """Helper function to implement |syntactic_form|."""
        optional_form = 'optional ' if self.is_optional else ''
        ext_attr_form = ('{} '.format(self.extended_attributes.syntactic_form)
                         if self.extended_attributes else '')
        return '{}{}{}'.format(optional_form, ext_attr_form,
                               syntactic_form_inner)

    def _format_type_name(self, type_name_inner):
        """Helper function to implement |type_name|."""
        return '{}{}'.format(type_name_inner, ''.join(
            sorted(self.extended_attributes.keys())))

    def _unwrap(self, switches):
        return self


class SimpleType(IdlType):
    """
    Represents built-in types that do not contain other types internally.
    e.g. primitive types, string types, and object types.
    https://heycam.github.io/webidl/#idl-types
    """

    _INTEGER_TYPES = ('byte', 'octet', 'short', 'unsigned short', 'long',
                      'unsigned long', 'long long', 'unsigned long long')
    _NUMERIC_TYPES = ('float', 'unrestricted float', 'double',
                      'unrestricted double') + _INTEGER_TYPES
    _STRING_TYPES = ('DOMString', 'ByteString', 'USVString')
    _VALID_TYPES = ('any', 'boolean', 'object', 'symbol',
                    'void') + _NUMERIC_TYPES + _STRING_TYPES

    def __init__(self,
                 name,
                 is_optional=False,
                 extended_attributes=None,
                 debug_info=None,
                 pass_key=None):
        assert name in SimpleType._VALID_TYPES, (
            'Unknown type name: {}'.format(name))
        IdlType.__init__(
            self,
            is_optional=is_optional,
            extended_attributes=extended_attributes,
            debug_info=debug_info,
            pass_key=pass_key)
        self._name = name

    def __eq__(self, other):
        return (IdlType.__eq__(self, other)
                and self.syntactic_form == other.syntactic_form)

    def __hash__(self):
        return hash(self._name)

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form(self._name)

    @property
    def type_name(self):
        name = 'String' if self._name == 'DOMString' else self._name
        return self._format_type_name(
            NameStyleConverter(name).to_upper_camel_case())

    @property
    def keyword_typename(self):
        return self._name

    @property
    def is_numeric(self):
        return self._name in SimpleType._NUMERIC_TYPES

    @property
    def is_integer(self):
        return self._name in SimpleType._INTEGER_TYPES

    @property
    def is_boolean(self):
        return self._name == 'boolean'

    @property
    def is_string(self):
        return self._name in SimpleType._STRING_TYPES

    @property
    def is_object(self):
        return self._name == 'object'

    @property
    def is_symbol(self):
        return self._name == 'symbol'

    @property
    def is_any(self):
        return self._name == 'any'

    @property
    def is_void(self):
        return self._name == 'void'


class ReferenceType(IdlType, RefById):
    """
    Represents a type specified with the given identifier.

    As the exact type definitions are unknown in early compilation phases, it
    will be resolved in very end of the compilation phases.  Once everything is
    resolved, a ReferenceType behaves as a proxy to the resolved type.

    'typedef' in Web IDL is not a type, but we have TypedefType.  The
    identifier may be resolved to a TypedefType.
    """

    def __init__(self,
                 identifier,
                 is_optional=False,
                 extended_attributes=None,
                 debug_info=None,
                 ref_by_id_factory=None,
                 pass_key=None):
        assert isinstance(ref_by_id_factory, RefByIdFactory)

        IdlType.__init__(
            self,
            is_optional=is_optional,
            extended_attributes=extended_attributes,
            debug_info=debug_info,
            pass_key=pass_key)
        ref_by_id_factory.init_subclass_instance(
            self, identifier=identifier, debug_info=debug_info)

    def __eq__(self, other):
        return (IdlType.__eq__(self, other)
                and self.identifier == other.identifier)

    def __hash__(self):
        return hash(self.identifier)


class DefinitionType(IdlType, WithIdentifier):
    """
    Represents a spec-author-defined type, e.g. interface type and dictionary
    type.

    Typedef and union type are not included.  They are represented as
    TypedefType and UnionType respectively.
    """

    def __init__(self,
                 user_defined_type,
                 debug_info=None,
                 pass_key=None):
        assert isinstance(user_defined_type, UserDefinedType)
        IdlType.__init__(
            self,
            debug_info=debug_info,
            pass_key=pass_key)
        WithIdentifier.__init__(self, user_defined_type.identifier)
        self._type_definition_object = user_defined_type

    def __eq__(self, other):
        return (IdlType.__eq__(self, other)
                and self.identifier == other.identifier)

    def __hash__(self):
        return hash(self.identifier)

    # IdlType overrides
    @property
    def syntactic_form(self):
        assert not self.extended_attributes
        assert not self.is_optional
        return self.identifier

    @property
    def type_name(self):
        assert not self.extended_attributes
        assert not self.is_optional
        return self.identifier

    @property
    def is_interface(self):
        return self.type_definition_object.is_interface

    @property
    def is_callback_interface(self):
        return self.type_definition_object.is_callback_interface

    @property
    def is_dictionary(self):
        return self.type_definition_object.is_dictionary

    @property
    def is_enumeration(self):
        return self.type_definition_object.is_enumeration

    @property
    def is_callback_function(self):
        return self.type_definition_object.is_callback_function

    @property
    def type_definition_object(self):
        return self._type_definition_object


class TypedefType(IdlType, WithIdentifier):
    """
    Represents a typedef definition as an IdlType.

    'typedef' in Web IDL is not a type, however, there are use cases that have
    interest in typedef names.  Thus, the IDL compiler does not resolve
    typedefs transparently (i.e. does not remove typedefs entirely), and
    IdlTypes representing typedefs remain and behave like NullableType.  You
    can track down the typedef'ed type to |original_type|.
    """

    def __init__(self,
                 typedef,
                 debug_info=None,
                 pass_key=None):
        assert isinstance(typedef, Typedef)
        IdlType.__init__(
            self,
            debug_info=debug_info,
            pass_key=pass_key)
        WithIdentifier.__init__(self, typedef.identifier)
        self._typedef = typedef

    def __eq__(self, other):
        return (IdlType.__eq__(self, other)
                and self.identifier == other.identifier)

    def __hash__(self):
        return hash(self.identifier)

    # IdlType overrides
    @property
    def syntactic_form(self):
        assert not self.extended_attributes
        assert not self.is_optional
        return self.identifier

    @property
    def type_name(self):
        assert not self.extended_attributes
        assert not self.is_optional
        return self.original_type.type_name

    def apply_to_all_composing_elements(self, callback):
        callback(self)
        self.original_type.apply_to_all_composing_elements(callback)

    @property
    def does_include_nullable_type(self):
        return self.original_type.does_include_nullable_type

    @property
    def is_typedef(self):
        return True

    @property
    def original_type(self):
        return self._typedef.idl_type

    def _unwrap(self, switches):
        if switches['typedef']:
            return self.original_type._unwrap(switches)
        return self


class _ArrayLikeType(IdlType):
    def __init__(self,
                 element_type,
                 is_optional=False,
                 extended_attributes=None,
                 debug_info=None,
                 pass_key=None):
        assert isinstance(element_type, IdlType)
        IdlType.__init__(
            self,
            is_optional=is_optional,
            extended_attributes=extended_attributes,
            debug_info=debug_info,
            pass_key=pass_key)
        self._element_type = element_type

    def __eq__(self, other):
        return (IdlType.__eq__(self, other)
                and self.element_type == other.element_type)

    def __hash__(self):
        return hash((self.__class__, self.element_type))

    # IdlType overrides
    def apply_to_all_composing_elements(self, callback):
        callback(self)
        self.element_type.apply_to_all_composing_elements(callback)

    @property
    def element_type(self):
        return self._element_type


class SequenceType(_ArrayLikeType):
    """https://heycam.github.io/webidl/#idl-sequence"""

    def __init__(self,
                 element_type,
                 is_optional=False,
                 extended_attributes=None,
                 debug_info=None,
                 pass_key=None):
        _ArrayLikeType.__init__(
            self,
            element_type,
            is_optional=is_optional,
            extended_attributes=extended_attributes,
            debug_info=debug_info,
            pass_key=pass_key)

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form('sequence<{}>'.format(
            self.element_type.syntactic_form))

    @property
    def type_name(self):
        return self._format_type_name('{}Sequence'.format(
            self.element_type.type_name))

    @property
    def is_sequence(self):
        return True


class FrozenArrayType(_ArrayLikeType):
    """https://heycam.github.io/webidl/#idl-frozen-array"""

    def __init__(self,
                 element_type,
                 is_optional=False,
                 extended_attributes=None,
                 debug_info=None,
                 pass_key=None):
        _ArrayLikeType.__init__(
            self,
            element_type,
            is_optional=is_optional,
            extended_attributes=extended_attributes,
            debug_info=debug_info,
            pass_key=pass_key)

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form('FrozenArray<{}>'.format(
            self.element_type.syntactic_form))

    @property
    def type_name(self):
        return self._format_type_name('{}Array'.format(
            self.element_type.type_name))

    @property
    def is_frozen_array(self):
        return True


class VariadicType(_ArrayLikeType):
    """Represents a type used for variadic arguments."""

    def __init__(self,
                 element_type,
                 debug_info=None,
                 pass_key=None):
        _ArrayLikeType.__init__(
            self,
            element_type,
            debug_info=debug_info,
            pass_key=pass_key)

    # IdlType overrides
    @property
    def syntactic_form(self):
        assert not self.extended_attributes
        assert not self.is_optional
        return '{}...'.format(self.element_type.syntactic_form)

    @property
    def type_name(self):
        # Blink-specific expansion of type name
        # The type name of a variadic type is the concatenation of the type
        # name of the element type and the string "Variadic".
        return '{}Variadic'.format(self.element_type.type_name)

    @property
    def is_variadic(self):
        return True


class RecordType(IdlType):
    """https://heycam.github.io/webidl/#idl-record"""

    def __init__(self,
                 key_type,
                 value_type,
                 is_optional=False,
                 extended_attributes=None,
                 debug_info=None,
                 pass_key=None):
        assert isinstance(key_type, IdlType)
        assert isinstance(value_type, IdlType)
        IdlType.__init__(
            self,
            is_optional=is_optional,
            extended_attributes=extended_attributes,
            debug_info=debug_info,
            pass_key=pass_key)
        self._key_type = key_type
        self._value_type = value_type

    def __eq__(self, other):
        return (IdlType.__eq__(self, other) and self.key_type == other.key_type
                and self.value_type == other.value_type)

    def __hash__(self):
        return hash((self.__class__, self.key_type, self.value_type))

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form('record<{}, {}>'.format(
            self.key_type.syntactic_form, self.value_type.syntactic_form))

    @property
    def type_name(self):
        return self._format_type_name('{}{}Record'.format(
            self.key_type.type_name, self.value_type.type_name))

    def apply_to_all_composing_elements(self, callback):
        callback(self)
        self.key_type.apply_to_all_composing_elements(callback)
        self.value_type.apply_to_all_composing_elements(callback)

    @property
    def is_record(self):
        return True

    @property
    def key_type(self):
        return self._key_type

    @property
    def value_type(self):
        return self._value_type


class PromiseType(IdlType):
    """https://heycam.github.io/webidl/#idl-promise"""

    def __init__(self,
                 result_type,
                 is_optional=False,
                 extended_attributes=None,
                 debug_info=None,
                 pass_key=None):
        assert isinstance(result_type, IdlType)
        IdlType.__init__(
            self,
            is_optional=is_optional,
            extended_attributes=extended_attributes,
            debug_info=debug_info,
            pass_key=pass_key)
        self._result_type = result_type

    def __eq__(self, other):
        return (IdlType.__eq__(self, other)
                and self.result_type == other.result_type)

    def __hash__(self):
        return hash((self.__class__, self.result_type))

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form('Promise<{}>'.format(
            self.result_type.syntactic_form))

    @property
    def type_name(self):
        return self._format_type_name('{}Promise'.format(
            self.result_type.type_name))

    def apply_to_all_composing_elements(self, callback):
        callback(self)
        self.result_type.apply_to_all_composing_elements(callback)

    @property
    def is_promise(self):
        return True

    @property
    def result_type(self):
        """
        Returns the result type.
        @return IdlType
        """
        return self._result_type


class UnionType(IdlType):
    """https://heycam.github.io/webidl/#idl-union"""

    def __init__(self,
                 member_types,
                 is_optional=False,
                 extended_attributes=None,
                 debug_info=None,
                 pass_key=None):
        assert isinstance(member_types, (list, tuple))
        assert all(isinstance(member, IdlType) for member in member_types)
        IdlType.__init__(
            self,
            is_optional=is_optional,
            extended_attributes=extended_attributes,
            debug_info=debug_info,
            pass_key=pass_key)
        self._member_types = tuple(member_types)

    def __eq__(self, other):
        """
        __eq__ is defined so that
            (A or B) == (B or A)
        but
            (A? or B) != (A or B?),
            (A or (B or C)) != ((A or B) or C), and
            (A or B) != (A or C) where C is typedef'ed to B.
        In short, the order of member types is not taken into account, but
        anything else is taken into account.  This is mostly consistent with
        that X != Y where Y is typedef'ed to X.
        """
        return (IdlType.__eq__(self, other)
                and set(self.member_types) == set(other.member_types))

    def __hash__(self):
        return hash((self.__class__,
                     functools.reduce(lambda x, idl_type: x + hash(idl_type),
                                      self.member_types, 0)))

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form('({})'.format(' or '.join(
            [member.syntactic_form for member in self.member_types])))

    @property
    def type_name(self):
        return self._format_type_name('Or'.join(
            [member.type_name for member in self.member_types]))

    def apply_to_all_composing_elements(self, callback):
        callback(self)
        for member_type in self.member_types:
            member_type.apply_to_all_composing_elements(callback)

    @property
    def does_include_nullable_type(self):
        return any(
            member.does_include_nullable_type for member in self.member_types)

    @property
    def is_union(self):
        return True

    @property
    def member_types(self):
        return self._member_types

    @property
    def flattened_member_types(self):
        def flatten(idl_type):
            if idl_type.is_union:
                return functools.reduce(
                    lambda x, idl_type: x + flatten(idl_type),
                    idl_type.member_types, [])
            elif idl_type.is_typedef:
                return flatten(idl_type.original_type)
            elif idl_type.is_nullable:
                return flatten(idl_type.inner_type)
            else:
                return [idl_type]

        return set(flatten(self))


class NullableType(IdlType):
    """https://heycam.github.io/webidl/#idl-nullable-type"""

    def __init__(self,
                 inner_type,
                 is_optional=False,
                 extended_attributes=None,
                 debug_info=None,
                 pass_key=None):
        assert isinstance(inner_type, IdlType)
        IdlType.__init__(
            self,
            is_optional=is_optional,
            extended_attributes=extended_attributes,
            debug_info=debug_info,
            pass_key=pass_key)
        self._inner_type = inner_type

    def __eq__(self, other):
        return (IdlType.__eq__(self, other)
                and self.inner_type == other.inner_type)

    def __hash__(self):
        return hash((self.__class__, self.inner_type))

    # IdlType overrides
    @property
    def syntactic_form(self):
        assert not self.extended_attributes
        return '{}?'.format(self.inner_type.syntactic_form)

    @property
    def type_name(self):
        assert not self.extended_attributes
        # https://heycam.github.io/webidl/#idl-annotated-types
        # Web IDL seems not supposing a case of [X] ([Y] Type)?, i.e. something
        # like [X] nullable<[Y] Type>, which should turn into "TypeYOrNullX".
        #
        # In case of '[Clamp] long?', it's interpreted as '([Clamp] long)?' but
        # the type name must be "LongOrNullClamp" instead of "LongClampOrNull".
        name = self.inner_type.type_name
        ext_attrs = ''.join(sorted(self.inner_type.extended_attributes.keys()))
        sep_index = len(name) - len(ext_attrs)
        return '{}OrNull{}'.format(name[0:sep_index], name[sep_index:])

    def apply_to_all_composing_elements(self, callback):
        callback(self)
        self.inner_type.apply_to_all_composing_elements(callback)

    @property
    def does_include_nullable_type(self):
        return True

    @property
    def is_nullable(self):
        return True

    @property
    def inner_type(self):
        return self._inner_type

    def _unwrap(self, switches):
        if switches['nullable']:
            return self.inner_type._unwrap(switches)
        return self
