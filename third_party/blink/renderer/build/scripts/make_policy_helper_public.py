# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import template_expander


class PublicPermissionsPolicyFeatureWriter(json5_generator.Writer):
    file_basename = 'policy_helper_public'

    def __init__(self, json5_file_path, output_dir):
        super(PublicPermissionsPolicyFeatureWriter,
              self).__init__(json5_file_path, output_dir)
        runtime_features = []
        permissions_policy_features = []

        for feature in self.json5_file.name_dictionaries:
            if feature['permissions_policy_name']:
                permissions_policy_features.append(feature)

        self._outputs = {
            self.file_basename + '.cc':
            template_expander.use_jinja(
                'templates/' + self.file_basename + '.cc.tmpl')(lambda: {
                    'header_guard':
                    self.make_header_guard(self._relative_output_dir + self.
                                           file_basename + '.h'),
                    'input_files':
                    self._input_files,
                    'permissions_policy_features':
                    permissions_policy_features,
                }),
        }


if __name__ == '__main__':
    json5_generator.Maker(PublicPermissionsPolicyFeatureWriter).main()
