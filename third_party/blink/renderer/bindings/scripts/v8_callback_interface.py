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

"""Generate template values for a callback interface.

Extends IdlTypeBase with property |callback_cpp_type|.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

from idl_types import IdlTypeBase
from v8_globals import includes
from v8_interface import constant_context
import v8_types
import v8_utilities

CALLBACK_INTERFACE_H_INCLUDES = frozenset([
    'platform/bindings/callback_interface_base.h',
    'platform/bindings/v8_value_or_script_wrappable_adapter.h',
])
CALLBACK_INTERFACE_CPP_INCLUDES = frozenset([
    'bindings/core/v8/generated_code_helper.h',
    'bindings/core/v8/v8_binding_for_core.h',
    'core/execution_context/execution_context.h',
    'platform/bindings/exception_messages.h',
    'platform/bindings/script_forbidden_scope.h',
])
LEGACY_CALLBACK_INTERFACE_H_INCLUDES = frozenset([
    'platform/bindings/dom_wrapper_world.h',
    'platform/bindings/wrapper_type_info.h',
])
LEGACY_CALLBACK_INTERFACE_CPP_INCLUDES = frozenset([
    'bindings/core/v8/v8_dom_configuration.h',
])


def _cpp_type(idl_type):
    # FIXME: remove this function by making callback types consistent
    # (always use usual v8_types.cpp_type)
    if idl_type.is_string_type or idl_type.is_enum:
        return 'const String&'
    if idl_type.name == 'void':
        return 'void'
    # Callbacks use raw pointers, so raw_type=True
    raw_cpp_type = idl_type.cpp_type_args(raw_type=True)
    # Pass containers and dictionaries to callback method by const reference rather than by value
    if raw_cpp_type.startswith(('Vector', 'HeapVector')) or idl_type.is_dictionary:
        return 'const %s&' % raw_cpp_type
    return raw_cpp_type

IdlTypeBase.callback_cpp_type = property(_cpp_type)


def callback_interface_context(callback_interface, _, component_info):
    is_legacy_callback_interface = len(callback_interface.constants) > 0

    includes.clear()
    includes.update(CALLBACK_INTERFACE_CPP_INCLUDES)
    if is_legacy_callback_interface:
        includes.update(LEGACY_CALLBACK_INTERFACE_CPP_INCLUDES)

    header_includes = set(CALLBACK_INTERFACE_H_INCLUDES)
    if is_legacy_callback_interface:
        header_includes.update(LEGACY_CALLBACK_INTERFACE_H_INCLUDES)

    # https://heycam.github.io/webidl/#dfn-single-operation-callback-interface
    is_single_operation = True
    if (callback_interface.parent or
            len(callback_interface.attributes) > 0 or
            len(callback_interface.operations) == 0):
        is_single_operation = False
    else:
        operations = callback_interface.operations
        basis = operations[0]
        for op in operations[1:]:
            if op.name != basis.name:
                is_single_operation = False
                break

    return {
        'constants': [constant_context(constant, callback_interface, component_info)
                      for constant in callback_interface.constants],
        'cpp_class': callback_interface.name,
        'do_not_check_constants': 'DoNotCheckConstants' in callback_interface.extended_attributes,
        'forward_declarations': sorted(forward_declarations(callback_interface)),
        'header_includes': header_includes,
        'interface_name': callback_interface.name,
        'is_legacy_callback_interface': is_legacy_callback_interface,
        'is_single_operation_callback_interface': is_single_operation,
        'methods': [method_context(operation)
                    for operation in callback_interface.operations],
        'v8_class': v8_utilities.v8_class_name(callback_interface),
    }


def forward_declarations(callback_interface):
    def find_forward_declaration(idl_type):
        if idl_type.is_interface_type or idl_type.is_dictionary:
            return idl_type.implemented_as
        elif idl_type.is_array_or_sequence_type:
            return find_forward_declaration(idl_type.element_type)
        elif idl_type.is_nullable:
            return find_forward_declaration(idl_type.inner_type)
        return None

    declarations = set()
    for operation in callback_interface.operations:
        for argument in operation.arguments:
            name = find_forward_declaration(argument.idl_type)
            if name:
                declarations.add(name)
    return declarations


def add_includes_for_operation(operation):
    operation.idl_type.add_includes_for_type()
    for argument in operation.arguments:
        argument.idl_type.add_includes_for_type()


def method_context(operation):
    extended_attributes = operation.extended_attributes
    idl_type = operation.idl_type
    idl_type_str = str(idl_type)

    add_includes_for_operation(operation)
    context = {
        'cpp_type': idl_type.callback_cpp_type,
        'idl_type': idl_type_str,
        'name': operation.name,
        'native_value_traits_tag': v8_types.idl_type_to_native_value_traits_tag(idl_type),
    }
    context.update(arguments_context(operation.arguments))
    return context


def arguments_context(arguments):
    def argument_context(argument):
        return {
            'cpp_value_to_v8_value': argument.idl_type.cpp_value_to_v8_value(
                argument.name, isolate='GetIsolate()',
                creation_context='argument_creation_context'),
            'name': argument.name,
            'v8_name': 'v8_' + argument.name,
        }

    argument_declarations = ['bindings::V8ValueOrScriptWrappableAdapter callback_this_value']
    argument_declarations.extend(
        '%s %s' % (argument.idl_type.callback_cpp_type, argument.name)
        for argument in arguments)
    return  {
        'argument_declarations': argument_declarations,
        'arguments': [argument_context(argument) for argument in arguments],
    }
