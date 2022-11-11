#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.css import css_properties
import json5_generator
from name_utilities import enum_key_for_css_keyword
import template_expander


class CSSOMTypesWriter(json5_generator.Writer):
    """
    Generates cssom_types.cc and cssom_keywords.cc. These classes provide
    utility methods for determining whether a given CSSStyleValue is valid
    for a given CSS property. The header files live in core/css/cssom.
    """

    def __init__(self, json5_file_paths, output_dir):
        super(CSSOMTypesWriter, self).__init__([], output_dir)

        self._input_files = json5_file_paths
        self._properties = (
            css_properties.CSSProperties(json5_file_paths)).longhands

        for property_ in self._properties:
            types = []
            # Expand types
            for single_type in property_.typedom_types:
                types.append(single_type)
            property_.typedom_types = types

            # Generate CSSValueID values from keywords.
            property_.keywordIDs = list(
                map(enum_key_for_css_keyword, property_.keywords))

        self._outputs = {
            'cssom_types.cc': self.generate_types,
            'cssom_keywords.cc': self.generate_keywords,
        }

    @template_expander.use_jinja('core/css/templates/cssom_types.cc.tmpl')
    def generate_types(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
        }

    @template_expander.use_jinja('core/css/templates/cssom_keywords.cc.tmpl')
    def generate_keywords(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
        }


if __name__ == '__main__':
    json5_generator.Maker(CSSOMTypesWriter).main()
