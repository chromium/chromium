# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script that runs tests before uploading a patch."""

USE_PYTHON3 = True


def _RunTests(input_api, output_api):
    """Runs all test files in the directory."""
    cmd_name = 'all_python_tests'
    cmd = ['python', '-m', 'unittest', 'discover', '-p', '*test.py']
    test_cmd = input_api.Command(
        name=cmd_name, cmd=cmd, kwargs={}, message=output_api.PresubmitError)
    if input_api.verbose:
        print('Running ' + cmd_name)
    return input_api.RunTests([test_cmd])


def CheckChangeOnUpload(input_api, output_api):
    return _RunTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _RunTests(input_api, output_api)
