# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Note(crbug.com/1167597): Do NOT change the file name to end with
# '_test.py' or '_unittest.py' as these files will be recognized by
# 'run_blinkpy_tests.py' task, where jinja2 module is not available.

import unittest
from make_document_policy_features import DocumentPolicyFeatureWriter
from writer_test_util import path_to_test_file, WriterTest


class MakeDocumentPolicyFeaturesTest(WriterTest):
    def test_default_value_control(self):
        self._test_writer(
            DocumentPolicyFeatureWriter, [
                path_to_test_file('document_policy_default_value_control',
                                  'input', 'document_policy_features.json5')
            ],
            path_to_test_file('document_policy_default_value_control',
                              'output'))


if __name__ == "__main__":
    unittest.main()
