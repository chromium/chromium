// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "net/base/address_family.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/request_priority.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_task_results_manager.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager_service_endpoint_request_impl.h"
#include "net/dns/host_resolver_manager_unittest.h"
#include "net/dns/host_resolver_results_test_util.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log_with_source.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

using net::test::IsError;
using net::test::IsOk;

namespace net {

using ServiceEndpointRequest = HostResolver::ServiceEndpointRequest;
using ResolveHostRequest = HostResolver::ResolveHostRequest;
using ResolveHostParameters = HostResolver::ResolveHostParameters;

namespace {

IPEndPoint MakeIPEndPoint(std::string_view ip_literal, uint16_t port = 0) {
  std::optional<IPAddress> ip = IPAddress::FromIPLiteral(std::move(ip_literal));
  return IPEndPoint(*ip, port);
}

// Sorts endpoints using IPAddress's comparator.
class FakeAddressSorter : public AddressSorter {
 public:
  void Sort(const std::vector<IPEndPoint>& endpoints,
            CallbackType callback) const override {
    std::vector<IPEndPoint> sorted = endpoints;
    std::sort(sorted.begin(), sorted.end(),
              [](const IPEndPoint& a, const IPEndPoint& b) {
                return a.address() < b.address();
              });
    std::move(callback).Run(true, sorted);
  }
};

class Requester : public ServiceEndpointRequest::Delegate {
 public:
  explicit Requester(std::unique_ptr<ServiceEndpointRequest> request)
      : request_(std::move(request)) {}

  ~Requester() override = default;

  // ServiceEndpointRequest::Delegate overrides:

  void OnServiceEndpointsUpdated() override {
    if (on_updated_callback_) {
      std::move(on_updated_callback_).Run();
    }
  }

  void OnServiceEndpointRequestFinished(int rv) override {
    SetFinishedResult(rv);

    if (on_finished_callback_) {
      std::move(on_finished_callback_).Run();
    }

    if (wait_for_finished_callback_) {
      std::move(wait_for_finished_callback_).Run();
    }
  }

  int Start() {
    int rv = request_->Start(this);
    if (rv != ERR_IO_PENDING) {
      SetFinishedResult(rv);
    }
    return rv;
  }

  void CancelRequest() { request_.reset(); }

  void CancelRequestOnUpdated() {
    SetOnUpdatedCallback(base::BindLambdaForTesting([&] { CancelRequest(); }));
  }

  void CancelRequestOnFinished() {
    SetOnFinishedCallback(base::BindLambdaForTesting([&] { CancelRequest(); }));
  }

  void SetOnUpdatedCallback(base::OnceClosure callback) {
    CHECK(!finished_result_);
    CHECK(!on_updated_callback_);
    on_updated_callback_ = std::move(callback);
  }

  void SetOnFinishedCallback(base::OnceClosure callback) {
    CHECK(!finished_result_);
    CHECK(!on_finished_callback_);
    on_finished_callback_ = std::move(callback);
  }

  void WaitForFinished() {
    CHECK(!finished_result_);
    CHECK(!wait_for_finished_callback_);
    base::RunLoop run_loop;
    wait_for_finished_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForOnUpdated() {
    base::RunLoop run_loop;
    SetOnUpdatedCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  ServiceEndpointRequest* request() const { return request_.get(); }

  std::optional<int> finished_result() const { return finished_result_; }

  const std::vector<ServiceEndpoint>& finished_endpoints() const {
    CHECK(finished_result_.has_value());
    return finished_endpoints_;
  }

 private:
  void SetFinishedResult(int rv) {
    CHECK(!finished_result_);
    finished_result_ = rv;

    if (request_) {
      finished_endpoints_ = request_->GetEndpointResults();
    }
  }

  std::unique_ptr<ServiceEndpointRequest> request_;

  std::optional<int> finished_result_;
  std::vector<ServiceEndpoint> finished_endpoints_;

  base::OnceClosure wait_for_finished_callback_;
  base::OnceClosure on_updated_callback_;
  base::OnceClosure on_finished_callback_;
};

class LegacyRequester {
 public:
  explicit LegacyRequester(std::unique_ptr<ResolveHostRequest> request)
      : request_(std::move(request)) {}

  ~LegacyRequester() = default;

  int Start() {
    return request_->Start(
        base::BindOnce(&LegacyRequester::OnComplete, base::Unretained(this)));
  }

  void CancelRequest() { request_.reset(); }

  std::optional<int> complete_result() const { return complete_result_; }

 private:
  void OnComplete(int rv) { complete_result_ = rv; }

  std::unique_ptr<ResolveHostRequest> request_;
  std::optional<int> complete_result_;
};

}  // namespace

class HostResolverServiceEndpointRequestTest
    : public HostResolverManagerDnsTest {
 public:
  HostResolverServiceEndpointRequestTest() {
    feature_list_.InitAndEnableFeature(features::kHappyEyeballsV3);
  }

  ~HostResolverServiceEndpointRequestTest() override = default;

 protected:
  void SetUp() override {
    HostResolverManagerDnsTest::SetUp();

    // MockHostResolverProc resolves all requests to "127.0.0.1" when there is
    // no rule. Add a rule to prevent the default behavior.
    proc_->AddRule(std::string(), ADDRESS_FAMILY_UNSPECIFIED, "192.0.2.1");
  }

  void set_globally_reachable_check_is_async(bool is_async) {
    globally_reachable_check_is_async_ = is_async;
  }

  void set_ipv6_reachable(bool reachable) { ipv6_reachable_ = reachable; }

  void SetDnsRules(MockDnsClientRuleList rules) {
    CreateResolverWithOptionsAndParams(
        DefaultOptions(),
        HostResolverSystemTask::Params(proc_,
                                       /*max_retry_attempts=*/1),
        ipv6_reachable_,
        /*is_async=*/globally_reachable_check_is_async_,
        /*ipv4_reachable=*/true);
    UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  }

  void UseNoDomanDnsRules(const std::string& host) {
    MockDnsClientRuleList rules;
    AddDnsRule(&rules, host, dns_protocol::kTypeA,
               MockDnsClientRule::ResultType::kNoDomain, /*delay=*/false);
    AddDnsRule(&rules, host, dns_protocol::kTypeAAAA,
               MockDnsClientRule::ResultType::kNoDomain, /*delay=*/false);
    SetDnsRules(std::move(rules));
  }

  void UseTimedOutDnsRules(const std::string& host) {
    MockDnsClientRuleList rules;
    AddDnsRule(&rules, host, dns_protocol::kTypeA,
               MockDnsClientRule::ResultType::kTimeout, /*delay=*/false);
    AddDnsRule(&rules, host, dns_protocol::kTypeAAAA,
               MockDnsClientRule::ResultType::kTimeout, /*delay=*/false);
    SetDnsRules(std::move(rules));
  }

  void UseNonDelayedDnsRules(const std::string& host) {
    MockDnsClientRuleList rules;
    AddDnsRule(&rules, host, dns_protocol::kTypeA,
               MockDnsClientRule::ResultType::kOk, /*delay=*/false);
    AddDnsRule(&rules, host, dns_protocol::kTypeAAAA,
               MockDnsClientRule::ResultType::kOk, /*delay=*/false);
    SetDnsRules(std::move(rules));
  }

  void UseIpv4DelayedDnsRules(const std::string& host) {
    MockDnsClientRuleList rules;
    AddDnsRule(&rules, host, dns_protocol::kTypeA,
               MockDnsClientRule::ResultType::kOk, /*delay=*/true);
    AddDnsRule(&rules, host, dns_protocol::kTypeAAAA,
               MockDnsClientRule::ResultType::kOk, /*delay=*/false);
    SetDnsRules(std::move(rules));
  }

  void UseIpv6DelayedDnsRules(const std::string& host) {
    MockDnsClientRuleList rules;
    AddDnsRule(&rules, host, dns_protocol::kTypeA,
               MockDnsClientRule::ResultType::kOk, /*delay=*/false);
    AddDnsRule(&rules, host, dns_protocol::kTypeAAAA,
               MockDnsClientRule::ResultType::kOk, /*delay=*/true);
    SetDnsRules(std::move(rules));
  }

  void UseHttpsDelayedDnsRules(const std::string& host) {
    MockDnsClientRuleList rules;
    AddDnsRule(&rules, host, dns_protocol::kTypeA,
               MockDnsClientRule::ResultType::kOk, /*delay=*/false);
    AddDnsRule(&rules, host, dns_protocol::kTypeAAAA,
               MockDnsClientRule::ResultType::kOk, /*delay=*/false);

    std::vector<DnsResourceRecord> records = {
        BuildTestHttpsServiceRecord(host, /*priority=*/1, /*service_name=*/".",
                                    /*params=*/{})};
    rules.emplace_back(host, dns_protocol::kTypeHttps,
                       /*secure=*/false,
                       MockDnsClientRule::Result(BuildTestDnsResponse(
                           host, dns_protocol::kTypeHttps, records)),
                       /*delay=*/true);
    SetDnsRules(std::move(rules));
  }

  std::unique_ptr<ServiceEndpointRequest> CreateRequest(
      std::string_view host,
      ResolveHostParameters parameters = ResolveHostParameters()) {
    return resolver_->CreateServiceEndpointRequest(
        url::SchemeHostPort(GURL(host)), NetworkAnonymizationKey(),
        NetLogWithSource(), std::move(parameters), resolve_context_.get());
  }

  Requester CreateRequester(
      std::string_view host,
      ResolveHostParameters parameters = ResolveHostParameters()) {
    return Requester(CreateRequest(host, std::move(parameters)));
  }

  LegacyRequester CreateLegacyRequester(std::string_view host) {
    return LegacyRequester(resolver_->CreateRequest(
        url::SchemeHostPort(GURL(host)), NetworkAnonymizationKey(),
        NetLogWithSource(), ResolveHostParameters(), resolve_context_.get()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  bool ipv6_reachable_ = true;
  bool globally_reachable_check_is_async_ = false;
};

TEST_F(HostResolverServiceEndpointRequestTest, NameNotResolved) {
  UseNoDomanDnsRules("nodomain");

  proc_->SignalMultiple(1u);
  Requester requester = CreateRequester("https://nodomain");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  requester.WaitForFinished();
  EXPECT_THAT(*requester.finished_result(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(requester.request()->GetResolveErrorInfo(),
              ResolveErrorInfo(ERR_NAME_NOT_RESOLVED));
}

TEST_F(HostResolverServiceEndpointRequestTest, TimedOut) {
  UseTimedOutDnsRules("timeout");
  set_allow_fallback_to_systemtask(false);

  proc_->SignalMultiple(1u);
  Requester requester = CreateRequester("https://timeout");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  requester.WaitForFinished();
  EXPECT_THAT(requester.finished_result(),
              Optional(IsError(ERR_NAME_NOT_RESOLVED)));
  EXPECT_THAT(requester.request()->GetResolveErrorInfo().error,
              IsError(ERR_DNS_TIMED_OUT));
}

// Tests that a request returns valid endpoints and DNS aliases after DnsTasks
// are aborted.
TEST_F(HostResolverServiceEndpointRequestTest, KillDnsTask) {
  UseIpv4DelayedDnsRules("4slow_ok");

  proc_->SignalMultiple(1u);
  Requester requester = CreateRequester("https://4slow_ok");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  requester.WaitForOnUpdated();

  // Simulate the case when the preference or policy has disabled the insecure
  // DNS client causing AbortInsecureDnsTasks.
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/false, /*additional_dns_types_enabled=*/false);
  ASSERT_TRUE(requester.request()->GetEndpointResults().empty());
  ASSERT_TRUE(requester.request()->GetDnsAliasResults().empty());
}

TEST_F(HostResolverServiceEndpointRequestTest, Ok) {
  UseNonDelayedDnsRules("ok");

  Requester requester = CreateRequester("https://ok");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  requester.WaitForFinished();
  EXPECT_THAT(*requester.finished_result(), IsOk());
  EXPECT_THAT(requester.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));
}

TEST_F(HostResolverServiceEndpointRequestTest,
       Ipv6GloballyReachableCheckAsyncOk) {
  set_globally_reachable_check_is_async(true);
  UseNonDelayedDnsRules("ok");

  Requester requester = CreateRequester("https://ok");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  requester.WaitForFinished();
  EXPECT_THAT(*requester.finished_result(), IsOk());
  EXPECT_THAT(requester.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));
}

TEST_F(HostResolverServiceEndpointRequestTest, Ipv6GloballyReachableCheckFail) {
  set_ipv6_reachable(false);
  set_globally_reachable_check_is_async(true);
  UseNonDelayedDnsRules("ok");

  Requester requester = CreateRequester("https://ok");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  requester.WaitForFinished();
  EXPECT_THAT(*requester.finished_result(), IsOk());
  EXPECT_THAT(requester.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)))));
  EXPECT_FALSE(GetLastIpv6ProbeResult());
}

TEST_F(HostResolverServiceEndpointRequestTest, ResolveLocally) {
  UseNonDelayedDnsRules("ok");

  // The first local only request should complete synchronously with a cache
  // miss.
  {
    ResolveHostParameters parameters;
    parameters.source = HostResolverSource::LOCAL_ONLY;
    Requester requester = CreateRequester("https://ok", std::move(parameters));
    int rv = requester.Start();
    EXPECT_THAT(rv, IsError(ERR_DNS_CACHE_MISS));
    EXPECT_THAT(requester.request()->GetResolveErrorInfo(),
                ResolveErrorInfo(ERR_DNS_CACHE_MISS));
  }

  // Populate the cache.
  {
    Requester requester = CreateRequester("https://ok");
    int rv = requester.Start();
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    requester.WaitForFinished();
    EXPECT_THAT(*requester.finished_result(), IsOk());
    EXPECT_THAT(requester.finished_endpoints(),
                ElementsAre(ExpectServiceEndpoint(
                    ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                    ElementsAre(MakeIPEndPoint("::1", 443)))));
  }

  // The second local only request should complete synchronously with a cache
  // hit.
  {
    ResolveHostParameters parameters;
    parameters.source = HostResolverSource::LOCAL_ONLY;
    Requester requester = CreateRequester("https://ok", std::move(parameters));
    int rv = requester.Start();
    EXPECT_THAT(rv, IsOk());
    EXPECT_THAT(requester.finished_endpoints(),
                ElementsAre(ExpectServiceEndpoint(
                    ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                    ElementsAre(MakeIPEndPoint("::1", 443)))));
  }
}

// Test that a local only request fails due to a blocked reachability check.
TEST_F(HostResolverServiceEndpointRequestTest,
       Ipv6GloballyReachableCheckAsyncLocalOnly) {
  set_globally_reachable_check_is_async(true);
  UseNonDelayedDnsRules("ok");

  ResolveHostParameters parameters;
  parameters.source = HostResolverSource::LOCAL_ONLY;
  Requester requester = CreateRequester("https://ok", std::move(parameters));
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_NAME_NOT_RESOLVED));
}

TEST_F(HostResolverServiceEndpointRequestTest, EndpointsAreSorted) {
  MockDnsClientRuleList rules;
  constexpr const char* kHost = "multiple";

  DnsResponse a_response = BuildTestDnsResponse(
      kHost, dns_protocol::kTypeA,
      {BuildTestAddressRecord(kHost, *IPAddress::FromIPLiteral("192.0.2.2")),
       BuildTestAddressRecord(kHost, *IPAddress::FromIPLiteral("192.0.2.1"))});
  DnsResponse aaaa_response = BuildTestDnsResponse(
      kHost, dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord(kHost, *IPAddress::FromIPLiteral("2001:db8::2")),
       BuildTestAddressRecord(kHost,
                              *IPAddress::FromIPLiteral("2001:db8::1"))});
  AddDnsRule(&rules, kHost, dns_protocol::kTypeA, std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, kHost, dns_protocol::kTypeAAAA, std::move(aaaa_response),
             /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(
      std::make_unique<FakeAddressSorter>());

  Requester requester = CreateRequester("https://multiple");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  requester.WaitForFinished();
  EXPECT_THAT(*requester.finished_result(), IsOk());
  EXPECT_THAT(requester.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443),
                              MakeIPEndPoint("192.0.2.2", 443)),
                  ElementsAre(MakeIPEndPoint("2001:db8::1", 443),
                              MakeIPEndPoint("2001:db8::2", 443)))));
}

TEST_F(HostResolverServiceEndpointRequestTest, CancelRequestOnUpdated) {
  UseIpv4DelayedDnsRules("4slow_ok");

  Requester requester = CreateRequester("https://4slow_ok");
  requester.CancelRequestOnUpdated();
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  RunUntilIdle();
  // The finished callback should not be called because the request was
  // already cancelled.
  ASSERT_FALSE(requester.finished_result().has_value());
  ASSERT_FALSE(requester.request());
}

TEST_F(HostResolverServiceEndpointRequestTest, CancelRequestOnFinished) {
  UseIpv4DelayedDnsRules("4slow_ok");

  Requester requester = CreateRequester("https://4slow_ok");
  requester.CancelRequestOnFinished();
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  mock_dns_client_->CompleteDelayedTransactions();
  requester.WaitForFinished();
  // The result should be OK because we cancel the request after completing the
  // associated Job.
  EXPECT_THAT(*requester.finished_result(), IsOk());
}

TEST_F(HostResolverServiceEndpointRequestTest, Ipv4Slow) {
  UseIpv4DelayedDnsRules("4slow_ok");

  Requester requester = CreateRequester("https://4slow_ok");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  // AAAA and HTTPS should complete.
  RunUntilIdle();
  EXPECT_EQ(1u, resolver_->num_running_dispatcher_jobs_for_tests());
  ASSERT_FALSE(requester.finished_result().has_value());
  ASSERT_TRUE(requester.request()->EndpointsCryptoReady());
  EXPECT_THAT(requester.request()->GetEndpointResults(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("::1", 443)))));
  EXPECT_THAT(requester.request()->GetDnsAliasResults(),
              UnorderedElementsAre("4slow_ok"));

  // Complete A request, which finishes the request synchronously.
  mock_dns_client_->CompleteDelayedTransactions();
  ASSERT_TRUE(requester.request()->EndpointsCryptoReady());
  EXPECT_THAT(*requester.finished_result(), IsOk());
  EXPECT_THAT(requester.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));
  EXPECT_THAT(requester.request()->GetDnsAliasResults(),
              UnorderedElementsAre("4slow_ok"));
}

TEST_F(HostResolverServiceEndpointRequestTest, Ipv6Slow) {
  UseIpv6DelayedDnsRules("6slow_ok");

  Requester requester = CreateRequester("https://6slow_ok");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  // A and HTTPS should complete, but no endpoints should be available since
  // waiting for AAAA response.
  RunUntilIdle();
  EXPECT_EQ(1u, resolver_->num_running_dispatcher_jobs_for_tests());
  ASSERT_FALSE(requester.finished_result().has_value());
  ASSERT_TRUE(requester.request()->EndpointsCryptoReady());
  EXPECT_THAT(requester.request()->GetEndpointResults(), IsEmpty());

  // Complete AAAA request, which finishes the request synchronously.
  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(*requester.finished_result(), IsOk());
  EXPECT_THAT(requester.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));
}

TEST_F(HostResolverServiceEndpointRequestTest, Ipv6SlowResolutionDelayPassed) {
  UseIpv6DelayedDnsRules("6slow_ok");

  Requester requester = CreateRequester("https://6slow_ok");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  // A and HTTPS should complete, but no endpoints should be available since
  // waiting for AAAA response.
  RunUntilIdle();
  EXPECT_EQ(1u, resolver_->num_running_dispatcher_jobs_for_tests());
  ASSERT_FALSE(requester.finished_result().has_value());
  ASSERT_TRUE(requester.request()->EndpointsCryptoReady());
  EXPECT_THAT(requester.request()->GetEndpointResults(), IsEmpty());

  // The resolution delay timer fired, IPv4 endpoints should be available.
  FastForwardBy(DnsTaskResultsManager::kResolutionDelay +
                base::Milliseconds(1));
  RunUntilIdle();
  EXPECT_EQ(1u, resolver_->num_running_dispatcher_jobs_for_tests());
  ASSERT_FALSE(requester.finished_result().has_value());
  EXPECT_THAT(requester.request()->GetEndpointResults(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)), IsEmpty())));

  // Complete AAAA request, which finishes the request synchronously.
  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(*requester.finished_result(), IsOk());
  EXPECT_THAT(requester.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));
}

TEST_F(HostResolverServiceEndpointRequestTest, HttpsSlow) {
  UseHttpsDelayedDnsRules("https_slow_ok");

  Requester requester = CreateRequester("https://https_slow_ok");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  // A and AAAA should complete.
  RunUntilIdle();
  EXPECT_EQ(1u, resolver_->num_running_dispatcher_jobs_for_tests());
  ASSERT_FALSE(requester.finished_result().has_value());
  ASSERT_FALSE(requester.request()->EndpointsCryptoReady());
  EXPECT_THAT(requester.request()->GetEndpointResults(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));

  // Complete HTTPS request, which finishes the request synchronously.
  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(*requester.finished_result(), IsOk());
  EXPECT_THAT(
      requester.finished_endpoints(),
      ElementsAre(
          ExpectServiceEndpoint(
              ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
              ElementsAre(MakeIPEndPoint("::1", 443)),
              ConnectionEndpointMetadata(
                  /*supported_protocol_alpns=*/{"http/1.1"},
                  /*ech_config_list=*/{}, std::string("https_slow_ok"))),
          // Non-SVCB endpoints.
          ExpectServiceEndpoint(ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                                ElementsAre(MakeIPEndPoint("::1", 443)))));
}

TEST_F(HostResolverServiceEndpointRequestTest, DestroyResolverWhileUpdating) {
  // Using 4slow_ok not to complete transactions at once.
  UseIpv4DelayedDnsRules("4slow_ok");

  Requester requester = CreateRequester("https://4slow_ok");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  requester.SetOnUpdatedCallback(
      base::BindLambdaForTesting([&]() { DestroyResolver(); }));

  RunUntilIdle();
  EXPECT_THAT(requester.finished_result(),
              Optional(IsError(ERR_NAME_NOT_RESOLVED)));
  EXPECT_THAT(requester.request()->GetResolveErrorInfo().error,
              IsError(ERR_DNS_REQUEST_CANCELLED));
}

TEST_F(HostResolverServiceEndpointRequestTest, DestroyResolverWhileFinishing) {
  UseNonDelayedDnsRules("ok");

  Requester requester = CreateRequester("https://ok");
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  requester.SetOnFinishedCallback(
      base::BindLambdaForTesting([&]() { DestroyResolver(); }));

  RunUntilIdle();
  EXPECT_THAT(*requester.finished_result(), IsOk());
}

TEST_F(HostResolverServiceEndpointRequestTest,
       EndpointsCryptoReadySystemTaskOnly) {
  proc_->AddRuleForAllFamilies("a.test", "192.0.2.1");
  ResolveHostParameters parameters;
  parameters.source = HostResolverSource::SYSTEM;
  Requester requester =
      CreateRequester("https://a.test", std::move(parameters));
  int rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Should not crash when calling EndpointsCryptoReady().
  ASSERT_FALSE(requester.request()->EndpointsCryptoReady());

  proc_->SignalMultiple(1u);
  requester.WaitForFinished();
  EXPECT_THAT(requester.finished_result(), Optional(IsOk()));
  ASSERT_TRUE(requester.request()->EndpointsCryptoReady());
}

TEST_F(HostResolverServiceEndpointRequestTest, MultipleRequestsOk) {
  UseIpv4DelayedDnsRules("4slow_ok");

  constexpr std::string_view kHost = "https://4slow_ok";
  Requester requester1 = CreateRequester(kHost);
  EXPECT_THAT(requester1.Start(), IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  Requester requester2 = CreateRequester(kHost);
  EXPECT_THAT(requester2.Start(), IsError(ERR_IO_PENDING));
  // The second request should share the same job with the first request.
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  RunUntilIdle();
  EXPECT_EQ(1u, resolver_->num_running_dispatcher_jobs_for_tests());

  // Complete the delayed transaction, which finishes requests synchronously.
  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(*requester1.finished_result(), IsOk());
  EXPECT_THAT(requester1.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));

  EXPECT_THAT(*requester2.finished_result(), IsOk());
  EXPECT_THAT(requester2.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));
}

TEST_F(HostResolverServiceEndpointRequestTest,
       MultipleRequestsAddRequestInTheMiddleOfResolution) {
  UseIpv4DelayedDnsRules("4slow_ok");

  constexpr std::string_view kHost = "https://4slow_ok";
  Requester requester1 = CreateRequester(kHost);
  EXPECT_THAT(requester1.Start(), IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  // Partially complete transactions. Only IPv6 endpoints should be available.
  RunUntilIdle();
  EXPECT_EQ(1u, resolver_->num_running_dispatcher_jobs_for_tests());
  ASSERT_FALSE(requester1.finished_result().has_value());
  EXPECT_THAT(requester1.request()->GetEndpointResults(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("::1", 443)))));

  // Add a new request in the middle of resolution. The request should be
  // attached to the ongoing job.
  Requester requester2 = CreateRequester(kHost);
  requester2.CancelRequestOnFinished();
  EXPECT_THAT(requester2.Start(), IsError(ERR_IO_PENDING));
  EXPECT_EQ(1u, resolver_->num_running_dispatcher_jobs_for_tests());

  // The second request should have the same intermediate results as the first
  // request.
  ASSERT_FALSE(requester2.finished_result().has_value());
  EXPECT_THAT(requester2.request()->GetEndpointResults(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("::1", 443)))));

  // Complete all transactions. Both requests should finish and have the same
  // results.
  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_EQ(0u, resolver_->num_running_dispatcher_jobs_for_tests());

  EXPECT_THAT(*requester1.finished_result(), IsOk());
  EXPECT_THAT(requester1.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));

  EXPECT_THAT(*requester2.finished_result(), IsOk());
  EXPECT_THAT(requester2.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));
}

TEST_F(HostResolverServiceEndpointRequestTest,
       MultipleRequestsAddAndCancelRequestInUpdatedCallback) {
  UseIpv4DelayedDnsRules("4slow_ok");

  constexpr std::string_view kHost = "https://4slow_ok";
  Requester requester1 = CreateRequester(kHost);
  Requester requester2 = CreateRequester(kHost);

  requester1.SetOnUpdatedCallback(base::BindLambdaForTesting([&] {
    EXPECT_THAT(requester2.Start(), IsError(ERR_IO_PENDING));
    requester1.CancelRequest();
  }));

  EXPECT_THAT(requester1.Start(), IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  // Partially complete transactions. The update callback of the first request
  // should start the second request and then cancel the first request.
  RunUntilIdle();
  EXPECT_EQ(1u, resolver_->num_running_dispatcher_jobs_for_tests());

  ASSERT_FALSE(requester1.finished_result().has_value());
  ASSERT_FALSE(requester1.request());

  ASSERT_FALSE(requester2.finished_result().has_value());
  EXPECT_THAT(requester2.request()->GetEndpointResults(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("::1", 443)))));

  // Complete all transactions. The second request should finish successfully.
  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_EQ(0u, resolver_->num_running_dispatcher_jobs_for_tests());

  ASSERT_FALSE(requester1.finished_result().has_value());
  ASSERT_FALSE(requester1.request());

  EXPECT_THAT(*requester2.finished_result(), IsOk());
  EXPECT_THAT(requester2.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));
}

TEST_F(HostResolverServiceEndpointRequestTest,
       MultipleRequestsAddRequestInFinishedCallback) {
  UseNonDelayedDnsRules("ok");

  constexpr std::string_view kHost = "https://ok";
  Requester requester1 = CreateRequester(kHost);
  Requester requester2 = CreateRequester(kHost);

  requester1.SetOnFinishedCallback(base::BindLambdaForTesting([&] {
    // The second request should finish synchronously because it should
    // share the same job as the first one and the job has finished already.
    EXPECT_THAT(requester2.Start(), IsOk());
  }));

  EXPECT_THAT(requester1.Start(), IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  RunUntilIdle();
  EXPECT_EQ(0u, resolver_->num_running_dispatcher_jobs_for_tests());

  EXPECT_THAT(*requester1.finished_result(), IsOk());
  EXPECT_THAT(requester1.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));

  EXPECT_THAT(*requester2.finished_result(), IsOk());
  EXPECT_THAT(requester2.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));
}

TEST_F(HostResolverServiceEndpointRequestTest,
       MultipleRequestsCancelOneRequestOnUpdated) {
  UseIpv4DelayedDnsRules("4slow_ok");

  constexpr std::string_view kHost = "https://4slow_ok";
  Requester requester1 = CreateRequester(kHost);
  EXPECT_THAT(requester1.Start(), IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  Requester requester2 = CreateRequester(kHost);
  // The second request destroys self when notified an update.
  requester2.CancelRequestOnUpdated();
  EXPECT_THAT(requester2.Start(), IsError(ERR_IO_PENDING));
  // The second request should share the same job with the first request.
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  RunUntilIdle();
  EXPECT_EQ(1u, resolver_->num_running_dispatcher_jobs_for_tests());

  // Complete the delayed transaction, which finishes the first request
  // synchronously.
  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(*requester1.finished_result(), IsOk());
  EXPECT_THAT(requester1.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));
  // The second request was destroyed so it didn't get notified.
  ASSERT_FALSE(requester2.finished_result().has_value());
  ASSERT_FALSE(requester2.request());
}

TEST_F(HostResolverServiceEndpointRequestTest,
       MultipleRequestsCancelAllRequestOnUpdated) {
  UseIpv4DelayedDnsRules("4slow_ok");

  constexpr std::string_view kHost = "https://4slow_ok";
  Requester requester1 = CreateRequester(kHost);
  requester1.CancelRequestOnUpdated();
  EXPECT_THAT(requester1.Start(), IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  Requester requester2 = CreateRequester(kHost);
  requester2.CancelRequestOnUpdated();
  EXPECT_THAT(requester2.Start(), IsError(ERR_IO_PENDING));
  // The second request should share the same job with the first request.
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  // Complete non-delayed transactions and invoke update callbacks, which
  // destroy all requests.
  RunUntilIdle();
  EXPECT_EQ(0u, resolver_->num_running_dispatcher_jobs_for_tests());

  ASSERT_FALSE(requester1.finished_result().has_value());
  ASSERT_FALSE(requester1.request());
  ASSERT_FALSE(requester2.finished_result().has_value());
  ASSERT_FALSE(requester2.request());
}

TEST_F(HostResolverServiceEndpointRequestTest,
       MultipleRequestsCancelAllRequestOnFinished) {
  UseNonDelayedDnsRules("ok");

  constexpr std::string_view kHost = "https://ok";
  Requester requester1 = CreateRequester(kHost);
  requester1.CancelRequestOnFinished();
  EXPECT_THAT(requester1.Start(), IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  Requester requester2 = CreateRequester(kHost);
  requester2.CancelRequestOnFinished();
  EXPECT_THAT(requester2.Start(), IsError(ERR_IO_PENDING));
  // The second request should share the same job with the first request.
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  RunUntilIdle();
  EXPECT_EQ(0u, resolver_->num_running_dispatcher_jobs_for_tests());
  EXPECT_THAT(*requester1.finished_result(), IsOk());
  EXPECT_THAT(*requester2.finished_result(), IsOk());
}

TEST_F(HostResolverServiceEndpointRequestTest, WithLegacyRequestOk) {
  UseIpv4DelayedDnsRules("4slow_ok");

  constexpr std::string_view kHost = "https://4slow_ok";
  LegacyRequester legacy_requester = CreateLegacyRequester(kHost);
  int rv = legacy_requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  Requester requester = CreateRequester(kHost);
  rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // The request should share the same job with the legacy request.
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  // Partially complete transactions. Requests should not complete but
  // the non-legacy request should provide intermediate endpoints.
  RunUntilIdle();
  EXPECT_EQ(1u, resolver_->num_running_dispatcher_jobs_for_tests());
  ASSERT_FALSE(legacy_requester.complete_result().has_value());
  ASSERT_FALSE(requester.finished_result().has_value());
  ASSERT_TRUE(requester.request()->EndpointsCryptoReady());
  EXPECT_THAT(requester.request()->GetEndpointResults(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("::1", 443)))));

  // Complete delayed transactions, which finish requests synchronously.
  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(*legacy_requester.complete_result(), IsOk());
  EXPECT_THAT(*requester.finished_result(), IsOk());
  EXPECT_THAT(requester.finished_endpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("127.0.0.1", 443)),
                  ElementsAre(MakeIPEndPoint("::1", 443)))));
}

TEST_F(HostResolverServiceEndpointRequestTest,
       WithLegacyRequestDestroyResolverOnUpdated) {
  UseIpv4DelayedDnsRules("4slow_ok");

  constexpr std::string_view kHost = "https://4slow_ok";
  LegacyRequester legacy_requester = CreateLegacyRequester(kHost);
  int rv = legacy_requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  Requester requester = CreateRequester(kHost);
  rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // The request should share the same job with the legacy request.
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  requester.SetOnUpdatedCallback(
      base::BindLambdaForTesting([&]() { DestroyResolver(); }));

  RunUntilIdle();
  // DestroyResolver() removed the corresponding job and the legacy reqquest
  // didn't get notified, but the non-legacy request got notified via the
  // update callback.
  ASSERT_FALSE(legacy_requester.complete_result().has_value());
  EXPECT_THAT(requester.finished_result(),
              Optional(IsError(ERR_NAME_NOT_RESOLVED)));
  EXPECT_THAT(requester.request()->GetResolveErrorInfo().error,
              IsError(ERR_DNS_REQUEST_CANCELLED));
}

TEST_F(HostResolverServiceEndpointRequestTest,
       WithLegacyRequestCancelRequestOnUpdated) {
  UseIpv4DelayedDnsRules("4slow_ok");

  constexpr std::string_view kHost = "https://4slow_ok";
  LegacyRequester legacy_requester = CreateLegacyRequester(kHost);
  int rv = legacy_requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  Requester requester = CreateRequester(kHost);
  requester.CancelRequestOnUpdated();
  rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // The request should share the same job with the legacy request.
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  // Partially complete transactions to trigger the update callback on
  // non-legacy request, which cancels the request itself.
  RunUntilIdle();
  ASSERT_FALSE(legacy_requester.complete_result().has_value());
  ASSERT_FALSE(requester.request());

  // Complete delayed transactions, which finish the legacy request
  // synchronously. Non-legacy request was already destroyed.
  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(*legacy_requester.complete_result(), IsOk());
}

TEST_F(HostResolverServiceEndpointRequestTest,
       WithLegacyRequestCancelLegacyRequest) {
  UseNonDelayedDnsRules("ok");

  constexpr std::string_view kHost = "https://ok";

  LegacyRequester legacy_requester = CreateLegacyRequester(kHost);
  int rv = legacy_requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  Requester requester = CreateRequester(kHost);
  requester.CancelRequestOnUpdated();
  rv = requester.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // The request should share the same job with the legacy request.
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  // Cancelling legacy request should not cancel non-legacy request.
  legacy_requester.CancelRequest();
  ASSERT_FALSE(requester.finished_result().has_value());
  EXPECT_EQ(3u, resolver_->num_running_dispatcher_jobs_for_tests());

  requester.WaitForFinished();
  EXPECT_EQ(0u, resolver_->num_running_dispatcher_jobs_for_tests());
  EXPECT_THAT(*requester.finished_result(), IsOk());
}

TEST_F(HostResolverServiceEndpointRequestTest, ChangePriority) {
  proc_->AddRuleForAllFamilies("req1", "192.0.2.1");
  proc_->AddRuleForAllFamilies("req2", "192.0.2.2");
  proc_->AddRuleForAllFamilies("req3", "192.0.2.3");

  CreateSerialResolver(/*check_ipv6_on_wifi=*/true);

  // Start three requests with the same initial priority, then change the
  // priority of the third request to HIGHEST. The first request starts
  // immediately so it should finish first. The third request should finish
  // second because its priority is changed to HIGHEST. The second request
  // should finish last.

  ResolveHostParameters params;
  params.initial_priority = RequestPriority::LOW;

  size_t request_finish_order = 0;

  Requester requester1 = CreateRequester("https://req1", params);
  requester1.SetOnFinishedCallback(base::BindLambdaForTesting([&] {
    ++request_finish_order;
    ASSERT_EQ(request_finish_order, 1u);
  }));
  int rv = requester1.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  Requester requester2 = CreateRequester("https://req2", params);
  requester2.SetOnFinishedCallback(base::BindLambdaForTesting([&] {
    ++request_finish_order;
    ASSERT_EQ(request_finish_order, 3u);
  }));
  rv = requester2.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  Requester requester3 = CreateRequester("https://req3", params);
  requester3.SetOnFinishedCallback(base::BindLambdaForTesting([&] {
    ++request_finish_order;
    ASSERT_EQ(request_finish_order, 2u);
  }));
  rv = requester3.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  requester3.request()->ChangeRequestPriority(RequestPriority::HIGHEST);

  proc_->SignalMultiple(3u);

  requester1.WaitForFinished();
  requester3.WaitForFinished();
  requester2.WaitForFinished();
}

TEST_F(HostResolverServiceEndpointRequestTest, ChangePriorityBeforeStart) {
  proc_->AddRuleForAllFamilies("req1", "192.0.2.1");
  proc_->AddRuleForAllFamilies("req2", "192.0.2.2");
  proc_->AddRuleForAllFamilies("req3", "192.0.2.3");

  CreateSerialResolver(/*check_ipv6_on_wifi=*/true);

  // Create three requests with the same initial priority, then change the
  // priority of the third request to HIGHEST before starting the requests. The
  // first request starts immediately so it should finish first. The third
  // request should finish second because its priority is changed to HIGHEST.
  // The second request should finish last.

  ResolveHostParameters params;
  params.initial_priority = RequestPriority::LOW;

  size_t request_finish_order = 0;

  Requester requester1 = CreateRequester("https://req1", params);
  requester1.SetOnFinishedCallback(base::BindLambdaForTesting([&] {
    ++request_finish_order;
    ASSERT_EQ(request_finish_order, 1u);
  }));

  Requester requester2 = CreateRequester("https://req2", params);
  requester2.SetOnFinishedCallback(base::BindLambdaForTesting([&] {
    ++request_finish_order;
    ASSERT_EQ(request_finish_order, 3u);
  }));

  Requester requester3 = CreateRequester("https://req3", params);
  requester3.SetOnFinishedCallback(base::BindLambdaForTesting([&] {
    ++request_finish_order;
    ASSERT_EQ(request_finish_order, 2u);
  }));

  requester3.request()->ChangeRequestPriority(RequestPriority::HIGHEST);

  int rv = requester1.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = requester2.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = requester3.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  proc_->SignalMultiple(3u);

  requester1.WaitForFinished();
  requester3.WaitForFinished();
  requester2.WaitForFinished();
}

}  // namespace net
