#!/usr/bin/env vpython
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Check if a web test expected file is an all-PASS testharness result.

web_tests/PRESUBMIT.py uses this script to identify generic all-PASS
testharness baselines, which are redundant because run_web_tests.py assumes
all-PASS results for testharness tests when baselines are not found.
"""

import sys

from blinkpy.web_tests.models.testharness_results import is_all_pass_testharness_result

paths = []

for path in sys.argv[1:]:
    content = open(path, 'r').read()
    if is_all_pass_testharness_result(content):
        paths.append(path)

if len(paths) > 0:
    sys.stderr.write(
        '* The following files are passing testharness results without console error messages, they should be removed:\n '
    )
    sys.stderr.write('\n '.join(paths))
    sys.stderr.write('\n')
    sys.exit(
        "ERROR: found passing testharness results without console error messages."
    )
