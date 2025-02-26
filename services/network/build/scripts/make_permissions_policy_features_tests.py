# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Note(crbug.com/1167597): Do NOT change the file name to end with
# '_test.py' or '_unittest.py' as these files will be recognized by
# 'run_blinkpy_tests.py' task, where jinja2 module is not available.

import sys
import os
import unittest
from make_permissions_policy_features import PermissionsPolicyFeatureWriter

# TODO(crbug.com/397934758): Move `writer_test_util` out of Blink and remove
# this hack.
current_dir = os.path.dirname(__file__)
module_path = os.path.join(current_dir, os.pardir, os.pardir, os.pardir,
                           os.pardir, 'third_party', 'blink', 'renderer', 'build', 'scripts')
sys.path.append(module_path)

from writer_test_util import WriterTest

def path_to_test_file(*path):
    return os.path.join(os.path.dirname(__file__), 'tests', *path)

class MakeDocumentPolicyFeaturesTest(WriterTest):
    def test_default_value_control(self):
        self._test_writer(
            PermissionsPolicyFeatureWriter, [
                path_to_test_file('permissions_policy_default_value_control',
                                  'input', 'permissions_policy_features.json5')
            ],
            path_to_test_file('permissions_policy_default_value_control',
                              'output'))


if __name__ == "__main__":
    unittest.main()
