# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for testing/trigger_scripts.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True


def CommonChecks(input_api, output_api):
    return input_api.canned_checks.RunUnitTestsInDirectory(
        input_api,
        output_api,
        '.',
        files_to_check=['.*test.py'],
        run_on_python2=not USE_PYTHON3,
        run_on_python3=USE_PYTHON3,
        skip_shebang_check=True)


def CheckChangeOnUpload(input_api, output_api):
    return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return CommonChecks(input_api, output_api)
