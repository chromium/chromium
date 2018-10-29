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

import os
import sys

import hasher
import json5_generator
import template_expander
import name_utilities


def _legacy_symbol(entry):
    if entry['Symbol'] is not None:
        return entry['Symbol']
    # FIXME: Remove this special case for the ugly x-webkit-foo attributes.
    if entry['name'].original.startswith('-webkit-'):
        return entry['name'].original.replace('-', '_')[1:]
    return name_utilities.cpp_name(entry).replace('-', '_').replace(' ', '_')


def _symbol(entry):
    if entry['Symbol'] is not None:
        return entry['Symbol']
    return 'k' + entry['name'].to_upper_camel_case()


class MakeNamesWriter(json5_generator.Writer):
    default_parameters = {
        'Conditional': {},  # FIXME: Add support for Conditional.
        'ImplementedAs': {},
        'RuntimeEnabled': {},  # What should we do for runtime-enabled features?
        'Symbol': {},
    }
    default_metadata = {
        'export': '',
        'namespace': '',
        'suffix': '',
    }
    filters = {
        'cpp_name': name_utilities.cpp_name,
        'hash': hasher.hash,
        'script_name': name_utilities.script_name,
        'symbol': _symbol,
    }

    def __init__(self, json5_file_path, output_dir):
        super(MakeNamesWriter, self).__init__(json5_file_path, output_dir)

        namespace = self.json5_file.metadata['namespace'].strip('"')
        suffix = self.json5_file.metadata['suffix'].strip('"')
        export = self.json5_file.metadata['export'].strip('"')

        assert namespace, 'A namespace is required.'
        # TODO(tkent): Remove the following condition.  Namespace fields of all
        # foo_names.json5 should be lower-cased.  crbug.com/889726
        if namespace.lower() != namespace:
            namespace = namespace + 'Names'
            MakeNamesWriter.filters['symbol'] = _legacy_symbol

        basename, _ = os.path.splitext(os.path.basename(json5_file_path[0]))
        self._outputs = {
            (basename + '.h'): self.generate_header,
            (basename + '.cc'): self.generate_implementation,
        }
        qualified_header = self._relative_output_dir + basename + '.h'
        self._template_context = {
            'namespace': namespace,
            'suffix': suffix,
            'export': export,
            'entries': self.json5_file.name_dictionaries,
            'header_guard': self.make_header_guard(qualified_header),
            'input_files': self._input_files,
            'this_include_path': qualified_header,
        }

    @template_expander.use_jinja("templates/make_names.h.tmpl", filters=filters)
    def generate_header(self):
        return self._template_context

    @template_expander.use_jinja("templates/make_names.cc.tmpl", filters=filters)
    def generate_implementation(self):
        return self._template_context


if __name__ == "__main__":
    json5_generator.Maker(MakeNamesWriter).main()
