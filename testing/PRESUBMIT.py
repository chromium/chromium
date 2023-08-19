# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for testing.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def _GetTestingEnv(input_api):
  """Gets the common environment for running testing/ tests."""
  testing_env = dict(input_api.environ)
  testing_path = input_api.PresubmitLocalPath()
  # TODO(crbug.com/1358733): This is temporary till gpu code in
  # flake_suppressor_commonis moved to gpu dir.
  # Only common code will reside under /testing.
  gpu_test_path = input_api.os_path.join(
      input_api.PresubmitLocalPath(), '..', 'content', 'test', 'gpu')
  testing_env.update({
      'PYTHONPATH': input_api.os_path.pathsep.join(
        [testing_path, gpu_test_path]),
      'PYTHONDONTWRITEBYTECODE': '1',
  })
  return testing_env


def CheckFlakeSuppressorCommonUnittests(input_api, output_api):
  """Runs unittests in the testing/flake_suppressor_common/ directory."""
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api,
      output_api,
      input_api.os_path.join(input_api.PresubmitLocalPath(),
                             'flake_suppressor_common'),
      [r'^.+_unittest\.py$'],
      env=_GetTestingEnv(input_api))


def CheckUnexpectedPassesCommonUnittests(input_api, output_api):
  """Runs unittests in the testing/unexpected_passes_common/ directory."""
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api,
      output_api,
      input_api.os_path.join(input_api.PresubmitLocalPath(),
                             'unexpected_passes_common'),
      [r'^.+_unittest\.py$'],
      env=_GetTestingEnv(input_api))


def CheckPylint(input_api, output_api):
  """Runs pylint on all directory content and subdirectories."""
  files_to_skip = input_api.DEFAULT_FILES_TO_SKIP
  if input_api.is_windows:
    # These scripts don't run on Windows and should not be linted on Windows -
    # trying to do so will lead to spurious errors.
    files_to_skip += ('xvfb.py', '.*host_info.py')
  pylint_checks = input_api.canned_checks.GetPylint(
      input_api,
      output_api,
      files_to_skip=files_to_skip,
      version='2.7')
  return input_api.RunTests(pylint_checks)
