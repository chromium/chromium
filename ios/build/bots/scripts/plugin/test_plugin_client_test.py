# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import os
import sys
import mock

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from test_plugin_client import TestPluginClient
from plugin_constants import PLUGIN_PROTOS_PATH, PLUGIN_SERVICE_ADDRESS

sys.path.append(PLUGIN_PROTOS_PATH)
import test_plugin_service_pb2
import test_plugin_service_pb2_grpc


class TestPluginClientTest(unittest.TestCase):

  def setUp(self):
    self.client = TestPluginClient(PLUGIN_SERVICE_ADDRESS)
    self.mock_channel_stub = mock.Mock()
    self.client.channel_stub = self.mock_channel_stub

  def test_ListEnabledPlugins(self):
    request = test_plugin_service_pb2.TestCaseWillStartRequest()
    self.client.ListEnabledPlugins(request)
    self.client.channel_stub.ListEnabledPlugins.assert_called_with(request)

  def test_TestCaseWillStart(self):
    request = test_plugin_service_pb2.TestCaseWillStartRequest()
    self.client.TestCaseWillStart(request)
    self.client.channel_stub.TestCaseWillStart.assert_called_with(request)

  def test_TestCaseDidFinish(self):
    request = test_plugin_service_pb2.TestCaseDidFinishRequest()
    self.client.TestCaseDidFinish(request)
    self.client.channel_stub.TestCaseDidFinish.assert_called_with(request)

  def test_TestCaseDidFail(self):
    request = test_plugin_service_pb2.TestCaseDidFailRequest()
    self.client.TestCaseDidFail(request)
    self.client.channel_stub.TestCaseDidFail.assert_called_with(request)


if __name__ == '__main__':
  unittest.main()
