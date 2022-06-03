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

import sys
import types

from core.css import css_properties
import json5_generator


def calculate_apply_functions_to_declare(property_):
    property_['should_declare_functions'] = \
        not property_['longhands'] \
        and property_['is_property']
    property_['use_property_class_in_stylebuilder'] = \
        property_['should_declare_functions']


class StyleBuilderWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(StyleBuilderWriter, self).__init__([], output_dir)

        self._json5_properties = css_properties.CSSProperties(json5_file_paths)
        self._input_files = json5_file_paths
        self._properties = self._json5_properties.longhands + \
            self._json5_properties.shorthands
        for property_ in self._properties:
            calculate_apply_functions_to_declare(property_)

    @property
    def css_properties(self):
        return self._json5_properties


if __name__ == '__main__':
    json5_generator.Maker(StyleBuilderWriter).main()
