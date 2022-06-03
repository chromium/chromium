#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import json5_generator
import template_expander

from core.css import css_properties

CSS_PROPERTIES_H_TMPL = 'core/css/properties/templates/css_properties.h.tmpl'
CSS_PROPERTIES_CC_TMPL = 'core/css/properties/templates/css_properties.cc.tmpl'


class CSSPropertiesWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(CSSPropertiesWriter, self).__init__([], output_dir)
        assert len(json5_file_paths) == 4,\
            ('CSSPropertiesWriter requires 4 input json5 files, ' +
             'got {}.'.format(len(json5_file_paths)))

        self._css_properties = css_properties.CSSProperties(json5_file_paths)

        # Map of property method name -> (return_type, parameters)
        self._property_methods = {}
        property_methods = json5_generator.Json5File.load_from_files(
            [json5_file_paths[3]])
        for property_method in property_methods.name_dictionaries:
            self._property_methods[property_method['name'].
                                   original] = property_method

        all_properties = self._css_properties.properties_including_aliases

        for property_ in all_properties:
            property_['property_methods'] = [
                self._property_methods[method_name]
                for method_name in property_['property_methods']
            ]

        # Clean up all the files that were previously generated. This prevents
        # accidentally including a stale header in the future.
        old_file = lambda prop: prop['namespace_group'].lower() + '/' \
            + prop['name'].to_snake_case()
        old_h = lambda prop: old_file(prop) + '.h'
        old_cc = lambda prop: old_file(prop) + '.cc'
        self._cleanup |= set(map(old_h, all_properties))
        self._cleanup |= set(map(old_cc, all_properties))

        self._input_files = json5_file_paths
        self._outputs = {}
        self._outputs['longhands.h'] = self.generate_longhands_h
        self._outputs['longhands.cc'] = self.generate_longhands_cc
        self._outputs['shorthands.h'] = self.generate_shorthands_h
        self._outputs['shorthands.cc'] = self.generate_shorthands_cc

    @template_expander.use_jinja(CSS_PROPERTIES_H_TMPL)
    def generate_longhands_h(self):
        return {
            'input_files': self._input_files,
            'properties': self._css_properties.longhands_including_aliases,
            'is_longhand': True,
        }

    @template_expander.use_jinja(CSS_PROPERTIES_CC_TMPL)
    def generate_longhands_cc(self):
        return {
            'input_files': self._input_files,
            'properties': self._css_properties.longhands_including_aliases,
            'is_longhand': True,
        }

    @template_expander.use_jinja(CSS_PROPERTIES_H_TMPL)
    def generate_shorthands_h(self):
        return {
            'input_files': self._input_files,
            'properties': self._css_properties.shorthands_including_aliases,
            'is_longhand': False,
        }

    @template_expander.use_jinja(CSS_PROPERTIES_CC_TMPL)
    def generate_shorthands_cc(self):
        return {
            'input_files': self._input_files,
            'properties': self._css_properties.shorthands_including_aliases,
            'is_longhand': False,
        }


if __name__ == '__main__':
    json5_generator.Maker(CSSPropertiesWriter).main()
