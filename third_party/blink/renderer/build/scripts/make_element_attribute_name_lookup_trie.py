#!/usr/bin/env python
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkbuild.name_style_converter import NameStyleConverter
import json5_generator
import trie_builder
import template_expander


class ElementAttributeNameLookupTrieWriter(json5_generator.Writer):
    # TODO(https://crbug.com/1338945): Unify common keys.
    # These are the names of the keys in the metadata section of the files.
    # The writer requires an entry for each key in the 'metadata' section of
    # the imported files.
    default_metadata = {
        'attrsNullNamespace': None,
        'export': '',
        'namespace': '',
        'namespacePrefix': '',
        'namespaceURI': '',
    }
    filters = {
        'symbol':
        lambda symbol: 'k' + NameStyleConverter(symbol).to_upper_camel_case()
    }

    def __init__(self, json5_file_paths, output_dir):
        super(ElementAttributeNameLookupTrieWriter,
              self).__init__(json5_file_paths, output_dir)
        self._names = {}
        for entry in self.json5_file.name_dictionaries:
            self._names[entry['name'].original] = entry['name'].original
        self._namespace = self.json5_file.metadata['namespace'].strip('"')
        basename = self._namespace.lower(
        ) + '_element_attribute_name_lookup_trie'
        self._outputs = {
            (basename + '.h'): self.generate_header,
            (basename + '.cc'): self.generate_implementation,
        }

    @template_expander.use_jinja(
        'templates/element_attribute_name_lookup_trie.h.tmpl')
    def generate_header(self):
        return {
            'input_files': self._input_files,
            'namespace': self._namespace,
        }

    @template_expander.use_jinja(
        'templates/element_attribute_name_lookup_trie.cc.tmpl',
        filters=filters)
    def generate_implementation(self):
        return {
            'input_files': self._input_files,
            'namespace': self._namespace,
            'length_tries': trie_builder.trie_list_by_str_length(self._names)
        }


if __name__ == '__main__':
    json5_generator.Maker(ElementAttributeNameLookupTrieWriter).main()
