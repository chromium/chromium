# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for ios/tools.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckChange(input_api, output_api):
    if input_api.is_windows:
        return []
    return input_api.canned_checks.RunUnitTestsInDirectory(
        input_api,
        output_api,
        input_api.PresubmitLocalPath(),
        files_to_check=[r'.*_unittest\.py$'],
    )
