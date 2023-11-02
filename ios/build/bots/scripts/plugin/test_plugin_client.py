# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import grpc
import logging
import sys

from plugin_constants import PLUGIN_PROTOS_PATH, PLUGIN_SERVICE_ADDRESS

sys.path.append(PLUGIN_PROTOS_PATH)
import test_plugin_service_pb2
import test_plugin_service_pb2_grpc


def run():
  with grpc.insecure_channel(PLUGIN_SERVICE_ADDRESS) as channel:
    stub = test_plugin_service_pb2_grpc.TestPluginServiceStub(channel)

    # replace below with the APIs you want to test
    response = stub.ListEnabledPlugins(
        test_plugin_service_pb2.ListEnabledPluginsRequest())
    print("Plugin client received: " + str(response))


if __name__ == '__main__':
  logging.basicConfig()
  run()
