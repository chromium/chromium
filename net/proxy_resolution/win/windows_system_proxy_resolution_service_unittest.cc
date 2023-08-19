// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/windows_system_proxy_resolution_service.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
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
#include "net/proxy_resolution/win/winhttp_status.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const GURL kResourceUrl("https://example.test:8080/");

class MockRequest : public WindowsSystemProxyResolver::Request {
 public:
  MockRequest(WindowsSystemProxyResolutionRequest* callback_target,
              const ProxyList& proxy_list,
              WinHttpStatus winhttp_status,
              int windows_error) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockRequest::DoCallback, weak_ptr_factory_.GetWeakPtr(),
                       callback_target, proxy_list, winhttp_status,
                       windows_error));
  }
  ~MockRequest() override = default;

 private:
  void DoCallback(WindowsSystemProxyResolutionRequest* callback_target,
                  const ProxyList& proxy_list,
                  WinHttpStatus winhttp_status,
                  int windows_error) {
    callback_target->ProxyResolutionComplete(proxy_list, winhttp_status,
                                             windows_error);
  }

  base::WeakPtrFactory<MockRequest> weak_ptr_factory_{this};
};

class MockWindowsSystemProxyResolver : public WindowsSystemProxyResolver {
 public:
  MockWindowsSystemProxyResolver() = default;
  ~MockWindowsSystemProxyResolver() override = default;

  void add_server_to_proxy_list(const ProxyServer& proxy_server) {
    proxy_list_.AddProxyServer(proxy_server);
  }

  void set_winhttp_status(WinHttpStatus winhttp_status) {
    winhttp_status_ = winhttp_status;
  }

  void set_windows_error(int windows_error) { windows_error_ = windows_error; }

  std::unique_ptr<Request> GetProxyForUrl(
      const GURL& url,
      WindowsSystemProxyResolutionRequest* callback_target) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return std::make_unique<MockRequest>(callback_target, proxy_list_,
                                         winhttp_status_, windows_error_);
  }

 private:
  ProxyList proxy_list_;
  WinHttpStatus winhttp_status_ = WinHttpStatus::kOk;
  int windows_error_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

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

    auto proxy_resolver = std::make_unique<MockWindowsSystemProxyResolver>();
    proxy_resolver_ = proxy_resolver.get();
    proxy_resolution_service_ = WindowsSystemProxyResolutionService::Create(
        std::move(proxy_resolver), /*net_log=*/nullptr);
    ASSERT_TRUE(proxy_resolution_service_);
  }

  WindowsSystemProxyResolutionService* service() {
    return proxy_resolution_service_.get();
  }

  MockWindowsSystemProxyResolver* resolver() { return proxy_resolver_; }

  void ResetProxyResolutionService() { proxy_resolution_service_.reset(); }

  void DoResolveProxyTest(const ProxyList& expected_proxy_list) {
    ProxyInfo info;
    TestCompletionCallback callback;
    NetLogWithSource log;
    std::unique_ptr<ProxyResolutionRequest> request;
    int result = service()->ResolveProxy(kResourceUrl, std::string(),
                                         NetworkAnonymizationKey(), &info,
                                         callback.callback(), &request, log);

    ASSERT_THAT(result, IsError(ERR_IO_PENDING));
    ASSERT_NE(request, nullptr);

    // Wait for result to come back.
    EXPECT_THAT(callback.GetResult(result), IsOk());

    EXPECT_TRUE(expected_proxy_list.Equals(info.proxy_list()));
    EXPECT_NE(request, nullptr);
  }

 private:
  std::unique_ptr<WindowsSystemProxyResolutionService>
      proxy_resolution_service_;
  raw_ptr<MockWindowsSystemProxyResolver, DanglingUntriaged> proxy_resolver_;
};

TEST_F(WindowsSystemProxyResolutionServiceTest, CreateWithNullResolver) {
  std::unique_ptr<WindowsSystemProxyResolutionService>
      proxy_resolution_service = WindowsSystemProxyResolutionService::Create(
          /*windows_system_proxy_resolver=*/nullptr, /*net_log=*/nullptr);
  EXPECT_FALSE(proxy_resolution_service);
}

TEST_F(WindowsSystemProxyResolutionServiceTest, ResolveProxyFailed) {
  resolver()->set_winhttp_status(WinHttpStatus::kAborted);

  // Make sure there would be a proxy result on success.
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->add_server_to_proxy_list(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = service()->ResolveProxy(kResourceUrl, std::string(),
                                       NetworkAnonymizationKey(), &info,
                                       callback.callback(), &request, log);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  // Wait for result to come back.
  EXPECT_THAT(callback.GetResult(result), IsOk());

  EXPECT_TRUE(info.is_direct());
  EXPECT_NE(request, nullptr);
}

TEST_F(WindowsSystemProxyResolutionServiceTest, ResolveProxyCancelled) {
  // Make sure there would be a proxy result on success.
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->add_server_to_proxy_list(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = service()->ResolveProxy(kResourceUrl, std::string(),
                                       NetworkAnonymizationKey(), &info,
                                       callback.callback(), &request, log);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  // Cancel the request.
  request.reset();

  // The proxy shouldn't resolve.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback.have_result());
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
      kResourceUrl, std::string(), NetworkAnonymizationKey(), &first_proxy_info,
      first_callback.callback(), &first_request, log);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(first_request, nullptr);

  ProxyInfo second_proxy_info;
  TestCompletionCallback second_callback;
  std::unique_ptr<ProxyResolutionRequest> second_request;
  result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(), &second_proxy_info,
      second_callback.callback(), &second_request, log);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(second_request, nullptr);

  // Wait for results to come back.
  EXPECT_THAT(first_callback.GetResult(result), IsOk());
  EXPECT_THAT(second_callback.GetResult(result), IsOk());

  EXPECT_TRUE(expected_proxy_list.Equals(first_proxy_info.proxy_list()));
  EXPECT_NE(first_request, nullptr);
  EXPECT_TRUE(expected_proxy_list.Equals(second_proxy_info.proxy_list()));
  EXPECT_NE(second_request, nullptr);
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
      kResourceUrl, std::string(), NetworkAnonymizationKey(), &first_proxy_info,
      first_callback.callback(), &first_request, log);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(first_request, nullptr);

  ProxyInfo second_proxy_info;
  TestCompletionCallback second_callback;
  std::unique_ptr<ProxyResolutionRequest> second_request;
  result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(), &second_proxy_info,
      second_callback.callback(), &second_request, log);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
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
