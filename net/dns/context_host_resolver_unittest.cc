// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/context_host_resolver.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_results_test_util.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log_with_source.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "net/android/network_change_notifier_factory_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace net {

namespace {
const IPEndPoint kEndpoint(IPAddress(1, 2, 3, 4), 100);
}

class ContextHostResolverTest : public ::testing::Test,
                                public WithTaskEnvironment {
 protected:
  // Use mock time to prevent the HostResolverManager's injected IPv6 probe
  // result from timing out.
  ContextHostResolverTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~ContextHostResolverTest() override = default;

  void SetUp() override {
    manager_ = std::make_unique<HostResolverManager>(
        HostResolver::ManagerOptions(),
        nullptr /* system_dns_config_notifier */, nullptr /* net_log */);
    manager_->SetLastIPv6ProbeResultForTesting(true);
  }

  void SetMockDnsRules(MockDnsClientRuleList rules) {
    IPAddress dns_ip(192, 168, 1, 0);
    DnsConfig config;
    config.nameservers.emplace_back(dns_ip, dns_protocol::kDefaultPort);
    config.doh_config = *DnsOverHttpsConfig::FromString("https://example.com");
    EXPECT_TRUE(config.IsValid());

    auto dns_client =
        std::make_unique<MockDnsClient>(std::move(config), std::move(rules));
    dns_client->set_ignore_system_config_changes(true);
    dns_client_ = dns_client.get();
    manager_->SetDnsClientForTesting(std::move(dns_client));
    manager_->SetInsecureDnsClientEnabled(
        /*enabled=*/true,
        /*additional_dns_types_enabled=*/true);

    // Ensure DnsClient is fully usable.
    EXPECT_TRUE(dns_client_->CanUseInsecureDnsTransactions());
    EXPECT_FALSE(dns_client_->FallbackFromInsecureTransactionPreferred());
    EXPECT_TRUE(dns_client_->GetEffectiveConfig());

    scoped_refptr<HostResolverProc> proc = CreateCatchAllHostResolverProc();
    manager_->set_host_resolver_system_params_for_test(
        HostResolverSystemTask::Params(proc, 1u));
  }

  std::unique_ptr<HostResolverManager> manager_;
  raw_ptr<MockDnsClient> dns_client_;
};

TEST_F(ContextHostResolverTest, Resolve) {
  auto context = CreateTestURLRequestContextBuilder()->Build();

  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", kEndpoint.address())),
                     false /* delay */, context.get());
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */, context.get());
  SetMockDnsRules(std::move(rules));

  auto resolve_context = std::make_unique<ResolveContext>(
      context.get(), false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  EXPECT_THAT(callback.GetResult(rv), test::IsOk());
  EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(net::OK));
  EXPECT_THAT(request->GetAddressResults()->endpoints(),
              testing::ElementsAre(kEndpoint));
}

TEST_F(ContextHostResolverTest, ResolveWithScheme) {
  auto context = CreateTestURLRequestContextBuilder()->Build();

  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", kEndpoint.address())),
                     false /* delay */, context.get());
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */, context.get());
  SetMockDnsRules(std::move(rules));

  auto resolve_context = std::make_unique<ResolveContext>(
      context.get(), false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(
          url::SchemeHostPort(url::kHttpsScheme, "example.com", 100),
          NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  EXPECT_THAT(callback.GetResult(rv), test::IsOk());
  EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(net::OK));
  EXPECT_THAT(request->GetAddressResults()->endpoints(),
              testing::ElementsAre(kEndpoint));
}

TEST_F(ContextHostResolverTest, ResolveWithSchemeAndIpLiteral) {
  auto context = CreateTestURLRequestContextBuilder()->Build();

  IPAddress expected_address;
  ASSERT_TRUE(expected_address.AssignFromIPLiteral("1234::5678"));

  auto resolve_context = std::make_unique<ResolveContext>(
      context.get(), false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(
          url::SchemeHostPort(url::kHttpsScheme, "[1234::5678]", 100),
          NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  EXPECT_THAT(callback.GetResult(rv), test::IsOk());
  EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(net::OK));
  EXPECT_THAT(request->GetAddressResults()->endpoints(),
              testing::ElementsAre(IPEndPoint(expected_address, 100)));
}

// Test that destroying a request silently cancels that request.
TEST_F(ContextHostResolverTest, DestroyRequest) {
  // Set up delayed results for "example.com".
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", IPAddress(1, 2, 3, 4))),
                     true /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(),
      std::make_unique<ResolveContext>(nullptr /* url_request_context */,
                                       false /* enable_caching */));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  // Cancel |request| before allowing delayed result to complete.
  request = nullptr;
  dns_client_->CompleteDelayedTransactions();

  // Ensure |request| never completes.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(rv, test::IsError(ERR_IO_PENDING));
  EXPECT_FALSE(callback.have_result());
}

TEST_F(ContextHostResolverTest, DohProbeRequest) {
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  auto context = CreateTestURLRequestContextBuilder()->Build();
  auto resolve_context = std::make_unique<ResolveContext>(
      context.get(), true /* enable caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  std::unique_ptr<HostResolver::ProbeRequest> request =
      resolver->CreateDohProbeRequest();

  ASSERT_FALSE(dns_client_->factory()->doh_probes_running());

  EXPECT_THAT(request->Start(), test::IsError(ERR_IO_PENDING));
  EXPECT_TRUE(dns_client_->factory()->doh_probes_running());

  request.reset();

  EXPECT_FALSE(dns_client_->factory()->doh_probes_running());
}

TEST_F(ContextHostResolverTest, DohProbesFromSeparateContexts) {
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  auto resolve_context1 = std::make_unique<ResolveContext>(
      nullptr /* url_request_context */, false /* enable_caching */);
  auto resolver1 = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context1));
  std::unique_ptr<HostResolver::ProbeRequest> request1 =
      resolver1->CreateDohProbeRequest();

  auto resolve_context2 = std::make_unique<ResolveContext>(
      nullptr /* url_request_context */, false /* enable_caching */);
  auto resolver2 = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context2));
  std::unique_ptr<HostResolver::ProbeRequest> request2 =
      resolver2->CreateDohProbeRequest();

  EXPECT_FALSE(dns_client_->factory()->doh_probes_running());

  EXPECT_THAT(request1->Start(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(request2->Start(), test::IsError(ERR_IO_PENDING));

  EXPECT_TRUE(dns_client_->factory()->doh_probes_running());

  request1.reset();

  EXPECT_TRUE(dns_client_->factory()->doh_probes_running());

  request2.reset();

  EXPECT_FALSE(dns_client_->factory()->doh_probes_running());
}

// Test that cancelling a resolver cancels its (and only its) requests.
TEST_F(ContextHostResolverTest, DestroyResolver) {
  // Set up delayed results for "example.com" and "google.com".
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", IPAddress(2, 3, 4, 5))),
                     true /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);
  rules.emplace_back("google.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "google.com", kEndpoint.address())),
                     true /* delay */);
  rules.emplace_back(
      "google.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolver1 = std::make_unique<ContextHostResolver>(
      manager_.get(),
      std::make_unique<ResolveContext>(nullptr /* url_request_context */,
                                       false /* enable_caching */));
  std::unique_ptr<HostResolver::ResolveHostRequest> request1 =
      resolver1->CreateRequest(HostPortPair("example.com", 100),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt);
  auto resolver2 = std::make_unique<ContextHostResolver>(
      manager_.get(),
      std::make_unique<ResolveContext>(nullptr /* url_request_context */,
                                       false /* enable_caching */));
  std::unique_ptr<HostResolver::ResolveHostRequest> request2 =
      resolver2->CreateRequest(HostPortPair("google.com", 100),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt);

  TestCompletionCallback callback1;
  int rv1 = request1->Start(callback1.callback());
  TestCompletionCallback callback2;
  int rv2 = request2->Start(callback2.callback());

  EXPECT_EQ(2u, manager_->num_jobs_for_testing());

  // Cancel |resolver1| before allowing delayed requests to complete.
  resolver1 = nullptr;
  dns_client_->CompleteDelayedTransactions();

  EXPECT_THAT(callback2.GetResult(rv2), test::IsOk());
  EXPECT_THAT(request2->GetAddressResults()->endpoints(),
              testing::ElementsAre(kEndpoint));

  // Ensure |request1| never completes.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(rv1, test::IsError(ERR_IO_PENDING));
  EXPECT_FALSE(callback1.have_result());
}

TEST_F(ContextHostResolverTest, DestroyResolver_CompletedRequests) {
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", kEndpoint.address())),
                     false /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(),
      std::make_unique<ResolveContext>(nullptr /* url_request_context */,
                                       false /* enable_caching */));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);

  // Complete request and then destroy the resolver.
  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  ASSERT_THAT(callback.GetResult(rv), test::IsOk());
  resolver = nullptr;

  // Expect completed results are still available.
  EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(net::OK));
  EXPECT_THAT(request->GetAddressResults()->endpoints(),
              testing::ElementsAre(kEndpoint));
}

// Test a request created before resolver destruction but not yet started.
TEST_F(ContextHostResolverTest, DestroyResolver_DelayedStartRequest) {
  // Set up delayed result for "example.com".
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", IPAddress(2, 3, 4, 5))),
                     true /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);

  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(),
      std::make_unique<ResolveContext>(nullptr /* url_request_context */,
                                       false /* enable_caching */));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);

  resolver = nullptr;

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  EXPECT_THAT(callback.GetResult(rv), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(request->GetResolveErrorInfo().error,
              test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_FALSE(request->GetAddressResults());
}

TEST_F(ContextHostResolverTest, DestroyResolver_DelayedStartDohProbeRequest) {
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  auto context = CreateTestURLRequestContextBuilder()->Build();
  auto resolve_context = std::make_unique<ResolveContext>(
      context.get(), false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  std::unique_ptr<HostResolver::ProbeRequest> request =
      resolver->CreateDohProbeRequest();

  resolver = nullptr;

  EXPECT_THAT(request->Start(), test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_FALSE(dns_client_->factory()->doh_probes_running());
}

TEST_F(ContextHostResolverTest, OnShutdown_PendingRequest) {
  // Set up delayed result for "example.com".
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", IPAddress(2, 3, 4, 5))),
                     true /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto context = CreateTestURLRequestContextBuilder()->Build();
  auto resolve_context = std::make_unique<ResolveContext>(
      context.get(), false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  // Trigger shutdown before allowing request to complete.
  resolver->OnShutdown();
  dns_client_->CompleteDelayedTransactions();

  // Ensure request never completes.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(rv, test::IsError(ERR_IO_PENDING));
  EXPECT_FALSE(callback.have_result());
}

TEST_F(ContextHostResolverTest, OnShutdown_CompletedRequests) {
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", kEndpoint.address())),
                     false /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto context = CreateTestURLRequestContextBuilder()->Build();
  auto resolve_context = std::make_unique<ResolveContext>(
      context.get(), false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);

  // Complete request and then shutdown the resolver.
  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  ASSERT_THAT(callback.GetResult(rv), test::IsOk());
  resolver->OnShutdown();

  // Expect completed results are still available.
  EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(net::OK));
  EXPECT_THAT(request->GetAddressResults()->endpoints(),
              testing::ElementsAre(kEndpoint));
}

TEST_F(ContextHostResolverTest, OnShutdown_SubsequentRequests) {
  auto context = CreateTestURLRequestContextBuilder()->Build();
  auto resolve_context = std::make_unique<ResolveContext>(
      context.get(), false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  resolver->OnShutdown();

  std::unique_ptr<HostResolver::ResolveHostRequest> request1 =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);
  std::unique_ptr<HostResolver::ResolveHostRequest> request2 =
      resolver->CreateRequest(HostPortPair("127.0.0.1", 100),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);

  TestCompletionCallback callback1;
  int rv1 = request1->Start(callback1.callback());
  TestCompletionCallback callback2;
  int rv2 = request2->Start(callback2.callback());

  EXPECT_THAT(callback1.GetResult(rv1), test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_THAT(request1->GetResolveErrorInfo().error,
              test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_FALSE(request1->GetAddressResults());
  EXPECT_THAT(callback2.GetResult(rv2), test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_THAT(request2->GetResolveErrorInfo().error,
              test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_FALSE(request2->GetAddressResults());
}

TEST_F(ContextHostResolverTest, OnShutdown_SubsequentDohProbeRequest) {
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  auto context = CreateTestURLRequestContextBuilder()->Build();
  auto resolve_context = std::make_unique<ResolveContext>(
      context.get(), false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  resolver->OnShutdown();

  std::unique_ptr<HostResolver::ProbeRequest> request =
      resolver->CreateDohProbeRequest();

  EXPECT_THAT(request->Start(), test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_FALSE(dns_client_->factory()->doh_probes_running());
}

// Test a request created before shutdown but not yet started.
TEST_F(ContextHostResolverTest, OnShutdown_DelayedStartRequest) {
  // Set up delayed result for "example.com".
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", IPAddress(2, 3, 4, 5))),
                     true /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);

  auto context = CreateTestURLRequestContextBuilder()->Build();
  auto resolve_context = std::make_unique<ResolveContext>(
      context.get(), false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);

  resolver->OnShutdown();

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  EXPECT_THAT(callback.GetResult(rv), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(request->GetResolveErrorInfo().error,
              test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_FALSE(request->GetAddressResults());
}

TEST_F(ContextHostResolverTest, OnShutdown_DelayedStartDohProbeRequest) {
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  auto context = CreateTestURLRequestContextBuilder()->Build();
  auto resolve_context = std::make_unique<ResolveContext>(
      context.get(), false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  std::unique_ptr<HostResolver::ProbeRequest> request =
      resolver->CreateDohProbeRequest();

  resolver->OnShutdown();

  EXPECT_THAT(request->Start(), test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_FALSE(dns_client_->factory()->doh_probes_running());
}

TEST_F(ContextHostResolverTest, ResolveFromCache) {
  auto resolve_context = std::make_unique<ResolveContext>(
      nullptr /* url_request_context */, true /* enable_caching */);
  HostCache* host_cache = resolve_context->host_cache();
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  // Create the cache entry after creating the ContextHostResolver, as
  // registering into the HostResolverManager initializes and invalidates the
  // cache.
  base::SimpleTestTickClock clock;
  clock.Advance(base::Days(62));  // Arbitrary non-zero time.
  std::vector<IPEndPoint> expected({kEndpoint});
  host_cache->Set(
      HostCache::Key("example.com", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey()),
      HostCache::Entry(OK, expected,
                       /*aliases=*/std::set<std::string>({"example.com"}),
                       HostCache::Entry::SOURCE_DNS, base::Days(1)),
      clock.NowTicks(), base::Days(1));
  resolver->SetTickClockForTesting(&clock);

  // Allow stale results and then confirm the result is not stale in order to
  // make the issue more clear if something is invalidating the cache.
  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::LOCAL_ONLY;
  parameters.cache_usage =
      HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              parameters);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  EXPECT_THAT(callback.GetResult(rv), test::IsOk());
  EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(net::OK));
  EXPECT_THAT(request->GetAddressResults()->endpoints(),
              testing::ElementsAre(kEndpoint));
  ASSERT_TRUE(request->GetStaleInfo());
  EXPECT_EQ(0, request->GetStaleInfo().value().network_changes);
  EXPECT_FALSE(request->GetStaleInfo().value().is_stale());
}

TEST_F(ContextHostResolverTest, ResultsAddedToCache) {
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", kEndpoint.address())),
                     false /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolve_context = std::make_unique<ResolveContext>(
      nullptr /* url_request_context */, true /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  std::unique_ptr<HostResolver::ResolveHostRequest> caching_request =
      resolver->CreateRequest(HostPortPair("example.com", 103),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);
  TestCompletionCallback caching_callback;
  int rv = caching_request->Start(caching_callback.callback());
  EXPECT_THAT(caching_callback.GetResult(rv), test::IsOk());

  HostResolver::ResolveHostParameters local_resolve_parameters;
  local_resolve_parameters.source = HostResolverSource::LOCAL_ONLY;
  std::unique_ptr<HostResolver::ResolveHostRequest> cached_request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              local_resolve_parameters);

  TestCompletionCallback callback;
  rv = cached_request->Start(callback.callback());
  EXPECT_THAT(callback.GetResult(rv), test::IsOk());
  EXPECT_THAT(cached_request->GetResolveErrorInfo().error,
              test::IsError(net::OK));
  EXPECT_THAT(cached_request->GetAddressResults()->endpoints(),
              testing::ElementsAre(kEndpoint));
}

// Do a lookup with a NetworkIsolationKey, and then make sure the entry added to
// the cache is in fact using that NetworkIsolationKey.
TEST_F(ContextHostResolverTest, ResultsAddedToCacheWithNetworkIsolationKey) {
  const SchemefulSite kSite(GURL("https://origin.test/"));
  const NetworkIsolationKey kNetworkIsolationKey(kSite, kSite);
  auto kNetworkAnonymizationKey =
      net::NetworkAnonymizationKey::CreateSameSite(kSite);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", kEndpoint.address())),
                     false /* delay */);
  rules.emplace_back(
      "example.com", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolve_context = std::make_unique<ResolveContext>(
      nullptr /* url_request_context */, true /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  std::unique_ptr<HostResolver::ResolveHostRequest> caching_request =
      resolver->CreateRequest(HostPortPair("example.com", 103),
                              kNetworkAnonymizationKey, NetLogWithSource(),
                              std::nullopt);
  TestCompletionCallback caching_callback;
  int rv = caching_request->Start(caching_callback.callback());
  EXPECT_THAT(caching_callback.GetResult(rv), test::IsOk());

  HostCache::Key cache_key("example.com", DnsQueryType::UNSPECIFIED,
                           0 /* host_resolver_flags */, HostResolverSource::ANY,
                           kNetworkAnonymizationKey);
  EXPECT_TRUE(
      resolver->GetHostCache()->Lookup(cache_key, base::TimeTicks::Now()));

  HostCache::Key cache_key_with_empty_nak(
      "example.com", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  EXPECT_FALSE(resolver->GetHostCache()->Lookup(cache_key_with_empty_nak,
                                                base::TimeTicks::Now()));
}

// Test that the underlying HostCache can receive invalidations from the manager
// and that it safely does not receive invalidations after the resolver (and the
// HostCache) is destroyed.
TEST_F(ContextHostResolverTest, HostCacheInvalidation) {
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  auto resolve_context = std::make_unique<ResolveContext>(
      nullptr /* url_request_context */, true /* enable_caching */);
  ResolveContext* resolve_context_ptr = resolve_context.get();
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  // No invalidations yet (other than the initialization "invalidation" from
  // registering the context).
  ASSERT_EQ(resolve_context_ptr->current_session_for_testing(),
            dns_client_->GetCurrentSession());
  ASSERT_EQ(resolve_context_ptr->host_cache()->network_changes(), 1);

  manager_->InvalidateCachesForTesting();
  EXPECT_EQ(resolve_context_ptr->current_session_for_testing(),
            dns_client_->GetCurrentSession());
  EXPECT_EQ(resolve_context_ptr->host_cache()->network_changes(), 2);

  // Expect manager to be able to safely do invalidations after an individual
  // ContextHostResolver has been destroyed (and deregisters its ResolveContext)
  resolver = nullptr;
  manager_->InvalidateCachesForTesting();
}

class FakeServiceEndpontRequestDelegate
    : public HostResolver::ServiceEndpointRequest::Delegate {
 public:
  void OnServiceEndpointsUpdated() override {}
  void OnServiceEndpointRequestFinished(int rv) override { result_ = rv; }

  std::optional<int> result() const { return result_; }

 private:
  std::optional<int> result_;
};

class ContextHostResolverServiceEndpointTest : public ContextHostResolverTest {
 public:
  ContextHostResolverServiceEndpointTest() = default;

  ~ContextHostResolverServiceEndpointTest() override = default;

  void SetUp() override {
    ContextHostResolverTest::SetUp();

    context_ = CreateTestURLRequestContextBuilder()->Build();

    MockDnsClientRuleList rules;
    rules.emplace_back("example.com", dns_protocol::kTypeA, /*secure=*/false,
                       MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                           "example.com", kEndpoint.address())),
                       /*delay=*/false, context_.get());
    rules.emplace_back(
        "example.com", dns_protocol::kTypeAAAA, /*secure=*/false,
        MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
        /*delay=*/false, context_.get());
    SetMockDnsRules(std::move(rules));
  }

 protected:
  std::unique_ptr<ContextHostResolver> CreateResolver() {
    auto resolve_context = std::make_unique<ResolveContext>(
        context_.get(), /*enable_caching=*/false);
    return std::make_unique<ContextHostResolver>(manager_.get(),
                                                 std::move(resolve_context));
  }

 private:
  std::unique_ptr<URLRequestContext> context_;
};

TEST_F(ContextHostResolverServiceEndpointTest, Resolve) {
  std::unique_ptr<ContextHostResolver> resolver = CreateResolver();

  std::unique_ptr<HostResolver::ServiceEndpointRequest> request =
      resolver->CreateServiceEndpointRequest(
          HostResolver::Host(
              url::SchemeHostPort(url::kHttpsScheme, "example.com", 100)),
          NetworkAnonymizationKey(), NetLogWithSource(),
          HostResolver::ResolveHostParameters());

  FakeServiceEndpontRequestDelegate delegate;
  int rv = request->Start(&delegate);
  EXPECT_THAT(rv, test::IsError(ERR_IO_PENDING));

  RunUntilIdle();
  EXPECT_THAT(*delegate.result(), test::IsOk());
  EXPECT_THAT(request->GetEndpointResults(),
              testing::ElementsAre(
                  ExpectServiceEndpoint(testing::ElementsAre(kEndpoint))));
}

TEST_F(ContextHostResolverServiceEndpointTest, DestroyResolver) {
  std::unique_ptr<ContextHostResolver> resolver = CreateResolver();

  std::unique_ptr<HostResolver::ServiceEndpointRequest> request =
      resolver->CreateServiceEndpointRequest(
          HostResolver::Host(
              url::SchemeHostPort(url::kHttpsScheme, "example.com", 100)),
          NetworkAnonymizationKey(), NetLogWithSource(),
          HostResolver::ResolveHostParameters());

  resolver.reset();

  FakeServiceEndpontRequestDelegate delegate;
  int rv = request->Start(&delegate);
  EXPECT_THAT(rv, test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_THAT(request->GetResolveErrorInfo(),
              ResolveErrorInfo(ERR_CONTEXT_SHUT_DOWN));
}

class NetworkBoundResolveContext : public ResolveContext {
 public:
  NetworkBoundResolveContext(URLRequestContext* url_request_context,
                             bool enable_caching,
                             handles::NetworkHandle target_network)
      : ResolveContext(url_request_context, enable_caching),
        target_network_(target_network) {}

  handles::NetworkHandle GetTargetNetwork() const override {
    return target_network_;
  }

 private:
  const handles::NetworkHandle target_network_;
};

// A mock HostResolverProc which returns different IP addresses based on the
// `network` parameter received.
class NetworkAwareHostResolverProc : public HostResolverProc {
 public:
  NetworkAwareHostResolverProc() : HostResolverProc(nullptr) {}

  NetworkAwareHostResolverProc(const NetworkAwareHostResolverProc&) = delete;
  NetworkAwareHostResolverProc& operator=(const NetworkAwareHostResolverProc&) =
      delete;

  int Resolve(const std::string& host,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* addrlist,
              int* os_error,
              handles::NetworkHandle network) override {
    // Presume failure
    *os_error = 1;
    const auto iter = kResults.find(network);
    if (iter == kResults.end())
      return ERR_NETWORK_CHANGED;

    *os_error = 0;
    *addrlist = AddressList();
    addrlist->push_back(ToIPEndPoint(iter->second));

    return OK;
  }

  int Resolve(const std::string& host,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* addrlist,
              int* os_error) override {
    return Resolve(host, address_family, host_resolver_flags, addrlist,
                   os_error, handles::kInvalidNetworkHandle);
  }

  struct IPv4 {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
  };

  static constexpr int kPort = 100;
  static constexpr auto kResults =
      base::MakeFixedFlatMap<handles::NetworkHandle, IPv4>(
          {{1, IPv4{1, 2, 3, 4}}, {2, IPv4{8, 8, 8, 8}}});

  static IPEndPoint ToIPEndPoint(const IPv4& ipv4) {
    return IPEndPoint(IPAddress(ipv4.a, ipv4.b, ipv4.c, ipv4.d), kPort);
  }

 protected:
  ~NetworkAwareHostResolverProc() override = default;
};

TEST_F(ContextHostResolverTest, ExistingNetworkBoundLookup) {
#if BUILDFLAG(IS_ANDROID)
  auto scoped_mock_network_change_notifier =
      std::make_unique<test::ScopedMockNetworkChangeNotifier>();
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->ForceNetworkHandlesSupported();

  const url::SchemeHostPort host(url::kHttpsScheme, "example.com",
                                 NetworkAwareHostResolverProc::kPort);
  auto resolver_proc = base::MakeRefCounted<NetworkAwareHostResolverProc>();
  ScopedDefaultHostResolverProc scoped_default_host_resolver;
  scoped_default_host_resolver.Init(resolver_proc.get());

  // ResolveContexts bound to a specific network should end up in a call to
  // Resolve with `network` == context.GetTargetNetwork(). Confirm that we do
  // indeed receive the IP address associated with that network.
  for (const auto& iter : NetworkAwareHostResolverProc::kResults) {
    auto network = iter.first;
    auto expected_ipv4 = iter.second;
    auto resolve_context = std::make_unique<NetworkBoundResolveContext>(
        nullptr /* url_request_context */, false /* enable_caching */, network);
    // DNS lookups originated from network-bound ResolveContexts must be
    // resolved through a HostResolverManager bound to the same network.
    auto manager = HostResolverManager::CreateNetworkBoundHostResolverManager(
        HostResolver::ManagerOptions(), network, nullptr /* net_log */);
    auto resolver = std::make_unique<ContextHostResolver>(
        manager.get(), std::move(resolve_context));
    std::unique_ptr<HostResolver::ResolveHostRequest> request =
        resolver->CreateRequest(host, NetworkAnonymizationKey(),
                                NetLogWithSource(), std::nullopt);

    TestCompletionCallback callback;
    int rv = request->Start(callback.callback());
    EXPECT_THAT(callback.GetResult(rv), test::IsOk());
    EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(net::OK));
    ASSERT_EQ(1u, request->GetAddressResults()->endpoints().size());
    EXPECT_THAT(request->GetAddressResults()->endpoints(),
                testing::ElementsAre(
                    NetworkAwareHostResolverProc::ToIPEndPoint(expected_ipv4)));
  }
#else   // !BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Network-bound HostResolverManager are supported only on Android.";
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(ContextHostResolverTest, NotExistingNetworkBoundLookup) {
  const url::SchemeHostPort host(url::kHttpsScheme, "example.com",
                                 NetworkAwareHostResolverProc::kPort);
  auto resolver_proc = base::MakeRefCounted<NetworkAwareHostResolverProc>();
  ScopedDefaultHostResolverProc scoped_default_host_resolver;
  scoped_default_host_resolver.Init(resolver_proc.get());

  // Non-bound ResolveContexts should end up with a call to Resolve with
  // `network` == kInvalidNetwork, which NetworkAwareHostResolverProc fails to
  // resolve.
  auto resolve_context = std::make_unique<ResolveContext>(
      nullptr /* url_request_context */, false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(host, NetworkAnonymizationKey(),
                              NetLogWithSource(), std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  EXPECT_THAT(callback.GetResult(rv),
              test::IsError(net::ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(request->GetResolveErrorInfo().error,
              test::IsError(net::ERR_NETWORK_CHANGED));
}

// Test that the underlying HostCache does not receive invalidations when its
// ResolveContext/HostResolverManager is bound to a network.
TEST_F(ContextHostResolverTest, NetworkBoundResolverCacheInvalidation) {
#if BUILDFLAG(IS_ANDROID)
  auto scoped_mock_network_change_notifier =
      std::make_unique<test::ScopedMockNetworkChangeNotifier>();
  test::MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();

  // The actual network handle doesn't really matter, this test just wants to
  // check that all the pieces are in place and configured correctly.
  constexpr handles::NetworkHandle network = 2;
  manager_ = HostResolverManager::CreateNetworkBoundHostResolverManager(
      HostResolver::ManagerOptions(), network, nullptr /* net_log */);
  manager_->SetLastIPv6ProbeResultForTesting(true);
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  auto resolve_context = std::make_unique<NetworkBoundResolveContext>(
      nullptr /* url_request_context */, true /* enable_caching */, network);
  ResolveContext* resolve_context_ptr = resolve_context.get();
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  // Network events should not trigger cache invalidations
  auto network_changes_before_events =
      resolve_context_ptr->host_cache()->network_changes();
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
      NetworkChangeNotifier::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(network_changes_before_events,
            resolve_context_ptr->host_cache()->network_changes());
#else   // !BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Network-bound HostResolverManagers are supported only on Android";
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace net
