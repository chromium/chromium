# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script that runs tests before uploading a patch."""


def _RunTests(input_api, output_api):
    """Runs all test files in the directory."""
    return input_api.canned_checks.RunUnitTests(input_api, output_api, [
        input_api.os_path.join(input_api.PresubmitLocalPath(),
                               'fuzz_integration_test.py')
    ])


def CheckChangeOnUpload(input_api, output_api):
    return _RunTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _RunTests(input_api, output_api)
