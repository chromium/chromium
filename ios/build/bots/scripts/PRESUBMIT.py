# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for ios test runner scripts.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True


def _RunTestRunnerUnitTests(input_api, output_api):
  # Don't run iOS tests on Windows.
  if input_api.is_windows:
    return []
  """ Runs iOS test runner unit tests """
  files = ['.*_test.py$']

  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api,
      output_api,
      '.',
      files_to_check=files,
      run_on_python2=not USE_PYTHON3,
      run_on_python3=USE_PYTHON3,
      skip_shebang_check=True)


def CheckChange(input_api, output_api):
  results = []
  results.extend(_RunTestRunnerUnitTests(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)
