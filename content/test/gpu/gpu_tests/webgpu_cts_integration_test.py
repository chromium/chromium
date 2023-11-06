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
                                 'dawn', 'webgpu-cts', 'expectations.txt')


class WebGpuCtsIntegrationTest(
    webgpu_cts_integration_test_base.WebGpuCtsIntegrationTestBase):
  @classmethod
  def UseWebGpuCompatMode(cls) -> bool:
    return False

  @classmethod
  def Name(cls) -> str:
    return 'webgpu_cts'

  def _GetSerialGlobs(self) -> Set[str]:
    globs = {
        # crbug.com/1406799. Large test.
        # Run serially to avoid impact on other tests.
        '*:api,operation,rendering,basic:large_draw:*',
    }

    # crbug.com/dawn/1500. Flaky tests on Mac-Intel when using 16 byte formats
    # in parallel.
    FORMATS_WITH_16_BYTE_BLOCKS = [
        # Basic color formats
        'rgba32uint',
        'rgba32sint',
        'rgba32float',
        # BC compression formats
        'bc2-rgba-unorm',
        'bc2-rgba-unorm-srgb',
        'bc3-rgba-unorm',
        'bc3-rgba-unorm-srgb',
        'bc5-rg-unorm',
        'bc5-rg-snorm',
        'bc6h-rgb-ufloat',
        'bc6h-rgb-float',
        'bc7-rgba-unorm',
        'bc7-rgba-unorm-srgb',
        # ETC2 compression formats
        'etc2-rgba8unorm',
        'etc2-rgba8unorm-srgb',
        'eac-rg11unorm',
        'eac-rg11snorm',
        # ASTC compression formats
        'astc-4x4-unorm',
        'astc-4x4-unorm-srgb',
        'astc-5x4-unorm',
        'astc-5x4-unorm-srgb',
        'astc-5x5-unorm',
        'astc-5x5-unorm-srgb',
        'astc-6x5-unorm',
        'astc-6x5-unorm-srgb',
        'astc-6x6-unorm',
        'astc-6x6-unorm-srgb',
        'astc-8x5-unorm',
        'astc-8x5-unorm-srgb',
        'astc-8x6-unorm',
        'astc-8x6-unorm-srgb',
        'astc-8x8-unorm',
        'astc-8x8-unorm-srgb',
        'astc-10x5-unorm',
        'astc-10x5-unorm-srgb',
        'astc-10x6-unorm',
        'astc-10x6-unorm-srgb',
        'astc-10x8-unorm',
        'astc-10x8-unorm-srgb',
        'astc-10x10-unorm',
        'astc-10x10-unorm-srgb',
        'astc-12x10-unorm',
        'astc-12x10-unorm-srgb',
        'astc-12x12-unorm',
        'astc-12x12-unorm-srgb'
    ]
    for f in FORMATS_WITH_16_BYTE_BLOCKS:
      globs.add((
          '*:api,operation,command_buffer,image_copy:origins_and_extents:'
          'initMethod="WriteTexture";checkMethod="PartialCopyT2B";format="%s";*'
      ) % f)

    # Run shader tests in serial on Mac.
    # The Metal shader compiler tends to be slow.
    if sys.platform == 'darwin':
      globs.add('webgpu:shader,execution*')

    # Run limit tests in serial if backend validation is enabled on Windows.
    # The validation layers add memory overhead which makes OOM likely when
    # many browsers and tests run in parallel.
    if sys.platform == 'win32' and self._enable_dawn_backend_validation:
      globs.add('webgpu:api,validation,capability_checks,limits*')
      globs.add('webgpu:api,validation,state,device_lost*')

    return globs

  def _GetSerialTests(self) -> Set[str]:
    return set()

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [EXPECTATIONS_FILE]


def load_tests(_loader: unittest.TestLoader, _tests: Any,
               _pattern: Any) -> unittest.TestSuite:
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
