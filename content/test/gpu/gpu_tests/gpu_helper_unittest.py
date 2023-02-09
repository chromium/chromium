#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import Dict, Optional, Union
import unittest

from gpu_tests import gpu_helper
from telemetry.internal.platform import gpu_info


# pylint: disable=too-many-arguments
def CreateGpuDeviceDict(vendor_id: Optional[str] = None,
                        device_id: Optional[str] = None,
                        sub_sys_id: Optional[int] = None,
                        revision: Optional[int] = None,
                        vendor_string: Optional[str] = None,
                        device_string: Optional[str] = None,
                        driver_vendor: Optional[str] = None,
                        driver_version: Optional[str] = None
                        ) -> Dict[str, Union[str, int]]:
  return {
      'vendor_id': vendor_id or 'vendor_id',
      'device_id': device_id or 'device_id',
      'sub_sys_id': sub_sys_id or 0,
      'revision': revision or 0,
      'vendor_string': vendor_string or 'vendor_string',
      'device_string': device_string or 'device_string',
      'driver_vendor': driver_vendor or 'driver_vendor',
      'driver_version': driver_version or 'driver_version',
  }


# pylint: enable=too-many-arguments


class TagHelpersUnittest(unittest.TestCase):

  # TODO(crbug.com/1413867): Add unittests for other tag generation helpers.
  def testGetClangCoverage(self) -> None:
    info = gpu_info.GPUInfo([CreateGpuDeviceDict()], {}, None, None)
    self.assertEqual(gpu_helper.GetClangCoverage(info), 'no-clang-coverage')
    info = gpu_info.GPUInfo([CreateGpuDeviceDict()],
                            {'is_clang_coverage': True}, None, None)
    self.assertEqual(gpu_helper.GetClangCoverage(info), 'clang-coverage')


class ReplaceTagsUnittest(unittest.TestCase):
  def testSubstringReplacement(self) -> None:
    tags = ['some_tag', 'some-nvidia-corporation', 'another_tag']
    self.assertEqual(gpu_helper.ReplaceTags(tags),
                     ['some_tag', 'some-nvidia', 'another_tag'])

  def testRegexReplacement(self) -> None:
    tags = [
        'some_tag',
        'google-Vulkan-1.3.0-(SwiftShader-Device-(LLVM-10.0.0)-(0x0000C0DE))',
        'another_tag'
    ]
    self.assertEqual(gpu_helper.ReplaceTags(tags),
                     ['some_tag', 'google-vulkan', 'another_tag'])


if __name__ == '__main__':
  unittest.main(verbosity=2)
