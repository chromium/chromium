# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for gpu.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

def CommonChecks(input_api, output_api):
  import sys

  output = []
  sys_path_backup = sys.path
  try:
    sys.path = [
        input_api.PresubmitLocalPath()
    ] + sys.path
    disabled_warnings = [
        'W0622',  # redefined-builtin
        'R0923',  # interface-not-implemented
    ]
    output.extend(input_api.canned_checks.RunPylint(
      input_api,
      output_api,
      disabled_warnings=disabled_warnings))
  finally:
    sys.path = sys_path_backup

  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
