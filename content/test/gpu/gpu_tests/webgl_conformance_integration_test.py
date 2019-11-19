# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import sys

from gpu_tests import gpu_helper
from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import webgl_test_util


conformance_harness_script = r"""
  var testHarness = {};
  testHarness._allTestSucceeded = true;
  testHarness._messages = '';
  testHarness._failures = 0;
  testHarness._finished = false;
  testHarness._originalLog = window.console.log;

  testHarness.log = function(msg) {
    testHarness._messages += msg + "\n";
    testHarness._originalLog.apply(window.console, [msg]);
  }

  testHarness.reportResults = function(url, success, msg) {
    testHarness._allTestSucceeded = testHarness._allTestSucceeded && !!success;
    if(!success) {
      testHarness._failures++;
      if(msg) {
        testHarness.log(msg);
      }
    }
  };
  testHarness.notifyFinished = function(url) {
    testHarness._finished = true;
  };
  testHarness.navigateToPage = function(src) {
    var testFrame = document.getElementById("test-frame");
    testFrame.src = src;
  };

  window.webglTestHarness = testHarness;
  window.parent.webglTestHarness = testHarness;
  window.console.log = testHarness.log;
  window.onerror = function(message, url, line) {
    testHarness.reportResults(null, false, message);
    testHarness.notifyFinished(null);
  };
  window.quietMode = function() { return true; }
"""

extension_harness_additional_script = r"""
  window.onload = function() { window._loaded = true; }
"""

def _CompareVersion(version1, version2):
  ver_num1 = [int(x) for x in version1.split('.')]
  ver_num2 = [int(x) for x in version2.split('.')]
  size = min(len(ver_num1), len(ver_num2))
  return cmp(ver_num1[0:size], ver_num2[0:size])


class WebGLConformanceIntegrationTest(gpu_integration_test.GpuIntegrationTest):

  _webgl_version = None
  _is_asan = False
  _crash_count = 0
  _gl_backend = ""
  _angle_backend = ""
  _command_decoder = ""
  _verified_flags = False

  @classmethod
  def Name(cls):
    return 'webgl_conformance'

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(WebGLConformanceIntegrationTest, cls).AddCommandlineArgs(parser)
    parser.add_option('--webgl-conformance-version',
        help='Version of the WebGL conformance tests to run.',
        default='1.0.4')
    parser.add_option('--webgl2-only',
        help='Whether we include webgl 1 tests if version is 2.0.0 or above.',
        default='false')
    parser.add_option('--is-asan',
        help='Indicates whether currently running an ASAN build',
        action='store_true', default=False)

  @classmethod
  def GenerateGpuTests(cls, options):
    #
    # Conformance tests
    #
    test_paths = cls._ParseTests(
        '00_test_list.txt',
        options.webgl_conformance_version,
        (options.webgl2_only == 'true'),
        None)
    cls._webgl_version = [
        int(x) for x in options.webgl_conformance_version.split('.')][0]
    cls._is_asan = options.is_asan
    for test_path in test_paths:
      test_path_with_args = test_path
      if cls._webgl_version > 1:
        test_path_with_args += '?webglVersion=' + str(cls._webgl_version)
      yield (test_path.replace(os.path.sep, '/'),
             os.path.join(
                 webgl_test_util.conformance_relpath, test_path_with_args),
             ('_RunConformanceTest'))

    #
    # Extension tests
    #
    extension_tests = cls._GetExtensionList()
    # Coverage test.
    yield('WebglExtension_TestCoverage',
          os.path.join(webgl_test_util.extensions_relpath,
                       'webgl_extension_test.html'),
          ('_RunExtensionCoverageTest',
           extension_tests,
           cls._webgl_version))
    # Individual extension tests.
    for extension in extension_tests:
      yield('WebglExtension_%s' % extension,
            os.path.join(webgl_test_util.extensions_relpath,
                         'webgl_extension_test.html'),
            ('_RunExtensionTest',
             extension,
             cls._webgl_version))

  @classmethod
  def _GetExtensionList(cls):
    if cls._webgl_version == 1:
      return [
        'ANGLE_instanced_arrays',
        'EXT_blend_minmax',
        'EXT_color_buffer_half_float',
        'EXT_disjoint_timer_query',
        'EXT_float_blend',
        'EXT_frag_depth',
        'EXT_shader_texture_lod',
        'EXT_sRGB',
        'EXT_texture_filter_anisotropic',
        'KHR_parallel_shader_compile',
        'OES_element_index_uint',
        'OES_fbo_render_mipmap',
        'OES_standard_derivatives',
        'OES_texture_float',
        'OES_texture_float_linear',
        'OES_texture_half_float',
        'OES_texture_half_float_linear',
        'OES_vertex_array_object',
        'WEBGL_color_buffer_float',
        'WEBGL_compressed_texture_astc',
        'WEBGL_compressed_texture_etc',
        'WEBGL_compressed_texture_etc1',
        'WEBGL_compressed_texture_pvrtc',
        'WEBGL_compressed_texture_s3tc',
        'WEBGL_compressed_texture_s3tc_srgb',
        'WEBGL_debug_renderer_info',
        'WEBGL_debug_shaders',
        'WEBGL_depth_texture',
        'WEBGL_draw_buffers',
        'WEBGL_lose_context',
        'WEBGL_multi_draw',
        'WEBGL_video_texture',
      ]
    else:
      return [
        'EXT_color_buffer_float',
        'EXT_disjoint_timer_query_webgl2',
        'EXT_float_blend',
        'EXT_texture_filter_anisotropic',
        'KHR_parallel_shader_compile',
        'OES_texture_float_linear',
        'OVR_multiview2',
        'WEBGL_compressed_texture_astc',
        'WEBGL_compressed_texture_etc',
        'WEBGL_compressed_texture_etc1',
        'WEBGL_compressed_texture_pvrtc',
        'WEBGL_compressed_texture_s3tc',
        'WEBGL_compressed_texture_s3tc_srgb',
        'WEBGL_debug_renderer_info',
        'WEBGL_debug_shaders',
        'WEBGL_draw_instanced_base_vertex_base_instance',
        'WEBGL_lose_context',
        'WEBGL_multi_draw',
        'WEBGL_multi_draw_instanced_base_vertex_base_instance',
        'WEBGL_video_texture',
      ]

  def RunActualGpuTest(self, test_path, *args):
    # This indirection allows these tests to trampoline through
    # _RunGpuTest.
    test_name = args[0]
    getattr(self, test_name)(test_path, *args[1:])

  def _GetGPUInfoErrorString(self, gpu_info):
    primary_gpu = gpu_info.devices[0]
    error_str = 'primary gpu=' + primary_gpu.device_string
    if gpu_info.aux_attributes:
      gl_renderer = gpu_info.aux_attributes.get('gl_renderer')
      if gl_renderer:
        error_str += ', gl_renderer=' + gl_renderer
    return error_str

  def _VerifyGLBackend(self, gpu_info):
    # Verify that Chrome's GL backend matches if a specific one was requested
    if self._gl_backend:
      if (self._gl_backend == 'angle' and
          gpu_helper.GetANGLERenderer(gpu_info) == 'no_angle'):
        self.fail('requested GL backend (' + self._gl_backend + ')' +
                  ' had no effect on the browser: ' +
                  self._GetGPUInfoErrorString(gpu_info))
        return False
    return True

  def _VerifyANGLEBackend(self, gpu_info):
    if self._angle_backend:
      # GPU exepections use slightly different names for the angle backends
      # than the Chrome flags
      known_backend_flag_map = {
        'd3d11': 'd3d11',
        'd3d9': 'd3d9',
        'opengl': 'gl',
        'opengles': 'gles',
        'vulkan': 'vulkan',
      }
      current_angle_backend = gpu_helper.GetANGLERenderer(gpu_info)
      if (current_angle_backend not in known_backend_flag_map or
          known_backend_flag_map[current_angle_backend] != \
          self._angle_backend):
        self.fail('requested ANGLE backend (' + self._angle_backend + ')' +
                  ' had no effect on the browser: ' +
                  self._GetGPUInfoErrorString(gpu_info))
        return False
    return True

  def _VerifyCommandDecoder(self, gpu_info):
    if self._command_decoder:
      # GPU exepections use slightly different names for the command decoders
      # than the Chrome flags
      known_command_decoder_flag_map = {
        'passthrough': 'passthrough',
        'no_passthrough': 'validating',
      }
      current_command_decoder = gpu_helper.GetCommandDecoder(gpu_info)
      if (current_command_decoder not in known_command_decoder_flag_map or
          known_command_decoder_flag_map[current_command_decoder] != \
          self._command_decoder):
        self.fail('requested command decoder (' + self._command_decoder + ')' +
                  ' had no effect on the browser: ' +
                  self._GetGPUInfoErrorString(gpu_info))
        return False
    return True

  def _NavigateTo(self, test_path, harness_script):
    gpu_info = self.browser.GetSystemInfo().gpu
    self._crash_count = gpu_info.aux_attributes['process_crash_count']
    if not self._verified_flags:
      # If the user specified any flags for ANGLE or the command decoder,
      # verify that the browser is actually using the requested configuration
      if (self._VerifyGLBackend(gpu_info) and
          self._VerifyANGLEBackend(gpu_info) and
          self._VerifyCommandDecoder(gpu_info)):
        self._verified_flags = True
    url = self.UrlOfStaticFilePath(test_path)
    self.tab.Navigate(url, script_to_evaluate_on_commit=harness_script)

  def _CheckTestCompletion(self):
    self.tab.action_runner.WaitForJavaScriptCondition(
        'webglTestHarness._finished', timeout=self._GetTestTimeout())
    if self._crash_count != self.browser.GetSystemInfo().gpu \
        .aux_attributes['process_crash_count']:
      self.fail('GPU process crashed during test.\n' +
                self._WebGLTestMessages(self.tab))
    elif not self._DidWebGLTestSucceed(self.tab):
      self.fail(self._WebGLTestMessages(self.tab))

  def _RunConformanceTest(self, test_path, *args):
    self._NavigateTo(test_path, conformance_harness_script)
    self._CheckTestCompletion()


  def _GetExtensionHarnessScript(self):
    return conformance_harness_script + extension_harness_additional_script

  def _RunExtensionCoverageTest(self, test_path, *args):
    self._NavigateTo(test_path, self._GetExtensionHarnessScript())
    self.tab.action_runner.WaitForJavaScriptCondition(
        'window._loaded', timeout=self._GetTestTimeout())
    extension_list = args[0]
    webgl_version = args[1]
    context_type = "webgl2" if webgl_version == 2 else "webgl"
    extension_list_string = "["
    for extension in extension_list:
      extension_list_string = extension_list_string + extension + ", "
    extension_list_string = extension_list_string + "]"
    self.tab.action_runner.EvaluateJavaScript(
        'checkSupportedExtensions({{ extensions_string }}, {{context_type}})',
        extensions_string=extension_list_string, context_type=context_type)
    self._CheckTestCompletion()

  def _RunExtensionTest(self, test_path, *args):
    self._NavigateTo(test_path, self._GetExtensionHarnessScript())
    self.tab.action_runner.WaitForJavaScriptCondition(
        'window._loaded', timeout=self._GetTestTimeout())
    extension = args[0]
    webgl_version = args[1]
    context_type = "webgl2" if webgl_version == 2 else "webgl"
    self.tab.action_runner.EvaluateJavaScript(
      'checkExtension({{ extension }}, {{ context_type }})',
      extension=extension, context_type=context_type)
    self._CheckTestCompletion()

  def _GetTestTimeout(self):
    timeout = 300
    if self._is_asan:
      # Asan runs much slower and needs a longer timeout
      timeout *= 2
    return timeout

  @classmethod
  def SetupWebGLBrowserArgs(cls, browser_args):
    # --test-type=gpu is used only to suppress the "Google API Keys are missing"
    # infobar, which causes flakiness in tests.
    browser_args += [
      '--autoplay-policy=no-user-gesture-required',
      '--disable-domain-blocking-for-3d-apis',
      '--disable-gpu-process-crash-limit',
      '--test-type=gpu',
      '--enable-webgl-draft-extensions',
      # Try disabling the GPU watchdog to see if this affects the
      # intermittent GPU process hangs that have been seen on the
      # waterfall. crbug.com/596622 crbug.com/609252
      '--disable-gpu-watchdog',
      # TODO(http://crbug.com/832952): Remove this when WebXR spec is more
      # stable and setCompatibleXRDevice is part of the conformance test.
      '--disable-blink-features=WebXR',
      # TODO(crbug.com/830901): see whether disabling this feature
      # makes the WebGL video upload tests reliable again.
      '--disable-features=UseSurfaceLayerForVideo',
    ]
    # Note that the overriding of the default --js-flags probably
    # won't interact well with RestartBrowserIfNecessaryWithArgs, but
    # we don't use that in this test.
    browser_options = cls._finder_options.browser_options
    builtin_js_flags = '--js-flags=--expose-gc'
    found_js_flags = False
    user_js_flags = ''
    if browser_options.extra_browser_args:
      for o in browser_options.extra_browser_args:
        if o.startswith('--js-flags'):
          found_js_flags = True
          user_js_flags = o
          break
        if o.startswith('--use-gl='):
          cls._gl_backend = o[len('--use-gl='):]
        if o.startswith('--use-angle='):
          cls._angle_backend = o[len('--use-angle='):]
        if o.startswith('--use-cmd-decoder='):
          cls._command_decoder = o[len('--use-cmd-decoder='):]
    if found_js_flags:
      logging.warning('Overriding built-in JavaScript flags:')
      logging.warning(' Original flags: ' + builtin_js_flags)
      logging.warning(' New flags: ' + user_js_flags)
    else:
      browser_args += [builtin_js_flags]
    cls.CustomizeBrowserArgs(browser_args)

  @classmethod
  def SetUpProcess(cls):
    super(WebGLConformanceIntegrationTest, cls).SetUpProcess()
    cls.SetupWebGLBrowserArgs([])
    cls.StartBrowser()
    # By setting multiple server directories, the root of the server
    # implicitly becomes the common base directory, i.e., the Chromium
    # src dir, and all URLs have to be specified relative to that.
    cls.SetStaticServerDirs([
      os.path.join(path_util.GetChromiumSrcDir(),
                   webgl_test_util.conformance_relpath),
      os.path.join(path_util.GetChromiumSrcDir(),
                   webgl_test_util.extensions_relpath)])

  # Helper functions.

  @staticmethod
  def _DidWebGLTestSucceed(tab):
    return tab.EvaluateJavaScript('webglTestHarness._allTestSucceeded')

  @staticmethod
  def _WebGLTestMessages(tab):
    return tab.EvaluateJavaScript('webglTestHarness._messages')

  @classmethod
  def _ParseTests(cls, path, version, webgl2_only, folder_min_version):
    test_paths = []
    current_dir = os.path.dirname(path)
    full_path = os.path.normpath(os.path.join(webgl_test_util.conformance_path,
                                              path))

    if not os.path.exists(full_path):
      raise Exception('The WebGL conformance test path specified ' +
        'does not exist: ' + full_path)

    with open(full_path, 'r') as f:
      for line in f:
        line = line.strip()

        if not line:
          continue

        if line.startswith('//') or line.startswith('#'):
          continue

        line_tokens = line.split(' ')
        test_name = line_tokens[-1]

        i = 0
        min_version = None
        max_version = None
        while i < len(line_tokens):
          token = line_tokens[i]
          if token == '--min-version':
            i += 1
            min_version = line_tokens[i]
          elif token == '--max-version':
            i += 1
            max_version = line_tokens[i]
          i += 1

        min_version_to_compare = min_version or folder_min_version

        if (min_version_to_compare and
            _CompareVersion(version, min_version_to_compare) < 0):
          continue

        if max_version and _CompareVersion(version, max_version) > 0:
          continue

        if (webgl2_only and not '.txt' in test_name and
            (not min_version_to_compare or
             not min_version_to_compare.startswith('2'))):
          continue

        if '.txt' in test_name:
          include_path = os.path.join(current_dir, test_name)
          # We only check min-version >= 2.0.0 for the top level list.
          test_paths += cls._ParseTests(
              include_path, version, webgl2_only, min_version_to_compare)
        else:
          test = os.path.join(current_dir, test_name)
          test_paths.append(test)

    return test_paths

  @classmethod
  def GetPlatformTags(cls, browser):
    tags = super(WebGLConformanceIntegrationTest, cls).GetPlatformTags(browser)
    tags.extend(
        [['no-asan', 'asan'][cls._is_asan],
         'webgl-version-%d' % cls._webgl_version])

    if gpu_helper.EXPECTATIONS_DRIVER_TAGS:
      system_info = browser.GetSystemInfo()
      if system_info:
        gpu_info = system_info.gpu
        driver_vendor = gpu_helper.GetGpuDriverVendor(gpu_info)
        driver_version = gpu_helper.GetGpuDriverVersion(gpu_info)
        if driver_vendor and driver_version:
          driver_vendor = driver_vendor.lower()
          driver_version = driver_version.lower()

          # Extract the string of vendor from 'angle (vendor)'
          matcher = re.compile(r'^angle \(([a-z]+)\)$')
          match = matcher.match(driver_vendor)
          if match:
            driver_vendor = match.group(1)

          # Extract the substring before first space/dash/underscore
          matcher = re.compile(r'^([a-z\d]+)([\s\-_]+[a-z\d]+)+$')
          match = matcher.match(driver_vendor)
          if match:
            driver_vendor = match.group(1)

          for tag in gpu_helper.EXPECTATIONS_DRIVER_TAGS:
            match = gpu_helper.MatchDriverTag(tag)
            assert match
            if (driver_vendor == match.group(1) and
                gpu_helper.EvaluateVersionComparison(
                    driver_version, match.group(2), match.group(3))):
              tags.append(tag)
    return tags

  @classmethod
  def ExpectationsFiles(cls):
    assert cls._webgl_version == 1 or cls._webgl_version == 2
    if cls._webgl_version == 1:
      file_name = 'webgl_conformance_expectations.txt'
    else:
      file_name = 'webgl2_conformance_expectations.txt'
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', file_name)]


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
