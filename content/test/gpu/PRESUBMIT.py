# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for content/test/gpu.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True


def CommonChecks(input_api, output_api):
  results = []

  gpu_env = dict(input_api.environ)
  current_path = input_api.PresubmitLocalPath()
  testing_path = input_api.os_path.realpath(
      input_api.os_path.join(current_path, '..', '..', '..', 'testing'))
  gpu_env.update({
      'PYTHONPATH':
      input_api.os_path.pathsep.join([testing_path, current_path]),
      'PYTHONDONTWRITEBYTECODE':
      '1',
  })

  gpu_tests = [
      input_api.Command(
          name='run_content_test_gpu_unittests',
          cmd=[input_api.python_executable, 'run_unittests.py', 'gpu_tests'],
          kwargs={'env': gpu_env},
          message=output_api.PresubmitError),
      input_api.Command(name='validate_tag_consistency',
                        cmd=[
                            input_api.python_executable,
                            'validate_tag_consistency.py',
                            'validate',
                        ],
                        kwargs={},
                        message=output_api.PresubmitError),
  ]
  results.extend(input_api.RunTests(gpu_tests))

  results.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(
          input_api,
          output_api,
          input_api.os_path.join(input_api.PresubmitLocalPath(),
                                 'unexpected_passes'), [r'^.+_unittest\.py$'],
          env=gpu_env,
          run_on_python2=False,
          run_on_python3=True))

  results.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(
          input_api,
          output_api,
          input_api.os_path.join(input_api.PresubmitLocalPath(),
                                 'flake_suppressor'), [r'^.+_unittest\.py$'],
          env=gpu_env,
          run_on_python2=False,
          run_on_python3=True))

  pylint_checks = input_api.canned_checks.GetPylint(input_api, output_api)
  results.extend(input_api.RunTests(pylint_checks))

  results.extend(CheckForNewSkipExpectations(input_api, output_api))

  return results


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


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
