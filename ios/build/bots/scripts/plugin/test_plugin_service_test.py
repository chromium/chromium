#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest.mock as mock
import sys
import unittest

from test_plugin_service import TestPluginServicer, TestPluginServicerWrapper
from utils import PLUGIN_PROTOS_PATH, PLUGIN_SERVICE_ADDRESS

sys.path.append(PLUGIN_PROTOS_PATH)
import test_plugin_service_pb2
import test_plugin_service_pb2_grpc


class UnitTest(unittest.TestCase):

  def setUp(self):
    self.servicer = TestPluginServicer()
    self.servicer_wrapper = TestPluginServicerWrapper(self.servicer)
    self.servicer_wrapper.server = mock.MagicMock()

  def test_TestCaseWillStart_succeed(self):
    request = test_plugin_service_pb2.TestCaseWillStartRequest()
    response = self.servicer.TestCaseWillStart(request, None)
    expected_response = test_plugin_service_pb2.TestCaseWillStartResponse()
    self.assertEqual(response, expected_response)

  def test_TestCaseDidFinish_succeed(self):
    request = test_plugin_service_pb2.TestCaseDidFinishRequest()
    response = self.servicer.TestCaseDidFinish(request, None)
    expected_response = test_plugin_service_pb2.TestCaseDidFinishResponse()
    self.assertEqual(response, expected_response)

  def test_TestCaseDidFail_succeed(self):
    request = test_plugin_service_pb2.TestCaseDidFailRequest()
    response = self.servicer.TestCaseDidFail(request, None)
    expected_response = test_plugin_service_pb2.TestCaseDidFailResponse()
    self.assertEqual(response, expected_response)

  def test_ListEnabledPlugins_succeed(self):
    request = test_plugin_service_pb2.ListEnabledPluginsRequest()
    response = self.servicer.ListEnabledPlugins(request, None)
    expected_response = test_plugin_service_pb2.ListEnabledPluginsResponse()
    self.assertEqual(response, expected_response)

  def test_start_server(self):
    self.servicer_wrapper.start_server()
    self.servicer_wrapper.server.add_insecure_port.assert_called_with(
        PLUGIN_SERVICE_ADDRESS)
    self.servicer_wrapper.server.start.assert_called_with()

  def test_wait_for_termination(self):
    self.servicer_wrapper.wait_for_termination()
    self.servicer_wrapper.server.wait_for_termination.assert_called_with()


if __name__ == '__main__':
  unittest.main()
