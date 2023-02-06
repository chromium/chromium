#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import make_runtime_features
import make_runtime_features_utilities as util
import template_expander


class RunTimeFeatureStateOverrideContextWriter(
        make_runtime_features.BaseRuntimeFeatureWriter):
    file_basename = "runtime_feature_state_override_context"

    def __init__(self, json5_file_path, output_dir):
        super(RunTimeFeatureStateOverrideContextWriter,
              self).__init__(json5_file_path, output_dir)
        self._outputs = {
            (self.file_basename + '.cc'): self.generate_implementation,
            (self.file_basename + '.h'): self.generate_header,
        }
        self._overridable_features = util.overridable_features(self._features)

    def _template_inputs(self):
        return {
            'features': self._features,
            'platforms': self._platforms(),
            'input_files': self._input_files,
            'header_guard': self._header_guard,
            'overridable_features': self._overridable_features,
            'origin_trial_controlled_features': self._origin_trial_features,
        }

    @template_expander.use_jinja(f'templates/{file_basename}.cc.tmpl')
    def generate_implementation(self):
        return self._template_inputs()

    @template_expander.use_jinja(f'templates/{file_basename}.h.tmpl')
    def generate_header(self):
        return self._template_inputs()


if __name__ == '__main__':
    json5_generator.Maker(RunTimeFeatureStateOverrideContextWriter).main()
