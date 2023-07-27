// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EGTEST_PLUGIN_CLIENT_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EGTEST_PLUGIN_CLIENT_H_

#import <Foundation/Foundation.h>

#import <grpc/grpc.h>
#import <grpcpp/grpcpp.h>

#import "ios/testing/plugin/test_plugin_service.grpc.pb.h"

using grpc::Channel;
using ios_test_plugin::TestPluginService;

namespace chrome_egtest_plugin {

// This the test plugin client for making gRPC calls to the
// test plugin server throughout different lifecycle stages of
// a test case. More info about how test plugin works can be found
// in ios/testing/plugin/README.md
class TestPluginClient {
 public:
  TestPluginClient(std::shared_ptr<Channel> channel);
  ~TestPluginClient();
  void TestCaseWillStart(std::string test_name, std::string device_name);
  void TestCaseDidFail(std::string test_name, std::string device_name);
  void TestCaseDidFinish(std::string test_name, std::string device_name);
  void TestBundleWillFinish(std::string device_name);
  std::vector<std::string> ListEnabledPlugins();
  void set_is_service_enabled(bool is_service_enabled);

  // always check if the service is enabled before making any
  // grpc calls.
  bool is_service_enabled();

 private:
  std::unique_ptr<TestPluginService::Stub> stub_;
  bool is_service_enabled_;
};
}  // namespace chrome_egtest_plugin

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EGTEST_PLUGIN_CLIENT_H_
