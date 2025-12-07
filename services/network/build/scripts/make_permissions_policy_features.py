# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

# TODO(crbug.com/397934758): Move `json5_generator` and `template_expander` out
# of Blink and remove this hack.
current_dir = os.path.dirname(os.path.abspath(__file__))
module_path = os.path.join(current_dir, os.pardir, os.pardir, os.pardir,
                           os.pardir, 'third_party', 'blink', 'renderer', 'build', 'scripts')
sys.path.append(module_path)

import json5_generator
import template_expander


class PermissionsPolicyFeatureWriter(json5_generator.Writer):
    file_basename = 'permissions_policy_features_generated'

    def __init__(self, json5_file_path, output_dir):
        super(PermissionsPolicyFeatureWriter,
              self).__init__(json5_file_path, output_dir)

        current_dir = os.path.dirname(os.path.realpath(__file__))
        @template_expander.use_jinja(self.file_basename + '.cc.tmpl',
                                     template_dir = os.path.join(current_dir, 'templates'))
        def generate_implementation():
            return {
                'input_files':
                self._input_files,
                'features':
                self.json5_file.name_dictionaries
            }

        self._outputs = {
            self.file_basename + '.cc': generate_implementation,
        }


if __name__ == '__main__':
    json5_generator.Maker(PermissionsPolicyFeatureWriter).main()
