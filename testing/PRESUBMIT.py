# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for testing.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

PYLINT_PATHS_COMPONENTS = [
    ('build', ),
    ('build', 'android'),
    ('build', 'util'),
    ('content', 'test', 'gpu'),
    ('testing', ),
    ('testing', 'buildbot'),
    ('testing', 'scripts'),
    ('testing', 'variations', 'presubmit'),
    ('third_party', ),
    ('third_party', 'blink', 'renderer', 'bindings', 'scripts'),
    ('third_party', 'blink', 'tools'),
    ('third_party', 'catapult', 'telemetry'),
    ('third_party', 'catapult', 'third_party', 'typ'),
    ('third_party', 'catapult', 'tracing'),
    ('third_party', 'domato', 'src'),
    ('third_party', 'js_code_coverage'),
    ('third_party', 'webdriver', 'pylib'),
    ('tools', 'perf'),
]


def _GetChromiumSrcPath(input_api):
  """Returns the path to the Chromium src directory."""
  return input_api.os_path.realpath(
      input_api.os_path.join(input_api.PresubmitLocalPath(), '..'))


def _GetTestingEnv(input_api):
  """Gets the common environment for running testing/ tests."""
  testing_env = dict(input_api.environ)
  testing_path = input_api.PresubmitLocalPath()
  # TODO(crbug.com/40237086): This is temporary till gpu code in
  # flake_suppressor_commonis moved to gpu dir.
  # Only common code will reside under /testing.
  gpu_test_path = input_api.os_path.join(input_api.PresubmitLocalPath(), '..',
                                         'content', 'test', 'gpu')
  typ_path = input_api.os_path.join(input_api.PresubmitLocalPath(), '..',
                                    'third_party', 'catapult', 'third_party',
                                    'typ')
  testing_env.update({
      'PYTHONPATH':
      input_api.os_path.pathsep.join([testing_path, gpu_test_path, typ_path]),
      'PYTHONDONTWRITEBYTECODE':
      '1',
  })
  return testing_env


def CheckFlakeSuppressorCommonUnittests(input_api, output_api):
  """Runs unittests in the testing/flake_suppressor_common/ directory."""
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api,
      output_api,
      input_api.os_path.join(input_api.PresubmitLocalPath(),
                             'flake_suppressor_common'), [r'^.+_unittest\.py$'],
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
  chromium_src_path = _GetChromiumSrcPath(input_api)
  pylint_extra_paths = [
      input_api.os_path.join(chromium_src_path, *component)
      for component in PYLINT_PATHS_COMPONENTS
  ]
  if input_api.is_windows:
    # These scripts don't run on Windows and should not be linted on Windows -
    # trying to do so will lead to spurious errors.
    files_to_skip += ('xvfb.py', '.*host_info.py')
  pylint_checks = input_api.canned_checks.GetPylint(
      input_api,
      output_api,
      extra_paths_list=pylint_extra_paths,
      files_to_skip=files_to_skip,
      # TODO(crbug.com/355016915): Remove this directory-specific pylintrc
      # file as the default one gets its disable list cleaned up.
      pylintrc='pylintrc',
      version='2.7')
  return input_api.RunTests(pylint_checks)


def CheckPatchFormatted(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(
      input_api,
      output_api,
      result_factory=output_api.PresubmitError,
  )
