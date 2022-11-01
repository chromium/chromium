#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import make_runtime_features
import make_runtime_features_utilities as util
import template_expander


class RunTimeFeatureStateContextImplWriter(
        make_runtime_features.BaseRuntimeFeatureWriter):
    file_basename = "runtime_feature_state_context"

    def __init__(self, json5_file_path, output_dir):
        super(RunTimeFeatureStateContextImplWriter,
              self).__init__(json5_file_path, output_dir)
        self._outputs = {
            (self.file_basename + '.cc'): self.generate_implementation,
        }

        self._browser_read_access_features = util.browser_read_access(
            self._features)

    def _template_inputs(self):
        return {
            'features': self._features,
            'browser_read_access_features': self._browser_read_access_features,
            'platforms': self._platforms(),
            'input_files': self._input_files,
            'header_guard': self._header_guard,
        }

    @template_expander.use_jinja(f'templates/{file_basename}.cc.tmpl')
    def generate_implementation(self):
        return self._template_inputs()


if __name__ == '__main__':
    json5_generator.Maker(RunTimeFeatureStateContextImplWriter).main()
