# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for gpu.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import sys

def CommonChecks(input_api, output_api):

  output = []
  sys_path_backup = sys.path
  try:
    sys.path = [
        input_api.PresubmitLocalPath()
    ] + sys.path
    pylint_checks = input_api.canned_checks.GetPylint(
        input_api,
        output_api,
        version='2.7')
    output.extend(input_api.RunTests(pylint_checks))
  finally:
    sys.path = sys_path_backup

  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
