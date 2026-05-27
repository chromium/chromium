// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared typed-test infrastructure for SystemProxyResolutionService tests.
// Each platform (Windows, macOS) includes this header in its own _unittest.cc
// and instantiates the typed test suite with a platform-specific traits struct.
//
// Pattern follows net/ssl/client_cert_store_unittest-inl.h.
//
// NOTE: If any TYPED_TEST_P cases are added, removed, or renamed, update the
// REGISTER_TYPED_TEST_SUITE_P call at the bottom of this file.

#ifndef NET_PROXY_RESOLUTION_SYSTEM_PROXY_RESOLUTION_SERVICE_UNITTEST_INL_H_
#define NET_PROXY_RESOLUTION_SYSTEM_PROXY_RESOLUTION_SERVICE_UNITTEST_INL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "net/base/fake_proxy_delegate.h"
#include "net/base/network_isolation_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_proxy_delegate.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

// URL used by all shared proxy resolution tests.
inline const GURL kResourceUrl("https://example.test:8080/");

// --------------------------------------------------------------------------
// Traits interface (documented contract for platform-specific delegates)
// --------------------------------------------------------------------------
//
// Each platform must define a struct conforming to:
//
//   struct SystemProxyResolutionTestTraits {
//     using ServiceType = ...;
//         // e.g., WindowsSystemProxyResolutionService
//     using MockResolverType = ...;   // e.g., MockWindowsSystemProxyResolver
//
//     // Create a service from a mock resolver.
//     static std::unique_ptr<ServiceType> CreateService(
//         std::unique_ptr<MockResolverType> resolver);
//
//     // Create a service with a null resolver (for null-check test).
//     static std::unique_ptr<ServiceType> CreateServiceWithNullResolver();
//
//     // Return true if tests should be skipped on this platform.
//     // Windows: !IsSupported(). Mac: false.
//     static bool ShouldSkipSetUp();
//
//     // Hook for platform-specific cleanup when service is destroyed.
//     // Called after the fixture nullifies its resolver raw_ptr but before
//     // the service is destroyed. No-op for most platforms.
//     static void OnResetService();
//   };

// --------------------------------------------------------------------------
// SystemProxyResolutionServiceTest<Traits> — typed test fixture
// --------------------------------------------------------------------------

template <typename Traits>
class SystemProxyResolutionServiceTest : public TestWithTaskEnvironment {
 public:
  using ServiceType = typename Traits::ServiceType;
  using MockResolverType = typename Traits::MockResolverType;

  void SetUp() override {
    testing::Test::SetUp();

    if (Traits::ShouldSkipSetUp()) {
      GTEST_SKIP() << "System proxy resolution not supported on this platform.";
    }

    auto proxy_resolver = std::make_unique<MockResolverType>();
    proxy_resolver_ = proxy_resolver.get();
    proxy_resolution_service_ =
        Traits::CreateService(std::move(proxy_resolver));
    ASSERT_TRUE(proxy_resolution_service_);
  }

  ServiceType* service() { return proxy_resolution_service_.get(); }

  MockResolverType* resolver() { return proxy_resolver_; }

  void ResetProxyResolutionService() {
    // Nullify before destroying the service to avoid dangling raw_ptr.
    proxy_resolver_ = nullptr;
    Traits::OnResetService();
    proxy_resolution_service_.reset();
  }

  void DoResolveProxyTest(const ProxyList& expected_proxy_list) {
    ProxyInfo info;
    TestCompletionCallback callback;
    NetLogWithSource log;
    std::unique_ptr<ProxyResolutionRequest> request;
    int result = this->service()->ResolveProxy(
        kResourceUrl, std::string(), NetworkAnonymizationKey(),
        handles::kInvalidNetworkHandle, &info, callback.callback(), &request,
        log, DEFAULT_PRIORITY);

    ASSERT_THAT(result, IsError(ERR_IO_PENDING));
    ASSERT_NE(request, nullptr);

    // Wait for result to come back.
    EXPECT_THAT(callback.GetResult(result), IsOk());

    EXPECT_TRUE(expected_proxy_list.Equals(info.proxy_list()));
    EXPECT_NE(request, nullptr);
  }

 private:
  std::unique_ptr<ServiceType> proxy_resolution_service_;
  raw_ptr<MockResolverType> proxy_resolver_ = nullptr;
};

TYPED_TEST_SUITE_P(SystemProxyResolutionServiceTest);

// --------------------------------------------------------------------------
// 16 shared typed tests
// --------------------------------------------------------------------------

TYPED_TEST_P(SystemProxyResolutionServiceTest, CreateWithNullResolver) {
  auto proxy_resolution_service = TypeParam::CreateServiceWithNullResolver();
  EXPECT_FALSE(proxy_resolution_service);
}

TYPED_TEST_P(SystemProxyResolutionServiceTest, ResolveProxyCancelled) {
  // Make sure there would be a proxy result on success.
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  this->resolver()->AddServerToProxyList(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = this->service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(),
      handles::kInvalidNetworkHandle, &info, callback.callback(), &request, log,
      DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  // Cancel the request.
  request.reset();

  // The proxy shouldn't resolve. Let pending tasks drain.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(callback.have_result());
}

TYPED_TEST_P(SystemProxyResolutionServiceTest, ResolveProxyEmptyResults) {
  ProxyList expected_proxy_list;
  this->DoResolveProxyTest(expected_proxy_list);
}

TYPED_TEST_P(SystemProxyResolutionServiceTest,
             MultipleProxyResolutionRequests) {
  ProxyList expected_proxy_list;
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  this->resolver()->AddServerToProxyList(proxy_server);
  expected_proxy_list.AddProxyServer(proxy_server);
  NetLogWithSource log;

  ProxyInfo first_proxy_info;
  TestCompletionCallback first_callback;
  std::unique_ptr<ProxyResolutionRequest> first_request;
  int result = this->service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(),
      handles::kInvalidNetworkHandle, &first_proxy_info,
      first_callback.callback(), &first_request, log, DEFAULT_PRIORITY);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(first_request, nullptr);

  ProxyInfo second_proxy_info;
  TestCompletionCallback second_callback;
  std::unique_ptr<ProxyResolutionRequest> second_request;
  result = this->service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(),
      handles::kInvalidNetworkHandle, &second_proxy_info,
      second_callback.callback(), &second_request, log, DEFAULT_PRIORITY);
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

TYPED_TEST_P(SystemProxyResolutionServiceTest,
             ProxyResolutionServiceDestructionWithInFlightRequests) {
  ProxyList expected_proxy_list;
  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS foopy:8443");
  this->resolver()->AddServerToProxyList(proxy_server);
  expected_proxy_list.AddProxyServer(proxy_server);
  NetLogWithSource log;

  ProxyInfo first_proxy_info;
  TestCompletionCallback first_callback;
  std::unique_ptr<ProxyResolutionRequest> first_request;
  int result = this->service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(),
      handles::kInvalidNetworkHandle, &first_proxy_info,
      first_callback.callback(), &first_request, log, DEFAULT_PRIORITY);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(first_request, nullptr);

  ProxyInfo second_proxy_info;
  TestCompletionCallback second_callback;
  std::unique_ptr<ProxyResolutionRequest> second_request;
  result = this->service()->ResolveProxy(
      kResourceUrl, std::string(), NetworkAnonymizationKey(),
      handles::kInvalidNetworkHandle, &second_proxy_info,
      second_callback.callback(), &second_request, log, DEFAULT_PRIORITY);
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(second_request, nullptr);

  // There are now 2 in-flight proxy resolution requests. Deleting the proxy
  // resolution service should call the callbacks immediately and do any
  // appropriate error handling.
  this->ResetProxyResolutionService();
  EXPECT_TRUE(first_callback.have_result());
  EXPECT_TRUE(second_callback.have_result());

  EXPECT_TRUE(first_proxy_info.is_direct());
  EXPECT_TRUE(second_proxy_info.is_direct());
}

TYPED_TEST_P(SystemProxyResolutionServiceTest,
             CastToConfiguredProxyResolutionService) {
  auto configured_service = ConfiguredProxyResolutionService::CreateDirect();
  ConfiguredProxyResolutionService* casted_service = configured_service.get();
  EXPECT_FALSE(
      this->service()->CastToConfiguredProxyResolutionService(&casted_service));
  EXPECT_EQ(nullptr, casted_service);
}

TYPED_TEST_P(SystemProxyResolutionServiceTest, ClearBadProxiesCache) {
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
  this->service()->ReportSuccess(proxy_info_with_retry);

  EXPECT_FALSE(this->service()->proxy_retry_info().empty());
  this->service()->ClearBadProxiesCache();
  EXPECT_TRUE(this->service()->proxy_retry_info().empty());
}

TYPED_TEST_P(SystemProxyResolutionServiceTest, ReportSuccessWithRetryInfo) {
  // Test ReportSuccess() behavior when a proxy delegate is present.
  // This ensures proxy delegate callbacks (OnSuccessfulRequestAfterFailures
  // and OnFallback) are invoked during retry info processing.
  TestProxyDelegate proxy_delegate;
  this->service()->SetProxyDelegate(&proxy_delegate);

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

  this->service()->ReportSuccess(proxy_info);

  const ProxyRetryInfoMap& service_retry_info =
      this->service()->proxy_retry_info();
  EXPECT_EQ(1u, service_retry_info.size());
  EXPECT_NE(service_retry_info.find(bad_proxy_chain), service_retry_info.end());
}

TYPED_TEST_P(SystemProxyResolutionServiceTest,
             ReportSuccessWithEmptyRetryInfo) {
  TestProxyDelegate proxy_delegate;
  this->service()->SetProxyDelegate(&proxy_delegate);
  ProxyInfo proxy_info;
  this->service()->ReportSuccess(proxy_info);

  EXPECT_TRUE(this->service()->proxy_retry_info().empty());
}

TYPED_TEST_P(SystemProxyResolutionServiceTest,
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
  this->service()->ReportSuccess(proxy_info);

  const ProxyRetryInfoMap& service_retry_info =
      this->service()->proxy_retry_info();
  EXPECT_EQ(1u, service_retry_info.size());
  EXPECT_NE(service_retry_info.find(bad_proxy_chain), service_retry_info.end());
}

TYPED_TEST_P(SystemProxyResolutionServiceTest,
             ReportSuccessUpdateExistingRetryInfo) {
  TestProxyDelegate proxy_delegate;
  this->service()->SetProxyDelegate(&proxy_delegate);

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
  this->service()->ReportSuccess(proxy_info1);

  // Second call - create another ProxyInfo with the same bad proxy
  ProxyInfo proxy_info2;
  ProxyList proxy_list2;
  proxy_list2.AddProxyChain(bad_proxy_chain);
  proxy_list2.AddProxyChain(good_proxy_chain);
  proxy_info2.UseProxyList(proxy_list2);

  // Simulate failure on proxy again
  EXPECT_TRUE(
      proxy_info2.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource()));

  this->service()->ReportSuccess(proxy_info2);

  // Verify the proxy retry info was stored
  const ProxyRetryInfoMap& service_retry_info =
      this->service()->proxy_retry_info();
  EXPECT_EQ(1u, service_retry_info.size());
  auto it = service_retry_info.find(bad_proxy_chain);
  EXPECT_NE(it, service_retry_info.end());
  // The bad_until time should have been updated to the later time
  EXPECT_GE(it->second.bad_until, first_bad_until);
}

// Test proxy delegate interaction with retry info through full resolution flow
TYPED_TEST_P(SystemProxyResolutionServiceTest,
             ProxyDelegateInteractionWithRetryInfo) {
  TestProxyDelegate proxy_delegate;
  this->service()->SetProxyDelegate(&proxy_delegate);

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
  this->service()->ReportSuccess(proxy_info_with_retry);

  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS proxy.example.com:8080");
  this->resolver()->AddServerToProxyList(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = this->service()->ResolveProxy(
      kResourceUrl, "GET", NetworkAnonymizationKey(),
      handles::kInvalidNetworkHandle, &info, callback.callback(), &request, log,
      DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  EXPECT_THAT(callback.GetResult(result), IsOk());

  EXPECT_FALSE(info.is_direct());
  EXPECT_NE(request, nullptr);
}

TYPED_TEST_P(SystemProxyResolutionServiceTest,
             ReportSuccessProxyDelegateCalls) {
  FakeProxyDelegate proxy_delegate;
  this->service()->SetProxyDelegate(&proxy_delegate);

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
  this->service()->ReportSuccess(proxy_info);

  // Verify OnSuccessfulRequestAfterFailures was called
  EXPECT_EQ(1u,
            proxy_delegate.on_successful_request_after_failures_call_count());
  EXPECT_EQ(1u, proxy_delegate.last_successful_request_retry_info().size());

  // Verify OnFallback was called for the bad proxy
  EXPECT_EQ(1u, proxy_delegate.on_fallback_call_count());
  EXPECT_EQ(bad_proxy_chain, proxy_delegate.last_fallback_chain());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED,
            proxy_delegate.last_fallback_net_error());
}

// Test ReportSuccess with multiple proxy retry entries
TYPED_TEST_P(SystemProxyResolutionServiceTest,
             ReportSuccessMultipleRetryEntries) {
  FakeProxyDelegate proxy_delegate;
  this->service()->SetProxyDelegate(&proxy_delegate);

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
  this->service()->ReportSuccess(proxy_info);

  // Verify OnSuccessfulRequestAfterFailures was called once
  EXPECT_EQ(1u,
            proxy_delegate.on_successful_request_after_failures_call_count());
  EXPECT_EQ(2u, proxy_delegate.last_successful_request_retry_info().size());

  // Verify OnFallback was called for both bad proxies
  EXPECT_EQ(2u, proxy_delegate.on_fallback_call_count());

  // Verify both proxies were stored in the service retry info
  const ProxyRetryInfoMap& service_retry_info =
      this->service()->proxy_retry_info();
  EXPECT_EQ(2u, service_retry_info.size());
  EXPECT_NE(service_retry_info.find(bad_proxy_chain1),
            service_retry_info.end());
  EXPECT_NE(service_retry_info.find(bad_proxy_chain2),
            service_retry_info.end());
}

// Test end-to-end proxy resolution with proxy delegate and retry info
TYPED_TEST_P(SystemProxyResolutionServiceTest,
             EndToEndProxyResolutionWithRetryInfo) {
  FakeProxyDelegate proxy_delegate;
  this->service()->SetProxyDelegate(&proxy_delegate);

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
  this->service()->ReportSuccess(proxy_info_with_retry);

  const ProxyServer proxy_server =
      PacResultElementToProxyServer("HTTPS proxy.example.com:8080");
  this->resolver()->AddServerToProxyList(proxy_server);

  ProxyInfo info;
  TestCompletionCallback callback;
  NetLogWithSource log;
  std::unique_ptr<ProxyResolutionRequest> request;
  int result = this->service()->ResolveProxy(
      kResourceUrl, "GET", NetworkAnonymizationKey(),
      handles::kInvalidNetworkHandle, &info, callback.callback(), &request, log,
      DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  EXPECT_THAT(callback.GetResult(result), IsOk());

  // Verify OnResolveProxy was called during the resolution process
  EXPECT_EQ(1u, proxy_delegate.on_resolve_proxy_call_count());
  EXPECT_EQ(1u, proxy_delegate.last_proxy_retry_info().size());
  EXPECT_FALSE(info.is_direct());
  EXPECT_NE(request, nullptr);
}

// Test that a proxy delegate can modify the proxy resolution result
TYPED_TEST_P(SystemProxyResolutionServiceTest,
             ProxyDelegateModifiesResolutionResult) {
  FakeProxyDelegate proxy_delegate;
  this->service()->SetProxyDelegate(&proxy_delegate);

  // Set up the resolver to return a specific proxy
  const ProxyServer original_proxy_server =
      PacResultElementToProxyServer("HTTPS original.example.com:8080");
  this->resolver()->AddServerToProxyList(original_proxy_server);

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
  int result = this->service()->ResolveProxy(
      kResourceUrl, "GET", NetworkAnonymizationKey(),
      handles::kInvalidNetworkHandle, &info, callback.callback(), &request, log,
      DEFAULT_PRIORITY);

  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_NE(request, nullptr);

  EXPECT_THAT(callback.GetResult(result), IsOk());

  EXPECT_EQ(1u, proxy_delegate.on_resolve_proxy_call_count());

  // Verify that the result was modified by the delegate
  EXPECT_FALSE(info.is_direct());
  EXPECT_TRUE(override_proxy_list.Equals(info.proxy_list()));
}

// --------------------------------------------------------------------------
// Register all typed tests
// --------------------------------------------------------------------------

REGISTER_TYPED_TEST_SUITE_P(
    SystemProxyResolutionServiceTest,
    CreateWithNullResolver,
    ResolveProxyCancelled,
    ResolveProxyEmptyResults,
    MultipleProxyResolutionRequests,
    ProxyResolutionServiceDestructionWithInFlightRequests,
    CastToConfiguredProxyResolutionService,
    ClearBadProxiesCache,
    ReportSuccessWithRetryInfo,
    ReportSuccessWithEmptyRetryInfo,
    ReportSuccessWithRetryInfoAndWithoutDelegate,
    ReportSuccessUpdateExistingRetryInfo,
    ProxyDelegateInteractionWithRetryInfo,
    ReportSuccessProxyDelegateCalls,
    ReportSuccessMultipleRetryEntries,
    EndToEndProxyResolutionWithRetryInfo,
    ProxyDelegateModifiesResolutionResult);

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_SYSTEM_PROXY_RESOLUTION_SERVICE_UNITTEST_INL_H_
