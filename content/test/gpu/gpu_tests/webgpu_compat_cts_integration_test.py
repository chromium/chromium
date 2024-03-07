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

EXPECTATIONS_FILE = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'third_party',
                                 'dawn', 'webgpu-cts',
                                 'compat-expectations.txt')


class WebGpuCompatCtsIntegrationTest(
    webgpu_cts_integration_test_base.WebGpuCtsIntegrationTestBase):
  @classmethod
  def UseWebGpuCompatMode(cls) -> bool:
    return True

  @classmethod
  def Name(cls) -> str:
    return 'webgpu_compat_cts'

  def _GetSerialGlobs(self) -> Set[str]:
    serial_globs = super()._GetSerialGlobs()
    return serial_globs

  def _GetSerialTests(self) -> Set[str]:
    serial_tests = super()._GetSerialTests()
    return serial_tests

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [EXPECTATIONS_FILE]


def load_tests(_loader: unittest.TestLoader, _tests: Any,
               _pattern: Any) -> unittest.TestSuite:
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
