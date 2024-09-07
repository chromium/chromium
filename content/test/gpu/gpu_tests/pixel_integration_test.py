# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
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
from gpu_tests.util import host_information

import gpu_path_util

from telemetry.util import image_util, screenshot

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
    if host_information.IsMac():
      serial_globs |= {
          # Flakily produces only half the image when run in parallel on Mac.
          'Pixel_OffscreenCanvasWebGL*',
          # Flakily fails to capture a screenshot when run in parallel on Mac.
          'Pixel_VideoStreamFrom*',
      }
    if host_information.IsWindows():
      serial_globs |= {
          # Serialized for the same reasons as in trace_integration_test.
          'Pixel_DirectComposition_Underlay*',
          'Pixel_DirectComposition_Video*',
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

    if host_information.IsLinux() and host_information.IsAmdGpu():
      serial_tests |= {
          # Flakily produces slightly incorrect images when run in parallel on
          # AMD.
          'Pixel_OffscreenCanvasWebGLSoftwareCompositingWorker',
      }

    if host_information.IsWindows() and host_information.IsArmCpu():
      serial_tests |= {
          # Context loss tests don't like being run in parallel on Windows
          # arm64.
          'Pixel_Video_Context_Loss_VP9',
          'Pixel_WebGLContextRestored',
          'Pixel_WebGLSadCanvas',
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
    pages += namespace.NoGpuProcessPages(cls.test_base_name)
    if host_information.IsMac():
      pages += namespace.MacSpecificPages(cls.test_base_name)
      # Unfortunately we don't have a browser instance here so can't tell
      # whether we should really run these tests. They're short-circuited to a
      # certain degree on the other platforms.
      pages += namespace.DualGPUMacSpecificPages(cls.test_base_name)
    if host_information.IsWindows():
      pages += namespace.DirectCompositionPages(cls.test_base_name)
      pages += namespace.HdrTestPages(cls.test_base_name)
    # Only run SwiftShader tests on platforms that support it.
    if host_information.IsLinux() or (host_information.IsWindows()
                                      and not host_information.IsArmCpu()):
      pages += namespace.SwiftShaderPages(cls.test_base_name)
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
    if test_case.RequiresFullScreenOSScreenshot():
      if not self.browser.platform.CanTakeScreenshot():
        logging.warning('Skipping the test because the platform does not '
                        'support OS screenshots')
        self.skipTest('The platform does not support fullscreen OS screenshot')

      fh = screenshot.TryCaptureScreenShot(self.browser.platform, None,
                                           self._GetScreenshotTimeout())
      if fh is None:
        self.fail('Unable to get file handle of the screenshot')
      screen_shot = image_util.FromPngFile(fh.GetAbsPath())
    elif test_case.ShouldCaptureFullScreenshot(self.browser):
      # Screenshot on Fuchsia can take a long time. See crbug.com/1376684.
      screen_shot = tab.FullScreenshot(15)
    else:
      screen_shot = tab.Screenshot(self._GetScreenshotTimeout())

    if screen_shot is None:
      self.fail('Could not capture screenshot')

    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    screen_shot = test_case.crop_action.CropScreenshot(
        screen_shot, dpr, self.browser.platform.GetDeviceTypeName(),
        self.browser.platform.GetOSName())

    image_name = self._UrlToImageName(test_case.name)
    self._UploadTestResultToSkiaGold(image_name, screen_shot, test_case)

  def _GetScreenshotTimeout(self) -> float:
    # Parallel jobs can cause heavier tests to flakily time out when capturing
    # screenshots, so increase the base timeout depending on the number of
    # parallel jobs. Aim for 2x the timeout with 4 jobs.
    multiplier = 1 + (self.child.jobs - 1) / 3.0
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
