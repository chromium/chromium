#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mock
import os
import sys
import unittest

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from test_plugin_service import TestPluginServicer, TestPluginServicerWrapper
from plugin_constants import PLUGIN_PROTOS_PATH, PLUGIN_SERVICE_ADDRESS

sys.path.append(PLUGIN_PROTOS_PATH)
import test_plugin_service_pb2
import test_plugin_service_pb2_grpc

VIDEO_RECORDER_PLUGIN_NAME = 'VideoRecorderPlugin'


class UnitTest(unittest.TestCase):

  def setUp(self):
    self.video_recorder_plugin = mock.MagicMock()
    self.video_recorder_plugin.__str__ = mock.Mock(
        return_value=VIDEO_RECORDER_PLUGIN_NAME)
    self.servicer = TestPluginServicer([self.video_recorder_plugin])
    self.servicer_wrapper = TestPluginServicerWrapper(self.servicer)
    self.servicer_wrapper.server = mock.MagicMock()

    # for device testing
    self.device_proxy = mock.MagicMock()
    self.servicer_wrapper_on_device = TestPluginServicerWrapper(
        self.servicer, self.device_proxy)

  def test_TestCaseWillStart_succeed(self):
    request = test_plugin_service_pb2.TestCaseWillStartRequest()
    response = self.servicer.TestCaseWillStart(request, None)
    expected_response = test_plugin_service_pb2.TestCaseWillStartResponse()
    self.assertEqual(response, expected_response)
    self.video_recorder_plugin.test_case_will_start.assert_called_with(request)

  def test_TestCaseDidFinish_succeed(self):
    request = test_plugin_service_pb2.TestCaseDidFinishRequest()
    response = self.servicer.TestCaseDidFinish(request, None)
    expected_response = test_plugin_service_pb2.TestCaseDidFinishResponse()
    self.assertEqual(response, expected_response)
    self.video_recorder_plugin.test_case_did_finish.assert_called_with(request)

  def test_TestCaseDidFail_succeed(self):
    request = test_plugin_service_pb2.TestCaseDidFailRequest()
    response = self.servicer.TestCaseDidFail(request, None)
    expected_response = test_plugin_service_pb2.TestCaseDidFailResponse()
    self.assertEqual(response, expected_response)
    self.video_recorder_plugin.test_case_did_fail.assert_called_with(request)

  def test_TestBundleWillFinish(self):
    request = test_plugin_service_pb2.TestBundleWillFinishRequest()
    response = self.servicer.TestBundleWillFinish(request, None)
    expected_response = test_plugin_service_pb2.TestBundleWillFinishResponse()
    self.assertEqual(response, expected_response)
    self.video_recorder_plugin.test_bundle_will_finish.assert_called_with(
        request)

  def test_ListEnabledPlugins_succeed(self):
    request = test_plugin_service_pb2.ListEnabledPluginsRequest()
    response = self.servicer.ListEnabledPlugins(request, None)
    expected_plugin_str = [VIDEO_RECORDER_PLUGIN_NAME]
    expected_response = test_plugin_service_pb2.ListEnabledPluginsResponse(
        enabled_plugins=expected_plugin_str)
    self.assertEqual(response, expected_response)

  def test_start_server(self):
    self.servicer_wrapper.start_server()
    self.servicer_wrapper.server.add_insecure_port.assert_called_with(
        PLUGIN_SERVICE_ADDRESS)
    self.servicer_wrapper.server.start.assert_called_with()
    self.assertEqual(self.servicer_wrapper.device_proxy, None)

    # Plugin feature running on physical device
    self.servicer_wrapper_on_device.start_server()
    self.servicer_wrapper_on_device.device_proxy.start.assert_called_with()

  def test_tear_down(self):
    self.servicer_wrapper.tear_down()
    self.video_recorder_plugin.reset.assert_called_with()
    self.servicer_wrapper.server.stop.assert_called_with(grace=None)

    # Plugin feature running on physical device
    self.servicer_wrapper_on_device.tear_down()
    self.servicer_wrapper_on_device.device_proxy.tear_down.assert_called_with()

  def test_wait_for_termination(self):
    self.servicer_wrapper.wait_for_termination()
    self.servicer_wrapper.server.wait_for_termination.assert_called_with()


if __name__ == '__main__':
  unittest.main()
