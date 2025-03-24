# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
from typing import Any
import unittest

from telemetry.internal.platform import gpu_info as telemetry_gpu_info

from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test
from gpu_tests import webgpu_cts_integration_test_base

import gpu_path_util

EXPECTATIONS_FILE = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'third_party',
                                 'dawn', 'webgpu-cts',
                                 'compat-expectations.txt')

class WebGpuCompatCtsIntegrationTest(
    webgpu_cts_integration_test_base.WebGpuCtsIntegrationTestBase):
  _use_min_es31 = False

  @classmethod
  def UseWebGpuCompatMode(cls) -> bool:
    return True

  @classmethod
  def Name(cls) -> str:
    return 'webgpu_compat_cts'

  def _GetSerialGlobs(self) -> set[str]:
    serial_globs = super()._GetSerialGlobs()
    return serial_globs

  def _GetSerialTests(self) -> set[str]:
    serial_tests = super()._GetSerialTests()
    return serial_tests

  @classmethod
  def ExpectationsFiles(cls) -> list[str]:
    return [EXPECTATIONS_FILE]

  @classmethod
  def GetPlatformTags(cls, browser: ct.Browser) -> list[str]:
    tags = super().GetPlatformTags(browser)
    if cls._use_min_es31:
      tags.append('compat-min-es31')
    else:
      tags.append('compat-default')
    return tags

  @classmethod
  def _DetermineExpectedFeatureValues(cls) -> None:
    super()._DetermineExpectedFeatureValues()
    browser_options = cls._finder_options.browser_options

    if not browser_options or not browser_options.extra_browser_args:
      return

    for arg in browser_options.extra_browser_args:
      if arg.startswith('--enable-dawn-features='):
        values = arg[len('--enable-dawn-features='):]
        for feature in values.split(','):
          if feature == 'gl_force_es_31_and_no_extensions':
            cls._use_min_es31 = True

  @classmethod
  def _VerifyBrowserFeaturesMatchExpectedValues(cls) -> None:
    super()._VerifyBrowserFeaturesMatchExpectedValues()
    gpu_info = cls.browser.GetSystemInfo().gpu
    cls._VerifyWebGPUCompatBackend(gpu_info)

  # pylint: disable=unused-argument
  @classmethod
  def _VerifyWebGPUCompatBackend(cls,
                                 gpu_info: telemetry_gpu_info.GPUInfo) -> None:
    """Verifies that WebGPU's compat ANGLE backend is OpenGL ES 3.1"""
    # TODO(crbug.com/388318201): Verify WebGPU Compat backend GLES version
    return

def load_tests(_loader: unittest.TestLoader, _tests: Any,
               _pattern: Any) -> unittest.TestSuite:
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
