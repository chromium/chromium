# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import grpc
import logging
import os
import sys

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from plugin_constants import PLUGIN_PROTOS_PATH, PLUGIN_SERVICE_ADDRESS

sys.path.append(PLUGIN_PROTOS_PATH)
import test_plugin_service_pb2
import test_plugin_service_pb2_grpc


class TestPluginClient:

  def __init__(self, plugin_service_address):
    self.plugin_service_address = plugin_service_address
    self.channel_stub = test_plugin_service_pb2_grpc.TestPluginServiceStub(
        grpc.insecure_channel(plugin_service_address))

  def ListEnabledPlugins(self, request):
    return self.channel_stub.ListEnabledPlugins(request)

  def TestCaseWillStart(self, request):
    return self.channel_stub.TestCaseWillStart(request)

  def TestCaseDidFinish(self, request):
    return self.channel_stub.TestCaseDidFinish(request)

  def TestCaseDidFail(self, request):
    return self.channel_stub.TestCaseDidFail(request)


# for manual testing purposes
def run():
  plugin_client = TestPluginClient(PLUGIN_SERVICE_ADDRESS)
  response = plugin_client.ListEnabledPlugins(
      test_plugin_service_pb2.ListEnabledPluginsRequest())
  print("Plugin client received: " + str(response))


if __name__ == '__main__':
  logging.basicConfig()
  run()
