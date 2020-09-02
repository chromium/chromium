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
"""Generate template values for attributes.

Extends IdlType with property |constructor_type_name|.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import os
import sys

sys.path.append(
    os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'scripts'))
from blinkbuild.name_style_converter import NameStyleConverter
import idl_types
from idl_types import inherits_interface
from v8_globals import includes
import v8_types
import v8_utilities
from v8_utilities import capitalize
from v8_utilities import cpp_encoded_property_name
from v8_utilities import cpp_name
from v8_utilities import extended_attribute_value_as_list
from v8_utilities import has_extended_attribute_value
from v8_utilities import is_unforgeable
from v8_utilities import scoped_name
from v8_utilities import strip_suffix
from v8_utilities import uncapitalize
from blinkbuild.name_style_converter import NameStyleConverter


def attribute_context(interface, attribute, interfaces, component_info):
    """Creates a Jinja template context for an attribute of an interface.

    Args:
        interface: An interface which |attribute| belongs to
        attribute: An attribute to create the context for
        interfaces: A dict which maps an interface name to the definition
            which can be referred if needed
        component_info: A dict containing component wide information

    Returns:
        A Jinja template context for |attribute|
    """

    idl_type = attribute.idl_type
    base_idl_type = idl_type.base_type
    extended_attributes = attribute.extended_attributes

    idl_type.add_includes_for_type(extended_attributes)
    if idl_type.enum_values:
        includes.add('core/inspector/console_message.h')
        includes.add('platform/heap/heap.h')

    # [CheckSecurity]
    is_cross_origin = 'CrossOrigin' in extended_attributes
    is_check_security_for_receiver = (has_extended_attribute_value(
        interface, 'CheckSecurity', 'Receiver') and is_cross_origin)
    is_check_security_for_return_value = (has_extended_attribute_value(
        attribute, 'CheckSecurity', 'ReturnValue'))
    if is_check_security_for_receiver or is_check_security_for_return_value:
        includes.add('bindings/core/v8/binding_security.h')
    if is_check_security_for_return_value:
        includes.add('core/frame/web_feature.h')
        includes.add('platform/instrumentation/use_counter.h')
    # [CrossOrigin]
    if has_extended_attribute_value(attribute, 'CrossOrigin', 'Setter'):
        includes.add('platform/bindings/v8_cross_origin_callback_info.h')
    # [Constructor]
    # TODO(yukishiino): Constructors are much like methods although constructors
    # are not methods.  Constructors must be data-type properties, and we can
    # support them as a kind of methods.
    constructor_type = idl_type.constructor_type_name if is_constructor_attribute(
        attribute) else None
    # [CEReactions]
    is_ce_reactions = 'CEReactions' in extended_attributes
    if is_ce_reactions:
        includes.add('core/html/custom/ce_reactions_scope.h')
    # [CustomElementCallbacks], [Reflect]
    is_custom_element_callbacks = 'CustomElementCallbacks' in extended_attributes
    is_reflect = 'Reflect' in extended_attributes
    # [ReflectOnly]
    reflect_only = extended_attribute_value_as_list(attribute, 'ReflectOnly')
    if reflect_only:
        reflect_only = map(
            lambda v: cpp_content_attribute_value_name(interface, v),
            reflect_only)
    if is_custom_element_callbacks or is_reflect:
        includes.add('core/html/custom/v0_custom_element_processing_stack.h')
    # [PerWorldBindings]
    if 'PerWorldBindings' in extended_attributes:
        assert idl_type.is_wrapper_type or 'LogActivity' in \
            extended_attributes, \
            '[PerWorldBindings] should only be used with wrapper types: %s.%s' % \
            (interface.name, attribute.name)
    # [SaveSameObject]
    is_save_same_object = ('SameObject' in attribute.extended_attributes and
                           'SaveSameObject' in attribute.extended_attributes)

    # [StringContext]
    if idl_type.has_string_context:
        includes.add('bindings/core/v8/generated_code_helper.h')

    # [CachedAccessor]
    is_cached_accessor = 'CachedAccessor' in extended_attributes

    # [LegacyLenientSetter]
    is_lenient_setter = 'LegacyLenientSetter' in extended_attributes

    # [CachedAttribute]
    cached_attribute_validation_method = extended_attributes.get(
        'CachedAttribute')

    keep_alive_for_gc = is_keep_alive_for_gc(interface, attribute)

    does_generate_getter = (not has_custom_getter(attribute)
                            and not constructor_type)
    does_generate_setter = (
        has_setter(interface, attribute)
        and not (has_custom_setter(attribute) or is_lenient_setter))

    use_private_property_in_getter = (does_generate_getter
                                      and (cached_attribute_validation_method
                                           or is_save_same_object
                                           or keep_alive_for_gc))
    use_private_property_in_setter = (does_generate_setter
                                      and cached_attribute_validation_method)
    private_property_is_shared_between_getter_and_setter = (
        use_private_property_in_getter and use_private_property_in_setter)

    does_use_private_property = (use_private_property_in_getter
                                 or use_private_property_in_setter
                                 or is_cached_accessor)
    if does_use_private_property:
        includes.add('platform/bindings/v8_private_property.h')

    # [LogActivity]
    if 'LogActivity' in extended_attributes:
        includes.add('platform/bindings/v8_per_context_data.h')

    # [DeprecateAs], [MeasureAs]
    deprecate_as = v8_utilities.deprecate_as(attribute)
    measure_as = v8_utilities.measure_as(attribute, interface)

    # [HighEntropy]
    high_entropy = v8_utilities.high_entropy(attribute)

    is_lazy_data_attribute = \
        (constructor_type and not (measure_as or deprecate_as)) or \
        (str(idl_type) == 'Window' and attribute.name in ('frames', 'self', 'window'))

    runtime_features = component_info['runtime_enabled_features']

    internal_name = cpp_encoded_property_name(attribute)

    cpp_type = idl_type.cpp_type
    if idl_type.is_explicit_nullable:
        cpp_type = v8_types.cpp_template_type('base::Optional', cpp_type)

    context = {
        # [ActivityLogging]
        'activity_logging_world_list_for_getter':
        v8_utilities.activity_logging_world_list(attribute, 'Getter'),
        # [ActivityLogging]
        'activity_logging_world_list_for_setter':
        v8_utilities.activity_logging_world_list(attribute, 'Setter'),
        # [ActivityLogging]
        'activity_logging_world_check':
        v8_utilities.activity_logging_world_check(attribute),
        'cached_accessor_name':
        'k%s%s' % (interface.name, attribute.name.capitalize()),
        'cached_attribute_validation_method':
        cached_attribute_validation_method,
        'camel_case_name':
        NameStyleConverter(internal_name).to_upper_camel_case(),
        'constructor_type':
        constructor_type,
        'context_enabled_feature_name':
        v8_utilities.context_enabled_feature_name(attribute),
        'cpp_name': cpp_name(attribute),
        'cpp_type': cpp_type,
        'cpp_type_initializer': idl_type.cpp_type_initializer,
        'deprecate_as': deprecate_as,
        'does_generate_getter': does_generate_getter,
        'does_generate_setter': does_generate_setter,
        'enum_type': idl_type.enum_type,
        'enum_values': idl_type.enum_values,
        # [Exposed]
        'exposed_test':
        v8_utilities.exposed(attribute, interface),
        'getter_has_no_side_effect':
        has_extended_attribute_value(attribute, 'Affects', 'Nothing'),
        'has_cross_origin_getter':
            has_extended_attribute_value(attribute, 'CrossOrigin', None) or
            has_extended_attribute_value(attribute, 'CrossOrigin', 'Getter'),
        'has_cross_origin_setter':
        has_extended_attribute_value(attribute, 'CrossOrigin', 'Setter'),
        'has_custom_getter': has_custom_getter(attribute),
        'has_custom_setter': has_custom_setter(attribute),
        'has_promise_type': idl_type.name == 'Promise',
        'has_setter': has_setter(interface, attribute),
        'high_entropy': high_entropy,
        'idl_type': str(idl_type),
        'is_cached_accessor': is_cached_accessor,
        'is_call_with_execution_context':
        has_extended_attribute_value(attribute, 'CallWith', 'ExecutionContext'),
        'is_call_with_script_state':
        has_extended_attribute_value(attribute, 'CallWith', 'ScriptState'),
        'is_ce_reactions': is_ce_reactions,
        'is_check_security_for_receiver': is_check_security_for_receiver,
        'is_check_security_for_return_value':
        is_check_security_for_return_value,
        'is_custom_element_callbacks': is_custom_element_callbacks,
        # TODO(yukishiino): Make all DOM attributes accessor-type properties.
        'is_data_type_property': is_data_type_property(interface, attribute),
        'is_getter_raises_exception':  # [RaisesException]
            'RaisesException' in extended_attributes and
            extended_attributes['RaisesException'] in (None, 'Getter'),
        'is_keep_alive_for_gc': keep_alive_for_gc,
        'is_lazy_data_attribute': is_lazy_data_attribute,
        'is_lenient_setter': is_lenient_setter,
        'is_lenient_this': 'LegacyLenientThis' in extended_attributes,
        'is_nullable': idl_type.is_nullable,
        'is_explicit_nullable': idl_type.is_explicit_nullable,
        'is_named_constructor': is_named_constructor_attribute(attribute),
        'is_partial_interface_member':
            'PartialInterfaceImplementedAs' in extended_attributes,
        'is_per_world_bindings': 'PerWorldBindings' in extended_attributes,
        'is_put_forwards': 'PutForwards' in extended_attributes,
        'is_read_only': attribute.is_read_only,
        'is_reflect': is_reflect,
        'is_replaceable': 'Replaceable' in attribute.extended_attributes,
        'is_save_same_object': is_save_same_object,
        'is_static': attribute.is_static,
        'is_url': 'URL' in extended_attributes,
        'is_unforgeable': is_unforgeable(attribute),
        'measure_as': measure_as,
        'name': attribute.name,
        'on_instance': v8_utilities.on_instance(interface, attribute),
        'on_interface': v8_utilities.on_interface(interface, attribute),
        'on_prototype': v8_utilities.on_prototype(interface, attribute),
        # [RuntimeEnabled] for origin trial
        'origin_trial_feature_name':
            v8_utilities.origin_trial_feature_name(attribute, runtime_features),
        'private_property_is_shared_between_getter_and_setter':
        private_property_is_shared_between_getter_and_setter,
        'property_attributes': property_attributes(interface, attribute),
        'reflect_empty': cpp_content_attribute_value_name(
            interface, extended_attributes.get('ReflectEmpty')),
        'reflect_invalid': cpp_content_attribute_value_name(
            interface, extended_attributes.get('ReflectInvalid', '')),
        'reflect_missing': cpp_content_attribute_value_name(
            interface, extended_attributes.get('ReflectMissing')),
        'reflect_only': reflect_only,
        # [RuntimeEnabled] if not in origin trial
        'runtime_enabled_feature_name':
        v8_utilities.runtime_enabled_feature_name(attribute, runtime_features),
        # [SecureContext]
        'secure_context_test': v8_utilities.secure_context(attribute, interface),
        'use_output_parameter_for_result': idl_type.use_output_parameter_for_result,
        'world_suffixes': (
            ['', 'ForMainWorld']
            if 'PerWorldBindings' in extended_attributes
            else ['']),  # [PerWorldBindings]
    }

    if not has_custom_getter(attribute):
        getter_context(interface, attribute, context)
    if not has_custom_setter(attribute) and has_setter(interface, attribute):
        setter_context(interface, attribute, interfaces, context)

    # [RuntimeCallStatsCounter]
    runtime_call_stats_context(interface, attribute, context)

    # [CrossOrigin] is incompatible with a number of other attributes, so check
    # for them here.
    if is_cross_origin:
        if context['has_cross_origin_setter'] and context['has_custom_setter']:
            raise Exception(
                '[CrossOrigin] and [Custom] are incompatible on the same setter: %s.%s',
                interface.name, attribute.name)
        if context['is_per_world_bindings']:
            raise Exception(
                '[CrossOrigin] and [PerWorldBindings] are incompatible: %s.%s',
                interface.name, attribute.name)
        if context['constructor_type']:
            raise Exception(
                '[CrossOrigin] cannot be used for constructors: %s.%s',
                interface.name, attribute.name)

    return context


def runtime_call_stats_context(interface, attribute, context):
    includes.add('platform/bindings/runtime_call_stats.h')
    generic_counter_name = (
        'Blink_' + v8_utilities.cpp_name(interface) + '_' + attribute.name)
    (counter, extended_attribute_defined) = v8_utilities.rcs_counter_name(
        attribute, generic_counter_name)
    runtime_call_stats = {
        'extended_attribute_defined':
        extended_attribute_defined,
        'getter_counter':
        '%s_Getter' % counter,
        'setter_counter':
        '%s_Setter' % counter,
        'constructor_getter_callback_counter':
        '%s_ConstructorGetterCallback' % generic_counter_name,
    }
    context.update({'runtime_call_stats': runtime_call_stats})


def is_origin_trial_enabled(attribute):
    return bool(attribute['origin_trial_feature_name'])


def is_secure_context(attribute):
    return bool(attribute['secure_context_test'])


def filter_accessors(attributes):
    return [
        attribute for attribute in attributes
        if not (attribute['exposed_test'] or is_secure_context(attribute)
                or attribute['context_enabled_feature_name']
                or is_origin_trial_enabled(attribute)
                or attribute['runtime_enabled_feature_name'])
        and not attribute['is_data_type_property']
    ]


def is_data_attribute(attribute):
    return (not (attribute['exposed_test'] or is_secure_context(attribute)
                 or attribute['context_enabled_feature_name']
                 or is_origin_trial_enabled(attribute)
                 or attribute['runtime_enabled_feature_name'])
            and attribute['is_data_type_property'])


def filter_data_attributes(attributes):
    return [
        attribute for attribute in attributes if is_data_attribute(attribute)
    ]


def filter_runtime_enabled(attributes):
    return [
        attribute for attribute in attributes
        if not (attribute['exposed_test'] or is_secure_context(attribute))
        and attribute['runtime_enabled_feature_name']
    ]


def filter_conditionally_enabled(attributes):
    return [
        attribute for attribute in attributes
        if attribute['exposed_test'] or (is_secure_context(
            attribute) and not is_origin_trial_enabled(attribute))
    ]


################################################################################
# Getter
################################################################################


def getter_context(interface, attribute, context):
    idl_type = attribute.idl_type
    base_idl_type = idl_type.base_type
    extended_attributes = attribute.extended_attributes

    cpp_value = getter_expression(interface, attribute, context)
    # Normally we can inline the function call into the return statement to
    # avoid the overhead of using a Ref<> temporary, but for some cases
    # (nullable types, EventHandler, [CachedAttribute], or if there are
    # exceptions), we need to use a local variable.
    # FIXME: check if compilers are smart enough to inline this, and if so,
    # always use a local variable (for readability and CG simplicity).
    if (idl_type.is_explicit_nullable or base_idl_type == 'EventHandler'
            or 'CachedAttribute' in extended_attributes
            or 'ReflectOnly' in extended_attributes
            or context['is_keep_alive_for_gc']
            or context['is_getter_raises_exception']
            or context['high_entropy'] == 'Direct'):
        context['cpp_value_original'] = cpp_value
        cpp_value = 'cpp_value'

    def v8_set_return_value_statement(for_main_world=False):
        if (context['is_keep_alive_for_gc']
                or 'CachedAttribute' in extended_attributes):
            return 'V8SetReturnValue(info, v8_value)'
        if idl_type.is_explicit_nullable:
            cpp_return_value = 'cpp_value.value()'
            if idl_type.is_frozen_array:
                cpp_return_value = 'FreezeV8Object(ToV8(cpp_value.value(), info.Holder(), info.GetIsolate()), info.GetIsolate())'
            return 'V8SetReturnValue(info, {})'.format(cpp_return_value)
        return idl_type.v8_set_return_value(
            cpp_value,
            extended_attributes=extended_attributes,
            script_wrappable='impl',
            for_main_world=for_main_world,
            is_static=attribute.is_static)

    cpp_value_to_script_wrappable = cpp_value
    if idl_type.is_array_buffer_view_or_typed_array:
        cpp_value_to_script_wrappable += '.View()'

    context.update({
        'cpp_value':
        cpp_value,
        'cpp_value_to_script_wrappable':
        cpp_value_to_script_wrappable,
        'cpp_value_to_v8_value':
        idl_type.cpp_value_to_v8_value(
            cpp_value=cpp_value,
            creation_context='holder',
            extended_attributes=extended_attributes),
        'is_getter_call_with_script_state':
        has_extended_attribute_value(attribute, 'GetterCallWith',
                                     'ScriptState'),
        'v8_set_return_value_for_main_world':
        v8_set_return_value_statement(for_main_world=True),
        'v8_set_return_value':
        v8_set_return_value_statement(),
    })


def getter_expression(interface, attribute, context):
    extra_arguments = []
    this_getter_base_name = getter_base_name(interface, attribute,
                                             extra_arguments)
    getter_name = scoped_name(interface, attribute, this_getter_base_name)

    arguments = v8_utilities.call_with_arguments(
        attribute.extended_attributes.get('GetterCallWith')
        or attribute.extended_attributes.get('CallWith'))
    # Members of IDL partial interface definitions are implemented in C++ as
    # static member functions, which for instance members (non-static members)
    # take *impl as their first argument
    if ('PartialInterfaceImplementedAs' in attribute.extended_attributes
            and not attribute.is_static):
        arguments.append('*impl')
    arguments.extend(extra_arguments)
    if context['is_getter_raises_exception']:
        arguments.append('exception_state')
    if attribute.idl_type.use_output_parameter_for_result:
        arguments.append('result')

    expression = '%s(%s)' % (getter_name, ', '.join(arguments))
    # Needed to handle getter expressions returning Type& as the
    # use site for |expression| expects Type*.
    if (attribute.idl_type.is_interface_type and len(arguments) == 0
            and not attribute.idl_type.is_array_buffer_view_or_typed_array):
        return 'WTF::GetPtr(%s)' % expression
    return expression


CONTENT_ATTRIBUTE_GETTER_NAMES = {
    'boolean': 'FastHasAttribute',
    'long': 'GetIntegralAttribute',
    'unsigned long': 'GetUnsignedIntegralAttribute',
    'Element': 'GetElementAttribute',
}


def getter_base_name(interface, attribute, arguments):
    extended_attributes = attribute.extended_attributes

    if 'Reflect' not in extended_attributes:
        name = cpp_name(attribute)
        return name if 'ImplementedAs' in extended_attributes \
            else uncapitalize(name)

    content_attribute_name = (extended_attributes['Reflect']
                              or attribute.name.lower())
    if content_attribute_name in ['class', 'id', 'name']:
        # Special-case for performance optimization.
        return 'Get%sAttribute' % content_attribute_name.capitalize()

    arguments.append(scoped_content_attribute_name(interface, attribute))

    base_idl_type = attribute.idl_type.base_type
    if base_idl_type in CONTENT_ATTRIBUTE_GETTER_NAMES:
        return CONTENT_ATTRIBUTE_GETTER_NAMES[base_idl_type]
    if 'URL' in attribute.extended_attributes:
        return 'GetURLAttribute'
    idl_type = attribute.idl_type
    if idl_type.is_frozen_array:
        return 'Get%sArrayAttribute' % idl_type.element_type
    return 'FastGetAttribute'


def is_keep_alive_for_gc(interface, attribute):
    idl_type = attribute.idl_type
    base_idl_type = idl_type.base_type
    extended_attributes = attribute.extended_attributes
    if attribute.is_static:
        return False
    if idl_type.is_array_buffer_or_view:
        return False
    return (
        # For readonly attributes, for performance reasons we keep the attribute
        # wrapper alive while the owner wrapper is alive, because the attribute
        # never changes.
        (
            attribute.is_read_only and idl_type.is_wrapper_type and
            # There are some exceptions, however:
            not (
                # Node lifetime is managed by object grouping.
                inherits_interface(interface.name, 'Node')
                or inherits_interface(base_idl_type, 'Node') or
                # A self-reference is unnecessary.
                attribute.name == 'self' or
                # FIXME: Remove these hard-coded hacks.
                base_idl_type in ['EventTarget', 'Window']
                or base_idl_type.startswith(('HTML', 'SVG')))))


################################################################################
# Setter
################################################################################


def setter_context(interface, attribute, interfaces, context):
    if 'PutForwards' in attribute.extended_attributes:
        # Make sure the target interface and attribute exist.
        target_interface_name = attribute.idl_type.base_type
        target_attribute_name = attribute.extended_attributes['PutForwards']
        interface = interfaces[target_interface_name]
        try:
            next(candidate for candidate in interface.attributes
                 if candidate.name == target_attribute_name)
        except StopIteration:
            raise Exception('[PutForward] target not found:\n'
                            'Attribute "%s" is not present in interface "%s"' %
                            (target_attribute_name, target_interface_name))
        context['target_attribute_name'] = target_attribute_name
        return

    if ('Replaceable' in attribute.extended_attributes):
        # Create the property, and early-return if an exception is thrown.
        # Subsequent cleanup code may not be prepared to handle a pending
        # exception.
        context['cpp_setter'] = (
            'if (info.Holder()->CreateDataProperty(' +
            'info.GetIsolate()->GetCurrentContext(), ' +
            'property_name, v8_value).IsNothing())' + '\n  return')
        return

    extended_attributes = attribute.extended_attributes
    idl_type = attribute.idl_type

    # [RaisesException], [RaisesException=Setter]
    is_setter_raises_exception = (
        'RaisesException' in extended_attributes
        and extended_attributes['RaisesException'] in [None, 'Setter'])

    has_type_checking_interface = idl_type.is_wrapper_type

    use_common_reflection_setter = False
    # Enable use_common_reflection_setter if
    #  * extended_attributes is [CEReactions, Reflect] or
    #    [CEReactions, Reflect, RuntimeEnabled],
    #  * the type is boolean, DOMString, or DOMString?, and
    #  * the interface inherits from 'Element'.
    if ('Reflect' in extended_attributes
            and 'CEReactions' in extended_attributes
            and str(idl_type) in ('boolean', 'DOMString', 'DOMString?')
            and inherits_interface(interface.name, 'Element')):
        if (len(extended_attributes) == 2
                or (len(extended_attributes) == 3
                    and 'RuntimeEnabled' in extended_attributes)):
            use_common_reflection_setter = True

    context.update({
        'has_setter_exception_state':
        is_setter_raises_exception or has_type_checking_interface
        or idl_type.v8_conversion_needs_exception_state,
        'has_type_checking_interface':
        has_type_checking_interface,
        'is_setter_call_with_execution_context':
        has_extended_attribute_value(attribute, 'SetterCallWith',
                                     'ExecutionContext'),
        'is_setter_call_with_script_state':
        has_extended_attribute_value(attribute, 'SetterCallWith',
                                     'ScriptState'),
        'is_setter_raises_exception':
        is_setter_raises_exception,
        'use_common_reflection_setter':
        use_common_reflection_setter,
        'v8_value_to_local_cpp_value':
        idl_type.v8_value_to_local_cpp_value(
            extended_attributes,
            'v8_value',
            'cpp_value',
            code_generation_target='attribute_set'),
    })

    # setter_expression() depends on context values we set above.
    context['cpp_setter'] = setter_expression(interface, attribute, context)


def setter_expression(interface, attribute, context):
    extended_attributes = attribute.extended_attributes
    arguments = v8_utilities.call_with_arguments(
        extended_attributes.get('SetterCallWith')
        or extended_attributes.get('CallWith'))

    extra_arguments = []
    this_setter_base_name = setter_base_name(interface, attribute,
                                             extra_arguments)
    setter_name = scoped_name(interface, attribute, this_setter_base_name)

    # Members of IDL partial interface definitions are implemented in C++ as
    # static member functions, which for instance members (non-static members)
    # take *impl as their first argument
    if ('PartialInterfaceImplementedAs' in extended_attributes
            and not attribute.is_static):
        arguments.append('*impl')
    arguments.extend(extra_arguments)
    idl_type = attribute.idl_type
    if idl_type.base_type in ('EventHandler', 'OnBeforeUnloadEventHandler',
                              'OnErrorEventHandler'):
        if idl_type.base_type == 'EventHandler':
            handler_type = 'kEventHandler'
        elif idl_type.base_type == 'OnBeforeUnloadEventHandler':
            handler_type = 'kOnBeforeUnloadEventHandler'
        elif idl_type.base_type == 'OnErrorEventHandler':
            handler_type = 'kOnErrorEventHandler'
        arguments.append('JSEventHandler::CreateOrNull(' + 'v8_value, ' +
                         'JSEventHandler::HandlerType::' + handler_type + ')')
    else:
        arguments.append('cpp_value')
    if context['is_setter_raises_exception']:
        arguments.append('exception_state')
    if context['use_common_reflection_setter']:
        attr_name = scoped_content_attribute_name(interface, attribute)
        if idl_type.base_type == 'boolean':
            setter_name = 'V8SetReflectedBooleanAttribute'
            arguments = [
                'info',
                '"%s"' % interface.name,
                '"%s"' % attribute.name, attr_name
            ]
        elif idl_type.base_type == 'DOMString':
            if idl_type.is_nullable:
                setter_name = 'V8SetReflectedNullableDOMStringAttribute'
            else:
                setter_name = 'V8SetReflectedDOMStringAttribute'
            arguments = ['info', attr_name]

    return '%s(%s)' % (setter_name, ', '.join(arguments))


CONTENT_ATTRIBUTE_SETTER_NAMES = {
    'boolean': 'SetBooleanAttribute',
    'long': 'SetIntegralAttribute',
    'unsigned long': 'SetUnsignedIntegralAttribute',
    'Element': 'SetElementAttribute',
}


def setter_base_name(interface, attribute, arguments):
    if 'Reflect' not in attribute.extended_attributes:
        return 'set%s' % capitalize(cpp_name(attribute))
    arguments.append(scoped_content_attribute_name(interface, attribute))

    base_idl_type = attribute.idl_type.base_type
    if base_idl_type in CONTENT_ATTRIBUTE_SETTER_NAMES:
        return CONTENT_ATTRIBUTE_SETTER_NAMES[base_idl_type]
    idl_type = attribute.idl_type
    if idl_type.is_frozen_array:
        return 'Set%sArrayAttribute' % idl_type.element_type
    return 'setAttribute'


def scoped_content_attribute_name(interface, attribute):
    content_attribute_name = (attribute.extended_attributes['Reflect']
                              or attribute.name.lower())
    symbol_name = 'k' + NameStyleConverter(
        content_attribute_name).to_upper_camel_case()
    if interface.name.startswith('SVG'):
        namespace = 'svg_names'
        includes.add('core/svg_names.h')
    else:
        namespace = 'html_names'
        includes.add('core/html_names.h')
    return '%s::%sAttr' % (namespace, symbol_name)


def cpp_content_attribute_value_name(interface, value):
    if value == '':
        return 'g_empty_atom'
    if not value:
        return value
    includes.add('core/keywords.h')
    return 'keywords::' + NameStyleConverter(value).to_enum_value()


################################################################################
# Attribute configuration
################################################################################


# Property descriptor's {writable: boolean}
def is_writable(attribute):
    return (not attribute.is_read_only or any(
        keyword in attribute.extended_attributes
        for keyword in ['PutForwards', 'Replaceable', 'LegacyLenientSetter']))


def is_data_type_property(interface, attribute):
    return (is_constructor_attribute(attribute)
            or 'CrossOrigin' in attribute.extended_attributes)


# [PutForwards], [Replaceable], [LegacyLenientSetter]
def has_setter(interface, attribute):
    if (is_data_type_property(interface, attribute)
            and (is_constructor_attribute(attribute)
                 or 'Replaceable' in attribute.extended_attributes)):
        return False

    return is_writable(attribute)


# [NotEnumerable], [LegacyUnforgeable]
def property_attributes(interface, attribute):
    extended_attributes = attribute.extended_attributes
    property_attributes_list = []
    if ('NotEnumerable' in extended_attributes
            or is_constructor_attribute(attribute)):
        property_attributes_list.append('v8::DontEnum')
    if is_unforgeable(attribute):
        property_attributes_list.append('v8::DontDelete')
    if not is_writable(attribute):
        property_attributes_list.append('v8::ReadOnly')
    return property_attributes_list or ['v8::None']


# [Custom], [Custom=Getter]
def has_custom_getter(attribute):
    extended_attributes = attribute.extended_attributes
    return ('Custom' in extended_attributes
            and extended_attributes['Custom'] in [None, 'Getter'])


# [Custom], [Custom=Setter]
def has_custom_setter(attribute):
    extended_attributes = attribute.extended_attributes
    return (not attribute.is_read_only and 'Custom' in extended_attributes
            and extended_attributes['Custom'] in [None, 'Setter'])


################################################################################
# Constructors
################################################################################

idl_types.IdlType.constructor_type_name = property(
    lambda self: strip_suffix(self.base_type, 'Constructor'))


def is_constructor_attribute(attribute):
    return attribute.idl_type.name.endswith('Constructor')


def is_named_constructor_attribute(attribute):
    return attribute.idl_type.name.endswith('ConstructorConstructor')
