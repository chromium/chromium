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
  # TODO(crbug.com/40237086): This is temporary till gpu code in
  # flake_suppressor_commonis moved to gpu dir.
  # Only common code will reside under /testing.
  gpu_test_path = input_api.os_path.join(
    input_api.PresubmitLocalPath(), '..', 'content', 'test', 'gpu'
  )
  typ_path = input_api.os_path.join(
    input_api.PresubmitLocalPath(),
    '..',
    'third_party',
    'catapult',
    'third_party',
    'typ',
  )
  testing_env.update(
    {
      'PYTHONPATH': input_api.os_path.pathsep.join(
        [testing_path, gpu_test_path, typ_path]
      ),
      'PYTHONDONTWRITEBYTECODE': '1',
    }
  )
  return testing_env


def _ShouldRunCommonUnittests(input_api, directories):
  """Returns True if affected files match directories or this PRESUBMIT.py."""
  if isinstance(directories, str):
    directories = [directories]
  this_presubmit = input_api.os_path.normcase(
    input_api.os_path.join(input_api.PresubmitLocalPath(), 'PRESUBMIT.py')
  )
  # Ensure trailing separator to avoid prefix-matching unrelated directories.
  prefixes = tuple(
    input_api.os_path.normcase(
      input_api.os_path.join(input_api.PresubmitLocalPath(), d, '')
    )
    for d in directories
  )
  affected_paths = (
    input_api.os_path.normcase(f.AbsoluteLocalPath())
    for f in input_api.AffectedFiles()
  )
  return any(
    p.startswith(prefixes) or p == this_presubmit for p in affected_paths
  )


def CheckFlakeSuppressorCommonUnittests(input_api, output_api):
  """Runs unittests in the testing/flake_suppressor_common/ directory."""
  # Note: flake_suppressor_common depends on unexpected_passes_common.
  if not _ShouldRunCommonUnittests(
    input_api, ['flake_suppressor_common', 'unexpected_passes_common']
  ):
    return []
  return input_api.canned_checks.RunUnitTestsInDirectory(
    input_api,
    output_api,
    input_api.os_path.join(
      input_api.PresubmitLocalPath(), 'flake_suppressor_common'
    ),
    [r'^.+_unittest\.py$'],
    env=_GetTestingEnv(input_api),
  )


def CheckUnexpectedPassesCommonUnittests(input_api, output_api):
  """Runs unittests in the testing/unexpected_passes_common/ directory."""
  if not _ShouldRunCommonUnittests(input_api, ['unexpected_passes_common']):
    return []
  return input_api.canned_checks.RunUnitTestsInDirectory(
    input_api,
    output_api,
    input_api.os_path.join(
      input_api.PresubmitLocalPath(), 'unexpected_passes_common'
    ),
    [r'^.+_unittest\.py$'],
    env=_GetTestingEnv(input_api),
  )


def CheckRuff(input_api, output_api):
  return input_api.RunTests(
    input_api.canned_checks.GetRuff(input_api, output_api)
  )


def CheckPatchFormatted(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(
    input_api,
    output_api,
    result_factory=output_api.PresubmitError,
  )
