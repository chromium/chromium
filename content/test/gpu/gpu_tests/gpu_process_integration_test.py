# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from gpu_tests import gpu_integration_test
from gpu_tests import gpu_process_expectations
from gpu_tests import path_util

data_path = os.path.join(
    path_util.GetChromiumSrcDir(), 'content', 'test', 'data')

test_harness_script = r"""
  var domAutomationController = {};
  domAutomationController._finished = false;
  domAutomationController._succeeded = false;
  domAutomationController.send = function(msg) {
    domAutomationController._finished = true;
    if (msg.toLowerCase() == "finished") {
      domAutomationController._succeeded = true;
    } else {
      domAutomationController._succeeded = false;
    }
  }

  window.domAutomationController = domAutomationController;

  function GetDriverBugWorkarounds() {
    var query_result = document.querySelector('.workarounds-list');
    var browser_list = []
    for (var i=0; i < query_result.childElementCount; i++)
      browser_list.push(query_result.children[i].textContent);
    return browser_list;
  };
"""

class GpuProcessIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  @classmethod
  def Name(cls):
    """The name by which this test is invoked on the command line."""
    return 'gpu_process'

  @classmethod
  def SetUpProcess(cls):
    super(GpuProcessIntegrationTest, cls).SetUpProcess()
    cls.CustomizeBrowserArgs(cls._AddDefaultArgs([]))
    cls.StartBrowser()
    cls.SetStaticServerDirs([data_path])

  @staticmethod
  def _AddDefaultArgs(browser_args):
    # All tests receive the following options.
    return [
      '--enable-gpu-benchmarking',
      # TODO(kbr): figure out why the following option seems to be
      # needed on Android for robustness.
      # https://github.com/catapult-project/catapult/issues/3122
      '--no-first-run'] + browser_args

  @classmethod
  def _CreateExpectations(cls):
    return gpu_process_expectations.GpuProcessExpectations()

  @classmethod
  def GenerateGpuTests(cls, options):
    # The browser test runner synthesizes methods with the exact name
    # given in GenerateGpuTests, so in order to hand-write our tests but
    # also go through the _RunGpuTest trampoline, the test needs to be
    # slightly differently named.

    # Also note that since functional_video.html refers to files in
    # ../media/ , the serving dir must be the common parent directory.
    tests = (('GpuProcess_canvas2d', 'gpu/functional_canvas_demo.html'),
             ('GpuProcess_css3d', 'gpu/functional_3d_css.html'),
             ('GpuProcess_webgl', 'gpu/functional_webgl.html'),
             ('GpuProcess_video', 'gpu/functional_video.html'),
             ('GpuProcess_gpu_info_complete', 'gpu/functional_3d_css.html'),
             ('GpuProcess_driver_bug_workarounds_in_gpu_process', 'chrome:gpu'),
             ('GpuProcess_readback_webgl_gpu_process', 'chrome:gpu'),
             ('GpuProcess_feature_status_under_swiftshader', 'chrome:gpu'),
             ('GpuProcess_one_extra_workaround', 'chrome:gpu'),
             ('GpuProcess_disable_gpu', 'gpu/functional_webgl.html'),
             ('GpuProcess_disable_gpu_and_swiftshader',
              'gpu/functional_webgl.html'),
             ('GpuProcess_disable_swiftshader', 'gpu/functional_webgl.html'),
             ('GpuProcess_disabling_workarounds_works', 'chrome:gpu'),
             ('GpuProcess_swiftshader_for_webgl', 'gpu/functional_webgl.html'),
             ('GpuProcess_webgl_disabled_extension',
              'gpu/functional_webgl_disabled_extension.html'))

    # The earlier has_transparent_visuals_gpu_process and
    # no_transparent_visuals_gpu_process tests became no-ops in
    # http://crrev.com/2347383002 and were deleted.

    for t in tests:
      yield (t[0], t[1], ('_' + t[0]))

  def RunActualGpuTest(self, test_path, *args):
    test_name = args[0]
    getattr(self, test_name)(test_path)

  ######################################
  # Helper functions for the tests below

  def _Navigate(self, test_path):
    url = self.UrlOfStaticFilePath(test_path)
    # It's crucial to use the action_runner, rather than the tab's
    # Navigate method directly. It waits for the document ready state
    # to become interactive or better, avoiding critical race
    # conditions.
    self.tab.action_runner.Navigate(
      url, script_to_evaluate_on_commit=test_harness_script)

  def _NavigateAndWait(self, test_path):
    self._Navigate(test_path)
    tab = self.tab
    tab.action_runner.WaitForJavaScriptCondition(
      'window.domAutomationController._finished', timeout=10)
    if not tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
      self.fail('Test reported that it failed')

  def _VerifyGpuProcessPresent(self):
    tab = self.tab
    if not tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuChannel()'):
      self.fail('No GPU channel detected')

  def _ValidateDriverBugWorkaroundsImpl(self, is_expected, workaround_name):
    tab = self.tab
    gpu_driver_bug_workarounds = tab.EvaluateJavaScript(
      'chrome.gpuBenchmarking.getGpuDriverBugWorkarounds()')

    is_present = workaround_name in gpu_driver_bug_workarounds
    failure = False
    if is_expected and not is_present:
      failure = True
      error_message = "is missing"
    elif not is_expected and is_present:
      failure = True
      error_message = "is not expected"

    if failure:
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      self.fail('%s %s workarounds: %s'
                % (workaround_name, error_message, gpu_driver_bug_workarounds))

  def _ValidateDriverBugWorkarounds(self, expected_workaround,
                                    unexpected_workaround):
    if not expected_workaround and not unexpected_workaround:
      return
    if expected_workaround:
      self._ValidateDriverBugWorkaroundsImpl(True, expected_workaround)
    if unexpected_workaround:
      self._ValidateDriverBugWorkaroundsImpl(False, unexpected_workaround)

  # This can only be called from one of the tests, i.e., after the
  # browser's been brought up once.
  def _RunningOnAndroid(self):
    options = self.__class__._original_finder_options.browser_options
    return options.browser_type.startswith('android')

  def _SupportsSwiftShader(self):
    # Currently we enable SwiftShader on Windows, Linux and MacOS.
    return (sys.platform in ('cygwin', 'win32', 'darwin') or
            (sys.platform.startswith('linux') and
             not self._RunningOnAndroid()))

  def _AlwaysRunsGpuProcess(self):
    # With VizDisplayCompositor enabled we always run a GPU process for the
    # display compositor, except on Windows where the display compositor will
    # run in the browser process if GPU and SwiftShader are disabled.
    system_info = self.browser.GetSystemInfo()
    if not system_info:
      self.fail("Browser doesn't support GetSystemInfo")

    viz_status = system_info.gpu.feature_status.get('viz_display_compositor')
    return viz_status == 'enabled_on' and sys.platform != 'win32'

  @staticmethod
  def _Filterer(workaround):
    # Filter all entries starting with "disabled_extension_" and
    # "disabled_webgl_extension_", as these are synthetic entries
    # added to make it easier to read the logs.
    banned_prefixes = ['disabled_extension_', 'disabled_webgl_extension_']
    for p in banned_prefixes:
      if workaround.startswith(p):
        return False
    return True

  def _CompareAndCaptureDriverBugWorkarounds(self):
    tab = self.tab
    if not tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuProcess()'):
      self.fail('No GPU process detected')

    if not tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuChannel()'):
      self.fail('No GPU channel detected')

    browser_list = [
      x for x in tab.EvaluateJavaScript('GetDriverBugWorkarounds()')
      if self._Filterer(x)]
    gpu_list = [
      x for x in tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.getGpuDriverBugWorkarounds()')
      if self._Filterer(x)]

    diff = set(browser_list).symmetric_difference(set(gpu_list))
    if len(diff) > 0:
      self.fail('Browser and GPU process list of driver bug'
                'workarounds are not equal: %s != %s, diff: %s' %
                (browser_list, gpu_list, list(diff)))

    basic_infos = tab.EvaluateJavaScript('browserBridge.gpuInfo.basicInfo')
    disabled_gl_extensions = None
    for info in basic_infos:
      if info['description'].startswith('Disabled Extensions'):
        disabled_gl_extensions = info['value']
        break

    return gpu_list, disabled_gl_extensions

  ######################################
  # The actual tests

  def _GpuProcess_canvas2d(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_css3d(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_webgl(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_video(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_gpu_info_complete(self, test_path):
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
    if (sys.platform != 'darwin' and
        len(system_info.gpu.aux_attributes['gl_renderer']) <= 0):
      # On MacOSX we don't create a context to collect GL strings.1
      self.fail('Must have a non-empty gl_renderer string')

  def _GpuProcess_driver_bug_workarounds_in_gpu_process(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([
      '--use_gpu_driver_workaround_for_testing'])
    self._Navigate(test_path)
    self._ValidateDriverBugWorkarounds(
      'use_gpu_driver_workaround_for_testing', None)

  def _GpuProcess_readback_webgl_gpu_process(self, test_path):
    # Hit test group 1 with entry 152 from kSoftwareRenderingListEntries.
    self.RestartBrowserIfNecessaryWithArgs([
      '--gpu-blacklist-test-group=1'])
    self._Navigate(test_path)
    feature_status_list = self.tab.EvaluateJavaScript(
        'browserBridge.gpuInfo.featureStatus.featureStatus')
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

  def _GpuProcess_feature_status_under_swiftshader(self, test_path):
    if not self._SupportsSwiftShader():
      return
    # Hit test group 2 with entry 153 from kSoftwareRenderingListEntries.
    self.RestartBrowserIfNecessaryWithArgs([
      '--gpu-blacklist-test-group=2'])
    self._Navigate(test_path)
    feature_status_list = self.tab.EvaluateJavaScript(
        'browserBridge.gpuInfo.featureStatus.featureStatus')
    for name, status in feature_status_list.items():
      if name == 'webgl':
        if status != 'unavailable_software':
          self.fail('WebGL status for SwiftShader failed: %s' % status)
          return
      elif name == '2d_canvas':
        if status != 'unavailable_software':
          self.fail('2D Canvas status for SwiftShader failed: %s' % status)
          return
      else:
        pass
    feature_status_for_hardware_gpu_list = self.tab.EvaluateJavaScript(
        'browserBridge.gpuInfo.featureStatusForHardwareGpu.featureStatus')
    for name, status in feature_status_for_hardware_gpu_list.items():
      if name == 'webgl':
        if status != 'unavailable_off':
          self.fail('WebGL status for hardware GPU failed: %s' % status)
          return
      elif name == '2d_canvas':
        if status != 'enabled':
          self.fail('2D Canvas status for hardware GPU failed: %s' % status)
          return
      else:
        pass

  def _GpuProcess_one_extra_workaround(self, test_path):
    # Start this test by launching the browser with no command line
    # arguments.
    self.RestartBrowserIfNecessaryWithArgs([])
    self._Navigate(test_path)
    self._VerifyGpuProcessPresent()
    recorded_workarounds, recorded_disabled_gl_extensions = (
      self._CompareAndCaptureDriverBugWorkarounds())
    # Relaunch the browser enabling test group 1 with entry 215, where
    # use_gpu_driver_workaround_for_testing is enabled.
    browser_args = ['--gpu-driver-bug-list-test-group=1']
    # Add the testing workaround to the recorded workarounds.
    recorded_workarounds.append('use_gpu_driver_workaround_for_testing')
    browser_args.append('--disable-gl-extensions=' +
                        recorded_disabled_gl_extensions)
    self.RestartBrowserIfNecessaryWithArgs(browser_args)
    self._Navigate(test_path)
    self._VerifyGpuProcessPresent()
    new_workarounds, new_disabled_gl_extensions = (
      self._CompareAndCaptureDriverBugWorkarounds())
    diff = set(recorded_workarounds).symmetric_difference(new_workarounds)
    tab = self.tab
    if len(diff) > 0:
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      self.fail(
        'GPU process and expected list of driver bug '
        'workarounds are not equal: %s != %s, diff: %s' %
        (recorded_workarounds, new_workarounds, list(diff)))
    if recorded_disabled_gl_extensions != new_disabled_gl_extensions:
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      self.fail(
        'The expected disabled gl extensions are '
        'incorrect: %s != %s:' %
        (recorded_disabled_gl_extensions, new_disabled_gl_extensions))

  def _GpuProcess_disable_gpu(self, test_path):
    # This test loads functional_webgl.html so that there is a
    # deliberate attempt to use an API which would start the GPU
    # process.
    if self._RunningOnAndroid():
      # Chrome on Android doesn't support software fallback, skip it.
      # TODO(zmo): If this test runs on ChromeOS, we also need to skip it.
      return
    self.RestartBrowserIfNecessaryWithArgs(['--disable-gpu'])
    self._NavigateAndWait(test_path)
    # On Windows, Linux or MacOS, SwiftShader is enabled, so GPU process
    # will still launch with SwiftShader.
    supports_swiftshader = self._SupportsSwiftShader()
    has_gpu_process = self.tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.hasGpuProcess()')
    if supports_swiftshader and not has_gpu_process:
      self.fail('GPU process not detected')
    elif not supports_swiftshader and has_gpu_process:
      self.fail('GPU process detected')

  def _GpuProcess_disable_gpu_and_swiftshader(self, test_path):
    # Disable SwiftShader, so GPU process should not launch anywhere.
    if self._RunningOnAndroid():
      # Chrome on Android doesn't support software fallback, skip it.
      # TODO(zmo): If this test runs on ChromeOS, we also need to skip it.
      return

    if self._AlwaysRunsGpuProcess():
      # The current configuration will always launch a GPU process, skip test.
      return

    self.RestartBrowserIfNecessaryWithArgs([
      '--disable-gpu',
      '--disable-software-rasterizer'])
    self._NavigateAndWait(test_path)
    if self.tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuProcess()'):
      self.fail('GPU process detected')

  def _GpuProcess_disable_swiftshader(self, test_path):
    # Disable SwiftShader, GPU process should be able to launch.
    self.RestartBrowserIfNecessaryWithArgs(['--disable-software-rasterizer'])
    self._NavigateAndWait(test_path)
    if not self.tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.hasGpuProcess()'):
      self.fail('GPU process not detected')

  def _GpuProcess_disabling_workarounds_works(self, test_path):
    # Hit exception from id 215 from kGpuDriverBugListEntries.
    self.RestartBrowserIfNecessaryWithArgs([
      '--gpu-driver-bug-list-test-group=1',
      '--use_gpu_driver_workaround_for_testing=0'])
    self._Navigate(test_path)
    workarounds, _ = (
      self._CompareAndCaptureDriverBugWorkarounds())
    if 'use_gpu_driver_workaround_for_testing' in workarounds:
      self.fail('use_gpu_driver_workaround_for_testing erroneously present')

  def _GpuProcess_swiftshader_for_webgl(self, test_path):
    # This test loads functional_webgl.html so that there is a deliberate
    # attempt to use an API which would start the GPU process.
    # On platforms where SwiftShader is not supported, skip this test.
    if not self._SupportsSwiftShader():
      return
    args_list = (
      # Triggering test_group 2 where WebGL is blacklisted.
      ['--gpu-blacklist-test-group=2'],
      # Explicitly disable GPU access.
      ['--disable-gpu'])
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
      if not gpu.aux_attributes['software_rendering']:
        self.fail("Software rendering was disabled")
      if 'SwiftShader' not in gpu.aux_attributes['gl_renderer']:
        self.fail("Expected 'SwiftShader' in GPU info GL renderer string")
      if 'Google' not in gpu.aux_attributes['gl_vendor']:
        self.fail("Expected 'Google' in GPU info GL vendor string")
      device = gpu.devices[0]
      if not device:
        self.fail("System Info doesn't have a device")
      # Validate extensions.
      ext_list = [
        'ANGLE_instanced_arrays',
        'EXT_blend_minmax',
        'EXT_texture_filter_anisotropic',
        'WEBKIT_EXT_texture_filter_anisotropic',
        'OES_element_index_uint',
        'OES_standard_derivatives',
        'OES_texture_float',
        'OES_texture_float_linear',
        'OES_texture_half_float',
        'OES_texture_half_float_linear',
        'OES_vertex_array_object',
        'WEBGL_compressed_texture_etc1',
        'WEBGL_debug_renderer_info',
        'WEBGL_debug_shaders',
        'WEBGL_depth_texture',
        'WEBKIT_WEBGL_depth_texture',
        'WEBGL_draw_buffers',
        'WEBGL_lose_context',
        'WEBKIT_WEBGL_lose_context',
      ]
      tab = self.tab
      for ext in ext_list:
        if tab.EvaluateJavaScript('!gl_context.getExtension("' + ext + '")'):
          self.fail("Expected " + ext + " support")

  def _GpuProcess_webgl_disabled_extension(self, test_path):
    # Hit exception from id 257 from kGpuDriverBugListEntries.
    self.RestartBrowserIfNecessaryWithArgs([
      '--gpu-driver-bug-list-test-group=2',
    ])
    self._NavigateAndWait(test_path)

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
