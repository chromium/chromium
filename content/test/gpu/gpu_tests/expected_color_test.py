# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from typing import Any, Dict, List

from gpu_tests import common_typing as ct
from gpu_tests import pixel_test_pages
from gpu_tests import skia_gold_integration_test_base

from telemetry.util import image_util
from telemetry.util import rgba_color

ExpectedColorExpectation = Dict[str, Any]


class ExpectedColorTest(
    skia_gold_integration_test_base.SkiaGoldIntegrationTestBase):
  """Variant of a regular pixel test that only uses Gold to surface images.

  Instead of normal pixel comparison, correctness is verified by looking for
  expected colors in certain areas of a test image. This is meant for cases
  where a test produces images that are so noisy that it's impractical to use
  Gold normally for the test.
  """

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    raise NotImplementedError(
        'RunActualGpuTest must be overridden in a subclass')

  def GetGoldJsonKeys(self,
                      page: pixel_test_pages.PixelTestPage) -> Dict[str, str]:
    keys = super().GetGoldJsonKeys(page)
    keys['expected_color_comment'] = (
        'This is an expected color test. Triaging in Gold will not affect test '
        'behavior.')
    return keys

  def _ValidateScreenshotSamplesWithSkiaGold(self, tab: ct.Tab,
                                             page: 'ExpectedColorPixelTestPage',
                                             screenshot: ct.Screenshot,
                                             device_pixel_ratio: float) -> None:
    """Samples the given screenshot and verifies pixel color values.

    In case any of the samples do not match the expected color, it raises
    a Failure and uploads the image to Gold.

    Args:
      tab: the Telemetry Tab object that the test was run in.
      page: the GPU PixelTestPage object for the test.
      screenshot: the screenshot of the test page as a Telemetry Bitmap.
      device_pixel_ratio: the device pixel ratio for the test device as a float.
    """
    try:
      self._CompareScreenshotSamples(tab, screenshot, page, device_pixel_ratio)
    except Exception:
      # An exception raised from self.fail() indicates a failure.
      image_name = self._UrlToImageName(page.name)
      # We want to report the screenshot comparison failure, not any failures
      # related to Gold.
      try:
        self._UploadTestResultToSkiaGold(image_name, screenshot, page)
      except Exception as gold_exception:  # pylint: disable=broad-except
        logging.error(str(gold_exception))
      raise

  def _CompareScreenshotSamples(self, tab: ct.Tab, screenshot: ct.Screenshot,
                                page: 'ExpectedColorPixelTestPage',
                                device_pixel_ratio: float) -> None:
    """Checks a screenshot for expected colors.

    Args:
      tab: the Telemetry Tab object that the test was run in.
      screenshot: the screenshot of the test page as a Telemetry Bitmap.
      page: the GPU PixelTestPage object for the test.
      device_pixel_ratio: the device pixel ratio for the test device as a float.

    Raises:
      AssertionError if the check fails for some reason.
    """

    def _CompareScreenshotWithExpectation(
        expectation: ExpectedColorExpectation):
      """Compares a portion of the screenshot to the given expectation.

      Fails the test if a the screenshot does not match within the tolerance.

      Args:
        expectation: A dict defining an expected color region. It must contain
            'location', 'size', and 'color' keys. See pixel_test_pages.py for
            examples.
      """
      location = expectation['location']
      size = expectation['size']
      x0 = int(location[0] * device_pixel_ratio)
      x1 = int((location[0] + size[0]) * device_pixel_ratio)
      y0 = int(location[1] * device_pixel_ratio)
      y1 = int((location[1] + size[1]) * device_pixel_ratio)
      for x in range(x0, x1):
        for y in range(y0, y1):
          if (x < 0 or y < 0 or x >= image_util.Width(screenshot)
              or y >= image_util.Height(screenshot)):
            self.fail(('Expected pixel location [%d, %d] is out of range on ' +
                       '[%d, %d] image') % (x, y, image_util.Width(screenshot),
                                            image_util.Height(screenshot)))

          actual_color = image_util.GetPixelColor(screenshot, x, y)
          expected_color = rgba_color.RgbaColor(
              expectation['color'][0], expectation['color'][1],
              expectation['color'][2],
              expectation['color'][3] if len(expectation['color']) > 3 else 255)
          if not actual_color.IsEqual(expected_color, tolerance):
            self.fail('Expected pixel at %s (actual pixel (%s, %s)) to be %s '
                      'but got [%s, %s, %s, %s]' %
                      (location, x, y, expectation['color'], actual_color.r,
                       actual_color.g, actual_color.b, actual_color.a))

    expected_colors = page.expected_colors
    tolerance = page.tolerance
    test_machine_name = self.GetOriginalFinderOptions().test_machine_name

    # First scan through the expected_colors and see if there are any scale
    # factor overrides that would preempt the device pixel ratio. This
    # is mainly a workaround for complex tests like the Maps test.
    for expectation in expected_colors:
      if 'scale_factor_overrides' in expectation:
        for override in expectation['scale_factor_overrides']:
          # Require exact matches to avoid confusion, because some
          # machine models and names might be subsets of others
          # (e.g. Nexus 5 vs Nexus 5X).
          if ('device_type' in override
              and (tab.browser.platform.GetDeviceTypeName() ==
                   override['device_type'])):
            logging.warning(
                'Overriding device_pixel_ration %s with scale '
                'factor %s for device type %s', device_pixel_ratio,
                override['scale_factor'], override['device_type'])
            device_pixel_ratio = override['scale_factor']
            break
          if (test_machine_name and 'machine_name' in override
              and override['machine_name'] == test_machine_name):
            logging.warning(
                'Overriding device_pixel_ratio %s with scale '
                'factor %s for machine name %s', device_pixel_ratio,
                override['scale_factor'], test_machine_name)
            device_pixel_ratio = override['scale_factor']
            break
        # Only support one "scale_factor_overrides" in the expectation format.
        break
    for expectation in expected_colors:
      if 'scale_factor_overrides' in expectation:
        continue
      _CompareScreenshotWithExpectation(expectation)


class ExpectedColorPixelTestPage(pixel_test_pages.PixelTestPage):
  """Extension of PixelTestPage with expected color information."""

  def __init__(self, expected_colors: List[ExpectedColorExpectation], *args,
               **kwargs):
    # The tolerance when comparing against the reference image.
    self.tolerance = kwargs.pop('tolerance', 2)

    super().__init__(*args, **kwargs)
    # The expected colors can be specified as a list of dictionaries. The format
    # is only defined by contract with _CompareScreenshotSamples in
    # expected_color_test.py.
    self.expected_colors = expected_colors
