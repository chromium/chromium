# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for overlay_support.py"""

import json
import os
from typing import Union
import unittest
from unittest import mock

from gpu_tests import constants
from gpu_tests import overlay_support

from pyfakefs import fake_filesystem_unittest  # pylint:disable=import-error

rotation = overlay_support.VideoRotation

# pylint: disable=too-many-public-methods


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

  def testEqualityDifferentClass(self):
    """Tests __eq__ behavior between GpuOverlayConfig and other types."""
    config = overlay_support.GpuOverlayConfig()

    self.assertNotEqual(config, 'NotAnOverlayConfig')

  def testEqualityBase(self):
    """Tests __eq__ behavior between GpuOverlayConfigs without arguments."""
    config = overlay_support.GpuOverlayConfig()
    other = overlay_support.GpuOverlayConfig()
    self.assertEqual(config, other)

    other.WithDirectComposition()
    self.assertNotEqual(config, other)
    config.WithDirectComposition()
    self.assertEqual(config, other)

    other.WithHardwareNV12Support()
    self.assertNotEqual(config, other)
    config.WithHardwareNV12Support()
    self.assertEqual(config, other)

    other.WithHardwareYUY2Support()
    self.assertNotEqual(config, other)
    config.WithHardwareYUY2Support()
    self.assertEqual(config, other)

    other.WithHardwareBGRA8Support()
    self.assertNotEqual(config, other)
    config.WithHardwareBGRA8Support()
    self.assertEqual(config, other)

  def testEqualityDriverConditionals(self):
    """Tests __eq__ behavior between GpuOverlayConfigs w/ DriverConditionals."""
    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.WithHardwareNV12Support(
        driver_conditionals=[overlay_support.DriverConditional('ge', '1.0.0')])
    self.assertNotEqual(config, other)
    config.WithHardwareNV12Support(
        driver_conditionals=[overlay_support.DriverConditional('ge', '1.0.0')])
    self.assertEqual(config, other)

    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.WithHardwareNV12Support(
        driver_conditionals=[overlay_support.DriverConditional('ge', '1.0.0')])
    config.WithHardwareNV12Support(
        driver_conditionals=[overlay_support.DriverConditional('le', '1.0.0')])
    self.assertNotEqual(config, other)

    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.WithHardwareNV12Support(
        driver_conditionals=[overlay_support.DriverConditional('ge', '1.0.0')])
    config.WithHardwareNV12Support(
        driver_conditionals=[overlay_support.DriverConditional('ge', '2.0.0')])
    self.assertNotEqual(config, other)

    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.WithForceComposedBGRA8(
        driver_conditionals=[overlay_support.DriverConditional('ge', '1.0.0')])
    self.assertNotEqual(config, other)
    config.WithForceComposedBGRA8(
        driver_conditionals=[overlay_support.DriverConditional('ge', '1.0.0')])
    self.assertEqual(config, other)

    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.WithForceComposedBGRA8(
        driver_conditionals=[overlay_support.DriverConditional('ge', '1.0.0')])
    config.WithForceComposedBGRA8(
        driver_conditionals=[overlay_support.DriverConditional('le', '1.0.0')])
    self.assertNotEqual(config, other)

  def testEqualitySupportedRotations(self):
    """Tests __eq__ behavior between GpuOverlayConfigs w/ supported rotations"""
    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.WithHardwareNV12Support(supported_rotations=[rotation.ROT180])
    self.assertNotEqual(config, other)
    config.WithHardwareNV12Support(supported_rotations=[rotation.ROT180])
    self.assertEqual(config, other)

    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.WithHardwareNV12Support(supported_rotations=[rotation.ROT180])
    config.WithHardwareNV12Support(supported_rotations=[rotation.ROT90])
    self.assertNotEqual(config, other)

  def testEqualityZeroCopy(self):
    """Tests __eq__ behavior between GpuOverlayConfigs w/ zero copy configs."""
    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.WithZeroCopyConfig(overlay_support.ZeroCopyConfig())
    config.WithZeroCopyConfig(overlay_support.ZeroCopyConfig())
    self.assertEqual(config, other)

    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.WithZeroCopyConfig(
        overlay_support.ZeroCopyConfig(supports_scaled_video=True))
    self.assertNotEqual(config, other)
    config.WithZeroCopyConfig(
        overlay_support.ZeroCopyConfig(supports_scaled_video=True))
    self.assertEqual(config, other)

    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.WithZeroCopyConfig(
        overlay_support.ZeroCopyConfig(
            supported_codecs=[overlay_support.ZeroCopyCodec.H264]))
    self.assertNotEqual(config, other)
    config.WithZeroCopyConfig(
        overlay_support.ZeroCopyConfig(
            supported_codecs=[overlay_support.ZeroCopyCodec.H264]))
    self.assertEqual(config, other)

  def testEqualityDriverVersion(self):
    """Tests __eq__ behavior between GpuOverlayConfigs w/ driver version."""
    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.OnDriverVersion('1')
    self.assertNotEqual(config, other)
    config.OnDriverVersion('1')
    self.assertEqual(config, other)

    config = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other = overlay_support.GpuOverlayConfig().WithDirectComposition()
    other.OnDriverVersion('1')
    config.OnDriverVersion('2')
    self.assertNotEqual(config, other)


def _createMockGpu(vendor: Union[constants.GpuVendor, int],
                   device: int) -> mock.Mock:
  gpu = mock.Mock()
  gpu.vendor_id = vendor
  gpu.device_id = device
  gpu.driver_version = '1.0.0'
  return gpu


class GetOverlayConfigForGpuUnittest(unittest.TestCase):

  def testKnownGpu(self):  # pylint: disable=no-self-use
    """Tests behavior when a known GPU is provided."""
    gpu = _createMockGpu(constants.GpuVendor.INTEL, 0x3e92)

    overlay_support.GetOverlayConfigForGpu(gpu)

  def testUnknownVendor(self):
    """Tests behavior when an unknown GPU vendor is provided."""
    gpu = _createMockGpu(0x1234, 0x3e92)

    with self.assertRaisesRegex(
        RuntimeError,
        'GPU with vendor ID 0x1234 and device ID 0x3e92 does not have an '
        'overlay config specified'):
      overlay_support.GetOverlayConfigForGpu(gpu)

  def testUnknownDevice(self):
    """Tests behavior when an unknown GPU devices is provided."""
    gpu = _createMockGpu(constants.GpuVendor.INTEL, 0x1234)

    with self.assertRaisesRegex(
        RuntimeError,
        'GPU with vendor ID 0x8086 and device ID 0x1234 does not have an '
        'overlay config specified'):
      overlay_support.GetOverlayConfigForGpu(gpu)


# mock.patch.dict is used for all tests here to ensure that any OVERLAY_CONFIGS
# changes do not persist across tests.
class ParseOverlayJsonFileUnittest(fake_filesystem_unittest.TestCase):

  def setUp(self):
    self.setUpPyfakefs()
    os.makedirs('tmp')
    self.filepath = os.path.join('tmp', 'input.json')

  def setJson(self, json_content: dict) -> None:
    with open(self.filepath, 'w', encoding='utf-8') as outfile:
      json.dump(json_content, outfile)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS,
                   overlay_support.OVERLAY_CONFIGS,
                   clear=True)
  def testDuplicateConfig(self):
    """Tests behavior when a duplicate config is provided."""
    json_content = {
        '0x8086': {
            '0x3e92': [],
        }
    }
    self.setJson(json_content)
    gpu = _createMockGpu(vendor=constants.GpuVendor.INTEL, device=0x3e92)
    original_config = overlay_support.GetOverlayConfigForGpu(gpu)
    overlay_support.ParseOverlayJsonFile(self.filepath)
    updated_config = overlay_support.GetOverlayConfigForGpu(gpu)
    self.assertEqual(updated_config, original_config)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS,
                   overlay_support.OVERLAY_CONFIGS,
                   clear=True)
  def testUnknownVendor(self):
    """Tests behavior when an unknown vendor is provided."""
    json_content = {
        '0x1234': {
            '0x3e92': [],
        },
    }
    self.setJson(json_content)
    with self.assertRaises(ValueError):
      overlay_support.ParseOverlayJsonFile(self.filepath)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS,
                   overlay_support.OVERLAY_CONFIGS,
                   clear=True)
  def testNonHexVendor(self):
    """Tests behavior when a non-hexadecimal vendor ID is provided."""
    json_content = {
        '8086': {
            '0x3e92': [],
        },
    }
    self.setJson(json_content)
    with self.assertRaises(AssertionError):
      overlay_support.ParseOverlayJsonFile(self.filepath)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS,
                   overlay_support.OVERLAY_CONFIGS,
                   clear=True)
  def testNonHexDevice(self):
    """Tests behavior when a non-hexadecimal device ID is provided."""
    json_content = {
        '0x8086': {
            '3e92': [],
        },
    }
    self.setJson(json_content)
    with self.assertRaises(AssertionError):
      overlay_support.ParseOverlayJsonFile(self.filepath)

  def _parseTestHelper(self, json_content, expected):
    self.setJson(json_content)
    overlay_support.ParseOverlayJsonFile(self.filepath)
    actual = overlay_support.GetOverlayConfigForGpu(
        _createMockGpu(constants.GpuVendor.INTEL, 0x1234))
    self.assertEqual(actual, expected)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS, {}, clear=True)
  def testBaseConfig(self):
    """Tests behavior when no additional functions are specified."""
    json_content = {
        '0x8086': {
            '0x1234': [],
        },
    }
    expected = overlay_support.GpuOverlayConfig().OnDriverVersion('1.0.0')
    self._parseTestHelper(json_content, expected)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS, {}, clear=True)
  def testWithDirectComposition(self):
    """Tests behavior when direct composition is specified."""
    json_content = {
        '0x8086': {
            '0x1234': [
                {
                    'function': 'WithDirectComposition',
                },
            ],
        },
    }
    expected = overlay_support.GpuOverlayConfig()\
               .WithDirectComposition()\
               .OnDriverVersion('1.0.0')
    self._parseTestHelper(json_content, expected)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS, {}, clear=True)
  def testWithHardwareNV12Support(self):
    """Tests behavior when NV12 support is specified."""
    json_content = {
        '0x8086': {
            '0x1234': [
                {
                    'function': 'WithDirectComposition',
                },
                {
                    'function': 'WithHardwareNV12Support',
                },
            ],
        },
    }
    expected = overlay_support.GpuOverlayConfig()\
               .WithDirectComposition()\
               .WithHardwareNV12Support()\
               .OnDriverVersion('1.0.0')
    self._parseTestHelper(json_content, expected)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS, {}, clear=True)
  def testWithHardwareYUY2Support(self):
    """Tests behavior when YUY2 support is specified."""
    json_content = {
        '0x8086': {
            '0x1234': [
                {
                    'function': 'WithDirectComposition',
                },
                {
                    'function': 'WithHardwareYUY2Support',
                },
            ],
        },
    }
    expected = overlay_support.GpuOverlayConfig()\
               .WithDirectComposition()\
               .WithHardwareYUY2Support()\
               .OnDriverVersion('1.0.0')
    self._parseTestHelper(json_content, expected)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS, {}, clear=True)
  def testWithHardwareBGRA8Support(self):
    """Tests behavior when BGRA8 support is specified."""
    json_content = {
        '0x8086': {
            '0x1234': [
                {
                    'function': 'WithDirectComposition',
                },
                {
                    'function': 'WithHardwareBGRA8Support',
                },
            ],
        },
    }
    expected = overlay_support.GpuOverlayConfig()\
               .WithDirectComposition()\
               .WithHardwareBGRA8Support()\
               .OnDriverVersion('1.0.0')
    self._parseTestHelper(json_content, expected)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS, {}, clear=True)
  def testWithHardwareSupportWithDriverConditional(self):
    """Tests behavior when support is specified with driver conditionals"""
    json_content = {
        '0x8086': {
            '0x1234': [
                {
                    'function': 'WithDirectComposition',
                },
                {
                    'function': 'WithHardwareNV12Support',
                    'args': {
                        'driver_conditionals': [
                            ['ge', '2.0.0'],
                        ],
                    },
                },
            ],
        },
    }
    expected = overlay_support.GpuOverlayConfig()\
               .WithDirectComposition()\
               .WithHardwareNV12Support(
                    driver_conditionals=[
                        overlay_support.DriverConditional('ge', '2.0.0')])\
               .OnDriverVersion('1.0.0')
    self._parseTestHelper(json_content, expected)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS, {}, clear=True)
  def testWithHardwareSupportWithSupportedRotation(self):
    """Tests behavior when support is specified with supported rotations."""
    json_content = {
        '0x8086': {
            '0x1234': [
                {
                    'function': 'WithDirectComposition',
                },
                {
                    'function': 'WithHardwareNV12Support',
                    'args': {
                        'supported_rotations': [
                            180,
                        ],
                    },
                },
            ],
        },
    }
    expected = overlay_support.GpuOverlayConfig()\
               .WithDirectComposition()\
               .WithHardwareNV12Support(supported_rotations=[rotation.ROT180])\
               .OnDriverVersion('1.0.0')
    self._parseTestHelper(json_content, expected)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS, {}, clear=True)
  def testWithForceComposedBGRA8(self):
    """Tests behavior when composed BGRA8 is forced."""
    json_content = {
        '0x8086': {
            '0x1234': [
                {
                    'function': 'WithDirectComposition',
                },
                {
                    'function': 'WithForceComposedBGRA8',
                    'args': {
                        'driver_conditionals': [
                            ['ge', '2.0.0'],
                        ],
                    },
                },
            ],
        },
    }
    expected = overlay_support.GpuOverlayConfig()\
               .WithDirectComposition()\
               .WithForceComposedBGRA8(driver_conditionals=[
                    overlay_support.DriverConditional('ge', '2.0.0')])\
               .OnDriverVersion('1.0.0')
    self._parseTestHelper(json_content, expected)

  @mock.patch.dict(overlay_support.OVERLAY_CONFIGS, {}, clear=True)
  def testWithZeroCopyConfig(self):
    """Tests behavior when a zero copy config is specified."""
    json_content = {
        '0x8086': {
            '0x1234': [
                {
                    'function': 'WithDirectComposition',
                },
                {
                    'function': 'WithZeroCopyConfig',
                    'args': {
                        'supports_scaled_video': True,
                        'supported_codecs': [
                            'H264',
                        ],
                    },
                },
            ],
        },
    }
    expected = overlay_support.GpuOverlayConfig()\
               .WithDirectComposition()\
               .WithZeroCopyConfig(overlay_support.ZeroCopyConfig(
                    supports_scaled_video=True,
                    supported_codecs=[overlay_support.ZeroCopyCodec.H264]))\
               .OnDriverVersion('1.0.0')
    self._parseTestHelper(json_content, expected)


if __name__ == '__main__':
  unittest.main(verbosity=2)
