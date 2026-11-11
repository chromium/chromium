#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.css import css_properties
import json5_generator
from name_utilities import enum_key_for_css_keyword
import template_expander


class CSSPropertyKeywordsWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super().__init__([], output_dir)
        self._input_files = json5_file_paths
        self._properties = (
            css_properties.CSSProperties(json5_file_paths)
        ).longhands
        for property_ in self._properties:
            property_.keyword_ids = [
                enum_key_for_css_keyword(keyword)
                for keyword in property_.keywords
            ]
            property_.runtime_enabled_keyword_features = {
                enum_key_for_css_keyword(keyword): feature
                for feature, keywords in property_.runtime_enabled_keywords.items()
                for keyword in keywords
            }
        self._outputs = {
            'css_property_keywords.cc': self.generate_keywords,
        }

    @template_expander.use_jinja(
        'core/css/templates/css_property_keywords.cc.tmpl'
    )
    def generate_keywords(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
        }


if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyKeywordsWriter).main()
