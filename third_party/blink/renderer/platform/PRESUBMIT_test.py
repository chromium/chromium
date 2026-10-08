#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
from unittest import mock

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_SRC_ROOT = os.path.abspath(os.path.join(_THIS_DIR, '..', '..', '..', '..'))
sys.path.insert(0, _THIS_DIR)
sys.path.append(_SRC_ROOT)

from PRESUBMIT_test_mocks import MockAffectedFile  # noqa: E402
from PRESUBMIT_test_mocks import MockInputApi  # noqa: E402
from PRESUBMIT_test_mocks import MockOutputApi  # noqa: E402
import PRESUBMIT  # noqa: E402

_FEATURES_FILE = 'runtime_enabled_features.json5'
_OVERRIDE_FILE = 'runtime_enabled_features.override.json5'
_PLATFORM_DIR = 'third_party/blink/renderer/platform'
_FEATURES_PATH = f'{_PLATFORM_DIR}/{_FEATURES_FILE}'
_OVERRIDE_PATH = f'{_PLATFORM_DIR}/{_OVERRIDE_FILE}'


class CheckRuntimeEnabledFeaturesTest(unittest.TestCase):
    def setUp(self):
        self.mock_input_api = MockInputApi()
        self.mock_input_api.presubmit_local_path = _THIS_DIR
        self.mock_output_api = MockOutputApi()

    def test_features_sorted_valid(self):
        features = [{'name': 'Alpha'}, {'name': 'Beta'}, {'name': 'Gamma'}]
        results = PRESUBMIT._CheckRuntimeEnabledFeaturesSorted(
            features, 'test.json5', self.mock_output_api
        )
        self.assertEqual(results, [])

    def test_features_unsorted(self):
        features = [{'name': 'Gamma'}, {'name': 'Alpha'}]
        results = PRESUBMIT._CheckRuntimeEnabledFeaturesSorted(
            features, 'test.json5', self.mock_output_api
        )
        self.assertEqual(len(results), 1)
        self.assertIn(
            'test.json5 features must be sorted alphabetically',
            results[0].message,
        )

    def test_unaffected_files_skip_check(self):
        self.mock_input_api.files = [
            MockAffectedFile(f'{_PLATFORM_DIR}/fonts/BUILD.gn', ['# Unrelated'])
        ]
        with mock.patch.object(
            PRESUBMIT, 'RuntimeEnabledFeatures'
        ) as mock_features:
            results = PRESUBMIT.CheckChangeOnUpload(
                self.mock_input_api, self.mock_output_api
            )
            self.assertEqual(results, [])
            mock_features.assert_not_called()

    def test_affected_features_file_runs_check(self):
        self.mock_input_api.files = [
            MockAffectedFile(_FEATURES_PATH, ['# modified'])
        ]
        valid = [{'name': 'Alpha'}, {'name': 'Beta'}]
        with mock.patch.object(
            PRESUBMIT, 'RuntimeEnabledFeatures', return_value=valid
        ) as mock_features:
            results = PRESUBMIT.CheckChangeOnUpload(
                self.mock_input_api, self.mock_output_api
            )
            self.assertEqual(results, [])
            mock_features.assert_called_once()

    def test_affected_override_file_runs_check(self):
        self.mock_input_api.files = [
            MockAffectedFile(_OVERRIDE_PATH, ['# modified'])
        ]
        unsorted = [{'name': 'Zeta'}, {'name': 'Alpha'}]
        with mock.patch.object(
            PRESUBMIT, 'RuntimeEnabledFeatures', return_value=unsorted
        ) as mock_features:
            results = PRESUBMIT.CheckChangeOnUpload(
                self.mock_input_api, self.mock_output_api
            )
            self.assertEqual(len(results), 1)
            self.assertIn(_OVERRIDE_FILE, results[0].message)
            mock_features.assert_called_once()

    def test_deleted_file_skips_check(self):
        self.mock_input_api.files = [
            MockAffectedFile(_FEATURES_PATH, ['# deleted'], action='D')
        ]
        with mock.patch.object(
            PRESUBMIT, 'RuntimeEnabledFeatures'
        ) as mock_features:
            results = PRESUBMIT.CheckChangeOnUpload(
                self.mock_input_api, self.mock_output_api
            )
            self.assertEqual(results, [])
            mock_features.assert_not_called()

    def test_runtime_enabled_features_loader(self):
        override_full_path = os.path.join(_THIS_DIR, _OVERRIDE_FILE)
        features = PRESUBMIT.RuntimeEnabledFeatures(
            self.mock_input_api, override_full_path
        )
        self.assertIsInstance(features, list)
        json5_path = os.path.normpath(
            os.path.join(_THIS_DIR, '..', '..', '..', 'pyjson5', 'src')
        )
        self.assertNotIn(json5_path, sys.path)


if __name__ == '__main__':
    unittest.main()
