# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
from typing import Any, List, Optional, Union
import unittest

import dataclasses  # Built-in, but pylint gives an ordering false positive.

from gpu_tests import common_typing as ct
from gpu_tests import constants
from gpu_tests import gpu_integration_test
from gpu_tests import overlay_support
from gpu_tests.util import host_information

from telemetry.internal.platform import gpu_info as gi


@dataclasses.dataclass
class InfoCollectionTestArgs():
  """Struct-like class for passing args to an InfoCollection test."""
  expected_vendor_id_str: Optional[str] = None
  expected_device_id_strs: Optional[List[str]] = None
  gpu: Optional[gi.GPUInfo] = None


class InfoCollectionTest(gpu_integration_test.GpuIntegrationTest):
  @classmethod
  def Name(cls) -> str:
    return 'info_collection'

  @classmethod
  def AddCommandlineArgs(cls, parser: ct.CmdArgParser) -> None:
    super(InfoCollectionTest, cls).AddCommandlineArgs(parser)
    parser.add_argument(
        '--expected-device-id',
        action='append',
        dest='expected_device_ids',
        default=[],
        help='The expected device id. Can be specified multiple times.')
    parser.add_argument('--expected-vendor-id', help='The expected vendor id')

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    yield ('InfoCollection_basic', '_', [
        '_RunBasicTest',
        InfoCollectionTestArgs(
            expected_vendor_id_str=options.expected_vendor_id,
            expected_device_id_strs=options.expected_device_ids)
    ])
    yield ('InfoCollection_direct_composition', '_',
           ['_RunDirectCompositionTest',
            InfoCollectionTestArgs()])
    yield ('InfoCollection_dx12_vulkan', '_',
           ['_RunDX12VulkanTest',
            InfoCollectionTestArgs()])
    yield ('InfoCollection_asan_info_surfaced', '_',
           ['_RunAsanInfoTest', InfoCollectionTestArgs()])
    yield ('InfoCollection_clang_coverage_info_surfaced', '_',
           ['_RunClangCoverageInfoTest',
            InfoCollectionTestArgs()])
    yield ('InfoCollection_host_information_matches_browser', '_', [
        '_RunHostInformationTest',
        InfoCollectionTestArgs(
            expected_vendor_id_str=options.expected_vendor_id,
            expected_device_id_strs=options.expected_device_ids)
    ])

  @classmethod
  def SetUpProcess(cls) -> None:
    super(cls, InfoCollectionTest).SetUpProcess()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    del test_path  # Unused in this particular GPU test.
    # Make sure the GPU process is started
    self.tab.action_runner.Navigate('chrome:gpu')

    # Gather the IDs detected by the GPU process
    system_info = self.browser.GetSystemInfo()
    if not system_info:
      self.fail("Browser doesn't support GetSystemInfo")

    assert len(args) == 2
    test_func = args[0]
    test_args = args[1]
    test_args.gpu = system_info.gpu
    getattr(self, test_func)(test_args)

  ######################################
  # Helper functions for the tests below

  def _RunBasicTest(self, test_args: InfoCollectionTestArgs) -> None:
    device = test_args.gpu.devices[0]
    if not device:
      self.fail("System Info doesn't have a gpu")

    detected_vendor_id = device.vendor_id
    detected_device_id = device.device_id

    # Gather the expected IDs passed on the command line
    if (not test_args.expected_vendor_id_str
        or not test_args.expected_device_id_strs):
      self.fail('Missing --expected-[vendor|device]-id command line args')

    expected_vendor_id = int(test_args.expected_vendor_id_str, 16)
    expected_device_ids = [
        int(id_str, 16) for id_str in test_args.expected_device_id_strs
    ]

    # Check expected and detected GPUs match
    if detected_vendor_id != expected_vendor_id:
      self.fail('Vendor ID mismatch, expected %s but got %s.' %
                (expected_vendor_id, detected_vendor_id))

    if detected_device_id not in expected_device_ids:
      self.fail('Device ID mismatch, expected %s but got %s.' %
                (expected_device_ids, detected_device_id))

  def _RunDirectCompositionTest(self,
                                test_args: InfoCollectionTestArgs) -> None:
    os_name = self.browser.platform.GetOSName()
    if os_name and os_name.lower() == 'win':
      overlay_bot_config = overlay_support.GetOverlayConfigForGpu(
          test_args.gpu.devices[0])
      aux_attributes = test_args.gpu.aux_attributes
      if not aux_attributes:
        self.fail('GPU info does not have aux_attributes.')
      expected_values = {
          'direct_composition': overlay_bot_config.direct_composition,
          'supports_overlays': overlay_bot_config.supports_overlays,
          'nv12_overlay_support': overlay_bot_config.nv12_overlay_support,
          'yuy2_overlay_support': overlay_bot_config.yuy2_overlay_support,
          'bgra8_overlay_support': overlay_bot_config.bgra8_overlay_support,
      }
      for field, expected in expected_values.items():
        detected = aux_attributes.get(field, 'NONE')
        if expected != detected:
          self.fail(
              '%s mismatch, expected %s but got %s.' %
              (field, self._ValueToStr(expected), self._ValueToStr(detected)))

  def _RunDX12VulkanTest(self, _: InfoCollectionTestArgs) -> None:
    os_name = self.browser.platform.GetOSName()
    if os_name and os_name.lower() == 'win':
      self.RestartBrowserIfNecessaryWithArgs(
          ['--no-delay-for-dx12-vulkan-info-collection'])
      # Need to re-request system info for DX12/Vulkan bits.
      system_info = self.browser.GetSystemInfo()
      if not system_info:
        self.fail("Browser doesn't support GetSystemInfo")
      gpu = system_info.gpu
      if gpu is None:
        raise Exception("System Info doesn't have a gpu")
      aux_attributes = gpu.aux_attributes
      if not aux_attributes:
        self.fail('GPU info does not have aux_attributes.')

      dx12_vulkan_bot_config = self._GetDx12VulkanBotConfig()
      for field, expected in dx12_vulkan_bot_config.items():
        detected = aux_attributes.get(field)
        if expected != detected:
          self.fail(
              '%s mismatch, expected %s but got %s.' %
              (field, self._ValueToStr(expected), self._ValueToStr(detected)))

  def _RunAsanInfoTest(self, _: InfoCollectionTestArgs) -> None:
    gpu_info = self.browser.GetSystemInfo().gpu
    self.assertIn('is_asan', gpu_info.aux_attributes)

  def _RunClangCoverageInfoTest(self, _: InfoCollectionTestArgs) -> None:
    gpu_info = self.browser.GetSystemInfo().gpu
    self.assertIn('is_clang_coverage', gpu_info.aux_attributes)

  def _RunHostInformationTest(self, test_args: InfoCollectionTestArgs) -> None:
    # This is used to verify that the functions in host_information align with
    # the information we pull from the browser.
    tags = self.GetPlatformTags(self.browser)
    if any(os_tag in tags for os_tag in ('android', 'chromeos', 'fuchsia')):
      self.skipTest('Test does not support remote platforms')

    if 'win' in tags:
      self.assertTrue(host_information.IsWindows())
    elif 'linux' in tags:
      self.assertTrue(host_information.IsLinux())
    elif 'mac' in tags:
      self.assertTrue(host_information.IsMac())
    else:
      self.fail('Running on unknown platform')

    expected_vendor_id = int(test_args.expected_vendor_id_str, 16)
    if expected_vendor_id == constants.GpuVendor.QUALCOMM:
      self.assertTrue(host_information.IsArmCpu())
      self.assertFalse(host_information.Isx86Cpu())
      self.assertTrue(host_information.IsQualcommGpu())
    elif expected_vendor_id == constants.GpuVendor.APPLE:
      self.assertTrue(host_information.IsArmCpu())
      self.assertFalse(host_information.Isx86Cpu())
      self.assertTrue(host_information.IsAppleGpu())
    else:
      self.assertTrue(host_information.Isx86Cpu())
      self.assertFalse(host_information.IsArmCpu())
      if expected_vendor_id == constants.GpuVendor.AMD:
        self.assertTrue(host_information.IsAmdGpu())
      elif expected_vendor_id == constants.GpuVendor.INTEL:
        self.assertTrue(host_information.IsIntelGpu())
      elif expected_vendor_id == constants.GpuVendor.NVIDIA:
        self.assertTrue(host_information.IsNvidiaGpu())
      else:
        self.fail('Running with unknown GPU vendor')


  @staticmethod
  def _ValueToStr(value: Union[str, bool]) -> str:
    if isinstance(value, str):
      return value
    if isinstance(value, bool):
      return 'supported' if value else 'unsupported'
    assert False
    return False

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', 'info_collection_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
