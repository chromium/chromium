# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import posixpath
import sys
import time
from typing import Any, List, Set
import unittest

from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test
from gpu_tests import pixel_test_pages
from gpu_tests import skia_gold_heartbeat_integration_test_base as sghitb

import gpu_path_util

from telemetry.util import image_util

# We're not sure if this is actually a fixed value or not, but it's 10 pixels
# wide on the only device we've had issues with so far (Pixel 4), so assume
# 10 pixels until we find evidence supporting something else.
SCROLLBAR_WIDTH = 10

DEFAULT_SCREENSHOT_TIMEOUT = 5
SLOW_SCREENSHOT_MULTIPLIER = 4


class PixelIntegrationTest(sghitb.SkiaGoldHeartbeatIntegrationTestBase):
  """GPU pixel tests backed by Skia Gold and Telemetry."""
  test_base_name = 'Pixel'

  @classmethod
  def Name(cls) -> str:
    """The name by which this test is invoked on the command line."""
    return 'pixel'

  @classmethod
  def _SuiteSupportsParallelTests(cls) -> bool:
    return True

  def _GetSerialGlobs(self) -> Set[str]:
    serial_globs = set()
    if sys.platform == 'darwin':
      serial_globs |= {
          # Flakily produces only half the image when run in parallel on Mac.
          'Pixel_OffscreenCanvasWebGL*',
          # Flakily fails to capture a screenshot when run in parallel on Mac.
          'Pixel_VideoStreamFrom*',
      }
    return serial_globs

  def _GetSerialTests(self) -> Set[str]:
    serial_tests = {
        # High/low power tests don't work properly with multiple browsers
        # active.
        'Pixel_OffscreenCanvasIBRCWebGLHighPerfWorker',
        'Pixel_OffscreenCanvasIBRCWebGLMain',
        'Pixel_OffscreenCanvasIBRCWebGLWorker',
        'Pixel_WebGLLowToHighPower',
        'Pixel_WebGLLowToHighPowerAlphaFalse',
    }

    if sys.platform.startswith('linux'):
      serial_tests |= {
          # Flakily produces slightly incorrect images when run in parallel on
          # AMD.
          'Pixel_OffscreenCanvasWebGLSoftwareCompositingWorker',
      }
    return serial_tests

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    namespace = pixel_test_pages.PixelTestPages
    pages = namespace.DefaultPages(cls.test_base_name)
    pages += namespace.GpuRasterizationPages(cls.test_base_name)
    pages += namespace.ExperimentalCanvasFeaturesPages(cls.test_base_name)
    pages += namespace.LowLatencyPages(cls.test_base_name)
    pages += namespace.WebGPUPages(cls.test_base_name)
    pages += namespace.WebGPUCanvasCapturePages(cls.test_base_name)
    pages += namespace.PaintWorkletPages(cls.test_base_name)
    pages += namespace.VideoFromCanvasPages(cls.test_base_name)
    # pages += namespace.NoGpuProcessPages(cls.test_base_name)
    # The following pages should run only on platforms where SwiftShader is
    # enabled. They are skipped on other platforms through test expectations.
    # pages += namespace.SwiftShaderPages(cls.test_base_name)
    if sys.platform.startswith('darwin'):
      pages += namespace.MacSpecificPages(cls.test_base_name)
      # Unfortunately we don't have a browser instance here so can't tell
      # whether we should really run these tests. They're short-circuited to a
      # certain degree on the other platforms.
      pages += namespace.DualGPUMacSpecificPages(cls.test_base_name)
    if sys.platform.startswith('win'):
      pages += namespace.DirectCompositionPages(cls.test_base_name)
      pages += namespace.HdrTestPages(cls.test_base_name)
    for p in pages:
      yield (p.name, posixpath.join(gpu_path_util.GPU_DATA_RELATIVE_PATH,
                                    p.url), [p])

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    super().RunActualGpuTest(test_path, args)
    test_case = args[0]
    # Some pixel tests require non-standard browser arguments. Need to
    # check before running each test case that it can run in the current
    # browser instance.
    self.RestartBrowserIfNecessaryWithArgs(test_case.browser_args)
    tab_data = sghitb.TabData(self.tab,
                              self.__class__.websocket_server,
                              is_default_tab=True)
    self.NavigateTo(test_path, tab_data)

    loop_state = sghitb.LoopState()
    for action in test_case.test_actions:
      action.Run(test_case, tab_data, loop_state, self)
    self._RunSkiaGoldBasedPixelTest(test_case)

    if (test_case.used_custom_test_actions
        or test_case.restart_browser_after_test):
      self._RestartBrowser(
          'Must restart after non-standard test actions or if required by test')
      if test_case.used_custom_test_actions and self.IsDualGPUMacLaptop():
        # Give the system a few seconds to reliably indicate that the
        # low-power GPU is active again, to avoid race conditions if the next
        # test makes assertions about the active GPU.
        time.sleep(4)

  def GetExpectedCrashes(self, args: ct.TestArgs) -> None:
    """Returns which crashes, per process type, to expect for the current test.

    Args:
      args: The list passed to _RunGpuTest()

    Returns:
      A dictionary mapping crash types as strings to the number of expected
      crashes of that type. Examples include 'gpu' for the GPU process,
      'renderer' for the renderer process, and 'browser' for the browser
      process.
    """
    # args[0] is the PixelTestPage for the current test.
    return args[0].expected_per_process_crashes

  def _RunSkiaGoldBasedPixelTest(
      self, test_case: pixel_test_pages.PixelTestPage) -> None:
    """Captures and compares a test image using Skia Gold.

    Raises an Exception if the comparison fails.

    Args:
      test_case: the GPU PixelTestPage object for the test.
    """
    tab = self.tab

    if test_case.ShouldCaptureFullScreenshot(self.browser):
      # Screenshot on Fuchsia can take a long time. See crbug.com/1376684.
      screenshot = tab.FullScreenshot(15)
    else:
      screenshot = tab.Screenshot(self._GetScreenshotTimeout())
    if screenshot is None:
      self.fail('Could not capture screenshot')
    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    if test_case.test_rect:
      start_x = int(test_case.test_rect[0] * dpr)
      start_y = int(test_case.test_rect[1] * dpr)
      # When actually clamping the value, it's possible we'll catch the
      # scrollbar, so account for its width in the clamp.
      end_x = min(int(test_case.test_rect[2] * dpr),
                  image_util.Width(screenshot) - SCROLLBAR_WIDTH)
      end_y = min(int(test_case.test_rect[3] * dpr),
                  image_util.Height(screenshot))
      crop_width = end_x - start_x
      crop_height = end_y - start_y
      screenshot = image_util.Crop(screenshot, start_x, start_y, crop_width,
                                   crop_height)

    image_name = self._UrlToImageName(test_case.name)
    self._UploadTestResultToSkiaGold(image_name, screenshot, test_case)

  def _GetScreenshotTimeout(self):
    multiplier = 1
    if self._IsSlowTest():
      multiplier = SLOW_SCREENSHOT_MULTIPLIER
    return DEFAULT_SCREENSHOT_TIMEOUT * multiplier

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'pixel_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
