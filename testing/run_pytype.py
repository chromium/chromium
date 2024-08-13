#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple helper script to run pytype on //testing code."""

import os
import sys

from pytype_common import pytype_runner

TESTING_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.realpath(os.path.join(TESTING_DIR, '..'))

EXTRA_PATHS_COMPONENTS = [
    ('third_party', 'catapult', 'third_party', 'typ'),
]
EXTRA_PATHS = [
    os.path.join(CHROMIUM_SRC_DIR, *p) for p in EXTRA_PATHS_COMPONENTS
]
EXTRA_PATHS.append(TESTING_DIR)

FILES_AND_DIRECTORIES_TO_CHECK = [
    'unexpected_passes_common',
    'flake_suppressor_common',
]
FILES_AND_DIRECTORIES_TO_CHECK = [
    os.path.join(TESTING_DIR, f) for f in FILES_AND_DIRECTORIES_TO_CHECK
]

TEST_NAME = 'testing_pytype'
TEST_LOCATION = '//testing/run_pytype.py'


def main() -> int:
  return pytype_runner.run_pytype(TEST_NAME, TEST_LOCATION,
                                  FILES_AND_DIRECTORIES_TO_CHECK, EXTRA_PATHS,
                                  TESTING_DIR)


if __name__ == '__main__':
  sys.exit(main())
