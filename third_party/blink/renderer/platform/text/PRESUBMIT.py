# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for third_party/blink/renderer/platform/text."""

PRESUBMIT_VERSION = '2.0.0'


def CheckPythonUnittestsPass(input_api, output_api):
    if not input_api.HasAffectedFiles(extensions='.py'):
        return []
    return input_api.RunTests(
        input_api.canned_checks.GetUnitTestsInDirectory(
            input_api,
            output_api,
            input_api.PresubmitLocalPath(),
            files_to_check=[r'.+_unittest\.py$'],
        )
    )
