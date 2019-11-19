# Copyright (C) 2013 Google Inc. All rights reserved.
# coding=utf-8
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

"""Generate template values for an interface.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""
import os
import sys
from operator import or_

sys.path.append(os.path.join(os.path.dirname(__file__),
                             '..', '..', 'build', 'scripts'))
from blinkbuild.name_style_converter import NameStyleConverter
from idl_definitions import IdlAttribute, IdlOperation, IdlArgument
from idl_types import IdlType, inherits_interface
from overload_set_algorithm import effective_overload_set_by_length
from overload_set_algorithm import method_overloads_by_name

import v8_attributes
from v8_globals import includes
import v8_methods
import v8_types
import v8_utilities
from v8_utilities import (binding_header_filename, context_enabled_feature_name,
                          cpp_name_or_partial, cpp_name,
                          has_extended_attribute_value,
                          runtime_enabled_feature_name)


INTERFACE_H_INCLUDES = frozenset([
    'bindings/core/v8/generated_code_helper.h',
    'bindings/core/v8/native_value_traits.h',
    'platform/bindings/script_wrappable.h',
    'bindings/core/v8/to_v8_for_core.h',
    'bindings/core/v8/v8_binding_for_core.h',
    'platform/bindings/v8_dom_wrapper.h',
    'platform/bindings/wrapper_type_info.h',
    'platform/heap/handle.h',
])
INTERFACE_CPP_INCLUDES = frozenset([
    'base/memory/scoped_refptr.h',
    'bindings/core/v8/v8_dom_configuration.h',
    'core/execution_context/execution_context.h',
    'platform/scheduler/public/cooperative_scheduling_manager.h',
    'platform/bindings/exception_messages.h',
    'platform/bindings/exception_state.h',
    'platform/bindings/v8_object_constructor.h',
    'platform/wtf/get_ptr.h',
])


def filter_has_constant_configuration(constants):
    return [constant for constant in constants if
            not constant['measure_as'] and
            not constant['deprecate_as'] and
            not constant['runtime_enabled_feature_name'] and
            not constant['origin_trial_feature_name']]


def filter_has_special_getter(constants):
    return [constant for constant in constants if
            constant['measure_as'] or
            constant['deprecate_as']]


def filter_runtime_enabled(constants):
    return [constant for constant in constants if
            constant['runtime_enabled_feature_name']]


def filter_origin_trial_enabled(constants):
    return [constant for constant in constants if
            constant['origin_trial_feature_name']]


def constant_filters():
    return {'has_constant_configuration': filter_has_constant_configuration,
            'has_special_getter': filter_has_special_getter,
            'runtime_enabled_constants': filter_runtime_enabled,
            'origin_trial_enabled_constants': filter_origin_trial_enabled}


def origin_trial_features(interface, constants, attributes, methods):
    """ Returns a list of the origin trial features used in this interface.

    Each element is a dictionary with keys 'name' and 'needs_instance'.
    'needs_instance' is true if any member associated with the interface needs
    to be installed on every instance of the interface. This list is the union
    of the sets of features used for constants, attributes and methods.
    """
    KEY = 'origin_trial_feature_name'  # pylint: disable=invalid-name

    def member_filter(members):
        return sorted([member for member in members if member.get(KEY)])

    def member_filter_by_name(members, name):
        return [member for member in members if member[KEY] == name]

    # Collect all members visible on this interface with a defined origin trial
    origin_trial_constants = member_filter(constants)
    origin_trial_attributes = member_filter(attributes)
    origin_trial_methods = member_filter([method for method in methods
                                          if v8_methods.method_is_visible(method, interface.is_partial) and
                                          not v8_methods.custom_registration(method)])

    feature_names = set([member[KEY] for member in origin_trial_constants + origin_trial_attributes + origin_trial_methods])

    # Construct the list of dictionaries. 'needs_instance' will be true if any
    # member for the feature has 'on_instance' defined as true.
    features = [{'name': name,
                 'constants': member_filter_by_name(origin_trial_constants, name),
                 'attributes': member_filter_by_name(origin_trial_attributes, name),
                 'methods': member_filter_by_name(origin_trial_methods, name)}
                for name in feature_names]
    for feature in features:
        members = feature['constants'] + feature['attributes'] + feature['methods']
        feature['needs_instance'] = any(member.get('on_instance', False) for member in members)
        # TODO(chasej): Need to handle method overloads? e.g.
        # (method['overloads']['secure_context_test_all'] if 'overloads' in method else method['secure_context_test'])
        feature['needs_secure_context'] = any(member.get('secure_context_test', False) for member in members)
        feature['needs_context'] = feature['needs_secure_context'] or any(member.get('exposed_test', False) for member in members)

    if features:
        includes.add('platform/bindings/script_state.h')
        includes.add('platform/runtime_enabled_features.h')
        includes.add('core/execution_context/execution_context.h')

    return features


def context_enabled_features(attributes):
    """ Returns a list of context-enabled features from a set of attributes.

    Each element is a dictionary with the feature's |name| and lists of
    |attributes| associated with the feature.
    """
    KEY = 'context_enabled_feature_name'  # pylint: disable=invalid-name

    def member_filter(members):
        return sorted([member for member in members if member.get(KEY) and not member.get('exposed_test')])

    def member_filter_by_name(members, name):
        return [member for member in members if member[KEY] == name]

    # Collect all members visible on this interface with a defined origin trial
    context_enabled_attributes = member_filter(attributes)
    feature_names = set([member[KEY] for member in context_enabled_attributes])
    features = [{'name': name,
                 'attributes': member_filter_by_name(context_enabled_attributes, name),
                 'needs_instance': False}
                for name in feature_names]
    if features:
        includes.add('platform/bindings/script_state.h')
    return features


def runtime_call_stats_context(interface):
    counter_prefix = 'Blink_' + v8_utilities.cpp_name(interface) + '_'
    return {
        'constructor_counter': counter_prefix + 'Constructor',
        'cross_origin_named_getter_counter': counter_prefix + 'CrossOriginNamedGetter',
        'cross_origin_named_setter_counter': counter_prefix + 'CrossOriginNamedSetter',
        'indexed_property_getter_counter': counter_prefix + 'IndexedPropertyGetter',
        'named_property_getter_counter': counter_prefix + 'NamedPropertyGetter',
        'named_property_query_counter': counter_prefix + 'NamedPropertyQuery',
        'named_property_setter_counter': counter_prefix + 'NamedPropertySetter',
    }


def interface_context(interface, interfaces, component_info):
    """Creates a Jinja template context for an interface.

    Args:
        interface: An interface to create the context for
        interfaces: A dict which maps an interface name to the definition
            which can be referred if needed
        component_info: A dict containing component wide information

    Returns:
        A Jinja template context for |interface|
    """

    includes.clear()
    includes.update(INTERFACE_CPP_INCLUDES)
    header_includes = set(INTERFACE_H_INCLUDES)

    if interface.is_partial:
        # A partial interface definition cannot specify that the interface
        # inherits from another interface. Inheritance must be specified on
        # the original interface definition.
        parent_interface = None
        is_event_target = False
        # partial interface needs the definition of its original interface.
        includes.add('bindings/core/v8/%s' % binding_header_filename(interface.name))
    else:
        parent_interface = interface.parent
        if parent_interface:
            header_includes.update(v8_types.includes_for_interface(parent_interface))
        is_event_target = inherits_interface(interface.name, 'EventTarget')

    extended_attributes = interface.extended_attributes

    is_array_buffer_or_view = interface.idl_type.is_array_buffer_or_view
    is_typed_array_type = interface.idl_type.is_typed_array
    if is_array_buffer_or_view:
        includes.update(('bindings/core/v8/v8_array_buffer.h',
                         'bindings/core/v8/v8_shared_array_buffer.h'))
    if interface.name == 'ArrayBufferView':
        includes.update((
            'bindings/core/v8/v8_int8_array.h',
            'bindings/core/v8/v8_int16_array.h',
            'bindings/core/v8/v8_int32_array.h',
            'bindings/core/v8/v8_uint8_array.h',
            'bindings/core/v8/v8_uint8_clamped_array.h',
            'bindings/core/v8/v8_uint16_array.h',
            'bindings/core/v8/v8_uint32_array.h',
            'bindings/core/v8/v8_big_int_64_array.h',
            'bindings/core/v8/v8_big_uint_64_array.h',
            'bindings/core/v8/v8_float32_array.h',
            'bindings/core/v8/v8_float64_array.h',
            'bindings/core/v8/v8_data_view.h'))

    # [ActiveScriptWrappable]
    active_scriptwrappable = 'ActiveScriptWrappable' in extended_attributes

    # [CheckSecurity]
    is_check_security = 'CheckSecurity' in extended_attributes
    if is_check_security:
        includes.add('bindings/core/v8/binding_security.h')
        includes.add('core/frame/local_dom_window.h')

    # [Global]
    is_global = 'Global' in extended_attributes

    # [ImmutablePrototype]
    # TODO(littledan): Is it possible to deduce this based on inheritance,
    # as in the WebIDL spec?
    is_immutable_prototype = is_global or 'ImmutablePrototype' in extended_attributes

    wrapper_class_id = ('kNodeClassId' if inherits_interface(interface.name, 'Node') else 'kObjectClassId')

    # [LegacyUnenumerableNamedProperties]
    # pylint: disable=C0103
    has_legacy_unenumerable_named_properties = (interface.has_named_property_getter and
                                                'LegacyUnenumerableNamedProperties' in extended_attributes)

    v8_class_name = v8_utilities.v8_class_name(interface)
    cpp_class_name = cpp_name(interface)
    cpp_class_name_or_partial = cpp_name_or_partial(interface)
    v8_class_name_or_partial = v8_utilities.v8_class_name_or_partial(interface)

    # TODO(peria): Generate the target list from 'Window' and 'HTMLDocument'.
    needs_runtime_enabled_installer = v8_class_name in [
        'V8Window', 'V8HTMLDocument', 'V8Document', 'V8Node', 'V8EventTarget']

    runtime_features = component_info['runtime_enabled_features']

    context = {
        'active_scriptwrappable': active_scriptwrappable,
        'context_enabled_feature_name': context_enabled_feature_name(interface),  # [ContextEnabled]
        'cpp_class': cpp_class_name,
        'cpp_class_or_partial': cpp_class_name_or_partial,
        'is_gc_type': True,
        # FIXME: Remove 'EventTarget' special handling, http://crbug.com/383699
        'has_access_check_callbacks': (is_check_security and
                                       interface.name != 'EventTarget'),
        'has_custom_legacy_call_as_function': has_extended_attribute_value(interface, 'Custom', 'LegacyCallAsFunction'),  # [Custom=LegacyCallAsFunction]
        'has_legacy_unenumerable_named_properties': has_legacy_unenumerable_named_properties,
        'has_partial_interface': len(interface.partial_interfaces) > 0,
        'header_includes': header_includes,
        'interface_name': interface.name,
        'internal_namespace': internal_namespace(interface),
        'is_array_buffer_or_view': is_array_buffer_or_view,
        'is_check_security': is_check_security,
        'is_event_target': is_event_target,
        'is_global': is_global,
        'is_immutable_prototype': is_immutable_prototype,
        'is_node': inherits_interface(interface.name, 'Node'),
        'is_partial': interface.is_partial,
        'is_typed_array_type': is_typed_array_type,
        'measure_as': v8_utilities.measure_as(interface, None),  # [MeasureAs]
        'needs_runtime_enabled_installer': needs_runtime_enabled_installer,
        'origin_trial_feature_name': v8_utilities.origin_trial_feature_name(interface, runtime_features),
        'parent_interface': parent_interface,
        'pass_cpp_type': cpp_name(interface) + '*',
        'runtime_call_stats': runtime_call_stats_context(interface),
        'runtime_enabled_feature_name': runtime_enabled_feature_name(interface, runtime_features),  # [RuntimeEnabled]
        'snake_case_v8_class': NameStyleConverter(v8_class_name).to_snake_case(),
        'v8_class': v8_class_name,
        'v8_class_or_partial': v8_class_name_or_partial,
        'wrapper_class_id': wrapper_class_id,
    }

    # Constructors
    constructors = [constructor_context(interface, constructor)
                    for constructor in interface.constructors
                    # FIXME: shouldn't put named constructors with constructors
                    # (currently needed for Perl compatibility)
                    # Handle named constructors separately
                    if constructor.name == 'Constructor']
    if len(constructors) > 1:
        context['constructor_overloads'] = overloads_context(interface, constructors)

    # [CustomConstructor]
    custom_constructors = [{  # Only needed for computing interface length
        'number_of_required_arguments':
            number_of_required_arguments(constructor),
    } for constructor in interface.custom_constructors]

    # [HTMLConstructor]
    has_html_constructor = 'HTMLConstructor' in extended_attributes
    # https://html.spec.whatwg.org/C/#html-element-constructors
    if has_html_constructor:
        if ('Constructor' in extended_attributes or
                'NoInterfaceObject' in extended_attributes or interface.is_mixin):
            raise Exception('[HTMLConstructor] cannot be specified with '
                            '[Constructor] or [NoInterfaceObject], or on '
                            'a mixin : %s' % interface.name)
        includes.add('bindings/core/v8/v8_html_constructor.h')

    # [NamedConstructor]
    named_constructor = named_constructor_context(interface)

    if constructors or custom_constructors or named_constructor:
        if interface.is_partial:
            raise Exception('[Constructor] and [NamedConstructor] MUST NOT be'
                            ' specified on partial interface definitions: '
                            '%s' % interface.name)
        if named_constructor:
            includes.add('platform/bindings/v8_per_context_data.h')
            includes.add('platform/bindings/v8_private_property.h')

        includes.add('platform/bindings/v8_object_constructor.h')
        includes.add('core/frame/local_dom_window.h')
    elif 'Measure' in extended_attributes or 'MeasureAs' in extended_attributes:
        if not interface.is_partial:
            raise Exception('[Measure] or [MeasureAs] specified for interface without a constructor: '
                            '%s' % interface.name)

    # [ConstructorCallWith=Document]
    if has_extended_attribute_value(interface, 'ConstructorCallWith', 'Document'):
        includes.add('core/dom/document.h')

    # [Unscopable] attributes and methods
    unscopables = []
    for attribute in interface.attributes:
        if 'Unscopable' in attribute.extended_attributes:
            unscopables.append((attribute.name, runtime_enabled_feature_name(attribute, runtime_features)))
    for method in interface.operations:
        if 'Unscopable' in method.extended_attributes:
            unscopables.append((method.name, runtime_enabled_feature_name(method, runtime_features)))

    # [CEReactions]
    setter_or_deleters = (
        interface.indexed_property_setter,
        interface.indexed_property_deleter,
        interface.named_property_setter,
        interface.named_property_deleter,
    )
    has_ce_reactions = any(setter_or_deleter and 'CEReactions' in setter_or_deleter.extended_attributes
                           for setter_or_deleter in setter_or_deleters)
    if has_ce_reactions:
        includes.add('core/html/custom/ce_reactions_scope.h')

    context.update({
        'constructors': constructors,
        'has_custom_constructor': bool(custom_constructors),
        'has_html_constructor': has_html_constructor,
        'interface_length':
            interface_length(constructors + custom_constructors),
        'is_constructor_raises_exception': extended_attributes.get('RaisesException') == 'Constructor',  # [RaisesException=Constructor]
        'named_constructor': named_constructor,
        'unscopables': sorted(unscopables),
    })

    # Constants
    context.update({
        'constants': [constant_context(constant, interface, component_info) for constant in interface.constants],
        'do_not_check_constants': 'DoNotCheckConstants' in extended_attributes,
    })

    # Attributes
    attributes = attributes_context(interface, interfaces, component_info)

    context.update({
        'attributes': attributes,
        # Elements in attributes are broken in following members.
        'accessors': v8_attributes.filter_accessors(attributes),
        'data_attributes': v8_attributes.filter_data_attributes(attributes),
        'runtime_enabled_attributes': v8_attributes.filter_runtime_enabled(attributes),
    })

    # Conditionally enabled attributes
    conditionally_enabled_attributes = v8_attributes.filter_conditionally_enabled(attributes)
    conditional_attributes = [attr for attr in conditionally_enabled_attributes if not attr['constructor_type']]
    conditional_interface_objects = [attr for attr in conditionally_enabled_attributes if attr['constructor_type']]
    has_conditional_secure_attributes = any(  # pylint: disable=invalid-name
        v8_attributes.is_secure_context(attr) for attr in conditionally_enabled_attributes)
    context.update({
        'conditional_attributes': conditional_attributes,
        'conditional_interface_objects': conditional_interface_objects,
        'has_conditional_secure_attributes': has_conditional_secure_attributes,
    })

    # Methods
    context.update(methods_context(interface, component_info))
    methods = context['methods']

    # Conditionally enabled methods
    conditional_methods = v8_methods.filter_conditionally_enabled(methods, interface.is_partial)
    has_conditional_secure_methods = any(  # pylint: disable=invalid-name
        v8_methods.is_secure_context(method) for method in conditional_methods)
    context.update({
        'has_conditional_secure_methods':
            has_conditional_secure_methods,
        'conditional_methods': conditional_methods,
    })

    # Window.idl in Blink has indexed properties, but the spec says Window
    # interface doesn't have indexed properties, instead the WindowProxy exotic
    # object has indexed properties.  Thus, Window interface must not support
    # iterators.
    has_array_iterator = (not interface.is_partial and
                          interface.has_indexed_elements and
                          interface.name != 'Window')
    context.update({
        'has_array_iterator': has_array_iterator,
        'iterable': interface.iterable,
    })

    # Conditionally enabled members
    install_conditional_features_func = None  # pylint: disable=invalid-name
    if unscopables or conditional_interface_objects or conditional_attributes or conditional_methods:
        install_conditional_features_func = (  # pylint: disable=invalid-name
            v8_class_name_or_partial + '::InstallConditionalFeatures')

    context.update({
        'install_conditional_features_func': install_conditional_features_func,
    })

    context.update({
        'indexed_property_getter': property_getter(interface.indexed_property_getter, ['index']),
        'indexed_property_setter': property_setter(interface.indexed_property_setter, interface),
        'indexed_property_deleter': property_deleter(interface.indexed_property_deleter),
        'is_override_builtins': 'OverrideBuiltins' in extended_attributes,
        'named_property_getter': property_getter(interface.named_property_getter, ['name']),
        'named_property_setter': property_setter(interface.named_property_setter, interface),
        'named_property_deleter': property_deleter(interface.named_property_deleter),
    })
    context.update({
        'has_named_properties_object': is_global and context['named_property_getter'],
    })

    # Origin Trials and ContextEnabled features
    context.update({
        'optional_features':
            sorted(origin_trial_features(interface, context['constants'], context['attributes'], context['methods']) +
                   context_enabled_features(context['attributes'])),
    })
    if context['optional_features']:
        includes.add('platform/bindings/v8_per_context_data.h')

    # Cross-origin interceptors
    has_cross_origin_named_getter = False
    has_cross_origin_named_setter = False
    has_cross_origin_indexed_getter = False

    for attribute in attributes:
        if attribute['has_cross_origin_getter']:
            has_cross_origin_named_getter = True
        if attribute['has_cross_origin_setter']:
            has_cross_origin_named_setter = True

    # Methods are exposed as getter attributes on the interface: e.g.
    # window.location gets the location attribute on the Window interface. For
    # the cross-origin case, this attribute getter is guaranteed to only return
    # a Function object, which the actual call is dispatched against.
    for method in methods:
        if method['is_cross_origin']:
            has_cross_origin_named_getter = True

    has_cross_origin_named_enumerator = has_cross_origin_named_getter or has_cross_origin_named_setter  # pylint: disable=invalid-name

    if context['named_property_getter'] and context['named_property_getter']['is_cross_origin']:
        has_cross_origin_named_getter = True

    if context['indexed_property_getter'] and context['indexed_property_getter']['is_cross_origin']:
        has_cross_origin_indexed_getter = True

    context.update({
        'has_cross_origin_named_getter': has_cross_origin_named_getter,
        'has_cross_origin_named_setter': has_cross_origin_named_setter,
        'has_cross_origin_named_enumerator': has_cross_origin_named_enumerator,
        'has_cross_origin_indexed_getter': has_cross_origin_indexed_getter,
    })

    return context


def attributes_context(interface, interfaces, component_info):
    """Creates a list of Jinja template contexts for attributes of an interface.

    Args:
        interface: An interface to create contexts for
        interfaces: A dict which maps an interface name to the definition
            which can be referred if needed

    Returns:
        A list of attribute contexts
    """

    attributes = [v8_attributes.attribute_context(interface, attribute, interfaces, component_info)
                  for attribute in interface.attributes]

    has_conditional_attributes = any(attribute['exposed_test'] for attribute in attributes)
    if has_conditional_attributes and interface.is_partial:
        raise Exception(
            'Conditional attributes between partial interfaces in modules '
            'and the original interfaces(%s) in core are not allowed.'
            % interface.name)

    # See also comment in methods_context.
    if not interface.is_partial and (interface.maplike or interface.setlike):
        if any(attribute['name'] == 'size' for attribute in attributes):
            raise ValueError(
                'An interface cannot define an attribute called "size"; it is '
                'implied by maplike/setlike in the IDL.')
        size_attribute = IdlAttribute()
        size_attribute.name = 'size'
        size_attribute.idl_type = IdlType('unsigned long')
        size_attribute.is_read_only = True
        size_attribute.extended_attributes['NotEnumerable'] = None
        attributes.append(v8_attributes.attribute_context(
            interface, size_attribute, interfaces, component_info))

    return attributes


def methods_context(interface, component_info):
    """Creates a list of Jinja template contexts for methods of an interface.

    Args:
        interface: An interface to create contexts for
        component_info: A dict containing component wide information

    Returns:
        A dictionary with 3 keys:
        'iterator_method': An iterator context if available or None.
        'iterator_method_alias': A string that can also be used to refer to the
                                 @@iterator symbol or None.
        'methods': A list of method contexts.
    """

    methods = []

    if interface.original_interface:
        methods.extend([v8_methods.method_context(interface, operation, component_info, is_visible=False)
                        for operation in interface.original_interface.operations
                        if operation.name])
    methods.extend([v8_methods.method_context(interface, method, component_info)
                    for method in interface.operations
                    if method.name])  # Skip anonymous special operations (methods)
    if interface.partial_interfaces:
        assert len(interface.partial_interfaces) == len(set(interface.partial_interfaces))
        for partial_interface in interface.partial_interfaces:
            methods.extend([v8_methods.method_context(interface, operation, component_info, is_visible=False)
                            for operation in partial_interface.operations
                            if operation.name])
    compute_method_overloads_context(interface, methods)

    def generated_method(return_type, name, arguments=None, extended_attributes=None, implemented_as=None):
        operation = IdlOperation()
        operation.idl_type = return_type
        operation.name = name
        if arguments:
            operation.arguments = arguments
        if extended_attributes:
            operation.extended_attributes.update(extended_attributes)
        if implemented_as is None:
            implemented_as = name + 'ForBinding'
        operation.extended_attributes['ImplementedAs'] = implemented_as
        return v8_methods.method_context(interface, operation, component_info)

    def generated_argument(idl_type, name, is_optional=False, extended_attributes=None):
        argument = IdlArgument()
        argument.idl_type = idl_type
        argument.name = name
        argument.is_optional = is_optional
        if extended_attributes:
            argument.extended_attributes.update(extended_attributes)
        return argument

    # iterable<>, maplike<> and setlike<>
    iterator_method = None

    # Depending on the declaration, @@iterator may be a synonym for e.g.
    # 'entries' or 'values'.
    iterator_method_alias = None

    # FIXME: support Iterable in partial interfaces. However, we don't
    # need to support iterator overloads between interface and
    # partial interface definitions.
    # http://heycam.github.io/webidl/#idl-overloading
    if (not interface.is_partial and (
            interface.iterable or interface.maplike or interface.setlike or
            interface.has_indexed_elements)):

        used_extended_attributes = {}

        if interface.iterable:
            used_extended_attributes.update(interface.iterable.extended_attributes)
        elif interface.maplike:
            used_extended_attributes.update(interface.maplike.extended_attributes)
        elif interface.setlike:
            used_extended_attributes.update(interface.setlike.extended_attributes)

        if 'RaisesException' in used_extended_attributes:
            raise ValueError('[RaisesException] is implied for iterable<>/maplike<>/setlike<>')
        if 'CallWith' in used_extended_attributes:
            raise ValueError('[CallWith=ScriptState] is implied for iterable<>/maplike<>/setlike<>')

        used_extended_attributes.update({
            'RaisesException': None,
            'CallWith': 'ScriptState',
        })

        forEach_extended_attributes = used_extended_attributes.copy()
        forEach_extended_attributes.update({
            'CallWith': ['ScriptState', 'ThisValue'],
        })

        def generated_iterator_method(name, implemented_as=None):
            return generated_method(
                return_type=IdlType('Iterator'),
                name=name,
                extended_attributes=used_extended_attributes,
                implemented_as=implemented_as)

        if not interface.has_indexed_elements:
            iterator_method = generated_iterator_method('iterator', implemented_as='GetIterator')

        if interface.iterable or interface.maplike or interface.setlike:
            non_overridable_methods = []
            overridable_methods = []

            is_value_iterator = interface.iterable and interface.iterable.key_type is None

            # For value iterators, the |entries|, |forEach|, |keys| and |values| are originally set
            # to corresponding properties in %ArrayPrototype%.
            # For pair iterators and maplike declarations, |entries| is an alias for @@iterator
            # itself. For setlike declarations, |values| is an alias for @@iterator.
            if not is_value_iterator:
                if not interface.setlike:
                    iterator_method_alias = 'entries'
                    entries_or_values_method = generated_iterator_method('values')
                else:
                    iterator_method_alias = 'values'
                    entries_or_values_method = generated_iterator_method('entries')

                non_overridable_methods.extend([
                    generated_iterator_method('keys'),
                    entries_or_values_method,

                    # void forEach(ForEachIteratorCallback callback, [DefaultValue=Undefined] optional any thisArg)
                    generated_method(IdlType('void'), 'forEach',
                                     arguments=[generated_argument(IdlType('ForEachIteratorCallback'), 'callback'),
                                                generated_argument(IdlType('any'), 'thisArg',
                                                                   is_optional=True,
                                                                   extended_attributes={'DefaultValue': 'Undefined'})],
                                     extended_attributes=forEach_extended_attributes),
                ])

            if interface.maplike:
                key_argument = generated_argument(interface.maplike.key_type, 'key')
                value_argument = generated_argument(interface.maplike.value_type, 'value')

                non_overridable_methods.extend([
                    generated_method(IdlType('boolean'), 'has',
                                     arguments=[key_argument],
                                     extended_attributes=used_extended_attributes),
                    generated_method(IdlType('any'), 'get',
                                     arguments=[key_argument],
                                     extended_attributes=used_extended_attributes),
                ])

                if not interface.maplike.is_read_only:
                    overridable_methods.extend([
                        generated_method(IdlType('void'), 'clear',
                                         extended_attributes=used_extended_attributes),
                        generated_method(IdlType('boolean'), 'delete',
                                         arguments=[key_argument],
                                         extended_attributes=used_extended_attributes),
                        generated_method(IdlType(interface.name), 'set',
                                         arguments=[key_argument, value_argument],
                                         extended_attributes=used_extended_attributes),
                    ])

            if interface.setlike:
                value_argument = generated_argument(interface.setlike.value_type, 'value')

                non_overridable_methods.extend([
                    generated_method(IdlType('boolean'), 'has',
                                     arguments=[value_argument],
                                     extended_attributes=used_extended_attributes),
                ])

                if not interface.setlike.is_read_only:
                    overridable_methods.extend([
                        generated_method(IdlType(interface.name), 'add',
                                         arguments=[value_argument],
                                         extended_attributes=used_extended_attributes),
                        generated_method(IdlType('void'), 'clear',
                                         extended_attributes=used_extended_attributes),
                        generated_method(IdlType('boolean'), 'delete',
                                         arguments=[value_argument],
                                         extended_attributes=used_extended_attributes),
                    ])

            methods_by_name = {}
            for method in methods:
                methods_by_name.setdefault(method['name'], []).append(method)

            for non_overridable_method in non_overridable_methods:
                if non_overridable_method['name'] in methods_by_name:
                    raise ValueError(
                        'An interface cannot define an operation called "%s()", it '
                        'comes from the iterable, maplike or setlike declaration '
                        'in the IDL.' % non_overridable_method['name'])
                methods.append(non_overridable_method)

            for overridable_method in overridable_methods:
                if overridable_method['name'] in methods_by_name:
                    # FIXME: Check that the existing method is compatible.
                    continue
                methods.append(overridable_method)

        # FIXME: maplike<> and setlike<> should also imply the presence of a
        # 'size' attribute.

    # Stringifier
    if interface.stringifier:
        stringifier = interface.stringifier
        stringifier_ext_attrs = stringifier.extended_attributes.copy()
        if stringifier.attribute:
            implemented_as = stringifier.attribute.name
        elif stringifier.operation:
            implemented_as = stringifier.operation.name
        else:
            implemented_as = 'toString'
        methods.append(generated_method(
            return_type=IdlType('DOMString'),
            name='toString',
            extended_attributes=stringifier_ext_attrs,
            implemented_as=implemented_as))

    for method in methods:
        # The value of the Function object’s “length” property is a Number
        # determined as follows:
        # 1. Let S be the effective overload set for regular operations (if the
        # operation is a regular operation) or for static operations (if the
        # operation is a static operation) with identifier id on interface I and
        # with argument count 0.
        # 2. Return the length of the shortest argument list of the entries in S.
        # FIXME: This calculation doesn't take into account whether runtime
        # enabled overloads are actually enabled, so length may be incorrect.
        # E.g., [RuntimeEnabled=Foo] void f(); void f(long x);
        # should have length 1 if Foo is not enabled, but length 0 if it is.
        method['length'] = (method['overloads']['length'] if 'overloads' in method else
                            method['number_of_required_arguments'])

    return {
        'iterator_method': iterator_method,
        'iterator_method_alias': iterator_method_alias,
        'methods': methods,
    }


def reflected_name(constant_name):
    """Returns the name to use for the matching constant name in blink code.

    Given an all-uppercase 'CONSTANT_NAME', returns a camel-case
    'kConstantName'.
    """
    # Check for SHOUTY_CASE constants
    if constant_name.upper() != constant_name:
        return constant_name
    return 'k' + ''.join(part.title() for part in constant_name.split('_'))


# [DeprecateAs], [Reflect], [RuntimeEnabled]
def constant_context(constant, interface, component_info):
    extended_attributes = constant.extended_attributes
    runtime_features = component_info['runtime_enabled_features']

    return {
        'camel_case_name': NameStyleConverter(constant.name).to_upper_camel_case(),
        'cpp_class': extended_attributes.get('PartialInterfaceImplementedAs'),
        'cpp_type': constant.idl_type.cpp_type,
        'deprecate_as': v8_utilities.deprecate_as(constant),  # [DeprecateAs]
        'idl_type': constant.idl_type.name,
        'measure_as': v8_utilities.measure_as(constant, interface),  # [MeasureAs]
        'high_entropy': v8_utilities.high_entropy(constant),  # [HighEntropy]
        'name': constant.name,
        'origin_trial_feature_name': v8_utilities.origin_trial_feature_name(constant,
                                                                            runtime_features),  # [RuntimeEnabled] for origin trial
        # FIXME: use 'reflected_name' as correct 'name'
        'rcs_counter': 'Blink_' + v8_utilities.cpp_name(interface) + '_' + constant.name + '_ConstantGetter',
        'reflected_name': extended_attributes.get('Reflect', reflected_name(constant.name)),
        'runtime_enabled_feature_name': runtime_enabled_feature_name(constant,
                                                                     runtime_features),  # [RuntimeEnabled] if not in origin trial
        'value': constant.value,
    }


################################################################################
# Overloads
################################################################################

def compute_method_overloads_context(interface, methods):
    # Regular methods
    compute_method_overloads_context_by_type(
        interface, [method for method in methods if not method['is_static']])
    # Static methods
    compute_method_overloads_context_by_type(
        interface, [method for method in methods if method['is_static']])


def compute_method_overloads_context_by_type(interface, methods):
    """Computes |method.overload*| template values.

    Called separately for static and non-static (regular) methods,
    as these are overloaded separately.
    Modifies |method| in place for |method| in |methods|.
    Doesn't change the |methods| list itself (only the values, i.e. individual
    methods), so ok to treat these separately.
    """
    # Add overload information only to overloaded methods, so template code can
    # easily verify if a function is overloaded
    for name, overloads in method_overloads_by_name(methods):
        # Resolution function is generated after last overloaded function;
        # package necessary information into |method.overloads| for that method.
        overloads[-1]['overloads'] = overloads_context(interface, overloads)
        overloads[-1]['overloads']['name'] = name
        overloads[-1]['overloads']['camel_case_name'] = NameStyleConverter(name).to_upper_camel_case()


def overloads_context(interface, overloads):
    """Returns |overloads| template values for a single name.

    Sets |method.overload_index| in place for |method| in |overloads|
    and returns dict of overall overload template values.
    """
    assert len(overloads) > 1  # only apply to overloaded names
    for index, method in enumerate(overloads, 1):
        method['overload_index'] = index

    # [RuntimeEnabled]
    # TODO(iclelland): Allow origin trials on method overloads
    # (crbug.com/621641)
    if any(method.get('origin_trial_feature_name') for method in overloads):
        raise Exception('[RuntimeEnabled] for origin trial cannot be specified on '
                        'overloaded methods: %s.%s' % (interface.name, overloads[0]['name']))

    effective_overloads_by_length = effective_overload_set_by_length(overloads)
    lengths = [length for length, _ in effective_overloads_by_length]
    name = overloads[0].get('name', '<constructor>')
    camel_case_name = NameStyleConverter(name).to_upper_camel_case()

    runtime_determined_lengths = None
    function_length = lengths[0]
    runtime_determined_maxargs = None
    maxarg = lengths[-1]

    # The special case handling below is not needed if all overloads are
    # runtime enabled by the same feature.
    if not common_value(overloads, 'runtime_enabled_feature_name'):
        # Check if all overloads with the shortest acceptable arguments list are
        # runtime enabled, in which case we need to have a runtime determined
        # Function.length.
        shortest_overloads = effective_overloads_by_length[0][1]
        if (all(method.get('runtime_enabled_feature_name')
                for method, _, _ in shortest_overloads)):
            # Generate a list of (length, runtime_enabled_feature_names) tuples.
            runtime_determined_lengths = []
            for length, effective_overloads in effective_overloads_by_length:
                runtime_enabled_feature_names = set(
                    method['runtime_enabled_feature_name']
                    for method, _, _ in effective_overloads)
                if None in runtime_enabled_feature_names:
                    # This "length" is unconditionally enabled, so stop here.
                    runtime_determined_lengths.append((length, [None]))
                    break
                runtime_determined_lengths.append(
                    (length, sorted(runtime_enabled_feature_names)))
            function_length = ('%s::%sMethodLength()'
                               % (internal_namespace(interface), camel_case_name))

        # Check if all overloads with the longest required arguments list are
        # runtime enabled, in which case we need to have a runtime determined
        # maximum distinguishing argument index.
        longest_overloads = effective_overloads_by_length[-1][1]
        if (not common_value(overloads, 'runtime_enabled_feature_name') and
                all(method.get('runtime_enabled_feature_name')
                    for method, _, _ in longest_overloads)):
            # Generate a list of (length, runtime_enabled_feature_name) tuples.
            runtime_determined_maxargs = []
            for length, effective_overloads in reversed(effective_overloads_by_length):
                runtime_enabled_feature_names = set(
                    method['runtime_enabled_feature_name']
                    for method, _, _ in effective_overloads
                    if method.get('runtime_enabled_feature_name'))
                if not runtime_enabled_feature_names:
                    # This "length" is unconditionally enabled, so stop here.
                    runtime_determined_maxargs.append((length, [None]))
                    break
                runtime_determined_maxargs.append(
                    (length, sorted(runtime_enabled_feature_names)))
            maxarg = ('%s::%sMethodMaxArg()' %
                      (internal_namespace(interface), camel_case_name))

    # Check and fail if overloads disagree about whether the return type
    # is a Promise or not.
    promise_overload_count = sum(1 for method in overloads if method.get('returns_promise'))
    if promise_overload_count not in (0, len(overloads)):
        raise ValueError('Overloads of %s have conflicting Promise/non-Promise types'
                         % (name))

    has_overload_visible = False
    has_overload_not_visible = False
    for overload in overloads:
        if overload.get('visible', True):
            # If there exists an overload which is visible, need to generate
            # overload_resolution, i.e. overlods_visible should be True.
            has_overload_visible = True
        else:
            has_overload_not_visible = True

    # If some overloads are not visible and others are visible,
    # the method is overloaded between core and modules.
    has_partial_overloads = has_overload_visible and has_overload_not_visible

    return {
        'deprecate_all_as': common_value(overloads, 'deprecate_as'),  # [DeprecateAs]
        'exposed_test_all': common_value(overloads, 'exposed_test'),  # [Exposed]
        'length': function_length,
        'length_tests_methods': length_tests_methods(effective_overloads_by_length),
        # 1. Let maxarg be the length of the longest type list of the
        # entries in S.
        'maxarg': maxarg,
        'measure_all_as': common_value(overloads, 'measure_as'),  # [MeasureAs]
        'returns_promise_all': promise_overload_count > 0,
        'runtime_determined_lengths': runtime_determined_lengths,
        'runtime_determined_maxargs': runtime_determined_maxargs,
        'runtime_enabled_all': common_value(overloads, 'runtime_enabled_feature_name'),  # [RuntimeEnabled]
        'secure_context_test_all': common_value(overloads, 'secure_context_test'),  # [SecureContext]
        'valid_arities': (lengths
                          # Only need to report valid arities if there is a gap in the
                          # sequence of possible lengths, otherwise invalid length means
                          # "not enough arguments".
                          if lengths[-1] - lengths[0] != len(lengths) - 1 else None),
        'visible': has_overload_visible,
        'has_partial_overloads': has_partial_overloads,
    }



def distinguishing_argument_index(entries):
    """Returns the distinguishing argument index for a sequence of entries.

    Entries are elements of the effective overload set with the same number
    of arguments (formally, same type list length), each a 3-tuple of the form
    (callable, type list, optionality list).

    Spec: http://heycam.github.io/webidl/#dfn-distinguishing-argument-index

    If there is more than one entry in an effective overload set that has a
    given type list length, then for those entries there must be an index i
    such that for each pair of entries the types at index i are
    distinguishable.
    The lowest such index is termed the distinguishing argument index for the
    entries of the effective overload set with the given type list length.
    """
    # Only applicable “If there is more than one entry”
    assert len(entries) > 1

    def typename_without_nullable(idl_type):
        if idl_type.is_nullable:
            return idl_type.inner_type.name
        return idl_type.name

    type_lists = [tuple(typename_without_nullable(idl_type)
                        for idl_type in entry[1])
                  for entry in entries]
    type_list_length = len(type_lists[0])
    # Only applicable for entries that “[have] a given type list length”
    assert all(len(type_list) == type_list_length for type_list in type_lists)
    name = entries[0][0].get('name', 'Constructor')  # for error reporting

    # The spec defines the distinguishing argument index by conditions it must
    # satisfy, but does not give an algorithm.
    #
    # We compute the distinguishing argument index by first computing the
    # minimum index where not all types are the same, and then checking that
    # all types in this position are distinguishable (and the optionality lists
    # up to this point are identical), since "minimum index where not all types
    # are the same" is a *necessary* condition, and more direct to check than
    # distinguishability.
    types_by_index = (set(types) for types in zip(*type_lists))
    try:
        # “In addition, for each index j, where j is less than the
        #  distinguishing argument index for a given type list length, the types
        #  at index j in all of the entries’ type lists must be the same”
        index = next(i for i, types in enumerate(types_by_index)
                     if len(types) > 1)
    except StopIteration:
        raise ValueError('No distinguishing index found for %s, length %s:\n'
                         'All entries have the same type list:\n'
                         '%s' % (name, type_list_length, type_lists[0]))
    # Check optionality
    # “and the booleans in the corresponding list indicating argument
    #  optionality must be the same.”
    # FIXME: spec typo: optionality value is no longer a boolean
    # https://www.w3.org/Bugs/Public/show_bug.cgi?id=25628
    initial_optionality_lists = set(entry[2][:index] for entry in entries)
    if len(initial_optionality_lists) > 1:
        raise ValueError(
            'Invalid optionality lists for %s, length %s:\n'
            'Optionality lists differ below distinguishing argument index %s:\n'
            '%s'
            % (name, type_list_length, index, set(initial_optionality_lists)))

    # Check distinguishability
    # http://heycam.github.io/webidl/#dfn-distinguishable
    # Use names to check for distinct types, since objects are distinct
    # FIXME: check distinguishability more precisely, for validation
    distinguishing_argument_type_names = [type_list[index]
                                          for type_list in type_lists]
    if (len(set(distinguishing_argument_type_names)) !=
            len(distinguishing_argument_type_names)):
        raise ValueError('Types in distinguishing argument are not distinct:\n'
                         '%s' % distinguishing_argument_type_names)

    return index


def length_tests_methods(effective_overloads_by_length):
    """Returns sorted list of resolution tests and associated methods, by length.

    This builds the main data structure for the overload resolution loop.
    For a given argument length, bindings test argument at distinguishing
    argument index, in order given by spec: if it is compatible with
    (optionality or) type required by an overloaded method, resolve to that
    method.

    Returns:
        [(length, [(test, method)])]
    """
    return [(length, list(resolution_tests_methods(effective_overloads)))
            for length, effective_overloads in effective_overloads_by_length]


def resolution_tests_methods(effective_overloads):
    """Yields resolution test and associated method, in resolution order, for effective overloads of a given length.

    This is the heart of the resolution algorithm.
    https://heycam.github.io/webidl/#dfn-overload-resolution-algorithm

    Note that a given method can be listed multiple times, with different tests!
    This is to handle implicit type conversion.

    Returns:
        [(test, method)]
    """
    methods = [effective_overload[0]
               for effective_overload in effective_overloads]
    if len(methods) == 1:
        # If only one method with a given length, no test needed
        yield 'true', methods[0]
        return

    # 8. If there is more than one entry in S, then set d to be the
    # distinguishing argument index for the entries of S.
    index = distinguishing_argument_index(effective_overloads)

    # (11. is for handling |undefined| values for optional arguments
    #  before the distinguishing argument (as “missing”).)
    # TODO(peria): We have to handle this step. Also in 15.4.2.

    # 12. If i = d, then:
    # 12.1. Let V be args[i].
    cpp_value = 'info[%s]' % index

    # Extract argument and IDL type to simplify accessing these in each loop.
    arguments = [method['arguments'][index] for method in methods]
    arguments_methods = zip(arguments, methods)
    idl_types = [argument['idl_type_object'] for argument in arguments]
    idl_types_methods = zip(idl_types, methods)

    # We can’t do a single loop through all methods or simply sort them, because
    # a method may be listed in multiple steps of the resolution algorithm, and
    # which test to apply differs depending on the step.
    #
    # Instead, we need to go through all methods at each step, either finding
    # first match (if only one test is allowed) or filtering to matches (if
    # multiple tests are allowed), and generating an appropriate tests.
    #
    # In listing types, we put ellipsis (...) for shorthand nullable type(s),
    # annotated type(s), and a (nullable/annotated) union type, which extend
    # listed types.
    # TODO(peria): Support handling general union types. https://crbug.com/838787

    # 12.2. If V is undefined, and there is an entry in S whose list of
    # optionality values has “optional” at index i, then remove from S all
    # other entries.
    try:
        method = next(method for argument, method in arguments_methods
                      if argument['is_optional'])
        test = '%s->IsUndefined()' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 12.3. Otherwise: if V is null or undefined, and there is an entry in S that
    # has one of the following types at position i of its type list,
    # • a nullable type
    # • a dictionary type
    # ...
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_nullable or idl_type.is_dictionary)
        test = 'IsUndefinedOrNull(%s)' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 12.4. Otherwise: if V is a platform object, and there is an entry in S that
    # has one of the following types at position i of its type list,
    # • an interface type that V implements
    # ...
    for idl_type, method in idl_types_methods:
        if idl_type.is_wrapper_type and not idl_type.is_array_buffer_or_view:
            test = 'V8{idl_type}::HasInstance({cpp_value}, info.GetIsolate())'.format(
                idl_type=idl_type.base_type, cpp_value=cpp_value)
            yield test, method

    # 12.5. Otherwise: if V is a DOMException platform object and there is an entry
    # in S that has one of the following types at position i of its type list,
    # • DOMException
    # • Error
    # ...
    # (DOMException is handled in 12.4, and we don't support Error type.)

    # 12.6. Otherwise: if Type(V) is Object, V has an [[ErrorData]] internal slot,
    # and there is an entry in S that has one of the following types at position
    # i of its type list,
    # • Error
    # ...
    # (We don't support Error type.)

    # 12.7. Otherwise: if Type(V) is Object, V has an [[ArrayBufferData]] internal
    # slot, and there is an entry in S that has one of the following types at
    # position i of its type list,
    # • ArrayBuffer
    # ...
    for idl_type, method in idl_types_methods:
        if idl_type.is_array_buffer_or_view or idl_type.is_typed_array:
            test = '{cpp_value}->Is{idl_type}()'.format(
                idl_type=idl_type.base_type, cpp_value=cpp_value)
            yield test, method

    # 12.8. Otherwise: if Type(V) is Object, V has a [[DataView]] internal slot,
    # and there is an entry in S that has one of the following types at position
    # i of its type list,
    # • DataView
    # ...
    # (DataView is included in 12.7.)

    # 12.9. Otherwise: if Type(V) is Object, V has a [[TypedArrayName]] internal
    # slot, and there is an entry in S that has one of the following types at
    # position i of its type list,
    # • a typed array type whose name is equal to the value of V’s
    #   [[TypedArrayName]] internal slot
    # ...
    # (TypedArrays are included in 12.7.)

    # 12.10. Otherwise: if IsCallable(V) is true, and there is an entry in S that
    # has one of the following types at position i of its type list,
    # • a callback function type
    # ...
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_callback_function)
        test = '%s->IsFunction()' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 12.11. Otherwise: if Type(V) is Object and there is an entry in S that has
    # one of the following types at position i of its type list,
    # • a sequence type
    # • a frozen array type
    # ...
    # and after performing the following steps,
    # 12.11.1. Let method be ? GetMethod(V, @@iterator).
    # method is not undefined, then remove from S all other entries.
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.native_array_element_type)
        # Either condition should be fulfilled to call this |method|.
        test = '%s->IsArray()' % cpp_value
        yield test, method
        test = 'HasCallableIteratorSymbol(info.GetIsolate(), %s, exception_state)' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 12.12. Otherwise: if Type(V) is Object and there is an entry in S that has
    # one of the following types at position i of its type list,
    # • a callback interface type
    # • a dictionary type
    # • a record type
    # ...
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_callback_interface or
                      idl_type.is_dictionary or idl_type.name == 'Dictionary' or
                      idl_type.is_record_type)
        test = '%s->IsObject()' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 12.13. Otherwise: if Type(V) is Boolean and there is an entry in S that has
    # one of the following types at position i of its type list,
    # • boolean
    # ...
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.name == 'Boolean')
        test = '%s->IsBoolean()' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 12.14. Otherwise: if Type(V) is Number and there is an entry in S that has
    # one of the following types at position i of its type list,
    # • a numeric type
    # ...
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_numeric_type)
        test = '%s->IsNumber()' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 12.15. Otherwise: if there is an entry in S that has one of the following
    # types at position i of its type list,
    # • a string type
    # ...
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_string_type or idl_type.is_enum
                      or (idl_type.is_union_type and idl_type.string_member_type))
        yield 'true', method
    except StopIteration:
        pass

    # 12.16. Otherwise: if there is an entry in S that has one of the following
    # types at position i of its type list,
    # • a numeric type
    # ...
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_numeric_type)
        yield 'true', method
    except StopIteration:
        pass

    # 12.17. Otherwise: if there is an entry in S that has one of the following
    # types at position i of its type list,
    # • boolean
    # ...
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.name == 'Boolean')
        yield 'true', method
    except StopIteration:
        pass


################################################################################
# Utility functions
################################################################################

def common(dicts, f):
    """Returns common result of f across an iterable of dicts, or None.

    Call f for each dict and return its result if the same across all dicts.
    """
    values = (f(d) for d in dicts)
    first_value = next(values)
    if all(value == first_value for value in values):
        return first_value
    return None


def common_key(dicts, key):
    """Returns common presence of a key across an iterable of dicts, or None.

    True if all dicts have the key, False if none of the dicts have the key,
    and None if some but not all dicts have the key.
    """
    return common(dicts, lambda d: key in d)


def common_value(dicts, key):
    """Returns common value of a key across an iterable of dicts, or None.

    Auxiliary function for overloads, so can consolidate an extended attribute
    that appears with the same value on all items in an overload set.
    """
    return common(dicts, lambda d: d.get(key))


def internal_namespace(interface):
    return (v8_utilities.to_snake_case(cpp_name_or_partial(interface)) +
            '_v8_internal')


################################################################################
# Constructors
################################################################################

# [Constructor]
def constructor_context(interface, constructor):
    # [RaisesException=Constructor]
    is_constructor_raises_exception = \
        interface.extended_attributes.get('RaisesException') == 'Constructor'

    argument_contexts = [
        v8_methods.argument_context(interface, constructor, argument, index)
        for index, argument in enumerate(constructor.arguments)]

    return {
        'arguments': argument_contexts,
        'cpp_type': cpp_name(interface) + '*',
        'cpp_value': v8_methods.cpp_value(
            interface, constructor, len(constructor.arguments)),
        'has_exception_state':
            is_constructor_raises_exception or
            any(argument for argument in constructor.arguments
                if argument.idl_type.name == 'SerializedScriptValue' or
                argument.idl_type.v8_conversion_needs_exception_state),
        'has_optional_argument_without_default_value':
            any(True for argument_context in argument_contexts
                if argument_context['is_optional_without_default_value']),
        'is_call_with_document':
            # [ConstructorCallWith=Document]
            has_extended_attribute_value(interface,
                                         'ConstructorCallWith', 'Document'),
        'is_call_with_execution_context':
            # [ConstructorCallWith=ExecutionContext]
            has_extended_attribute_value(interface,
                                         'ConstructorCallWith', 'ExecutionContext'),
        'is_call_with_script_state':
            # [ConstructorCallWith=ScriptState]
            has_extended_attribute_value(
                interface, 'ConstructorCallWith', 'ScriptState'),
        'is_constructor': True,
        'is_named_constructor': False,
        'is_raises_exception': is_constructor_raises_exception,
        'number_of_required_arguments':
            number_of_required_arguments(constructor),
        'rcs_counter': 'Blink_' + v8_utilities.cpp_name(interface) + '_ConstructorCallback'
    }


# [NamedConstructor]
def named_constructor_context(interface):
    extended_attributes = interface.extended_attributes
    if 'NamedConstructor' not in extended_attributes:
        return None
    # FIXME: parser should return named constructor separately;
    # included in constructors (and only name stored in extended attribute)
    # for Perl compatibility
    idl_constructor = interface.constructors[-1]
    assert idl_constructor.name == 'NamedConstructor'
    context = constructor_context(interface, idl_constructor)
    context.update({
        'name': extended_attributes['NamedConstructor'],
        'is_named_constructor': True,
    })
    return context


def number_of_required_arguments(constructor):
    return len([argument for argument in constructor.arguments
                if not (argument.is_optional or argument.is_variadic)])


def interface_length(constructors):
    # Docs: http://heycam.github.io/webidl/#es-interface-call
    if not constructors:
        return 0
    return min(constructor['number_of_required_arguments']
               for constructor in constructors)


################################################################################
# Special operations (methods)
# http://heycam.github.io/webidl/#idl-special-operations
################################################################################

def property_getter(getter, cpp_arguments):
    if not getter:
        return None

    def is_null_expression(idl_type):
        if idl_type.use_output_parameter_for_result or idl_type.is_string_type:
            return 'result.IsNull()'
        if idl_type.is_interface_type:
            return '!result'
        if idl_type.base_type in ('any', 'object'):
            return 'result.IsEmpty()'
        return ''

    extended_attributes = getter.extended_attributes
    has_no_side_effect = v8_utilities.has_extended_attribute_value(getter, 'Affects', 'Nothing')
    idl_type = getter.idl_type
    idl_type.add_includes_for_type(extended_attributes)
    is_call_with_script_state = v8_utilities.has_extended_attribute_value(getter, 'CallWith', 'ScriptState')
    is_raises_exception = 'RaisesException' in extended_attributes
    use_output_parameter_for_result = idl_type.use_output_parameter_for_result

    # FIXME: make more generic, so can use v8_methods.cpp_value
    cpp_method_name = 'impl->%s' % cpp_name(getter)

    if is_call_with_script_state:
        cpp_arguments.insert(0, 'script_state')
    if is_raises_exception:
        cpp_arguments.append('exception_state')
    if use_output_parameter_for_result:
        cpp_arguments.append('result')

    cpp_value = '%s(%s)' % (cpp_method_name, ', '.join(cpp_arguments))

    return {
        'cpp_type': idl_type.cpp_type,
        'cpp_value': cpp_value,
        'has_no_side_effect': has_no_side_effect,
        'is_call_with_script_state': is_call_with_script_state,
        'is_cross_origin': 'CrossOrigin' in extended_attributes,
        'is_custom':
            'Custom' in extended_attributes and
            (not extended_attributes['Custom'] or
             has_extended_attribute_value(getter, 'Custom', 'PropertyGetter')),
        'is_custom_property_enumerator': has_extended_attribute_value(
            getter, 'Custom', 'PropertyEnumerator'),
        'is_custom_property_query': has_extended_attribute_value(
            getter, 'Custom', 'PropertyQuery'),
        # TODO(rakuco): [NotEnumerable] does not make sense here and is only
        # used in non-standard IDL operations. We need to get rid of them.
        'is_enumerable': 'NotEnumerable' not in extended_attributes,
        'is_null_expression': is_null_expression(idl_type),
        'is_raises_exception': is_raises_exception,
        'name': cpp_name(getter),
        'use_output_parameter_for_result': use_output_parameter_for_result,
        'v8_set_return_value': idl_type.v8_set_return_value('result', extended_attributes=extended_attributes, script_wrappable='impl'),
    }


def property_setter(setter, interface):
    if not setter:
        return None

    extended_attributes = setter.extended_attributes
    idl_type = setter.arguments[1].idl_type
    idl_type.add_includes_for_type(extended_attributes)
    is_call_with_script_state = v8_utilities.has_extended_attribute_value(setter, 'CallWith', 'ScriptState')
    is_raises_exception = 'RaisesException' in extended_attributes
    is_ce_reactions = 'CEReactions' in extended_attributes

    has_type_checking_interface = idl_type.is_wrapper_type

    return {
        'has_exception_state': (is_raises_exception or
                                idl_type.v8_conversion_needs_exception_state),
        'has_type_checking_interface': has_type_checking_interface,
        'idl_type': idl_type.base_type,
        'is_call_with_script_state': is_call_with_script_state,
        'is_ce_reactions': is_ce_reactions,
        'is_custom': 'Custom' in extended_attributes,
        'is_nullable': idl_type.is_nullable,
        'is_raises_exception': is_raises_exception,
        'name': cpp_name(setter),
        'v8_value_to_local_cpp_value': idl_type.v8_value_to_local_cpp_value(
            extended_attributes, 'v8_value', 'property_value'),
    }


def property_deleter(deleter):
    if not deleter:
        return None

    extended_attributes = deleter.extended_attributes
    is_call_with_script_state = v8_utilities.has_extended_attribute_value(deleter, 'CallWith', 'ScriptState')
    is_ce_reactions = 'CEReactions' in extended_attributes
    return {
        'is_call_with_script_state': is_call_with_script_state,
        'is_ce_reactions': is_ce_reactions,
        'is_custom': 'Custom' in extended_attributes,
        'is_raises_exception': 'RaisesException' in extended_attributes,
        'name': cpp_name(deleter),
    }
