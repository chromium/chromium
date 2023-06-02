# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/extensions/common.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

import sys


def CheckExternsOnUpload(input_api, output_api):
  original_sys_path = sys.path
  try:
    sys.path.append(input_api.PresubmitLocalPath())
    from externs_checker import ExternsChecker
  finally:
    sys.path = original_sys_path

  return ExternsChecker(input_api, output_api).RunChecks()
