# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile
import unittest
from unittest import mock

import PRESUBMIT


class MockInputApi:

  def __init__(self):
    self.os_path = os.path
    self.change = mock.Mock()

  @staticmethod
  def PresubmitLocalPath():
    return os.path.dirname(__file__)


class MockOutputApi:

  @staticmethod
  def PresubmitError(message, items=None):
    # Using a tuple to make it easy to compare.
    return ('Error', message, items)

  @staticmethod
  def PresubmitResult(message, items=None):
    # Using a tuple to make it easy to compare.
    return ('Result', message, items)


class UndeclaredFeaturesTest(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.TemporaryDirectory()
    self.repo_root = self.temp_dir.name

    self.mock_input_api = MockInputApi()
    self.mock_input_api.change.RepositoryRoot.return_value = self.repo_root
    self.mock_output_api = MockOutputApi()

  def tearDown(self):
    self.temp_dir.cleanup()
    # Ensure find_features is reloaded for the next test or test suite
    # by removing it from the module cache. This prevents test state from
    # leaking.
    if 'find_features' in sys.modules:
      del sys.modules['find_features']

  def _create_file_in_repo(self, path, content):
    full_path = os.path.join(self.repo_root, path)
    os.makedirs(os.path.dirname(full_path), exist_ok=True)
    with open(full_path, 'w', encoding='utf-8') as f:
      f.write(content)

  def testAllFeaturesOnChangedLinesAreDeclared(self):
    # Declare a variety of features using both 2- and 3-argument macros.
    self._create_file_in_repo(
        'components/feature_a.cc',
        'BASE_FEATURE(kFeatureA, base::FEATURE_ENABLED_BY_DEFAULT);')
    self._create_file_in_repo(
        'chrome/feature_b.cc', 'BASE_FEATURE(kFeatureB, "FeatureB", '
        'base::FEATURE_ENABLED_BY_DEFAULT);')
    self._create_file_in_repo(
        'components/feature_c.cc', 'BASE_FEATURE(kFeatureC, "FeatureC",\n'
        '             base::FEATURE_ENABLED_BY_DEFAULT);')

    json_data = {
        'Study1': [{
            'experiments': [{
                'name': 'group1',
                'enable_features': ['FeatureA', 'FeatureB'],
            }]
        }],
        'Study2_Unaffected': [{
            'experiments': [{
                'name': 'group2',
                'enable_features': ['UndeclaredFeature'],
            }]
        }]
    }
    # The check should only trigger on features in changed lines.
    # FeatureA and FeatureB are declared, so this should pass.
    # UndeclaredFeature is not on a changed line, so it's ignored.
    changed_lines = [(1, '"enable_features": ["FeatureA", "FeatureB"]')]
    result = PRESUBMIT.CheckUndeclaredFeatures(self.mock_input_api,
                                               self.mock_output_api, json_data,
                                               changed_lines)
    self.assertEqual(result, [])

  def testUndeclaredFeatureOnChangedLine(self):
    self._create_file_in_repo(
        'components/feature_a.cc',
        'BASE_FEATURE(kFeatureA, base::FEATURE_ENABLED_BY_DEFAULT);')
    json_data = {
        'Study1': [{
            'experiments': [{
                'name': 'group1',
                'enable_features': ['FeatureA', 'UndeclaredFeature']
            }]
        }]
    }
    changed_lines = [(1, '"enable_features": ["UndeclaredFeature"]')]
    result = PRESUBMIT.CheckUndeclaredFeatures(self.mock_input_api,
                                               self.mock_output_api, json_data,
                                               changed_lines)
    self.assertEqual(len(result), 1)
    self.assertEqual(result[0][0], 'Result')
    self.assertIn('UndeclaredFeature', str(result[0]))
    self.assertIn('Study1', str(result[0]))

  def testFeatureWithMacroParametersOnDifferentLines(self):
    self._create_file_in_repo(
        'components/feature_d.cc', 'BASE_FEATURE(kFeatureD,\n'
        '             "FeatureD",\n'
        '             base::FEATURE_ENABLED_BY_DEFAULT);')
    json_data = {
        'Study1': [{
            'experiments': [{
                'name': 'group1',
                'enable_features': ['FeatureD']
            }]
        }]
    }
    changed_lines = [(1, '"enable_features": ["FeatureD"]')]
    result = PRESUBMIT.CheckUndeclaredFeatures(self.mock_input_api,
                                               self.mock_output_api, json_data,
                                               changed_lines)
    self.assertEqual(result, [])

  def testTwoParameterMacro(self):
    self._create_file_in_repo(
        'components/feature_g.cc',
        'BASE_FEATURE(kFeatureG, base::FEATURE_ENABLED_BY_DEFAULT);')
    json_data = {
        'Study1': [{
            'experiments': [{
                'name': 'group1',
                'enable_features': ['FeatureG']
            }]
        }]
    }
    changed_lines = [(1, '"enable_features": ["FeatureG"]')]
    result = PRESUBMIT.CheckUndeclaredFeatures(self.mock_input_api,
                                               self.mock_output_api, json_data,
                                               changed_lines)
    self.assertEqual(result, [])

  def testTwoParameterMacroWithParametersOnDifferentLines(self):
    self._create_file_in_repo(
        'components/feature_h.cc', 'BASE_FEATURE(kFeatureH,\n'
        '             base::FEATURE_ENABLED_BY_DEFAULT);')
    json_data = {
        'Study1': [{
            'experiments': [{
                'name': 'group1',
                'enable_features': ['FeatureH']
            }]
        }]
    }
    changed_lines = [(1, '"enable_features": ["FeatureH"]')]
    result = PRESUBMIT.CheckUndeclaredFeatures(self.mock_input_api,
                                               self.mock_output_api, json_data,
                                               changed_lines)
    self.assertEqual(result, [])

  def testFeatureWithPreprocessorDirectives(self):
    self._create_file_in_repo(
        'components/feature_f.cc', 'BASE_FEATURE(kFeatureF,\n'
        '             "FeatureF",\n'
        '#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)\n'
        '             base::FEATURE_ENABLED_BY_DEFAULT\n'
        '#else\n'
        '             base::FEATURE_DISABLED_BY_DEFAULT\n'
        '#endif\n'
        ');')
    json_data = {
        'Study1': [{
            'experiments': [{
                'name': 'group1',
                'enable_features': ['FeatureF']
            }]
        }]
    }
    changed_lines = [(1, '"enable_features": ["FeatureF"]')]
    result = PRESUBMIT.CheckUndeclaredFeatures(self.mock_input_api,
                                               self.mock_output_api, json_data,
                                               changed_lines)
    self.assertEqual(result, [])

  def testTwoParameterMacroWithPreprocessorDirectives(self):
    self._create_file_in_repo(
        'components/feature_k.cc', 'BASE_FEATURE(kFeatureK,\n'
        '#if BUILDFLAG(IS_WIN)\n'
        '    base::FEATURE_ENABLED_BY_DEFAULT\n'
        '#else\n'
        '    base::FEATURE_DISABLED_BY_DEFAULT\n'
        '#endif\n'
        ');')
    json_data = {
        'Study1': [{
            'experiments': [{
                'name': 'group1',
                'enable_features': ['FeatureK']
            }]
        }]
    }
    changed_lines = [(1, '"enable_features": ["FeatureK"]')]
    result = PRESUBMIT.CheckUndeclaredFeatures(self.mock_input_api,
                                               self.mock_output_api, json_data,
                                               changed_lines)
    self.assertEqual(result, [])

  def testNoFeaturesDeclaredInRepo(self):
    # No files created, so no features will be found, which is an error
    # condition.
    result = PRESUBMIT.CheckUndeclaredFeatures(self.mock_input_api,
                                               self.mock_output_api, {}, [])
    self.assertEqual(len(result), 1)
    self.assertEqual(result[0][0], 'Error')
    self.assertIn('unable to find any declared flags', result[0][1])


if __name__ == '__main__':
  unittest.main()
