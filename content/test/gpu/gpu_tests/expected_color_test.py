# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
from typing import Any, Dict, List
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
        os.path.join(_MAPS_PERF_TEST_PATH, 'dataset', 'load_dataset'),
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

    screenshot = test_case.crop_action.CropScreenshot(
        screenshot, dpr, self.browser.platform.GetDeviceTypeName(),
        self.browser.platform.GetOSName())

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


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
