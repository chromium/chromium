# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))

CATAPULT_DIR = os.path.join(CHROMIUM_SRC_DIR, 'third_party', 'catapult')

GPU_DIR = os.path.join(CHROMIUM_SRC_DIR, 'content', 'test', 'gpu')
_GPU_DATA_RELATIVE_PATH_COMPONENTS = ('content', 'test', 'data', 'gpu')
GPU_DATA_RELATIVE_PATH = os.path.join(*_GPU_DATA_RELATIVE_PATH_COMPONENTS)
GPU_DATA_DIR = os.path.join(CHROMIUM_SRC_DIR,
                            *_GPU_DATA_RELATIVE_PATH_COMPONENTS)
GPU_TESTS_DIR = os.path.join(GPU_DIR, 'gpu_tests')
GPU_EXPECTATIONS_DIR = os.path.join(GPU_TESTS_DIR, 'test_expectations')
GPU_TEST_HARNESS_JAVASCRIPT_DIR = os.path.join(GPU_TESTS_DIR, 'javascript')

TOOLS_PERF_DIR = os.path.join(CHROMIUM_SRC_DIR, 'tools', 'perf')


# pylint: disable=no-value-for-parameter
def AddDirToPathIfNeeded(*path_parts):
  path = os.path.abspath(os.path.join(*path_parts))
  if os.path.isdir(path) and path not in sys.path:
    sys.path.append(path)


# pylint: enable=no-value-for-parameter
