# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import enum
import fnmatch
import json
import logging
import os
import re
import time
from typing import Dict, List, Optional

import dataclasses  # Built-in, but pylint gives an ordering false positive.

from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test
from gpu_tests.util import host_information
from gpu_tests.util import websocket_server as wss
from gpu_tests.util import websocket_utils as wsu
from typ import expectations_parser

import gpu_path_util

SLOW_TESTS_FILE = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'third_party',
                               'dawn', 'webgpu-cts', 'slow_tests.txt')
TEST_LIST_FILE = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'third_party',
                              'dawn', 'third_party', 'gn', 'webgpu-cts',
                              'test_list.txt')
WORKER_TEST_GLOB_FILE = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR,
                                     'third_party', 'dawn', 'webgpu-cts',
                                     'worker_test_globs.txt')

TEST_RUNS_BETWEEN_CLEANUP = 1000
WEBSOCKET_PORT_TIMEOUT_SECONDS = 10
WEBSOCKET_SETUP_TIMEOUT_SECONDS = 5
DEFAULT_TEST_TIMEOUT = 30
SLOW_MULTIPLIER = 5
ASAN_MULTIPLIER = 4
BACKEND_VALIDATION_MULTIPLIER = 6
FIRST_LOAD_TEST_STARTED_MULTIPLER = 3

MESSAGE_TIMEOUT_CONNECTION_ACK = 5
# In most cases, this should be very fast, but the first test run after a page
# load can be slow.
MESSAGE_TIMEOUT_TEST_STARTED = 10
MESSAGE_TIMEOUT_HEARTBEAT = 10
MESSAGE_TIMEOUT_TEST_LOG = 1

# Thresholds for how slow parts of the test have to be for the test to be
# considered slow overall.
SLOW_HEARTBEAT_THRESHOLD = 0.5
SLOW_GLOBAL_TIMEOUT_THRESHOLD = 0.8

HTML_FILENAME = os.path.join('webgpu-cts', 'test_page.html')

JAVASCRIPT_DURATION = 'javascript_duration'
MAY_EXONERATE = 'may_exonerate'
MESSAGE_TYPE_CONNECTION_ACK = 'CONNECTION_ACK'
MESSAGE_TYPE_INFRA_FAILURE = 'INFRA_FAILURE'
MESSAGE_TYPE_TEST_STARTED = 'TEST_STARTED'
MESSAGE_TYPE_TEST_HEARTBEAT = 'TEST_HEARTBEAT'
MESSAGE_TYPE_TEST_STATUS = 'TEST_STATUS'
MESSAGE_TYPE_TEST_LOG = 'TEST_LOG'
MESSAGE_TYPE_TEST_FINISHED = 'TEST_FINISHED'

TEST_NAME_REGEX = re.compile(r'([^:]+:[^:]+:[^:]+:).*')

# This can be switched to a StrEnum once Python 3.11+ is used.
class WorkerType(enum.Enum):
  NONE = 'none'
  SERVICE = 'service'
  DEDICATED = 'dedicated'
  SHARED = 'shared'


@dataclasses.dataclass
class WebGpuTestResult():
  """Struct-like object for holding a single test result."""
  status: Optional[str] = None
  log_pieces: List[str] = ct.EmptyList()


@dataclasses.dataclass
class WebGpuTestArgs():
  """Struct-like object for holding arguments for a single test."""
  query: str
  additional_browser_args: Optional[List[str]] = None

class WebGpuCtsIntegrationTestBase(gpu_integration_test.GpuIntegrationTest):
  # Whether the test page has already been loaded. Caching this state here is
  # faster than checking the URL every time, and given how fast these tests are,
  # additional overhead like that can add up quickly.
  page_loaded = False

  # The first attempt to handle the websocket connection flakily takes
  # significantly longer, potentially due to resource contention from all
  # parallel browsers starting at the same time. See crbug.com/344009517 for
  # more information.
  attempted_websocket_connection = False

  _test_timeout = DEFAULT_TEST_TIMEOUT
  _enable_dawn_backend_validation = False
  _use_webgpu_adapter: Optional[str] = None  # use the default
  _original_environ: Optional[collections.abc.Mapping] = None
  _use_webgpu_power_preference: Optional[str] = None
  _use_fxc = False
  _os_name: Optional[str] = None
  _worker_type: Optional[WorkerType] = None

  _build_dir: Optional[str] = None

  _test_list: Optional[List[str]] = None
  _worker_test_globs: Optional[List[str]] = None

  total_tests_run = 0

  websocket_server: Optional[wss.WebsocketServer] = None

  _slow_tests: Optional[expectations_parser.TestExpectations] = None

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self._query: Optional[str] = None
    self._longest_time_between_heartbeats = 0
    self._heartbeat_timeout = 0
    self._test_duration = 0
    self._global_timeout_without_slow_multiplier = 1

  # Only perform the pre/post test cleanup every X tests instead of every test
  # to reduce overhead.
  def ShouldPerformMinidumpCleanupOnSetUp(self) -> bool:
    return (self.total_tests_run % TEST_RUNS_BETWEEN_CLEANUP == 0
            and super().ShouldPerformMinidumpCleanupOnSetUp())

  def ShouldPerformMinidumpCleanupOnTearDown(self) -> bool:
    return self.ShouldPerformMinidumpCleanupOnSetUp()

  @classmethod
  def _SuiteSupportsParallelTests(cls) -> bool:
    return True

  @classmethod
  def _GetSlowTests(cls) -> expectations_parser.TestExpectations:
    if cls._slow_tests is None:
      with open(SLOW_TESTS_FILE, 'r') as f:
        expectations = expectations_parser.TestExpectations()
        expectations.parse_tagged_list(f.read(), f.name)
        cls._slow_tests = expectations
    return cls._slow_tests

  @classmethod
  def UseWebGpuCompatMode(cls) -> bool:
    raise NotImplementedError()

  @classmethod
  def AddCommandlineArgs(cls, parser: ct.CmdArgParser) -> None:
    super().AddCommandlineArgs(parser)
    parser.add_argument('--override-timeout',
                        type=float,
                        help='Override the test timeout in seconds')
    parser.add_argument(
        '--enable-dawn-backend-validation',
        action='store_true',
        default=False,
        help='Runs the browser with Dawn backend validation enabled')
    parser.add_argument(
        '--use-webgpu-adapter',
        help='Runs the browser with a particular WebGPU adapter')
    parser.add_argument(
        '--use-webgpu-power-preference',
        help='Runs the browser with a particular WebGPU power preference')
    parser.add_argument(
        '--use-fxc',
        action='store_true',
        default=False,
        help=('On Windows, pass --disable-dawn-features=use_dxc to the '
              'browser.'))
    parser.add_argument(
        '--use-worker',
        choices=['none', 'service', 'dedicated', 'shared'],
        default='none',
        help=('Whether to run tests in workers or not. Defaults to running '
              'all tests outside of workers. If a worker type is specified, '
              'then a subset of tests will be run in the specified worker '
              'type.'))

  @classmethod
  def StartBrowser(cls) -> None:
    cls.page_loaded = False
    super().StartBrowser()
    cls._os_name = cls.browser.platform.GetOSName()
    # Set up the slow tests expectations' tags to match the test runner
    # expectations' tags
    cls._GetSlowTests().set_tags(cls.child.expectations.tags)

  @classmethod
  def GenerateBrowserArgs(cls, additional_args: List[str]) -> List[str]:
    """Adds default arguments to |additional_args|.

    See the parent class' method documentation for additional information.
    """
    browser_args = super().GenerateBrowserArgs(additional_args)

    enable_dawn_features = ['allow_unsafe_apis']
    disable_dawn_features = []

    if host_information.IsWindows():
      if cls._use_fxc:
        disable_dawn_features.append('use_dxc')
      else:
        enable_dawn_features.append('use_dxc')

    # TODO(crbug.com/364675466): Remove this when Tint IR is launched on macOS.
    if host_information.IsMac():
      enable_dawn_features.append('use_tint_ir')

    if enable_dawn_features:
      browser_args.append('--enable-dawn-features=%s' %
                          ','.join(enable_dawn_features))

    if disable_dawn_features:
      browser_args.append('--disable-dawn-features=%s' %
                          ','.join(disable_dawn_features))

    browser_args.extend(cba.ENABLE_WEBGPU_FOR_TESTING)
    if cls._use_webgpu_adapter:
      browser_args.append('--use-webgpu-adapter=%s' % cls._use_webgpu_adapter)
    if cls._use_webgpu_power_preference:
      browser_args.append('--use-webgpu-power-preference=%s' %
                          cls._use_webgpu_power_preference)
    if cls._enable_dawn_backend_validation:
      if host_information.IsWindows():
        browser_args.append('--enable-dawn-backend-validation=partial')
      else:
        browser_args.append('--enable-dawn-backend-validation')
    return browser_args

  @classmethod
  def SetUpProcess(cls) -> None:
    super().SetUpProcess()

    cls.websocket_server = wss.WebsocketServer()
    cls.websocket_server.StartServer()

    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()

    # Shared workers are not supported on Android and test suites should be
    # removed accordingly, but add a runtime check in case tests are still run
    # for some reason.
    if cls._os_name == 'android' and cls._worker_type == WorkerType.SHARED:
      raise RuntimeError('Shared workers are not supported on Android')

    # pylint:disable=protected-access
    cls._build_dir = cls.browser._browser_backend.build_dir
    # pylint:enable=protected-access
    cls.SetStaticServerDirs([
        os.path.join(cls._build_dir, 'gen', 'third_party', 'dawn'),
    ])

  @classmethod
  def TearDownProcess(cls) -> None:
    cls.websocket_server.StopServer()
    cls.websocket_server = None
    super().TearDownProcess()

  @classmethod
  def _SetClassVariablesFromOptions(cls, options: ct.ParsedCmdArgs) -> None:
    super()._SetClassVariablesFromOptions(options)
    if options.override_timeout:
      cls._test_timeout = options.override_timeout
    cls._enable_dawn_backend_validation = options.enable_dawn_backend_validation
    cls._use_webgpu_adapter = options.use_webgpu_adapter
    cls._use_webgpu_power_preference = options.use_webgpu_power_preference
    cls._use_fxc = options.use_fxc
    cls._worker_type = WorkerType(options.use_worker)

  @classmethod
  def _ModifyBrowserEnvironment(cls) -> None:
    super()._ModifyBrowserEnvironment()
    if host_information.IsMac() and cls._enable_dawn_backend_validation:
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

  @classmethod
  def _GetAdditionalBrowserArgsForQuery(cls, query: str) -> Optional[List[str]]:
    """Returns additional browser args for a given query.

    Should be overridden by child class to actually return args when necessary.
    """
    del query

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    cls._SetClassVariablesFromOptions(options)

    if cls._test_list is None:
      with open(TEST_LIST_FILE) as f:
        cls._test_list = [l for l in f.read().splitlines() if l]

    if cls._worker_type != WorkerType.NONE:
      if cls._worker_test_globs is None:
        with open(WORKER_TEST_GLOB_FILE) as f:
          contents = f.read()
        cls._worker_test_globs = [l for l in contents.splitlines() if l]

    # Iterate through all valid test names. Generate a test for each one if not
    # running worker tests, or generate a test for each if we are running worker
    # tests and the test name matches a worker glob.
    for line in cls._test_list:  # pylint:disable=not-an-iterable
      if cls._worker_type != WorkerType.NONE:
        for wg in cls._worker_test_globs:  # pylint:disable=not-an-iterable
          if fnmatch.fnmatch(line, wg):
            break
        else:
          continue
      # At this point, we're either not running worker tests or we are running
      # worker tests and the current test is a worker test.
      additional_browser_args = cls._GetAdditionalBrowserArgsForQuery(line)
      test_args = WebGpuTestArgs(
          query=line, additional_browser_args=additional_browser_args)
      yield test_args.query, HTML_FILENAME, [test_args]

  def _DetermineRetryWorkaround(self, exception: Exception) -> bool:
    # Instances of WebGpuMessageTimeoutError:
    # https://luci-analysis.appspot.com/p/chromium/rules/b9130da14f0fcab5d6ee415d209bf71b
    # https://luci-analysis.appspot.com/p/chromium/rules/6aea7ea7df4734c677f4fc5944f3b66a
    if not isinstance(exception, WebGpuMessageTimeoutError):
      return False

    if self.browser.GetAllUnsymbolizedMinidumpPaths():
      # There were some crashes, potentially expected. Don't automatically
      # retry the test.
      return False

    # Otherwise, there were no crashes and we got a `WebGpuMessageTimeoutError`.
    # Retry in case the timeout was a flake.
    return True

  def _TestWasSlow(self) -> bool:
    # Consider the test slow if it either had a relatively long time between
    # heartbeats or took up a significant chunk of the global timeout.
    heartbeat_was_slow = False
    if self._heartbeat_timeout > 0:
      heartbeat_fraction = (self._longest_time_between_heartbeats /
                            self._heartbeat_timeout)
      heartbeat_was_slow = heartbeat_fraction > SLOW_HEARTBEAT_THRESHOLD

    global_fraction = (self._test_duration /
                       self._global_timeout_without_slow_multiplier)
    global_was_slow = global_fraction > SLOW_GLOBAL_TIMEOUT_THRESHOLD

    return heartbeat_was_slow or global_was_slow

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    cls = self.__class__
    test_args = args[0]
    self._query = test_args.query
    additional_browser_args = test_args.additional_browser_args
    # Some CTS tests require non-standard browser arguments so we need to
    # verify before running each case.
    self.RestartBrowserIfNecessaryWithArgs(additional_browser_args)
    # Only a single instance is used to run tests despite a number of instances
    # (~2x the number of total tests) being initialized, so make sure to clear
    # this state so we don't accidentally keep it around from a previous test.
    if JAVASCRIPT_DURATION in self.additionalTags:
      del self.additionalTags[JAVASCRIPT_DURATION]
    if MAY_EXONERATE in self.additionalTags:
      del self.additionalTags[MAY_EXONERATE]

    if self.UseWebGpuCompatMode():
      self._query += '&compatibility=1'

    if self._worker_type != WorkerType.NONE:
      self._query += f'&worker={self._worker_type.value}'

    try:
      first_load = self._NavigateIfNecessary(test_path)
      cls.websocket_server.Send(json.dumps({'q': self._query}))
      result = self.HandleMessageLoop(first_load)

      log_str = ''.join(result.log_pieces)

      if result.status in ['skip', 'fail']:
        log_summary, *log_rest = log_str.split('\n', maxsplit=1)
        if len(log_rest):
          log_details = log_rest[0]
          logging.log(logging.ERROR, log_details)

        if result.status == 'skip':
          self.skipTest('WebGPU CTS JavaScript reported test skip\n' +
                        log_summary)
        elif result.status == 'fail':
          self.fail(
              TEST_NAME_REGEX.match(self._query).group(1) + ' failed\n' +
              log_summary)
    except wss.ClientClosedConnectionError as e:
      raise RuntimeError(
          'Detected closed websocket - likely caused by renderer crash') from e
    except WebGpuMessageTimeoutError as e:
      self.additionalTags[MAY_EXONERATE] = str(
          self._DetermineRetryWorkaround(e))
      raise
    finally:
      cls.total_tests_run += 1

  def GetBrowserTimeoutMultipler(self) -> float:
    """Compute the timeout multiplier to account for overall browser slowness.

    Returns:
      A float.
    """
    # Parallel jobs can cause heavier tests to flakily time out, so increase the
    # timeout based on the number of parallel jobs. 2x the timeout with 4 jobs
    # seemed to work well, so target that.
    # This multiplier will be applied for all message events.
    browser_timeout_multiplier = 1 + (self.child.jobs - 1) / 3.0
    # Scale up all timeouts with ASAN.
    if self._is_asan:
      browser_timeout_multiplier *= ASAN_MULTIPLIER

    return browser_timeout_multiplier

  def GetTestExecutionTimeoutMultiplier(self) -> float:
    """Compute the timeout multiplier to account for slow test execution.

    Returns:
      A float.
    """
    return (self._GetTestExecutionTimeoutMultiplierWithoutSlowMultiplier() *
            self._GetSlowTestMultiplier())

  def _GetTestExecutionTimeoutMultiplierWithoutSlowMultiplier(self) -> float:
    test_execution_timeout_multiplier = 1
    if self._enable_dawn_backend_validation:
      test_execution_timeout_multiplier *= BACKEND_VALIDATION_MULTIPLIER
    return test_execution_timeout_multiplier

  def _GetSlowTestMultiplier(self) -> float:
    if self._IsSlowTest():
      return SLOW_MULTIPLIER
    return 1

  #pylint: disable=too-many-branches
  def HandleMessageLoop(self, first_load) -> WebGpuTestResult:
    """Helper function to handle the loop for the message protocol.

    See //docs/gpu/webgpu_cts_harness_message_protocol.md for more information
    on the message format.

    Args:
      first_load: A bool denoting whether this is the first test run after a
          page load. Increases the timeout for the TEST_STARTED step.

    Returns:
      A filled WebGpuTestResult instance.
    """
    result = WebGpuTestResult()
    message_state = {
        MESSAGE_TYPE_TEST_STARTED: False,
        MESSAGE_TYPE_TEST_STATUS: False,
        MESSAGE_TYPE_TEST_LOG: False,
    }
    step_timeout = MESSAGE_TIMEOUT_TEST_STARTED
    if first_load:
      step_timeout *= FIRST_LOAD_TEST_STARTED_MULTIPLER

    browser_timeout_multiplier = self.GetBrowserTimeoutMultipler()
    test_execution_timeout_multiplier = self.GetTestExecutionTimeoutMultiplier()

    global_timeout = (self._test_timeout * test_execution_timeout_multiplier *
                      browser_timeout_multiplier)
    self._global_timeout_without_slow_multiplier = (
        self._test_timeout *
        self._GetTestExecutionTimeoutMultiplierWithoutSlowMultiplier() *
        browser_timeout_multiplier)
    start_time = time.time()

    # Loop until one of the following happens:
    #   * The test sends MESSAGE_TYPE_TEST_FINISHED, in which case we report the
    #     result normally.
    #   * We hit the timeout for waiting for a message, e.g. if the JavaScript
    #     code fails to send a heartbeat, in which case we report a timeout.
    #   * We hit the global timeout, e.g. if the JavaScript code somehow got
    #     stuck in a loop, in which case we report a timeout.
    #   * We receive a message in the wrong order or an unknown message, in
    #     which case we report a message protocol error.
    while True:
      timeout = step_timeout * browser_timeout_multiplier
      self._heartbeat_timeout = timeout
      try:
        response_start_time = time.time()
        response = self.__class__.websocket_server.Receive(timeout)
        self._longest_time_between_heartbeats = max(
            self._longest_time_between_heartbeats,
            time.time() - response_start_time)
        response = json.loads(response)
        response_type = response['type']

        if time.time() - start_time > global_timeout:
          self.HandleDurationTagOnFailure(message_state, global_timeout)
          raise WebGpuTestTimeoutError(
              '%s hit %.3f second global timeout. Message state: %s' %
              (self._query, global_timeout, message_state))

        if response_type == MESSAGE_TYPE_INFRA_FAILURE:
          self.fail(response['message'])

        if response_type == MESSAGE_TYPE_TEST_STARTED:
          # If we ever want the adapter information from WebGPU, we would
          # retrieve it from the message here. However, to avoid pylint
          # complaining about unused variables, don't grab it until we actually
          # need it.
          VerifyMessageOrderTestStarted(message_state)
          step_timeout = (MESSAGE_TIMEOUT_HEARTBEAT *
                          test_execution_timeout_multiplier)

        elif response_type == MESSAGE_TYPE_TEST_HEARTBEAT:
          VerifyMessageOrderTestHeartbeat(message_state)
          continue

        elif response_type == MESSAGE_TYPE_TEST_STATUS:
          VerifyMessageOrderTestStatus(message_state)
          result.status = response['status']
          js_duration = response['js_duration_ms'] / 1000
          # Specify the precision to avoid scientific notation. Nanoseconds
          # should be more precision than we need anyways.
          self.additionalTags[JAVASCRIPT_DURATION] = '%.9fs' % js_duration
          step_timeout = MESSAGE_TIMEOUT_TEST_LOG

        elif response_type == MESSAGE_TYPE_TEST_LOG:
          VerifyMessageOrderTestLog(message_state)
          result.log_pieces.append(response['log'])

        elif response_type == MESSAGE_TYPE_TEST_FINISHED:
          VerifyMessageOrderTestFinished(message_state)
          break

        else:
          raise WebGpuMessageProtocolError(
              '%s received unknown message type %s' % self._query,
              response_type)
      except wss.WebsocketReceiveMessageTimeoutError as e:
        self.HandleDurationTagOnFailure(message_state, global_timeout)
        raise WebGpuMessageTimeoutError(
            '%s timed out waiting %.3f seconds for a message. Message state: %s'
            % (self._query, timeout, message_state)) from e
      finally:
        self._test_duration = time.time() - start_time
    return result
  # pylint: enable=too-many-branches

  def HandleDurationTagOnFailure(self, message_state: Dict[str, bool],
                                 test_timeout: float) -> None:
    """Handles setting the JAVASCRIPT_DURATION tag on failure.

    Args:
      message_state: A map from message type to a boolean denoting whether a
          message of that type has been received before.
      test_timeout: A float denoting the number of seconds to wait for the test
          to finish before timing out.
    """
    # Report the max timeout if the JavaScript code actually timed out (i.e. we
    # were between TEST_STARTED and TEST_STATUS), otherwise don't modify
    # anything.
    if (message_state[MESSAGE_TYPE_TEST_STARTED]
        and not message_state[MESSAGE_TYPE_TEST_STATUS]
        and JAVASCRIPT_DURATION not in self.additionalTags):
      self.additionalTags[JAVASCRIPT_DURATION] = '%.9fs' % test_timeout

  def _NavigateIfNecessary(self, path: str) -> bool:
    cls = self.__class__
    if cls.page_loaded:
      return False
    cls.websocket_server.ClearCurrentConnection()
    url = self.UrlOfStaticFilePath(path)
    self.tab.Navigate(url)
    self.tab.action_runner.WaitForJavaScriptCondition(
        'window.setupWebsocket != undefined')
    self.tab.action_runner.ExecuteJavaScript('window.setupWebsocket("%s")' %
                                             cls.websocket_server.server_port)
    timeout_multiplier = 1
    if not cls.attempted_websocket_connection:
      cls.attempted_websocket_connection = True
      timeout_multiplier = 2
    cls.websocket_server.WaitForConnection(
        timeout_multiplier * wsu.GetScaledConnectionTimeout(self.child.jobs))

    # Wait for the page to set up the websocket.
    response = cls.websocket_server.Receive(MESSAGE_TIMEOUT_CONNECTION_ACK)
    assert json.loads(response)['type'] == MESSAGE_TYPE_CONNECTION_ACK

    cls.page_loaded = True
    return True

  def _IsSlowTest(self) -> bool:
    # We access the expectations directly instead of using
    # self.GetExpectationsForTest since we need the raw results, but that method
    # only returns the parsed results and whether the test should be retried.
    expectation = self._GetSlowTests().expectations_for(self._query)
    return 'Slow' in expectation.raw_results

  @classmethod
  def GetPlatformTags(cls, browser: ct.Browser) -> List[str]:
    tags = super().GetPlatformTags(browser)
    if cls._enable_dawn_backend_validation:
      tags.append('dawn-backend-validation')
    else:
      tags.append('dawn-no-backend-validation')
    if cls._use_webgpu_adapter:
      tags.append('webgpu-adapter-' + cls._use_webgpu_adapter)
    else:
      tags.append('webgpu-adapter-default')

    if host_information.IsWindows():
      if cls._use_fxc:
        tags.append('webgpu-dxc-disabled')
      else:
        tags.append('webgpu-dxc-enabled')

    tags.append(_GetWorkerTag(cls._worker_type))

    # No need to tag _use_webgpu_power_preference here,
    # since Telemetry already reports the GPU vendorID

    return tags

  @classmethod
  def GetExpectationsFilesRepoPath(cls) -> str:
    return os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'third_party', 'dawn')


class WebGpuMessageProtocolError(RuntimeError):
  pass


class WebGpuMessageTimeoutError(RuntimeError):
  pass


class WebGpuTestTimeoutError(RuntimeError):
  pass


def VerifyMessageOrderTestStarted(message_state: Dict[str, bool]) -> None:
  """Helper function to verify that messages are ordered correctly.

  Handles MESSAGE_TYPE_TEST_STARTED messages.

  Split out to reduce the number of branches within a single function.

  Args:
    message_state: A map from message type to a boolean denoting whether a
        message of that type has been received before.
  """
  if message_state[MESSAGE_TYPE_TEST_STARTED]:
    raise WebGpuMessageProtocolError(
        'Received multiple start messages for one test')
  message_state[MESSAGE_TYPE_TEST_STARTED] = True


def VerifyMessageOrderTestHeartbeat(message_state: Dict[str, bool]) -> None:
  """Helper function to verify that messages are ordered correctly.

  Handles MESSAGE_TYPE_TEST_HEARTBEAT messages.

  Split out to reduce the number of branches within a single function.

  Args:
    message_state: A map from message type to a boolean denoting whether a
        message of that type has been received before.
  """
  if not message_state[MESSAGE_TYPE_TEST_STARTED]:
    raise WebGpuMessageProtocolError('Received heartbeat before test start')
  if message_state[MESSAGE_TYPE_TEST_STATUS]:
    raise WebGpuMessageProtocolError(
        'Received heartbeat after test supposedly done')


def VerifyMessageOrderTestStatus(message_state: Dict[str, bool]) -> None:
  """Helper function to verify that messages are ordered correctly.

  Handles MESSAGE_TYPE_TEST_STATUS messages.

  Split out to reduce the number of branches within a single function.

  Args:
    message_state: A map from message type to a boolean denoting whether a
        message of that type has been received before.
  """
  if not message_state[MESSAGE_TYPE_TEST_STARTED]:
    raise WebGpuMessageProtocolError(
        'Received test status message before test start')
  if message_state[MESSAGE_TYPE_TEST_STATUS]:
    raise WebGpuMessageProtocolError(
        'Received multiple status messages for one test')
  message_state[MESSAGE_TYPE_TEST_STATUS] = True


def VerifyMessageOrderTestLog(message_state: Dict[str, bool]) -> None:
  """Helper function to verify that messages are ordered correctly.

  Handles MESSAGE_TYPE_TEST_LOG messages.

  Split out to reduce the number of branches within a single function.

  Args:
    message_state: A map from message type to a boolean denoting whether a
        message of that type has been received before.
  """
  if not message_state[MESSAGE_TYPE_TEST_STATUS]:
    raise WebGpuMessageProtocolError(
        'Received log message before status message')
  message_state[MESSAGE_TYPE_TEST_LOG] = True


def VerifyMessageOrderTestFinished(message_state: Dict[str, bool]) -> None:
  """Helper function to verify that messages are ordered correctly.

  Handles MESSAGE_TYPE_TEST_FINISHED messages.

  Split out to reduce the number of branches within a single function.

  Args:
    message_state: A map from message type to a boolean denoting whether a
        message of that type has been received before.
  """
  if not message_state[MESSAGE_TYPE_TEST_LOG]:
    raise WebGpuMessageProtocolError(
        'Received finish message before log message')


def _GetWorkerTag(worker_type: WorkerType) -> str:
  """Helper function to get worker type tags."""
  if worker_type == WorkerType.NONE:
    return 'webgpu-no-worker'
  if worker_type == WorkerType.SERVICE:
    return 'webgpu-service-worker'
  if worker_type == WorkerType.DEDICATED:
    return 'webgpu-dedicated-worker'
  if worker_type == WorkerType.SHARED:
    return 'webgpu-shared-worker'
  raise RuntimeError(f'Unhandled worker type {worker_type.name}')
