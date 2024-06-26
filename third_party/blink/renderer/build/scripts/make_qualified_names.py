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

import copy

import json5_generator
import template_expander

from aria_properties import ARIAReader
from json5_generator import Json5File
from name_utilities import tag_symbol_for_entry


def _symbol(entry):
    return 'k' + entry['name'].to_upper_camel_case()


class MakeQualifiedNamesWriter(json5_generator.Writer):
    default_parameters = {}
    default_metadata = {
        'attrsNullNamespace': None,
        'export': '',
        'namespace': '',
        'namespacePrefix': '',
        'namespaceURI': '',
    }
    filters = {
        'symbol': _symbol,
        'tag_symbol': tag_symbol_for_entry,
    }

    def __init__(self, json5_file_paths, output_dir):
        super(MakeQualifiedNamesWriter, self).__init__(None, output_dir)
        self._input_files = copy.copy(json5_file_paths)
        assert len(json5_file_paths) <= 3, \
            'MakeQualifiedNamesWriter requires at most 3 in files, got %d.' % \
            len(json5_file_paths)

        # Input files are in a strict order with more optional files *first*:
        # 1) ARIA properties
        # 2) Tags
        # 3) Attributes

        if len(json5_file_paths) >= 3:
            aria_json5_filename = json5_file_paths.pop(0)
            self.aria_reader = ARIAReader(aria_json5_filename)
        else:
            self.aria_reader = None

        if len(json5_file_paths) >= 2:
            tags_json5_filename = json5_file_paths.pop(0)
            self.tags_json5_file = Json5File.load_from_files(
                [tags_json5_filename], self.default_metadata,
                self.default_parameters)
        else:
            self.tags_json5_file = None

        self.attrs_json5_file = Json5File.load_from_files(
            [json5_file_paths.pop()], self.default_metadata,
            self.default_parameters)

        if self.aria_reader is not None:
            self.attrs_json5_file.merge_from(
                self.aria_reader.attributes_list())

        self.namespace = self._metadata('namespace')
        cpp_namespace = self.namespace.lower() + '_names'
        namespace_prefix = self._metadata('namespacePrefix') or 'k'

        namespace_uri = self._metadata('namespaceURI')
        use_namespace_for_attrs = self.attrs_json5_file.metadata[
            'attrsNullNamespace'] is None

        self._outputs = {
            (self.namespace.lower() + "_names.h"): self.generate_header,
            (self.namespace.lower() + "_names.cc"):
            self.generate_implementation,
        }
        qualified_header = self._relative_output_dir + self.namespace.lower(
        ) + '_names.h'
        self._template_context = {
            'attrs':
            self.attrs_json5_file.name_dictionaries,
            'cpp_namespace':
            cpp_namespace,
            'export':
            self._metadata('export'),
            'header_guard':
            self.make_header_guard(qualified_header),
            'input_files':
            self._input_files,
            'namespace':
            self.namespace,
            'namespace_prefix':
            namespace_prefix,
            'namespace_uri':
            namespace_uri,
            'tags':
            self.tags_json5_file.name_dictionaries
            if self.tags_json5_file else [],
            'this_include_path':
            qualified_header,
            'use_namespace_for_attrs':
            use_namespace_for_attrs,
        }

    def _metadata(self, name):
        metadata = self.attrs_json5_file.metadata[name].strip('"')
        if self.tags_json5_file:
            assert metadata == self.tags_json5_file.metadata[name].strip(
                '"'), 'Both files must have the same %s.' % name
        return metadata

    @template_expander.use_jinja(
        'templates/make_qualified_names.h.tmpl', filters=filters)
    def generate_header(self):
        return self._template_context

    @template_expander.use_jinja(
        'templates/make_qualified_names.cc.tmpl', filters=filters)
    def generate_implementation(self):
        return self._template_context


if __name__ == "__main__":
    json5_generator.Maker(MakeQualifiedNamesWriter).main()
