# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import template_expander
from make_document_policy_features_util import parse_default_value


class DocumentPolicyFeatureWriter(json5_generator.Writer):
    file_basename = 'document_policy_features'

    def __init__(self, json5_file_path, output_dir):
        super(DocumentPolicyFeatureWriter, self).__init__(
            json5_file_path, output_dir)

        @template_expander.use_jinja(
            'templates/' + self.file_basename + '.cc.tmpl')
        def generate_implementation():
            return {
                'input_files':
                self._input_files,
                'features':
                self.json5_file.name_dictionaries,
                'parse_default_value':
                parse_default_value
            }

        self._outputs = {
            self.file_basename + '.cc': generate_implementation,
        }


if __name__ == '__main__':
    json5_generator.Maker(DocumentPolicyFeatureWriter).main()
