#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import make_runtime_features
import json5_generator
import template_expander


class CommonFeaturesImplWriter(make_runtime_features.BaseRuntimeFeatureWriter):
    file_basename = 'features_generated'

    def __init__(self, json5_file_path, output_dir):
        super(CommonFeaturesImplWriter, self).__init__(json5_file_path,
                                                       output_dir)
        self._outputs = {
            (self.file_basename + '.cc'): self.generate_implementation,
        }

    def _template_inputs(self):
        return {
            'features': self._features,
            'feature_sets': self._feature_sets(),
            'platforms': self._platforms(),
            'input_files': self._input_files,
            'origin_trial_controlled_features': self._origin_trial_features,
        }

    @template_expander.use_jinja('templates/' + file_basename + '.cc.tmpl')
    def generate_implementation(self):
        return self._template_inputs()


if __name__ == '__main__':
    json5_generator.Maker(CommonFeaturesImplWriter).main()
