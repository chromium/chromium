# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for ios test runner plugins.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

def _RunTestRunnerUnitTests(input_api, output_api):
  # Don't run iOS tests on Windows.
  if input_api.is_windows:
    return []
  # Runs iOS test runner unit tests
  files = ['.*_test.py$']

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
