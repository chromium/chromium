# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for content/test/gpu.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

def CommonChecks(input_api, output_api):
  commands = [
    input_api.Command(
      name='run_content_test_gpu_unittests', cmd=[
        input_api.python_executable, 'run_unittests.py', 'gpu_tests'],
      kwargs={}, message=output_api.PresubmitError),
  ]
  return input_api.RunTests(commands)

def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
