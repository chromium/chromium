# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for content/test/gpu.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

def CommonChecks(input_api, output_api):
  results = []

  gpu_env = dict(input_api.environ)
  gpu_env.update({
      'PYTHONPATH': input_api.PresubmitLocalPath(),
      'PYTHONDONTWRITEBYTECODE': '1',
  })

  gpu_tests = [
      input_api.Command(
          name='run_content_test_gpu_unittests',
          cmd=[input_api.python_executable, 'run_unittests.py', 'gpu_tests'],
          kwargs={},
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
          env=gpu_env))

  pylint_checks = input_api.canned_checks.GetPylint(input_api, output_api)
  results.extend(input_api.RunTests(pylint_checks))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
