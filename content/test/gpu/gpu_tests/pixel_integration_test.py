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
from gpu_tests import skia_gold_integration_test_base

import gpu_path_util

from telemetry.util import image_util

# We're not sure if this is actually a fixed value or not, but it's 10 pixels
# wide on the only device we've had issues with so far (Pixel 4), so assume
# 10 pixels until we find evidence supporting something else.
SCROLLBAR_WIDTH = 10


class PixelIntegrationTest(
    skia_gold_integration_test_base.SkiaGoldIntegrationTestBase):
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
    page = args[0]
    # Some pixel tests require non-standard browser arguments. Need to
    # check before running each page that it can run in the current
    # browser instance.
    self.RestartBrowserIfNecessaryWithArgs(page.browser_args)
    url = self.UrlOfStaticFilePath(test_path)
    # This property actually comes off the class, not 'self'.
    tab = self.tab
    tab.Navigate(
        url,
        script_to_evaluate_on_commit=self._dom_automation_controller_script)

    try:
      tab.action_runner.WaitForJavaScriptCondition(
          'domAutomationController._proceed', timeout=page.timeout)
    except:
      # Only log messages during exceptions here, they'll otherwise be logged
      # below if the test progresses to the first domAutomationController.send.
      test_messages = _TestHarnessMessages(tab)
      if test_messages:
        logging.info('Logging messages from the test:\n%s', test_messages)
      raise

    do_page_action = tab.EvaluateJavaScript(
        'domAutomationController._readyForActions')
    try:
      if do_page_action:
        # The page action may itself signal test failure via self.fail().
        self._DoPageAction(tab, page)
      self._RunSkiaGoldBasedPixelTest(page)
    finally:
      test_messages = _TestHarnessMessages(tab)
      if test_messages:
        logging.info('Logging messages from the test:\n%s', test_messages)
      if do_page_action or page.restart_browser_after_test:
        self._RestartBrowser(
            'Must restart after page actions or if required by test')
        if do_page_action and self._IsDualGPUMacLaptop():
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

  def _RunSkiaGoldBasedPixelTest(self,
                                 page: pixel_test_pages.PixelTestPage) -> None:
    """Captures and compares a test image using Skia Gold.

    Raises an Exception if the comparison fails.

    Args:
      page: the GPU PixelTestPage object for the test.
    """
    tab = self.tab
    # Actually run the test and capture the screenshot.
    if not tab.EvaluateJavaScript('domAutomationController._succeeded'):
      self.fail('page indicated test failure')

    if page.ShouldCaptureFullScreenshot(self.browser):
      # Screenshot on Fuchsia can take a long time. See crbug.com/1376684.
      screenshot = tab.FullScreenshot(15)
    else:
      screenshot = tab.Screenshot(5)
    if screenshot is None:
      self.fail('Could not capture screenshot')
    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    if page.test_rect:
      start_x = int(page.test_rect[0] * dpr)
      start_y = int(page.test_rect[1] * dpr)
      # When actually clamping the value, it's possible we'll catch the
      # scrollbar, so account for its width in the clamp.
      end_x = min(int(page.test_rect[2] * dpr),
                  image_util.Width(screenshot) - SCROLLBAR_WIDTH)
      end_y = min(int(page.test_rect[3] * dpr), image_util.Height(screenshot))
      crop_width = end_x - start_x
      crop_height = end_y - start_y
      screenshot = image_util.Crop(screenshot, start_x, start_y, crop_width,
                                   crop_height)

    image_name = self._UrlToImageName(page.name)
    self._UploadTestResultToSkiaGold(image_name, screenshot, page)

  def _DoPageAction(self, tab: ct.Tab,
                    page: pixel_test_pages.PixelTestPage) -> None:
    getattr(self, '_' + page.optional_action)(tab, page)
    # Now that we've done the page's specific action, wait for it to
    # report completion.
    tab.action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout=300)

  def _AssertLowPowerGPU(self) -> None:
    if self._IsDualGPUMacLaptop():
      if not self._IsIntelGPUActive():
        self.fail("Low power GPU should have been active but wasn't")

  def _AssertHighPerformanceGPU(self) -> None:
    if self._IsDualGPUMacLaptop():
      if self._IsIntelGPUActive():
        self.fail("High performance GPU should have been active but wasn't")

  #
  # Optional actions pages can take.
  # These are specified as methods taking the tab and the page as
  # arguments.
  #
  # pylint: disable=no-self-use
  def _CrashGpuProcess(self, tab: ct.Tab,
                       page: pixel_test_pages.PixelTestPage) -> None:
    # Crash the GPU process.
    #
    # This used to create a new tab and navigate it to
    # chrome://gpucrash, but there was enough unreliability
    # navigating between these tabs (one of which was created solely
    # in order to navigate to chrome://gpucrash) that the simpler
    # solution of provoking the GPU process crash from this renderer
    # process was chosen.
    del page  # Unused in this particular action.
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')

  def _CrashGpuProcessTwiceWaitForContextRestored(
      self, tab: ct.Tab, page: pixel_test_pages.PixelTestPage) -> None:
    # Crash the GPU process twice.
    del page  # Unused in this particular action.
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')
    # This is defined in the specific test's page.
    tab.action_runner.WaitForJavaScriptCondition('window.contextRestored',
                                                 timeout=30)
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')

  # pylint: enable=no-self-use

  def _SwitchTabs(self, tab: ct.Tab,
                  page: pixel_test_pages.PixelTestPage) -> None:
    del page  # Unused in this particular action.
    if not tab.browser.supports_tab_control:
      self.fail('Browser must support tab control')
    dummy_tab = tab.browser.tabs.New()
    dummy_tab.Activate()
    # Wait for 2 seconds so that new tab becomes visible.
    dummy_tab.action_runner.Wait(2)
    tab.Activate()

  def _SwitchTabsAndCopyImage(self, tab: ct.Tab,
                              page: pixel_test_pages.PixelTestPage) -> None:
    del page  # Unused in this particular action.
    if not tab.browser.supports_tab_control:
      self.fail('Browser must support tab control')
    dummy_tab = tab.browser.tabs.New()
    dummy_tab.Activate()
    # Wait for 2 seconds so that new tab becomes visible.
    dummy_tab.action_runner.Wait(2)
    # Close new tab.
    dummy_tab.Close()
    tab.EvaluateJavaScript('copyImage()')

  def _RunTestWithHighPerformanceTab(self, tab: ct.Tab,
                                     page: pixel_test_pages.PixelTestPage
                                     ) -> None:
    del page  # Unused in this particular action.
    if not self._IsDualGPUMacLaptop():
      # Short-circuit this test.
      logging.info('Short-circuiting test because not running on dual-GPU Mac '
                   'laptop')
      tab.EvaluateJavaScript('initialize(false)')
      tab.action_runner.WaitForJavaScriptCondition(
          'domAutomationController._readyForActions', timeout=30)
      tab.EvaluateJavaScript('runToCompletion()')
      return
    # Reset the ready state of the harness.
    tab.EvaluateJavaScript('domAutomationController._readyForActions = false')
    high_performance_tab = tab.browser.tabs.New()
    high_performance_tab.Navigate(
        self.UrlOfStaticFilePath(
            posixpath.join(gpu_path_util.GPU_DATA_RELATIVE_PATH,
                           'functional_webgl_high_performance.html')),
        script_to_evaluate_on_commit=self._dom_automation_controller_script)
    high_performance_tab.action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout=30)
    # Wait a few seconds for the GPU switched notification to propagate
    # throughout the system.
    time.sleep(5)
    # Switch back to the main tab and quickly start its rendering, while the
    # high-power GPU is still active.
    tab.Activate()
    tab.EvaluateJavaScript('initialize(true)')
    tab.action_runner.WaitForJavaScriptCondition(
        'domAutomationController._readyForActions', timeout=30)
    # Close the high-performance tab.
    high_performance_tab.Close()
    # Wait for ~15 seconds for the system to switch back to the
    # integrated GPU.
    time.sleep(15)
    # Run the page to completion.
    tab.EvaluateJavaScript('runToCompletion()')

  def _RunLowToHighPowerTest(self, tab: ct.Tab,
                             page: pixel_test_pages.PixelTestPage) -> None:
    del page  # Unused in this particular action.
    is_dual_gpu = self._IsDualGPUMacLaptop()
    tab.EvaluateJavaScript('initialize(' +
                           ('true' if is_dual_gpu else 'false') + ')')
    # The harness above will take care of waiting for the test to
    # complete with either a success or failure.

  def _RunOffscreenCanvasIBRCWebGLTest(self, tab: ct.Tab,
                                       page: pixel_test_pages.PixelTestPage
                                       ) -> None:
    del page  # Unused in this particular action.
    self._AssertLowPowerGPU()
    tab.EvaluateJavaScript('setup()')
    # Wait a few seconds for any (incorrect) GPU switched
    # notifications to propagate throughout the system.
    time.sleep(5)
    self._AssertLowPowerGPU()
    tab.EvaluateJavaScript('render()')

  def _RunOffscreenCanvasIBRCWebGLHighPerfTest(
      self, tab: ct.Tab, page: pixel_test_pages.PixelTestPage) -> None:
    del page  # Unused in this particular action.
    self._AssertLowPowerGPU()
    tab.EvaluateJavaScript('setup(true)')
    # Wait a few seconds for any (incorrect) GPU switched
    # notifications to propagate throughout the system.
    time.sleep(5)
    self._AssertHighPerformanceGPU()
    tab.EvaluateJavaScript('render()')

  # pylint: disable=R0201
  def _ScrollOutAndBack(self, tab: ct.Tab,
                        page: pixel_test_pages.PixelTestPage) -> None:
    del page  # Unused in this particular action.
    tab.EvaluateJavaScript('scrollOutAndBack()')

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'pixel_expectations.txt')
    ]


def _TestHarnessMessages(tab: ct.Tab) -> str:
  return tab.EvaluateJavaScript('domAutomationController._messages')


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
