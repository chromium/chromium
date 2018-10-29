#!/usr/bin/env python
# Copyright (C) 2013 Google Inc. All rights reserved.
# Copyright (C) 2013 Igalia S.L. All rights reserved.
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

import sys
import json5_generator
import template_expander
import name_utilities
from make_settings import to_passing_type, to_idl_type


class MakeInternalSettingsWriter(json5_generator.Writer):
    filters = {
        'to_passing_type': to_passing_type,
        'to_idl_type': to_idl_type,
    }
    _FILE_BASENAME = 'internal_settings_generated'

    def __init__(self, json5_file_path, output_dir):
        super(MakeInternalSettingsWriter, self).__init__(json5_file_path, output_dir)

        self.json5_file.name_dictionaries.sort(key=lambda entry: entry['name'].original)

        self._outputs = {
            (self._FILE_BASENAME + '.h'): self.generate_header,
            (self._FILE_BASENAME + '.cc'): self.generate_implementation,
            (self._FILE_BASENAME + '.idl'): self.generate_idl,
        }
        self._template_context = {
            'input_files': self._input_files,
            'primary_header_name': 'internal_settings_generated.h',
            'settings': self.json5_file.name_dictionaries,
            'header_guard': self.make_header_guard(self._relative_output_dir + self._FILE_BASENAME + '.h')
        }

    @template_expander.use_jinja('templates/internal_settings_generated.h.tmpl', filters=filters)
    def generate_header(self):
        return self._template_context

    @template_expander.use_jinja('templates/internal_settings_generated.cc.tmpl', filters=filters)
    def generate_implementation(self):
        return self._template_context

    @template_expander.use_jinja('templates/internal_settings_generated.idl.tmpl', filters=filters)
    def generate_idl(self):
        return self._template_context


if __name__ == '__main__':
    json5_generator.Maker(MakeInternalSettingsWriter).main()
