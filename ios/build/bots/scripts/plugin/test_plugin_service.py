# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from concurrent import futures
import grpc
import logging
import os
import subprocess
import sys

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from plugin_constants import PLUGIN_PROTOS_PATH, PLUGIN_SERVICE_WORKER_COUNT, PLUGIN_SERVICE_ADDRESS, PLUGIN_PROXY_SERVICE_PORT, REMOTE_PLUGIN_PROXY_PORT

sys.path.append(PLUGIN_PROTOS_PATH)
import test_plugin_service_pb2
import test_plugin_service_pb2_grpc

LOGGER = logging.getLogger(__name__)

class TestPluginServicer(test_plugin_service_pb2_grpc.TestPluginServiceServicer
                        ):
  """
  Implementation of test plugin service for communication between
  iOS test runner and EG tests
  """

  def __init__(self, enabled_plugins):
    """ Initializes a new instance of this class.

    Args:
      enabled_plugins: a list of initialized plugins to be used during
        different lifecycle stages of test execution. See the list of
        available test plugins in test_plugins.py

    """
    self.plugins = enabled_plugins

  def TestCaseWillStart(self, request, context):
    """ Executes plugin tasks when a test case is about to start """
    LOGGER.info('Received request for TestCaseWillStart %s', request)
    for plugin in self.plugins:
      plugin.test_case_will_start(request)
    return test_plugin_service_pb2.TestCaseWillStartResponse()

  def TestCaseDidFinish(self, request, context):
    """ Executes plugin tasks when a test case just finished executing """
    LOGGER.info('Received request for TestCaseDidFinish %s', request)
    for plugin in self.plugins:
      plugin.test_case_did_finish(request)
    return test_plugin_service_pb2.TestCaseDidFinishResponse()

  def TestCaseDidFail(self, request, context):
    """ Executes plugin tasks when a test case failed unexpectedly """
    LOGGER.info('Received request for TestCaseDidFail %s', request)
    for plugin in self.plugins:
      plugin.test_case_did_fail(request)
    return test_plugin_service_pb2.TestCaseDidFailResponse()

  def TestBundleWillFinish(self, request, context):
    """ Executes plugin tasks when a test bundle is about to finish"""
    LOGGER.info('Received request for TestBundleWillFinish %s', request)
    for plugin in self.plugins:
      plugin.test_bundle_will_finish(request)
    return test_plugin_service_pb2.TestBundleWillFinishResponse()

  def ListEnabledPlugins(self, request, context):
    """ Returns the list of enabled plugins """
    LOGGER.info('Received request for ListEnabledPlugins %s', request)
    plugins_str = [str(p) for p in self.plugins]
    return test_plugin_service_pb2.ListEnabledPluginsResponse(
        enabled_plugins=plugins_str)

  def reset(self):
    """
    Runs reset tasks for each plugin. This might be useful between
    each attempt of test runs
    """
    for plugin in self.plugins:
      LOGGER.info('Resetting %s', plugin)
      plugin.reset()


class TestPluginServicerWrapper:
  """ Wrapper for the test plugin service above.

  This class is useful for
  managing the plugin service such as start, tear_down, etc...
  """

  def __init__(self, servicer, device_proxy=None):
    """Initializes a new instance of this class.

    Args:
      servicer: an initialized test plugin service above
      device_proxy: default to be none. Pass in an instance of
        PluginServiceProxyWrapper if the test is running on physical device.

    """
    self.servicer = servicer
    self.server = grpc.server(
        futures.ThreadPoolExecutor(max_workers=PLUGIN_SERVICE_WORKER_COUNT))
    self.device_proxy = device_proxy

  def start_server(self):
    """ Starts a test plugin service, so it can receive gRPC requests """
    LOGGER.info('Starting test plugin server...')
    test_plugin_service_pb2_grpc.add_TestPluginServiceServicer_to_server(
        self.servicer, self.server)
    self.server.add_insecure_port(PLUGIN_SERVICE_ADDRESS)
    self.server.start()
    LOGGER.info('Test plugin server is running!')

    if self.device_proxy:
      self.device_proxy.start()
      LOGGER.info('Test plugin proxy server is running!')

  def wait_for_termination(self):
    """ Block current thread until the test plugin service stops """
    self.server.wait_for_termination()

  def tear_down(self):
    """ Resets and stop the test plugin service """
    LOGGER.info('Tearing down plugin service...')
    self.reset()
    self.server.stop(grace=None)

    if self.device_proxy:
      self.device_proxy.tear_down()

  def reset(self):
    """ Resets the test plugin service. This might be useful between
    each attempt of test runs """
    LOGGER.info('Resetting plugin service')
    self.servicer.reset()

    if self.device_proxy:
      self.device_proxy.reset()


# for testing purpose only when running locally
if __name__ == '__main__':
  server = TestPluginServicerWrapper(TestPluginServicer([]))
  server.start_server()
  server.wait_for_termination()
