#!/usr/bin/env python
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.css import css_properties
import json5_generator
import template_expander


class LogicalPropertiesWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(LogicalPropertiesWriter, self).__init__([], output_dir)

        self._input_files = json5_file_paths
        properties = (css_properties.CSSProperties(json5_file_paths)
                      ).properties_including_aliases

        self._logical_group_properties = [
            p.enum_key for p in properties if p.logical_property_group
            and p.logical_property_group['is_logical']
        ]

        self._outputs = {
            'logical_group_properties.cc': self.generate_list,
        }

    @template_expander.use_jinja(
        'core/css/properties/templates/logical_group_properties.cc.tmpl')
    def generate_list(self):
        return {
            'input_files': self._input_files,
            'logical_group_properties': self._logical_group_properties,
        }


if __name__ == '__main__':
    json5_generator.Maker(LogicalPropertiesWriter).main()
