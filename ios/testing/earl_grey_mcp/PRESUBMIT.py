# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for ios/testing/earl_grey_mcp."""

PRESUBMIT_VERSION = '2.0.0'


def CheckChange(input_api, output_api):
    """Runs all Python unit tests under scripts/."""
    scripts_dir = input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'scripts'
    )
    return input_api.canned_checks.RunUnitTestsInDirectory(
        input_api,
        output_api,
        scripts_dir,
        files_to_check=[r'^.+_unittest\.py$'],
    )
