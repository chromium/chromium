#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import make_runtime_features
import make_runtime_features_utilities as util
import template_expander


class RunTimeFeatureStateContextHeaderWriter(
        make_runtime_features.BaseRuntimeFeatureWriter):
    file_basename = "runtime_feature_state_context"

    def __init__(self, json5_file_path, output_dir):
        super(RunTimeFeatureStateContextHeaderWriter,
              self).__init__(json5_file_path, output_dir)
        self._outputs = {
            (self.file_basename + '.h'): self.generate_header,
        }

        self._browser_read_access_features = util.browser_read_access(
            self._features)
        self._browser_write_access_features = util.browser_write_access(
            self._features)

    def _template_inputs(self):
        return {
            'features': self._features,
            'browser_read_access_features': self._browser_read_access_features,
            'browser_write_access_features':
            self._browser_write_access_features,
            'platforms': self._platforms(),
            'input_files': self._input_files,
            'header_guard': self._header_guard,
        }

    @template_expander.use_jinja(f'templates/{file_basename}.h.tmpl')
    def generate_header(self):
        return self._template_inputs()


class RunTimeFeatureStateReadContextHeaderWriter(
        make_runtime_features.BaseRuntimeFeatureWriter):
    file_basename = "runtime_feature_state_read_context"

    def __init__(self, json5_file_path, output_dir):
        super(RunTimeFeatureStateReadContextHeaderWriter,
              self).__init__(json5_file_path, output_dir)
        self._outputs = {
            (self.file_basename + '.h'): self.generate_header,
        }
        self._browser_read_access_features = util.browser_read_access(
            self._features)
        self._browser_write_access_features = util.browser_write_access(
            self._features)
        self._browser_read_access_with_third_party_features = util.browser_read_access_with_third_party(
            self._features)

    def _template_inputs(self):
        return {
            'features': self._features,
            'browser_read_access_features': self._browser_read_access_features,
            'browser_write_access_features':
            self._browser_write_access_features,
            'browser_read_access_with_third_party_features':
            self._browser_read_access_with_third_party_features,
            'platforms': self._platforms(),
            'input_files': self._input_files,
            'header_guard': self._header_guard,
        }

    @template_expander.use_jinja(f'templates/{file_basename}.h.tmpl')
    def generate_header(self):
        return self._template_inputs()


if __name__ == '__main__':
    json5_generator.Maker(RunTimeFeatureStateReadContextHeaderWriter).main()
    json5_generator.Maker(RunTimeFeatureStateContextHeaderWriter).main()
