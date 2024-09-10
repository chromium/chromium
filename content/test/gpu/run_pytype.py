#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple helper script to run pytype on GPU Python code."""

import os
import sys

# We can't depend on gpu_path_util, otherwise pytype's dependency graph ends up
# finding a cycle.
GPU_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.realpath(os.path.join(GPU_DIR, '..', '..', '..'))

sys.path.append(os.path.join(CHROMIUM_SRC_DIR, 'testing'))

from pytype_common import pytype_runner  # pylint: disable=wrong-import-position

# This list should be kept in sync with EXTRA_PATH_COMPONENTS in PRESUBMIT.py
EXTRA_PATHS_COMPONENTS = [
    ('build', ),
    ('build', 'fuchsia', 'test'),
    ('build', 'util'),
    ('testing', ),
    ('third_party', 'catapult', 'common', 'py_utils'),
    ('third_party', 'catapult', 'devil'),
    ('third_party', 'catapult', 'telemetry'),
    ('third_party', 'catapult', 'third_party', 'typ'),
    ('tools', 'perf'),
]
EXTRA_PATHS = [
    os.path.join(CHROMIUM_SRC_DIR, *p) for p in EXTRA_PATHS_COMPONENTS
]
EXTRA_PATHS.append(GPU_DIR)

FILES_AND_DIRECTORIES_TO_CHECK = [
    # Directories.
    'bad_machine_finder',
    'flake_suppressor',
    'gold_inexact_matching',
    'gpu_tests',
    'machine_times',
    'unexpected_passes',
    # Files.
    'find_bad_machines.py',
    'get_machine_times.py',
    'unexpected_pass_finder.py',
]
FILES_AND_DIRECTORIES_TO_CHECK = [
    os.path.join(GPU_DIR, f) for f in FILES_AND_DIRECTORIES_TO_CHECK
]

TEST_NAME = 'gpu_pytype'
TEST_LOCATION = '//content/test/gpu/run_pytype.py'


def main() -> int:
  return pytype_runner.run_pytype(TEST_NAME, TEST_LOCATION,
                                  FILES_AND_DIRECTORIES_TO_CHECK, EXTRA_PATHS,
                                  GPU_DIR)


if __name__ == '__main__':
  sys.exit(main())
