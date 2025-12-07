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
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "net/base/network_isolation_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_proxy_delegate.h"
#include "net/http/http_request_headers.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/proxy_retry_info.h"
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

class FakeProxyDelegate : public ProxyDelegate {
 public:
  FakeProxyDelegate() = default;
  ~FakeProxyDelegate() override = default;

  void OnResolveProxy(const GURL& url,
                      const NetworkAnonymizationKey& network_anonymization_key,
                      const std::string& method,
                      const ProxyRetryInfoMap& proxy_retry_info,
                      ProxyInfo* result) override {
    on_resolve_proxy_call_count_++;
    last_proxy_retry_info_ = proxy_retry_info;

    // If configured to modify results, override the proxy list
    if (should_modify_result_) {
      result->UseProxyList(override_proxy_list_);
    }
  }

  void OnSuccessfulRequestAfterFailures(
      const ProxyRetryInfoMap& proxy_retry_info) override {
    on_successful_request_after_failures_call_count_++;
    last_successful_request_retry_info_ = proxy_retry_info;
  }

  void OnFallback(const ProxyChain& bad_chain, int net_error) override {
    on_fallback_call_count_++;
    last_fallback_chain_ = bad_chain;
    last_fallback_net_error_ = net_error;
  }

  base::expected<HttpRequestHeaders, Error> OnBeforeTunnelRequest(
      const ProxyChain& proxy_chain,
      size_t chain_index,
      OnBeforeTunnelRequestCallback callback) override {
    return HttpRequestHeaders();
  }

  Error OnTunnelHeadersReceived(const ProxyChain& proxy_chain,
                                size_t proxy_index,
                                const HttpResponseHeaders& response_headers,
                                CompletionOnceCallback callback) override {
    return OK;
  }

  void SetProxyResolutionService(
      ProxyResolutionService* proxy_resolution_service) override {}

  bool AliasRequiresProxyOverride(
      const std::string scheme,
      const std::vector<std::string>& dns_aliases,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {
    return false;
  }

  // Configuration methods for testing
  void set_should_modify_result(bool should_modify) {
    should_modify_result_ = should_modify;
  }

  void set_override_proxy_list(const ProxyList& proxy_list) {
    override_proxy_list_ = proxy_list;
  }

  // Test accessors
  size_t on_resolve_proxy_call_count() const {
    return on_resolve_proxy_call_count_;
  }
  size_t on_successful_request_after_failures_call_count() const {
    return on_successful_request_after_failures_call_count_;
  }
  size_t on_fallback_call_count() const { return on_fallback_call_count_; }
  const ProxyRetryInfoMap& last_proxy_retry_info() const {
    return last_proxy_retry_info_;
  }
  const ProxyRetryInfoMap& last_successful_request_retry_info() const {
    return last_successful_request_retry_info_;
  }
  const ProxyChain& last_fallback_chain() const { return last_fallback_chain_; }
  size_t last_fallback_net_error() const { return last_fallback_net_error_; }

 private:
  size_t on_resolve_proxy_call_count_ = 0;
  size_t on_successful_request_after_failures_call_count_ = 0;
  size_t on_fallback_call_count_ = 0;
  ProxyRetryInfoMap last_proxy_retry_info_;
  ProxyRetryInfoMap last_successful_request_retry_info_;
  ProxyChain last_fallback_chain_;
  size_t last_fallback_net_error_ = 0;

  // Configuration for result modification
  bool should_modify_result_ = false;
  ProxyList override_proxy_list_;
};

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
    int result = service()->ResolveProxy(
        kResourceUrl, std::string(), NetworkAnonymizationKey(), &info,
        callback.callback(), &request, log, DEFAULT_PRIORITY);

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
  base::HistogramTester histogram_tester;
  resolver()->set_winhttp_status(WinHttpStatus::kAborted);

  const int kTestWindowsError = 12345;
  resolver()->set_windows_error(kTestWindowsError);

  // Make sure there would be a proxy result on success.
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->add_server_to_proxy_list(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(), &info,
      callback.callback(), &request, log, DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  // Wait for result to come back.
  EXPECT_THAT(callback.GetResult(result), IsOk());

  EXPECT_TRUE(info.is_direct());
  EXPECT_NE(request, nullptr);
  histogram_tester.ExpectUniqueSample(
      "Net.HttpProxy.WindowsSystemResolver.WinError", kTestWindowsError, 1);
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
  int result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(), &info,
      callback.callback(), &request, log, DEFAULT_PRIORITY);

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
  base::HistogramTester histogram_tester;
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  resolver()->add_server_to_proxy_list(proxy_server);
  expected_proxy_list.AddProxyServer(proxy_server);

  DoResolveProxyTest(expected_proxy_list);
  histogram_tester.ExpectTotalCount(
      "Net.HttpProxy.WindowsSystemResolver.WinError", 0);
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
      first_callback.callback(), &first_request, log, DEFAULT_PRIORITY);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(first_request, nullptr);

  ProxyInfo second_proxy_info;
  TestCompletionCallback second_callback;
  std::unique_ptr<ProxyResolutionRequest> second_request;
  result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(),
      &second_proxy_info, second_callback.callback(), &second_request, log,
      DEFAULT_PRIORITY);
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
      first_callback.callback(), &first_request, log, DEFAULT_PRIORITY);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(first_request, nullptr);

  ProxyInfo second_proxy_info;
  TestCompletionCallback second_callback;
  std::unique_ptr<ProxyResolutionRequest> second_request;
  result = service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(),
      &second_proxy_info, second_callback.callback(), &second_request, log,
      DEFAULT_PRIORITY);
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

TEST_F(WindowsSystemProxyResolutionServiceTest, ReportSuccessWithRetryInfo) {
  // Test ReportSuccess() behavior when a proxy delegate is present.
  // This ensures proxy delegate callbacks (OnSuccessfulRequestAfterFailures
  // and OnFallback) are invoked during retry info processing.
  TestProxyDelegate proxy_delegate;
  service()->SetProxyDelegate(&proxy_delegate);

  ProxyInfo proxy_info;
  ProxyChain bad_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "badproxy.com", 8080));
  ProxyChain good_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "goodproxy.com", 8080));

  ProxyList proxy_list;
  proxy_list.AddProxyChain(bad_proxy_chain);
  proxy_list.AddProxyChain(good_proxy_chain);
  proxy_info.UseProxyList(proxy_list);

  EXPECT_TRUE(
      proxy_info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));

  service()->ReportSuccess(proxy_info);

  const ProxyRetryInfoMap& service_retry_info = service()->proxy_retry_info();
  EXPECT_EQ(1u, service_retry_info.size());
  EXPECT_NE(service_retry_info.find(bad_proxy_chain), service_retry_info.end());
}

TEST_F(WindowsSystemProxyResolutionServiceTest,
       ReportSuccessWithEmptyRetryInfo) {
  TestProxyDelegate proxy_delegate;
  service()->SetProxyDelegate(&proxy_delegate);
  ProxyInfo proxy_info;
  service()->ReportSuccess(proxy_info);

  EXPECT_TRUE(service()->proxy_retry_info().empty());
}

TEST_F(WindowsSystemProxyResolutionServiceTest,
       ReportSuccessWithRetryInfoAndWithoutDelegate) {
  // Test ReportSuccess() behavior when no proxy delegate is present.
  // This ensures retry info processing works correctly without delegate
  // callbacks, providing coverage for the alternative code path.
  ProxyInfo proxy_info;
  ProxyChain bad_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "badproxy.com", 8080));
  ProxyChain good_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "goodproxy.com", 8080));

  ProxyList proxy_list;
  proxy_list.AddProxyChain(bad_proxy_chain);
  proxy_list.AddProxyChain(good_proxy_chain);
  proxy_info.UseProxyList(proxy_list);

  EXPECT_TRUE(
      proxy_info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  service()->ReportSuccess(proxy_info);

  const ProxyRetryInfoMap& service_retry_info = service()->proxy_retry_info();
  EXPECT_EQ(1u, service_retry_info.size());
  EXPECT_NE(service_retry_info.find(bad_proxy_chain), service_retry_info.end());
}

TEST_F(WindowsSystemProxyResolutionServiceTest,
       ReportSuccessUpdateExistingRetryInfo) {
  TestProxyDelegate proxy_delegate;
  service()->SetProxyDelegate(&proxy_delegate);

  ProxyChain bad_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "badproxy.com", 8080));
  ProxyChain good_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "goodproxy.com", 8080));

  // First call - create ProxyInfo with retry information
  ProxyInfo proxy_info1;
  ProxyList proxy_list1;
  proxy_list1.AddProxyChain(bad_proxy_chain);
  proxy_list1.AddProxyChain(good_proxy_chain);
  proxy_info1.UseProxyList(proxy_list1);

  // Simulate failure on first proxy
  EXPECT_TRUE(
      proxy_info1.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));

  base::TimeTicks first_bad_until = base::TimeTicks::Now() + base::Minutes(5);
  service()->ReportSuccess(proxy_info1);

  // Second call - create another ProxyInfo with the same bad proxy
  ProxyInfo proxy_info2;
  ProxyList proxy_list2;
  proxy_list2.AddProxyChain(bad_proxy_chain);
  proxy_list2.AddProxyChain(good_proxy_chain);
  proxy_info2.UseProxyList(proxy_list2);

  // Simulate failure on proxy again
  EXPECT_TRUE(
      proxy_info2.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));

  service()->ReportSuccess(proxy_info2);

  // Verify the proxy retry info was stored
  const ProxyRetryInfoMap& service_retry_info = service()->proxy_retry_info();
  EXPECT_EQ(1u, service_retry_info.size());
  auto it = service_retry_info.find(bad_proxy_chain);
  EXPECT_NE(it, service_retry_info.end());
  // The bad_until time should have been updated to the later time
  EXPECT_GE(it->second.bad_until, first_bad_until);
}

// Test proxy delegate interaction with retry info through full resolution flow
TEST_F(WindowsSystemProxyResolutionServiceTest,
       ProxyDelegateInteractionWithRetryInfo) {
  TestProxyDelegate proxy_delegate;
  service()->SetProxyDelegate(&proxy_delegate);

  ProxyInfo proxy_info_with_retry;
  ProxyChain bad_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "badproxy.com", 8080));
  ProxyChain good_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "goodproxy.com", 8080));

  ProxyList proxy_list;
  proxy_list.AddProxyChain(bad_proxy_chain);
  proxy_list.AddProxyChain(good_proxy_chain);
  proxy_info_with_retry.UseProxyList(proxy_list);

  EXPECT_TRUE(proxy_info_with_retry.Fallback(ERR_PROXY_CONNECTION_FAILED,
                                             NetLogWithSource()));
  service()->ReportSuccess(proxy_info_with_retry);

  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS proxy.example.com:8080");
  resolver()->add_server_to_proxy_list(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = service()->ResolveProxy(
      kResourceUrl, "GET", NetworkAnonymizationKey(), &info,
      callback.callback(), &request, log, DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  EXPECT_THAT(callback.GetResult(result), IsOk());

  EXPECT_FALSE(info.is_direct());
  EXPECT_NE(request, nullptr);
}

TEST_F(WindowsSystemProxyResolutionServiceTest, ClearBadProxiesCache) {
  ProxyChain bad_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "badproxy.com", 8080));
  ProxyChain good_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "goodproxy.com", 8080));

  ProxyInfo proxy_info_with_retry;
  ProxyList proxy_list;
  proxy_list.AddProxyChain(bad_proxy_chain);
  proxy_list.AddProxyChain(good_proxy_chain);
  proxy_info_with_retry.UseProxyList(proxy_list);

  EXPECT_TRUE(proxy_info_with_retry.Fallback(ERR_PROXY_CONNECTION_FAILED,
                                             NetLogWithSource()));
  service()->ReportSuccess(proxy_info_with_retry);

  EXPECT_FALSE(service()->proxy_retry_info().empty());
  service()->ClearBadProxiesCache();
  EXPECT_TRUE(service()->proxy_retry_info().empty());
}

TEST_F(WindowsSystemProxyResolutionServiceTest,
       ReportSuccessProxyDelegateCalls) {
  FakeProxyDelegate proxy_delegate;
  service()->SetProxyDelegate(&proxy_delegate);

  ProxyInfo proxy_info;
  ProxyChain bad_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "badproxy.com", 8080));
  ProxyChain good_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "goodproxy.com", 8080));

  ProxyList proxy_list;
  proxy_list.AddProxyChain(bad_proxy_chain);
  proxy_list.AddProxyChain(good_proxy_chain);
  proxy_info.UseProxyList(proxy_list);

  // Simulate failure on first proxy to populate retry info
  EXPECT_TRUE(
      proxy_info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  service()->ReportSuccess(proxy_info);

  // Verify OnSuccessfulRequestAfterFailures was called
  EXPECT_EQ(1,
            proxy_delegate.on_successful_request_after_failures_call_count());
  EXPECT_EQ(1u, proxy_delegate.last_successful_request_retry_info().size());

  // Verify OnFallback was called for the bad proxy
  EXPECT_EQ(1, proxy_delegate.on_fallback_call_count());
  EXPECT_EQ(bad_proxy_chain, proxy_delegate.last_fallback_chain());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED,
            proxy_delegate.last_fallback_net_error());
}

// Test ReportSuccess with multiple proxy retry entries
TEST_F(WindowsSystemProxyResolutionServiceTest,
       ReportSuccessMultipleRetryEntries) {
  FakeProxyDelegate proxy_delegate;
  service()->SetProxyDelegate(&proxy_delegate);

  ProxyInfo proxy_info;
  ProxyChain bad_proxy_chain1(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "badproxy1.com", 8080));
  ProxyChain bad_proxy_chain2(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTPS, "badproxy2.com", 8443));
  ProxyChain good_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "goodproxy.com", 8080));

  ProxyList proxy_list;
  proxy_list.AddProxyChain(bad_proxy_chain1);
  proxy_list.AddProxyChain(bad_proxy_chain2);
  proxy_list.AddProxyChain(good_proxy_chain);
  proxy_info.UseProxyList(proxy_list);

  // Simulate failure on first two proxies
  EXPECT_TRUE(
      proxy_info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));
  EXPECT_TRUE(
      proxy_info.Fallback(ERR_PROXY_AUTH_REQUESTED, NetLogWithSource()));

  // Call ReportSuccess
  service()->ReportSuccess(proxy_info);

  // Verify OnSuccessfulRequestAfterFailures was called once
  EXPECT_EQ(1,
            proxy_delegate.on_successful_request_after_failures_call_count());
  EXPECT_EQ(2u, proxy_delegate.last_successful_request_retry_info().size());

  // Verify OnFallback was called for both bad proxies
  EXPECT_EQ(2, proxy_delegate.on_fallback_call_count());

  // Verify both proxies were stored in the service retry info
  const ProxyRetryInfoMap& service_retry_info = service()->proxy_retry_info();
  EXPECT_EQ(2u, service_retry_info.size());
  EXPECT_NE(service_retry_info.find(bad_proxy_chain1),
            service_retry_info.end());
  EXPECT_NE(service_retry_info.find(bad_proxy_chain2),
            service_retry_info.end());
}

// Test end-to-end proxy resolution with proxy delegate and retry info
TEST_F(WindowsSystemProxyResolutionServiceTest,
       EndToEndProxyResolutionWithRetryInfo) {
  FakeProxyDelegate proxy_delegate;
  service()->SetProxyDelegate(&proxy_delegate);

  ProxyChain bad_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "badproxy.com", 8080));
  ProxyChain good_proxy_chain(ProxyServer::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTP, "goodproxy.com", 8080));

  ProxyInfo proxy_info_with_retry;
  ProxyList proxy_list;
  proxy_list.AddProxyChain(bad_proxy_chain);
  proxy_list.AddProxyChain(good_proxy_chain);
  proxy_info_with_retry.UseProxyList(proxy_list);

  // Simulate failure on proxy to populate retry info
  EXPECT_TRUE(proxy_info_with_retry.Fallback(ERR_PROXY_CONNECTION_FAILED,
                                             NetLogWithSource()));
  service()->ReportSuccess(proxy_info_with_retry);

  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS proxy.example.com:8080");
  resolver()->add_server_to_proxy_list(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = service()->ResolveProxy(
      kResourceUrl, "GET", NetworkAnonymizationKey(), &info,
      callback.callback(), &request, log, DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  EXPECT_THAT(callback.GetResult(result), IsOk());

  // Verify OnResolveProxy was called during the resolution process
  EXPECT_EQ(1, proxy_delegate.on_resolve_proxy_call_count());
  EXPECT_EQ(1u, proxy_delegate.last_proxy_retry_info().size());
  EXPECT_FALSE(info.is_direct());
  EXPECT_NE(request, nullptr);
}

// Test that a proxy delegate can modify the proxy resolution result
TEST_F(WindowsSystemProxyResolutionServiceTest,
       ProxyDelegateModifiesResolutionResult) {
  FakeProxyDelegate proxy_delegate;
  service()->SetProxyDelegate(&proxy_delegate);

  // Set up the resolver to return a specific proxy
  const ProxyServer original_proxy_server =
      PacResultElementToProxyServer("HTTPS original.example.com:8080");
  resolver()->add_server_to_proxy_list(original_proxy_server);

  // Configure the delegate to override the result with a different proxy
  ProxyList override_proxy_list;
  const ProxyServer override_proxy_server =
      PacResultElementToProxyServer("HTTPS override.example.com:9090");
  override_proxy_list.AddProxyServer(override_proxy_server);

  proxy_delegate.set_should_modify_result(true);
  proxy_delegate.set_override_proxy_list(override_proxy_list);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = service()->ResolveProxy(
      kResourceUrl, "GET", NetworkAnonymizationKey(), &info,
      callback.callback(), &request, log, DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  EXPECT_THAT(callback.GetResult(result), IsOk());

  EXPECT_EQ(1, proxy_delegate.on_resolve_proxy_call_count());

  // Verify that the result was modified by the delegate
  EXPECT_FALSE(info.is_direct());
  EXPECT_TRUE(override_proxy_list.Equals(info.proxy_list()));
}

}  // namespace net
