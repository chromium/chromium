# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from concurrent import futures
import grpc
import logging
import sys

from utils import PLUGIN_PROTOS_PATH, PLUGIN_SERVICE_WORKER_COUNT, PLUGIN_SERVICE_ADDRESS

sys.path.append(PLUGIN_PROTOS_PATH)
import test_plugin_service_pb2
import test_plugin_service_pb2_grpc

LOGGER = logging.getLogger(__name__)


class TestPluginServicer(test_plugin_service_pb2_grpc.TestPluginServiceServicer
                        ):

  def TestCaseWillStart(self, request, context):
    # doing nothing in the api for now
    LOGGER.info('Received request for TestCaseWillStart %s', request)
    return test_plugin_service_pb2.TestCaseWillStartResponse()

  def TestCaseDidFinish(self, request, context):
    # doing nothing in the api for now
    LOGGER.info('Received request for TestCaseDidFinish %s', request)
    return test_plugin_service_pb2.TestCaseDidFinishResponse()

  def TestCaseDidFail(self, request, context):
    # doing nothing in the api for now
    LOGGER.info('Received request for TestCaseDidFail %s', request)
    return test_plugin_service_pb2.TestCaseDidFailResponse()

  def ListEnabledPlugins(self, request, context):
    # doing nothing in the api for now
    LOGGER.info('Received request for ListEnabledPlugins %s', request)
    return test_plugin_service_pb2.ListEnabledPluginsResponse()


class TestPluginServicerWrapper:

  def __init__(self, servicer):
    self.servicer = servicer
    self.server = grpc.server(
        futures.ThreadPoolExecutor(max_workers=PLUGIN_SERVICE_WORKER_COUNT))

  def start_server(self):
    test_plugin_service_pb2_grpc.add_TestPluginServiceServicer_to_server(
        self.servicer, self.server)
    self.server.add_insecure_port(PLUGIN_SERVICE_ADDRESS)
    self.server.start()

  def wait_for_termination(self):
    self.server.wait_for_termination()


if __name__ == '__main__':
  logging.basicConfig()
  server = TestPluginServicerWrapper(TestPluginServicer())
  server.start_server()
  server.wait_for_termination()
