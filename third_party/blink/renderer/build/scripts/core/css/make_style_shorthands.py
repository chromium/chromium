#!/usr/bin/env python
# Copyright (C) 2013 Intel Corporation. All rights reserved.
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

from core.css import css_properties
from collections import defaultdict
import json5_generator
from name_utilities import enum_key_for_css_property, id_for_css_property
import template_expander


class StylePropertyShorthandWriter(json5_generator.Writer):
    class_name = 'StylePropertyShorthand'
    _FILE_BASENAME = 'style_property_shorthand'

    def __init__(self, json5_file_paths, output_dir):
        super(StylePropertyShorthandWriter, self).__init__([], output_dir)
        self._input_files = json5_file_paths
        self._outputs = {
            (self._FILE_BASENAME + '.cc'):
                self.generate_style_property_shorthand_cpp,
            (self._FILE_BASENAME + '.h'):
                self.generate_style_property_shorthand_h
        }

        json5_properties = css_properties.CSSProperties(json5_file_paths)
        self._shorthands = json5_properties.shorthands

        self._longhand_dictionary = defaultdict(list)
        for property_ in json5_properties.shorthands:
            property_['longhand_enum_keys'] = map(
                enum_key_for_css_property, property_['longhands'])
            property_['longhand_property_ids'] = map(
                id_for_css_property, property_['longhands'])
            for longhand_enum_key in property_['longhand_enum_keys']:
                self._longhand_dictionary[longhand_enum_key].append(property_)

        for longhands in self._longhand_dictionary.values():
            # Sort first by number of longhands in decreasing order, then
            # alphabetically
            longhands.sort(
                key=lambda property_: (
                    -len(property_['longhand_property_ids']), property_['name'].original)
            )

    @template_expander.use_jinja(
        'core/css/templates/style_property_shorthand.cc.tmpl')
    def generate_style_property_shorthand_cpp(self):
        return {
            'input_files': self._input_files,
            'properties': self._shorthands,
            'longhands_dictionary': self._longhand_dictionary,
        }

    @template_expander.use_jinja(
        'core/css/templates/style_property_shorthand.h.tmpl')
    def generate_style_property_shorthand_h(self):
        return {
            'input_files': self._input_files,
            'properties': self._shorthands,
            'header_guard': self.make_header_guard(self._relative_output_dir + self._FILE_BASENAME + '.h')
        }

if __name__ == '__main__':
    json5_generator.Maker(StylePropertyShorthandWriter).main()
