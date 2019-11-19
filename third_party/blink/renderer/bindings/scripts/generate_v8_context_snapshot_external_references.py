# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=relative-import

import argparse
import os
import posixpath

from code_generator import initialize_jinja_env
from idl_reader import IdlReader
from utilities import create_component_info_provider, write_file
import utilities
import v8_attributes
import v8_interface
import v8_types
import v8_utilities


INCLUDES = frozenset([
    'third_party/blink/renderer/bindings/core/v8/generated_code_helper.h',
    'third_party/blink/renderer/bindings/core/v8/v8_html_document.h',
    'third_party/blink/renderer/bindings/core/v8/v8_initializer.h',
    'third_party/blink/renderer/bindings/core/v8/v8_window.h',
    'third_party/blink/renderer/platform/bindings/v8_object_constructor.h',
    'v8/include/v8.h'])

TEMPLATE_FILE = 'external_reference_table.cc.tmpl'

SNAPSHOTTED_INTERFACES = frozenset([
    'Window',
    'EventTarget',
    'HTMLDocument',
    'Document',
    'Node',
])


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--idl-files-list', type=str, required=True,
                        help='file listing IDL files')
    parser.add_argument('--output', type=str, required=True,
                        help='output file path')
    parser.add_argument('--info-dir', type=str, required=True,
                        help='directory contains component info')
    parser.add_argument('--cache-dir', type=str, required=True,
                        help='cache directory')
    parser.add_argument('--target-component', type=str, required=True,
                        help='target component')
    return parser.parse_known_args()


# This class creates a Jinja template context about an interface.
class InterfaceTemplateContextBuilder(object):

    def __init__(self, opts, info_provider):
        self._opts = opts
        self._info_provider = info_provider

    def create_interface_context(self, interface, component, interfaces):
        '''Creates a Jinja context which is based on an interface.'''

        assert component in ['core', 'modules']

        name = '%s%s' % (v8_utilities.cpp_name(interface), 'Partial' if interface.is_partial else '')

        # Constructors
        has_constructor_callback = False
        if not interface.is_partial:
            constructors = any(constructor.name == 'Constructor' for constructor in interface.constructors)
            custom_constructors = interface.custom_constructors
            html_constructor = 'HTMLConstructor' in interface.extended_attributes
            has_constructor_callback = constructors or custom_constructors or html_constructor

        attributes = []
        methods = []
        has_cross_origin_indexed_getter = False
        has_cross_origin_named_enum = False
        has_cross_origin_named_getter = False
        has_cross_origin_named_setter = False
        has_security_check = False
        indexed_property_getter = None
        is_global = False
        named_property_getter = None
        component_info = self._info_provider.component_info
        if interface.name in SNAPSHOTTED_INTERFACES:
            attributes = [v8_attributes.attribute_context(interface, attribute, interfaces, component_info)
                          for attribute in interface.attributes]
            methods = v8_interface.methods_context(interface, component_info)['methods']
            is_global = 'Global' in interface.extended_attributes

            named_property_getter = v8_interface.property_getter(
                interface.named_property_getter, ['name'])
            indexed_property_getter = v8_interface.property_getter(
                interface.indexed_property_getter, ['index'])

            if not interface.is_partial:
                has_security_check = ('CheckSecurity' in interface.extended_attributes and
                                      interface.name != 'EventTarget')
                has_cross_origin_named_getter = (any(method['is_cross_origin'] for method in methods) or
                                                 any(attribute['has_cross_origin_getter'] for attribute in attributes))
                has_cross_origin_named_setter = any(attribute['has_cross_origin_setter'] for attribute in attributes)
                has_cross_origin_indexed_getter = indexed_property_getter and indexed_property_getter['is_cross_origin']
                has_cross_origin_named_enum = has_cross_origin_named_getter or has_cross_origin_named_setter
                if named_property_getter and named_property_getter['is_cross_origin']:
                    has_cross_origin_named_getter = True

        return {
            'attributes': attributes,
            'component': component,
            'has_constructor_callback': has_constructor_callback,
            'has_cross_origin_named_getter': has_cross_origin_named_getter,
            'has_cross_origin_named_setter': has_cross_origin_named_setter,
            'has_cross_origin_named_enumerator': has_cross_origin_named_enum,
            'has_cross_origin_indexed_getter': has_cross_origin_indexed_getter,
            'has_security_check': has_security_check,
            'indexed_property_getter': indexed_property_getter,
            'indexed_property_setter': v8_interface.property_setter(interface.indexed_property_setter, interface),
            'indexed_property_deleter': v8_interface.property_deleter(interface.indexed_property_deleter),
            'internal_namespace': v8_interface.internal_namespace(interface),
            'is_partial': interface.is_partial,
            'methods': methods,
            'name': name,
            'named_constructor': v8_interface.named_constructor_context(interface),
            'named_property_getter': named_property_getter,
            'named_property_setter': v8_interface.property_setter(interface.named_property_setter, interface),
            'named_property_deleter': v8_interface.property_deleter(interface.named_property_deleter),
            'v8_class': v8_utilities.v8_class_name_or_partial(interface),
        }


# This class applies a Jinja template and creates a .cpp file for the external reference table.
class ExternalReferenceTableGenerator(object):
    def __init__(self, opts, info_provider):
        self._opts = opts
        self._info_provider = info_provider
        self._reader = IdlReader(
            info_provider.interfaces_info, opts.cache_dir)
        self._interface_contexts = {}
        self._include_files = set(INCLUDES)
        v8_types.set_component_dirs(info_provider.interfaces_info['component_dirs'])

    # Creates a Jinja context from an IDL file.
    def process_idl_file(self, idl_filename):
        definitions = self._reader.read_idl_definitions(idl_filename)
        for component in definitions:
            target_definitions = definitions[component]
            interfaces = target_definitions.interfaces
            first_name = target_definitions.first_name
            if first_name in interfaces.keys():
                interface = interfaces[first_name]
                self._process_interface(interface, component, interfaces)

    # Creates a Jinja context from an interface. Some interfaces are not used
    # in V8 context snapshot, so we can skip them.
    def _process_interface(self, interface, component, interfaces):
        def has_impl(interface):
            component_info = self._info_provider.component_info
            runtime_features = component_info['runtime_enabled_features']
            # Non legacy callback interface does not provide V8 callbacks.
            if interface.is_callback:
                return len(interface.constants) > 0
            if v8_utilities.runtime_enabled_feature_name(interface, runtime_features):
                return False
            if 'Exposed' not in interface.extended_attributes:
                return True
            return any(exposure.exposed == 'Window' and exposure.runtime_enabled is None
                       for exposure in interface.extended_attributes['Exposed'])

        if not has_impl(interface):
            return

        context_builder = InterfaceTemplateContextBuilder(self._opts, self._info_provider)
        context = context_builder.create_interface_context(interface, component, interfaces)
        name = '%s%s' % (interface.name, 'Partial' if interface.is_partial else '')
        self._interface_contexts[name] = context

        # Do not include unnecessary header files.
        if not context['attributes'] and not context['named_property_setter']:
            return

        include_file = 'third_party/blink/renderer/bindings/%s/v8/%s.h' % (
            component, utilities.to_snake_case(context['v8_class']))
        self._include_files.add(include_file)

    # Gathers all interface-dependent information and returns as a Jinja template context.
    def _create_template_context(self):
        interfaces = []
        for name in sorted(self._interface_contexts):
            interfaces.append(self._interface_contexts[name])
        header_path = 'third_party/blink/renderer/bindings/modules/v8/v8_context_snapshot_external_references.h'
        include_files = list(self._include_files)
        return {
            'class': 'V8ContextSnapshotExternalReferences',
            'interfaces': interfaces,
            'include_files': sorted(include_files),
            'this_include_header_path': header_path,
            'code_generator': os.path.basename(__file__),
            'jinja_template_filename': TEMPLATE_FILE
        }

    # Applies a Jinja template on a context and generates a C++ code.
    def generate(self):
        jinja_env = initialize_jinja_env(self._opts.cache_dir)
        context = self._create_template_context()
        cpp_template = jinja_env.get_template(TEMPLATE_FILE)
        cpp_text = cpp_template.render(context)
        return cpp_text


def main():
    opts, _ = parse_args()
    # TODO(peria): get rid of |info_provider|
    info_provider = create_component_info_provider(
        opts.info_dir, opts.target_component)
    generator = ExternalReferenceTableGenerator(opts, info_provider)

    idl_files = utilities.read_idl_files_list_from_file(opts.idl_files_list)
    for idl_file in idl_files:
        generator.process_idl_file(idl_file)
    output_code = generator.generate()
    output_path = opts.output
    write_file(output_code, output_path)


if __name__ == '__main__':
    main()
