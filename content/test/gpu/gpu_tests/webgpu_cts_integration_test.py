# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import asyncio
import fnmatch
import json
import logging
import os
import sys
import threading
from typing import Any, Dict, List, Set
import unittest

import websockets  # pylint:disable=import-error
import websockets.server as ws_server  # pylint: disable=import-error

from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test

import gpu_path_util

EXPECTATIONS_FILE = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'third_party',
                                 'dawn', 'webgpu-cts', 'expectations.txt')
TEST_LIST_FILE = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'third_party',
                              'dawn', 'third_party', 'gn', 'webgpu-cts',
                              'test_list.txt')
WORKER_TEST_GLOB_FILE = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR,
                                     'third_party', 'dawn', 'webgpu-cts',
                                     'worker_test_globs.txt')

TEST_RUNS_BETWEEN_CLEANUP = 1000
WEBSOCKET_PORT_TIMEOUT_SECONDS = 10
WEBSOCKET_SETUP_TIMEOUT_SECONDS = 5
DEFAULT_TEST_TIMEOUT = 5
SLOW_MULTIPLIER = 5
ASAN_MULTIPLIER = 4
BACKEND_VALIDATION_MULTIPLIER = 6

# In most cases, this should be very fast, but the first test run after a page
# load can be slow.
MESSAGE_TIMEOUT_TEST_STARTED = 10
MESSAGE_TIMEOUT_TEST_LOG = 0.5

HTML_FILENAME = os.path.join('webgpu-cts', 'test_page.html')

JAVASCRIPT_DURATION = 'javascript_duration'
MESSAGE_TYPE_TEST_STARTED = 'TEST_STARTED'
MESSAGE_TYPE_TEST_HEARTBEAT = 'TEST_HEARTBEAT'
MESSAGE_TYPE_TEST_STATUS = 'TEST_STATUS'
MESSAGE_TYPE_TEST_LOG = 'TEST_LOG'
MESSAGE_TYPE_TEST_FINISHED = 'TEST_FINISHED'


class WebGpuTestResult():
  """Struct-like object for holding a single test result."""

  def __init__(self):
    self.status = None
    self.log_pieces = []


async def StartWebsocketServer() -> None:
  async def HandleWebsocketConnection(
      websocket: ws_server.WebSocketServerProtocol) -> None:
    # We only allow one active connection - if there are multiple, something is
    # wrong.
    assert WebGpuCtsIntegrationTest.connection_stopper is None
    assert WebGpuCtsIntegrationTest.websocket is None
    WebGpuCtsIntegrationTest.connection_stopper = asyncio.Future()
    WebGpuCtsIntegrationTest.websocket = websocket
    WebGpuCtsIntegrationTest.connection_received_event.set()
    await WebGpuCtsIntegrationTest.connection_stopper

  async with websockets.serve(HandleWebsocketConnection, '127.0.0.1',
                              0) as server:
    WebGpuCtsIntegrationTest.event_loop = asyncio.get_running_loop()
    WebGpuCtsIntegrationTest.server_port = server.sockets[0].getsockname()[1]
    WebGpuCtsIntegrationTest.port_set_event.set()
    WebGpuCtsIntegrationTest.server_stopper = asyncio.Future()
    await WebGpuCtsIntegrationTest.server_stopper


class ServerThread(threading.Thread):
  def run(self) -> None:
    try:
      asyncio.run(StartWebsocketServer())
    except asyncio.CancelledError:
      pass
    except Exception as e:  # pylint:disable=broad-except
      sys.stdout.write('Server thread had exception: %s\n' % e)


class WebGpuCtsIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  # Whether the test page has already been loaded. Caching this state here is
  # faster than checking the URL every time, and given how fast these tests are,
  # additional overhead like that can add up quickly.
  _page_loaded = False

  _test_timeout = DEFAULT_TEST_TIMEOUT
  _is_asan = False
  _enable_dawn_backend_validation = False
  _use_webgpu_adapter = None  # use the default

  _build_dir = None

  _test_list = None
  _worker_test_globs = None

  total_tests_run = 0

  server_stopper = None
  connection_stopper = None
  server_port = None
  websocket = None
  port_set_event = None
  connection_received_event = None
  event_loop = None
  _server_thread = None

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self._query = None
    self._run_in_worker = False

  # Only perform the pre/post test cleanup every X tests instead of every test
  # to reduce overhead.
  def ShouldPerformMinidumpCleanupOnSetUp(self) -> bool:
    return self.total_tests_run % TEST_RUNS_BETWEEN_CLEANUP == 0

  def ShouldPerformMinidumpCleanupOnTearDown(self) -> bool:
    return self.ShouldPerformMinidumpCleanupOnSetUp()

  @classmethod
  def Name(cls) -> str:
    return 'webgpu_cts'

  def _SuiteSupportsParallelTests(self) -> bool:
    return True

  def _GetSerialGlobs(self) -> Set[str]:
    return set()

  def _GetSerialTests(self) -> Set[str]:
    return set()

  @classmethod
  def AddCommandlineArgs(cls, parser: ct.CmdArgParser) -> None:
    super(WebGpuCtsIntegrationTest, cls).AddCommandlineArgs(parser)
    parser.add_option('--override-timeout',
                      type=float,
                      help='Override the test timeout in seconds')
    parser.add_option(
        '--enable-dawn-backend-validation',
        action='store_true',
        default=False,
        help=('Runs the browser with Dawn backend validation enabled'))
    parser.add_option(
        '--use-webgpu-adapter',
        type=str,
        default=None,
        help=('Runs the browser with a particular WebGPU adapter'))

  @classmethod
  def StartBrowser(cls) -> None:
    cls._page_loaded = False
    super(WebGpuCtsIntegrationTest, cls).StartBrowser()

  @classmethod
  def SetUpWebsocketServer(cls) -> None:
    cls.port_set_event = threading.Event()
    cls.connection_received_event = threading.Event()
    cls._server_thread = ServerThread()
    # Mark as a daemon so that the harness does not hang when shutting down if
    # the thread fails to shut down properly.
    cls._server_thread.daemon = True
    cls._server_thread.start()
    got_port = WebGpuCtsIntegrationTest.port_set_event.wait(
        WEBSOCKET_PORT_TIMEOUT_SECONDS)
    if not got_port:
      raise RuntimeError('Server did not provide a port.')

  @classmethod
  def SetUpProcess(cls) -> None:
    super(WebGpuCtsIntegrationTest, cls).SetUpProcess()

    cls.SetUpWebsocketServer()
    browser_args = [
        '--enable-unsafe-webgpu',
        '--disable-dawn-features=disallow_unsafe_apis',
        # When running tests in parallel, windows can be treated as occluded if
        # a newly opened window fully covers a previous one, which can cause
        # issues in a few tests. This is practically only an issue on Windows
        # since Linux/Mac stagger new windows, but pass in on all platforms
        # since it could technically be hit on any platform.
        '--disable-backgrounding-occluded-windows',
    ]
    if cls._use_webgpu_adapter:
      browser_args.append('--use-webgpu-adapter=%s' % cls._use_webgpu_adapter)
    if cls._enable_dawn_backend_validation:
      if sys.platform == 'win32':
        browser_args.append('--enable-dawn-backend-validation=partial')
      else:
        browser_args.append('--enable-dawn-backend-validation')
    cls.CustomizeBrowserArgs(browser_args)
    cls.StartBrowser()
    # pylint:disable=protected-access
    cls._build_dir = cls.browser._browser_backend.build_dir
    # pylint:enable=protected-access
    cls.SetStaticServerDirs([
        os.path.join(cls._build_dir, 'gen', 'third_party', 'dawn'),
    ])

  @classmethod
  def TearDownWebsocketServer(cls) -> None:
    if cls.connection_stopper:
      cls.connection_stopper.cancel()
      try:
        cls.connection_stopper.exception()
      except asyncio.CancelledError:
        pass
    if cls.server_stopper:
      cls.server_stopper.cancel()
      try:
        cls.server_stopper.exception()
      except asyncio.CancelledError:
        pass
    cls.server_stopper = None
    cls.connection_stopper = None
    cls.server_port = None
    cls.websocket = None

    cls._server_thread.join(5)
    if cls._server_thread.is_alive():
      logging.error(
          'WebSocket server did not shut down properly - this might be '
          'indicative of an issue in the test harness')

  @classmethod
  def TearDownProcess(cls) -> None:
    cls.TearDownWebsocketServer()
    super(WebGpuCtsIntegrationTest, cls).TearDownProcess()

  @classmethod
  def _SetClassVariablesFromOptions(cls, options: ct.ParsedCmdArgs) -> None:
    if options.override_timeout:
      cls._test_timeout = options.override_timeout
    cls._enable_dawn_backend_validation = options.enable_dawn_backend_validation
    cls._use_webgpu_adapter = options.use_webgpu_adapter

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    cls._SetClassVariablesFromOptions(options)
    if cls._test_list is None:
      with open(TEST_LIST_FILE) as f:
        cls._test_list = [l for l in f.read().splitlines() if l]
    if cls._worker_test_globs is None:
      with open(WORKER_TEST_GLOB_FILE) as f:
        contents = f.read()
      cls._worker_test_globs = [l for l in contents.splitlines() if l]
    for line in cls._test_list:  # pylint:disable=not-an-iterable
      test_inputs = [line, False]
      for wg in cls._worker_test_globs:  # pylint:disable=not-an-iterable
        if fnmatch.fnmatch(line, wg):
          yield (TestNameFromInputs(*test_inputs), HTML_FILENAME, test_inputs)
          test_inputs = [line, True]
          yield (TestNameFromInputs(*test_inputs), HTML_FILENAME, test_inputs)
          break
      else:
        yield (TestNameFromInputs(*test_inputs), HTML_FILENAME, test_inputs)

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    self._query, self._run_in_worker = args
    # Only a single instance is used to run tests despite a number of instances
    # (~2x the number of total tests) being initialized, so make sure to clear
    # this state so we don't accidentally keep it around from a previous test.
    if JAVASCRIPT_DURATION in self.additionalTags:
      del self.additionalTags[JAVASCRIPT_DURATION]

    timeout = self._GetTestTimeout()

    try:
      self._NavigateIfNecessary(test_path)
      asyncio.run_coroutine_threadsafe(
          WebGpuCtsIntegrationTest.websocket.send(
              json.dumps({
                  'q': self._query,
                  'w': self._run_in_worker
              })), WebGpuCtsIntegrationTest.event_loop)
      result = self.HandleMessageLoop(timeout)

      log_str = ''.join(result.log_pieces)
      status = result.status
      if status == 'skip':
        self.skipTest('WebGPU CTS JavaScript reported test skip with logs ' +
                      log_str)
      elif status == 'fail':
        self.fail(log_str)
    except websockets.exceptions.ConnectionClosedOK as e:
      raise RuntimeError(
          'Detected closed websocket - likely caused by renderer crash') from e
    finally:
      WebGpuCtsIntegrationTest.total_tests_run += 1

  def HandleMessageLoop(self, test_timeout: float) -> WebGpuTestResult:
    """Helper function to handle the loop for the message protocol.

    See //docs/gpu/webgpu_cts_harness_message_protocol.md for more information
    on the message format.

    TODO(crbug.com/1340602): Update this to be the total test timeout once the
    heartbeat mechanism is implemented.

    Args:
      test_timeout: A float denoting the number of seconds to wait for the test
          to finish before timing out.

    Returns:
      A filled WebGpuTestResult instance.
    """
    result = WebGpuTestResult()
    message_state = {
        MESSAGE_TYPE_TEST_STARTED: False,
        MESSAGE_TYPE_TEST_STATUS: False,
        MESSAGE_TYPE_TEST_LOG: False,
    }
    timeout = MESSAGE_TIMEOUT_TEST_STARTED
    # Loop until we receive a message saying that the test is finished. This
    # currently has no practical effect, but it is an intermediate step to
    # supporting a heartbeat mechanism. See crbug.com/1340602.
    try:
      while True:
        future = asyncio.run_coroutine_threadsafe(
            asyncio.wait_for(WebGpuCtsIntegrationTest.websocket.recv(),
                             timeout), WebGpuCtsIntegrationTest.event_loop)
        response = future.result()
        response = json.loads(response)
        response_type = response['type']

        if response_type == MESSAGE_TYPE_TEST_STARTED:
          # If we ever want the adapter information from WebGPU, we would
          # retrieve it from the message here. However, to avoid pylint
          # complaining about unused variables, don't grab it until we actually
          # need it.
          VerifyMessageOrderTestStarted(message_state)
          timeout = test_timeout

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
          timeout = MESSAGE_TIMEOUT_TEST_LOG

        elif response_type == MESSAGE_TYPE_TEST_LOG:
          VerifyMessageOrderTestLog(message_state)
          result.log_pieces.append(response['log'])

        elif response_type == MESSAGE_TYPE_TEST_FINISHED:
          VerifyMessageOrderTestFinished(message_state)
          break

        else:
          raise WebGpuMessageProtocolError('Received unknown message type %s' %
                                           response_type)
    except asyncio.TimeoutError as e:
      # Report the max timeout if the JavaScript code actually timed out (i.e.
      # we were between TEST_STARTED and TEST_STATUS), otherwise don't modify
      # anything.
      if (message_state[MESSAGE_TYPE_TEST_STARTED]
          and not message_state[MESSAGE_TYPE_TEST_STATUS]
          and JAVASCRIPT_DURATION not in self.additionalTags):
        self.additionalTags[JAVASCRIPT_DURATION] = '%.9fs' % test_timeout
      raise WebGpuTimeoutError(
          'Timed out waiting %.3f seconds for a message. Message state: %s' %
          (timeout, message_state)) from e
    return result

  @classmethod
  def CleanUpExistingWebsocket(cls) -> None:
    if cls.connection_stopper:
      cls.connection_stopper.cancel()
      try:
        cls.connection_stopper.exception()
      except asyncio.CancelledError:
        pass
    cls.connection_stopper = None
    cls.websocket = None
    cls.connection_received_event.clear()

  def _NavigateIfNecessary(self, path: str) -> None:
    if WebGpuCtsIntegrationTest._page_loaded:
      return
    WebGpuCtsIntegrationTest.CleanUpExistingWebsocket()
    url = self.UrlOfStaticFilePath(path)
    self.tab.Navigate(url)
    self.tab.action_runner.WaitForJavaScriptCondition(
        'window.setupWebsocket != undefined')
    self.tab.action_runner.ExecuteJavaScript(
        'window.setupWebsocket("%s")' % WebGpuCtsIntegrationTest.server_port)
    WebGpuCtsIntegrationTest.connection_received_event.wait(
        WEBSOCKET_SETUP_TIMEOUT_SECONDS)
    if not WebGpuCtsIntegrationTest.websocket:
      raise RuntimeError('Websocket connection was not established.')
    WebGpuCtsIntegrationTest._page_loaded = True

  def _IsSlowTest(self) -> bool:
    # We access the expectations directly instead of using
    # self.GetExpectationsForTest since we need the raw results, but that method
    # only returns the parsed results and whether the test should be retried.
    expectation = self.child.expectations.expectations_for(
        TestNameFromInputs(self._query, self._run_in_worker))
    return 'Slow' in expectation.raw_results

  def _GetTestTimeout(self) -> int:
    timeout = self._test_timeout
    # Parallel jobs can cause heavier tests to flakily time out, so increase the
    # timeout based on the number of parallel jobs. 2x the timeout with 4 jobs
    # seemed to work well, so target that.
    timeout *= 1 + (self.child.jobs - 1) / 3.0

    if self._IsSlowTest():
      timeout *= SLOW_MULTIPLIER
    if self._is_asan:
      timeout *= ASAN_MULTIPLIER
    if self._enable_dawn_backend_validation:
      timeout *= BACKEND_VALIDATION_MULTIPLIER

    return int(timeout)

  @classmethod
  def GetPlatformTags(cls, browser: ct.Browser) -> List[str]:
    tags = super(WebGpuCtsIntegrationTest, cls).GetPlatformTags(browser)
    if cls._enable_dawn_backend_validation:
      tags.append('dawn-backend-validation')
    else:
      tags.append('dawn-no-backend-validation')
    if cls._use_webgpu_adapter:
      tags.append('webgpu-adapter-' + cls._use_webgpu_adapter)
    else:
      tags.append('webgpu-adapter-default')

    system_info = browser.GetSystemInfo()
    if system_info:
      cls._is_asan = system_info.gpu.aux_attributes.get('is_asan', False)

    return tags

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [EXPECTATIONS_FILE]


class WebGpuMessageProtocolError(RuntimeError):
  pass


class WebGpuTimeoutError(RuntimeError):
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


def TestNameFromInputs(query: str, worker: bool) -> str:
  return 'worker_%s' % query if worker else query


def load_tests(_loader: unittest.TestLoader, _tests: Any,
               _pattern: Any) -> unittest.TestSuite:
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
