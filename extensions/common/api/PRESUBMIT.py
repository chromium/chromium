# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/extensions/common.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import sys


def _CheckExterns(input_api, output_api):
  original_sys_path = sys.path

  try:
    sys.path.append(input_api.PresubmitLocalPath())
    from externs_checker import ExternsChecker
  finally:
    sys.path = original_sys_path

  join = input_api.os_path.join
  api_root = input_api.PresubmitLocalPath()
  externs_root = input_api.os_path.abspath(join(
      api_root, '..', '..', '..', 'third_party', 'closure_compiler', 'externs'))

  api_pairs = {
    join(api_root, 'audio.idl'): join(externs_root, 'audio.js'),
    join(api_root, 'automation.idl'): join(externs_root, 'automation.js'),
    join(api_root, 'bluetooth.idl'): join(externs_root, 'bluetooth.js'),
    join(api_root, 'bluetooth_private.idl'):
        join(externs_root, 'bluetooth_private.js'),
    join(api_root, 'clipboard.idl'): join(externs_root, 'clipboard.js'),
    join(api_root, 'management.json'): join(externs_root, 'management.js'),
    join(api_root, 'metrics_private.json'):
        join(externs_root, 'metrics_private.js'),
    join(api_root, 'mime_handler_private.idl'):
        join(externs_root, 'mime_handler_private.js'),
    join(api_root, 'networking_private.idl'):
        join(externs_root, 'networking_private.js'),
    join(api_root, 'system_display.idl'):
        join(externs_root, 'system_display.js'),
    # TODO(rdevlin.cronin): Add more!
  }

  return ExternsChecker(input_api, output_api, api_pairs).RunChecks()


def CheckChangeOnUpload(input_api, output_api):
  return _CheckExterns(input_api, output_api)
