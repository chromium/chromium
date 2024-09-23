# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for crop_actions.py"""

import unittest

from gpu_tests import crop_actions as ca

from telemetry.util import image_util

WHITE = (255, 255, 255)
RED = (255, 0, 0)
GREEN = (0, 255, 0)


class FixedRectCropActionUnittest(unittest.TestCase):

  def testInit(self):
    """Tests constructor checks."""
    with self.assertRaises(AssertionError):
      ca.FixedRectCropAction(-1, 0, 1, 1)
    with self.assertRaises(AssertionError):
      ca.FixedRectCropAction(0, -1, 1, 1)
    with self.assertRaises(AssertionError):
      ca.FixedRectCropAction(0, 0, 0, 1)
    with self.assertRaises(AssertionError):
      ca.FixedRectCropAction(0, 0, 1, 0)

  def testCropTopLeft(self):
    """Tests that cropping works properly when done at the top left bounds."""
    image_width = 20
    # 20 x 2 green image with a single red pixel in the top left.
    # yapf: disable
    pixels = [*RED, *(GREEN * (image_width - 1)),
              *(GREEN * image_width)]
    # yapf: enable
    image = image_util.FromRGBPixels(20, 2, pixels, bpp=3)
    action = ca.FixedRectCropAction(0, 0, 1, 1)
    cropped_image = action.CropScreenshot(image, 1, '', '')
    expected_image = image_util.FromRGBPixels(1, 1, [*RED], bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))

  def testCropCenter(self):
    """Tests that cropping works properly when not along any bounds."""
    image_width = 20
    # 20 x 3 green image with a single red pixel one column/row away from the
    # top left.
    # yapf: disable
    pixels = [*(GREEN * image_width),
              *GREEN, *RED, *(GREEN * (image_width - 2)),
              *(GREEN * image_width)]
    # yapf: enable
    image = image_util.FromRGBPixels(20, 3, pixels, bpp=3)
    action = ca.FixedRectCropAction(1, 1, 2, 2)
    cropped_image = action.CropScreenshot(image, 1, '', '')
    expected_image = image_util.FromRGBPixels(1, 1, [*RED], bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))

  def testCropScrollbarRemoved(self):
    """Tests that right columns are auto-removed to avoid scrollbars."""
    image_width = 20
    # 20 x 1 green image.
    pixels = [*(GREEN * image_width)]
    image = image_util.FromRGBPixels(20, 1, pixels, bpp=3)
    action = ca.FixedRectCropAction(0, 0, 20, 1)
    cropped_image = action.CropScreenshot(image, 1, '', '')
    expected_width = image_width - ca.FixedRectCropAction.SCROLLBAR_WIDTH
    expected_pixels = [*(GREEN * expected_width)]
    expected_image = image_util.FromRGBPixels(expected_width,
                                              1,
                                              expected_pixels,
                                              bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))

  def testBoundsClamping(self):
    """Tests that the bottom right coordinates are automatically clamped."""
    image_width = 20
    # 20 x 1 green image.
    pixels = [*(GREEN * image_width)]
    image = image_util.FromRGBPixels(20, 1, pixels, bpp=3)
    action = ca.FixedRectCropAction(0, 0, 999999, 999999)
    cropped_image = action.CropScreenshot(image, 1, '', '')
    expected_width = image_width - ca.FixedRectCropAction.SCROLLBAR_WIDTH
    expected_pixels = [*(GREEN * expected_width)]
    expected_image = image_util.FromRGBPixels(expected_width,
                                              1,
                                              expected_pixels,
                                              bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))

  def testNoneBounds(self):
    """Tests that None can be used to specify the bottom right coordinates."""
    image_width = 20
    # 20 x 1 green image.
    pixels = [*(GREEN * image_width)]
    image = image_util.FromRGBPixels(20, 1, pixels, bpp=3)
    action = ca.FixedRectCropAction(0, 0, None, None)
    cropped_image = action.CropScreenshot(image, 1, '', '')
    expected_width = image_width - ca.FixedRectCropAction.SCROLLBAR_WIDTH
    expected_pixels = [*(GREEN * expected_width)]
    expected_image = image_util.FromRGBPixels(expected_width,
                                              1,
                                              expected_pixels,
                                              bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))

  def testDprScaling(self):
    """Tests that crop bounds are scaled by DPR."""
    image_width = 20
    # 20 x 5 green image with a group of 4 red pixels towards the top left.
    # yapf: disable
    pixels = [
        *(GREEN * image_width),
        *(GREEN * image_width),
        *GREEN, *GREEN, *RED, *RED, *(GREEN * (image_width - 4)),
        *GREEN, *GREEN, *RED, *RED, *(GREEN * (image_width - 4)),
        *(GREEN * image_width),
    ]
    # yapf: enable
    image = image_util.FromRGBPixels(20, 5, pixels, bpp=3)
    action = ca.FixedRectCropAction(1, 1, 2, 2)
    cropped_image = action.CropScreenshot(image, 2.4, '', '')
    expected_pixels = [*(RED * 4)]
    expected_image = image_util.FromRGBPixels(2, 2, expected_pixels, bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))


class NonWhiteContentCropAction(unittest.TestCase):

  def testNoWhiteContent(self):
    """Tests behavior when there is no white content to remove."""
    # 3 x 3 green image.
    pixels = [*(GREEN * 9)]
    image = image_util.FromRGBPixels(3, 3, pixels, bpp=3)
    action = ca.NonWhiteContentCropAction()
    cropped_image = action.CropScreenshot(image, 1, '', '')
    expected_image = image_util.FromRGBPixels(3, 3, pixels, bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))

  def testSomeWhiteContent(self):
    """Tests behavior when there is some white content to remove."""
    # 4 x 4 white image with red pixels in the middle.
    # yapf: disable
    pixels = [*WHITE, *WHITE, *WHITE, *WHITE,
              *WHITE, *RED,   *RED,   *WHITE,
              *WHITE, *RED,   *RED,   *WHITE,
              *WHITE, *WHITE, *WHITE, *WHITE]
    # yapf: enable
    image = image_util.FromRGBPixels(4, 4, pixels, bpp=3)
    action = ca.NonWhiteContentCropAction()
    cropped_image = action.CropScreenshot(image, 1, '', '')
    expected_image = image_util.FromRGBPixels(2, 2, [*(RED * 4)], bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))

  def testSomeWhiteContentSinglePixel(self):
    """Checks for off-by-one errors when only a single pixel isn't white."""
    # 3 x 3 white image with a red pixel in the middle.
    # yapf: disable
    pixels = [*WHITE, *WHITE, *WHITE,
              *WHITE, *RED,   *WHITE,
              *WHITE, *WHITE, *WHITE]
    # yapf: enable
    image = image_util.FromRGBPixels(3, 3, pixels, bpp=3)
    action = ca.NonWhiteContentCropAction()
    cropped_image = action.CropScreenshot(image, 1, '', '')
    expected_image = image_util.FromRGBPixels(1, 1, [*RED], bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))

  def testAllWhiteContent(self):
    """Tests that trying to crop all white content is an error."""
    pixels = [*WHITE]
    image = image_util.FromRGBPixels(1, 1, pixels, bpp=3)
    action = ca.NonWhiteContentCropAction()
    with self.assertRaisesRegex(
        RuntimeError,
        'Attempted to crop to non-white content in an all white image'):
      action.CropScreenshot(image, 1, '', '')

  def testInitialCrop(self):
    """Tests that a provided initial crop is used first."""
    image_width = 20
    # A 20 x 3 image with one red and one green pixel.
    # yapf: disable
    pixels = [
        *WHITE, *WHITE, *WHITE, *(WHITE * (image_width - 3)),
        *WHITE, *RED,   *WHITE, *(WHITE * (image_width - 3)),
        *WHITE, *WHITE, *GREEN, *(WHITE * (image_width - 3)),
    ]
    # yapf: enable
    image = image_util.FromRGBPixels(20, 3, pixels, bpp=3)
    # This should crop out the green pixel.
    initial_crop = ca.FixedRectCropAction(0, 0, 2, 2)
    action = ca.NonWhiteContentCropAction(initial_crop=initial_crop)
    cropped_image = action.CropScreenshot(image, 1, '', '')
    expected_image = image_util.FromRGBPixels(1, 1, [*RED], bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))

  def testMacCrop(self):
    """Tests that some bottom rows are cropped on Mac due to rounded corners."""
    image_height = 30
    pixels = [*(RED * image_height)]
    image = image_util.FromRGBPixels(1, image_height, pixels, bpp=3)
    action = ca.NonWhiteContentCropAction()
    cropped_image = action.CropScreenshot(image, 1, '', 'mac')
    expected_image = image_util.FromRGBPixels(1,
                                              10,
                                              [*(RED * (image_height - 20))],
                                              bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))

  def testOffWhiteCrop(self):
    """Tests cropping of the first row on devices that produce off-white."""
    pixels = [*(RED * 9)]
    image = image_util.FromRGBPixels(3, 3, pixels, bpp=3)
    action = ca.NonWhiteContentCropAction()
    cropped_image = action.CropScreenshot(image, 1, 'SM-A235M', '')
    expected_image = image_util.FromRGBPixels(3, 2, [*(RED * 6)], bpp=3)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))

  def testCropWithAlpha(self):
    """Tests that cropping works when an alpha channel is provided."""
    white = (255, 255, 255, 255)
    red = (255, 0, 0, 255)
    # The same pixel data as testSomeWhiteContent, but with an alpha channel.
    # 4 x 4 white image with red pixels in the middle.
    # yapf: disable
    pixels = [*white, *white, *white, *white,
              *white, *red,   *red,   *white,
              *white, *red,   *red,   *white,
              *white, *white, *white, *white]
    # yapf: enable
    image = image_util.FromRGBPixels(4, 4, pixels, bpp=4)
    action = ca.NonWhiteContentCropAction()
    cropped_image = action.CropScreenshot(image, 1, '', '')
    expected_image = image_util.FromRGBPixels(2, 2, [*(red * 4)], bpp=4)
    self.assertTrue(
        image_util.AreEqual(cropped_image,
                            expected_image,
                            tolerance=0,
                            likely_equal=True))
