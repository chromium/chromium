# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for native_client_sdk.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True


def CommonChecks(input_api, output_api):
  output = []
  disabled_warnings = [
    'F0401',  # Unable to import module
    'R0401',  # Cyclic import
    'W0613',  # Unused argument
    'W0403',  # relative import warnings
    'E1103',  # subprocess.communicate() generates these :(
    'R0201',  # method could be function (doesn't reference self)
  ]
  files_to_skip = [
    r'src[\\\/]build_tools[\\\/]tests[\\\/].*',
    r'src[\\\/]build_tools[\\\/]sdk_tools[\\\/]third_party[\\\/].*',
    r'src[\\\/]doc[\\\/]*',
    r'src[\\\/]gonacl_appengine[\\\/]*',
  ]
  canned = input_api.canned_checks
  output.extend(canned.RunPylint(input_api, output_api,
                                 files_to_skip=files_to_skip,
                                 disabled_warnings=disabled_warnings))
  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
