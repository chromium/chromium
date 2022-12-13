# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for testing.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True


def CommonChecks(input_api, output_api):
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

  tests = []
  tests.extend(input_api.canned_checks.GetUnitTestsInDirectory(
      input_api,
      output_api,
      '.',
      [r'^.+_unittest\.py$'],
      run_on_python2=False,
      run_on_python3=USE_PYTHON3,
      skip_shebang_check=True))
  tests.extend(input_api.canned_checks.GetUnitTestsInDirectory(
      input_api,
      output_api,
      input_api.os_path.join(input_api.PresubmitLocalPath(),
                             'flake_suppressor_common'),
      [r'^.+_unittest\.py$'],
      env=testing_env,
      run_on_python2=False,
      run_on_python3=USE_PYTHON3,
      skip_shebang_check=True))
  tests.extend(input_api.canned_checks.GetUnitTestsInDirectory(
      input_api,
      output_api,
      input_api.os_path.join(input_api.PresubmitLocalPath(),
                             'unexpected_passes_common'),
      [r'^.+_unittest\.py$'],
      env=testing_env,
      run_on_python2=False,
      run_on_python3=USE_PYTHON3,
      skip_shebang_check=True))
  files_to_skip = input_api.DEFAULT_FILES_TO_SKIP
  if input_api.is_windows:
    # These scripts don't run on Windows and should not be linted on Windows -
    # trying to do so will lead to spurious errors.
    files_to_skip += ('xvfb.py', '.*host_info.py')
  tests.extend(input_api.canned_checks.GetPylint(
      input_api,
      output_api,
      files_to_skip=files_to_skip,
      version='2.7'))

  return input_api.RunTests(tests)


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
