# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate template values for a callback function.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

from v8_globals import includes
import v8_types

CALLBACK_FUNCTION_H_INCLUDES = frozenset([
    'platform/bindings/callback_function_base.h',
    'platform/bindings/v8_value_or_script_wrappable_adapter.h',
    'platform/wtf/forward.h',
])
CALLBACK_FUNCTION_CPP_INCLUDES = frozenset([
    'base/stl_util.h',
    'bindings/core/v8/generated_code_helper.h',
    'bindings/core/v8/native_value_traits_impl.h',
    'bindings/core/v8/to_v8_for_core.h',
    'bindings/core/v8/v8_binding_for_core.h',
    'core/execution_context/execution_context.h',
    'platform/bindings/exception_messages.h',
    'platform/bindings/exception_state.h',
    'platform/bindings/script_forbidden_scope.h',
])


def callback_function_context(callback_function):
    includes.clear()
    includes.update(CALLBACK_FUNCTION_CPP_INCLUDES)
    idl_type = callback_function.idl_type
    idl_type_str = str(idl_type)

    for argument in callback_function.arguments:
        argument.idl_type.add_includes_for_type(
            callback_function.extended_attributes)

    context = {
        # While both |callback_function_name| and |cpp_class| are identical at
        # the moment, the two are being defined because their values may change
        # in the future (e.g. if we support [ImplementedAs=] in callback
        # functions).
        'callback_function_name': callback_function.name,
        'cpp_class': 'V8%s' % callback_function.name,
        'cpp_includes': sorted(includes),
        'forward_declarations': sorted(forward_declarations(callback_function)),
        'header_includes': sorted(CALLBACK_FUNCTION_H_INCLUDES),
        'idl_type': idl_type_str,
        'is_treat_non_object_as_null': 'TreatNonObjectAsNull' in callback_function.extended_attributes,
        'native_value_traits_tag': v8_types.idl_type_to_native_value_traits_tag(idl_type),
        'return_cpp_type': idl_type.cpp_type,
    }

    context.update(arguments_context(callback_function.arguments))
    return context


def forward_declarations(callback_function):
    def find_forward_declaration(idl_type):
        if idl_type.is_interface_type or idl_type.is_dictionary:
            return idl_type.implemented_as
        elif idl_type.is_array_or_sequence_type:
            return find_forward_declaration(idl_type.element_type)
        return None

    declarations = set()
    for argument in callback_function.arguments:
        name = find_forward_declaration(argument.idl_type)
        if name:
            declarations.add(name)
    return declarations


def arguments_context(arguments):
    def argument_context(argument):
        idl_type = argument.idl_type
        return {
            'cpp_value_to_v8_value': idl_type.cpp_value_to_v8_value(
                argument.name, isolate='GetIsolate()',
                creation_context='argument_creation_context'),
            'enum_type': idl_type.enum_type,
            'enum_values': idl_type.enum_values,
            'is_variadic': argument.is_variadic,
            'name': argument.name,
            'v8_name': 'v8_%s' % argument.name,
        }

    def argument_cpp_type(argument):
        cpp_type = argument.idl_type.callback_cpp_type
        if argument.is_variadic:
            if argument.idl_type.is_traceable:
                return 'const HeapVector<%s>&' % cpp_type
            else:
                return 'const Vector<%s>&' % cpp_type
        else:
            return cpp_type

    argument_declarations = ['bindings::V8ValueOrScriptWrappableAdapter callback_this_value']
    argument_declarations.extend(
        '%s %s' % (argument_cpp_type(argument), argument.name)
        for argument in arguments)
    return {
        'argument_declarations': argument_declarations,
        'arguments': [argument_context(argument) for argument in arguments],
    }
