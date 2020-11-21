// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/context_host_resolver.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_source.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log_with_source.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
    config.nameservers.push_back(
        IPEndPoint(dns_ip, dns_protocol::kDefaultPort));
    config.dns_over_https_servers.emplace_back("example.com",
                                               true /* use_post */);
    EXPECT_TRUE(config.IsValid());

    auto dns_client =
        std::make_unique<MockDnsClient>(std::move(config), std::move(rules));
    dns_client->set_ignore_system_config_changes(true);
    dns_client_ = dns_client.get();
    manager_->SetDnsClientForTesting(std::move(dns_client));
    manager_->SetInsecureDnsClientEnabled(true);

    // Ensure DnsClient is fully usable.
    EXPECT_TRUE(dns_client_->CanUseInsecureDnsTransactions());
    EXPECT_FALSE(dns_client_->FallbackFromInsecureTransactionPreferred());
    EXPECT_TRUE(dns_client_->GetEffectiveConfig());

    scoped_refptr<HostResolverProc> proc = CreateCatchAllHostResolverProc();
    manager_->set_proc_params_for_test(ProcTaskParams(proc.get(), 1u));
  }

  MockDnsClient* dns_client_;
  std::unique_ptr<HostResolverManager> manager_;
};

TEST_F(ContextHostResolverTest, Resolve) {
  URLRequestContext context;

  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", kEndpoint.address())),
                     false /* delay */, &context);
  rules.emplace_back("example.com", dns_protocol::kTypeAAAA, false /* secure */,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */, &context);
  SetMockDnsRules(std::move(rules));

  auto resolve_context =
      std::make_unique<ResolveContext>(&context, false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  EXPECT_THAT(callback.GetResult(rv), test::IsOk());
  EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(net::OK));
  EXPECT_THAT(request->GetAddressResults().value().endpoints(),
              testing::ElementsAre(kEndpoint));
}

// Test that destroying a request silently cancels that request.
TEST_F(ContextHostResolverTest, DestroyRequest) {
  // Set up delayed results for "example.com".
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", IPAddress(1, 2, 3, 4))),
                     true /* delay */);
  rules.emplace_back("example.com", dns_protocol::kTypeAAAA, false /* secure */,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(),
      std::make_unique<ResolveContext>(nullptr /* url_request_context */,
                                       false /* enable_caching */));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);
  EXPECT_EQ(1u, resolver->GetNumActiveRequestsForTesting());

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  // Cancel |request| before allowing delayed result to complete.
  request = nullptr;
  dns_client_->CompleteDelayedTransactions();

  // Ensure |request| never completes.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(rv, test::IsError(ERR_IO_PENDING));
  EXPECT_FALSE(callback.have_result());
  EXPECT_EQ(0u, resolver->GetNumActiveRequestsForTesting());
}

TEST_F(ContextHostResolverTest, DohProbeRequest) {
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  URLRequestContext context;
  auto resolve_context =
      std::make_unique<ResolveContext>(&context, true /* enable caching */);
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
  rules.emplace_back("example.com", dns_protocol::kTypeAAAA, false /* secure */,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */);
  rules.emplace_back("google.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "google.com", kEndpoint.address())),
                     true /* delay */);
  rules.emplace_back("google.com", dns_protocol::kTypeAAAA, false /* secure */,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolver1 = std::make_unique<ContextHostResolver>(
      manager_.get(),
      std::make_unique<ResolveContext>(nullptr /* url_request_context */,
                                       false /* enable_caching */));
  std::unique_ptr<HostResolver::ResolveHostRequest> request1 =
      resolver1->CreateRequest(HostPortPair("example.com", 100),
                               NetworkIsolationKey(), NetLogWithSource(),
                               base::nullopt);
  auto resolver2 = std::make_unique<ContextHostResolver>(
      manager_.get(),
      std::make_unique<ResolveContext>(nullptr /* url_request_context */,
                                       false /* enable_caching */));
  std::unique_ptr<HostResolver::ResolveHostRequest> request2 =
      resolver2->CreateRequest(HostPortPair("google.com", 100),
                               NetworkIsolationKey(), NetLogWithSource(),
                               base::nullopt);

  TestCompletionCallback callback1;
  int rv1 = request1->Start(callback1.callback());
  TestCompletionCallback callback2;
  int rv2 = request2->Start(callback2.callback());

  EXPECT_EQ(2u, manager_->num_jobs_for_testing());

  // Cancel |resolver1| before allowing delayed requests to complete.
  resolver1 = nullptr;
  dns_client_->CompleteDelayedTransactions();

  EXPECT_THAT(callback2.GetResult(rv2), test::IsOk());
  EXPECT_THAT(request2->GetAddressResults().value().endpoints(),
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
  rules.emplace_back("example.com", dns_protocol::kTypeAAAA, false /* secure */,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(),
      std::make_unique<ResolveContext>(nullptr /* url_request_context */,
                                       false /* enable_caching */));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);

  // Complete request and then destroy the resolver.
  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  ASSERT_THAT(callback.GetResult(rv), test::IsOk());
  resolver = nullptr;

  // Expect completed results are still available.
  EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(net::OK));
  EXPECT_THAT(request->GetAddressResults().value().endpoints(),
              testing::ElementsAre(kEndpoint));
}

TEST_F(ContextHostResolverTest, DestroyResolver_DohProbeRequest) {
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  URLRequestContext context;
  auto resolve_context =
      std::make_unique<ResolveContext>(&context, false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  std::unique_ptr<HostResolver::ProbeRequest> request =
      resolver->CreateDohProbeRequest();

  request->Start();
  ASSERT_TRUE(dns_client_->factory()->doh_probes_running());

  resolver.reset();

  EXPECT_FALSE(dns_client_->factory()->doh_probes_running());
}

// Test a request created before resolver destruction but not yet started.
TEST_F(ContextHostResolverTest, DestroyResolver_DelayedStartRequest) {
  // Set up delayed result for "example.com".
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", IPAddress(2, 3, 4, 5))),
                     true /* delay */);
  rules.emplace_back("example.com", dns_protocol::kTypeAAAA, false /* secure */,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */);

  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(),
      std::make_unique<ResolveContext>(nullptr /* url_request_context */,
                                       false /* enable_caching */));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);

  resolver = nullptr;

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  EXPECT_THAT(callback.GetResult(rv), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(ERR_FAILED));
  EXPECT_FALSE(request->GetAddressResults());
}

TEST_F(ContextHostResolverTest, DestroyResolver_DelayedStartDohProbeRequest) {
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  URLRequestContext context;
  auto resolve_context =
      std::make_unique<ResolveContext>(&context, false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  std::unique_ptr<HostResolver::ProbeRequest> request =
      resolver->CreateDohProbeRequest();

  resolver = nullptr;

  EXPECT_THAT(request->Start(), test::IsError(ERR_FAILED));
  EXPECT_FALSE(dns_client_->factory()->doh_probes_running());
}

TEST_F(ContextHostResolverTest, OnShutdown_PendingRequest) {
  // Set up delayed result for "example.com".
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", IPAddress(2, 3, 4, 5))),
                     true /* delay */);
  rules.emplace_back("example.com", dns_protocol::kTypeAAAA, false /* secure */,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */);
  SetMockDnsRules(std::move(rules));

  URLRequestContext context;
  auto resolve_context =
      std::make_unique<ResolveContext>(&context, false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);

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

TEST_F(ContextHostResolverTest, OnShutdown_DohProbeRequest) {
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  URLRequestContext context;
  auto resolve_context =
      std::make_unique<ResolveContext>(&context, false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  std::unique_ptr<HostResolver::ProbeRequest> request =
      resolver->CreateDohProbeRequest();

  request->Start();
  ASSERT_TRUE(dns_client_->factory()->doh_probes_running());

  resolver->OnShutdown();

  EXPECT_FALSE(dns_client_->factory()->doh_probes_running());
}

TEST_F(ContextHostResolverTest, OnShutdown_CompletedRequests) {
  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", kEndpoint.address())),
                     false /* delay */);
  rules.emplace_back("example.com", dns_protocol::kTypeAAAA, false /* secure */,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */);
  SetMockDnsRules(std::move(rules));

  URLRequestContext context;
  auto resolve_context =
      std::make_unique<ResolveContext>(&context, false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);

  // Complete request and then shutdown the resolver.
  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  ASSERT_THAT(callback.GetResult(rv), test::IsOk());
  resolver->OnShutdown();

  // Expect completed results are still available.
  EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(net::OK));
  EXPECT_THAT(request->GetAddressResults().value().endpoints(),
              testing::ElementsAre(kEndpoint));
}

TEST_F(ContextHostResolverTest, OnShutdown_SubsequentRequests) {
  URLRequestContext context;
  auto resolve_context =
      std::make_unique<ResolveContext>(&context, false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  resolver->OnShutdown();

  std::unique_ptr<HostResolver::ResolveHostRequest> request1 =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);
  std::unique_ptr<HostResolver::ResolveHostRequest> request2 =
      resolver->CreateRequest(HostPortPair("127.0.0.1", 100),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);

  TestCompletionCallback callback1;
  int rv1 = request1->Start(callback1.callback());
  TestCompletionCallback callback2;
  int rv2 = request2->Start(callback2.callback());

  EXPECT_THAT(callback1.GetResult(rv1), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(request1->GetResolveErrorInfo().error,
              test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_FALSE(request1->GetAddressResults());
  EXPECT_THAT(callback2.GetResult(rv2), test::IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(request2->GetResolveErrorInfo().error,
              test::IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_FALSE(request2->GetAddressResults());
}

TEST_F(ContextHostResolverTest, OnShutdown_SubsequentDohProbeRequest) {
  // Set empty MockDnsClient rules to ensure DnsClient is mocked out.
  MockDnsClientRuleList rules;
  SetMockDnsRules(std::move(rules));

  URLRequestContext context;
  auto resolve_context =
      std::make_unique<ResolveContext>(&context, false /* enable_caching */);
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
  rules.emplace_back("example.com", dns_protocol::kTypeAAAA, false /* secure */,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */);

  URLRequestContext context;
  auto resolve_context =
      std::make_unique<ResolveContext>(&context, false /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);

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

  URLRequestContext context;
  auto resolve_context =
      std::make_unique<ResolveContext>(&context, false /* enable_caching */);
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
  clock.Advance(base::TimeDelta::FromDays(62));  // Arbitrary non-zero time.
  AddressList expected(kEndpoint);
  host_cache->Set(
      HostCache::Key("example.com", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkIsolationKey()),
      HostCache::Entry(OK, expected, HostCache::Entry::SOURCE_DNS,
                       base::TimeDelta::FromDays(1)),
      clock.NowTicks(), base::TimeDelta::FromDays(1));
  resolver->SetTickClockForTesting(&clock);

  // Allow stale results and then confirm the result is not stale in order to
  // make the issue more clear if something is invalidating the cache.
  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::LOCAL_ONLY;
  parameters.cache_usage =
      HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkIsolationKey(), NetLogWithSource(),
                              parameters);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  EXPECT_THAT(callback.GetResult(rv), test::IsOk());
  EXPECT_THAT(request->GetResolveErrorInfo().error, test::IsError(net::OK));
  EXPECT_THAT(request->GetAddressResults().value().endpoints(),
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
  rules.emplace_back("example.com", dns_protocol::kTypeAAAA, false /* secure */,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolve_context = std::make_unique<ResolveContext>(
      nullptr /* url_request_context */, true /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  std::unique_ptr<HostResolver::ResolveHostRequest> caching_request =
      resolver->CreateRequest(HostPortPair("example.com", 103),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);
  TestCompletionCallback caching_callback;
  int rv = caching_request->Start(caching_callback.callback());
  EXPECT_THAT(caching_callback.GetResult(rv), test::IsOk());

  HostResolver::ResolveHostParameters local_resolve_parameters;
  local_resolve_parameters.source = HostResolverSource::LOCAL_ONLY;
  std::unique_ptr<HostResolver::ResolveHostRequest> cached_request =
      resolver->CreateRequest(HostPortPair("example.com", 100),
                              NetworkIsolationKey(), NetLogWithSource(),
                              local_resolve_parameters);

  TestCompletionCallback callback;
  rv = cached_request->Start(callback.callback());
  EXPECT_THAT(callback.GetResult(rv), test::IsOk());
  EXPECT_THAT(cached_request->GetResolveErrorInfo().error,
              test::IsError(net::OK));
  EXPECT_THAT(cached_request->GetAddressResults().value().endpoints(),
              testing::ElementsAre(kEndpoint));
}

// Do a lookup with a NetworkIsolationKey, and then make sure the entry added to
// the cache is in fact using that NetworkIsolationKey.
TEST_F(ContextHostResolverTest, ResultsAddedToCacheWithNetworkIsolationKey) {
  const SchemefulSite kSite(GURL("https://origin.test/"));
  const NetworkIsolationKey kNetworkIsolationKey(kSite, kSite);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kSplitHostCacheByNetworkIsolationKey);

  MockDnsClientRuleList rules;
  rules.emplace_back("example.com", dns_protocol::kTypeA, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsAddressResponse(
                         "example.com", kEndpoint.address())),
                     false /* delay */);
  rules.emplace_back("example.com", dns_protocol::kTypeAAAA, false /* secure */,
                     MockDnsClientRule::Result(MockDnsClientRule::EMPTY),
                     false /* delay */);
  SetMockDnsRules(std::move(rules));

  auto resolve_context = std::make_unique<ResolveContext>(
      nullptr /* url_request_context */, true /* enable_caching */);
  auto resolver = std::make_unique<ContextHostResolver>(
      manager_.get(), std::move(resolve_context));

  std::unique_ptr<HostResolver::ResolveHostRequest> caching_request =
      resolver->CreateRequest(HostPortPair("example.com", 103),
                              kNetworkIsolationKey, NetLogWithSource(),
                              base::nullopt);
  TestCompletionCallback caching_callback;
  int rv = caching_request->Start(caching_callback.callback());
  EXPECT_THAT(caching_callback.GetResult(rv), test::IsOk());

  HostCache::Key cache_key("example.com", DnsQueryType::UNSPECIFIED,
                           0 /* host_resolver_flags */, HostResolverSource::ANY,
                           kNetworkIsolationKey);
  EXPECT_TRUE(
      resolver->GetHostCache()->Lookup(cache_key, base::TimeTicks::Now()));

  HostCache::Key cache_key_with_empty_nik(
      "example.com", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkIsolationKey());
  EXPECT_FALSE(resolver->GetHostCache()->Lookup(cache_key_with_empty_nik,
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

}  // namespace net
