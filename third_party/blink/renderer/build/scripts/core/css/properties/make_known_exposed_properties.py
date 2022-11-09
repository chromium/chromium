#!/usr/bin/env python
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.css import css_properties
import json5_generator
import template_expander


class KnownExposedPropertiesWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(KnownExposedPropertiesWriter, self).__init__([], output_dir)

        self._input_files = json5_file_paths
        properties = (
            css_properties.CSSProperties(json5_file_paths)).properties_including_aliases

        known_exposed_properties = []
        for property in properties:
            if not property['is_internal'] and \
               not property['runtime_flag'] and \
               not property['in_origin_trial']:
                known_exposed_properties.append(property['enum_key'])

        self._known_exposed_properties = known_exposed_properties

        self._outputs = {
            'known_exposed_properties.cc': self.generate_list,
        }

    @template_expander.use_jinja('core/css/properties/templates/known_exposed_properties.cc.tmpl')
    def generate_list(self):
        return {
            'input_files': self._input_files,
            'known_exposed_properties': self._known_exposed_properties,
        }


if __name__ == '__main__':
    json5_generator.Maker(KnownExposedPropertiesWriter).main()
