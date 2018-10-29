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

from blinkbuild.name_style_converter import NameStyleConverter
import json5_generator
import template_expander


class RuntimeFeatureWriter(json5_generator.Writer):
    class_name = 'RuntimeEnabledFeatures'
    file_basename = 'runtime_enabled_features'

    def __init__(self, json5_file_path, output_dir):
        super(RuntimeFeatureWriter, self).__init__(json5_file_path, output_dir)
        self._outputs = {(self.file_basename + '.h'): self.generate_header,
                         (self.file_basename + '.cc'): self.generate_implementation,
                        }

        self._features = self.json5_file.name_dictionaries
        # Make sure the resulting dictionaries have all the keys we expect.
        for feature in self._features:
            feature['data_member_name'] = self._data_member_name(feature['name'])
            # Most features just check their is_foo_enabled_ bool
            # but some depend on or are implied by other bools.
            enabled_condition = feature['data_member_name']
            assert not feature['implied_by'] or not feature['depends_on'], 'Only one of implied_by and depends_on is allowed'
            for implied_by_name in feature['implied_by']:
                enabled_condition += ' || ' + self._data_member_name(implied_by_name)
            for dependant_name in feature['depends_on']:
                enabled_condition += ' && ' + self._data_member_name(dependant_name)
            feature['enabled_condition'] = enabled_condition
        self._standard_features = [feature for feature in self._features if not feature['custom']]
        self._origin_trial_features = [feature for feature in self._features if feature['origin_trial_feature_name']]
        self._header_guard = self.make_header_guard(self._relative_output_dir + self.file_basename + '.h')

    @staticmethod
    def _data_member_name(str_or_converter):
        converter = NameStyleConverter(str_or_converter) if type(str_or_converter) is str else str_or_converter
        return converter.to_class_data_member(prefix='is', suffix='enabled')

    def _feature_sets(self):
        # Another way to think of the status levels is as "sets of features"
        # which is how we're referring to them in this generator.
        return self.json5_file.parameters['status']['valid_values']

    def _template_inputs(self):
        return {
            'features': self._features,
            'feature_sets': self._feature_sets(),
            'input_files': self._input_files,
            'standard_features': self._standard_features,
            'origin_trial_controlled_features': self._origin_trial_features,
            'header_guard': self._header_guard,
        }

    @template_expander.use_jinja('templates/' + file_basename + '.h.tmpl')
    def generate_header(self):
        return self._template_inputs()

    @template_expander.use_jinja('templates/' + file_basename + '.cc.tmpl')
    def generate_implementation(self):
        return self._template_inputs()


class RuntimeFeatureTestHelpersWriter(json5_generator.Writer):
    class_name = 'ScopedRuntimeEnabledFeatureForTest'
    file_basename = 'runtime_enabled_features_test_helpers'

    def __init__(self, json5_file_path, output_dir):
        super(RuntimeFeatureTestHelpersWriter, self).__init__(json5_file_path, output_dir)
        self._outputs = {('testing/' + self.file_basename + '.h'): self.generate_header}
        self._features = self.json5_file.name_dictionaries
        self._header_guard = self.make_header_guard(self._relative_output_dir + self.file_basename + '.h')

    def _template_inputs(self):
        return {
            'features': self._features,
            'input_files': self._input_files,
            'header_guard': self._header_guard,
        }

    @template_expander.use_jinja('templates/' + file_basename + '.h.tmpl')
    def generate_header(self):
        return self._template_inputs()

if __name__ == '__main__':
    json5_generator.Maker(RuntimeFeatureWriter).main()
    json5_generator.Maker(RuntimeFeatureTestHelpersWriter).main()
