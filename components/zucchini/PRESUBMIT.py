# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs tests for the Zucchini Python scripts."""

PRESUBMIT_VERSION = '2.0.0'


def CheckPython(input_api, output_api):
    if not input_api.HasAffectedFiles(extensions='.py'):
        return []
    results = []
    results.extend(
        input_api.canned_checks.RunUnitTestsInDirectory(
            input_api,
            output_api,
            directory='.',
            files_to_check=[r'.+_unittest\.py$'],
        )
    )
    results.extend(input_api.canned_checks.RunPylint(input_api, output_api))
    return results
