#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import template_expander

from core.css import css_properties

class CSSPropertyInstancesWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(CSSPropertyInstancesWriter, self).__init__([], output_dir)
        self._input_files = json5_file_paths
        self._outputs = {
            'css_property_instances.h':
            self.generate_property_instances_header,
            'css_property_instances.cc':
            self.generate_property_instances_implementation
        }
        # These files are no longer generated. If the files are present from
        # a previous build, we remove them. This avoids accidentally #including
        # a stale generated header.
        self._cleanup = set([
            'css_property.cc', 'css_property.h', 'css_unresolved_property.cc',
            'css_unresolved_property.h'
        ])

        self._css_properties = css_properties.CSSProperties(json5_file_paths)

        self._properties = self._css_properties.longhands + self._css_properties.shorthands
        self._aliases = self._css_properties.aliases

        self._properties.sort(key=lambda t: t.enum_value)
        self._aliases.sort(key=lambda t: t.enum_value)

    @template_expander.use_jinja(
        'core/css/properties/templates/css_property_instances.h.tmpl')
    def generate_property_instances_header(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
            'aliases': self._aliases,
        }

    @template_expander.use_jinja(
        'core/css/properties/templates/css_property_instances.cc.tmpl')
    def generate_property_instances_implementation(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
            'aliases': self._aliases,
        }


if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyInstancesWriter).main()
