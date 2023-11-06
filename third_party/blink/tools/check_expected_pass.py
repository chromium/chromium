#!/usr/bin/env vpython3
#
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Check if a web test expected file is an all-PASS testharness result.

web_tests/PRESUBMIT.py uses this script to identify generic all-PASS
testharness/wdspec baselines, which are redundant because run_web/wpt_tests.py
assumes all-PASS results for them when baselines are not found.
"""

import sys

from blinkpy.web_tests.models.testharness_results import is_all_pass_test_result

paths = []

if len(sys.argv) == 3 and sys.argv[1] == '--path-files':
    with open(sys.argv[2]) as f:
        filelist = [x.strip() for x in f.readlines()]
else:
    filelist = sys.argv[1:]

for path in filelist:
    # Call .decode() with errors="ignore" because there are a few files that
    # are invalid UTF-8 and will otherwise trigger decode exceptions.
    content = open(path, 'rb').read().decode(errors="ignore")
    if is_all_pass_test_result(content):
        paths.append(path)

if len(paths) > 0:
    sys.stderr.write(
        '* The following files are passing test results without console error messages, they should be removed:\n '
    )
    sys.stderr.write('\n '.join(paths))
    sys.stderr.write('\n')
    sys.exit(
        "ERROR: found passing test results without console error messages."
    )
