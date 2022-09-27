#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple helper script to run pytype on //third_party/blink/tools/ code."""

import os
import sys

BLINK_TOOLS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(BLINK_TOOLS_DIR, '..', '..', '..'))

sys.path.append(os.path.join(CHROMIUM_SRC_DIR, 'testing'))

from pytype_common import pytype_runner

EXTRA_PATHS_COMPONENTS = [('testing', )]
EXTRA_PATHS = [
    os.path.join(CHROMIUM_SRC_DIR, *p) for p in EXTRA_PATHS_COMPONENTS
]
EXTRA_PATHS.append(BLINK_TOOLS_DIR)

FILES_AND_DIRECTORIES_TO_CHECK = [
    os.path.join('blinkpy', 'web_tests', 'stale_expectation_removal'),
]
FILES_AND_DIRECTORIES_TO_CHECK = [
    os.path.join(BLINK_TOOLS_DIR, f) for f in FILES_AND_DIRECTORIES_TO_CHECK
]

TEST_NAME = 'blinkpy_pytype'
TEST_LOCATION = "//third_party/blink/tools/run_pytype.py"


def main() -> int:
    return pytype_runner.run_pytype(TEST_NAME, TEST_LOCATION,
                                    FILES_AND_DIRECTORIES_TO_CHECK,
                                    EXTRA_PATHS, BLINK_TOOLS_DIR)


if __name__ == '__main__':
    sys.exit(main())
