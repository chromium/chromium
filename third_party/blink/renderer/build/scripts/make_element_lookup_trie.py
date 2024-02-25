#!/usr/bin/env python
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

from name_utilities import tag_symbol_for_entry
import json5_generator
import trie_builder
import template_expander


class ElementLookupTrieWriter(json5_generator.Writer):
    # TODO(https://crbug.com/1338945): Inherit all these from somewhere.
    default_parameters = {
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

    def __init__(self, json5_file_paths, output_dir):
        super(ElementLookupTrieWriter, self).__init__(json5_file_paths,
                                                      output_dir)
        self._tags = {}
        self._tag_symbols = {}
        self._runtimeEnabledWithoutOriginTrial = {}
        for entry in self.json5_file.name_dictionaries:
            tagname = entry['name'].original
            self._tags[tagname] = tagname
            self._tag_symbols[tagname] = tag_symbol_for_entry(entry)
            if 'runtimeEnabled' in entry and not entry.get(
                    'runtimeFlagHasOriginTrial', False):
                self._runtimeEnabledWithoutOriginTrial[tagname] = entry[
                    'runtimeEnabled']
        self._namespace = self.json5_file.metadata['namespace'].strip('"')
        basename = self._namespace.lower() + '_element_lookup_trie'
        self._outputs = {
            (basename + '.h'): self.generate_header,
            (basename + '.cc'): self.generate_implementation,
        }

    @template_expander.use_jinja('templates/element_lookup_trie.h.tmpl')
    def generate_header(self):
        return {
            'input_files': self._input_files,
            'namespace': self._namespace,
        }

    @template_expander.use_jinja('templates/element_lookup_trie.cc.tmpl')
    def generate_implementation(self):
        return {
            'input_files': self._input_files,
            'namespace': self._namespace,
            'length_tries': trie_builder.trie_list_by_str_length(self._tags),
            'runtimeEnabledWithoutOriginTrial':
            self._runtimeEnabledWithoutOriginTrial,
            'tag_symbols': self._tag_symbols,
        }


if __name__ == '__main__':
    json5_generator.Maker(ElementLookupTrieWriter).main()
