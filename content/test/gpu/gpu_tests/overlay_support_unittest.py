# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for overlay_support.py"""

import unittest
from unittest import mock

from gpu_tests import gpu_helper
from gpu_tests import overlay_support

rotation = overlay_support.VideoRotation


class PresentationModeEventToStrUnittest(unittest.TestCase):

  def testAllEventsHaveStringEnum(self):
    """Tests that all defined enums have corresponding string values."""
    for event in overlay_support.PresentationModeEvent:
      event_str = overlay_support.PresentationModeEventToStr(event)
      self.assertNotIn('(unknown)', event_str)

  def testIntConversion(self):
    """Tests that ints are automatically converted to enum values."""
    event_str = overlay_support.PresentationModeEventToStr(0)
    self.assertNotIn('(unknown)', event_str)

  def testUnknownValue(self):
    """Tests behavior when an unknown value is provided."""
    event_str = overlay_support.PresentationModeEventToStr(999)
    self.assertEqual(event_str, '999 (unknown)')


class GpuOverlayConfigUnittest(unittest.TestCase):

  def testNoDirectCompositionSupport(self):
    """Tests behavior when no direct composition support is specified."""
    config = overlay_support.GpuOverlayConfig().OnDriverVersion('1')

    self.assertFalse(config.direct_composition)
    self.assertFalse(config.supports_overlays)

    self.assertEqual(config.nv12_overlay_support,
                     overlay_support.OverlaySupport.NONE)
    self.assertEqual(config.yuy2_overlay_support,
                     overlay_support.OverlaySupport.NONE)
    self.assertEqual(config.bgra8_overlay_support,
                     overlay_support.OverlaySupport.NONE)

    self.assertFalse(config.supports_hw_nv12_overlays)
    self.assertFalse(config.supports_hw_yuy2_overlays)
    self.assertFalse(config.supports_hw_bgra8_overlays)

    for pixel_format in overlay_support.PixelFormat.ALL_PIXEL_FORMATS:
      self.assertEqual(config.GetExpectedPixelFormat(None),
                       overlay_support.PixelFormat.BGRA8)
      self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                       overlay_support.PixelFormat.BGRA8)

    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.BGRA8,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.COMPOSED)

  def testSoftwareDirectCompositionSupport(self):
    """Tests behavior when software direct composition support is specified."""
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .OnDriverVersion('1')

    self.assertTrue(config.direct_composition)
    self.assertTrue(config.supports_overlays)

    self.assertEqual(config.nv12_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.yuy2_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.bgra8_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)

    self.assertFalse(config.supports_hw_nv12_overlays)
    self.assertFalse(config.supports_hw_yuy2_overlays)
    self.assertFalse(config.supports_hw_bgra8_overlays)

    for pixel_format in overlay_support.PixelFormat.ALL_PIXEL_FORMATS:
      self.assertEqual(config.GetExpectedPixelFormat(None),
                       overlay_support.PixelFormat.BGRA8)
      self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                       overlay_support.PixelFormat.BGRA8)

    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.BGRA8,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.OVERLAY)

  def testNV12HardwareDirectCompositionSupport(self):
    """Tests behavior when NV12 direct composition support is specified."""
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithHardwareNV12Support()\
             .OnDriverVersion('1')

    self.assertTrue(config.direct_composition)
    self.assertTrue(config.supports_overlays)

    self.assertEqual(config.nv12_overlay_support,
                     overlay_support.OverlaySupport.SCALING)
    self.assertEqual(config.yuy2_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.bgra8_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)

    self.assertTrue(config.supports_hw_nv12_overlays)
    self.assertFalse(config.supports_hw_yuy2_overlays)
    self.assertFalse(config.supports_hw_bgra8_overlays)

    for pixel_format in overlay_support.PixelFormat.ALL_PIXEL_FORMATS:
      self.assertEqual(config.GetExpectedPixelFormat(None),
                       overlay_support.PixelFormat.NV12)
      if pixel_format == overlay_support.PixelFormat.NV12:
        self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                         overlay_support.PixelFormat.NV12)
      else:
        self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                         overlay_support.PixelFormat.BGRA8)

    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.NV12,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.OVERLAY)
    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.BGRA8,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.OVERLAY)

  def testYUY2HardwareDirectCompositionSupport(self):
    """Tests behavior when YUY2 direct composition support is specified."""
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithHardwareYUY2Support()\
             .OnDriverVersion('1')

    self.assertTrue(config.direct_composition)
    self.assertTrue(config.supports_overlays)

    self.assertEqual(config.nv12_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.yuy2_overlay_support,
                     overlay_support.OverlaySupport.SCALING)
    self.assertEqual(config.bgra8_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)

    self.assertFalse(config.supports_hw_nv12_overlays)
    self.assertTrue(config.supports_hw_yuy2_overlays)
    self.assertFalse(config.supports_hw_bgra8_overlays)

    for pixel_format in overlay_support.PixelFormat.ALL_PIXEL_FORMATS:
      self.assertEqual(config.GetExpectedPixelFormat(None),
                       overlay_support.PixelFormat.YUY2)
      if pixel_format == overlay_support.PixelFormat.YUY2:
        self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                         overlay_support.PixelFormat.YUY2)
      else:
        self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                         overlay_support.PixelFormat.BGRA8)

    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.YUY2,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.OVERLAY)
    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.BGRA8,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.OVERLAY)

  def testBGRA8HardwareDirectCompositionSupport(self):
    """Tests behavior when BGRA8 direct composition support is specified."""
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithHardwareBGRA8Support()\
             .OnDriverVersion('1')

    self.assertTrue(config.direct_composition)
    self.assertTrue(config.supports_overlays)

    self.assertEqual(config.nv12_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.yuy2_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.bgra8_overlay_support,
                     overlay_support.OverlaySupport.SCALING)

    self.assertFalse(config.supports_hw_nv12_overlays)
    self.assertFalse(config.supports_hw_yuy2_overlays)
    self.assertTrue(config.supports_hw_bgra8_overlays)

    for pixel_format in overlay_support.PixelFormat.ALL_PIXEL_FORMATS:
      self.assertEqual(config.GetExpectedPixelFormat(None),
                       overlay_support.PixelFormat.BGRA8)
      self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                       overlay_support.PixelFormat.BGRA8)

    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.BGRA8,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.OVERLAY)

  def testForceComposedBGRA8(self):
    """Tests behavior when software BGRA8 overlays are forcibly composed."""
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithForceComposedBGRA8(driver_conditionals=[
                  overlay_support.DriverConditional('lt', '21.0.0')])\
             .OnDriverVersion('20.0.3')

    self.assertTrue(config.direct_composition)
    self.assertTrue(config.supports_overlays)

    self.assertEqual(config.nv12_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.yuy2_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.bgra8_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)

    self.assertFalse(config.supports_hw_nv12_overlays)
    self.assertFalse(config.supports_hw_yuy2_overlays)
    self.assertFalse(config.supports_hw_bgra8_overlays)

    for pixel_format in overlay_support.PixelFormat.ALL_PIXEL_FORMATS:
      self.assertEqual(config.GetExpectedPixelFormat(None),
                       overlay_support.PixelFormat.BGRA8)
      self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                       overlay_support.PixelFormat.BGRA8)

    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.BGRA8,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.COMPOSED)

  def testInvalidDriver(self):
    """Tests behavior when an invalid driver is used."""
    # This should end up being equivalent to only having software support.
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithHardwareNV12Support(driver_conditionals=[
                  overlay_support.DriverConditional('ge', '21.0.0')])\
             .WithHardwareYUY2Support(driver_conditionals=[
                  overlay_support.DriverConditional('ge', '21.0.0')])\
             .OnDriverVersion('20.0.3')

    self.assertTrue(config.direct_composition)
    self.assertTrue(config.supports_overlays)

    self.assertEqual(config.nv12_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.yuy2_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.bgra8_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)

    self.assertFalse(config.supports_hw_nv12_overlays)
    self.assertFalse(config.supports_hw_yuy2_overlays)
    self.assertFalse(config.supports_hw_bgra8_overlays)

    for pixel_format in overlay_support.PixelFormat.ALL_PIXEL_FORMATS:
      self.assertEqual(config.GetExpectedPixelFormat(None),
                       overlay_support.PixelFormat.BGRA8)
      self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                       overlay_support.PixelFormat.BGRA8)

  def testZeroCopyVideoRotation(self):
    """Tests the effect of video rotation on zero copy."""
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithHardwareNV12Support()\
             .WithZeroCopyConfig(overlay_support.ZeroCopyConfig(
                  supported_codecs=[overlay_support.ZeroCopyCodec.VP9]))\
             .OnDriverVersion('1')

    for r in overlay_support.VideoRotation:
      zero_copy = config.GetExpectedZeroCopyUsage(
          expected_pixel_format=overlay_support.PixelFormat.NV12,
          video_rotation=r,
          fullsize=True,
          codec=overlay_support.ZeroCopyCodec.VP9)
      if r == rotation.UNROTATED:
        self.assertTrue(zero_copy)
      else:
        self.assertFalse(zero_copy)

  def testZeroCopyPixelFormat(self):
    """Tests the effect of pixel format on zero copy."""
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithHardwareNV12Support()\
             .WithZeroCopyConfig(overlay_support.ZeroCopyConfig(
                  supported_codecs=[overlay_support.ZeroCopyCodec.VP9]))\
             .OnDriverVersion('1')

    for pixel_format in overlay_support.PixelFormat.ALL_PIXEL_FORMATS:
      zero_copy = config.GetExpectedZeroCopyUsage(
          expected_pixel_format=pixel_format,
          video_rotation=rotation.UNROTATED,
          fullsize=True,
          codec=overlay_support.ZeroCopyCodec.VP9)
      if pixel_format == overlay_support.PixelFormat.NV12:
        self.assertTrue(zero_copy)
      else:
        self.assertFalse(zero_copy)

  def testZeroCopyScaledVideo(self):
    """Tests the effect of scaled video support on zero copy."""
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithHardwareNV12Support()\
             .WithZeroCopyConfig(overlay_support.ZeroCopyConfig(
                  supported_codecs=[overlay_support.ZeroCopyCodec.VP9]))\
             .OnDriverVersion('1')

    self.assertTrue(
        config.GetExpectedZeroCopyUsage(
            expected_pixel_format=overlay_support.PixelFormat.NV12,
            video_rotation=rotation.UNROTATED,
            fullsize=True,
            codec=overlay_support.ZeroCopyCodec.VP9))

    # Same as above but with fullsize=False.
    self.assertFalse(
        config.GetExpectedZeroCopyUsage(
            expected_pixel_format=overlay_support.PixelFormat.NV12,
            video_rotation=rotation.UNROTATED,
            fullsize=False,
            codec=overlay_support.ZeroCopyCodec.VP9))

    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithHardwareNV12Support()\
             .WithZeroCopyConfig(overlay_support.ZeroCopyConfig(
                  supports_scaled_video=True,
                  supported_codecs=[overlay_support.ZeroCopyCodec.VP9]))\
             .OnDriverVersion('1')

    # Same as first assert.
    self.assertTrue(
        config.GetExpectedZeroCopyUsage(
            expected_pixel_format=overlay_support.PixelFormat.NV12,
            video_rotation=rotation.UNROTATED,
            fullsize=True,
            codec=overlay_support.ZeroCopyCodec.VP9))

    # Same as second assert, but now we expect True.
    self.assertTrue(
        config.GetExpectedZeroCopyUsage(
            expected_pixel_format=overlay_support.PixelFormat.NV12,
            video_rotation=rotation.UNROTATED,
            fullsize=False,
            codec=overlay_support.ZeroCopyCodec.VP9))

  def testZeroCopyCodecSupport(self):
    """Tests the effect of codec support on zero copy."""
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithHardwareNV12Support()\
             .WithZeroCopyConfig(overlay_support.ZeroCopyConfig(
                  supported_codecs=[overlay_support.ZeroCopyCodec.H264]))\
             .OnDriverVersion('1')

    self.assertTrue(
        config.GetExpectedZeroCopyUsage(
            expected_pixel_format=overlay_support.PixelFormat.NV12,
            video_rotation=rotation.UNROTATED,
            fullsize=True,
            codec=overlay_support.ZeroCopyCodec.H264))

    self.assertFalse(
        config.GetExpectedZeroCopyUsage(
            expected_pixel_format=overlay_support.PixelFormat.NV12,
            video_rotation=rotation.UNROTATED,
            fullsize=True,
            codec=overlay_support.ZeroCopyCodec.VP9))

  def testNV12RotatedVideo(self):
    """Tests NV12 behavior with rotated video."""
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithHardwareNV12Support(supported_rotations=[rotation.ROT180])\
             .OnDriverVersion('1')

    self.assertTrue(config.direct_composition)
    self.assertTrue(config.supports_overlays)

    self.assertEqual(config.nv12_overlay_support,
                     overlay_support.OverlaySupport.SCALING)
    self.assertEqual(config.yuy2_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.bgra8_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)

    self.assertTrue(config.supports_hw_nv12_overlays)
    self.assertFalse(config.supports_hw_yuy2_overlays)
    self.assertFalse(config.supports_hw_bgra8_overlays)

    for pixel_format in overlay_support.PixelFormat.ALL_PIXEL_FORMATS:
      self.assertEqual(config.GetExpectedPixelFormat(None),
                       overlay_support.PixelFormat.NV12)
      if pixel_format == overlay_support.PixelFormat.NV12:
        self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                         overlay_support.PixelFormat.NV12)
      else:
        self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                         overlay_support.PixelFormat.BGRA8)

    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.NV12,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.OVERLAY)
    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.NV12,
                                           rotation.ROT90),
        overlay_support.PresentationMode.COMPOSED)
    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.NV12,
                                           rotation.ROT180),
        overlay_support.PresentationMode.OVERLAY)
    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.NV12,
                                           rotation.ROT270),
        overlay_support.PresentationMode.COMPOSED)
    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.BGRA8,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.OVERLAY)

  def testYUY2RotatedVideo(self):
    """Tests YUY2 behavior with rotated video."""
    config = overlay_support.GpuOverlayConfig()\
             .WithDirectComposition()\
             .WithHardwareYUY2Support(supported_rotations=[rotation.ROT180])\
             .OnDriverVersion('1')

    self.assertTrue(config.direct_composition)
    self.assertTrue(config.supports_overlays)

    self.assertEqual(config.nv12_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)
    self.assertEqual(config.yuy2_overlay_support,
                     overlay_support.OverlaySupport.SCALING)
    self.assertEqual(config.bgra8_overlay_support,
                     overlay_support.OverlaySupport.SOFTWARE)

    self.assertFalse(config.supports_hw_nv12_overlays)
    self.assertTrue(config.supports_hw_yuy2_overlays)
    self.assertFalse(config.supports_hw_bgra8_overlays)

    for pixel_format in overlay_support.PixelFormat.ALL_PIXEL_FORMATS:
      self.assertEqual(config.GetExpectedPixelFormat(None),
                       overlay_support.PixelFormat.YUY2)
      if pixel_format == overlay_support.PixelFormat.YUY2:
        self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                         overlay_support.PixelFormat.YUY2)
      else:
        self.assertEqual(config.GetExpectedPixelFormat(pixel_format),
                         overlay_support.PixelFormat.BGRA8)

    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.YUY2,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.OVERLAY)
    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.YUY2,
                                           rotation.ROT90),
        overlay_support.PresentationMode.COMPOSED)
    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.YUY2,
                                           rotation.ROT180),
        overlay_support.PresentationMode.OVERLAY)
    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.YUY2,
                                           rotation.ROT270),
        overlay_support.PresentationMode.COMPOSED)
    self.assertEqual(
        config.GetExpectedPresentationMode(overlay_support.PixelFormat.BGRA8,
                                           rotation.UNROTATED),
        overlay_support.PresentationMode.OVERLAY)

  def testDriverVersionRequired(self):
    """Tests that usage fails if a driver version is not added."""
    config = overlay_support.GpuOverlayConfig()

    with self.assertRaises(AssertionError):
      _ = config.nv12_overlay_support

  def testDriverCanBeSetWithSameValue(self):  # pylint: disable=no-self-use
    """Tests that the driver version can be set again with the same value."""
    config = overlay_support.GpuOverlayConfig()\
             .OnDriverVersion('1')\
             .OnDriverVersion('1')
    del config

  def testDriverCannotBeSetWithDifferentValue(self):
    """Tests that the driver version cannot be set with a different value."""
    config = overlay_support.GpuOverlayConfig().OnDriverVersion('1')
    with self.assertRaises(AssertionError):
      config.OnDriverVersion('2')

  def testUnknownPixelFormat(self):
    """Tests behavior when an unknown pixel format is specified."""
    config = overlay_support.GpuOverlayConfig().OnDriverVersion('1')
    with self.assertRaises(AssertionError):
      config.GetExpectedPixelFormat('NotReal')

    with self.assertRaises(KeyError):
      config.GetExpectedPresentationMode('NotReal', rotation.UNROTATED)


class GetOverlayConfigForGpuUnittest(unittest.TestCase):

  def testKnownGpu(self):  # pylint: disable=no-self-use
    """Tests behavior when a known GPU is provided."""
    gpu = mock.Mock()
    gpu.vendor_id = gpu_helper.GpuVendors.INTEL
    gpu.device_id = 0x3e92
    gpu.driver_version = '1'

    overlay_support.GetOverlayConfigForGpu(gpu)

  def testUnknownVendor(self):
    """Tests behavior when an unknown GPU vendor is provided."""
    gpu = mock.Mock()
    gpu.vendor_id = 0x1234
    gpu.device_id = 0x3e92
    gpu.driver_version = '1'

    with self.assertRaisesRegex(
        RuntimeError,
        'GPU with vendor ID 0x1234 and device ID 0x3e92 does not have an '
        'overlay config specified'):
      overlay_support.GetOverlayConfigForGpu(gpu)

  def testUnknownDevice(self):
    """Tests behavior when an unknown GPU devices is provided."""
    gpu = mock.Mock()
    gpu.vendor_id = gpu_helper.GpuVendors.INTEL
    gpu.device_id = 0x1234
    gpu.driver_version = '1'

    with self.assertRaisesRegex(
        RuntimeError,
        'GPU with vendor ID 0x8086 and device ID 0x1234 does not have an '
        'overlay config specified'):
      overlay_support.GetOverlayConfigForGpu(gpu)


if __name__ == '__main__':
  unittest.main(verbosity=2)
