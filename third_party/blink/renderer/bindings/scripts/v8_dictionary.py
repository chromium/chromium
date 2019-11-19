# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate template contexts of dictionaries for both v8 bindings and
implementation classes that are used by blink's core/modules.
"""

import operator
from idl_types import IdlType
from utilities import to_snake_case
from v8_globals import includes
import v8_types
import v8_utilities
from v8_utilities import has_extended_attribute_value


DICTIONARY_H_INCLUDES = frozenset([
    'bindings/core/v8/native_value_traits.h',
    'bindings/core/v8/to_v8_for_core.h',
    'bindings/core/v8/v8_binding_for_core.h',
    'platform/heap/handle.h',
])

DICTIONARY_CPP_INCLUDES = frozenset([
    'base/stl_util.h',
    'platform/bindings/exception_state.h',
])


def getter_name_for_dictionary_member(member):
    name = v8_utilities.cpp_name(member)
    return name


def setter_name_for_dictionary_member(member):
    name = v8_utilities.cpp_name(member)
    return 'set%s' % v8_utilities.capitalize(name)


def null_setter_name_for_dictionary_member(member):
    if member.idl_type.is_nullable:
        name = v8_utilities.cpp_name(member)
        return 'set%sToNull' % v8_utilities.capitalize(name)
    return None


def has_method_name_for_dictionary_member(member):
    name = v8_utilities.cpp_name(member)
    return 'has%s' % v8_utilities.capitalize(name)


def unwrap_nullable_if_needed(idl_type):
    if idl_type.is_nullable:
        return idl_type.inner_type
    return idl_type


# Context for V8 bindings

def dictionary_context(dictionary, interfaces_info, component_info):
    includes.clear()
    includes.update(DICTIONARY_CPP_INCLUDES)

    if 'RuntimeEnabled' in dictionary.extended_attributes:
        raise Exception(
            'Dictionary cannot be RuntimeEnabled: %s' % dictionary.name)

    members = [member_context(dictionary, member, component_info)
               for member in sorted(dictionary.members,
                                    key=operator.attrgetter('name'))]

    for member in members:
        if member['runtime_enabled_feature_name']:
            includes.add('platform/runtime_enabled_features.h')
            break

    has_origin_trial_members = False
    for member in members:
        if member['origin_trial_feature_name']:
            has_origin_trial_members = True
            includes.add('core/origin_trials/origin_trials.h')
            includes.add('core/execution_context/execution_context.h')
            break

    cpp_class = v8_utilities.cpp_name(dictionary)
    context = {
        'cpp_class': cpp_class,
        'has_origin_trial_members': has_origin_trial_members,
        'header_includes': set(DICTIONARY_H_INCLUDES),
        'members': members,
        'required_member_names': sorted([member.name
                                         for member in dictionary.members
                                         if member.is_required]),
        'use_permissive_dictionary_conversion': 'PermissiveDictionaryConversion' in dictionary.extended_attributes,
        'v8_class': v8_types.v8_type(cpp_class),
    }
    if dictionary.parent:
        IdlType(dictionary.parent).add_includes_for_type()
        parent_cpp_class = v8_utilities.cpp_name_from_interfaces_info(
            dictionary.parent, interfaces_info)
        context.update({
            'parent_cpp_class': parent_cpp_class,
            'parent_v8_class': v8_types.v8_type(parent_cpp_class),
        })
    return context


def member_context(_, member, component_info):
    extended_attributes = member.extended_attributes
    idl_type = member.idl_type
    idl_type.add_includes_for_type(extended_attributes)
    unwrapped_idl_type = unwrap_nullable_if_needed(idl_type)

    if member.is_required and member.default_value:
        raise Exception(
            'Required member %s must not have a default value.' % member.name)

    if idl_type.is_nullable and idl_type.inner_type.is_dictionary:
        raise Exception(
            'The inner type of nullable member %s must not be a dictionary.' %
            member.name)

    # In most cases, we don't have to distinguish `null` and `not present`,
    # and use null-states (e.g. nullptr, foo.IsUndefinedOrNull()) to show such
    # states for some types for memory usage and performance.
    # For types whose |has_explicit_presence| is True, we provide explicit
    # states of presence.
    has_explicit_presence = (
        idl_type.is_nullable and idl_type.inner_type.is_interface_type)

    def default_values():
        if not member.default_value:
            return None, None
        if member.default_value.is_null:
            return None, 'v8::Null(isolate)'

        cpp_default_value = unwrapped_idl_type.literal_cpp_value(
            member.default_value)
        v8_default_value = unwrapped_idl_type.cpp_value_to_v8_value(
            cpp_value=cpp_default_value, isolate='isolate',
            creation_context='creationContext')
        return cpp_default_value, v8_default_value

    cpp_default_value, v8_default_value = default_values()
    snake_case_name = to_snake_case(member.name)
    cpp_value = snake_case_name + "_cpp_value"
    v8_value = snake_case_name + "_value"
    has_value_or_default = snake_case_name + "_has_value_or_default"
    getter_name = getter_name_for_dictionary_member(member)
    is_deprecated_dictionary = unwrapped_idl_type.name == 'Dictionary'
    runtime_features = component_info['runtime_enabled_features']

    return {
        'cpp_default_value': cpp_default_value,
        'cpp_type': unwrapped_idl_type.cpp_type,
        'cpp_value': cpp_value,
        'cpp_value_to_v8_value': unwrapped_idl_type.cpp_value_to_v8_value(
            cpp_value='impl->%s()' % getter_name, isolate='isolate',
            creation_context='creationContext',
            extended_attributes=extended_attributes),
        'deprecate_as': v8_utilities.deprecate_as(member),
        'enum_type': idl_type.enum_type,
        'enum_values': idl_type.enum_values,
        'getter_name': getter_name,
        'has_explicit_presence': has_explicit_presence,
        'has_method_name': has_method_name_for_dictionary_member(member),
        'idl_type': idl_type.base_type,
        'is_callback_function_type': idl_type.is_callback_function,
        'is_interface_type': idl_type.is_interface_type and not is_deprecated_dictionary,
        'is_nullable': idl_type.is_nullable,
        'is_object': unwrapped_idl_type.name == 'Object' or is_deprecated_dictionary,
        'is_string_type': idl_type.preprocessed_type.is_string_type,
        'is_required': member.is_required,
        'name': member.name,
        'origin_trial_feature_name':
            v8_utilities.origin_trial_feature_name(member, runtime_features),  # [RuntimeEnabled] for origin trial
        'runtime_enabled_feature_name':
            v8_utilities.runtime_enabled_feature_name(member, runtime_features),  # [RuntimeEnabled] if not in origin trial
        'setter_name': setter_name_for_dictionary_member(member),
        'has_value_or_default': has_value_or_default,
        'null_setter_name': null_setter_name_for_dictionary_member(member),
        'v8_default_value': v8_default_value,
        'v8_value': v8_value,
        'v8_value_to_local_cpp_value': idl_type.v8_value_to_local_cpp_value(
            extended_attributes, v8_value, cpp_value, isolate='isolate',
            use_exception_state=True),
    }


# Context for implementation classes

def dictionary_impl_context(dictionary, interfaces_info):
    def remove_duplicate_members(members):
        # When [ImplementedAs] is used, cpp_name can conflict. For example,
        # dictionary D { long foo; [ImplementedAs=foo, DeprecateAs=Foo] long oldFoo; };
        # This function removes such duplications, checking they have the same type.
        members_dict = {}
        for member in members:
            cpp_name = member['cpp_name']
            duplicated_member = members_dict.get(cpp_name)
            if duplicated_member and duplicated_member != member:
                raise Exception('Member name conflict: %s' % cpp_name)
            members_dict[cpp_name] = member
        return sorted(members_dict.values(), key=lambda member: member['cpp_name'])

    includes.clear()
    header_forward_decls = set()
    header_includes = set(['platform/heap/handle.h'])
    members = [member_impl_context(member, interfaces_info,
                                   header_includes, header_forward_decls)
               for member in dictionary.members]
    members = remove_duplicate_members(members)
    context = {
        'header_forward_decls': header_forward_decls,
        'header_includes': header_includes,
        'cpp_class': v8_utilities.cpp_name(dictionary),
        'members': members,
    }
    if dictionary.parent:
        context['parent_cpp_class'] = v8_utilities.cpp_name_from_interfaces_info(
            dictionary.parent, interfaces_info)
        parent_interface_info = interfaces_info.get(dictionary.parent)
        if parent_interface_info:
            context['header_includes'].add(
                parent_interface_info['include_path'])
    else:
        context['parent_cpp_class'] = 'IDLDictionaryBase'
        context['header_includes'].add(
            'bindings/core/v8/idl_dictionary_base.h')
    return context


def member_impl_context(member, interfaces_info, header_includes,
                        header_forward_decls):
    idl_type = unwrap_nullable_if_needed(member.idl_type)
    cpp_name = to_snake_case(v8_utilities.cpp_name(member))

    # In most cases, we don't have to distinguish `null` and `not present`,
    # and use null-states (e.g. nullptr, foo.IsUndefinedOrNull()) to show such
    # states for some types for memory usage and performance.
    # For types whose |has_explicit_presence| is True, we provide explicit
    # states of presence.
    has_explicit_presence = (
        member.idl_type.is_nullable and member.idl_type.inner_type.is_interface_type)

    nullable_indicator_name = None
    if not idl_type.cpp_type_has_null_value or has_explicit_presence:
        nullable_indicator_name = 'has_' + cpp_name + '_'

    def has_method_expression():
        if nullable_indicator_name:
            return nullable_indicator_name
        if idl_type.is_union_type or idl_type.is_enum or idl_type.is_string_type:
            return '!%s_.IsNull()' % cpp_name
        if idl_type.name == 'Any':
            return '!({0}_.IsEmpty() || {0}_.IsUndefined())'.format(cpp_name)
        if idl_type.name == 'Object':
            return '!({0}_.IsEmpty() || {0}_.IsNull() || {0}_.IsUndefined())'.format(cpp_name)
        if idl_type.name == 'Dictionary':
            return '!%s_.IsUndefinedOrNull()' % cpp_name
        return '%s_' % cpp_name

    cpp_default_value = None
    if member.default_value:
        if not member.default_value.is_null or has_explicit_presence:
            cpp_default_value = idl_type.literal_cpp_value(member.default_value)

    forward_decl_name = idl_type.impl_forward_declaration_name
    if forward_decl_name:
        includes.update(idl_type.impl_includes_for_type(interfaces_info))
        header_forward_decls.add(forward_decl_name)
    else:
        header_includes.update(idl_type.impl_includes_for_type(interfaces_info))

    setter_value = 'value'
    if idl_type.is_array_buffer_view_or_typed_array:
        setter_value += '.View()'

    non_null_type = idl_type.inner_type if idl_type.is_nullable else idl_type
    setter_inline = 'inline ' if (
        non_null_type.is_basic_type or
        non_null_type.is_enum or
        non_null_type.is_wrapper_type) else ''

    extended_attributes = member.extended_attributes

    return {
        'cpp_default_value': cpp_default_value,
        'cpp_name': cpp_name,
        'has_explicit_presence': has_explicit_presence,
        'getter_expression': cpp_name + '_',
        'getter_name': getter_name_for_dictionary_member(member),
        'has_method_expression': has_method_expression(),
        'has_method_name': has_method_name_for_dictionary_member(member),
        'is_nullable': idl_type.is_nullable,
        'is_traceable': idl_type.is_traceable,
        'member_cpp_type': idl_type.cpp_type_args(used_in_cpp_sequence=True, extended_attributes=extended_attributes),
        'null_setter_name': null_setter_name_for_dictionary_member(member),
        'nullable_indicator_name': nullable_indicator_name,
        'rvalue_cpp_type': idl_type.cpp_type_args(used_as_rvalue_type=True, extended_attributes=extended_attributes),
        'setter_inline': setter_inline,
        'setter_name': setter_name_for_dictionary_member(member),
        'setter_value': setter_value,
    }
