# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for ios test runner scripts.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True


def _RunTestRunnerUnitTests(input_api, output_api):
  """ Runs iOS test runner unit tests """
  # TODO(crbug.com/1056457): Replace the list with regex ".*_test.py" once
  # all test files are fixed.
  files = [
      'file_util_test.py',
      'gtest_utils_test.py',
      'iossim_util_test.py',
      'result_sink_util_test.py',
      'run_test.py',
      'shard_util_test.py',
      'standard_json_util_test.py',
      'test_apps_test.py',
      # 'test_runner_test.py',
      'wpr_runner_test.py',
      'xcode_log_parser_test.py',
      'xcode_util_test.py',
      # 'xcodebuild_runner_test.py',
  ]

  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, '.', files_to_check=files)


def CheckChange(input_api, output_api):
  results = []
  results.extend(_RunTestRunnerUnitTests(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)
