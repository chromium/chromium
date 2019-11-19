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

"""Generate template values for methods.

Extends IdlArgument with property |default_cpp_value|.
Extends IdlTypeBase and IdlUnionType with property |union_arguments|.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__),
                             '..', '..', 'build', 'scripts'))
from blinkbuild.name_style_converter import NameStyleConverter
from idl_definitions import IdlArgument, IdlOperation
from idl_types import IdlTypeBase, IdlUnionType, inherits_interface
from v8_globals import includes
import v8_types
import v8_utilities
from v8_utilities import (has_extended_attribute_value, is_unforgeable)


def method_is_visible(method, interface_is_partial):
    if 'overloads' in method:
        return method['overloads']['visible'] and not (method['overloads']['has_partial_overloads'] and interface_is_partial)
    return method['visible'] and 'overload_index' not in method


def is_origin_trial_enabled(method):
    return bool(method['origin_trial_feature_name'])


def is_secure_context(method):
    return bool(method['overloads']['secure_context_test_all'] if 'overloads' in method else method['secure_context_test'])


def is_conditionally_enabled(method):
    exposed = method['overloads']['exposed_test_all'] if 'overloads' in method else method['exposed_test']
    return exposed or is_secure_context(method)


def filter_conditionally_enabled(methods, interface_is_partial):
    return [method for method in methods if (
        method_is_visible(method, interface_is_partial) and
        is_conditionally_enabled(method) and
        not is_origin_trial_enabled(method))]


def custom_registration(method):
    # TODO(dcheng): Currently, bindings must create a function object for each
    # realm as a hack to support the incumbent realm. Remove the need for custom
    # registration when Blink properly supports the incumbent realm.
    if method['is_cross_origin']:
        return True
    if 'overloads' in method:
        return (method['overloads']['runtime_determined_lengths'] or
                (method['overloads']['runtime_enabled_all'] and not is_conditionally_enabled(method)))
    return method['runtime_enabled_feature_name'] and not is_conditionally_enabled(method)


def filter_custom_registration(methods, interface_is_partial):
    return [method for method in methods if (
        method_is_visible(method, interface_is_partial) and custom_registration(method))]


def filter_method_configuration(methods, interface_is_partial):
    return [method for method in methods if
            method_is_visible(method, interface_is_partial) and
            not is_origin_trial_enabled(method) and
            not is_conditionally_enabled(method) and
            not custom_registration(method)]


def method_filters():
    return {'custom_registration': filter_custom_registration,
            'has_method_configuration': filter_method_configuration}


def use_local_result(method):
    extended_attributes = method.extended_attributes
    idl_type = method.idl_type
    return (has_extended_attribute_value(method, 'CallWith', 'ScriptState') or
            'NewObject' in extended_attributes or
            'RaisesException' in extended_attributes or
            idl_type.is_union_type or
            idl_type.is_dictionary or
            idl_type.is_explicit_nullable)


def runtime_call_stats_context(interface, method):
    includes.add('platform/bindings/runtime_call_stats.h')
    generic_counter_name = 'Blink_' + v8_utilities.cpp_name(interface) + '_' + method.name
    (method_counter, extended_attribute_defined) = v8_utilities.rcs_counter_name(method, generic_counter_name)
    return {
        'extended_attribute_defined': extended_attribute_defined,
        'method_counter': method_counter,
        'origin_safe_method_getter_counter': generic_counter_name + '_OriginSafeMethodGetter'
    }


def method_context(interface, method, component_info, is_visible=True):
    arguments = method.arguments
    extended_attributes = method.extended_attributes
    idl_type = method.idl_type
    is_static = method.is_static
    name = method.name

    if is_visible:
        idl_type.add_includes_for_type(extended_attributes)

    this_cpp_value = cpp_value(interface, method, len(arguments))

    is_call_with_script_state = has_extended_attribute_value(method, 'CallWith', 'ScriptState')
    is_call_with_this_value = has_extended_attribute_value(method, 'CallWith', 'ThisValue')
    if is_call_with_script_state or is_call_with_this_value:
        includes.add('platform/bindings/script_state.h')

    # [CheckSecurity]
    is_cross_origin = 'CrossOrigin' in extended_attributes
    is_check_security_for_receiver = (
        has_extended_attribute_value(interface, 'CheckSecurity', 'Receiver') and
        not is_cross_origin)
    is_check_security_for_return_value = (
        has_extended_attribute_value(method, 'CheckSecurity', 'ReturnValue'))
    if is_check_security_for_receiver or is_check_security_for_return_value:
        includes.add('bindings/core/v8/binding_security.h')
    if is_check_security_for_return_value:
        includes.add('core/frame/web_feature.h')
        includes.add('platform/instrumentation/use_counter.h')

    is_ce_reactions = 'CEReactions' in extended_attributes
    if is_ce_reactions:
        includes.add('core/html/custom/ce_reactions_scope.h')
    is_custom_element_callbacks = 'CustomElementCallbacks' in extended_attributes
    if is_custom_element_callbacks:
        includes.add('core/html/custom/v0_custom_element_processing_stack.h')

    is_raises_exception = 'RaisesException' in extended_attributes
    is_custom_call_prologue = has_extended_attribute_value(method, 'Custom', 'CallPrologue')
    is_custom_call_epilogue = has_extended_attribute_value(method, 'Custom', 'CallEpilogue')

    if 'LenientThis' in extended_attributes:
        raise Exception('[LenientThis] is not supported for operations.')

    if has_extended_attribute_value(method, 'Affects', 'Nothing'):
        side_effect_type = 'V8DOMConfiguration::kHasNoSideEffect'
    else:
        side_effect_type = 'V8DOMConfiguration::kHasSideEffect'

    # [LogActivity]
    if 'LogActivity' in extended_attributes:
        includes.add('platform/bindings/v8_per_context_data.h')

    argument_contexts = [
        argument_context(interface, method, argument, index, is_visible=is_visible)
        for index, argument in enumerate(arguments)]

    runtime_features = component_info['runtime_enabled_features']

    return {
        'activity_logging_world_list': v8_utilities.activity_logging_world_list(method),  # [ActivityLogging]
        'arguments': argument_contexts,
        'camel_case_name': NameStyleConverter(name).to_upper_camel_case(),
        'cpp_type': (v8_types.cpp_template_type('base::Optional', idl_type.cpp_type)
                     if idl_type.is_explicit_nullable
                     else v8_types.cpp_type(idl_type, extended_attributes=extended_attributes)),
        'cpp_value': this_cpp_value,
        'cpp_type_initializer': idl_type.cpp_type_initializer,
        'high_entropy': v8_utilities.high_entropy(method),  # [HighEntropy]
        'deprecate_as': v8_utilities.deprecate_as(method),  # [DeprecateAs]
        'do_not_test_new_object': 'DoNotTestNewObject' in extended_attributes,
        'exposed_test': v8_utilities.exposed(method, interface),  # [Exposed]
        'has_exception_state':
            is_raises_exception or
            is_check_security_for_receiver or
            any(argument for argument in arguments
                if (argument.idl_type.name == 'SerializedScriptValue' or
                    argument_conversion_needs_exception_state(method, argument))),
        'has_optional_argument_without_default_value':
            any(True for argument_context in argument_contexts
                if argument_context['is_optional_without_default_value']),
        'idl_type': idl_type.base_type,
        'is_call_with_execution_context': has_extended_attribute_value(method, 'CallWith', 'ExecutionContext'),
        'is_call_with_script_state': is_call_with_script_state,
        'is_call_with_this_value': is_call_with_this_value,
        'is_ce_reactions': is_ce_reactions,
        'is_check_security_for_receiver': is_check_security_for_receiver,
        'is_check_security_for_return_value': is_check_security_for_return_value,
        'is_cross_origin': 'CrossOrigin' in extended_attributes,
        'is_custom': 'Custom' in extended_attributes and
            not (is_custom_call_prologue or is_custom_call_epilogue),
        'is_custom_call_prologue': is_custom_call_prologue,
        'is_custom_call_epilogue': is_custom_call_epilogue,
        'is_custom_element_callbacks': is_custom_element_callbacks,
        'is_explicit_nullable': idl_type.is_explicit_nullable,
        'is_new_object': 'NewObject' in extended_attributes,
        'is_partial_interface_member':
            'PartialInterfaceImplementedAs' in extended_attributes,
        'is_per_world_bindings': 'PerWorldBindings' in extended_attributes,
        'is_raises_exception': is_raises_exception,
        'is_static': is_static,
        'is_unforgeable': is_unforgeable(method),
        'is_variadic': arguments and arguments[-1].is_variadic,
        'measure_as': v8_utilities.measure_as(method, interface),  # [MeasureAs]
        'name': name,
        'number_of_arguments': len(arguments),
        'number_of_required_arguments': len([
            argument for argument in arguments
            if not (argument.is_optional or argument.is_variadic)]),
        'number_of_required_or_variadic_arguments': len([
            argument for argument in arguments
            if not argument.is_optional]),
        'on_instance': v8_utilities.on_instance(interface, method),
        'on_interface': v8_utilities.on_interface(interface, method),
        'on_prototype': v8_utilities.on_prototype(interface, method),
        'origin_trial_feature_name':
            v8_utilities.origin_trial_feature_name(method, runtime_features),  # [RuntimeEnabled] for origin trial
        'property_attributes': property_attributes(interface, method),
        'returns_promise': method.returns_promise,
        'runtime_call_stats': runtime_call_stats_context(interface, method),
        'runtime_enabled_feature_name':
            v8_utilities.runtime_enabled_feature_name(method, runtime_features),  # [RuntimeEnabled] if not in origin trial
        'secure_context_test': v8_utilities.secure_context(method, interface),  # [SecureContext]
        'side_effect_type': side_effect_type,  # [Affects]
        'snake_case_name': NameStyleConverter(name).to_snake_case(),
        'use_output_parameter_for_result': idl_type.use_output_parameter_for_result,
        'use_local_result': use_local_result(method),
        'v8_set_return_value': v8_set_return_value(interface.name, method, this_cpp_value),
        'v8_set_return_value_for_main_world': v8_set_return_value(interface.name, method, this_cpp_value, for_main_world=True),
        'visible': is_visible,
        'world_suffixes': ['', 'ForMainWorld'] if 'PerWorldBindings' in extended_attributes else [''],  # [PerWorldBindings],
    }


def argument_context(interface, method, argument, index, is_visible=True):
    extended_attributes = argument.extended_attributes
    idl_type = argument.idl_type
    if is_visible:
        idl_type.add_includes_for_type(extended_attributes)
    this_cpp_value = cpp_value(interface, method, index)
    is_variadic_wrapper_type = argument.is_variadic and idl_type.is_wrapper_type

    has_type_checking_interface = idl_type.is_wrapper_type

    set_default_value = argument.set_default_value
    this_cpp_type = idl_type.cpp_type_args(extended_attributes=extended_attributes,
                                           raw_type=True,
                                           used_as_variadic_argument=argument.is_variadic)
    snake_case_name = NameStyleConverter(argument.name).to_snake_case()
    context = {
        'cpp_type': (
            v8_types.cpp_template_type('base::Optional', this_cpp_type)
            if idl_type.is_explicit_nullable and not argument.is_variadic
            else this_cpp_type),
        'cpp_value': this_cpp_value,
        # FIXME: check that the default value's type is compatible with the argument's
        'enum_type': idl_type.enum_type,
        'enum_values': idl_type.enum_values,
        'handle': '%s_handle' % snake_case_name,
        # TODO(peria): remove once [DefaultValue] removed and just use
        # argument.default_value. https://crbug.com/924419
        'has_default': 'DefaultValue' in extended_attributes or set_default_value,
        'has_type_checking_interface': has_type_checking_interface,
        # Dictionary is special-cased, but arrays and sequences shouldn't be
        'idl_type': idl_type.base_type,
        'idl_type_object': idl_type,
        'index': index,
        'is_callback_function': idl_type.is_callback_function,
        'is_callback_interface': idl_type.is_callback_interface,
        # FIXME: Remove generic 'Dictionary' special-casing
        'is_dictionary': idl_type.is_dictionary or idl_type.base_type == 'Dictionary',
        'is_explicit_nullable': idl_type.is_explicit_nullable,
        'is_nullable': idl_type.is_nullable,
        'is_optional': argument.is_optional,
        'is_variadic': argument.is_variadic,
        'is_variadic_wrapper_type': is_variadic_wrapper_type,
        'is_wrapper_type': idl_type.is_wrapper_type,
        'local_cpp_variable': snake_case_name,
        'name': argument.name,
        'set_default_value': set_default_value,
        'use_permissive_dictionary_conversion': 'PermissiveDictionaryConversion' in extended_attributes,
        'v8_set_return_value': v8_set_return_value(interface.name, method, this_cpp_value),
        'v8_set_return_value_for_main_world': v8_set_return_value(interface.name, method, this_cpp_value, for_main_world=True),
        'v8_value_to_local_cpp_value': v8_value_to_local_cpp_value(interface.name, method, argument, index),
    }
    context.update({
        'is_optional_without_default_value':
            context['is_optional'] and
            not context['has_default'] and
            not context['is_dictionary'] and
            not context['is_callback_interface'],
    })
    return context


################################################################################
# Value handling
################################################################################

def cpp_value(interface, method, number_of_arguments):
    # Truncate omitted optional arguments
    arguments = method.arguments[:number_of_arguments]
    cpp_arguments = []

    if method.is_constructor:
        call_with_values = interface.extended_attributes.get('ConstructorCallWith')
    else:
        call_with_values = method.extended_attributes.get('CallWith')
    cpp_arguments.extend(v8_utilities.call_with_arguments(call_with_values))

    # Members of IDL partial interface definitions are implemented in C++ as
    # static member functions, which for instance members (non-static members)
    # take *impl as their first argument
    if ('PartialInterfaceImplementedAs' in method.extended_attributes and
            not method.is_static):
        cpp_arguments.append('*impl')
    for argument in arguments:
        variable_name = NameStyleConverter(argument.name).to_snake_case()
        if argument.idl_type.base_type == 'SerializedScriptValue':
            cpp_arguments.append('std::move(%s)' % variable_name)
        else:
            cpp_arguments.append(variable_name)

    if ('RaisesException' in method.extended_attributes or
          (method.is_constructor and
           has_extended_attribute_value(interface, 'RaisesException', 'Constructor'))):
        cpp_arguments.append('exception_state')

    # If a method returns an IDL dictionary or union type, the return value is
    # passed as an argument to impl classes.
    idl_type = method.idl_type
    if idl_type and idl_type.use_output_parameter_for_result:
        cpp_arguments.append('result')

    if method.name == 'Constructor':
        base_name = 'Create'
    elif method.name == 'NamedConstructor':
        base_name = 'CreateForJSConstructor'
    else:
        base_name = v8_utilities.cpp_name(method)

    cpp_method_name = v8_utilities.scoped_name(interface, method, base_name)
    return '%s(%s)' % (cpp_method_name, ', '.join(cpp_arguments))


def v8_set_return_value(interface_name, method, cpp_value, for_main_world=False):
    idl_type = method.idl_type
    extended_attributes = method.extended_attributes
    if not idl_type or idl_type.name == 'void':
        # Constructors and void methods don't have a return type
        return None

    # [CallWith=ScriptState], [RaisesException]
    if use_local_result(method):
        if idl_type.is_explicit_nullable:
            # result is of type base::Optional<T>
            cpp_value = 'result.value()'
        else:
            cpp_value = 'result'

    script_wrappable = 'impl' if inherits_interface(interface_name, 'Node') else ''
    return idl_type.v8_set_return_value(cpp_value, extended_attributes, script_wrappable=script_wrappable, for_main_world=for_main_world, is_static=method.is_static)


def v8_value_to_local_cpp_variadic_value(argument, index):
    assert argument.is_variadic
    idl_type = v8_types.native_value_traits_type_name(argument.idl_type,
                                                      argument.extended_attributes, True)

    return {
        'assign_expression': 'ToImplArguments<%s>(info, %s, exception_state)' % (idl_type, index),
        'check_expression': 'exception_state.HadException()',
        'cpp_name': NameStyleConverter(argument.name).to_snake_case(),
        'declare_variable': False,
    }


def v8_value_to_local_cpp_ssv_value(extended_attributes, idl_type, v8_value, variable_name, for_storage):
    this_cpp_type = idl_type.cpp_type_args(extended_attributes=extended_attributes, raw_type=True)

    storage_policy = 'kForStorage' if for_storage else 'kNotForStorage'
    arguments = ', '.join([
        'info.GetIsolate()',
        v8_value,
        '{ssv}::SerializeOptions({ssv}::{storage_policy})',
        'exception_state'
    ])
    cpp_expression_format = 'NativeValueTraits<{ssv}>::NativeValue(%s)' % arguments
    this_cpp_value = cpp_expression_format.format(
        ssv='SerializedScriptValue',
        storage_policy=storage_policy
    )

    return {
        'assign_expression': this_cpp_value,
        'check_expression': 'exception_state.HadException()',
        'cpp_type': this_cpp_type,
        'cpp_name': variable_name,
        'declare_variable': False,
    }


def v8_value_to_local_cpp_value(interface_name, method, argument, index):
    extended_attributes = argument.extended_attributes
    idl_type = argument.idl_type
    name = NameStyleConverter(argument.name).to_snake_case()
    v8_value = 'info[{index}]'.format(index=index)

    # History.pushState and History.replaceState are explicitly specified as
    # serializing the value for storage. The default is to not serialize for
    # storage. See https://html.spec.whatwg.org/C/#dom-history-pushstate
    if idl_type.name == 'SerializedScriptValue':
        for_storage = (interface_name == 'History' and
                       method.name in ('pushState', 'replaceState'))
        return v8_value_to_local_cpp_ssv_value(extended_attributes, idl_type,
                                               v8_value, name,
                                               for_storage=for_storage)

    if argument.is_variadic:
        return v8_value_to_local_cpp_variadic_value(argument, index)
    return idl_type.v8_value_to_local_cpp_value(extended_attributes, v8_value,
                                                name, declare_variable=False,
                                                use_exception_state=method.returns_promise)


################################################################################
# Auxiliary functions
################################################################################

# [NotEnumerable], [Unforgeable]
def property_attributes(interface, method):
    extended_attributes = method.extended_attributes
    property_attributes_list = []
    if 'NotEnumerable' in extended_attributes:
        property_attributes_list.append('v8::DontEnum')
    if is_unforgeable(method):
        property_attributes_list.append('v8::ReadOnly')
        property_attributes_list.append('v8::DontDelete')
    return property_attributes_list


def argument_set_default_value(argument):
    idl_type = argument.idl_type
    default_value = argument.default_value
    variable_name = NameStyleConverter(argument.name).to_snake_case()
    if not default_value:
        return None
    if idl_type.is_dictionary:
        if argument.default_value.is_null:
            return None
        if argument.default_value.value == '{}':
            return None
        raise Exception('invalid default value for dictionary type')
    if idl_type.is_array_or_sequence_type:
        if default_value.value != '[]':
            raise Exception('invalid default value for sequence type: %s' % default_value.value)
        # Nothing to do when we set an empty sequence as default value, but we
        # need to return non-empty value so that we don't generate method calls
        # without this argument.
        return '/* Nothing to do */'
    if idl_type.is_union_type:
        if argument.default_value.is_null:
            if not idl_type.includes_nullable_type:
                raise Exception('invalid default value for union type: null for %s'
                                % idl_type.name)
            # Union container objects are "null" initially.
            return '/* null default value */'
        if isinstance(default_value.value, basestring):
            member_type = idl_type.string_member_type
        elif isinstance(default_value.value, (int, float)):
            member_type = idl_type.numeric_member_type
        elif isinstance(default_value.value, bool):
            member_type = idl_type.boolean_member_type
        else:
            member_type = None
        if member_type is None:
            raise Exception('invalid default value for union type: %r for %s'
                            % (default_value.value, idl_type.name))
        member_type_name = (member_type.inner_type.name
                            if member_type.is_nullable else
                            member_type.name)
        return '%s.Set%s(%s)' % (variable_name, member_type_name,
                                 member_type.literal_cpp_value(default_value))
    return '%s = %s' % (variable_name,
                        idl_type.literal_cpp_value(default_value))

IdlArgument.set_default_value = property(argument_set_default_value)


def method_returns_promise(method):
    return method.idl_type and method.idl_type.name == 'Promise'

IdlOperation.returns_promise = property(method_returns_promise)


def argument_conversion_needs_exception_state(method, argument):
    idl_type = argument.idl_type
    return (idl_type.v8_conversion_needs_exception_state or
            argument.is_variadic or
            (method.returns_promise and idl_type.is_string_type))
