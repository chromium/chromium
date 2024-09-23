#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import Any, Callable, Dict, List, Optional, Tuple, Union
import unittest
from unittest import mock

import dataclasses  # Built-in, but pylint gives an ordering false positive.

from gpu_tests import common_typing as ct
from gpu_tests import gpu_helper
from telemetry.internal.platform import gpu_info


# pylint: disable=too-many-arguments
def CreateGpuDeviceDict(vendor_id: Optional[int] = None,
                        device_id: Optional[int] = None,
                        sub_sys_id: Optional[int] = None,
                        revision: Optional[int] = None,
                        vendor_string: Optional[str] = None,
                        device_string: Optional[str] = None,
                        driver_vendor: Optional[str] = None,
                        driver_version: Optional[str] = None
                        ) -> Dict[str, Union[str, int]]:
  return {
      'vendor_id':
      vendor_id or 0,
      'device_id':
      device_id or 0,
      'sub_sys_id':
      sub_sys_id or 0,
      'revision':
      revision or 0,
      'vendor_string':
      'vendor_string' if vendor_string is None else vendor_string,
      'device_string':
      'device_string' if device_string is None else device_string,
      'driver_vendor':
      'driver_vendor' if driver_vendor is None else driver_vendor,
      'driver_version':
      'driver_version' if driver_version is None else driver_version,
  }


# pylint: enable=too-many-arguments


@dataclasses.dataclass
class TagHelperTestCase():
  """Struct-like class for defining a tag helper test case."""
  expected_result: Any
  device_dict: Dict[str, Union[str, int]] = ct.EmptyDict()
  aux_attributes: Dict[str, Any] = ct.EmptyDict()
  feature_status: Dict[str, str] = ct.EmptyDict()
  extra_browser_args: List[str] = ct.EmptyList()


class TagHelpersUnittest(unittest.TestCase):

  def runTagHelperTestWithIndex(
      self, tc: TagHelperTestCase,
      test_method: Callable[[Optional[gpu_info.GPUInfo], int], Any]) -> None:
    """Helper method for running a single tag helper test case w/ index."""
    info = gpu_info.GPUInfo([CreateGpuDeviceDict(**tc.device_dict)],
                            tc.aux_attributes, tc.feature_status, None)
    self.assertEqual(test_method(info, 0), tc.expected_result)

  def runTagHelperTest(self, tc: TagHelperTestCase,
                       test_method: Callable[[Optional[gpu_info.GPUInfo]], Any]
                       ) -> None:
    """Helper method for running a single tag helper test case w/o index."""
    info = gpu_info.GPUInfo([CreateGpuDeviceDict(**tc.device_dict)],
                            tc.aux_attributes, tc.feature_status, None)
    self.assertEqual(test_method(info), tc.expected_result)

  def testGetGpuVendorString(self) -> None:
    """Tests all code paths for the GetGpuVendorString() method."""
    cases = [
        # Explicit ID -> AMD.
        TagHelperTestCase(
            'amd', {
                'vendor_id': 0x1002,
                'device_string': 'ANGLE (ANGLE gpu, 1, 2)',
                'vendor_string': 'Vendor_gpu 1 2'
            }),
        # Explicit ID -> Intel.
        TagHelperTestCase(
            'intel', {
                'vendor_id': 0x8086,
                'device_string': 'ANGLE (ANGLE gpu, 1, 2)',
                'vendor_string': 'Vendor_gpu 1 2'
            }),
        # Explicit ID -> NVIDIA.
        TagHelperTestCase(
            'nvidia', {
                'vendor_id': 0x10DE,
                'device_string': 'ANGLE (ANGLE gpu, 1, 2)',
                'vendor_string': 'Vendor_gpu 1 2'
            }),
        # ANGLE vendor string.
        TagHelperTestCase(
            'angle gpu', {
                'device_string': 'ANGLE (ANGLE gpu, 1, 2)',
                'vendor_string': 'Vendor_gpu 1 2'
            }),
        # Vendor string.
        TagHelperTestCase('vendor_gpu', {'vendor_string': 'Vendor_gpu 1 2'}),
        # Defined info but unknown.
        TagHelperTestCase('unknown_gpu', {'vendor_string': ''}),
    ]

    for tc in cases:
      self.runTagHelperTestWithIndex(tc, gpu_helper.GetGpuVendorString)

    # Undefined info.
    self.assertEqual(gpu_helper.GetGpuVendorString(None, 0), 'unknown_gpu')

  def testGetGpuDeviceId(self) -> None:
    """Tests all code paths for the GetGpuDeviceId() method."""
    cases = [
        # Explicit device.
        TagHelperTestCase(0xFFFF, {
            'device_id': 0xFFFF,
            'device_string': 'ANGLE (Vendor, Device, Driver)'
        }),
        # ANGLE device string.
        TagHelperTestCase('Device',
                          {'device_string': 'ANGLE (Vendor, Device, Driver)'}),
        # Device string.
        TagHelperTestCase('Some device', {'device_string': 'Some device'}),
    ]

    for tc in cases:
      self.runTagHelperTestWithIndex(tc, gpu_helper.GetGpuDeviceId)

    # Undefined info.
    self.assertEqual(gpu_helper.GetGpuDeviceId(None, 0), 0)

  def testIntelMasks(self) -> None:
    """Tests the masking methods for determining Intel generation."""
    # Sample of real IDs taken from
    # https://dgpu-docs.intel.com/devices/hardware-table.html
    # Note that 12th gen is referred to as "Xe" or "XeHPG.
    gen_9_ids = {0x1923, 0x3184, 0x3EA4, 0x591C, 0x5A85, 0x9BC8}
    # 0x4F and 0xA7-prefixed samples missing since none were listed.
    gen_12_ids = {0x4C8A, 0x9A40, 0x4905, 0x4680, 0x5690}

    for pci_id in gen_9_ids:
      self.assertTrue(gpu_helper.IsIntelGen9(pci_id))
      self.assertFalse(gpu_helper.IsIntelGen12(pci_id))

    for pci_id in gen_12_ids:
      self.assertTrue(gpu_helper.IsIntelGen12(pci_id))
      self.assertFalse(gpu_helper.IsIntelGen9(pci_id))

  def testGetGpuDriverVendor(self) -> None:
    """Tests all code paths for the GetGpuDriverVendor() method."""
    # Explicit vendor.
    self.runTagHelperTest(
        TagHelperTestCase('vendor', {'driver_vendor': 'vendor'}),
        gpu_helper.GetGpuDriverVendor)
    # Undefined info.
    self.assertEqual(gpu_helper.GetGpuDriverVendor(None), None)

  def testGetGpuDriverVersion(self) -> None:
    """Tests all code paths for the GetGpuDriverVersion() method."""
    # Explicit version.
    self.runTagHelperTest(
        TagHelperTestCase('Some version', {'driver_version': 'Some version'}),
        gpu_helper.GetGpuDriverVersion)
    # Undefined info.
    self.assertEqual(gpu_helper.GetGpuDriverVersion(None), None)

  def testGetANGLERenderer(self) -> None:
    """Tests all code paths for the GetANGLERenderer() method."""
    cases = [
        # No aux attributes.
        TagHelperTestCase('angle-disabled'),
        # Non-ANGLE renderer.
        TagHelperTestCase('angle-disabled',
                          aux_attributes={'gl_renderer': 'renderer'}),
        # D3D11.
        TagHelperTestCase('angle-d3d11',
                          aux_attributes={'gl_renderer': 'ANGLE Direct3D11'}),
        # D3D9.
        TagHelperTestCase('angle-d3d9',
                          aux_attributes={'gl_renderer': 'ANGLE Direct3D9'}),
        # OpenGL ES.
        TagHelperTestCase('angle-opengles',
                          aux_attributes={'gl_renderer': 'ANGLE OpenGL ES'}),
        # OpenGL.
        TagHelperTestCase('angle-opengl',
                          aux_attributes={'gl_renderer': 'ANGLE OpenGL'}),
        # Metal.
        TagHelperTestCase('angle-metal',
                          aux_attributes={'gl_renderer': 'ANGLE Metal'}),
        # SwiftShader, explicitly test that it's chosen over Vulkan.
        TagHelperTestCase(
            'angle-swiftshader',
            aux_attributes={'gl_renderer': 'ANGLE Vulkan SwiftShader'}),
        # Vulkan.
        TagHelperTestCase('angle-vulkan',
                          aux_attributes={'gl_renderer': 'ANGLE Vulkan'}),
    ]

    for tc in cases:
      self.runTagHelperTest(tc, gpu_helper.GetANGLERenderer)

    # Undefined info.
    self.assertEqual(gpu_helper.GetANGLERenderer(None), 'angle-disabled')

  def testGetCommandDecoder(self) -> None:
    """Tests all code paths for the GetcommandDecoder() method."""
    cases = [
        # No aux attributes.
        TagHelperTestCase('no_passthrough'),
        # Validating.
        TagHelperTestCase('no_passthrough',
                          aux_attributes={'passthrough_cmd_decoder': False}),
        # Passthrough.
        TagHelperTestCase('passthrough',
                          aux_attributes={'passthrough_cmd_decoder': True}),
    ]

    for tc in cases:
      self.runTagHelperTest(tc, gpu_helper.GetCommandDecoder)

    # Undefined info.
    self.assertEqual(gpu_helper.GetCommandDecoder(None), 'no_passthrough')

  def testGetSkiaGraphiteStatus(self) -> None:
    """Tests all the code paths for the GetSkiaGraphiteStatus() method."""
    cases = [
        # No feature status.
        TagHelperTestCase('graphite-disabled'),
        # Feature status off.
        TagHelperTestCase('graphite-disabled',
                          feature_status={'skia_graphite': 'disabled'}),
        # Feature status on.
        TagHelperTestCase('graphite-enabled',
                          feature_status={'skia_graphite': 'enabled_on'}),
    ]

    for tc in cases:
      self.runTagHelperTest(tc, gpu_helper.GetSkiaGraphiteStatus)

    # Undefined info.
    self.assertEqual(gpu_helper.GetSkiaGraphiteStatus(None),
                     'graphite-disabled')

  def testGetSkiaRenderer(self) -> None:
    """Tests all code paths for the GetSkiaRenderer() method."""
    cases = [
        # No feature status.
        TagHelperTestCase('renderer-software'),
        # No GPU Compositing.
        TagHelperTestCase('renderer-software',
                          feature_status={'gpu_compositing': 'disabled'}),
        # No renderer.
        TagHelperTestCase('renderer-software',
                          feature_status={'gpu_compositing': 'enabled'}),
        # Vulkan Skia Renderer.
        TagHelperTestCase('renderer-skia-vulkan',
                          feature_status={
                              'gpu_compositing': 'enabled',
                              'vulkan': 'enabled_on',
                              'opengl': 'enabled_on'
                          }),
        # GL Skia Renderer.
        TagHelperTestCase('renderer-skia-gl',
                          feature_status={
                              'gpu_compositing': 'enabled',
                              'vulkan': 'enabled_off',
                              'opengl': 'enabled_on'
                          }),
    ]

    for tc in cases:
      self.runTagHelperTest(tc, gpu_helper.GetSkiaRenderer)

    # Undefined info.
    self.assertEqual(gpu_helper.GetSkiaRenderer(None), 'renderer-software')

  def testGetDisplayServer(self) -> None:
    """Tests all code paths for the GetDisplayServer() method."""
    with mock.patch('gpu_tests.util.host_information.IsLinux',
                    return_value=True):
      # Remote platforms.
      for browser_type in gpu_helper.REMOTE_BROWSER_TYPES:
        self.assertEqual(gpu_helper.GetDisplayServer(browser_type), None)
      # X.
      with mock.patch.dict('os.environ', {}, clear=True):
        self.assertEqual(gpu_helper.GetDisplayServer(''), 'display-server-x')
      # Wayland.
      with mock.patch.dict('os.environ', {'WAYLAND_DISPLAY': '1'}, clear=True):
        self.assertEqual(gpu_helper.GetDisplayServer(''),
                         'display-server-wayland')

    with mock.patch('gpu_tests.util.host_information.IsLinux',
                    return_value=False):
      self.assertEqual(gpu_helper.GetDisplayServer(''), None)

  def testGetOOPCanvasStatus(self) -> None:
    """Tests all the code paths for the GetOOPCanvasStatus() method."""
    cases = [
        # No feature status.
        TagHelperTestCase('no-oop-c'),
        # Feature status off.
        TagHelperTestCase(
            'no-oop-c',
            feature_status={'canvas_oop_rasterization': 'enabled_off'}),
        # Feature status on.
        TagHelperTestCase(
            'oop-c', feature_status={'canvas_oop_rasterization': 'enabled_on'}),
    ]

    for tc in cases:
      self.runTagHelperTest(tc, gpu_helper.GetOOPCanvasStatus)

    # Undefined info.
    self.assertEqual(gpu_helper.GetOOPCanvasStatus(None), 'no-oop-c')

  def testGetAsanStatus(self) -> None:
    """Tests all code paths for the GetAsanStatus() method."""
    cases = [
        # No aux attributes.
        TagHelperTestCase('no-asan'),
        # Built without ASan.
        TagHelperTestCase('no-asan', aux_attributes={'is_asan': False}),
        # Built with ASan.
        TagHelperTestCase('asan', aux_attributes={'is_asan': True}),
    ]

    for tc in cases:
      self.runTagHelperTest(tc, gpu_helper.GetAsanStatus)

    # Undefined info.
    self.assertEqual(gpu_helper.GetAsanStatus(None), 'no-asan')

  def testGetTargetCpuStatus(self) -> None:
    """Tests all code paths for the GetTargetCpuStatus() method."""
    cases = [
        # No aux attributes.
        TagHelperTestCase('target-cpu-unknown'),
        # Target CPU specified.
        TagHelperTestCase('target-cpu-32',
                          aux_attributes={'target_cpu_bits': 32}),
    ]

    for tc in cases:
      self.runTagHelperTest(tc, gpu_helper.GetTargetCpuStatus)

    # Undefined info.
    self.assertEqual(gpu_helper.GetTargetCpuStatus(None), 'target-cpu-unknown')

  def testGetClangCoverage(self) -> None:
    """Tests all code paths for the GetClangCoverage() method."""
    cases = [
        # No aux attributes.
        TagHelperTestCase('no-clang-coverage'),
        # Built without Clang coverage.
        TagHelperTestCase('no-clang-coverage',
                          aux_attributes={'is_clang_coverage': False}),
        # Built with Clang coverage.
        TagHelperTestCase('clang-coverage',
                          aux_attributes={'is_clang_coverage': True}),
    ]

    for tc in cases:
      self.runTagHelperTest(tc, gpu_helper.GetClangCoverage)

    # Undefined info.
    self.assertEqual(gpu_helper.GetClangCoverage(None), 'no-clang-coverage')


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


class EvaluateVersionComparisonUnittest(unittest.TestCase):
  def testWindowsIntelUncomparableVersions(self) -> None:
    """Tests Windows Intel comparison when versions cannot be compared."""
    non_ne_operations = ('eq', 'ge', 'gt', 'le', 'lt')
    # Versions should only be comparable with 4 elements.
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('1.2.3', 'ne', '1.2.3', 'win',
                                             'intel'))
    for op in non_ne_operations:
      self.assertFalse(
          gpu_helper.EvaluateVersionComparison('1.2.3', op, '1.2.3', 'win',
                                               'intel'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('1.2.3.4.5', 'ne', '1.2.3.4.5',
                                             'win', 'intel'))
    for op in non_ne_operations:
      self.assertFalse(
          gpu_helper.EvaluateVersionComparison('1.2.3.4.5', op, '1.2.3.4.5',
                                               'win', 'intel'))

  def testWindowsIntelOnlyLastTwoPartsUsed(self) -> None:
    """Tests that only the last two version parts are used on Windows Intel."""
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('1.2.3.4', 'eq', '2.3.3.4', 'win',
                                             'intel'))
    self.assertFalse(
        gpu_helper.EvaluateVersionComparison('1.2.3.4', 'eq', '2.3.5.4', 'win',
                                             'intel'))

  def testInvalidOperation(self) -> None:
    """Tests that an error is raised when using an invalid operation."""
    with self.assertRaisesRegex(Exception, 'Invalid operation: foo'):
      gpu_helper.EvaluateVersionComparison('1.2.3.4', 'foo', '2.3.4.5')

  def testEqual(self) -> None:
    """Tests that equality operations work as expected."""
    eq_operations = ('eq', 'ge', 'le')
    for op in eq_operations:
      # Purely numerical.
      self.assertTrue(
          gpu_helper.EvaluateVersionComparison('1.2.3.4', op, '1.2.3.4'))

      # Numerical + suffix.
      self.assertTrue(
          gpu_helper.EvaluateVersionComparison('1a.2b.3c.4a', op,
                                               '1a.2b.3c.4a'))

      # Mismatched length implies 0.
      self.assertTrue(gpu_helper.EvaluateVersionComparison(
          '1.2.0.0', op, '1.2'))
      self.assertTrue(gpu_helper.EvaluateVersionComparison(
          '1.2', op, '1.2.0.0'))

      # Failure to parse gets skipped unless in the reference version.
      self.assertTrue(
          gpu_helper.EvaluateVersionComparison('1.2..4', op, '1.2.3.4'))
      with self.assertRaises(AssertionError):
        gpu_helper.EvaluateVersionComparison('1.2.3.4', op, '1.2..4')

  def testNotEqual(self) -> None:
    """Tests that the not equal operation works as expected."""
    true_cases = (
        # Purely numerical.
        ('1.2.3.4', '1.2.3'),
        ('1.2.3', '1.2.3.4'),
        # Same suffix, different numerical.
        ('2a.2b.3c.4d', '1a.2b.3c.4d'),
        ('1a.3b.3c.4d', '1a.2b.3c.4d'),
        ('1a.2b.4c.4d', '1a.2b.3c.4d'),
        ('1a.2b.3c.5d', '1a.2b.3c.4d'),
        # Same numerical, different suffix.
        ('1b.2b.3c.4d', '1a.2b.3c.4d'),
        ('1a.2c.3c.4d', '1a.2b.3c.4d'),
        ('1a.2b.3d.4d', '1a.2b.3c.4d'),
        ('1a.2b.3c.4e', '1a.2b.3c.4d'),
    )
    false_cases = (
        # Numerical.
        ('1.2.3.4', '1.2.3.4'),
        # Numerical + suffix.
        ('1a.2b.3c.4d', '1a.2b.3c.4d'),
    )
    for left, right in true_cases:
      self.assertTrue(gpu_helper.EvaluateVersionComparison(left, 'ne', right))
      self.assertTrue(gpu_helper.EvaluateVersionComparison(right, 'ne', left))
    for left, right in false_cases:
      self.assertFalse(gpu_helper.EvaluateVersionComparison(left, 'ne', right))
      self.assertFalse(gpu_helper.EvaluateVersionComparison(right, 'ne', left))

  def testGreater(self) -> None:
    """Tests that greater operations work as expected."""
    gt_operations = ('gt', 'ge')
    for op in gt_operations:
      for left, right in GetGreaterTestCases():
        self.assertTrue(gpu_helper.EvaluateVersionComparison(left, op, right))
        self.assertFalse(gpu_helper.EvaluateVersionComparison(right, op, left))

  def testLess(self) -> None:
    """Tests that less operations work as expected."""
    lt_operations = ('lt', 'le')
    for op in lt_operations:
      # Less than test cases are simply the inverse of greater than test cases,
      # so just reverse the order.
      for right, left in GetGreaterTestCases():
        self.assertTrue(gpu_helper.EvaluateVersionComparison(left, op, right))
        self.assertFalse(gpu_helper.EvaluateVersionComparison(right, op, left))


def GetGreaterTestCases() -> Tuple[Tuple[str, str], ...]:
  return (
      # Purely numerical.
      ('2.2.3.4', '1.2.3.4'),
      ('1.3.3.4', '1.2.3.4'),
      ('1.2.4.4', '1.2.3.4'),
      ('1.2.3.5', '1.2.3.4'),
      # Same suffix, different numerical.
      ('2a.2b.3c.4d', '1a.2b.3c.4d'),
      ('1a.3b.3c.4d', '1a.2b.3c.4d'),
      ('1a.2b.4c.4d', '1a.2b.3c.4d'),
      ('1a.2b.3c.5d', '1a.2b.3c.4d'),
      # Same numerical, different suffix.
      ('1b.2b.3c.4d', '1a.2b.3c.4d'),
      ('1a.2c.3c.4d', '1a.2b.3c.4d'),
      ('1a.2b.3d.4d', '1a.2b.3c.4d'),
      ('1a.2b.3c.4e', '1a.2b.3c.4d'),
  )


if __name__ == '__main__':
  unittest.main(verbosity=2)
