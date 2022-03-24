# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import asyncio
import fnmatch
import json
import logging
import os
import subprocess
import sys
import threading

import websockets  # pylint:disable=import-error

from gpu_tests import gpu_integration_test

import gpu_path_util

EXPECTATIONS_FILE = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'third_party',
                                 'dawn', 'webgpu-cts', 'expectations.txt')
LIST_SCRIPT = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'third_party',
                           'dawn', 'webgpu-cts', 'scripts', 'list.py')
TYPESCRIPT_DIR = os.path.join(gpu_path_util.GPU_DIR, '.webgpu_typescript')

TEST_RUNS_BETWEEN_CLEANUP = 1000
WEBSOCKET_PORT_TIMEOUT_SECONDS = 10
WEBSOCKET_SETUP_TIMEOUT_SECONDS = 5
DEFAULT_TEST_TIMEOUT = 5
SLOW_MULTIPLIER = 5

# TODO: Switch this to reading from a file in the Dawn repo so that Dawn
# contributors can update this without a full Chromium checkout.
# Tests that should be run in a worker in addition to normally.
WORKER_TEST_GLOBS = [
    'webgpu:api,operation,buffers,map:mapAsync,write:*',
    'webgpu:api,operation,buffers,map:mapAsync,read:*',
    'webgpu:api,operation,buffers,map:mapAsync,read,typedArrayAccess:*',
    'webgpu:api,operation,buffers,map:mappedAtCreation:*',
    'webgpu:api,operation,buffers,map:remapped_for_write:*',
    'webgpu:api,operation,buffers,map_detach:while_mapped:*',
    'webgpu:api,operation,command_buffer,basic:*',
    'webgpu:api,operation,command_buffer,copyBufferToBuffer:*',
    'webgpu:api,operation,compute,basic:memcpy:*',
    'webgpu:api,operation,compute,basic:large_dispatch:*',
    'webgpu:api,operation,rendering,basic:clear:*',
    'webgpu:api,operation,rendering,basic:fullscreen_quad:*',
    'webgpu:api,operation,rendering,basic:large_draw:*',
    'webgpu:api,operation,render_pass,storeOp:*',
    'webgpu:api,operation,render_pass,storeop2:*',
    'webgpu:api,operation,onSubmittedWorkDone:*',
    'webgpu:api,validation,buffer,destroy:*',
    'webgpu:api,validation,buffer,mapping:*',
]

HTML_FILENAME = os.path.join('gen', 'third_party', 'dawn', 'webgpu-cts',
                             'test_page.html')


async def StartWebsocketServer():
  async def HandleWebsocketConnection(websocket):
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
  def run(self):
    try:
      asyncio.run(StartWebsocketServer())
    except Exception as e:  # pylint:disable=broad-except
      sys.stdout.write('Server thread had exception: %s\n' % e)


class WebGpuCtsIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  # Whether the test page has already been loaded. Caching this state here is
  # faster than checking the URL every time, and given how fast these tests are,
  # additional overhead like that can add up quickly.
  _page_loaded = False

  _test_timeout = DEFAULT_TEST_TIMEOUT
  _is_backend_validation = False

  _build_dir = None

  _test_list = None

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
  def ShouldPerformMinidumpCleanupOnSetUp(self):
    return self.total_tests_run % TEST_RUNS_BETWEEN_CLEANUP == 0

  def ShouldPerformMinidumpCleanupOnTearDown(self):
    return self.ShouldPerformMinidumpCleanupOnSetUp()

  @classmethod
  def Name(cls):
    return 'webgpu_cts'

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(WebGpuCtsIntegrationTest, cls).AddCommandlineArgs(parser)
    parser.add_option('--override-timeout',
                      type=float,
                      help='Override the test timeout in seconds')
    parser.add_option('--is-backend-validation',
                      action='store_true',
                      default=False,
                      help=('Signals that the tests are being run with backend '
                            'validation enabled'))

  @classmethod
  def StartBrowser(cls):
    cls._page_loaded = False
    super(WebGpuCtsIntegrationTest, cls).StartBrowser()

  @classmethod
  def SetUpWebsocketServer(cls):
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
  def SetUpProcess(cls):
    super(WebGpuCtsIntegrationTest, cls).SetUpProcess()
    cls.SetUpWebsocketServer()
    browser_args = [
        '--enable-unsafe-webgpu',
        '--disable-dawn-features=disallow_unsafe_apis',
    ]
    if cls._is_backend_validation:
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
        cls._build_dir,
    ])

  @classmethod
  def TearDownWebsocketServer(cls):
    if cls.connection_stopper:
      cls.connection_stopper.cancel()
    if cls.server_stopper:
      cls.server_stopper.cancel()
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
  def TearDownProcess(cls):
    cls.TearDownWebsocketServer()
    super(WebGpuCtsIntegrationTest, cls).TearDownProcess()

  @classmethod
  def GenerateGpuTests(cls, options):
    if options.override_timeout:
      cls._test_timeout = options.override_timeout
    cls._is_backend_validation = options.is_backend_validation
    if cls._test_list is None:
      p = subprocess.run(
          [sys.executable, LIST_SCRIPT, '--js-out-dir', TYPESCRIPT_DIR],
          stdout=subprocess.PIPE,
          check=True)
      cls._test_list = p.stdout.decode('utf-8').splitlines()
    for line in cls._test_list:  # pylint:disable=not-an-iterable
      if not line:
        continue
      test_inputs = (line, False)
      for wg in WORKER_TEST_GLOBS:
        if fnmatch.fnmatch(line, wg):
          yield (TestNameFromInputs(*test_inputs), HTML_FILENAME, test_inputs)
          test_inputs = (line, True)
          yield (TestNameFromInputs(*test_inputs), HTML_FILENAME, test_inputs)
          break
      else:
        yield (TestNameFromInputs(*test_inputs), HTML_FILENAME, test_inputs)

  def RunActualGpuTest(self, test_path, *args):
    try:
      self._query, self._run_in_worker = args
      self._NavigateIfNecessary(test_path)
      asyncio.run_coroutine_threadsafe(
          WebGpuCtsIntegrationTest.websocket.send(
              json.dumps({
                  'q': self._query,
                  'w': self._run_in_worker
              })), WebGpuCtsIntegrationTest.event_loop)
      future = asyncio.run_coroutine_threadsafe(
          asyncio.wait_for(WebGpuCtsIntegrationTest.websocket.recv(),
                           self._GetTestTimeout()),
          WebGpuCtsIntegrationTest.event_loop)
      response = future.result()
      response = json.loads(response)
      status = response['s']
      logs = response['l']
      if isinstance(logs, list):
        log_str = '\n'.join(logs)
      else:
        log_str = logs
      if status == 'skip':
        self.skipTest('WebGPU CTS JavaScript reported test skip with logs ' +
                      log_str)
      elif status == 'fail':
        self.fail(log_str)
    finally:
      WebGpuCtsIntegrationTest.total_tests_run += 1

  @classmethod
  def CleanUpExistingWebsocket(cls):
    if cls.connection_stopper:
      cls.connection_stopper.cancel()
    cls.connection_stopper = None
    cls.websocket = None
    cls.connection_received_event.clear()

  def _NavigateIfNecessary(self, path):
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

  def _IsSlowTest(self):
    # We access the expectations directly instead of using
    # self.GetExpectationsForTest since we need the raw results, but that method
    # only returns the parsed results and whether the test should be retried.
    expectation = self.child.expectations.expectations_for(
        TestNameFromInputs(self._query, self._run_in_worker))
    return 'Slow' in expectation.raw_results

  def _GetTestTimeout(self):
    timeout = (self._test_timeout *
               SLOW_MULTIPLIER if self._IsSlowTest() else self._test_timeout)
    return timeout

  @classmethod
  def GetPlatformTags(cls, browser):
    tags = super(WebGpuCtsIntegrationTest, cls).GetPlatformTags(browser)
    if cls._is_backend_validation:
      tags.append('dawn-backend-validation')
    else:
      tags.append('dawn-no-backend-validation')
    return tags

  @classmethod
  def ExpectationsFiles(cls):
    return [EXPECTATIONS_FILE]


def TestNameFromInputs(query, worker):
  return 'worker_%s' % query if worker else query


def load_tests(_loader, _tests, _pattern):
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
