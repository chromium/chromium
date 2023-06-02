# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""scripts presubmit script

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
  output = []
  output.extend(
    input_api.canned_checks.RunPylint(input_api, output_api, version='2.6'))

  # These tests fail on Windows and don't need to run there.
  if not input_api.is_windows:
    py_tests = input_api.canned_checks.GetUnitTestsRecursively(
        input_api,
        output_api,
        input_api.PresubmitLocalPath(),
        files_to_check=[r'.+_test\.py$'],
        files_to_skip=[])

    output.extend(input_api.RunTests(py_tests, False))
  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
