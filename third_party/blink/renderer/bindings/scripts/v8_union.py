# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from utilities import to_snake_case
import v8_types
import v8_utilities

UNION_CPP_INCLUDES = frozenset([
    'base/stl_util.h',
    'bindings/core/v8/native_value_traits_impl.h',
    'bindings/core/v8/to_v8_for_core.h',
])

UNION_H_INCLUDES = frozenset([
    'bindings/core/v8/dictionary.h',
    'bindings/core/v8/native_value_traits.h',
    'bindings/core/v8/v8_binding_for_core.h',
    'platform/bindings/exception_state.h',
    'platform/heap/handle.h',
    'third_party/abseil-cpp/absl/types/optional.h',
])

cpp_includes = set()
header_forward_decls = set()
header_includes = set()


def container_context(union_type, info_provider):
    cpp_includes.clear()
    header_forward_decls.clear()
    header_includes.clear()
    cpp_includes.update(UNION_CPP_INCLUDES)
    header_includes.update(UNION_H_INCLUDES)
    members = []

    # These variables refer to member contexts if the given union type has
    # corresponding types. They are used for V8 -> impl conversion.
    array_buffer_type = None
    array_buffer_view_type = None
    array_or_sequence_type = None
    boolean_type = None
    dictionary_type = None
    interface_types = []
    numeric_type = None
    object_type = None
    record_type = None
    string_type = None
    for member in sorted(
            union_type.flattened_member_types, key=lambda m: m.name):
        context = member_context(member, info_provider)
        members.append(context)
        if member.base_type == 'ArrayBuffer':
            if array_buffer_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            array_buffer_type = context
        elif member.base_type == 'ArrayBufferView':
            if array_buffer_view_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            array_buffer_view_type = context
        elif member.is_dictionary:
            if dictionary_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            dictionary_type = context
        elif member.is_array_or_sequence_type:
            if array_or_sequence_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            array_or_sequence_type = context
        elif member.base_type == 'object':
            if object_type or record_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            object_type = context
        elif member.is_record_type:
            if object_type or record_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            record_type = context
        elif member.is_interface_type:
            interface_types.append(context)
        elif member is union_type.boolean_member_type:
            boolean_type = context
        elif member is union_type.numeric_member_type:
            numeric_type = context
        elif member is union_type.string_member_type:
            string_type = context
        else:
            raise Exception(
                '%s is not supported as an union member.' % member.name)

    # Nullable restriction checks
    nullable_members = union_type.number_of_nullable_member_types
    if nullable_members > 1:
        raise Exception(
            '%s contains more than one nullable members' % union_type.name)
    if dictionary_type and nullable_members == 1:
        raise Exception(
            '%s has a dictionary and a nullable member' % union_type.name)

    cpp_class = union_type.cpp_type
    return {
        'array_buffer_type': array_buffer_type,
        'array_buffer_view_type': array_buffer_view_type,
        'array_or_sequence_type': array_or_sequence_type,
        'boolean_type': boolean_type,
        'cpp_class': cpp_class,
        'cpp_includes': sorted(cpp_includes),
        'dictionary_type': dictionary_type,
        'header_includes': sorted(header_includes),
        'header_forward_decls': sorted(header_forward_decls),
        'includes_nullable_type': union_type.includes_nullable_type,
        'interface_types': interface_types,
        'members': members,
        'numeric_type': numeric_type,
        'object_type': object_type,
        'record_type': record_type,
        'string_type': string_type,
        'type_string': str(union_type),
        'v8_class': v8_types.v8_type(cpp_class),
    }


def _update_includes_and_forward_decls(member, info_provider):
    interface_info = info_provider.interfaces_info.get(member.name, None)
    if interface_info:
        cpp_includes.update(
            interface_info.get('dependencies_include_paths', []))
        # We need complete types for IDL dictionaries in union containers.
        if member.is_dictionary or member.is_array_buffer_view_or_typed_array:
            header_includes.update(member.includes_for_type())
        else:
            cpp_includes.update(member.includes_for_type())
            header_forward_decls.add(member.implemented_as)
    else:
        if member.is_record_type:
            _update_includes_and_forward_decls(member.key_type, info_provider)
            _update_includes_and_forward_decls(member.value_type,
                                               info_provider)
        elif member.is_array_or_sequence_type:
            _update_includes_and_forward_decls(member.element_type,
                                               info_provider)
            cpp_includes.add('bindings/core/v8/script_iterator.h')
        elif member.is_union_type:
            # Reaching this block means we have a union that is inside a
            # record or sequence.
            header_forward_decls.add(member.name)
            cpp_includes.update(
                [info_provider.include_path_for_union_types(member)])
        cpp_includes.update(member.includes_for_type())


def member_context(member, info_provider):
    _update_includes_and_forward_decls(member, info_provider)
    if member.is_nullable:
        member = member.inner_type
    type_name = (member.inner_type
                 if member.is_annotated_type else member).name
    # When converting a sequence or frozen array, we need to call the GetMethod(V, @@iterator)
    # ES abstract operation and then use the result of that call to create a sequence from an
    # iterable. For the purposes of this method, it means we need to pass |script_iterator|
    # rather than |v8_value| in v8_value_to_local_cpp_value().
    if member.is_array_or_sequence_type:
        v8_value_name = 'std::move(script_iterator)'
    else:
        v8_value_name = 'v8_value'
    return {
        'cpp_name':
        to_snake_case(v8_utilities.cpp_name(member)),
        'cpp_type':
        member.cpp_type_args(used_in_cpp_sequence=True),
        'cpp_local_type':
        member.cpp_type,
        'cpp_value_to_v8_value':
        member.cpp_value_to_v8_value(
            cpp_value='impl.GetAs%s()' % type_name,
            isolate='isolate',
            creation_context='creationContext'),
        'enum_type':
        member.enum_type,
        'enum_values':
        member.enum_values,
        'is_array_buffer_or_view_type':
        member.is_array_buffer_or_view,
        'is_array_buffer_view_or_typed_array':
        member.is_array_buffer_view_or_typed_array,
        'is_traceable':
        member.is_traceable,
        'rvalue_cpp_type':
        member.cpp_type_args(used_as_rvalue_type=True),
        'specific_type_enum':
        'k' + member.name,
        'type_name':
        type_name,
        'v8_value_to_local_cpp_value':
        member.v8_value_to_local_cpp_value({},
                                           v8_value_name,
                                           'cpp_value',
                                           isolate='isolate',
                                           use_exception_state=True)
    }
