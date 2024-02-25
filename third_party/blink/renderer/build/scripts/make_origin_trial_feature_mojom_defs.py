# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import make_runtime_features
import template_expander


class OriginTrialFeatureMojomWriter(
        make_runtime_features.BaseRuntimeFeatureWriter):
    file_basename = "origin_trial_feature"

    def __init__(self, json5_file_path, output_dir):
        super(OriginTrialFeatureMojomWriter,
              self).__init__(json5_file_path, output_dir)
        self._outputs = {
            (self.file_basename + '.mojom'): self.generate_mojom_definition
        }

    def _template_inputs(self):
        return {
            'features': self._features,
            'input_files': self._input_files,
            'header_guard': self._header_guard,
        }

    @template_expander.use_jinja(f'templates/{file_basename}.mojom.tmpl')
    def generate_mojom_definition(self):
        return self._template_inputs()


if __name__ == '__main__':
    json5_generator.Maker(OriginTrialFeatureMojomWriter).main()
