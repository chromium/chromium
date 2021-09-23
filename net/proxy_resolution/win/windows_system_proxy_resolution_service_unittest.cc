// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/windows_system_proxy_resolution_service.h"

#include <limits>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/base/network_isolation_key.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolution_request.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolver.h"
#include "net/proxy_resolution/win/winhttp_api_wrapper.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const GURL kResourceUrl("https://example.test:8080/");

class MockWindowsSystemProxyResolver : public WindowsSystemProxyResolver {
 public:
  MockWindowsSystemProxyResolver() : WindowsSystemProxyResolver(nullptr) {}

  void set_get_proxy_for_url_success(bool get_proxy_for_url_success) {
    get_proxy_for_url_success_ = get_proxy_for_url_success;
  }
  bool GetProxyForUrl(WindowsSystemProxyResolutionRequest* callback_target,
                      const std::string& url) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!get_proxy_for_url_success_)
      return false;

    const int request_handle = proxy_resolver_identifier_++;
    pending_callback_target_map_[callback_target] = request_handle;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockWindowsSystemProxyResolver::DoQueryComplete,
                       base::Unretained(this), callback_target,
                       request_handle));

    return get_proxy_for_url_success_;
  }

  void RemovePendingCallbackTarget(
      WindowsSystemProxyResolutionRequest* callback_target) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    pending_callback_target_map_.erase(callback_target);
  }

  bool HasPendingCallbackTarget(
      WindowsSystemProxyResolutionRequest* callback_target) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return (pending_callback_target_map_.find(callback_target) !=
            pending_callback_target_map_.end());
  }

  void add_server_to_proxy_list(const ProxyServer& proxy_server) {
    proxy_list_.AddProxyServer(proxy_server);
  }

  void set_net_error(int net_error) { net_error_ = net_error; }

  void set_windows_error(int windows_error) { windows_error_ = windows_error; }

 private:
  ~MockWindowsSystemProxyResolver() override {
    if (!pending_callback_target_map_.empty())
      ADD_FAILURE()
          << "The WindowsSystemProxyResolutionRequests must account for all "
             "pending requests in the WindowsSystemProxyResolver.";
  }

  void DoQueryComplete(WindowsSystemProxyResolutionRequest* callback_target,
                       int request_handle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (HasPendingCallbackTarget(callback_target) &&
        pending_callback_target_map_[callback_target] == request_handle)
      callback_target->AsynchronousProxyResolutionComplete(
          proxy_list_, net_error_, windows_error_);
  }

  bool get_proxy_for_url_success_ = true;
  ProxyList proxy_list_;
  int net_error_ = OK;
  // TODO(https://crbug.com/1032820): Add tests for the |windows_error_|
  // code when it is used.
  int windows_error_ = 0;

  int proxy_resolver_identifier_ = 1;
  std::unordered_map<WindowsSystemProxyResolutionRequest*, int>
      pending_callback_target_map_;

  SEQUENCE_CHECKER(sequence_checker_);
};

scoped_refptr<WindowsSystemProxyResolver>
CreateWindowsSystemProxyResolverFails() {
  return nullptr;
}

}  // namespace

// These tests verify the behavior of the WindowsSystemProxyResolutionService in
// isolation by mocking out the WindowsSystemProxyResolver.
class WindowsSystemProxyResolutionServiceTest : public TestWithTaskEnvironment {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    if (!WindowsSystemProxyResolutionService::IsSupported()) {
      GTEST_SKIP()
          << "Windows System Proxy Resolution is only supported on Windows 8+.";
    }

    proxy_resolver_ = base::MakeRefCounted<MockWindowsSystemProxyResolver>();
    proxy_resolution_service_ =
        WindowsSystemProxyResolutionService::Create(/*net_log=*/nullptr);
    proxy_resolution_service_->SetWindowsSystemProxyResolverForTesting(
        proxy_resolver_);
  }

  WindowsSystemProxyResolutionService* service() {
    return proxy_resolution_service_.get();
  }

  scoped_refptr<MockWindowsSystemProxyResolver> resolver() {
    return proxy_resolver_;
  }

  size_t PendingRequestSizeForTesting() {
    return proxy_resolution_service_->PendingRequestSizeForTesting();
  }

  void ResetProxyResolutionService() { proxy_resolution_service_.reset(); }

  void DoResolveProxyCompletedSynchronouslyTest() {
    // Make sure there would be a proxy result on success.
    const ProxyServer proxy_server =
        PacResultElementToProxyServer("HTTPS foopy:8443");
    resolver()->add_server_to_proxy_list(proxy_server);

    ProxyInfo info;
    TestCompletionCallback callback;
    NetLogWithSource log;
    std::unique_ptr<ProxyResolutionRequest> request;
    const int result = service()->ResolveProxy(
        kResourceUrl, std::string(), NetworkIsolationKey(), &info,
        callback.callback(), &request, log);

    EXPECT_THAT(result, IsOk());
    EXPECT_TRUE(info.is_direct());
    EXPECT_FALSE(callback.have_result());
    EXPECT_EQ(PendingRequestSizeForTesting(), 0u);
    EXPECT_EQ(request, nullptr);
  }

  void DoResolveProxyTest(const ProxyList& expected_proxy_list) {
    ProxyInfo info;
    TestCompletionCallback callback;
    NetLogWithSource log;
    std::unique_ptr<ProxyResolutionRequest> request;
    int result = service()->ResolveProxy(kResourceUrl, std::string(),
                                         NetworkIsolationKey(), &info,
                                         callback.callback(), &request, log);

    ASSERT_THAT(result, IsError(ERR_IO_PENDING));
    ASSERT_EQ(PendingRequestSizeForTesting(), 1u);
    ASSERT_NE(request, nullptr);

    // Wait for result to come back.
    EXPECT_THAT(callback.GetResult(result), IsOk());

    EXPECT_TRUE(expected_proxy_list.Equals(info.proxy_list()));
    EXPECT_EQ(PendingRequestSizeForTesting(), 0u);
    EXPECT_NE(request, nullptr);
  }

  scoped_refptr<WindowsSystemProxyResolver>
  CreateMockWindowsSystemProxyResolver() {
    return proxy_resolver_;
  }

 private:
  std::unique_ptr<WindowsSystemProxyResolutionService>
      proxy_resolution_service_;
  scoped_refptr<MockWindowsSystemProxyResolver> proxy_resolver_;
};

TEST_F(WindowsSystemProxyResolutionServiceTest,
       ResolveProxyFailedToCreateResolver) {
  service()->SetWindowsSystemProxyResolverForTesting(nullptr);
  service()->SetCreateWindowsSystemProxyResolverFunctionForTesting(
      &CreateWindowsSystemProxyResolverFails);
  DoResolveProxyCompletedSynchronouslyTest();
}

TEST_F(WindowsSystemProxyResolutionServiceTest,
       ResolveProxyCompletedSynchronously) {
  resolver()->set_get_proxy_for_url_success(false);
  DoResolveProxyCompletedSynchronouslyTest();
}

TEST_F(WindowsSystemProxyResolutionServiceTest,
       ResolveProxyFailedAsynchronously) {
  resolver()->set_net_error(ERR_FAILED);

  // Make sure there would be a proxy result on success.
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->add_server_to_proxy_list(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = service()->ResolveProxy(kResourceUrl, std::string(),
                                       NetworkIsolationKey(), &info,
                                       callback.callback(), &request, log);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_EQ(PendingRequestSizeForTesting(), 1u);
  ASSERT_NE(request, nullptr);

  // Wait for result to come back.
  EXPECT_THAT(callback.GetResult(result), IsOk());

  EXPECT_TRUE(info.is_direct());
  EXPECT_EQ(PendingRequestSizeForTesting(), 0u);
  EXPECT_NE(request, nullptr);
}

TEST_F(WindowsSystemProxyResolutionServiceTest, ResolveProxyEmptyResults) {
  ProxyList expected_proxy_list;
  DoResolveProxyTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolutionServiceTest, ResolveProxyWithResults) {
  ProxyList expected_proxy_list;
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->add_server_to_proxy_list(proxy_server);
  expected_proxy_list.AddProxyServer(proxy_server);

  DoResolveProxyTest(expected_proxy_list);
}

TEST_F(WindowsSystemProxyResolutionServiceTest,
       MultipleProxyResolutionRequests) {
  ProxyList expected_proxy_list;
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->add_server_to_proxy_list(proxy_server);
  expected_proxy_list.AddProxyServer(proxy_server);
  NetLogWithSource log;

  ProxyInfo first_proxy_info;
  TestCompletionCallback first_callback;
  std::unique_ptr<ProxyResolutionRequest> first_request;
  int result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkIsolationKey(), &first_proxy_info,
      first_callback.callback(), &first_request, log);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_EQ(PendingRequestSizeForTesting(), 1u);
  ASSERT_NE(first_request, nullptr);

  ProxyInfo second_proxy_info;
  TestCompletionCallback second_callback;
  std::unique_ptr<ProxyResolutionRequest> second_request;
  result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkIsolationKey(), &second_proxy_info,
      second_callback.callback(), &second_request, log);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_EQ(PendingRequestSizeForTesting(), 2u);
  ASSERT_NE(second_request, nullptr);

  // Wait for results to come back.
  EXPECT_THAT(first_callback.GetResult(result), IsOk());
  EXPECT_THAT(second_callback.GetResult(result), IsOk());

  EXPECT_TRUE(expected_proxy_list.Equals(first_proxy_info.proxy_list()));
  EXPECT_NE(first_request, nullptr);
  EXPECT_TRUE(expected_proxy_list.Equals(second_proxy_info.proxy_list()));
  EXPECT_NE(second_request, nullptr);

  EXPECT_EQ(PendingRequestSizeForTesting(), 0u);
}

TEST_F(WindowsSystemProxyResolutionServiceTest,
       ProxyResolutionServiceDestructionWithInFlightRequests) {
  ProxyList expected_proxy_list;
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->add_server_to_proxy_list(proxy_server);
  expected_proxy_list.AddProxyServer(proxy_server);
  NetLogWithSource log;

  ProxyInfo first_proxy_info;
  TestCompletionCallback first_callback;
  std::unique_ptr<ProxyResolutionRequest> first_request;
  int result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkIsolationKey(), &first_proxy_info,
      first_callback.callback(), &first_request, log);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_EQ(PendingRequestSizeForTesting(), 1u);
  ASSERT_NE(first_request, nullptr);

  ProxyInfo second_proxy_info;
  TestCompletionCallback second_callback;
  std::unique_ptr<ProxyResolutionRequest> second_request;
  result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkIsolationKey(), &second_proxy_info,
      second_callback.callback(), &second_request, log);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_EQ(PendingRequestSizeForTesting(), 2u);
  ASSERT_NE(second_request, nullptr);

  // There are now 2 in-flight proxy resolution requests. Deleting the proxy
  // resolution service should call the callbacks immediately and do any
  // appropriate error handling.
  ResetProxyResolutionService();
  EXPECT_TRUE(first_callback.have_result());
  EXPECT_TRUE(second_callback.have_result());

  EXPECT_TRUE(first_proxy_info.is_direct());
  EXPECT_TRUE(second_proxy_info.is_direct());
}

TEST_F(WindowsSystemProxyResolutionServiceTest,
       CastToConfiguredProxyResolutionService) {
  auto configured_service = ConfiguredProxyResolutionService::CreateDirect();
  ConfiguredProxyResolutionService* casted_service = configured_service.get();
  EXPECT_FALSE(
      service()->CastToConfiguredProxyResolutionService(&casted_service));
  EXPECT_EQ(nullptr, casted_service);
}

}  // namespace net
