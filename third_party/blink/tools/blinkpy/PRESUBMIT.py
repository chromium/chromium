# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""blinkpy/ presubmit script.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import inspect
import sys


def CheckChangeOnUpload(input_api, output_api):
    results = []
    os_path = input_api.os_path
    sys.path.append(
        os_path.join(
            os_path.dirname(
                os_path.abspath(inspect.getfile(CheckChangeOnUpload))), '..'))
    from blinkpy.presubmit.lint_test_expectations import (
        PresubmitCheckTestExpectations)
    results.extend(PresubmitCheckTestExpectations(input_api, output_api))
    return results
