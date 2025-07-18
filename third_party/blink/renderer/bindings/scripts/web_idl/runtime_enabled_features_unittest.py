# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for runtime_enabled_features.py."""

import unittest
import os

from .runtime_enabled_features import RuntimeEnabledFeatures


class RuntimeEnabledFeaturesTest(unittest.TestCase):

    def path_of_test_file(self, file_name):
        return os.path.join(os.path.dirname(os.path.realpath(__file__)),
                            'tests', file_name)

    def test_cycle(self):
        runtime_enabled_features_paths = self.path_of_test_file(
            'runtime_enabled_features_valid.json5')
        RuntimeEnabledFeatures.init(filepaths=[runtime_enabled_features_paths])
        assert RuntimeEnabledFeatures._is_initialized is True

        expected = [{
            'name': 'Feature1',
            'browser_process_read_write_access': False,
            'origin_trial_feature_name': None,
            'status': 'stable',
        }, {
            'name': 'Feature2',
            'browser_process_read_write_access': None,
            'origin_trial_feature_name': None,
            'status': 'test',
        }, {
            'name': 'Feature3',
            'browser_process_read_write_access': None,
            'origin_trial_feature_name': None,
            'status': 'experimental',
        }, {
            'name': 'Feature4',
            'browser_process_read_write_access': None,
            'origin_trial_feature_name': None,
            'status': 'stable',
        }, {
            'name': 'Feature5',
            'browser_process_read_write_access': True,
            'origin_trial_feature_name': None,
            'status': 'stable',
        }, {
            'name': 'Feature6',
            'browser_process_read_write_access': None,
            'origin_trial_feature_name': 'Feature6OT',
            'status': 'experimental',
        }]

        self.assertEqual(len(RuntimeEnabledFeatures._features), len(expected))
        for expected_item in expected:
            self.assertIn(expected_item['name'],
                          RuntimeEnabledFeatures._features)
            actual_feature = RuntimeEnabledFeatures._features[
                expected_item['name']]

            self.assertEqual(
                expected_item['browser_process_read_write_access'],
                actual_feature.get('browser_process_read_write_access'))
            self.assertEqual(expected_item['status'],
                             actual_feature.get('status'))
            self.assertEqual(expected_item['origin_trial_feature_name'],
                             actual_feature.get('origin_trial_feature_name'))


if __name__ == "__main__":
    unittest.main()
