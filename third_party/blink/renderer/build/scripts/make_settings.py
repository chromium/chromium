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

import json5_generator
import name_utilities
import template_expander


def to_passing_type(typename):
    if typename == 'String':
        return 'const String&'
    return typename


def to_idl_type(typename):
    if typename == 'int':
        return 'long'
    if typename == 'unsigned' or typename == 'size_t':
        return 'unsigned long'
    if typename == 'String':
        return 'DOMString'
    if typename == 'bool':
        return 'boolean'
    if typename == 'double':
        return 'double'
    return None


class MakeSettingsWriter(json5_generator.Writer):
    filters = {
        'cpp_bool': name_utilities.cpp_bool,
        'to_passing_type': to_passing_type,
        'to_idl_type': to_idl_type,
    }

    def __init__(self, json5_file_path, output_dir):
        super(MakeSettingsWriter, self).__init__(json5_file_path, output_dir)

        self.json5_file.name_dictionaries.sort(
            key=lambda entry: entry['name'].original)

        for setting in self.json5_file.name_dictionaries:
            # If 'initial' is a dict, extract a list of specified platforms.
            if isinstance(setting['initial'], dict):
                assert 'default' in setting[
                    'initial'], "a 'default' initial value is required for '{}'".format(
                        setting['name'])
                setting[
                    'initial_platforms'] = self._platforms_with_initial_values(
                        setting['initial'])

        self._outputs = {
            'settings_base.cc': self.generate_cc,
            'settings_base.h': self.generate_h,
        }

    def _platforms_with_initial_values(self, initial):
        initial_platforms = set(initial.keys()) & set(self._all_platforms())
        return list(sorted(initial_platforms))

    def _all_platforms(self):
        # Remove all occurrences of 'default' from 'valid_keys'
        platforms = self.json5_file.parameters['initial']['valid_keys']
        return [platform for platform in platforms if platform != 'default']

    def _get_include_paths(self):
        include_paths = set()
        for setting in self.json5_file.name_dictionaries:
            include_paths.update(setting['include_paths'])
        return list(sorted(include_paths))

    @template_expander.use_jinja('templates/settings_base.cc.tmpl',
                                 filters=filters)
    def generate_cc(self):
        return {
            'input_files': self._input_files,
            'settings': self.json5_file.name_dictionaries,
        }

    @template_expander.use_jinja('templates/settings_base.h.tmpl',
                                 filters=filters)
    def generate_h(self):
        return {
            'input_files':
            self._input_files,
            'include_paths':
            self._get_include_paths(),
            'settings':
            self.json5_file.name_dictionaries,
            'header_guard':
            self.make_header_guard(self._relative_output_dir +
                                   'settings_base.h')
        }


if __name__ == '__main__':
    json5_generator.Maker(MakeSettingsWriter).main()
