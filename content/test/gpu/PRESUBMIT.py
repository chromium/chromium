# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for content/test/gpu.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


PRESUBMIT_VERSION = '2.0.0'

EXTRA_PATHS_COMPONENTS = [
    ('build', ),
    ('build', 'fuchsia', 'test'),
    ('build', 'util'),
    ('testing', ),
    ('third_party', 'catapult', 'common', 'py_utils'),
    ('third_party', 'catapult', 'devil'),
    ('third_party', 'catapult', 'telemetry'),
    ('third_party', 'catapult', 'third_party', 'typ'),
    ('tools', 'perf'),
]


def _GetChromiumSrcPath(input_api):
  """Returns the path to the Chromium src directory."""
  return input_api.os_path.realpath(
      input_api.os_path.join(input_api.PresubmitLocalPath(), '..', '..', '..'))


def _GetGpuEnv(input_api):
  """Gets the common environment for running GPU tests."""
  gpu_env = dict(input_api.environ)
  current_path = input_api.PresubmitLocalPath()
  chromium_src_path = _GetChromiumSrcPath(input_api)
  testing_path = input_api.os_path.join(chromium_src_path, 'testing')
  gpu_env.update({
      'PYTHONPATH':
      input_api.os_path.pathsep.join([testing_path, current_path]),
      'PYTHONDONTWRITEBYTECODE':
      '1',
  })
  return gpu_env


def CheckGpuTestsUnittests(input_api, output_api):
  """Runs the unittests for the gpu_tests directory."""
  gpu_env = _GetGpuEnv(input_api)
  command = input_api.Command(
      name='run_content_test_gpu_unittests',
      cmd=[input_api.python3_executable, 'run_unittests.py', 'gpu_tests'],
      kwargs={'env': gpu_env},
      message=output_api.PresubmitError,
      python3=True)
  return input_api.RunTests([command])


def CheckMachineTimesUnittests(input_api, output_api):
  """Runs the unittests for the machine_times directory."""
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api,
      output_api,
      input_api.os_path.join(input_api.PresubmitLocalPath(),
                             'machine_times'), [r'^.+_unittest\.py$'],
      env=_GetGpuEnv(input_api),
      run_on_python2=False,
      run_on_python3=True,
      skip_shebang_check=True)


def CheckUnexpectedPassesUnittests(input_api, output_api):
  """Runs the unittests for the unexpected_passes directory."""
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api,
      output_api,
      input_api.os_path.join(input_api.PresubmitLocalPath(),
                             'unexpected_passes'), [r'^.+_unittest\.py$'],
      env=_GetGpuEnv(input_api))


def CheckFlakeSuppressorUnittests(input_api, output_api):
  """Runs the unittests for the flake_suppressor directory."""
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api,
      output_api,
      input_api.os_path.join(input_api.PresubmitLocalPath(),
                             'flake_suppressor'), [r'^.+_unittest\.py$'],
      env=_GetGpuEnv(input_api))


def CheckValidateTagConsistency(input_api, output_api):
  """Checks that GPU expectation tags are consistent across all files."""
  command = input_api.Command(name='validate_tag_consistency',
                              cmd=[
                                  input_api.python3_executable,
                                  'validate_tag_consistency.py', 'validate'
                              ],
                              kwargs={},
                              message=output_api.PresubmitError)
  return input_api.RunTests([command])


def CheckForNewSkipExpectations(input_api, output_api):
  """Checks for and dissuades the addition of new Skip expectations."""
  new_skips = []
  expectation_file_dir = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                                'gpu_tests',
                                                'test_expectations')
  file_filter = lambda f: f.AbsoluteLocalPath().startswith(expectation_file_dir)
  for affected_file in input_api.AffectedFiles(file_filter=file_filter):
    for _, line in affected_file.ChangedContents():
      if input_api.re.search(r'\[\s*Skip\s*\]', line):
        new_skips.append((affected_file, line))
  result = []
  if new_skips:
    warnings = []
    for affected_file, line in new_skips:
      warnings.append('  Line "%s" in file %s' %
                      (line, affected_file.LocalPath()))
    result.append(
        output_api.PresubmitPromptWarning(
            'Suspected new Skip expectations found:\n%s\nPlease only use such '
            'expectations when they are strictly necessary, e.g. the test is '
            'impacting other tests. Otherwise, opt for a '
            'Failure/RetryOnFailure expectation.' % '\n'.join(warnings)))
  return result


def CheckPylint(input_api, output_api):
  """Runs pylint on all directory content and subdirectories."""
  chromium_src_path = _GetChromiumSrcPath(input_api)
  pylint_extra_paths = [
      input_api.os_path.join(chromium_src_path, *component)
      for component in EXTRA_PATHS_COMPONENTS
  ]
  pylint_checks = input_api.canned_checks.GetPylint(
      input_api,
      output_api,
      extra_paths_list=pylint_extra_paths,
      pylintrc='pylintrc',
      version='2.7')
  return input_api.RunTests(pylint_checks)


def CheckPytypePathsInSync(input_api, output_api):
  """Checks that run_pytype.py's paths are in sync with PRESUBMIT.py's"""
  filepath = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                    'run_pytype.py')
  with open(filepath, encoding='utf-8') as infile:
    contents = infile.read()
  # Grab the EXTRA_PATHS_COMPONENTS = [...] portion as a string.
  match = input_api.re.search(r'(EXTRA_PATHS_COMPONENTS\s*=\s*[^=]*\]\n)',
                              contents, input_api.re.DOTALL)
  if not match:
    return [
        output_api.PresubmitError(
            'Unable to find EXTRA_PATHS_COMPONENTS in run_pytype.py. Maybe '
            'the code in PRESUBMIT.py needs to be updated?')
    ]
  expression = match.group(0)
  expression = expression.split('=', 1)[1]
  expression = expression.lstrip()
  pytype_path_components = input_api.ast.literal_eval(expression)
  if EXTRA_PATHS_COMPONENTS != pytype_path_components:
    return [
        output_api.PresubmitError(
            'EXTRA_PATHS_COMPONENTS is not synced between PRESUBMIT.py and '
            'run_pytype.py, please ensure they are identical.')
    ]
  return []
