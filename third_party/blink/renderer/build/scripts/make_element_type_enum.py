#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import template_expander


def _symbol(tag):
    return 'k' + tag['name'].to_upper_camel_case()


class MakeElementTypeEnumWriter(json5_generator.Writer):
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

    def __init__(self, json5_file_paths, output_dir):
        super(MakeElementTypeEnumWriter, self).__init__(None, output_dir)

        self._input_files = json5_file_paths
        self._outputs = {
            ('element_type_enum.h'): self.generate_enum_header,
        }

        self._template_context = {
            'input_files': self._input_files,
            'elements': set(),
        }
        elements = self._template_context['elements']

        for filename in json5_file_paths:
            json5_file = json5_generator.Json5File.load_from_files(
                [filename], self.default_metadata, self.default_parameters)
            namespace = json5_file.metadata['namespace'].strip('"')
            for tag in json5_file.name_dictionaries:
                tag['interface'] = self._interface(namespace, tag)
                elements.add(tag['interface'])

    @template_expander.use_jinja("templates/element_type_enum.h.tmpl",
                                 filters=filters)
    def generate_enum_header(self):
        return self._template_context

    def _interface(self, namespace, tag):
        if tag['interfaceName']:
            return tag['interfaceName']
        name = tag['name'].to_upper_camel_case()
        # FIXME: We shouldn't hard-code HTML here.
        if name == 'HTML':
            name = 'Html'
        return '%s%sElement' % (namespace, name)


if __name__ == "__main__":
    json5_generator.Maker(MakeElementTypeEnumWriter).main()
