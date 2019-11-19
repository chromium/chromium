#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import template_expander

from collections import namedtuple
from core.css import css_properties

class PropertyClassData(
        namedtuple('PropertyClassData', 'enum_key,enum_value,property_id,classname,namespace_group,filename')):
    pass


class CSSPropertyInstancesWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(CSSPropertyInstancesWriter, self).__init__([], output_dir)
        self._input_files = json5_file_paths
        self._outputs = {
            'css_property_instances.h': self.generate_property_instances_header,
            'css_property_instances.cc':
                self.generate_property_instances_implementation
        }
        # These files are no longer generated. If the files are present from
        # a previous build, we remove them. This avoids accidentally #including
        # a stale generated header.
        self._cleanup = set([
            'css_property.cc',
            'css_property.h',
            'css_unresolved_property.cc',
            'css_unresolved_property.h'
        ])

        self._css_properties = css_properties.CSSProperties(json5_file_paths)

        properties = self._css_properties.longhands + self._css_properties.shorthands
        aliases = self._css_properties.aliases

        # Lists of PropertyClassData.
        self._property_classes_by_id = map(self.get_class, properties)
        self._alias_classes_by_id = map(self.get_class, aliases)

        # Sort by enum value.
        self._property_classes_by_id.sort(key=lambda t: t.enum_value)
        self._alias_classes_by_id.sort(key=lambda t: t.enum_value)

    def get_class(self, property_):
        """Gets the automatically
        generated class name for a property.
        Args:
            property_: A single property from CSSProperties.properties()
        Returns:
            The name to use for the property class.
        """
        namespace_group = 'Shorthand' if property_['longhands'] else 'Longhand'
        return PropertyClassData(
            enum_key=property_['enum_key'],
            enum_value=property_['enum_value'],
            property_id=property_['property_id'],
            classname=property_['name'].to_upper_camel_case(),
            namespace_group=namespace_group,
            filename=property_['name'].to_snake_case())

    @property
    def css_properties(self):
        return self._css_properties

    @template_expander.use_jinja(
        'core/css/properties/templates/css_property_instances.h.tmpl')
    def generate_property_instances_header(self):
        return {
            'input_files': self._input_files,
            'property_classes_by_property_id': self._property_classes_by_id,
            'alias_classes_by_property_id': self._alias_classes_by_id,
        }

    @template_expander.use_jinja(
        'core/css/properties/templates/css_property_instances.cc.tmpl')
    def generate_property_instances_implementation(self):
        return {
            'input_files': self._input_files,
            'property_classes_by_property_id': self._property_classes_by_id,
            'alias_classes_by_property_id': self._alias_classes_by_id,
        }

if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyInstancesWriter).main()
