# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Base class for WebGL conformance tests."""

import collections
import logging
import json
import os
import time
from typing import Any, List, Optional, Set, Tuple

import dataclasses  # Built-in, but pylint gives an ordering false positive.

from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import gpu_helper
from gpu_tests import gpu_integration_test
from gpu_tests import webgl_test_util
from gpu_tests.util import host_information
from gpu_tests.util import websocket_server as wss
from gpu_tests.util import websocket_utils

import gpu_path_util

from telemetry.internal.platform import gpu_info as telemetry_gpu_info

TEST_PAGE_RELPATH = os.path.join(webgl_test_util.extensions_relpath,
                                 'webgl_test_page.html')

WEBSOCKET_JAVASCRIPT_TIMEOUT_S = 5
HEARTBEAT_TIMEOUT_S = 15
ASAN_MULTIPLIER = 2
SLOW_MULTIPLIER = 4
WEBENGINE_MULTIPLIER = 4

# Thresholds for how slow parts of the test have to be for the test to be
# considered slow overall.
SLOW_HEARTBEAT_THRESHOLD = 0.5

# Non-standard timeouts that can't be handled by a Slow expectation, likely due
# to being particularly long or not specific to a configuration. Try to use
# expectations first.
NON_STANDARD_HEARTBEAT_TIMEOUTS = {}

# Non-standard timeouts for executing the JavaScript to establish the websocket
# connection. A test being in here implies that it starts doing a huge amount
# of work in JavaScript immediately, preventing execution of JavaScript via
# devtools as well.
NON_STANDARD_WEBSOCKET_JAVASCRIPT_TIMEOUTS = {}


# cmp no longer exists in Python 3
def cmp(a: Any, b: Any) -> int:
  return int(a > b) - int(a < b)


def _CompareVersion(version1: str, version2: str) -> int:
  ver_num1 = [int(x) for x in version1.split('.')]
  ver_num2 = [int(x) for x in version2.split('.')]
  size = min(len(ver_num1), len(ver_num2))
  return cmp(ver_num1[0:size], ver_num2[0:size])


@dataclasses.dataclass
class WebGLTestArgs():
  """Struct-like class for passing args to a WebGLConformance test."""
  webgl_version: Optional[int] = None
  extension: Optional[str] = None
  extension_list: Optional[List[str]] = None


class WebGLConformanceIntegrationTestBase(
    gpu_integration_test.GpuIntegrationTest):

  _webgl_version: Optional[int] = None
  _crash_count = 0
  _gl_backend = ''
  _angle_backend = ''
  _command_decoder = ''
  _verified_flags = False
  _original_environ: Optional[collections.abc.Mapping] = None
  page_loaded = False

  # Scripts read from file during process start up.
  _conformance_harness_script: Optional[str] = None
  _extension_harness_additional_script: Optional[str] = None

  websocket_server: Optional[wss.WebsocketServer] = None

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self._longest_time_between_heartbeats = 0

  @classmethod
  def _SuiteSupportsParallelTests(cls) -> bool:
    return True

  def _GetSerialGlobs(self) -> Set[str]:
    serial_globs = set()
    if host_information.IsMac():
      if host_information.IsAmdGpu():
        serial_globs |= {
            # crbug.com/1345466. Can be removed once OpenGL is no longer used on
            # Mac.
            'deqp/functional/gles3/transformfeedback/*',
        }
      if host_information.IsIntelGpu():
        serial_globs |= {
            # crbug.com/1412460. Flaky timeouts on Mac Intel.
            'deqp/functional/gles3/shadermatrix/*',
            'deqp/functional/gles3/shaderoperator/*',
        }
    return serial_globs

  def _GetSerialTests(self) -> Set[str]:
    serial_tests = set()
    if host_information.IsLinux() and host_information.IsNvidiaGpu():
      serial_tests |= {
          # crbug.com/328528533. Regularly takes 2-3 minutes to complete on
          # Linux/NVIDIA/Debug and can flakily hit the 5 minute timeout.
          'conformance/uniforms/uniform-samplers-test.html',
      }
    return serial_tests

  @classmethod
  def AddCommandlineArgs(cls, parser: ct.CmdArgParser) -> None:
    super().AddCommandlineArgs(parser)
    parser.add_argument('--webgl-conformance-version',
                        help='Version of the WebGL conformance tests to run.',
                        default='1.0.4')
    parser.add_argument(
        '--webgl2-only',
        action='store_true',
        default=False,
        help='Whether we include webgl 1 tests if version is 2.0.0 or above.')
    parser.add_argument('--enable-metal-debug-layers',
                        action='store_true',
                        default=False,
                        help='Whether to enable Metal debug layers')

  @classmethod
  def StartBrowser(cls) -> None:
    cls.page_loaded = False
    super().StartBrowser()

  @classmethod
  def StopBrowser(cls) -> None:
    if cls.websocket_server:
      cls.websocket_server.ClearCurrentConnection()
    super().StopBrowser()

  @classmethod
  def _SetClassVariablesFromOptions(cls, options: ct.ParsedCmdArgs) -> None:
    super()._SetClassVariablesFromOptions(options)
    cls._webgl_version = int(options.webgl_conformance_version.split('.')[0])
    if not cls._conformance_harness_script:
      with open(
          os.path.join(gpu_path_util.GPU_TEST_HARNESS_JAVASCRIPT_DIR,
                       'websocket_heartbeat.js')) as f:
        cls._conformance_harness_script = f.read()
      cls._conformance_harness_script += '\n'
      with open(
          os.path.join(gpu_path_util.GPU_TEST_HARNESS_JAVASCRIPT_DIR,
                       'webgl_conformance_harness_script.js')) as f:
        cls._conformance_harness_script += f.read()
    if not cls._extension_harness_additional_script:
      with open(
          os.path.join(
              gpu_path_util.GPU_TEST_HARNESS_JAVASCRIPT_DIR,
              'webgl_conformance_extension_harness_additional_script.js')) as f:
        cls._extension_harness_additional_script = f.read()

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    #
    # Conformance tests
    #
    test_paths = cls._ParseTests('00_test_list.txt',
                                 options.webgl_conformance_version,
                                 options.webgl2_only, None)
    assert cls._webgl_version is not None
    for test_path in test_paths:
      test_path_with_args = test_path
      if cls._webgl_version > 1:
        test_path_with_args += '?webglVersion=' + str(cls._webgl_version)
      yield (test_path.replace(os.path.sep, '/'),
             os.path.join(webgl_test_util.conformance_relpath,
                          test_path_with_args),
             ['_RunConformanceTest', WebGLTestArgs()])

    #
    # Extension tests
    #
    extension_tests = cls._GetExtensionList()
    # Coverage test.
    yield ('WebglExtension_TestCoverage',
           os.path.join(webgl_test_util.extensions_relpath,
                        'webgl_extension_test.html'), [
                            '_RunExtensionCoverageTest',
                            WebGLTestArgs(webgl_version=cls._webgl_version,
                                          extension_list=extension_tests)
                        ])
    # Individual extension tests.
    for extension in extension_tests:
      yield ('WebglExtension_%s' % extension,
             os.path.join(webgl_test_util.extensions_relpath,
                          'webgl_extension_test.html'), [
                              '_RunExtensionTest',
                              WebGLTestArgs(webgl_version=cls._webgl_version,
                                            extension=extension)
                          ])

  @classmethod
  def _GetExtensionList(cls) -> List[str]:
    raise NotImplementedError()

  @classmethod
  def _ModifyBrowserEnvironment(cls) -> None:
    super()._ModifyBrowserEnvironment()
    if (host_information.IsMac()
        and cls.GetOriginalFinderOptions().enable_metal_debug_layers):
      if cls._original_environ is None:
        cls._original_environ = os.environ.copy()
      os.environ['MTL_DEBUG_LAYER'] = '1'
      os.environ['MTL_DEBUG_LAYER_VALIDATE_LOAD_ACTIONS'] = '1'
      # TODO(crbug.com/40275874)  Re-enable when Apple fixes the validation
      # os.environ['MTL_DEBUG_LAYER_VALIDATE_STORE_ACTIONS'] = '1'
      os.environ['MTL_DEBUG_LAYER_VALIDATE_UNRETAINED_RESOURCES'] = '4'

  @classmethod
  def _RestoreBrowserEnvironment(cls) -> None:
    if cls._original_environ is not None:
      os.environ.clear()
      os.environ.update(cls._original_environ)
    super()._RestoreBrowserEnvironment()

  def _ShouldForceRetryOnFailureFirstTest(self) -> bool:
    retry_from_super = super()._ShouldForceRetryOnFailureFirstTest()
    # Force RetryOnFailure of the first test on a shard on ChromeOS VMs.
    # See crbug.com/1079244.
    try:
      retry_on_amd64_generic = ('chromeos-board-amd64-generic'
                                in self.GetPlatformTags(self.browser))
    except Exception:  # pylint: disable=broad-except
      logging.warning(
          'Failed to determine if running on a ChromeOS VM, assuming no')
      retry_on_amd64_generic = False
    return retry_from_super or retry_on_amd64_generic

  def _TestWasSlow(self) -> bool:
    # Consider the test slow if it had a relatively long time between
    # heartbeats.
    heartbeat_fraction = (self._longest_time_between_heartbeats /
                          self._GetNonSlowHeartbeatTimeout())
    return heartbeat_fraction > SLOW_HEARTBEAT_THRESHOLD

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    # This indirection allows these tests to trampoline through
    # _RunGpuTest.
    assert len(args) == 2
    test_name = args[0]
    test_args = args[1]
    getattr(self, test_name)(test_path, test_args)

  def _VerifyGLBackend(self, gpu_info: telemetry_gpu_info.GPUInfo) -> bool:
    # Verify that Chrome's GL backend matches if a specific one was requested
    if self._gl_backend:
      if (self._gl_backend == 'angle'
          and gpu_helper.GetANGLERenderer(gpu_info) == 'angle-disabled'):
        self.fail('requested GL backend (' + self._gl_backend + ')' +
                  ' had no effect on the browser: ' +
                  _GetGPUInfoErrorString(gpu_info))
        return False
    return True

  def _VerifyANGLEBackend(self, gpu_info: telemetry_gpu_info.GPUInfo) -> bool:
    if self._angle_backend:
      # GPU exepections use slightly different names for the angle backends
      # than the Chrome flags
      known_backend_flag_map = {
          'angle-d3d11': ['d3d11'],
          'angle-d3d9': ['d3d9'],
          'angle-opengl': ['gl'],
          'angle-opengles': ['gles'],
          'angle-metal': ['metal'],
          'angle-vulkan': ['vulkan'],
          # Support setting VK_ICD_FILENAMES for swiftshader when requesting
          # the 'vulkan' backend.
          'angle-swiftshader': ['swiftshader', 'vulkan'],
      }
      current_angle_backend = gpu_helper.GetANGLERenderer(gpu_info)
      if (current_angle_backend not in known_backend_flag_map or
          self._angle_backend not in \
            known_backend_flag_map[current_angle_backend]):
        self.fail('requested ANGLE backend (' + self._angle_backend + ')' +
                  ' had no effect on the browser: ' +
                  _GetGPUInfoErrorString(gpu_info))
        return False
    return True

  def _VerifyCommandDecoder(self, gpu_info: telemetry_gpu_info.GPUInfo) -> bool:
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
                  _GetGPUInfoErrorString(gpu_info))
        return False
    return True

  def _NavigateTo(self, test_path: str, harness_script: str) -> None:
    if not self.__class__.page_loaded:
      # If we haven't loaded the test page that we use to run tests within an
      # iframe, load it and establish the websocket connection.
      url = self.UrlOfStaticFilePath(TEST_PAGE_RELPATH)
      self.tab.Navigate(url, script_to_evaluate_on_commit=harness_script)
      self.tab.WaitForDocumentReadyStateToBeComplete(timeout=5)
      self.tab.action_runner.EvaluateJavaScript(
          'connectWebsocket("%d")' %
          self.__class__.websocket_server.server_port,
          timeout=WEBSOCKET_JAVASCRIPT_TIMEOUT_S)
      self.__class__.websocket_server.WaitForConnection(
          websocket_utils.GetScaledConnectionTimeout(self.child.jobs))
      response = self.__class__.websocket_server.Receive(
          WEBSOCKET_JAVASCRIPT_TIMEOUT_S)
      response = json.loads(response)
      assert response['type'] == 'CONNECTION_ACK'
      self.__class__.page_loaded = True

    gpu_info = self.browser.GetSystemInfo().gpu
    self._crash_count = gpu_info.aux_attributes['process_crash_count']
    if not self._verified_flags:
      # If the user specified any flags for ANGLE or the command decoder,
      # verify that the browser is actually using the requested configuration
      if (self._VerifyGLBackend(gpu_info) and self._VerifyANGLEBackend(gpu_info)
          and self._VerifyCommandDecoder(gpu_info)):
        self._verified_flags = True
    url = self.UrlOfStaticFilePath(test_path)
    self.tab.action_runner.EvaluateJavaScript('runTest("%s")' % url)

  def _HandleMessageLoop(self, test_timeout: float) -> None:
    got_test_started = False
    start_time = time.time()
    try:
      while True:
        response_start_time = time.time()
        response = self.__class__.websocket_server.Receive(
            self._GetHeartbeatTimeout())
        self._longest_time_between_heartbeats = max(
            self._longest_time_between_heartbeats,
            time.time() - response_start_time)
        response = json.loads(response)
        response_type = response['type']

        if time.time() - start_time > test_timeout:
          raise RuntimeError(
              'Hit %.3f second global timeout, but page continued to send '
              'messages over the websocket, i.e. was not due to a renderer '
              'crash.' % test_timeout)

        if not got_test_started:
          if response_type != 'TEST_STARTED':
            raise RuntimeError('Got response %s when expected a test start.' %
                               response_type)
          got_test_started = True
          continue

        if response_type == 'TEST_HEARTBEAT':
          continue
        if response_type == 'TEST_FINISHED':
          break
        raise RuntimeError('Received unknown message type %s' % response_type)
    except wss.WebsocketReceiveMessageTimeoutError:
      websocket_utils.HandleWebsocketReceiveTimeoutError(self.tab, start_time)
      raise
    except wss.ClientClosedConnectionError as e:
      websocket_utils.HandlePrematureSocketClose(e, start_time)

  def _GetHeartbeatTimeout(self) -> int:
    return int(self._GetNonSlowHeartbeatTimeout() * self._GetSlowMultiplier())

  def _GetNonSlowHeartbeatTimeout(self) -> float:
    return (NON_STANDARD_HEARTBEAT_TIMEOUTS.get(self.shortName(),
                                                HEARTBEAT_TIMEOUT_S) *
            self._GetBrowserTimeoutMultiplier())

  def _GetBrowserTimeoutMultiplier(self) -> float:
    """Compute the multiplier to account for overall browser slowness."""
    # Parallel jobs increase load and can slow down test execution, so scale
    # based on the number of jobs. Target 2x increase with 4 jobs.
    multiplier = 1 + (self.child.jobs - 1) / 3.0
    if self._is_asan:
      multiplier *= ASAN_MULTIPLIER
    if self._finder_options.browser_type == 'web-engine-shell':
      multiplier *= WEBENGINE_MULTIPLIER
    return multiplier

  def _GetSlowMultiplier(self) -> float:
    if self._IsSlowTest():
      return SLOW_MULTIPLIER
    return 1

  def _IsSlowTest(self) -> bool:
    # We access the expectations directly instead of using
    # self.GetExpectationsForTest since we need the raw results, but that method
    # only returns the parsed results and whether the test should be retried.
    expectation = self.child.expectations.expectations_for(self.shortName())
    return 'Slow' in expectation.raw_results

  def _CheckTestCompletion(self) -> None:
    self._HandleMessageLoop(self._GetTestTimeout())
    if self._crash_count != self.browser.GetSystemInfo().gpu \
        .aux_attributes['process_crash_count']:
      self.fail('GPU process crashed during test.\n' +
                self._WebGLTestMessages(self.tab))
    elif not self._DidWebGLTestSucceed(self.tab):
      self.fail(self._WebGLTestMessages(self.tab))

  def _RunConformanceTest(self, test_path: str, _: WebGLTestArgs) -> None:
    self._NavigateTo(test_path, self._conformance_harness_script)
    self._CheckTestCompletion()

  def _RunExtensionCoverageTest(self, test_path: str,
                                test_args: WebGLTestArgs) -> None:
    self._NavigateTo(test_path, self._GetExtensionHarnessScript())
    self.tab.action_runner.WaitForJavaScriptCondition(
        'testIframeLoaded', timeout=self._GetTestTimeout())
    context_type = 'webgl2' if test_args.webgl_version == 2 else 'webgl'
    extension_list_string = '['
    for extension in test_args.extension_list:
      extension_list_string = extension_list_string + extension + ', '
    extension_list_string = extension_list_string + ']'
    self.tab.action_runner.EvaluateJavaScript(
        'testIframe.contentWindow.'
        'checkSupportedExtensions({{ extensions_string }}, {{context_type}})',
        extensions_string=extension_list_string,
        context_type=context_type)
    self._CheckTestCompletion()

  def _RunExtensionTest(self, test_path: str, test_args: WebGLTestArgs) -> None:
    self._NavigateTo(test_path, self._GetExtensionHarnessScript())
    self.tab.action_runner.WaitForJavaScriptCondition(
        'testIframeLoaded', timeout=self._GetTestTimeout())
    context_type = 'webgl2' if test_args.webgl_version == 2 else 'webgl'
    self.tab.action_runner.EvaluateJavaScript(
        'testIframe.contentWindow.'
        'checkExtension({{ extension }}, {{ context_type }})',
        extension=test_args.extension,
        context_type=context_type)
    self._CheckTestCompletion()

  def _GetTestTimeout(self) -> int:
    timeout = 300
    if self._is_asan:
      # Asan runs much slower and needs a longer timeout
      timeout *= 2
    return timeout

  def _GetExtensionHarnessScript(self) -> str:
    assert self._conformance_harness_script is not None
    assert self._extension_harness_additional_script is not None
    return (self._conformance_harness_script +
            self._extension_harness_additional_script)

  @classmethod
  def GenerateBrowserArgs(cls, additional_args: List[str]) -> List[str]:
    """Adds default arguments to |additional_args|.

    See the parent class' method documentation for additional information.
    """
    default_args = super().GenerateBrowserArgs(additional_args)

    # --test-type=gpu is used only to suppress the "Google API Keys are missing"
    # infobar, which causes flakiness in tests.
    default_args.extend([
        cba.AUTOPLAY_POLICY_NO_USER_GESTURE_REQUIRED,
        cba.DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS,
        cba.DISABLE_GPU_PROCESS_CRASH_LIMIT,
        cba.TEST_TYPE_GPU,
        '--enable-webgl-draft-extensions',
        # Try disabling the GPU watchdog to see if this affects the
        # intermittent GPU process hangs that have been seen on the
        # waterfall. crbug.com/596622 crbug.com/609252
        '--disable-gpu-watchdog',
        # Force-enable SharedArrayBuffer to be able to test its
        # support in WEBGL_multi_draw.
        '--enable-blink-features=SharedArrayBuffer',
    ])
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
        elif o.startswith('--use-gl='):
          cls._gl_backend = o[len('--use-gl='):]
        elif o.startswith('--use-angle='):
          cls._angle_backend = o[len('--use-angle='):]
        elif o.startswith('--use-cmd-decoder='):
          cls._command_decoder = o[len('--use-cmd-decoder='):]
    if found_js_flags:
      logging.warning('Overriding built-in JavaScript flags:')
      logging.warning(' Original flags: %s', builtin_js_flags)
      logging.warning(' New flags: %s', user_js_flags)
    else:
      default_args.append(builtin_js_flags)

    return default_args

  @classmethod
  def SetUpProcess(cls) -> None:
    super().SetUpProcess()

    # Logging every time a connection is opened/closed is spammy, so decrease
    # the default log level.
    logging.getLogger('websockets.server').setLevel(logging.WARNING)
    cls.websocket_server = wss.WebsocketServer()
    cls.websocket_server.StartServer()

    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    # By setting multiple server directories, the root of the server
    # implicitly becomes the common base directory, i.e., the Chromium
    # src dir, and all URLs have to be specified relative to that.
    cls.SetStaticServerDirs([
        os.path.join(gpu_path_util.CHROMIUM_SRC_DIR,
                     webgl_test_util.conformance_relpath),
        os.path.join(gpu_path_util.CHROMIUM_SRC_DIR,
                     webgl_test_util.extensions_relpath),
    ])

  @classmethod
  def TearDownProcess(cls) -> None:
    cls.websocket_server.StopServer()
    cls.websocket_server = None
    super(WebGLConformanceIntegrationTestBase, cls).TearDownProcess()

  # Helper functions.

  @staticmethod
  def _DidWebGLTestSucceed(tab: ct.Tab) -> bool:
    # Ensure that we actually ran tests and they all passed.
    return tab.EvaluateJavaScript('webglTestHarness._allTestSucceeded '
                                  '&& webglTestHarness._totalTests > 0')

  @staticmethod
  def _WebGLTestMessages(tab: ct.Tab) -> str:
    return tab.EvaluateJavaScript('webglTestHarness._messages')

  @classmethod
  def _ParseTests(cls, path: str, version: str, webgl2_only: bool,
                  folder_min_version: Optional[str]) -> List[str]:
    def _ParseTestNameAndVersions(line: str
                                  ) -> Tuple[str, Optional[str], Optional[str]]:
      """Parses any min/max versions and the test name on the given line.

      Args:
        line: A string containing the line to be parsed.

      Returns:
        A tuple (test_name, min_version, max_version) containing the test name
        and parsed minimum/maximum versions found as strings. Min/max values can
        be None if no version was found.
      """
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
      return test_name, min_version, max_version

    test_paths = []
    full_path = os.path.normpath(
        os.path.join(webgl_test_util.conformance_path, path))

    if not os.path.exists(full_path):
      raise Exception('The WebGL conformance test path specified ' +
                      'does not exist: ' + full_path)

    with open(full_path, 'r', encoding='utf-8') as f:
      for line in f:
        line = line.strip()

        if not line:
          continue
        if line.startswith('//') or line.startswith('#'):
          continue

        test_name, min_version, max_version = _ParseTestNameAndVersions(line)
        min_version_to_compare = min_version or folder_min_version

        if (min_version_to_compare
            and _CompareVersion(version, min_version_to_compare) < 0):
          continue
        if max_version and _CompareVersion(version, max_version) > 0:
          continue
        if (webgl2_only and not '.txt' in test_name
            and (not min_version_to_compare
                 or not min_version_to_compare.startswith('2'))):
          continue

        include_path = os.path.join(os.path.dirname(path), test_name)
        if '.txt' in test_name:
          # We only check min-version >= 2.0.0 for the top level list.
          test_paths += cls._ParseTests(include_path, version, webgl2_only,
                                        min_version_to_compare)
        else:
          test_paths.append(include_path)

    return test_paths

  @classmethod
  def GetPlatformTags(cls, browser: ct.Browser) -> List[str]:
    assert cls._webgl_version is not None
    tags = super().GetPlatformTags(browser)
    return tags

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    raise NotImplementedError()


def _GetGPUInfoErrorString(gpu_info: telemetry_gpu_info.GPUInfo) -> str:
  primary_gpu = gpu_info.devices[0]
  error_str = 'primary gpu=' + primary_gpu.device_string
  if gpu_info.aux_attributes:
    gl_renderer = gpu_info.aux_attributes.get('gl_renderer')
    if gl_renderer:
      error_str += ', gl_renderer=' + gl_renderer
  return error_str
