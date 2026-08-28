# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs tests and lint checks for the Zucchini seed scripts."""

PRESUBMIT_VERSION = '2.0.0'

_PYTHON_TEST_INPUTS = {
    'BUILD.gn',
    'file_pair.proto',
}


def _should_run_python_tests(input_api):
    presubmit_dir = input_api.PresubmitLocalPath()
    for affected_file in input_api.AffectedFiles():
        path = input_api.os_path.relpath(affected_file.AbsoluteLocalPath(),
                                         presubmit_dir)
        if path == '..' or path.startswith('..' + input_api.os_path.sep):
            continue
        if path.endswith('.py') or path in _PYTHON_TEST_INPUTS:
            return True
    return False


def CheckPython(input_api, output_api):
    checks = []
    if _should_run_python_tests(input_api):
        checks.extend(
            input_api.canned_checks.GetUnitTestsInDirectory(
                input_api,
                output_api,
                directory='.',
                files_to_check=[r'.+_test\.py$'],
            ))
    checks.extend(
        input_api.canned_checks.GetPylint(
            input_api,
            output_api,
            files_to_check=[r'.+\.py$'],
            version='3.2',
        ))
    return input_api.RunTests(checks)
