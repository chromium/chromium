# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math
import os
import random
import sys

from gpu_tests import color_profile_manager
from gpu_tests import gpu_integration_test
from gpu_tests import path_util

from telemetry.util import image_util
from telemetry.util import rgba_color

data_path = os.path.join(
    path_util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')

class ScreenshotSyncIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  """Tests that screenshots are properly synchronized with the frame on
  which they were requested.
  """

  @classmethod
  def Name(cls):
    """The name by which this test is invoked on the command line."""
    return 'screenshot_sync'

  # The command line options (which are passed to subclasses'
  # GenerateGpuTests) *must* be configured here, via a call to
  # SetParsedCommandLineOptions. If they are not, an error will be
  # raised when running the tests.
  _parsed_command_line_options = None

  @classmethod
  def SetParsedCommandLineOptions(cls, options):
    cls._parsed_command_line_options = options

  @classmethod
  def GetParsedCommandLineOptions(cls):
    return cls._parsed_command_line_options

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(ScreenshotSyncIntegrationTest, cls).AddCommandlineArgs(parser)
    parser.add_option(
      '--dont-restore-color-profile-after-test',
      dest='dont_restore_color_profile_after_test',
      action='store_true', default=False,
      help='(Mainly on Mac) don\'t restore the system\'s original color '
      'profile after the test completes; leave the system using the sRGB color '
      'profile. See http://crbug.com/784456.')

  @classmethod
  def SetUpProcess(cls):
    options = cls.GetParsedCommandLineOptions()
    color_profile_manager.ForceUntilExitSRGB(
      options.dont_restore_color_profile_after_test)
    super(cls, ScreenshotSyncIntegrationTest).SetUpProcess()
    cls.CustomizeBrowserArgs(cls._AddDefaultArgs([]))
    cls.StartBrowser()
    cls.SetStaticServerDirs([data_path])

  @staticmethod
  def _AddDefaultArgs(browser_args):
    # --test-type=gpu is used to suppress the "Google API Keys are
    # missing" infobar, which causes flakiness in tests.
    return [
      '--force-color-profile=srgb',
      '--ensure-forced-color-profile',
      '--test-type=gpu'] + browser_args

  @classmethod
  def GenerateGpuTests(cls, options):
    cls.SetParsedCommandLineOptions(options)
    yield('ScreenshotSync_SWRasterWithCanvas',
          'screenshot_sync_canvas.html',
          ('--disable-gpu-rasterization'))
    yield('ScreenshotSync_SWRasterWithDivs',
          'screenshot_sync_divs.html',
          ('--disable-gpu-rasterization'))
    yield('ScreenshotSync_GPURasterWithCanvas',
          'screenshot_sync_canvas.html',
          ('--force-gpu-rasterization'))
    yield('ScreenshotSync_GPURasterWithDivs',
          'screenshot_sync_divs.html',
          ('--force-gpu-rasterization'))

  def _Navigate(self, test_path):
    url = self.UrlOfStaticFilePath(test_path)
    # It's crucial to use the action_runner, rather than the tab's
    # Navigate method directly. It waits for the document ready state
    # to become interactive or better, avoiding critical race
    # conditions.
    self.tab.action_runner.Navigate(url)

  def _CheckColorMatchAtLocation(self, expectedRGB, screenshot, x, y):
    pixel_value = image_util.GetPixelColor(screenshot, x, y)
    # Allow for off-by-one errors due to color conversion.
    tolerance = 1
    if not expectedRGB.IsEqual(pixel_value, tolerance):
      error_message = ('Color mismatch at (%d, %d): expected (%d, %d, %d), ' +
                       'got (%d, %d, %d)') % (
                         x, y, expectedRGB.r, expectedRGB.g, expectedRGB.b,
                         pixel_value.r, pixel_value.g, pixel_value.b)
      self.fail(error_message)

  def _CheckScreenshot(self):
    canvasRGB = rgba_color.RgbaColor(random.randint(0, 255),
                                     random.randint(0, 255),
                                     random.randint(0, 255),
                                     255)
    tab = self.tab
    tab.EvaluateJavaScript(
        "window.draw({{ red }}, {{ green }}, {{ blue }});",
        red=canvasRGB.r, green=canvasRGB.g, blue=canvasRGB.b)
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

  def RunActualGpuTest(self, test_path, *args):
    browser_arg = args[0]
    self.RestartBrowserIfNecessaryWithArgs(self._AddDefaultArgs([browser_arg]))
    self._Navigate(test_path)
    repetitions = 20
    for _ in range(0, repetitions):
      self._CheckScreenshot()

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations',
                     'screenshot_sync_expectations.txt')]

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
