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
"""Functions for type handling and type conversion (Blink/C++ <-> V8/JS).

Extends IdlType and IdlUnionType with V8-specific properties, methods, and
class methods.

Spec:
http://www.w3.org/TR/WebIDL/#es-type-mapping

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import posixpath

from idl_types import IdlAnnotatedType
from idl_types import IdlArrayOrSequenceType
from idl_types import IdlNullableType
from idl_types import IdlRecordType
from idl_types import IdlType
from idl_types import IdlTypeBase
from idl_types import IdlUnionType
from utilities import to_snake_case
import v8_attributes  # for IdlType.constructor_type_name
from v8_globals import includes
from v8_utilities import binding_header_filename, extended_attribute_value_contains

################################################################################
# V8-specific handling of IDL types
################################################################################

NON_WRAPPER_TYPES = frozenset([
    'EventHandler',
    'NodeFilter',
    'OnBeforeUnloadEventHandler',
    'OnErrorEventHandler',
])
TYPED_ARRAY_TYPES = frozenset([
    'Float32Array',
    'Float64Array',
    'Int8Array',
    'Int16Array',
    'Int32Array',
    'Uint8Array',
    'Uint8ClampedArray',
    'Uint16Array',
    'Uint32Array',
    'BigInt64Array',
    'BigUint64Array',
])
ARRAY_BUFFER_VIEW_AND_TYPED_ARRAY_TYPES = TYPED_ARRAY_TYPES.union(
    frozenset(['ArrayBufferView']))
ARRAY_BUFFER_AND_VIEW_TYPES = TYPED_ARRAY_TYPES.union(
    frozenset([
        'ArrayBuffer',
        'ArrayBufferView',
        'DataView',
        'SharedArrayBuffer',
    ]))
# We have an unfortunate hack that treats types whose name ends with
# 'Constructor' as aliases to IDL interface object. This list is used to disable
# the hack.
_CALLBACK_CONSTRUCTORS = frozenset((
    'AnimatorConstructor',
    'BlinkAudioWorkletProcessorConstructor',
    'CustomElementConstructor',
    'NoArgumentConstructor',
))

IdlType.is_array_buffer_or_view = property(
    lambda self: self.base_type in ARRAY_BUFFER_AND_VIEW_TYPES)

IdlType.is_array_buffer_view_or_typed_array = property(
    lambda self: self.base_type in ARRAY_BUFFER_VIEW_AND_TYPED_ARRAY_TYPES)

IdlType.is_typed_array = property(
    lambda self: self.base_type in TYPED_ARRAY_TYPES)

IdlType.is_wrapper_type = property(
    lambda self: (self.is_interface_type and not self.is_callback_interface and self.base_type not in NON_WRAPPER_TYPES)
)

################################################################################
# C++ types
################################################################################

CPP_TYPE_SAME_AS_IDL_TYPE = set([
    'double',
    'float',
])
CPP_INTEGER_CONVERSION_RULES = {
    'byte': 'int8_t',
    'octet': 'uint8_t',
    'short': 'int16_t',
    'unsigned short': 'uint16_t',
    'long': 'int32_t',
    'unsigned long': 'uint32_t',
    'long long': 'int64_t',
    'unsigned long long': 'uint64_t',
}
CPP_SPECIAL_CONVERSION_RULES = {
    'EventHandler': 'EventListener*',
    'OnBeforeUnloadEventHandler': 'EventListener*',
    'OnErrorEventHandler': 'EventListener*',
    'Promise': 'ScriptPromise',
    'ScriptValue': 'ScriptValue',
    # FIXME: Eliminate custom bindings for XPathNSResolver  http://crbug.com/345529
    'XPathNSResolver': 'XPathNSResolver*',
    'boolean': 'bool',
    'object': 'ScriptValue',
    'unrestricted double': 'double',
    'unrestricted float': 'float',
}


def string_resource_mode(idl_type):
    """Returns a V8StringResourceMode value corresponding to the IDL type.

    Args:
        idl_type:
            A string IdlType.
    """
    if idl_type.is_nullable:
        return 'kTreatNullAndUndefinedAsNullString'
    if idl_type.is_annotated_type:
        treat_null_as = idl_type.extended_attributes.get('TreatNullAs')
        if treat_null_as == 'EmptyString':
            return 'kTreatNullAsEmptyString'
        elif treat_null_as:
            raise ValueError(
                'Unknown value for [TreatNullAs]: %s' % treat_null_as)
    return ''


def cpp_type(idl_type,
             extended_attributes=None,
             raw_type=False,
             used_as_rvalue_type=False,
             used_as_variadic_argument=False,
             used_in_cpp_sequence=False):
    """Returns C++ type corresponding to IDL type.

    |idl_type| argument is of type IdlType, while return value is a string

    Args:
        idl_type:
            IdlType
        raw_type:
            bool, True if idl_type's raw/primitive C++ type should be returned.
        used_as_rvalue_type:
            bool, True if the C++ type is used as an argument or the return
            type of a method.
        used_as_variadic_argument:
            bool, True if the C++ type is used as a variadic argument of a method.
        used_in_cpp_sequence:
            bool, True if the C++ type is used as an element of a container.
            Containers can be an array, a sequence, a dictionary or a record.
    """

    extended_attributes = extended_attributes or {}
    idl_type = idl_type.preprocessed_type

    # Nullable types
    def needs_optional_wrapper():
        if not idl_type.is_nullable or not used_in_cpp_sequence:
            return False
        # NativeValueTraits<T>::NullValue should exist in order to provide the
        # implicit null value, if needed.
        return not idl_type.inner_type.cpp_type_has_null_value

    if needs_optional_wrapper():
        inner_type = idl_type.inner_type
        if inner_type.is_dictionary or inner_type.is_sequence or inner_type.is_record_type:
            # TODO(jbroman, bashi): Implement this if needed.
            # This is non-trivial to support because HeapVector refuses to hold
            # base::Optional<>, and IDLDictionaryBase (and subclasses) have no
            # integrated null state that can be distinguished from a present but
            # empty dictionary. It's unclear whether this will ever come up in
            # real spec WebIDL.
            raise NotImplementedError(
                'Sequences of nullable dictionary, sequence or record types are not yet supported.'
            )
        return 'base::Optional<%s>' % inner_type.cpp_type_args(
            extended_attributes, raw_type, used_as_rvalue_type,
            used_as_variadic_argument, used_in_cpp_sequence)

    # Array or sequence types
    if used_as_variadic_argument:
        native_array_element_type = idl_type
    else:
        native_array_element_type = idl_type.native_array_element_type
    if native_array_element_type:
        vector_type = cpp_ptr_type('Vector', 'HeapVector',
                                   native_array_element_type.is_traceable)
        vector_template_type = cpp_template_type(
            vector_type,
            native_array_element_type.cpp_type_args(used_in_cpp_sequence=True))
        if used_as_rvalue_type:
            return 'const %s&' % vector_template_type
        return vector_template_type

    # Record types.
    if idl_type.is_record_type:
        vector_type = cpp_ptr_type('Vector', 'HeapVector',
                                   idl_type.value_type.is_traceable)
        value_type = idl_type.value_type.cpp_type_args(
            used_in_cpp_sequence=True)
        vector_template_type = cpp_template_type(
            vector_type, 'std::pair<String, %s>' % value_type)
        if used_as_rvalue_type:
            return 'const %s&' % vector_template_type
        return vector_template_type

    # Simple types
    base_idl_type = idl_type.base_type

    if base_idl_type in CPP_TYPE_SAME_AS_IDL_TYPE:
        return base_idl_type
    if base_idl_type in CPP_INTEGER_CONVERSION_RULES:
        return CPP_INTEGER_CONVERSION_RULES[base_idl_type]
    if base_idl_type in CPP_SPECIAL_CONVERSION_RULES:
        return CPP_SPECIAL_CONVERSION_RULES[base_idl_type]

    if idl_type.is_string_type:
        if idl_type.has_string_context:
            return 'String'
        if not raw_type:
            return 'const String&' if used_as_rvalue_type else 'String'
        return 'V8StringResource<%s>' % string_resource_mode(idl_type)

    if base_idl_type == 'ArrayBufferView' and 'FlexibleArrayBufferView' in extended_attributes:
        return 'FlexibleArrayBufferView'
    if base_idl_type in TYPED_ARRAY_TYPES and 'FlexibleArrayBufferView' in extended_attributes:
        return 'Flexible' + base_idl_type
    if base_idl_type in ARRAY_BUFFER_VIEW_AND_TYPED_ARRAY_TYPES or base_idl_type == 'DataView':
        if 'AllowShared' in extended_attributes:
            return cpp_template_type('MaybeShared', idl_type.implemented_as)
        else:
            return cpp_template_type('NotShared', idl_type.implemented_as)
    if idl_type.is_interface_type or idl_type.is_dictionary:
        implemented_as_class = idl_type.implemented_as
        if raw_type or not used_in_cpp_sequence:
            return implemented_as_class + '*'
        if not used_in_cpp_sequence:
            return implemented_as_class + '*'
        if used_as_rvalue_type and idl_type.is_garbage_collected:
            return 'const %s*' % implemented_as_class
        return cpp_template_type('Member', implemented_as_class)
    if idl_type.is_union_type:
        # Avoid "AOrNullOrB" for cpp type of (A? or B) because we generate
        # V8AOrBOrNull to handle nulle for (A? or B), (A or B?) and (A or B)?
        def member_cpp_name(idl_type):
            if idl_type.is_nullable:
                return idl_type.inner_type.name
            return idl_type.name

        idl_type_name = 'Or'.join(
            member_cpp_name(member) for member in idl_type.member_types)
        return 'const %s&' % idl_type_name if used_as_rvalue_type else idl_type_name
    if idl_type.is_callback_function:
        v8_type_name = 'V8' + base_idl_type
        if idl_type.is_custom_callback_function:
            return v8_type_name
        if not used_in_cpp_sequence:
            return v8_type_name + '*'
        return cpp_template_type('Member', v8_type_name)

    if base_idl_type == 'void':
        return base_idl_type
    # Default, assume native type is a pointer with same type name as idl type
    return base_idl_type + '*'


def cpp_type_initializer(idl_type):
    """Returns a string containing a C++ initialization statement for the
    corresponding type.

    |idl_type| argument is of type IdlType.
    """

    base_idl_type = idl_type.base_type

    if idl_type.native_array_element_type:
        return ''
    if idl_type.is_explicit_nullable:
        return ''
    if idl_type.is_numeric_type:
        return ' = 0'
    if base_idl_type == 'boolean':
        return ' = false'
    if (base_idl_type in NON_WRAPPER_TYPES
            or base_idl_type in CPP_SPECIAL_CONVERSION_RULES
            or base_idl_type == 'any' or idl_type.is_string_type
            or idl_type.is_enum):
        return ''
    return ' = nullptr'


# Allow access as idl_type.cpp_type if no arguments
IdlTypeBase.cpp_type = property(cpp_type)
IdlTypeBase.cpp_type_initializer = property(cpp_type_initializer)
IdlTypeBase.cpp_type_args = cpp_type
IdlUnionType.cpp_type_initializer = ''

IdlArrayOrSequenceType.native_array_element_type = property(
    lambda self: self.element_type)


def cpp_template_type(template, inner_type):
    """Returns C++ template specialized to type."""
    format_string = '{template}<{inner_type}>'
    return format_string.format(template=template, inner_type=inner_type)


def cpp_ptr_type(old_type, new_type, is_gc_type):
    if is_gc_type:
        return new_type
    return old_type


def v8_type(interface_name):
    return 'V8' + interface_name


# [ImplementedAs]
# This handles [ImplementedAs] on interface types, not [ImplementedAs] in the
# interface being generated. e.g., given:
#   Foo.idl: interface Foo {attribute Bar bar};
#   Bar.idl: [ImplementedAs=Zork] interface Bar {};
# when generating bindings for Foo, the [ImplementedAs] on Bar is needed.
# This data is external to Foo.idl, and hence computed as global information in
# compute_interfaces_info.py to avoid having to parse IDLs of all used interfaces.
IdlType.implemented_as_interfaces = {}


def implemented_as(idl_type):
    base_idl_type = idl_type.base_type
    if base_idl_type in IdlType.implemented_as_interfaces:
        return IdlType.implemented_as_interfaces[base_idl_type]
    elif idl_type.is_callback_function or idl_type.is_callback_interface:
        return 'V8%s' % base_idl_type
    return base_idl_type


IdlType.implemented_as = property(implemented_as)

IdlType.set_implemented_as_interfaces = classmethod(
    lambda cls, new_implemented_as_interfaces: \
        cls.implemented_as_interfaces.update(new_implemented_as_interfaces)
)

# [GarbageCollected]
IdlType.garbage_collected_types = set()

IdlType.is_garbage_collected = property(
    lambda self: self.base_type in IdlType.garbage_collected_types)

IdlType.set_garbage_collected_types = classmethod(
    lambda cls, new_garbage_collected_types: \
        cls.garbage_collected_types.update(new_garbage_collected_types)
)


def is_gc_type(idl_type):
    return idl_type.is_garbage_collected or idl_type.is_union_type


IdlTypeBase.is_gc_type = property(is_gc_type)


def is_traceable(idl_type):
    return (idl_type.is_garbage_collected or idl_type.is_callback_function
            or idl_type.cpp_type in ('ScriptValue', 'ScriptPromise'))


IdlTypeBase.is_traceable = property(is_traceable)
IdlUnionType.is_traceable = property(lambda self: True)
IdlArrayOrSequenceType.is_traceable = property(
    lambda self: self.element_type.is_traceable)
IdlRecordType.is_traceable = property(
    lambda self: self.value_type.is_traceable)
IdlNullableType.is_traceable = property(
    lambda self: self.inner_type.is_traceable)

################################################################################
# Includes
################################################################################

INCLUDES_FOR_TYPE = {
    'object':
    set(['bindings/core/v8/script_value.h']),
    'ArrayBufferView':
    set([
        'bindings/core/v8/v8_array_buffer_view.h',
        'core/typed_arrays/array_buffer_view_helpers.h',
        'core/typed_arrays/flexible_array_buffer_view.h'
    ]),
    'EventHandler':
    set(['bindings/core/v8/js_event_handler.h']),
    'HTMLCollection':
    set([
        'bindings/core/v8/v8_html_collection.h', 'core/dom/class_collection.h',
        'core/dom/tag_collection.h', 'core/html/html_collection.h',
        'core/html/html_table_rows_collection.h',
        'core/html/forms/html_data_list_options_collection.h',
        'core/html/forms/html_form_controls_collection.h'
    ]),
    'NodeList':
    set([
        'bindings/core/v8/v8_node_list.h', 'core/dom/name_node_list.h',
        'core/dom/node_list.h', 'core/dom/static_node_list.h',
        'core/html/forms/labels_node_list.h'
    ]),
    'Promise':
    set(['bindings/core/v8/script_promise.h']),
    'ScriptValue':
    set(['bindings/core/v8/script_value.h']),
}


def includes_for_type(idl_type, extended_attributes=None):
    idl_type = idl_type.preprocessed_type
    extended_attributes = extended_attributes or {}

    # Simple types
    base_idl_type = idl_type.base_type
    if base_idl_type in INCLUDES_FOR_TYPE:
        return INCLUDES_FOR_TYPE[base_idl_type]
    if base_idl_type in TYPED_ARRAY_TYPES:
        return INCLUDES_FOR_TYPE['ArrayBufferView'].union(
            set([
                'bindings/%s/v8/%s' % (component_dir[base_idl_type],
                                       binding_header_filename(base_idl_type))
            ]))
    if idl_type.is_basic_type:
        return set([
            'bindings/core/v8/idl_types.h',
            'bindings/core/v8/native_value_traits_impl.h'
        ])
    if base_idl_type.endswith('ConstructorConstructor'):
        # FIXME: rename to NamedConstructor
        # FIXME: replace with a [NamedConstructorAttribute] extended attribute
        # Ending with 'ConstructorConstructor' indicates a named constructor,
        # and these do not have header files, as they are part of the generated
        # bindings for the interface
        return set()
    if (base_idl_type.endswith('Constructor')
            and base_idl_type not in _CALLBACK_CONSTRUCTORS):
        # FIXME: replace with a [ConstructorAttribute] extended attribute
        base_idl_type = idl_type.constructor_type_name
    if idl_type.is_custom_callback_function:
        return set()
    if idl_type.is_callback_function:
        component = IdlType.callback_functions[base_idl_type]['component_dir']
        return set([
            'bindings/%s/v8/%s' % (component,
                                   binding_header_filename(base_idl_type))
        ])
    if base_idl_type not in component_dir:
        return set()
    return set([
        'bindings/%s/v8/%s' % (component_dir[base_idl_type],
                               binding_header_filename(base_idl_type))
    ])


IdlType.includes_for_type = includes_for_type


def includes_for_union_type(idl_type, extended_attributes=None):
    return set.union(*[
        member_type.includes_for_type(extended_attributes)
        for member_type in idl_type.member_types
    ])


IdlUnionType.includes_for_type = includes_for_union_type


def includes_for_array_or_sequence_type(idl_type, extended_attributes=None):
    return set.union(
        set([
            'bindings/core/v8/idl_types.h',
            'bindings/core/v8/native_value_traits_impl.h'
        ]), idl_type.element_type.includes_for_type(extended_attributes))


IdlArrayOrSequenceType.includes_for_type = includes_for_array_or_sequence_type


def includes_for_record_type(idl_type, extended_attributes=None):
    return set.union(
        idl_type.key_type.includes_for_type(extended_attributes),
        idl_type.value_type.includes_for_type(extended_attributes))


IdlRecordType.includes_for_type = includes_for_record_type


def add_includes_for_type(idl_type, extended_attributes=None):
    includes.update(idl_type.includes_for_type(extended_attributes))


IdlTypeBase.add_includes_for_type = add_includes_for_type


def includes_for_interface(interface_name):
    return IdlType(interface_name).includes_for_type()


def add_includes_for_interface(interface_name):
    includes.update(includes_for_interface(interface_name))


def impl_includes_for_type(idl_type, interfaces_info):
    includes_for_type = set()

    idl_type = idl_type.preprocessed_type
    native_array_element_type = idl_type.native_array_element_type
    if native_array_element_type:
        includes_for_type.update(
            impl_includes_for_type(native_array_element_type, interfaces_info))
        includes_for_type.add('platform/wtf/vector.h')

    base_idl_type = idl_type.base_type
    if idl_type.is_string_type:
        includes_for_type.add('platform/wtf/text/wtf_string.h')
    if idl_type.is_record_type:
        includes_for_type.update(impl_includes_for_type(idl_type.key_type,
                                                        interfaces_info))
        includes_for_type.update(impl_includes_for_type(idl_type.value_type,
                                                        interfaces_info))
    if idl_type.is_callback_function:
        component = IdlType.callback_functions[base_idl_type]['component_dir']
        return set([
            'bindings/%s/v8/%s' % (component,
                                   binding_header_filename(base_idl_type))
        ])
    if base_idl_type in interfaces_info:
        interface_info = interfaces_info[base_idl_type]
        includes_for_type.add(interface_info['include_path'])
    if base_idl_type in INCLUDES_FOR_TYPE:
        includes_for_type.update(INCLUDES_FOR_TYPE[base_idl_type])
    if idl_type.is_array_buffer_view_or_typed_array:
        return set([
            'core/typed_arrays/dom_typed_array.h',
            'core/typed_arrays/array_buffer_view_helpers.h'
        ])
    return includes_for_type


def impl_includes_for_type_union(idl_type, interfaces_info):
    includes_for_type = set()
    for member_type in idl_type.member_types:
        includes_for_type.update(
            member_type.impl_includes_for_type(interfaces_info))
    return includes_for_type


IdlTypeBase.impl_includes_for_type = impl_includes_for_type
IdlUnionType.impl_includes_for_type = impl_includes_for_type_union


def impl_forward_declaration_name(idl_type):
    element_type = idl_type.native_array_element_type
    if element_type:
        return element_type.impl_forward_declaration_name

    if ((idl_type.is_wrapper_type
         and not idl_type.is_array_buffer_view_or_typed_array)
            or idl_type.is_dictionary):
        return idl_type.implemented_as
    return None


IdlTypeBase.impl_forward_declaration_name = property(
    impl_forward_declaration_name)

component_dir = {}


def set_component_dirs(new_component_dirs):
    component_dir.update(new_component_dirs)


################################################################################
# V8 -> C++
################################################################################

# TODO(rakuco): Get rid of this definition altogether and move to NativeValueTraits<T>::nativeValue().
#               That requires not requiring ExceptionState where it is not used, and we must be careful not
#               to introduce any performance regressions.
V8_VALUE_TO_CPP_VALUE = {
    # Basic
    'DOMString':
    '{v8_value}',
    # Interface types
    'FlexibleArrayBufferView':
    'ToFlexibleArrayBufferView({isolate}, {v8_value}, {variable_name})',
    'Promise':
    'ScriptPromise::Cast(ScriptState::Current({isolate}), {v8_value})',
    'ScriptValue':
    'ScriptValue({isolate}, {v8_value})',
    'Window':
    'ToDOMWindow({isolate}, {v8_value})',
    'XPathNSResolver':
    'ToXPathNSResolver(ScriptState::Current({isolate}), {v8_value})',
}


def v8_conversion_needs_exception_state(idl_type):
    return (idl_type.is_numeric_type or idl_type.is_enum
            or idl_type.is_dictionary
            or idl_type.is_array_buffer_view_or_typed_array
            or idl_type.has_string_context or
            idl_type.name in ('Boolean', 'ByteString', 'Object', 'USVString'))


IdlType.v8_conversion_needs_exception_state = property(
    v8_conversion_needs_exception_state)
IdlAnnotatedType.v8_conversion_needs_exception_state = property(
    v8_conversion_needs_exception_state)
IdlArrayOrSequenceType.v8_conversion_needs_exception_state = True
IdlRecordType.v8_conversion_needs_exception_state = True
IdlUnionType.v8_conversion_needs_exception_state = True

TRIVIAL_CONVERSIONS = frozenset(
    ['any', 'boolean', 'NodeFilter', 'XPathNSResolver', 'Promise'])


def v8_conversion_is_trivial(idl_type):
    # The conversion is a simple expression that returns the converted value and
    # cannot raise an exception.
    return (idl_type.base_type in TRIVIAL_CONVERSIONS
            or idl_type.is_wrapper_type)


IdlType.v8_conversion_is_trivial = property(v8_conversion_is_trivial)


def native_value_traits_type_name(idl_type,
                                  extended_attributes,
                                  in_sequence_or_record=False):
    idl_type = idl_type.preprocessed_type

    if idl_type.is_string_type:
        # Strings are handled separately because null and/or undefined are
        # processed by V8StringResource due to the [TreatNullAs] extended
        # attribute and nullable string types.
        name = 'IDL%s' % idl_type.name
    elif idl_type.is_nullable:
        inner_type = idl_type.inner_type
        inner_type_nvt_type = native_value_traits_type_name(
            inner_type, extended_attributes)
        # The IDL compiler has special cases to handle some nullable types in operation
        # parameters, dictionary fields, etc.
        if in_sequence_or_record or inner_type.name == 'Object':
            name = 'IDLNullable<%s>' % inner_type_nvt_type
        else:
            name = inner_type_nvt_type
    elif idl_type.native_array_element_type:
        name = 'IDLSequence<%s>' % native_value_traits_type_name(
            idl_type.native_array_element_type, extended_attributes, True)
    elif idl_type.is_record_type:
        name = 'IDLRecord<%s, %s>' % (native_value_traits_type_name(
            idl_type.key_type, extended_attributes),
                                      native_value_traits_type_name(
                                          idl_type.value_type,
                                          extended_attributes, True))
    elif idl_type.is_basic_type or idl_type.name in ['Object', 'Promise']:
        name = 'IDL%s' % idl_type.name
    elif idl_type.implemented_as is not None:
        name = idl_type.implemented_as
    else:
        name = idl_type.name
    return name


def v8_value_to_cpp_value(idl_type, extended_attributes, v8_value,
                          variable_name, isolate, for_constructor_callback):
    if idl_type.name == 'void':
        return ''

    # Simple types
    idl_type = idl_type.preprocessed_type
    base_idl_type = idl_type.as_union_type.name if idl_type.is_union_type else idl_type.base_type

    if 'FlexibleArrayBufferView' in extended_attributes:
        if base_idl_type not in ARRAY_BUFFER_VIEW_AND_TYPED_ARRAY_TYPES:
            raise ValueError(
                'Unrecognized base type for extended attribute "FlexibleArrayBufferView": %s'
                % (idl_type.base_type))
        if 'AllowShared' not in extended_attributes:
            raise ValueError(
                '"FlexibleArrayBufferView" extended attribute requires "AllowShared" on %s'
                % (idl_type.base_type))
        base_idl_type = 'FlexibleArrayBufferView'

    if 'AllowShared' in extended_attributes and not idl_type.is_array_buffer_view_or_typed_array:
        raise ValueError(
            'Unrecognized base type for extended attribute "AllowShared": %s' %
            (idl_type.base_type))

    if idl_type.is_integer_type:
        arguments = ', '.join([v8_value, 'exception_state'])
    elif idl_type.v8_conversion_needs_exception_state:
        arguments = ', '.join([v8_value, 'exception_state'])
    else:
        arguments = v8_value

    if idl_type.has_string_context:
        execution_context = 'bindings::ExecutionContextFromV8Wrappable(impl)'
        if for_constructor_callback:
            execution_context = 'CurrentExecutionContext(info.GetIsolate())'
        cpp_expression_format = 'NativeValueTraits<IDL%s>::NativeValue(%s, %s, exception_state, %s)' % (
            idl_type.name, isolate, v8_value, execution_context)
    elif base_idl_type in V8_VALUE_TO_CPP_VALUE:
        cpp_expression_format = V8_VALUE_TO_CPP_VALUE[base_idl_type]
    elif idl_type.name == 'ArrayBuffer':
        cpp_expression_format = (
            '{v8_value}->Is{idl_type}() ? '
            'V8{idl_type}::ToImpl(v8::Local<v8::{idl_type}>::Cast({v8_value})) : 0'
        )
    elif idl_type.is_array_buffer_view_or_typed_array or base_idl_type == 'DataView':
        this_cpp_type = idl_type.cpp_type_args(
            extended_attributes=extended_attributes)
        if 'AllowShared' in extended_attributes:
            cpp_expression_format = (
                'ToMaybeShared<%s>({isolate}, {v8_value}, exception_state)' %
                this_cpp_type)
        else:
            cpp_expression_format = (
                'ToNotShared<%s>({isolate}, {v8_value}, exception_state)' %
                this_cpp_type)
    elif idl_type.is_union_type:
        nullable = 'UnionTypeConversionMode::kNullable' if idl_type.includes_nullable_type \
            else 'UnionTypeConversionMode::kNotNullable'
        # We need to consider the moving of the null through the union in order
        # to generate the correct V8* class name.
        this_cpp_type = idl_type.cpp_type_args(
            extended_attributes=extended_attributes)
        cpp_expression_format = '%s::ToImpl({isolate}, {v8_value}, {variable_name}, %s, exception_state)' % \
            (v8_type(this_cpp_type), nullable)
    elif idl_type.use_output_parameter_for_result:
        cpp_expression_format = 'V8{idl_type}::ToImpl({isolate}, {v8_value}, {variable_name}, exception_state)'
    elif idl_type.is_callback_function:
        cpp_expression_format = 'V8{idl_type}::Create({v8_value}.As<v8::Function>())'
    elif idl_type.v8_conversion_needs_exception_state:
        # Effectively, this if branch means everything with v8_conversion_needs_exception_state == True
        # except for unions and dictionary interfaces.
        base_idl_type = native_value_traits_type_name(idl_type,
                                                      extended_attributes)
        cpp_expression_format = (
            'NativeValueTraits<{idl_type}>::NativeValue({isolate}, {arguments})'
        )
    else:
        cpp_expression_format = (
            'V8{idl_type}::ToImplWithTypeCheck({isolate}, {v8_value})')

    return cpp_expression_format.format(
        arguments=arguments,
        idl_type=base_idl_type,
        v8_value=v8_value,
        variable_name=variable_name,
        isolate=isolate)


# FIXME: this function should be refactored, as this takes too many flags.
def v8_value_to_local_cpp_value(idl_type,
                                extended_attributes,
                                v8_value,
                                variable_name,
                                declare_variable=True,
                                isolate='info.GetIsolate()',
                                bailout_return_value=None,
                                use_exception_state=False,
                                code_generation_target=None,
                                for_constructor_callback=False):
    """Returns an expression that converts a V8 value to a C++ value and stores it as a local value."""

    this_cpp_type = idl_type.cpp_type_args(
        extended_attributes=extended_attributes, raw_type=True)
    idl_type = idl_type.preprocessed_type

    cpp_value = v8_value_to_cpp_value(
        idl_type,
        extended_attributes,
        v8_value,
        variable_name,
        isolate,
        for_constructor_callback=for_constructor_callback)

    # Optional expression that returns a value to be assigned to the local variable.
    assign_expression = None
    # Optional void expression executed unconditionally.
    set_expression = None
    # Optional expression that returns true if the conversion fails.
    check_expression = None
    # Optional expression used as the return value when returning. Only
    # meaningful if 'check_expression' is not None.
    return_expression = bailout_return_value

    if 'FlexibleArrayBufferView' in extended_attributes:
        if idl_type.base_type not in ARRAY_BUFFER_VIEW_AND_TYPED_ARRAY_TYPES:
            raise ValueError(
                'Unrecognized base type for extended attribute "FlexibleArrayBufferView": %s'
                % (idl_type.base_type))
        set_expression = cpp_value
    elif idl_type.is_string_type or idl_type.v8_conversion_needs_exception_state:
        # Types for which conversion can fail and that need error handling.

        check_expression = 'exception_state.HadException()'

        if idl_type.is_union_type:
            set_expression = cpp_value
        else:
            assign_expression = cpp_value
            # Note: 'not idl_type.v8_conversion_needs_exception_state' implies
            # 'idl_type.is_string_type', but there are types for which both are
            # true (ByteString and USVString), so using idl_type.is_string_type
            # as the condition here would be wrong.
            if not idl_type.v8_conversion_needs_exception_state:
                if use_exception_state:
                    check_expression = '!%s.Prepare(exception_state)' % variable_name
                else:
                    check_expression = '!%s.Prepare()' % variable_name
    elif not idl_type.v8_conversion_is_trivial and not idl_type.is_callback_function:
        return {
            'error_message':
            'no V8 -> C++ conversion for IDL type: %s' % idl_type.name
        }
    else:
        assign_expression = cpp_value

    # Types that don't need error handling, and simply assign a value to the
    # local variable.

    if (idl_type.is_explicit_nullable
            and code_generation_target == 'attribute_set'):
        this_cpp_type = cpp_template_type('base::Optional', this_cpp_type)
        expr = '{cpp_type}({expr})'.format(
            cpp_type=this_cpp_type, expr=assign_expression)
        assign_expression = ("is_null "
                             "? base::nullopt "
                             ": {expr}".format(expr=expr))

    return {
        'assign_expression': assign_expression,
        'check_expression': check_expression,
        'cpp_type': this_cpp_type,
        'cpp_name': variable_name,
        'declare_variable': declare_variable,
        'return_expression': return_expression,
        'set_expression': set_expression,
    }


IdlTypeBase.v8_value_to_local_cpp_value = v8_value_to_local_cpp_value


def use_output_parameter_for_result(idl_type):
    """True when methods/getters which return the given idl_type should
    take the output argument.
    """
    return idl_type.is_union_type


IdlTypeBase.use_output_parameter_for_result = property(
    use_output_parameter_for_result)

################################################################################
# C++ -> V8
################################################################################


def preprocess_idl_type(idl_type):
    if idl_type.is_nullable:
        return IdlNullableType(idl_type.inner_type.preprocessed_type)
    if idl_type.is_enum:
        # Enumerations are internally DOMStrings
        return IdlType('DOMString')
    if idl_type.base_type == 'any' or idl_type.is_custom_callback_function:
        return IdlType('ScriptValue')
    if idl_type.is_callback_function:
        return idl_type
    return idl_type


IdlTypeBase.preprocessed_type = property(preprocess_idl_type)


def preprocess_idl_type_and_value(idl_type, cpp_value, extended_attributes):
    """Returns IDL type and value, with preliminary type conversions applied."""
    idl_type = idl_type.preprocessed_type
    if idl_type.name == 'Promise':
        idl_type = IdlType('ScriptValue')
    if idl_type.base_type in ['long long', 'unsigned long long']:
        # long long and unsigned long long are not representable in ECMAScript;
        # we represent them as doubles.
        is_nullable = idl_type.is_nullable
        idl_type = IdlType('double')
        if is_nullable:
            idl_type = IdlNullableType(idl_type)
        cpp_value = 'static_cast<double>(%s)' % cpp_value
    # HTML5 says that unsigned reflected attributes should be in the range
    # [0, 2^31). When a value isn't in this range, a default value (or 0)
    # should be returned instead.
    extended_attributes = extended_attributes or {}
    if ('Reflect' in extended_attributes
            and idl_type.base_type in ['unsigned long', 'unsigned short']):
        cpp_value = cpp_value.replace('GetUnsignedIntegralAttribute',
                                      'GetIntegralAttribute')
        cpp_value = 'std::max(0, static_cast<int>(%s))' % cpp_value
    return idl_type, cpp_value


def v8_conversion_type(idl_type, extended_attributes):
    """Returns V8 conversion type, adding any additional includes.

    The V8 conversion type is used to select the C++ -> V8 conversion function
    or V8SetReturnValue* function; it can be an idl_type, a cpp_type, or a
    separate name for the type of conversion (e.g., 'DOMWrapper').
    """
    extended_attributes = extended_attributes or {}

    # Nullable dictionaries need to be handled differently than either
    # non-nullable dictionaries or unions.
    if idl_type.is_dictionary and idl_type.is_nullable:
        return 'NullableDictionary'

    if idl_type.is_dictionary or idl_type.is_union_type:
        return 'DictionaryOrUnion'

    # Array or sequence types
    native_array_element_type = idl_type.native_array_element_type
    if native_array_element_type:
        return 'FrozenArray' if idl_type.is_frozen_array else 'sequence'

    # Record types.
    if idl_type.is_record_type:
        return 'Record'

    # Simple types
    base_idl_type = idl_type.base_type
    # Basic types, without additional includes
    if base_idl_type in CPP_INTEGER_CONVERSION_RULES:
        return CPP_INTEGER_CONVERSION_RULES[base_idl_type]
    if idl_type.is_string_type:
        if idl_type.is_nullable:
            return 'StringOrNull'
        return base_idl_type
    if idl_type.is_basic_type:
        return base_idl_type
    if base_idl_type in ['object', 'ScriptValue']:
        return 'ScriptValue'

    # Data type with potential additional includes
    if base_idl_type in V8_SET_RETURN_VALUE:  # Special V8SetReturnValue treatment
        return base_idl_type

    # Pointer type
    return 'DOMWrapper'


IdlTypeBase.v8_conversion_type = v8_conversion_type

V8_SET_RETURN_VALUE = {
    'boolean':
    'V8SetReturnValueBool(info, {cpp_value})',
    'DOMString':
    'V8SetReturnValueString(info, {cpp_value}, info.GetIsolate())',
    'ByteString':
    'V8SetReturnValueString(info, {cpp_value}, info.GetIsolate())',
    'USVString':
    'V8SetReturnValueString(info, {cpp_value}, info.GetIsolate())',
    'StringOrNull':
    'V8SetReturnValueStringOrNull(info, {cpp_value}, info.GetIsolate())',
    'void':
    '',
    # All the int types below are converted to (u)int32_t in the V8SetReturnValue{Int,Unsigned}() calls.
    # The 64-bit int types have already been converted to double when V8_SET_RETURN_VALUE is used, so they are not
    # listed here.
    'int8_t':
    'V8SetReturnValueInt(info, {cpp_value})',
    'int16_t':
    'V8SetReturnValueInt(info, {cpp_value})',
    'int32_t':
    'V8SetReturnValueInt(info, {cpp_value})',
    'uint8_t':
    'V8SetReturnValueUnsigned(info, {cpp_value})',
    'uint16_t':
    'V8SetReturnValueUnsigned(info, {cpp_value})',
    'uint32_t':
    'V8SetReturnValueUnsigned(info, {cpp_value})',
    # No special V8SetReturnValue* function (set value directly)
    'float':
    'V8SetReturnValue(info, {cpp_value})',
    'unrestricted float':
    'V8SetReturnValue(info, {cpp_value})',
    'double':
    'V8SetReturnValue(info, {cpp_value})',
    'unrestricted double':
    'V8SetReturnValue(info, {cpp_value})',
    # No special V8SetReturnValue* function, but instead convert value to V8
    # and then use general V8SetReturnValue.
    'sequence':
    'V8SetReturnValue(info, {cpp_value})',
    'FrozenArray':
    'V8SetReturnValue(info, {cpp_value})',
    'EventHandler':
    'V8SetReturnValue(info, {cpp_value})',
    'NodeFilter':
    'V8SetReturnValue(info, {cpp_value})',
    'OnBeforeUnloadEventHandler':
    'V8SetReturnValue(info, {cpp_value})',
    'OnErrorEventHandler':
    'V8SetReturnValue(info, {cpp_value})',
    'ScriptValue':
    'V8SetReturnValue(info, {cpp_value})',
    # Records.
    'Record':
    'V8SetReturnValue(info, ToV8({cpp_value}, info.Holder(), info.GetIsolate()))',
    # DOMWrapper
    'DOMWrapperForMainWorld':
    'V8SetReturnValueForMainWorld(info, {cpp_value})',
    'DOMWrapperFast':
    'V8SetReturnValueFast(info, {cpp_value}, {script_wrappable})',
    'DOMWrapperDefault':
    'V8SetReturnValue(info, {cpp_value})',
    # If [CheckSecurity=ReturnValue] is specified, the returned object must be
    # wrapped in its own realm, which can be different from the realm of the
    # receiver object.
    #
    # [CheckSecurity=ReturnValue] is used only for contentDocument and
    # getSVGDocument attributes of HTML{IFrame,Frame,Object,Embed}Element,
    # and Window.frameElement.  Except for Window.frameElement, all interfaces
    # support contentWindow(), so we create a new wrapper in the realm of
    # contentWindow().  Note that DOMWindow* has its own realm and there is no
    # need to pass |creationContext| in for ToV8(DOMWindow*).
    # Window.frameElement is implemented with [Custom].
    'DOMWrapperAcrossContext':
    ('V8SetReturnValue(info, ToV8({cpp_value}, ' +
     'ToV8(impl->contentWindow(), v8::Local<v8::Object>(), ' +
     'info.GetIsolate()).As<v8::Object>(), info.GetIsolate()))'),
    # Note that static attributes and operations do not check whether |this| is
    # an instance of the interface nor |this|'s creation context is the same as
    # the current context.  So we must always use the current context as the
    # creation context of the DOM wrapper for the return value.
    'DOMWrapperStatic':
    'V8SetReturnValue(info, {cpp_value}, info.GetIsolate()->GetCurrentContext()->Global())',
    # Nullable dictionaries
    'NullableDictionary':
    'V8SetReturnValue(info, result)',
    'NullableDictionaryStatic':
    'V8SetReturnValue(info, result, info.GetIsolate()->GetCurrentContext()->Global())',
    # Union types or dictionaries
    'DictionaryOrUnion':
    'V8SetReturnValue(info, result)',
    'DictionaryOrUnionStatic':
    'V8SetReturnValue(info, result, info.GetIsolate()->GetCurrentContext()->Global())',
}


def v8_set_return_value(idl_type,
                        cpp_value,
                        extended_attributes=None,
                        script_wrappable='',
                        for_main_world=False,
                        is_static=False):
    """Returns a statement that converts a C++ value to a V8 value and sets it as a return value.

    """

    def dom_wrapper_conversion_type():
        if ('CheckSecurity' in extended_attributes
                and extended_attribute_value_contains(
                    extended_attributes['CheckSecurity'], 'ReturnValue')):
            return 'DOMWrapperAcrossContext'
        if is_static:
            return 'DOMWrapperStatic'
        if not script_wrappable:
            return 'DOMWrapperDefault'
        if for_main_world:
            return 'DOMWrapperForMainWorld'
        return 'DOMWrapperFast'

    idl_type, cpp_value = preprocess_idl_type_and_value(
        idl_type, cpp_value, extended_attributes)
    this_v8_conversion_type = idl_type.v8_conversion_type(extended_attributes)
    # SetReturn-specific overrides
    if this_v8_conversion_type in ('EventHandler', 'NodeFilter',
                                   'OnBeforeUnloadEventHandler',
                                   'OnErrorEventHandler', 'ScriptValue',
                                   'sequence', 'FrozenArray'):
        # Convert value to V8 and then use general V8SetReturnValue
        cpp_value = idl_type.cpp_value_to_v8_value(
            cpp_value, extended_attributes=extended_attributes)
    if this_v8_conversion_type == 'DOMWrapper':
        this_v8_conversion_type = dom_wrapper_conversion_type()
    if is_static and this_v8_conversion_type in ('NullableDictionary',
                                                 'DictionaryOrUnion'):
        this_v8_conversion_type += 'Static'

    format_string = V8_SET_RETURN_VALUE[this_v8_conversion_type]
    statement = format_string.format(
        cpp_value=cpp_value, script_wrappable=script_wrappable)
    return statement


IdlTypeBase.v8_set_return_value = v8_set_return_value

CPP_VALUE_TO_V8_VALUE = {
    # Built-in types
    'DOMString':
    'V8String({isolate}, {cpp_value})',
    'ByteString':
    'V8String({isolate}, {cpp_value})',
    'USVString':
    'V8String({isolate}, {cpp_value})',
    'boolean':
    'v8::Boolean::New({isolate}, {cpp_value})',
    # All the int types below are converted to (u)int32_t in the v8::Integer::New*() calls.
    # The 64-bit int types have already been converted to double when CPP_VALUE_TO_V8_VALUE is used, so they are not
    # listed here.
    'int8_t':
    'v8::Integer::New({isolate}, {cpp_value})',
    'int16_t':
    'v8::Integer::New({isolate}, {cpp_value})',
    'int32_t':
    'v8::Integer::New({isolate}, {cpp_value})',
    'uint8_t':
    'v8::Integer::NewFromUnsigned({isolate}, {cpp_value})',
    'uint16_t':
    'v8::Integer::NewFromUnsigned({isolate}, {cpp_value})',
    'uint32_t':
    'v8::Integer::NewFromUnsigned({isolate}, {cpp_value})',
    'float':
    'v8::Number::New({isolate}, {cpp_value})',
    'unrestricted float':
    'v8::Number::New({isolate}, {cpp_value})',
    'double':
    'v8::Number::New({isolate}, {cpp_value})',
    'unrestricted double':
    'v8::Number::New({isolate}, {cpp_value})',
    'StringOrNull':
    ('({cpp_value}.IsNull() ? ' + 'v8::Null({isolate}).As<v8::Value>() : ' +
     'V8String({isolate}, {cpp_value}).As<v8::Value>())'),
    # Special cases
    'EventHandler':
    'JSEventHandler::AsV8Value({isolate}, impl, {cpp_value})',
    'NodeFilter':
    'ToV8({cpp_value}, {creation_context}, {isolate})',
    'OnBeforeUnloadEventHandler':
    'JSEventHandler::AsV8Value({isolate}, impl, {cpp_value})',
    'OnErrorEventHandler':
    'JSEventHandler::AsV8Value({isolate}, impl, {cpp_value})',
    'Record':
    'ToV8({cpp_value}, {creation_context}, {isolate})',
    'ScriptValue':
    '{cpp_value}.V8Value()',
    # General
    'sequence':
    'ToV8({cpp_value}, {creation_context}, {isolate})',
    'FrozenArray':
    'FreezeV8Object(ToV8({cpp_value}, {creation_context}, {isolate}), {isolate})',
    'DOMWrapper':
    'ToV8({cpp_value}, {creation_context}, {isolate})',
    # Passing nullable dictionaries isn't a pattern currently used
    # anywhere in the web platform, and more work would be needed in
    # the code generator to distinguish between passing null, and
    # passing an object which happened to not contain any of the
    # dictionary's defined attributes. For now, don't define
    # NullableDictionary here, which will cause an exception to be
    # thrown during code generation if an argument to a method is a
    # nullable dictionary type.
    #
    # Union types or dictionaries
    'DictionaryOrUnion':
    'ToV8({cpp_value}, {creation_context}, {isolate})',
}


def cpp_value_to_v8_value(idl_type,
                          cpp_value,
                          isolate='info.GetIsolate()',
                          creation_context='info.Holder()',
                          extended_attributes=None):
    """Returns an expression that converts a C++ value to a V8 value."""
    # the isolate parameter is needed for callback interfaces
    idl_type, cpp_value = preprocess_idl_type_and_value(
        idl_type, cpp_value, extended_attributes)
    this_v8_conversion_type = idl_type.v8_conversion_type(extended_attributes)
    format_string = CPP_VALUE_TO_V8_VALUE[this_v8_conversion_type]
    statement = format_string.format(
        cpp_value=cpp_value,
        isolate=isolate,
        creation_context=creation_context)
    return statement


IdlTypeBase.cpp_value_to_v8_value = cpp_value_to_v8_value


def literal_cpp_value(idl_type, idl_literal):
    """Converts an expression that is a valid C++ literal for this type."""
    # FIXME: add validation that idl_type and idl_literal are compatible
    if idl_type.base_type in ('any', 'object') and idl_literal.is_null:
        return 'ScriptValue::CreateNull(script_state->GetIsolate())'
    literal_value = str(idl_literal)
    if idl_type.base_type in ('octet', 'unsigned short', 'unsigned long'):
        return literal_value + 'u'
    if idl_type.is_dictionary and literal_value == '{}':
        return 'MakeGarbageCollected<{}>()'.format(idl_type.base_type)
    return literal_value


def union_literal_cpp_value(idl_type, idl_literal):
    if idl_literal.is_null:
        return idl_type.name + '()'
    elif idl_literal.idl_type == 'DOMString':
        member_type = idl_type.string_member_type
    elif idl_literal.idl_type in ('integer', 'float'):
        member_type = idl_type.numeric_member_type
    elif idl_literal.idl_type == 'boolean':
        member_type = idl_type.boolean_member_type
    elif idl_literal.idl_type == 'sequence':
        member_type = idl_type.sequence_member_type
    elif idl_literal.idl_type == 'dictionary':
        member_type = idl_type.dictionary_member_type
    else:
        raise ValueError('Unsupported literal type: ' + idl_literal.idl_type)

    return '%s::From%s(%s)' % (idl_type.cpp_type_args(), member_type.name,
                               member_type.literal_cpp_value(idl_literal))


def array_or_sequence_literal_cpp_value(idl_type, idl_literal):
    # Only support empty sequences.
    if idl_literal.value == '[]':
        return cpp_type(idl_type) + '()'
    raise ValueError('Unsupported literal type: ' + idl_literal.idl_type)


IdlType.literal_cpp_value = literal_cpp_value
IdlUnionType.literal_cpp_value = union_literal_cpp_value
IdlArrayOrSequenceType.literal_cpp_value = array_or_sequence_literal_cpp_value

_IDL_TYPE_TO_NATIVE_VALUE_TRAITS_TAG_MAP = {
    'DOMString': 'IDLString',
    'USVString': 'IDLUSVString',
    'DOMStringOrNull': 'IDLStringOrNull',
    'USVStringOrNull': 'IDLUSVStringOrNull',
    'any': 'ScriptValue',
    'boolean': 'IDLBoolean',
    'long': 'IDLLong',
    'sequence<DOMString>': 'IDLSequence<IDLString>',
    'unsigned short': 'IDLUnsignedShort',
    'void': None,
}


def idl_type_to_native_value_traits_tag(idl_type):
    idl_type_str = str(idl_type)
    if idl_type.is_nullable:
        idl_type_str += "OrNull"
    if idl_type_str in _IDL_TYPE_TO_NATIVE_VALUE_TRAITS_TAG_MAP:
        return _IDL_TYPE_TO_NATIVE_VALUE_TRAITS_TAG_MAP[idl_type_str]
    else:
        raise Exception("Type `%s' is not supported." % idl_type_str)


################################################################################
# Utility properties for nullable types
################################################################################


def cpp_type_has_null_value(idl_type):
    # - String types (String/AtomicString) represent null as a null string,
    #   i.e. one for which String::IsNull() returns true.
    # - Enum types, as they are implemented as Strings.
    # - Interface types and Dictionary types represent null as a null pointer.
    # - Union types, as thier container classes can represent null value.
    # - 'Object' and 'any' type. We use ScriptValue for object type.
    return (idl_type.is_string_type or idl_type.is_enum
            or idl_type.is_interface_type or idl_type.is_callback_interface
            or idl_type.is_callback_function
            or idl_type.is_custom_callback_function or idl_type.is_dictionary
            or idl_type.is_union_type or idl_type.base_type == 'object'
            or idl_type.base_type == 'any')


IdlTypeBase.cpp_type_has_null_value = property(cpp_type_has_null_value)


def is_implicit_nullable(idl_type):
    # Nullable type where the corresponding C++ type supports a null value.
    return idl_type.is_nullable and idl_type.cpp_type_has_null_value


def is_explicit_nullable(idl_type):
    # Nullable type that isn't implicit nullable (see above.) For such types,
    # we use base::Optional<T> or similar explicit ways to represent a null value.
    return idl_type.is_nullable and not idl_type.is_implicit_nullable


IdlTypeBase.is_implicit_nullable = property(is_implicit_nullable)
IdlUnionType.is_implicit_nullable = False
IdlTypeBase.is_explicit_nullable = property(is_explicit_nullable)


def includes_nullable_type_union(idl_type):
    # http://heycam.github.io/webidl/#dfn-includes-a-nullable-type
    return idl_type.number_of_nullable_member_types == 1


IdlTypeBase.includes_nullable_type = False
IdlNullableType.includes_nullable_type = True
IdlUnionType.includes_nullable_type = property(includes_nullable_type_union)
