// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_egtest_plugin_client.h"

#import "base/logging.h"
#import "ios/testing/plugin/test_plugin_service.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using ios_test_plugin::DeviceInfo;
using ios_test_plugin::ListEnabledPluginsRequest;
using ios_test_plugin::ListEnabledPluginsResponse;
using ios_test_plugin::TestBundleWillFinishRequest;
using ios_test_plugin::TestBundleWillFinishResponse;
using ios_test_plugin::TestCaseDidFailRequest;
using ios_test_plugin::TestCaseDidFailResponse;
using ios_test_plugin::TestCaseDidFinishRequest;
using ios_test_plugin::TestCaseDidFinishResponse;
using ios_test_plugin::TestCaseInfo;
using ios_test_plugin::TestCaseWillStartRequest;
using ios_test_plugin::TestCaseWillStartResponse;
using ios_test_plugin::TestPluginService;

namespace chrome_egtest_plugin {
TestPluginClient::TestPluginClient(std::shared_ptr<Channel> channel)
    : stub_(TestPluginService::NewStub(channel)), is_service_enabled_(false) {}

TestPluginClient::~TestPluginClient() {}

void TestPluginClient::TestCaseWillStart(std::string test_name,
                                         std::string device_name) {
  TestCaseWillStartRequest request;
  TestCaseInfo* info = request.mutable_test_case_info();
  info->set_name(test_name);
  DeviceInfo* device = request.mutable_device_info();
  device->set_name(device_name);
  ClientContext context;
  TestCaseWillStartResponse response;
  Status status = stub_->TestCaseWillStart(&context, request, &response);
  if (!status.ok()) {
    LOG(WARNING) << "TestCaseWillStart Grpc call failed with error: "
                 << status.error_code() << ": " << status.error_message()
                 << std::endl;
  }
}

void TestPluginClient::TestCaseDidFail(std::string test_name,
                                       std::string device_name) {
  TestCaseDidFailRequest request;
  TestCaseInfo* info = request.mutable_test_case_info();
  info->set_name(test_name);
  DeviceInfo* device = request.mutable_device_info();
  device->set_name(device_name);
  ClientContext context;
  TestCaseDidFailResponse response;
  Status status = stub_->TestCaseDidFail(&context, request, &response);
  if (!status.ok()) {
    LOG(WARNING) << "TestCaseDidFail Grpc call failed with error: "
                 << status.error_code() << ": " << status.error_message()
                 << std::endl;
  }
}

void TestPluginClient::TestCaseDidFinish(std::string test_name,
                                         std::string device_name) {
  TestCaseDidFinishRequest request;
  TestCaseInfo* info = request.mutable_test_case_info();
  info->set_name(test_name);
  DeviceInfo* device = request.mutable_device_info();
  device->set_name(device_name);
  ClientContext context;
  TestCaseDidFinishResponse response;
  Status status = stub_->TestCaseDidFinish(&context, request, &response);
  if (!status.ok()) {
    LOG(WARNING) << "TestCaseDidFinish Grpc call failed with error: "
                 << status.error_code() << ": " << status.error_message()
                 << std::endl;
  }
}

void TestPluginClient::TestBundleWillFinish(std::string device_name) {
  TestBundleWillFinishRequest request;
  DeviceInfo* device = request.mutable_device_info();
  device->set_name(device_name);
  ClientContext context;
  TestBundleWillFinishResponse response;
  Status status = stub_->TestBundleWillFinish(&context, request, &response);
  if (!status.ok()) {
    LOG(WARNING) << "TestBundleWillFinish Grpc call failed with error: "
                 << status.error_code() << ": " << status.error_message()
                 << std::endl;
  }
}

std::vector<std::string> TestPluginClient::ListEnabledPlugins() {
  ListEnabledPluginsRequest request;
  ClientContext context;
  std::chrono::system_clock::time_point deadline =
      std::chrono::system_clock::now() + std::chrono::seconds(3);
  context.set_deadline(deadline);
  ListEnabledPluginsResponse response;
  Status status = stub_->ListEnabledPlugins(&context, request, &response);
  std::vector<std::string> enabled_plugins;
  if (status.ok()) {
    for (int i = 0; i < response.enabled_plugins_size(); ++i) {
      enabled_plugins.push_back(response.enabled_plugins(i));
    }
  } else {
    if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
      LOG(WARNING) << "Unable to connect to test runner gRPC server";
    }
    LOG(WARNING) << "ListEnabledPlugins Grpc call failed with error: "
                 << status.error_code() << ": " << status.error_message()
                 << std::endl;
  }
  return enabled_plugins;
}

void TestPluginClient::set_is_service_enabled(bool is_service_enabled) {
  this->is_service_enabled_ = is_service_enabled;
}

bool TestPluginClient::is_service_enabled() {
  return this->is_service_enabled_;
}
}  // namespace chrome_egtest_plugin
