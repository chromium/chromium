# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/extensions/common/permissions.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import sys

PRESUBMIT_VERSION = '2.0.0'


def CheckAPIPermissionIDUpload(input_api, output_api):
  original_sys_path = sys.path

  try:
    sys.path.append(
        input_api.os_path.join(
            input_api.PresubmitLocalPath(),
            '..',
            '..',
            '..',
            'tools',
            'strict_enum_value_checker',
        )
    )
    from strict_enum_value_checker import StrictEnumValueChecker
  finally:
    sys.path = original_sys_path

  return StrictEnumValueChecker(
      input_api,
      output_api,
      start_marker='enum APIPermissionID {',
      end_marker='};',
      path='extensions/common/mojom/api_permission_id.mojom',
  ).Run()
