# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import math
import os
import random
import sys
from typing import Any, List
import unittest

from gpu_tests import color_profile_manager
from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test

import gpu_path_util

from telemetry.util import image_util
from telemetry.util import rgba_color


class ScreenshotSyncIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  """Tests that screenshots are properly synchronized with the frame on
  which they were requested.
  """

  @classmethod
  def Name(cls) -> str:
    """The name by which this test is invoked on the command line."""
    return 'screenshot_sync'

  @classmethod
  def AddCommandlineArgs(cls, parser: ct.CmdArgParser) -> None:
    super(ScreenshotSyncIntegrationTest, cls).AddCommandlineArgs(parser)
    parser.add_argument(
        '--dont-restore-color-profile-after-test',
        action='store_true',
        default=False,
        help=("(Mainly on Mac) don't restore the system's original color "
              'profile after the test completes; leave the system using the '
              'sRGB color profile. See http://crbug.com/784456.'))

  @classmethod
  def SetUpProcess(cls) -> None:
    super(cls, ScreenshotSyncIntegrationTest).SetUpProcess()
    options = cls.GetOriginalFinderOptions()
    color_profile_manager.ForceUntilExitSRGB(
        options.dont_restore_color_profile_after_test)
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs([gpu_path_util.GPU_DATA_DIR])

  @classmethod
  def GenerateBrowserArgs(cls, additional_args: List[str]) -> List[str]:
    """Adds default arguments to |additional_args|.

    See the parent class' method documentation for additional information.
    """
    default_args = super(ScreenshotSyncIntegrationTest,
                         cls).GenerateBrowserArgs(additional_args)
    default_args.extend([
        cba.FORCE_COLOR_PROFILE_SRGB,
        cba.ENSURE_FORCED_COLOR_PROFILE,
        # --test-type=gpu is used to suppress the "Google API Keys are
        # missing" and "Chrome for Testing" infobars, which cause flakiness
        # in tests.
        cba.TEST_TYPE_GPU,
    ])
    return default_args

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    yield ('ScreenshotSync_SWRasterWithCanvas', 'screenshot_sync_canvas.html',
           ['--disable-gpu-rasterization'])
    yield ('ScreenshotSync_SWRasterWithDivs', 'screenshot_sync_divs.html',
           ['--disable-gpu-rasterization'])
    yield ('ScreenshotSync_GPURasterWithCanvas', 'screenshot_sync_canvas.html',
           [cba.ENABLE_GPU_RASTERIZATION])
    yield ('ScreenshotSync_GPURasterWithDivs', 'screenshot_sync_divs.html',
           [cba.ENABLE_GPU_RASTERIZATION])

  def _Navigate(self, test_path: str) -> None:
    url = self.UrlOfStaticFilePath(test_path)
    # It's crucial to use the action_runner, rather than the tab's
    # Navigate method directly. It waits for the document ready state
    # to become interactive or better, avoiding critical race
    # conditions.
    self.tab.action_runner.Navigate(url)

  def _CheckColorMatchAtLocation(self, expectedRGB: rgba_color.RgbaColor,
                                 screenshot: ct.Screenshot, x: int,
                                 y: int) -> None:
    pixel_value = image_util.GetPixelColor(screenshot, x, y)
    # Allow for off-by-one errors due to color conversion.
    tolerance = 1
    # Pixel 4 devices require a slightly higher tolerance. See
    # crbug.com/1166379.
    if self.tab.browser.platform.GetDeviceTypeName() == 'Pixel 4':
      tolerance = 7
    if not expectedRGB.IsEqual(pixel_value, tolerance):
      error_message = ('Color mismatch at (%d, %d): expected (%d, %d, %d), ' +
                       'got (%d, %d, %d)') % (
                           x, y, expectedRGB.r, expectedRGB.g, expectedRGB.b,
                           pixel_value.r, pixel_value.g, pixel_value.b)
      self.fail(error_message)

  def _CheckScreenshot(self) -> None:
    canvasRGB = rgba_color.RgbaColor(
        random.randint(0, 255), random.randint(0, 255), random.randint(0, 255),
        255)
    tab = self.tab
    tab.EvaluateJavaScript('window.draw({{ red }}, {{ green }}, {{ blue }});',
                           red=canvasRGB.r,
                           green=canvasRGB.g,
                           blue=canvasRGB.b)
    screenshot = tab.Screenshot(10)
    # Avoid checking along antialiased boundary due to limited Adreno 3xx
    # interpolation precision (crbug.com/847984). We inset by one CSS pixel
    # adjusted by the device pixel ratio.
    inset = int(math.ceil(tab.EvaluateJavaScript('window.devicePixelRatio')))
    # It seems that we should be able to set start_x to 2 * inset (one to
    # account for the inner div having left=1 and one to avoid sampling the
    # aa edge). For reasons not fully understood this is insufficent on
    # several bots (N9, 6P, mac-rel).
    start_x = 10
    start_y = inset
    outer_size = 256 - inset
    skip = 10
    for y in range(start_y, outer_size, skip):
      for x in range(start_x, outer_size, skip):
        self._CheckColorMatchAtLocation(canvasRGB, screenshot, x, y)

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    browser_arg = args[0]
    self.RestartBrowserIfNecessaryWithArgs([browser_arg])
    self._Navigate(test_path)
    repetitions = 20
    for _ in range(0, repetitions):
      self._CheckScreenshot()

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'screenshot_sync_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
