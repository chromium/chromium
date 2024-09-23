# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import os
import sys
import time
from typing import Any, List, Optional, Set, Tuple
import unittest

from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test
from gpu_tests import gpu_helper
from gpu_tests.util import host_information

import gpu_path_util

from telemetry.core import exceptions

harness_script = r"""
  var domAutomationController = {};

  domAutomationController._loaded = false;
  domAutomationController._succeeded = false;
  domAutomationController._finished = false;

  domAutomationController.send = function(msg) {
    msg = msg.toLowerCase();
    if (msg == "loaded") {
      domAutomationController._loaded = true;
    } else if (msg == "success") {
      /* Don't squelch earlier failures! */
      if (!domAutomationController._finished) {
        domAutomationController._succeeded = true;
      }
      domAutomationController._finished = true;
    } else {
      /* Always record failures. */
      domAutomationController._succeeded = false;
      domAutomationController._finished = true;
    }
  }

  domAutomationController.reset = function() {
    domAutomationController._succeeded = false;
    domAutomationController._finished = false;
  }

  window.domAutomationController = domAutomationController;
  console.log("Harness injected.");
"""

feature_query_script = """
  function GetFeatureStatus(feature_name, for_hardware_gpu) {
    return getGPUInfo(for_hardware_gpu
        ? 'feature-status-for-hardware-gpu-list'
        : 'feature-status-list', feature_name);
  }
"""

vendor_id_query_script = """
  function GetActiveVendorId(for_hardware_gpu) {
    return getGPUInfo(for_hardware_gpu
        ? 'active-gpu-for-hardware'
        : 'active-gpu');
  }
"""


class ContextLostIntegrationTest(gpu_integration_test.GpuIntegrationTest):

  @classmethod
  def Name(cls) -> str:
    return 'context_lost'

  @classmethod
  def _SuiteSupportsParallelTests(cls) -> bool:
    return True

  def _GetSerialTests(self) -> Set[str]:
    serial_tests = {
        # High/low power tests don't work properly with multiple browsers
        # active.
        'ContextLost_MacWebGLMultisamplingHighPowerSwitchLosesContext',
        'ContextLost_MacWebGLMultisamplingHighPowerSwitchDoesNotCrash',
        'ContextLost_MacWebGLCopyTexSubImage2DHighPowerSwitchDoesNotCrash',
        'ContextLost_MacWebGLPreserveDBHighPowerSwitchLosesContext',
    }
    if host_information.IsMac():
      serial_tests |= {
          # crbug.com/338574390, flaky on Mac/ASan.
          'ContextLost_WebGLContextRestoredInHiddenTab',
      }
    if host_information.IsMac() or host_information.IsWindows():
      serial_tests |= {
          # Flaky timeout http://crbug.com/352077583
          'GpuNormalTermination_WebGPUNotBlocked',
      }
    return serial_tests

  @classmethod
  def GenerateBrowserArgs(cls, additional_args: List[str]) -> List[str]:
    """Adds default arguments to |additional_args|.

    See the parent class' method documentation for additional information.
    """
    default_args = super(ContextLostIntegrationTest,
                         cls).GenerateBrowserArgs(additional_args)
    default_args.extend([
        # Required to call crashGpuProcess.
        cba.ENABLE_GPU_BENCHMARKING,
        # Disable:
        #   Do you want the application "Chromium Helper.app" to accept incoming
        #   network connections?
        # dialogs on macOS. crbug.com/969559
        cba.DISABLE_DEVICE_DISCOVERY_NOTIFICATIONS,
    ])
    return default_args

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    # pylint: disable=line-too-long
    # Could not figure out how to prevent yapf from breaking the formatting
    # below.
    # yapf: disable
    tests: Tuple[Tuple[str, str], ...] = (
             ('GpuCrash_GPUProcessCrashesExactlyOncePerVisitToAboutGpuCrash',
              'gpu_process_crash.html'),
             ('GpuCrash_GPUProcessCrashesExactlyOnce_SurfaceControlDisabled',
              'gpu_process_crash.html'),
             ('ContextLost_WebGPUContextLostFromGPUProcessExit',
              'webgpu-context-lost.html?query=kill_after_notification'),
             ('ContextLost_WebGPUStressRequestDeviceAndRemoveLoop',
              'webgpu-stress-request-device-and-remove-loop.html'),
             ('ContextLost_WebGLContextLostFromGPUProcessExit',
              'webgl.html?query=kill_after_notification'),
             ('ContextLost_WebGLContextLostFromLoseContextExtension',
              'webgl.html?query=WEBGL_lose_context'),
             ('ContextLost_WebGLContextLostFromQuantity',
              'webgl.html?query=forced_quantity_loss'),
             ('ContextLost_WebGLContextLostFromSelectElement',
              'webgl_with_select_element.html'),
             ('ContextLost_WebGLContextLostInHiddenTab',
              'webgl.html?query=kill_after_notification'),
             ('ContextLost_WebGLContextLostOverlyLargeUniform',
              'webgl-overly-large-uniform.html'),
             ('ContextLost_WebGLContextRestoredInHiddenTab',
              'webgl.html?query=kill_after_notification'),
             ('ContextLost_WebGLBlockedAfterJSNavigation',
              'webgl-domain-blocking-page1.html'),
             ('ContextLost_WebGLUnblockedAfterUserInitiatedReload',
              'webgl-domain-unblocking.html'),
             ('GpuNormalTermination_OriginalWebGLNotBlocked',
              'webgl-domain-not-blocked.html'),
             ('GpuNormalTermination_NewWebGLNotBlocked',
              'webgl-domain-not-blocked.html'),
             ('ContextLost_Canvas2dGPUCrash', 'canvas_2d_gpu_crash.html'),
             ('ContextLost_WorkerWebGLRAFAfterGPUCrash',
              'worker-webgl-raf-after-gpu-crash.html'),
             ('ContextLost_OffscreenCanvasRecoveryAfterGPUCrash',
              'offscreencanvas_recovery_after_gpu_crash.html'),
             ('ContextLost_WebGL2Blocked', 'webgl2-context-blocked.html'),
             ('ContextLost_WebGL2UnpackImageHeight',
              'webgl2-unpack-image-height.html'),
             ('ContextLost_MacWebGLMultisamplingHighPowerSwitchLosesContext',
              'webgl2-multisampling-high-power-switch-loses-context.html'),
             ('ContextLost_MacWebGLMultisamplingHighPowerSwitchDoesNotCrash',
              'webgl2-multisampling-high-power-switch-does-not-crash.html'),
             ('ContextLost_MacWebGLCopyTexSubImage2DHighPowerSwitchDoesNotCrash',
              'webgl2-copytexsubimage2d-high-power-switch-does-not-crash.html'),
             ('ContextLost_MacWebGLPreserveDBHighPowerSwitchLosesContext',
              'webgl2-preserve-db-high-power-switch-loses-context.html'),
             ('GpuCrash_InfoForHardwareGpu', 'simple.html'),
             ('GpuCrash_InfoForDualHardwareGpus', 'webgl-high-perf.html'),
             ('ContextLost_WebGPUBlockedAfterJSNavigation',
              'webgpu-domain-blocking-page1.html'),
             ('ContextLost_WebGPUUnblockedAfterUserInitiatedReload',
              'webgpu-domain-unblocking.html'),
             ('GpuNormalTermination_WebGPUNotBlocked',
              'webgpu-domain-not-blocked.html'))
    # yapf: enable

    # pylint: enable=line-too-long

    for t in tests:
      yield (t[0], t[1], ['_' + t[0]])

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    test_name = args[0]
    tab = self.tab
    if not tab.browser.supports_tab_control:
      self.fail('Browser must support tab control')
    getattr(self, test_name)(test_path)

  @classmethod
  def SetUpProcess(cls) -> None:
    super(ContextLostIntegrationTest, cls).SetUpProcess()
    # Most of the tests need this, so add it to the default set of
    # command line arguments used to launch the browser, to reduce the
    # number of browser restarts between tests.
    cls.CustomizeBrowserArgs([cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS])
    cls.StartBrowser()
    cls.SetStaticServerDirs([gpu_path_util.GPU_DATA_DIR])

  # Can be changed to functools.cache on Python 3.9+.
  @functools.lru_cache(maxsize=None)
  def _GetWaitTimeout(self):
    timeout = 60
    if self._is_asan:
      timeout *= 2
    return timeout

  def _WaitForPageToFinish(self, tab, timeout: Optional[int] = None) -> bool:
    timeout = timeout or self._GetWaitTimeout()
    try:
      tab.WaitForJavaScriptCondition('window.domAutomationController._finished',
                                     timeout=timeout)
      return True
    except exceptions.TimeoutException:
      return False

  def _KillGPUProcess(self,
                      number_of_gpu_process_kills: int,
                      check_crash_count: bool,
                      timeout: Optional[int] = None) -> None:
    timeout = timeout or self._GetWaitTimeout()
    tab = self.tab
    # Doing the GPU process kill operation cooperatively -- in the
    # same page's context -- is much more stressful than restarting
    # the browser every time.
    for x in range(number_of_gpu_process_kills):
      expected_kills = x + 1

      # Reset the test's state.
      tab.EvaluateJavaScript('window.domAutomationController.reset()')

      # If we're running the GPU process crash test, we need the test
      # to have fully reset before crashing the GPU process.
      if check_crash_count:
        tab.WaitForJavaScriptCondition(
            'window.domAutomationController._finished', timeout=timeout)

      # Crash the GPU process.
      #
      # This used to create a new tab and navigate it to
      # chrome://gpucrash, but there was enough unreliability
      # navigating between these tabs (one of which was created solely
      # in order to navigate to chrome://gpucrash) that the simpler
      # solution of provoking the GPU process crash from this renderer
      # process was chosen.
      tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')

      completed = self._WaitForPageToFinish(tab, timeout=timeout)

      if check_crash_count:
        self._CheckCrashCount(tab, expected_kills)

      if not completed:
        self.fail("Test didn't complete (no context lost event?)")
      if not tab.EvaluateJavaScript(
          'window.domAutomationController._succeeded'):
        self.fail('Test failed (context not restored properly?)')

  def _CheckCrashCount(self, tab: ct.Tab, expected_kills: int) -> None:
    system_info = tab.browser.GetSystemInfo()
    if not system_info:
      self.fail('Browser must support system info')

    if not tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
      self.fail("Test failed (didn't render content properly?)")

    number_of_crashes = -1
    if expected_kills > 0:
      # To allow time for a gpucrash to complete, wait up to 20s,
      # polling repeatedly.
      start_time = time.time()
      current_time = time.time()
      while current_time - start_time < 20:
        system_info = tab.browser.GetSystemInfo()
        number_of_crashes = \
            system_info.gpu.aux_attributes[u'process_crash_count']
        if number_of_crashes >= expected_kills:
          break
        time.sleep(1)
        current_time = time.time()

    # Wait 5 more seconds and re-read process_crash_count, in
    # attempt to catch latent process crashes.
    time.sleep(5)
    system_info = tab.browser.GetSystemInfo()
    number_of_crashes = \
        system_info.gpu.aux_attributes[u'process_crash_count']

    if number_of_crashes < expected_kills:
      self.fail('Timed out waiting for a gpu process crash')
    elif number_of_crashes != expected_kills:
      self.fail('Expected %d gpu process crashes; got: %d' %
                (expected_kills, number_of_crashes))

  def _NavigateAndWaitForLoad(self, test_path: str) -> None:
    url = self.UrlOfStaticFilePath(test_path)
    tab = self.tab
    tab.Navigate(url, script_to_evaluate_on_commit=harness_script)
    tab.action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._loaded')

  def _GetWebGLFeatureStatus(self, for_hardware_gpu: bool) -> str:
    tab = self.tab.browser.tabs.New()
    tab.Navigate('chrome:gpu',
                 script_to_evaluate_on_commit=feature_query_script)
    tab.WaitForJavaScriptCondition('window.gpuPagePopulated', timeout=10)
    status = (tab.EvaluateJavaScript('GetFeatureStatus("webgl", %s)' %
                                     ('true' if for_hardware_gpu else 'false')))
    tab.Close()
    return status

  def _GetActiveVendorId(self, for_hardware_gpu: bool) -> str:
    tab = self.tab.browser.tabs.New()
    tab.Navigate('chrome:gpu',
                 script_to_evaluate_on_commit=vendor_id_query_script)
    tab.WaitForJavaScriptCondition('window.gpuPagePopulated', timeout=10)
    vid = (tab.EvaluateJavaScript('GetActiveVendorId(%s)' %
                                  ('true' if for_hardware_gpu else 'false')))
    tab.Close()
    return vid

  def _WaitForTabAndCheckCompletion(self,
                                    timeout: Optional[int] = None) -> None:
    tab = self.tab
    completed = self._WaitForPageToFinish(tab, timeout=timeout)
    if not completed:
      self.fail("Test didn't complete (no context lost / restored event?)")
    if not tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
      self.fail('Test failed (context not restored properly?)')

  # The browser test runner synthesizes methods with the exact name
  # given in GenerateGpuTests, so in order to hand-write our tests but
  # also go through the _RunGpuTest trampoline, the test needs to be
  # slightly differently named.
  def _GpuCrash_GPUProcessCrashesExactlyOncePerVisitToAboutGpuCrash(
      self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs(
        [cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS])
    self._NavigateAndWaitForLoad(test_path)
    self._KillGPUProcess(2, True)
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _GpuCrash_GPUProcessCrashesExactlyOnce_SurfaceControlDisabled(
      self, test_path: str) -> None:
    os_name = self.browser.platform.GetOSName()
    if os_name != 'android':
      logging.info('Skipping test because not running on Android')
      return

    self.RestartBrowserIfNecessaryWithArgs([
        cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS,
        '--disable-features=AndroidSurfaceControl'
    ])
    self._NavigateAndWaitForLoad(test_path)
    self._KillGPUProcess(1, True)
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_WebGLContextLostFromGPUProcessExit(self,
                                                      test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs(
        [cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS])
    self._NavigateAndWaitForLoad(test_path)
    self._KillGPUProcess(1, False)
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_WebGPUContextLostFromGPUProcessExit(self,
                                                       test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs(cba.ENABLE_WEBGPU_FOR_TESTING)
    self._NavigateAndWaitForLoad(test_path)
    self.tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.terminateGpuProcessNormally()')

    # The gpu startup sometimes takes longer on the bots.
    # Increasing the timeout for this test as it times out before completion
    self._WaitForTabAndCheckCompletion(timeout=180)

    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_WebGPUStressRequestDeviceAndRemoveLoop(self, test_path: str
                                                          ) -> None:
    self.RestartBrowserIfNecessaryWithArgs(cba.ENABLE_WEBGPU_FOR_TESTING)
    self._NavigateAndWaitForLoad(test_path)

    # Test runs for 90 seconds; wait for 120 seconds.
    self._WaitForTabAndCheckCompletion(timeout=120)

  def _ContextLost_WebGLContextLostFromLoseContextExtension(
      self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs(
        [cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS])
    url = self.UrlOfStaticFilePath(test_path)
    tab = self.tab
    tab.Navigate(url, script_to_evaluate_on_commit=harness_script)
    tab.action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._finished')

  def _ContextLost_WebGLContextLostFromQuantity(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs(
        [cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS])
    self._NavigateAndWaitForLoad(test_path)
    # Try to coerce GC to clean up any contexts not attached to the page.
    # This method seems unreliable, so the page will also attempt to
    # force GC through excessive allocations.
    self.tab.CollectGarbage()
    self._WaitForTabAndCheckCompletion()

  def _ContextLost_WebGLContextLostFromSelectElement(self,
                                                     test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs(
        [cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS])
    self._NavigateAndWaitForLoad(test_path)
    self._WaitForTabAndCheckCompletion()

  def _ContextLost_WebGLContextLostInHiddenTab(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs(
        [cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS])
    self._NavigateAndWaitForLoad(test_path)
    # Test losing a context in a hidden tab. This test passes if the tab
    # doesn't crash.
    tab = self.tab
    dummy_tab = tab.browser.tabs.New()
    tab.EvaluateJavaScript('loseContextUsingExtension()')
    tab.Activate()
    self._WaitForTabAndCheckCompletion()

  def _ContextLost_WebGLContextLostOverlyLargeUniform(self,
                                                      test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([
        cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS,
        '--enable-features=DisableArrayBufferSizeLimitsForTesting'
    ])
    self._NavigateAndWaitForLoad(test_path)
    # No reason to wait more than 10 seconds for this test to complete.
    self._WaitForTabAndCheckCompletion(timeout=10)

  def _ContextLost_WebGLContextRestoredInHiddenTab(self, test_path: str
                                                   ) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWaitForLoad(test_path)
    tab = self.tab
    if not tab.browser.supports_tab_control:
      self.fail('Browser must support tab control')
    # Make sure the tab got a WebGL context.
    if tab.EvaluateJavaScript('window.domAutomationController._finished'):
      # This means the test failed for some reason.
      if tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
        self.fail('Initial page claimed to succeed early')
      else:
        self.fail('Initial page failed to get a WebGL context')
    # Open a new tab occluding the one containing the WebGL context.
    blank_tab = tab.browser.tabs.New()
    blank_tab.Activate()
    # Wait for 2 seconds so that the new tab becomes visible.
    blank_tab.action_runner.Wait(2)
    # Kill the GPU process.
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')
    # Wait for the WebGL context to be restored and for the test to complete.
    # This will fail cooperatively if the context wasn't restored properly, and
    # will time out (and fail) if the context wasn't restored at all.
    self._WaitForTabAndCheckCompletion(timeout=10)
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_WebGLBlockedAfterJSNavigation(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWaitForLoad(test_path)
    tab = self.tab
    # Make sure the tab got a WebGL context.
    if tab.EvaluateJavaScript('window.domAutomationController._finished'):
      # This means the test failed for some reason.
      if tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
        self.fail('Initial page claimed to succeed early')
      else:
        self.fail('Initial page failed to get a WebGL context')
    # Kill the GPU process.
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')
    # Wait for the WebGL context to be restored.
    tab.WaitForJavaScriptCondition('window.restored',
                                   timeout=self._GetWaitTimeout())
    # Kill the GPU process again. This will cause WebGL to be blocked.
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')
    # The original tab will navigate to a new page. Wait for it to
    # finish running its onload handler.
    # TODO(kbr): figure out when it's OK to evaluate this JavaScript.
    # Seems racy to do it immediately after crashing the GPU process.
    tab.WaitForJavaScriptCondition('window.initFinished',
                                   timeout=self._GetWaitTimeout())
    # Make sure the page failed to get a GL context.
    if tab.EvaluateJavaScript('window.gotGL'):
      self.fail(
          'Page should have been blocked from getting a new WebGL context')
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_WebGLUnblockedAfterUserInitiatedReload(self, test_path: str
                                                          ) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWaitForLoad(test_path)
    tab = self.tab
    # Make sure the tab initially got a WebGL context.
    if not tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
      self.fail('Tab failed to get an initial WebGL context')
    # Kill the GPU process once. This won't block WebGL yet.
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')
    # Wait for the page to receive context loss and restoration events.
    tab.WaitForJavaScriptCondition('window.contextRestored',
                                   timeout=self._GetWaitTimeout())
    # Kill the GPU process again. This will cause WebGL to be blocked.
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')
    # Make sure WebGL is blocked.
    tab.WaitForJavaScriptCondition('window.contextLostReceived',
                                   timeout=self._GetWaitTimeout())
    if not tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
      self.fail('WebGL should have been blocked after a second context loss')
    # Reload the page via Telemetry / DevTools. This is treated as a
    # user-initiated navigation, so WebGL is unblocked.
    self._NavigateAndWaitForLoad(test_path)
    # Ensure WebGL is unblocked.
    if not tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
      self.fail(
          'WebGL should have been unblocked after a user-initiated navigation')
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _GpuNormalTermination_OriginalWebGLNotBlocked(self,
                                                    test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWaitForLoad(test_path)
    tab = self.tab

    tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.terminateGpuProcessNormally()')

    # The webglcontextrestored event on the original canvas should trigger and
    # report success or failure.
    self._WaitForTabAndCheckCompletion()
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _GpuNormalTermination_NewWebGLNotBlocked(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWaitForLoad(test_path)
    tab = self.tab

    tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.terminateGpuProcessNormally()')
    tab.WaitForJavaScriptCondition('window.contextLost',
                                   timeout=self._GetWaitTimeout())
    tab.EvaluateJavaScript('window.testNewWebGLContext()')

    self._WaitForTabAndCheckCompletion()
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_Canvas2dGPUCrash(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWaitForLoad(test_path)
    self._KillGPUProcess(1, False)
    self._WaitForTabAndCheckCompletion()
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_WorkerWebGLRAFAfterGPUCrash(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWaitForLoad(test_path)
    self._KillGPUProcess(1, False)
    self._WaitForTabAndCheckCompletion()
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_OffscreenCanvasRecoveryAfterGPUCrash(self,
                                                        test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWaitForLoad(test_path)
    self._KillGPUProcess(1, False)
    self._WaitForTabAndCheckCompletion()
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_WebGL2Blocked(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs(['--disable_es3_gl_context=1'])
    self._NavigateAndWaitForLoad(test_path)
    tab = self.tab
    tab.EvaluateJavaScript('runTest()')
    self._WaitForTabAndCheckCompletion()
    # Attempting to create a WebGL 2.0 context when ES 3.0 is
    # blocklisted should not cause the GPU process to crash.
    self._CheckCrashCount(tab, 0)

  def _ContextLost_WebGL2UnpackImageHeight(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([
        cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS,
        '--enable-features=DisableArrayBufferSizeLimitsForTesting'
    ])
    self._NavigateAndWaitForLoad(test_path)
    # No reason to wait more than 10 seconds for this test to complete.
    self._WaitForTabAndCheckCompletion(timeout=10)

  def _ContextLost_MacWebGLMultisamplingHighPowerSwitchLosesContext(
      self, test_path: str) -> None:
    # Verifies that switching from the low-power to the high-power GPU
    # on a dual-GPU Mac, while the user has allocated multisampled
    # renderbuffers via the WebGL 2.0 API, causes the context to be
    # lost.
    if not self.IsDualGPUMacLaptop():
      logging.info('Skipping test because not running on dual-GPU Mac laptop')
      self.skipTest('Not running on dual-GPU Mac laptop')
    # Start with a browser with clean GPU process state.
    self.RestartBrowserWithArgs([])
    # Wait a few seconds for the system to dispatch any GPU switched
    # notifications.
    time.sleep(3)
    self._NavigateAndWaitForLoad(test_path)
    if not gpu_helper.IsIntel(
        self.browser.GetSystemInfo().gpu.devices[0].vendor_id):
      self.fail('Test did not start up on low-power GPU')
    tab = self.tab
    tab.EvaluateJavaScript('runTest()')
    self._WaitForTabAndCheckCompletion()
    self._CheckCrashCount(tab, 0)

  def _ContextLost_MacWebGLMultisamplingHighPowerSwitchDoesNotCrash(
      self, test_path: str) -> None:
    # Verifies that switching from the low-power to the high-power GPU
    # on a dual-GPU Mac, while the user has allocated multisampled
    # renderbuffers via the WebGL 2.0 API, does not crash.
    if not self.IsDualGPUMacLaptop():
      logging.info('Skipping test because not running on dual-GPU Mac laptop')
      self.skipTest('Not running on dual-GPU Mac laptop')
    # Start with a browser with clean GPU process state.
    self.RestartBrowserWithArgs([])
    # Wait a few seconds for the system to dispatch any GPU switched
    # notifications.
    time.sleep(3)
    self._NavigateAndWaitForLoad(test_path)
    if not gpu_helper.IsIntel(
        self.browser.GetSystemInfo().gpu.devices[0].vendor_id):
      self.fail('Test did not start up on low-power GPU')
    tab = self.tab
    tab.EvaluateJavaScript('runTest()')
    self._WaitForTabAndCheckCompletion()
    self._CheckCrashCount(tab, 0)

  def _ContextLost_MacWebGLCopyTexSubImage2DHighPowerSwitchDoesNotCrash(
      self, test_path: str) -> None:
    # Verifies that switching from the low-power to the high-power GPU
    # on a dual-GPU Mac, while the user has allocated multisampled
    # renderbuffers via the WebGL 2.0 API, and calling
    # CopyTexSubImage2D all the while, does not crash.
    if not self.IsDualGPUMacLaptop():
      logging.info('Skipping test because not running on dual-GPU Mac laptop')
      self.skipTest('Not running on dual-GPU Mac laptop')
    # Start with a browser with clean GPU process state.
    self.RestartBrowserWithArgs([])
    # Wait a few seconds for the system to dispatch any GPU switched
    # notifications.
    time.sleep(3)
    self._NavigateAndWaitForLoad(test_path)
    if not gpu_helper.IsIntel(
        self.browser.GetSystemInfo().gpu.devices[0].vendor_id):
      self.fail('Test did not start up on low-power GPU')
    tab = self.tab
    tab.EvaluateJavaScript('runTest()')
    self._WaitForTabAndCheckCompletion()
    self._CheckCrashCount(tab, 0)

  def _ContextLost_MacWebGLPreserveDBHighPowerSwitchLosesContext(
      self, test_path: str) -> None:
    # Verifies that switching from the low-power to the high-power GPU on a
    # dual-GPU Mac, when the user specified preserveDrawingBuffer:true, causes
    # the context to be lost.
    if not self.IsDualGPUMacLaptop():
      logging.info('Skipping test because not running on dual-GPU Mac laptop')
      self.skipTest('Not running on dual-GPU Mac laptop')
    # Start with a browser with clean GPU process state.
    self.RestartBrowserWithArgs([])
    # Wait a few seconds for the system to dispatch any GPU switched
    # notifications.
    time.sleep(3)
    self._NavigateAndWaitForLoad(test_path)
    if not gpu_helper.IsIntel(
        self.browser.GetSystemInfo().gpu.devices[0].vendor_id):
      self.fail('Test did not start up on low-power GPU')
    tab = self.tab
    tab.EvaluateJavaScript('runTest()')
    self._WaitForTabAndCheckCompletion()
    self._CheckCrashCount(tab, 0)

  def _GpuCrash_InfoForHardwareGpu(self, test_path: str) -> None:
    # Ensure that info displayed in chrome:gpu for hardware gpu is correct,
    # after gpu process crashes three times and falls back to SwiftShader.
    self.RestartBrowserIfNecessaryWithArgs(
        [cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS])
    self._NavigateAndWaitForLoad(test_path)
    # Check WebGL status at browser startup.
    webgl_status = self._GetWebGLFeatureStatus(False)
    if webgl_status != 'enabled':
      self.fail('WebGL should be hardware accelerated initially, but got %s' %
                webgl_status)
    webgl_status_for_hardware_gpu = self._GetWebGLFeatureStatus(True)
    if webgl_status_for_hardware_gpu != '':
      self.fail('Feature status for hardware gpu should not be displayed '
                'initially')
    # Check WebGL status after three GPU crashes - fallback to SwiftShader.
    self._KillGPUProcess(3, True)
    webgl_status = self._GetWebGLFeatureStatus(False)
    if webgl_status != 'unavailable_software':
      self.fail('WebGL should be software only with SwiftShader, but got %s' %
                webgl_status)
    webgl_status_for_hardware_gpu = self._GetWebGLFeatureStatus(True)
    if webgl_status_for_hardware_gpu != 'enabled':
      self.fail('WebGL status for hardware gpu should be "enabled", '
                'but got %s' % webgl_status_for_hardware_gpu)
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _GpuCrash_InfoForDualHardwareGpus(self, test_path: str) -> None:
    # Ensure that info displayed in chrome:gpu for hardware gpu is from
    # the latest active GPU before the crash, after gpu process crashes three
    # times and falls back to SwiftShader.
    # Currently the test only works on Mac dual GPU bots.
    if not self.IsDualGPUMacLaptop():
      logging.info('Skipping test because not running on dual-GPU Mac laptop')
      self.skipTest('Not running on dual-GPU Mac laptop')
    self.RestartBrowserIfNecessaryWithArgs([
        cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS, '--enable-unsafe-swiftshader'
    ])
    active_vendor_id = self._GetActiveVendorId(False)
    # Load WebGL content and switch to discrete GPU.
    self._NavigateAndWaitForLoad(test_path)
    new_active_vendor_id = self._GetActiveVendorId(False)
    if not active_vendor_id or not new_active_vendor_id:
      self.fail('Fail to query the active GPU vendor id from about:gpu')
    # After three GPU crashes, check if the active vendor id for hardware GPU
    # is the new_active_vendor_id.
    self._KillGPUProcess(3, True)
    active_vendor_id_for_hardware_gpu = self._GetActiveVendorId(True)
    if not active_vendor_id_for_hardware_gpu:
      self.fail('Fail to query the active GPU vendor id for hardware GPU')
    if active_vendor_id_for_hardware_gpu != new_active_vendor_id:
      self.fail('vendor id for hw GPU should be 0x%04x, got 0x%04x' %
                (new_active_vendor_id, active_vendor_id_for_hardware_gpu))
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_WebGPUBlockedAfterJSNavigation(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs(cba.ENABLE_WEBGPU_FOR_TESTING)
    self._NavigateAndWaitForLoad(test_path)

    tab = self.tab
    if tab.EvaluateJavaScript('window.domAutomationController._finished'):
      # This means the test failed for some reason.
      if tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
        self.fail('Initial page claimed to succeed early')
      else:
        self.fail('Initial page failed to get a WebGPU device')

    # Two times: wait for the page to get a device, and kill the GPU
    # process. The first time, wait for device lost. The second time,
    # WebGPU will be blocked. The loop is unrolled for easier debugging.
    tab.WaitForJavaScriptCondition('window.gotDevice',
                                   timeout=self._GetWaitTimeout())
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')
    tab.WaitForJavaScriptCondition('window.deviceLost',
                                   timeout=self._GetWaitTimeout())
    tab.EvaluateJavaScript('proceed = true;')

    tab.WaitForJavaScriptCondition('window.gotDevice',
                                   timeout=self._GetWaitTimeout())
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')

    # The original tab will navigate to a new page. Wait for it to
    # finish running its onload handler.
    tab.WaitForJavaScriptCondition('window.initFinished',
                                   timeout=self._GetWaitTimeout())

    ## Make sure the page failed to get a WebGPU adapter.
    if tab.EvaluateJavaScript('window.gotAdapter'):
      self.fail(
          'Page should have been blocked from getting a new WebGPU device')
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_WebGPUUnblockedAfterUserInitiatedReload(
      self, test_path: str) -> None:
    """Tests that user initiated reload unblocks WebGPU crashes.

    The corresponding test page has two non-failure meaningful state:
      - Loaded:  Page was loaded and a WebGPU device was successfully acquired.
      - Success: GPU crash occurred and verified that WebGPU is blocked.
    After the 'Loaded' state, the page waits until a GPU crash occurs and does
    nothing otherwise.

    The test runs the test page twice, verifying that the first run can reach
    'Success' state while the second run only needs to reach 'Loaded' state to
    verify that a WebGPU has been unblocked.
    """
    self.RestartBrowserIfNecessaryWithArgs(cba.ENABLE_WEBGPU_FOR_TESTING)
    # Make sure the tab loaded and initially got a WebGPU device.
    self._NavigateAndWaitForLoad(test_path)
    tab = self.tab

    # Two times: wait for the page to get a device, and kill the GPU
    # process. The first time, wait for device lost. The second time,
    # WebGPU will be blocked. The loop is unrolled for easier debugging.
    tab.WaitForJavaScriptCondition('window.gotDevice',
                                   timeout=self._GetWaitTimeout())
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')
    tab.WaitForJavaScriptCondition('window.deviceLost',
                                   timeout=self._GetWaitTimeout())
    tab.EvaluateJavaScript('proceed = true;')

    tab.WaitForJavaScriptCondition('window.gotDevice',
                                   timeout=self._GetWaitTimeout())
    tab.EvaluateJavaScript('chrome.gpuBenchmarking.crashGpuProcess()')

    # Verify that WebGPU is now blocked.
    self._WaitForTabAndCheckCompletion()

    # Reload the page via Telemetry / DevTools. This is treated as a
    # user-initiated navigation, so WebGPU is unblocked, and we should be able
    # to get a new WebGPU device on load.
    self._NavigateAndWaitForLoad(test_path)
    tab.WaitForJavaScriptCondition('window.gotDevice',
                                   timeout=self._GetWaitTimeout())
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _GpuNormalTermination_WebGPUNotBlocked(self, test_path: str) -> None:
    """Tests that normal GPU process termination does not block WebGPU.
    """
    self.RestartBrowserIfNecessaryWithArgs(cba.ENABLE_WEBGPU_FOR_TESTING)
    self._NavigateAndWaitForLoad(test_path)
    tab = self.tab

    # Terminate the GPU process.
    tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.terminateGpuProcessNormally()')

    # Wait for GPU process to terminate and verify that WebGPU is NOT blocked.
    self._WaitForTabAndCheckCompletion()
    self._RestartBrowser('must restart after tests that kill the GPU process')

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'context_lost_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
