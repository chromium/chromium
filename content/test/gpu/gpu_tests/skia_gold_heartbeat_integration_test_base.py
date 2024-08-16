# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import time
from typing import Any, List, Optional

import bs4  # pylint: disable=import-error

import dataclasses  # Built-in, but pylint gives an ordering false positive.

from gpu_tests import common_typing as ct
from gpu_tests import skia_gold_integration_test_base as sgitb
from gpu_tests import webgl_test_util
from gpu_tests.util import websocket_server as wss
from gpu_tests.util import websocket_utils

import gpu_path_util

TEST_PAGE_RELPATH = os.path.join(webgl_test_util.extensions_relpath,
                                 'pixel_test_page.html')

DEFAULT_HEARTBEAT_TIMEOUT = 15
SLOW_HEARTBEAT_MULTIPLIER = 8
DEFAULT_CONTROLLER_MESSAGE_TIMEOUT = 5
SLOW_CONTROLLER_MESSAGE_MULTIPLIER = 5

DEFAULT_GLOBAL_TIMEOUT = 300


@dataclasses.dataclass
class LoopState:
  """Stores the state of the websocket heartbeat loop.

  Used to allow nested loopes, e.g. during pixel test page actions.
  """
  test_started: bool = False
  test_finished: bool = False


@dataclasses.dataclass
class TabData:
  """Stores all the components for interacting with a tab."""
  tab: ct.Tab
  websocket_server: wss.WebsocketServer
  is_default_tab: bool = False


class TestAction():
  """Defines some action to run before capturing a screenshot.

  Tests are able to define custom lists of actions if additional steps are
  necessary to run the test after loading the test page.
  """
  def Run(self, test_case: 'SkiaGoldHeartbeatTestCase', tab_data: TabData,
          loop_state: LoopState,
          test_instance: 'SkiaGoldHeartbeatIntegrationTestBase') -> None:
    raise NotImplementedError()


class _TestActionHandleMessageLoop(TestAction):
  def __init__(self, timeout: float):
    super().__init__()
    self.timeout = timeout

  def Run(self, test_case: 'SkiaGoldHeartbeatTestCase', tab_data: TabData,
          loop_state: LoopState,
          test_instance: 'SkiaGoldHeartbeatIntegrationTestBase') -> None:
    test_instance.HandleMessageLoop(self.timeout, tab_data, loop_state)


class TestActionWaitForContinue(_TestActionHandleMessageLoop):
  """Handles the heartbeat message loop and waits for a CONTINUE signal."""
  def Run(self, test_case: 'SkiaGoldHeartbeatTestCase', tab_data: TabData,
          loop_state: LoopState,
          test_instance: 'SkiaGoldHeartbeatIntegrationTestBase') -> None:
    super().Run(test_case, tab_data, loop_state, test_instance)
    test_instance.assertFalse(loop_state.test_finished)


class TestActionWaitForFinish(_TestActionHandleMessageLoop):
  """Handles the heartbeat message loop and waits for the test to finish."""
  def Run(self, test_case: 'SkiaGoldHeartbeatTestCase', tab_data: TabData,
          loop_state: LoopState,
          test_instance: 'SkiaGoldHeartbeatIntegrationTestBase') -> None:
    super().Run(test_case, tab_data, loop_state, test_instance)
    test_instance.assertTrue(loop_state.test_finished)


class TestActionRunJavaScript(TestAction):
  """Evaluates the given JavaScript in the test page's iframe."""
  def __init__(self, javascript: str):
    super().__init__()
    self.javascript = javascript

  def Run(self, test_case: 'SkiaGoldHeartbeatTestCase', tab_data: TabData,
          loop_state: LoopState,
          test_instance: 'SkiaGoldHeartbeatIntegrationTestBase') -> None:
    EvalInTestIframe(tab_data.tab, self.javascript)


class TestActionWaitForInnerTestPageLoad(TestAction):
  """Waits for the test page's iframe to completely load.

  Most of the time, this isn't necessary since the test page will notify the
  test harness when it is either done or ready to continue. However, if a test
  needs to be manually started via a JavaScript function, then we need to wait
  for a full page load before doing that.
  """
  def __init__(self, timeout: float = 10):
    super().__init__()
    self.timeout = timeout

  def Run(self, test_case: 'SkiaGoldHeartbeatTestCase', tab_data: TabData,
          loop_state: LoopState,
          test_instance: 'SkiaGoldHeartbeatIntegrationTestBase') -> None:
    tab_data.tab.WaitForJavaScriptCondition('testIframeLoaded',
                                            timeout=self.timeout)


class SkiaGoldHeartbeatTestCase(sgitb.SkiaGoldTestCase):
  def __init__(self,
               name: str,
               *args,
               test_actions: Optional[List[TestAction]] = None,
               **kwargs):
    super().__init__(name, *args, **kwargs)
    if test_actions:
      self.test_actions = test_actions
      self.used_custom_test_actions = True
    else:
      self.test_actions = [TestActionWaitForFinish(DEFAULT_GLOBAL_TIMEOUT)]
      self.used_custom_test_actions = False


class SkiaGoldHeartbeatIntegrationTestBase(sgitb.SkiaGoldIntegrationTestBase):
  """Base class for tests that upload results to Skia Gold and use heartbeats.

  Tests are run using a fixed test page that the actual test page is loaded into
  using an iframe.
  """

  websocket_server: Optional[wss.WebsocketServer] = None
  page_loaded = False
  reload_page_for_next_navigation = False

  @classmethod
  def SetUpProcess(cls) -> None:
    super().SetUpProcess()

    # Logging every time a connection is opened/closed is spammy, so decrease
    # the default log level.
    logging.getLogger('websockets.server').setLevel(logging.WARNING)
    cls.websocket_server = wss.WebsocketServer()
    cls.websocket_server.StartServer()

  @classmethod
  def TearDownProcess(cls) -> None:
    cls.websocket_server.StopServer()
    cls.websocket_server = None
    super().TearDownProcess()

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
  def _ForceRefreshForNextNavigation(cls) -> None:
    if cls.websocket_server:
      cls.websocket_server.ClearCurrentConnection()
    cls.page_loaded = False

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    cls = self.__class__
    if cls.reload_page_for_next_navigation:
      self._ForceRefreshForNextNavigation()
    test_case = args[0]
    cls.reload_page_for_next_navigation = test_case.refresh_after_finish

  def NavigateTo(self, test_path: str, tab_data: TabData) -> None:
    """Navigates the given |test_path| URL in the test iframe.

    Will navigate the browser to the test page with the iframe first under the
    following conditions:
      * This is being run in the primary tab and the page has not been loaded
        since the last browser restart.
      * This is being run in a secondary tab

    Args:
      test_path: A string containing the test file to navigate to.
      tab_data: The tab and websocket server to use when loading the test page.
    """
    tab = tab_data.tab
    websocket_server = tab_data.websocket_server
    if not tab_data.is_default_tab or (tab_data.is_default_tab
                                       and not self.__class__.page_loaded):
      # If we haven't loaded the test page that we use to run tests within an
      # iframe, load it and establish the websocket connection.
      url = self.UrlOfStaticFilePath(TEST_PAGE_RELPATH)
      tab.Navigate(
          url,
          script_to_evaluate_on_commit=self._dom_automation_controller_script)
      tab.WaitForDocumentReadyStateToBeComplete(timeout=5)
      tab.action_runner.EvaluateJavaScript('connectWebsocket("%d")' %
                                           websocket_server.server_port,
                                           timeout=5)
      websocket_server.WaitForConnection(
          websocket_utils.GetScaledConnectionTimeout(self.child.jobs))
      response = websocket_server.Receive(5)
      response = json.loads(response)
      assert response['type'] == 'CONNECTION_ACK'
      if tab_data.is_default_tab:
        self.__class__.page_loaded = True

    url = self.UrlOfStaticFilePath(test_path)
    initial_scaling = PageHasViewportInitialScaling(test_path)
    tab.action_runner.EvaluateJavaScript('runTest("%s", %s)' %
                                         (url, initial_scaling))

  # pylint: disable=too-many-branches
  def HandleMessageLoop(self, test_timeout: float, tab_data: TabData,
                        loop_state: LoopState) -> None:
    """Handles the websocket message loop until an error or requested break.

    Args:
      test_timeout: How long the loop is allowed to run for in seconds before
          raising an error.
      tab_data: If set, the provided tab/websocket server will be used
          instead of the ones associated with the primary tab.
      loop_state: The LoopState to use. Will be modified in place.
    """
    tab = tab_data.tab
    websocket_server = tab_data.websocket_server
    start_time = time.time()
    try:
      while True:
        response = websocket_server.Receive(self._GetHeartbeatTimeout())
        response = json.loads(response)
        response_type = response['type']

        if time.time() - start_time > test_timeout:
          raise RuntimeError(
              'Hit %.3f second global timeout, but page continued to send '
              'messages over the websocket, i.e. was not due to a renderer '
              'crash.' % test_timeout)

        if response_type == 'TEST_STARTED':
          VerifyMessageOrderTestStarted(loop_state)
          continue

        if response_type == 'TEST_HEARTBEAT':
          VerifyMessageOrderTestHeartbeat(loop_state)
          continue

        # Used by several tests that have custom logic. Indicates that the
        # JavaScript code has reached some pre-determined point and is ready for
        # the Python code to do something.
        # PERFORM_PAGE_ACTION is a legacy response from when only a single
        # interrupt during a test was allowed, but is now equivalent to
        # TEST_CONTINUE
        if response_type in ('TEST_CONTINUE', 'PERFORM_PAGE_ACTION'):
          VerifyMessageOrderTestContinue(loop_state)
          break

        if response_type == 'TEST_FINISHED':
          VerifyMessageOrderTestFinished(loop_state)
          success = response['success']
          if not success:
            self.fail('Page reported failure')
          break

        raise RuntimeError('Received unknown message type %s' % response_type)
    except wss.WebsocketReceiveMessageTimeoutError:
      websocket_utils.HandleWebsocketReceiveTimeoutError(tab, start_time)
      raise
    except wss.ClientClosedConnectionError as e:
      websocket_utils.HandlePrematureSocketClose(e, start_time)
    finally:
      try:
        test_messages = tab.EvaluateJavaScript(
            'domAutomationController._messages',
            timeout=self._GetControllerMessageTimeout())
        if test_messages:
          logging.info('Logging messages from the test:\n%s', test_messages)
      except Exception:  # pylint:disable=broad-except
        logging.warning('Could not retrieve messages from test page.')

  # pylint: enable=too-many-branches

  def _GetHeartbeatTimeout(self) -> int:
    multiplier = 1
    if self._IsSlowTest():
      multiplier = SLOW_HEARTBEAT_MULTIPLIER
    return DEFAULT_HEARTBEAT_TIMEOUT * multiplier

  def _GetControllerMessageTimeout(self) -> int:
    multiplier = 1
    if self._IsSlowTest():
      multiplier = SLOW_CONTROLLER_MESSAGE_MULTIPLIER
    return DEFAULT_CONTROLLER_MESSAGE_TIMEOUT * multiplier

  def _IsSlowTest(self) -> bool:
    # We access the expectations directly instead of using
    # self.GetExpectationsForTest since we need the raw results, but that method
    # only returns the parsed results and whether the test should be retried.
    expectation = self.child.expectations.expectations_for(self.shortName())
    return 'Slow' in expectation.raw_results


# Pytype erroneously claims that bs4 elements can't be accessed with [].
# pytype: disable=unsupported-operands
def PageHasViewportInitialScaling(test_path: str) -> float:
  """Determines initial viewport scaling, if any, from |test_path|.

  Args:
    test_path: A src-relative filepath to a test HTML file to check.

  Returns:
    The initial scaling value specified by |test_path|. Returns 0 if no relevant
    viewport scaling information can be found.
  """
  # Some test paths include URL arguments, so strip those off.
  test_path = test_path.split('?', 1)[0]
  filepath = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, test_path)
  with open(filepath) as infile:
    contents = infile.read()
  soup = bs4.BeautifulSoup(contents, 'html.parser')
  for meta_tag in soup.find_all('meta'):
    if meta_tag['name'] != 'viewport':
      continue
    for content_piece in meta_tag['content'].split():
      if not content_piece.startswith('initial-scale'):
        continue
      return float(content_piece.split('=')[1])
  return 0


# pytype: enable=unsupported-operands


def EvalInTestIframe(tab: ct.Tab, expression: str) -> Any:
  escaped_expression = expression.replace('`', '\\`')
  eval_expression = f'evalInIframe(`{escaped_expression}`)'
  return tab.EvaluateJavaScript(eval_expression)


def VerifyMessageOrderTestStarted(loop_state: LoopState) -> None:
  if loop_state.test_started:
    raise RuntimeError('Received multiple start messages for one test')
  loop_state.test_started = True


def VerifyMessageOrderTestHeartbeat(loop_state: LoopState) -> None:
  if not loop_state.test_started:
    raise RuntimeError('Received heartbeat before test start')
  if loop_state.test_finished:
    raise RuntimeError('Received heartbeat after test finished')


def VerifyMessageOrderTestContinue(loop_state: LoopState) -> None:
  if not loop_state.test_started:
    raise RuntimeError('Received continue before test start')
  if loop_state.test_finished:
    raise RuntimeError('Received continue after test finished')


def VerifyMessageOrderTestFinished(loop_state: LoopState) -> None:
  if not loop_state.test_started:
    raise RuntimeError('Received test finish before test start')
  if loop_state.test_finished:
    raise RuntimeError('Received multiple finish messages for one test')
  loop_state.test_finished = True
