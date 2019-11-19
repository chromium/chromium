# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import time

from gpu_tests import gpu_integration_test
from gpu_tests import pixel_test_pages
from gpu_tests import skia_gold_integration_test_base

from telemetry.util import image_util

test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._proceed = false;

  domAutomationController._readyForActions = false;
  domAutomationController._succeeded = false;
  domAutomationController._finished = false;

  domAutomationController.send = function(msg) {
    domAutomationController._proceed = true;
    let lmsg = msg.toLowerCase();
    if (lmsg == "ready") {
      domAutomationController._readyForActions = true;
    } else {
      domAutomationController._finished = true;
      if (lmsg == "success") {
        domAutomationController._succeeded = true;
      } else {
        domAutomationController._succeeded = false;
      }
    }
  }

  window.domAutomationController = domAutomationController;
"""


class PixelIntegrationTest(
    skia_gold_integration_test_base.SkiaGoldIntegrationTestBase):
  """GPU pixel tests backed by Skia Gold and Telemetry."""
  test_base_name = 'Pixel'

  @classmethod
  def Name(cls):
    """The name by which this test is invoked on the command line."""
    return 'pixel'

  @classmethod
  def GenerateGpuTests(cls, options):
    cls.SetParsedCommandLineOptions(options)
    namespace = pixel_test_pages.PixelTestPages
    pages = namespace.DefaultPages(cls.test_base_name)
    pages += namespace.GpuRasterizationPages(cls.test_base_name)
    pages += namespace.ExperimentalCanvasFeaturesPages(cls.test_base_name)
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
      pages += namespace.LowLatencySwapChainPages(cls.test_base_name)
    for p in pages:
      yield(p.name,
           skia_gold_integration_test_base.GPU_RELATIVE_PATH + p.url,
           (p))

  def RunActualGpuTest(self, test_path, *args):
    page = args[0]
    # Some pixel tests require non-standard browser arguments. Need to
    # check before running each page that it can run in the current
    # browser instance.
    self.RestartBrowserIfNecessaryWithArgs(self._AddDefaultArgs(
      page.browser_args))
    url = self.UrlOfStaticFilePath(test_path)
    # This property actually comes off the class, not 'self'.
    tab = self.tab
    tab.Navigate(url, script_to_evaluate_on_commit=test_harness_script)
    tab.action_runner.WaitForJavaScriptCondition(
      'domAutomationController._proceed', timeout=300)
    do_page_action = tab.EvaluateJavaScript(
      'domAutomationController._readyForActions')
    if do_page_action:
      self._DoPageAction(tab, page)
    self._RunSkiaGoldBasedPixelTest(do_page_action, page)

  def _RunSkiaGoldBasedPixelTest(self, do_page_action, page):
    """Captures and compares a test image using Skia Gold.

    Raises an Exception if the comparison fails.

    Args:
      do_page_action: a bool indicating if an action was run on the page.
      page: the GPU PixelTestPage object for the test.
    """
    tab = self.tab
    try:
      # Actually run the test and capture the screenshot.
      if not tab.EvaluateJavaScript('domAutomationController._succeeded'):
        self.fail('page indicated test failure')
      if not tab.screenshot_supported:
        self.fail('Browser does not support screenshot capture')
      screenshot = tab.Screenshot(5)
      if screenshot is None:
        self.fail('Could not capture screenshot')
      dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
      if page.test_rect:
        screenshot = image_util.Crop(
            screenshot, int(page.test_rect[0] * dpr),
            int(page.test_rect[1] * dpr), int(page.test_rect[2] * dpr),
            int(page.test_rect[3] * dpr))

      build_id_args = self._GetBuildIdArgs()

      # Compare images against approved images/colors.
      if page.expected_colors:
        # Use expected colors instead of hash comparison for validation.
        self._ValidateScreenshotSamplesWithSkiaGold(
            tab, page, screenshot, dpr, build_id_args)
        return
      image_name = self._UrlToImageName(page.name)
      self._UploadTestResultToSkiaGold(
        image_name, screenshot,
        tab, page,
        build_id_args=build_id_args)
    finally:
      if do_page_action:
        # Assume that page actions might have killed the GPU process.
        self._RestartBrowser('Must restart after page actions')

  def _DoPageAction(self, tab, page):
    getattr(self, '_' + page.optional_action)(tab, page)
    # Now that we've done the page's specific action, wait for it to
    # report completion.
    tab.action_runner.WaitForJavaScriptCondition(
      'domAutomationController._finished', timeout=300)

  #
  # Optional actions pages can take.
  # These are specified as methods taking the tab and the page as
  # arguments.
  #
  def _CrashGpuProcess(self, tab, page):
    # Crash the GPU process.
    #
    # This used to create a new tab and navigate it to
    # chrome://gpucrash, but there was enough unreliability
    # navigating between these tabs (one of which was created solely
    # in order to navigate to chrome://gpucrash) that the simpler
    # solution of provoking the GPU process crash from this renderer
    # process was chosen.
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')

  def _SwitchTabs(self, tab, page):
    if not tab.browser.supports_tab_control:
      self.fail('Browser must support tab control')
    dummy_tab = tab.browser.tabs.New()
    dummy_tab.Activate()
    # Wait for 2 seconds so that new tab becomes visible.
    dummy_tab.action_runner.Wait(2)
    tab.Activate()

  def _RunTestWithHighPerformanceTab(self, tab, page):
    if not self._IsDualGPUMacLaptop():
      # Short-circuit this test.
      logging.info('Short-circuiting test because not running on dual-GPU Mac '
                   'laptop')
      tab.EvaluateJavaScript('initialize()')
      tab.action_runner.WaitForJavaScriptCondition(
        'domAutomationController._readyForActions', timeout=30)
      tab.EvaluateJavaScript('runToCompletion()')
      return
    # Reset the ready state of the harness.
    tab.EvaluateJavaScript('domAutomationController._readyForActions = false')
    high_performance_tab = tab.browser.tabs.New()
    high_performance_tab.Navigate(self.UrlOfStaticFilePath(
      skia_gold_integration_test_base.GPU_RELATIVE_PATH +
      'functional_webgl_high_performance.html'),
      script_to_evaluate_on_commit=test_harness_script)
    high_performance_tab.action_runner.WaitForJavaScriptCondition(
      'domAutomationController._finished', timeout=30)
    # Wait a few seconds for the GPU switched notification to propagate
    # throughout the system.
    time.sleep(5)
    # Switch back to the main tab and quickly start its rendering, while the
    # high-power GPU is still active.
    tab.Activate()
    tab.EvaluateJavaScript('initialize()')
    tab.action_runner.WaitForJavaScriptCondition(
      'domAutomationController._readyForActions', timeout=30)
    # Close the high-performance tab.
    high_performance_tab.Close()
    # Wait for ~15 seconds for the system to switch back to the
    # integrated GPU.
    time.sleep(15)
    # Run the page to completion.
    tab.EvaluateJavaScript('runToCompletion()')

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations',
                     'pixel_expectations.txt')]

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
