# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
from typing import Any, List, Set
import unittest

from gpu_tests import gpu_integration_test
from gpu_tests import webgpu_cts_integration_test_base

import gpu_path_util

# TODO(crbug.com/dawn/1996): Change this to the actual compat expectation file
# once it is available.
EXPECTATIONS_FILE = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'third_party',
                                 'dawn', 'webgpu-cts', 'expectations.txt')


class WebGpuCompatCtsIntegrationTest(
    webgpu_cts_integration_test_base.WebGpuCtsIntegrationTestBase):
  @classmethod
  def UseWebGpuCompatMode(cls) -> bool:
    return True

  @classmethod
  def Name(cls) -> str:
    return 'webgpu_compat_cts'

  def _GetSerialGlobs(self) -> Set[str]:
    return set()

  def _GetSerialTests(self) -> Set[str]:
    return set()

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return []


def load_tests(_loader: unittest.TestLoader, _tests: Any,
               _pattern: Any) -> unittest.TestSuite:
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
