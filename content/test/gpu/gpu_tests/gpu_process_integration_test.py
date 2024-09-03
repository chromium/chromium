# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import time
from typing import Any, List, Optional, Tuple
import unittest

from devil.android.sdk import version_codes

from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test
from gpu_tests.util import host_information

import gpu_path_util

_GPU_PAGE_TIMEOUT = 30

data_path = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'content', 'test',
                         'data')

test_harness_script = r"""
  var domAutomationController = {};
  domAutomationController._finished = false;
  domAutomationController._succeeded = false;
  domAutomationController.send = function(msg) {
    domAutomationController._finished = true;
    if (msg.toLowerCase() == "success") {
      domAutomationController._succeeded = true;
    } else {
      domAutomationController._succeeded = false;
    }
  }

  window.domAutomationController = domAutomationController;

  function GetDriverBugWorkarounds() {
    return getGPUInfo('workarounds');
  };
"""


def _GetBrowserBridgeProperty(tab: ct.Tab, path: str) -> dict:
  """The GPU WebUI uses JS modules and may not have initialized the global
    browserBridge object by the time we can start injecting JavaScript. This
    ensures we don't have that problem."""
  tab.WaitForJavaScriptCondition('window.gpuPagePopulated',
                                 timeout=_GPU_PAGE_TIMEOUT)
  return tab.EvaluateJavaScript('browserBridge.' + path)


class GpuProcessIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  @classmethod
  def Name(cls) -> str:
    """The name by which this test is invoked on the command line."""
    return 'gpu_process'

  @classmethod
  def SetUpProcess(cls) -> None:
    super(GpuProcessIntegrationTest, cls).SetUpProcess()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs([data_path])

  @classmethod
  def GenerateBrowserArgs(cls, additional_args: List[str]) -> List[str]:
    """Adds default arguments to |additional_args|.

    See the parent class' method documentation for additional information.
    """
    default_args = super(GpuProcessIntegrationTest,
                         cls).GenerateBrowserArgs(additional_args)
    default_args.extend([
        cba.ENABLE_GPU_BENCHMARKING,
        # TODO(kbr): figure out why the following option seems to be
        # needed on Android for robustness.
        # https://github.com/catapult-project/catapult/issues/3122
        '--no-first-run',
        # Disable:
        #   Do you want the application "Chromium Helper.app" to accept incoming
        #   network connections?
        # dialogs on macOS. crbug.com/969559
        cba.DISABLE_DEVICE_DISCOVERY_NOTIFICATIONS,
    ])
    return default_args

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    # The browser test runner synthesizes methods with the exact name
    # given in GenerateGpuTests, so in order to hand-write our tests but
    # also go through the _RunGpuTest trampoline, the test needs to be
    # slightly differently named.

    # Also note that since functional_video.html refers to files in
    # ../media/ , the serving dir must be the common parent directory.
    tests = (
        ('GpuProcess_canvas2d', 'gpu/functional_canvas_demo.html'),
        ('GpuProcess_css3d', 'gpu/functional_3d_css.html'),
        ('GpuProcess_webgl', 'gpu/functional_webgl.html'),
        ('GpuProcess_video', 'gpu/functional_video.html'),
        ('GpuProcess_gpu_info_complete', 'gpu/functional_3d_css.html'),
        ('GpuProcess_driver_bug_workarounds_in_gpu_process', 'chrome:gpu'),
        ('GpuProcess_readback_webgl_gpu_process', 'chrome:gpu'),
        ('GpuProcess_feature_status_under_swiftshader', 'chrome:gpu'),
        ('GpuProcess_one_extra_workaround', 'chrome:gpu'),
        ('GpuProcess_disable_gpu', 'gpu/functional_webgl.html'),
        ('GpuProcess_disable_gpu_and_swiftshader', 'gpu/functional_webgl.html'),
        ('GpuProcess_disable_swiftshader', 'gpu/functional_webgl.html'),
        ('GpuProcess_disabling_workarounds_works', 'chrome:gpu'),
        ('GpuProcess_mac_webgl_backgrounded_high_performance',
         'gpu/functional_blank.html'),
        ('GpuProcess_mac_webgl_high_performance',
         'gpu/functional_webgl_high_performance.html'),
        ('GpuProcess_mac_webgl_low_power',
         'gpu/functional_webgl_low_power.html'),
        ('GpuProcess_mac_webgl_terminated_high_performance',
         'gpu/functional_blank.html'),
        ('GpuProcess_swiftshader_for_webgl', 'gpu/functional_webgl.html'),
        ('GpuProcess_no_swiftshader_for_webgl_without_flags',
         'gpu/functional_webgl.html'),
        ('GpuProcess_webgl_disabled_extension',
         'gpu/functional_webgl_disabled_extension.html'),
        ('GpuProcess_webgpu_iframe_removed', 'gpu/webgpu-iframe-removed.html'),
        ('GpuProcess_visibility', 'about:blank'),
    )

    for t in tests:
      yield (t[0], t[1], ['_' + t[0]])

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    test_name = args[0]
    getattr(self, test_name)(test_path)

  ######################################
  # Helper functions for the tests below

  def _Navigate(self, test_path: str) -> None:
    url = self.UrlOfStaticFilePath(test_path)
    # It's crucial to use the action_runner, rather than the tab's
    # Navigate method directly. It waits for the document ready state
    # to become interactive or better, avoiding critical race
    # conditions.
    self.tab.action_runner.Navigate(
        url, script_to_evaluate_on_commit=test_harness_script)

  def _WaitForTestCompletion(self, tab: ct.Tab) -> None:
    tab.action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._finished', timeout=10)
    if not tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
      self.fail('Test reported that it failed')

  def _NavigateAndWait(self, test_path: str) -> None:
    self._Navigate(test_path)
    self._WaitForTestCompletion(self.tab)

  def _VerifyGpuProcessPresent(self) -> None:
    tab = self.tab
    if not tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuChannel()'):
      self.fail('No GPU channel detected')

  def _ValidateDriverBugWorkaroundsImpl(self, is_expected: bool,
                                        workaround_name: str) -> None:
    tab = self.tab
    gpu_driver_bug_workarounds = tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.getGpuDriverBugWorkarounds()')

    is_present = workaround_name in gpu_driver_bug_workarounds
    failure = False
    if is_expected and not is_present:
      failure = True
      error_message = 'is missing'
    elif not is_expected and is_present:
      failure = True
      error_message = 'is not expected'

    if failure:
      print('Test failed. Printing page contents:')
      print(tab.EvaluateJavaScript('document.body.innerHTML'))
      self.fail('%s %s workarounds: %s' % (workaround_name, error_message,
                                           gpu_driver_bug_workarounds))

  def _ValidateDriverBugWorkarounds(self, expected_workaround: Optional[str],
                                    unexpected_workaround: Optional[str]
                                    ) -> None:
    if not expected_workaround and not unexpected_workaround:
      return
    if expected_workaround:
      self._ValidateDriverBugWorkaroundsImpl(True, expected_workaround)
    if unexpected_workaround:
      self._ValidateDriverBugWorkaroundsImpl(False, unexpected_workaround)

  @staticmethod
  def _Filterer(workaround: str) -> bool:
    # Filter all entries starting with "disabled_extension_" and
    # "disabled_webgl_extension_", as these are synthetic entries
    # added to make it easier to read the logs.
    banned_prefixes = ['disabled_extension_', 'disabled_webgl_extension_']
    for p in banned_prefixes:
      if workaround.startswith(p):
        return False
    return True

  def _CompareAndCaptureDriverBugWorkarounds(
      self) -> Tuple[List[str], Optional[str]]:
    tab = self.tab
    if not tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuProcess()'):
      self.fail('No GPU process detected')

    if not tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuChannel()'):
      self.fail('No GPU channel detected')

    browser_list = [
        x for x in tab.EvaluateJavaScript('GetDriverBugWorkarounds()')
        if self._Filterer(x)
    ]
    gpu_list = [
        x for x in tab.EvaluateJavaScript(
            'chrome.gpuBenchmarking.getGpuDriverBugWorkarounds()')
        if self._Filterer(x)
    ]

    diff = set(browser_list).symmetric_difference(set(gpu_list))
    if len(diff) > 0:
      self.fail('Browser and GPU process list of driver bug'
                'workarounds are not equal: %s != %s, diff: %s' %
                (browser_list, gpu_list, list(diff)))

    basic_infos = _GetBrowserBridgeProperty(tab, 'gpuInfo.basicInfo')
    disabled_gl_extensions = None
    for info in basic_infos:
      if info['description'].startswith('Disabled Extensions'):
        disabled_gl_extensions = info['value']
        break

    return gpu_list, disabled_gl_extensions

  ######################################
  # The actual tests

  def _GpuProcess_canvas2d(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_css3d(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_webgl(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_video(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_gpu_info_complete(self, test_path: str) -> None:
    # Regression test for crbug.com/454906
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    tab = self.tab
    system_info = tab.browser.GetSystemInfo()
    if not system_info:
      self.fail('Browser must support system info')
    if not system_info.gpu:
      self.fail('Target machine must have a GPU')
    if not system_info.gpu.aux_attributes:
      self.fail('Browser must support GPU aux attributes')
    if not 'gl_renderer' in system_info.gpu.aux_attributes:
      self.fail('Browser must have gl_renderer in aux attribs')
    if (not host_information.IsMac()
        and len(system_info.gpu.aux_attributes['gl_renderer']) <= 0):
      # On MacOSX we don't create a context to collect GL strings.1
      self.fail('Must have a non-empty gl_renderer string')

  def _GpuProcess_driver_bug_workarounds_in_gpu_process(self,
                                                        test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs(
        ['--use_gpu_driver_workaround_for_testing'])
    self._Navigate(test_path)
    self.tab.WaitForJavaScriptCondition('window.gpuPagePopulated',
                                        timeout=_GPU_PAGE_TIMEOUT)
    self._ValidateDriverBugWorkarounds('use_gpu_driver_workaround_for_testing',
                                       None)

  def _GpuProcess_readback_webgl_gpu_process(self, test_path: str) -> None:
    # Hit test group 1 with entry 152 from kSoftwareRenderingListEntries.
    self.RestartBrowserIfNecessaryWithArgs([
        '--gpu-blocklist-test-group=1',
        cba.DISABLE_GPU_COMPOSITING,
    ])
    self._Navigate(test_path)
    feature_status_list = _GetBrowserBridgeProperty(
        self.tab, 'gpuInfo.featureStatus.featureStatus')
    result = True
    for name, status in feature_status_list.items():
      if name == 'webgl':
        result = result and status == 'enabled_readback'
      elif name == 'webgl2':
        result = result and status == 'unavailable_off'
      else:
        pass
    if not result:
      self.fail('WebGL readback setup failed: %s' % feature_status_list)

  def _GpuProcess_feature_status_under_swiftshader(self,
                                                   test_path: str) -> None:
    # Hit test group 2 with entry 153 from kSoftwareRenderingListEntries.
    self.RestartBrowserIfNecessaryWithArgs(
        ['--gpu-blocklist-test-group=2', '--enable-unsafe-swiftshader'])
    self._Navigate(test_path)
    feature_status_list = _GetBrowserBridgeProperty(
        self.tab, 'gpuInfo.featureStatus.featureStatus')
    for name, status in feature_status_list.items():
      if name == 'webgl' and status != 'unavailable_software':
        self.fail('WebGL status for SwiftShader failed: %s' % status)
      elif name == '2d_canvas' and status != 'unavailable_software':
        self.fail('2D Canvas status for SwiftShader failed: %s' % status)

    # On Linux we relaunch GPU process to fallback to SwiftShader, therefore
    # featureStatusForHardwareGpu isn't available. So finish early if we're on
    # Linux.
    if host_information.IsLinux():
      return

    feature_status_for_hardware_gpu_list = _GetBrowserBridgeProperty(
        self.tab, 'gpuInfo.featureStatusForHardwareGpu.featureStatus')
    for name, status in feature_status_for_hardware_gpu_list.items():
      if name == 'webgl' and status != 'unavailable_off':
        self.fail('WebGL status for hardware GPU failed: %s' % status)
      elif name == '2d_canvas' and status != 'enabled':
        self.fail('2D Canvas status for hardware GPU failed: %s' % status)

  def _GpuProcess_one_extra_workaround(self, test_path: str) -> None:
    # Start this test by launching the browser with no command line
    # arguments.
    self.RestartBrowserIfNecessaryWithArgs([])
    self._Navigate(test_path)
    self._VerifyGpuProcessPresent()
    self.tab.WaitForJavaScriptCondition('window.gpuPagePopulated',
                                        timeout=_GPU_PAGE_TIMEOUT)
    recorded_workarounds, recorded_disabled_gl_extensions = (
        self._CompareAndCaptureDriverBugWorkarounds())
    # Relaunch the browser enabling test group 1 with entry 215, where
    # use_gpu_driver_workaround_for_testing is enabled.
    additional_args = ['--gpu-driver-bug-list-test-group=1']
    # Add the testing workaround to the recorded workarounds.
    recorded_workarounds.append('use_gpu_driver_workaround_for_testing')
    additional_args.append('--disable-gl-extensions=' +
                           recorded_disabled_gl_extensions)
    self.RestartBrowserIfNecessaryWithArgs(additional_args)
    self._Navigate(test_path)
    self._VerifyGpuProcessPresent()
    self.tab.WaitForJavaScriptCondition('window.gpuPagePopulated',
                                        timeout=_GPU_PAGE_TIMEOUT)
    new_workarounds, new_disabled_gl_extensions = (
        self._CompareAndCaptureDriverBugWorkarounds())
    diff = set(recorded_workarounds).symmetric_difference(new_workarounds)
    tab = self.tab
    if len(diff) > 0:
      print('Test failed. Printing page contents:')
      print(tab.EvaluateJavaScript('document.body.innerHTML'))
      self.fail('GPU process and expected list of driver bug '
                'workarounds are not equal: %s != %s, diff: %s' %
                (recorded_workarounds, new_workarounds, list(diff)))
    if recorded_disabled_gl_extensions != new_disabled_gl_extensions:
      print('Test failed. Printing page contents:')
      print(tab.EvaluateJavaScript('document.body.innerHTML'))
      self.fail('The expected disabled gl extensions are '
                'incorrect: %s != %s:' % (recorded_disabled_gl_extensions,
                                          new_disabled_gl_extensions))

  def _GpuProcess_disable_gpu(self, test_path: str) -> None:
    # This test loads functional_webgl.html so that there is a
    # deliberate attempt to use an API which would start the GPU
    # process.
    self.RestartBrowserIfNecessaryWithArgs([cba.DISABLE_GPU])
    self._NavigateAndWait(test_path)
    has_gpu_process = self.tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.hasGpuProcess()')
    if not has_gpu_process:
      self.fail('GPU process not detected')

  def _GpuProcess_visibility(self, test_path: str) -> None:
    os_name = self.browser.platform.GetOSName()
    if os_name != 'android':
      logging.info('Skipping test because not running on Android')
      return

    # pylint: disable=protected-access
    sdk_version = \
        self.browser.platform._platform_backend.device.build_version_sdk
    # pylint: enable=protected-access
    if sdk_version < version_codes.PIE:
      logging.info('Skipping test because not running on Android P+')
      return

    has_gpu_process = self.tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.hasGpuProcess()')
    if not has_gpu_process:
      logging.info('Skipping test because no out-of-process GPU service')
      return

    self.RestartBrowserIfNecessaryWithArgs([])
    self._Navigate(test_path)
    system_info = self.browser.GetSystemInfo()
    callback_count = system_info.gpu.aux_attributes[
        'visibility_callback_call_count']
    # initial callback count should be 1 since the app became visible
    if callback_count != 1:
      self.fail('Visibility callback call count expected 1, got %d' %
                callback_count)

    self.browser.platform.android_action_runner.TurnScreenOff()
    self.tab.WaitForJavaScriptCondition('document.visibilityState == "hidden"',
                                        timeout=_GPU_PAGE_TIMEOUT)
    system_info = self.browser.GetSystemInfo()
    callback_count = system_info.gpu.aux_attributes[
        'visibility_callback_call_count']
    if callback_count != 2:
      self.fail('Visibility callback call count expected 2, got %d' %
                callback_count)

    self.browser.platform.android_action_runner.TurnScreenOn()
    self.tab.WaitForJavaScriptCondition('document.visibilityState == "visible"',
                                        timeout=_GPU_PAGE_TIMEOUT)
    system_info = self.browser.GetSystemInfo()
    callback_count = system_info.gpu.aux_attributes[
        'visibility_callback_call_count']
    if callback_count != 3:
      self.fail('Visibility callback call count expected 3, got %d' %
                callback_count)

  def _GpuProcess_disable_gpu_and_swiftshader(self, test_path: str) -> None:
    # Disable SwiftShader, GPU process should launch for display compositing.
    self.RestartBrowserIfNecessaryWithArgs(
        [cba.DISABLE_GPU, cba.DISABLE_SOFTWARE_RASTERIZER])
    self._NavigateAndWait(test_path)
    has_gpu_process = self.tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.hasGpuProcess()')
    if not has_gpu_process:
      self.fail('GPU process not detected')

  def _GpuProcess_disable_swiftshader(self, test_path: str) -> None:
    # Disable SwiftShader, GPU process should be able to launch.
    self.RestartBrowserIfNecessaryWithArgs([cba.DISABLE_SOFTWARE_RASTERIZER])
    self._NavigateAndWait(test_path)
    has_gpu_process = self.tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.hasGpuProcess()')
    if not has_gpu_process:
      self.fail('GPU process not detected')

  def _GpuProcess_disabling_workarounds_works(self, test_path):
    # Hit exception from id 215 from kGpuDriverBugListEntries.
    self.RestartBrowserIfNecessaryWithArgs([
        '--gpu-driver-bug-list-test-group=1',
        '--use_gpu_driver_workaround_for_testing=0'
    ])
    self._Navigate(test_path)
    self.tab.WaitForJavaScriptCondition('window.gpuPagePopulated',
                                        timeout=_GPU_PAGE_TIMEOUT)
    workarounds, _ = (self._CompareAndCaptureDriverBugWorkarounds())
    if 'use_gpu_driver_workaround_for_testing' in workarounds:
      self.fail('use_gpu_driver_workaround_for_testing erroneously present')

  def _GpuProcess_swiftshader_for_webgl(self, test_path: str) -> None:
    # This test loads functional_webgl.html so that there is a deliberate
    # attempt to use an API which would start the GPU process.
    args_list = (
        # Triggering test_group 2 where WebGL is blocklisted.
        ['--gpu-blocklist-test-group=2', '--enable-unsafe-swiftshader'],
        # Explicitly disable GPU access.
        [cba.DISABLE_GPU, '--enable-unsafe-swiftshader'])
    for args in args_list:
      self.RestartBrowserIfNecessaryWithArgs(args)
      self._NavigateAndWait(test_path)
      # Validate the WebGL unmasked renderer string.
      renderer = self.tab.EvaluateJavaScript('gl_renderer')
      if not renderer:
        self.fail('getParameter(UNMASKED_RENDERER_WEBGL) was null')
      if 'SwiftShader' not in renderer:
        self.fail('Expected SwiftShader renderer; instead got ' + renderer)
      # Validate GPU info.
      system_info = self.browser.GetSystemInfo()
      if not system_info:
        self.fail("Browser doesn't support GetSystemInfo")
      gpu = system_info.gpu
      if not gpu:
        self.fail('Target machine must have a GPU')
      if not gpu.aux_attributes:
        self.fail('Browser must support GPU aux attributes')
      if 'SwiftShader' not in gpu.aux_attributes['gl_renderer']:
        self.fail('Expected "SwiftShader" in GPU info GL renderer string')
      if 'Google' not in gpu.aux_attributes['gl_vendor']:
        self.fail('Expected "Google" in GPU info GL vendor string')
      device = gpu.devices[0]
      if not device:
        self.fail("System Info doesn't have a device")
      # Validate extensions.
      ext_list = [
          'ANGLE_instanced_arrays',
          'EXT_blend_minmax',
          'EXT_texture_filter_anisotropic',
          'OES_element_index_uint',
          'OES_standard_derivatives',
          'OES_texture_float',
          'OES_texture_float_linear',
          'OES_texture_half_float',
          'OES_texture_half_float_linear',
          'OES_vertex_array_object',
          'WEBGL_compressed_texture_etc1',
          'WEBGL_debug_renderer_info',
          'WEBGL_depth_texture',
          'WEBGL_draw_buffers',
          'WEBGL_lose_context',
      ]
      tab = self.tab
      for ext in ext_list:
        if tab.EvaluateJavaScript('!gl_context.getExtension("' + ext + '")'):
          self.fail('Expected %s support' % ext)

  def _GpuProcess_no_swiftshader_for_webgl_without_flags(
      self, test_path: str) -> None:
    # This test loads functional_webgl.html with GPU disabled and verifies that
    # SwiftShader is not available without the --enable-unsafe-swiftshader flag
    # or AllowSwiftShaderFallback killswitch.
    # Because AllowSwiftShaderFallback is currently enabled by default, disable
    # it to test the upcoming default behavior
    disable_hardware_webgl_args_list = [
        # Triggering test_group 2 where WebGL is blocklisted.
        ['--gpu-blocklist-test-group=2'],
        # Explicitly disable GPU access.
        [cba.DISABLE_GPU],
    ]
    disable_swiftshader_fallback_feature = [
        '--disable-features=AllowSwiftShaderFallback'
    ]
    allow_swiftshader_args_list = [
        ['--enable-unsafe-swiftshader'],
        ['--use-gl=angle', '--use-angle=swiftshader'],
        ['--enable-features=AllowSwiftShaderFallback']
    ]
    for disable_hardware_webgl_args in disable_hardware_webgl_args_list:
      self.RestartBrowserIfNecessaryWithArgs(
          disable_hardware_webgl_args + disable_swiftshader_fallback_feature)
      self._NavigateAndWait(test_path)

      renderer = self.tab.EvaluateJavaScript('gl_renderer')
      if renderer:
        self.fail('Expected no WebGL renderer; instead got ' + renderer)

      for allow_swiftshader_args in allow_swiftshader_args_list:
        self.RestartBrowserIfNecessaryWithArgs(disable_hardware_webgl_args +
                                               allow_swiftshader_args)
        self._NavigateAndWait(test_path)

        # Validate the WebGL unmasked renderer string.
        renderer = self.tab.EvaluateJavaScript('gl_renderer')
        if not renderer:
          self.fail('getParameter(UNMASKED_RENDERER_WEBGL) was null')
        if 'SwiftShader' not in renderer:
          self.fail('Expected SwiftShader renderer; instead got ' + renderer)

  def _GpuProcess_webgl_disabled_extension(self, test_path: str) -> None:
    # Hit exception from id 257 from kGpuDriverBugListEntries.
    self.RestartBrowserIfNecessaryWithArgs([
        '--gpu-driver-bug-list-test-group=2',
    ])
    self._NavigateAndWait(test_path)

  def _GpuProcess_mac_webgl_low_power(self, test_path: str) -> None:
    # Ensures that low-power WebGL content stays on the low-power GPU.
    if not self.IsDualGPUMacLaptop():
      logging.info('Skipping test because not running on dual-GPU Mac laptop')
      return
    # Start with a clean browser instance to ensure the GPU process is in a
    # clean state.
    self.RestartBrowserWithArgs([])
    # Wait a few seconds for the system to dispatch any GPU switched
    # notifications.
    time.sleep(3)
    self._NavigateAndWait(test_path)
    # Sleep for several seconds to ensure that any GPU switch is detected.
    time.sleep(6)
    if not self._IsIntelGPUActive():
      self.fail(
          'Low-power WebGL context incorrectly activated the high-performance '
          'GPU')

  def _GpuProcess_mac_webgl_high_performance(self, test_path: str) -> None:
    # Ensures that high-performance WebGL content activates the high-performance
    # GPU.
    if not self.IsDualGPUMacLaptop():
      logging.info('Skipping test because not running on dual-GPU Mac laptop')
      return
    # Start with a clean browser instance to ensure the GPU process is in a
    # clean state.
    self.RestartBrowserWithArgs([])
    # Wait a few seconds for the system to dispatch any GPU switched
    # notifications.
    time.sleep(3)
    self._NavigateAndWait(test_path)
    # Sleep for several seconds to ensure that any GPU switch is detected.
    time.sleep(6)
    if self._IsIntelGPUActive():
      self.fail('High-performance WebGL context did not activate the '
                'high-performance GPU')

  def _GpuProcess_mac_webgl_backgrounded_high_performance(self, test_path: str
                                                          ) -> None:
    # Ensures that high-performance WebGL content in a background tab releases
    # the hold on the discrete GPU after 10 seconds.
    if not self.IsDualGPUMacLaptop():
      logging.info('Skipping test because not running on dual-GPU Mac laptop')
      return
    # Start with a clean browser instance to ensure the GPU process is in a
    # clean state.
    self.RestartBrowserWithArgs([])
    # Wait a few seconds for the system to dispatch any GPU switched
    # notifications.
    time.sleep(3)
    self._NavigateAndWait(test_path)
    blank_tab = self.tab
    # Create a new tab and navigate it to the high-power WebGL test.
    webgl_tab = self.browser.tabs.New()
    webgl_tab.Activate()
    webgl_url = self.UrlOfStaticFilePath(
        'gpu/functional_webgl_high_performance.html')
    webgl_tab.action_runner.Navigate(
        webgl_url, script_to_evaluate_on_commit=test_harness_script)
    self._WaitForTestCompletion(webgl_tab)
    # Verify that the high-performance GPU is active.
    if self._IsIntelGPUActive():
      self.fail('High-performance WebGL context did not activate the '
                'high-performance GPU')
    # Now activate the original tab.
    blank_tab.Activate()
    # Sleep for >10 seconds in order to wait for the hold on the
    # high-performance GPU to be released.
    time.sleep(15)
    if not self._IsIntelGPUActive():
      self.fail(
          'Backgrounded high-performance WebGL context did not release the '
          'hold on the high-performance GPU')

  def _GpuProcess_mac_webgl_terminated_high_performance(self,
                                                        test_path: str) -> None:
    # Ensures that high-performance WebGL content in a background tab releases
    # the hold on the discrete GPU after 10 seconds.
    if not self.IsDualGPUMacLaptop():
      logging.info('Skipping test because not running on dual-GPU Mac laptop')
      return
    # Start with a clean browser instance to ensure the GPU process is in a
    # clean state.
    self.RestartBrowserWithArgs([])
    # Wait a few seconds for the system to dispatch any GPU switched
    # notifications.
    time.sleep(3)
    self._NavigateAndWait(test_path)
    # Create a new tab and navigate it to the high-power WebGL test.
    webgl_tab = self.browser.tabs.New()
    webgl_tab.Activate()
    webgl_url = self.UrlOfStaticFilePath(
        'gpu/functional_webgl_high_performance.html')
    webgl_tab.action_runner.Navigate(
        webgl_url, script_to_evaluate_on_commit=test_harness_script)
    self._WaitForTestCompletion(webgl_tab)
    # Verify that the high-performance GPU is active.
    if self._IsIntelGPUActive():
      self.fail('High-performance WebGL context did not activate the '
                'high-performance GPU')
    # Close the high-performance WebGL tab.
    webgl_tab.Close()
    # Sleep for >10 seconds in order to wait for the hold on the
    # high-performance GPU to be released.
    time.sleep(15)
    if not self._IsIntelGPUActive():
      self.fail(
          'Backgrounded high-performance WebGL context did not release the '
          'hold on the high-performance GPU')

  def _GpuProcess_webgpu_iframe_removed(self, test_path: str) -> None:
    self.RestartBrowserIfNecessaryWithArgs(cba.ENABLE_WEBGPU_FOR_TESTING)
    self._NavigateAndWait(test_path)

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'gpu_process_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
