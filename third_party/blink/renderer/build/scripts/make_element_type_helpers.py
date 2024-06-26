#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict

import json5_generator
import template_expander

from blinkbuild.name_style_converter import NameStyleConverter


def _symbol(tag):
    return 'k' + tag['name'].to_upper_camel_case()


class MakeElementTypeHelpersWriter(json5_generator.Writer):
    default_parameters = {
        'Conditional': {},
        'ImplementedAs': {},
        'JSInterfaceName': {},
        'constructorNeedsCreateElementFlags': {},
        'interfaceHeaderDir': {},
        'interfaceName': {},
        'noConstructor': {},
        'noTypeHelpers': {},
        'runtimeEnabled': {},
        'runtimeFlagHasOriginTrial': {},
    }
    default_metadata = {
        'attrsNullNamespace': None,
        'export': '',
        'fallbackInterfaceName': '',
        'fallbackJSInterfaceName': '',
        'namespace': '',
        'namespacePrefix': '',
        'namespaceURI': '',
    }
    filters = {
        'symbol': _symbol,
    }

    def __init__(self, json5_file_path, output_dir):
        super(MakeElementTypeHelpersWriter, self).__init__(
            json5_file_path, output_dir)

        self.namespace = self.json5_file.metadata['namespace'].strip('"')
        self.fallback_interface = self.json5_file.metadata[
            'fallbackInterfaceName'].strip('"')

        assert self.namespace, 'A namespace is required.'

        basename = self.namespace.lower() + '_element_type_helpers'
        self._outputs = {
            (basename + '.h'): self.generate_helper_header,
            (basename + '.cc'): self.generate_helper_implementation,
        }

        base_element_header = 'third_party/blink/renderer/core/{}/{}_element.h'.format(
            self.namespace.lower(),
            NameStyleConverter(self.namespace).to_snake_case())
        self._template_context = {
            'base_element_header': base_element_header,
            'cpp_namespace': self.namespace.lower() + '_names',
            'input_files': self._input_files,
            'namespace': self.namespace,
            'tags': self.json5_file.name_dictionaries,
            'elements': set(),
        }

        tags = self._template_context['tags']
        elements = self._template_context['elements']
        interface_counts = defaultdict(int)
        for tag in tags:
            tag['interface'] = self._interface(tag)
            interface_counts[tag['interface']] += 1
            tag['js_interface'] = tag['interface']
            if tag['JSInterfaceName']:
                tag['js_interface'] = tag['JSInterfaceName']
            elements.add(tag['js_interface'])

        for tag in tags:
            tag['multipleTagNames'] = (
                interface_counts[tag['interface']] > 1
                or tag['interface'] == self.fallback_interface)

    @template_expander.use_jinja(
        "templates/element_type_helpers.h.tmpl", filters=filters)
    def generate_helper_header(self):
        return self._template_context

    @template_expander.use_jinja(
        "templates/element_type_helpers.cc.tmpl", filters=filters)
    def generate_helper_implementation(self):
        return self._template_context

    def _interface(self, tag):
        if tag['interfaceName']:
            return tag['interfaceName']
        name = tag['name'].to_upper_camel_case()
        # FIXME: We shouldn't hard-code HTML here.
        if name == 'HTML':
            name = 'Html'
        return '%s%sElement' % (self.namespace, name)


if __name__ == "__main__":
    json5_generator.Maker(MakeElementTypeHelpersWriter).main()
