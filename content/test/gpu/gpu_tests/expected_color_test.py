# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
from typing import Any, Dict, List, Tuple
import unittest

import gpu_path_util

from gpu_tests import common_typing as ct
from gpu_tests import expected_color_test_cases
from gpu_tests import gpu_integration_test
from gpu_tests import skia_gold_heartbeat_integration_test_base as sghitb

from py_utils import cloud_storage
from telemetry.util import image_util
from telemetry.util import rgba_color

_MAPS_PERF_TEST_PATH = os.path.join(gpu_path_util.TOOLS_PERF_DIR, 'page_sets',
                                    'maps_perf_test')

_OFF_WHITE_TOP_ROW_DEVICES = {
    # Samsung A13.
    'SM-A135M',
    # Samsung A23.
    'SM-A235M',
}


class ExpectedColorTest(sghitb.SkiaGoldHeartbeatIntegrationTestBase):
  """Variant of a regular pixel test that only uses Gold to surface images.

  Instead of normal pixel comparison, correctness is verified by looking for
  expected colors in certain areas of a test image. This is meant for cases
  where a test produces images that are so noisy that it's impractical to use
  Gold normally for the test.
  """

  @classmethod
  def Name(cls) -> str:
    return 'expected_color'

  @classmethod
  def SetUpProcess(cls) -> None:
    cloud_storage.GetIfChanged(
        os.path.join(_MAPS_PERF_TEST_PATH, 'load_dataset'),
        cloud_storage.PUBLIC_BUCKET)
    super().SetUpProcess()

  @classmethod
  def _GetStaticServerDirs(cls) -> List[str]:
    static_dirs = super()._GetStaticServerDirs()
    static_dirs.append(_MAPS_PERF_TEST_PATH)
    return static_dirs

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    test_cases = expected_color_test_cases.MapsTestCases()
    test_cases.extend(expected_color_test_cases.MediaRecorderTestCases())
    for tc in test_cases:
      yield (tc.name, tc.url, [tc])

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', 'expected_color_expectations.txt')
    ]

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    super().RunActualGpuTest(test_path, args)
    test_case = args[0]
    # Some pixel tests require non-standard browser arguments. Need to
    # check before running each page that it can run in the current
    # browser instance.
    self.RestartBrowserIfNecessaryWithArgs(test_case.extra_browser_args)
    tab_data = sghitb.TabData(self.tab,
                              self.__class__.websocket_server,
                              is_default_tab=True)
    self.NavigateTo(test_path, tab_data)

    loop_state = sghitb.LoopState()
    for action in test_case.test_actions:
      action.Run(test_case, tab_data, loop_state, self)

    if test_case.ShouldCaptureFullScreenshot(self.browser):
      screenshot = self.tab.FullScreenshot(5)
    else:
      screenshot = self.tab.Screenshot(5)
    if screenshot is None:
      self.fail('Could not capture screenshot')

    dpr = self.tab.EvaluateJavaScript('window.devicePixelRatio')
    logging.info('devicePixelRatio is %s', dpr)

    # The bottom corners of Mac screenshots have black triangles due to the
    # rounded corners of Mac windows. So, crop the bottom few rows off now to
    # get rid of those. The triangles appear to be 5 pixels wide and tall
    # regardless of DPI, so 10 pixels should be sufficient. However, when
    # running under Python 3, 10 isn't quite enough for some reason, so use
    # 20 instead.
    if self.browser.platform.GetOSName() == 'mac':
      img_height = image_util.Height(screenshot)
      img_width = image_util.Width(screenshot)
      screenshot = image_util.Crop(screenshot, 0, 0, img_width, img_height - 20)
    # For some reason, the top row of the screenshot is very slightly off-white
    # instead of pure white on some devices, which messes with the crop
    # boundaries. So, chop off the top row now.
    if (self.tab.browser.platform.GetDeviceTypeName()
        in _OFF_WHITE_TOP_ROW_DEVICES):
      screenshot = image_util.Crop(screenshot, 0, 1,
                                   image_util.Width(screenshot),
                                   image_util.Height(screenshot) - 1)
    x1, y1, x2, y2 = _GetCropBoundaries(screenshot)
    screenshot = image_util.Crop(screenshot, x1, y1, x2 - x1, y2 - y1)

    self._ValidateScreenshotSamplesWithSkiaGold(self.tab, test_case, screenshot,
                                                dpr)

  def GetGoldOptionalKeys(self) -> Dict[str, str]:
    keys = super().GetGoldOptionalKeys()
    keys['expected_color_comment'] = (
        'This is an expected color test. Triaging in Gold will not affect test '
        'behavior.')
    return keys

  def _ValidateScreenshotSamplesWithSkiaGold(
      self, tab: ct.Tab,
      test_case: expected_color_test_cases.ExpectedColorTestCase,
      screenshot: ct.Screenshot, device_pixel_ratio: float) -> None:
    """Samples the given screenshot and verifies pixel color values.

    In case any of the samples do not match the expected color, it raises
    a Failure and uploads the image to Gold.

    Args:
      tab: the Telemetry Tab object that the test was run in.
      test_case: the GPU ExpectedColorTestCase object for the test.
      screenshot: the screenshot of the test page as a Telemetry Bitmap.
      device_pixel_ratio: the device pixel ratio for the test device as a float.
    """
    try:
      self._CompareScreenshotSamples(tab, screenshot, test_case,
                                     device_pixel_ratio)
    except Exception:
      # An exception raised from self.fail() indicates a failure.
      image_name = self._UrlToImageName(test_case.name)
      # We want to report the screenshot comparison failure, not any failures
      # related to Gold.
      try:
        self._UploadTestResultToSkiaGold(image_name, screenshot, test_case)
      except Exception as gold_exception:  # pylint: disable=broad-except
        logging.error(str(gold_exception))
      raise

  def _CompareScreenshotSamples(
      self, tab: ct.Tab, screenshot: ct.Screenshot,
      test_case: expected_color_test_cases.ExpectedColorTestCase,
      device_pixel_ratio: float) -> None:
    """Checks a screenshot for expected colors.

    Args:
      tab: the Telemetry Tab object that the test was run in.
      screenshot: the screenshot of the test page as a Telemetry Bitmap.
      test_case: the GPU ExpectedColorTestCase object for the test.
      device_pixel_ratio: the device pixel ratio for the test device as a float.

    Raises:
      AssertionError if the check fails for some reason.
    """

    def _CompareScreenshotWithExpectation(
        expectation: expected_color_test_cases.ExpectedColorExpectation):
      """Compares a portion of the screenshot to the given expectation.

      Fails the test if a the screenshot does not match within the tolerance.

      Args:
        expectation: A dict defining an expected color region. It must contain
            'location', 'size', and 'color' keys. See
            expected_color_test_cases.py for examples.
      """
      location = expectation.location
      size = expectation.size
      x0 = int(location.x * device_pixel_ratio)
      x1 = int((location.x + size.width) * device_pixel_ratio)
      y0 = int(location.y * device_pixel_ratio)
      y1 = int((location.y + size.height) * device_pixel_ratio)
      for x in range(x0, x1):
        for y in range(y0, y1):
          if (x < 0 or y < 0 or x >= image_util.Width(screenshot)
              or y >= image_util.Height(screenshot)):
            self.fail(('Expected pixel location [%d, %d] is out of range on ' +
                       '[%d, %d] image') % (x, y, image_util.Width(screenshot),
                                            image_util.Height(screenshot)))

          actual_color = image_util.GetPixelColor(screenshot, x, y)
          expected_color = rgba_color.RgbaColor(expectation.color.r,
                                                expectation.color.g,
                                                expectation.color.b,
                                                expectation.color.a)
          if not actual_color.IsEqual(expected_color, tolerance):
            self.fail('Expected pixel at %s (actual pixel (%s, %s)) to be %s '
                      'but got [%s, %s, %s, %s]' %
                      (location, x, y, expectation.color, actual_color.r,
                       actual_color.g, actual_color.b, actual_color.a))

    expected_colors = test_case.expected_colors

    # First scan through the expected_colors and see if there are any scale
    # factor overrides that would preempt the device pixel ratio. This
    # is mainly a workaround for complex tests like the Maps test.
    for device_type, scale_factor in test_case.scale_factor_overrides.items():
      # Require exact matches to avoid confusion, because some
      # machine models and names might be subsets of others
      # (e.g. Nexus 5 vs Nexus 5X).
      if tab.browser.platform.GetDeviceTypeName() == device_type:
        logging.warning(
            'Overriding device_pixel_ration %s with scale '
            'factor %s for device type %s', device_pixel_ratio, scale_factor,
            device_type)
        device_pixel_ratio = scale_factor
        break
    for color_expectation in expected_colors:
      tolerance = (test_case.base_tolerance
                   if color_expectation.tolerance is None else
                   color_expectation.tolerance)
      _CompareScreenshotWithExpectation(color_expectation)


def _GetCropBoundaries(screenshot: ct.Screenshot) -> Tuple[int, int, int, int]:
  """Returns the boundaries to crop the screenshot to.

  Specifically, we look for the boundaries where the white background
  transitions into the (non-white) content we care about.

  Args:
    screenshot: A screenshot returned by Tab.Screenshot() (numpy ndarray?)

  Returns:
    A 4-tuple (x1, y1, x2, y2) denoting the top left and bottom right
    coordinates to crop to.
  """
  img_height = image_util.Height(screenshot)
  img_width = image_util.Width(screenshot)

  # We include start/end as optional arguments as an optimization for finding
  # the lower right corner. If the original image is large and the non-white
  # portions are small and in the upper left (which is the most common case),
  # checking every row/column for white can take a while.
  def RowIsWhite(row, start=None, end=None):
    start = start or 0
    end = end or img_width
    for col in range(start, end):
      pixel = image_util.GetPixelColor(screenshot, col, row)
      if pixel.r != 255 or pixel.g != 255 or pixel.b != 255:
        return False
    return True

  def ColumnIsWhite(column, start=None, end=None):
    start = start or 0
    end = end or img_height
    for row in range(start, end):
      pixel = image_util.GetPixelColor(screenshot, column, row)
      if pixel.r != 255 or pixel.g != 255 or pixel.b != 255:
        return False
    return True

  x1 = y1 = 0
  x2 = img_width
  y2 = img_height
  for column in range(img_width):
    if not ColumnIsWhite(column):
      x1 = column
      break

  for row in range(img_height):
    if not RowIsWhite(row, start=x1):
      y1 = row
      break

  # We work from the right/bottom of the image here in case there are multiple
  # things that need to be tested separated by whitespace like is the case for
  # many video-related tests.
  for column in range(img_width - 1, x1, -1):
    if not ColumnIsWhite(column, start=y1):
      x2 = column
      break

  for row in range(img_height - 1, y1, -1):
    if not RowIsWhite(row, start=x1, end=x2):
      y2 = row
      break
  return x1, y1, x2, y2


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
