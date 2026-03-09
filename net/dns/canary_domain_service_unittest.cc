// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/canary_domain_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/dns_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log_with_source.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {
const char kHost[] = "canary-domain.com";
}  // namespace

class CanaryDomainServiceTest : public ::testing::TestWithParam<bool>,
                                public WithTaskEnvironment {
 protected:
  CanaryDomainServiceTest()
      : url_request_context_(CreateTestURLRequestContextBuilder()->Build()),
        resolve_context_(url_request_context_.get(), /*enable_caching=*/false) {
  }

  bool synchronous_mode() const { return GetParam(); }

  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kProbeSecureDnsCanaryDomain,
        {{features::kSecureDnsCanaryDomainHost.name, kHost}});

    // Set up a session with AUTOMATIC mode and fallback upgrade enabled.
    DnsConfig config;
    config.secure_dns_mode = SecureDnsMode::kAutomatic;
    config.should_perform_doh_fallback_upgrade = true;
    session_ = base::MakeRefCounted<DnsSession>(
        config, base::BindRepeating([](int, int) -> int { return 0; }),
        /*net_log=*/nullptr);
    resolve_context_.InvalidateCachesAndPerSessionData(session_.get(), false);

    // Must be created after enabling the feature.
    canary_domain_service_ = std::make_unique<CanaryDomainService>(
        resolve_context_.AsSafeRef(), host_resolver_.AsSafeRef());

    // Parameterizes whether the probe request will complete synchronously or
    // asynchronously.
    host_resolver_.set_synchronous_mode(synchronous_mode());
  }

  void StartAndWaitForProbeComplete() {
    canary_domain_service_->SetOnProbeCompleteCallbackForTesting(
        probe_complete_future_.GetCallback());
    canary_domain_service_->Start();
    EXPECT_TRUE(probe_complete_future_.Wait());
    probe_complete_future_.Clear();
  }

  void OnDohServerUnavailableAndWaitForProbeComplete() {
    canary_domain_service_->SetOnProbeCompleteCallbackForTesting(
        probe_complete_future_.GetCallback());
    canary_domain_service_->OnDohServerUnavailable(/*network_change=*/false);
    EXPECT_TRUE(probe_complete_future_.Wait());
    probe_complete_future_.Clear();
  }

  void ExpectLoggedResult(CanaryDomainResult result) {
    histogram_tester_.ExpectUniqueSample(
        "Net.DNS.CanaryDomainService.SecureDns.Result", result, 1);
  }

  std::unique_ptr<URLRequestContext> url_request_context_;
  ResolveContext resolve_context_;
  MockHostResolver host_resolver_;
  base::test::TestFuture<void> probe_complete_future_;
  scoped_refptr<DnsSession> session_;
  std::unique_ptr<CanaryDomainService> canary_domain_service_;
  net::NetLogWithSource net_log_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    CanaryDomainServiceTest,
    ::testing::Bool(),
    [](const ::testing::TestParamInfo<CanaryDomainServiceTest::ParamType>&
           info) {
      return info.param ? "synchronous_mode_true" : "synchronous_mode_false";
    });

TEST_P(CanaryDomainServiceTest, ProbeSecureDnsDomain_PositiveResult) {
  host_resolver_.rules()->AddRule(kHost, "1.2.3.4");
  StartAndWaitForProbeComplete();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kPositive);
  ExpectLoggedResult(CanaryDomainResult::kPositive);
}

TEST_P(CanaryDomainServiceTest,
       ProbeSecureDnsDomain_NegativeResultNoErrorWithNoRecords) {
  host_resolver_.rules()->AddRule(kHost, "");
  StartAndWaitForProbeComplete();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNegative);
  ExpectLoggedResult(CanaryDomainResult::kNegativeNoErrorWithoutRecords);
}

TEST_P(CanaryDomainServiceTest, ProbeSecureDnsDomain_NegativeResultNxDomain) {
  host_resolver_.rules()->AddRule(kHost, net::ERR_NAME_NOT_RESOLVED);
  StartAndWaitForProbeComplete();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNegative);
  ExpectLoggedResult(CanaryDomainResult::kNegativeNxDomainOrOtherError);
}

TEST_P(CanaryDomainServiceTest, ProbeSecureDnsDomain_NegativeResultServfail) {
  host_resolver_.rules()->AddRule(kHost, net::ERR_DNS_SERVER_FAILURE);
  StartAndWaitForProbeComplete();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNegative);
  // MockHostResolver squashes ERR_ABORTED to ERR_NAME_NOT_RESOLVED.
  ExpectLoggedResult(CanaryDomainResult::kNegativeNxDomainOrOtherError);
}

TEST_P(CanaryDomainServiceTest, ProbeSecureDnsDomain_NegativeResultOtherError) {
  host_resolver_.rules()->AddRule(kHost, net::ERR_DNS_NAME_HTTPS_ONLY);
  StartAndWaitForProbeComplete();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNegative);
  ExpectLoggedResult(CanaryDomainResult::kNegativeOtherError);
}

TEST_P(CanaryDomainServiceTest, OnSessionChangedResetsStatus) {
  host_resolver_.rules()->AddRule(kHost, "1.2.3.4");
  StartAndWaitForProbeComplete();

  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kPositive);

  canary_domain_service_->OnSessionChanged();

  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNotStarted);
}

TEST_P(CanaryDomainServiceTest, OnDohServerUnavailableTriggersProbe) {
  host_resolver_.rules()->AddRule(kHost, "1.2.3.4");
  StartAndWaitForProbeComplete();
  canary_domain_service_->OnSessionChanged();

  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNotStarted);

  OnDohServerUnavailableAndWaitForProbeComplete();

  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kPositive);
}

TEST_P(CanaryDomainServiceTest, CanaryDomainResolutionIsNotCached) {
  host_resolver_.rules()->AddRule(kHost, "1.2.3.4");
  StartAndWaitForProbeComplete();

  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kPositive);

  host_resolver_.rules()->ClearRules();
  host_resolver_.rules()->AddRule(kHost, net::ERR_NAME_NOT_RESOLVED);

  canary_domain_service_->OnSessionChanged();
  OnDohServerUnavailableAndWaitForProbeComplete();

  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNegative);
}

class CanaryDomainServiceDisabledTest : public testing::Test,
                                        public WithTaskEnvironment {
 protected:
  CanaryDomainServiceDisabledTest()
      : url_request_context_(CreateTestURLRequestContextBuilder()->Build()),
        resolve_context_(url_request_context_.get(), /*enable_caching=*/false) {
  }

  void SetUp() override {
    // Set up a session with AUTOMATIC mode and fallback upgrade enabled.
    DnsConfig config;
    config.secure_dns_mode = SecureDnsMode::kAutomatic;
    config.should_perform_doh_fallback_upgrade = true;
    session_ = base::MakeRefCounted<DnsSession>(
        config, base::BindRepeating([](int min, int max) -> int { return 0; }),
        /*net_log=*/nullptr);
    resolve_context_.InvalidateCachesAndPerSessionData(session_.get(), false);
  }

  void OnDohServerUnavailableAndWaitForProbeComplete() {
    canary_domain_service_->SetOnProbeCompleteCallbackForTesting(
        probe_complete_future_.GetCallback());
    canary_domain_service_->OnDohServerUnavailable(/*network_change=*/false);
    EXPECT_TRUE(probe_complete_future_.Wait());
    probe_complete_future_.Clear();
  }

  std::unique_ptr<URLRequestContext> url_request_context_;
  ResolveContext resolve_context_;
  MockHostResolver host_resolver_;
  base::test::TestFuture<void> probe_complete_future_;
  scoped_refptr<DnsSession> session_;
  std::unique_ptr<CanaryDomainService> canary_domain_service_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CanaryDomainServiceDisabledTest, NoProbeWhenFeatureDisabled) {
  feature_list_.InitAndDisableFeature(features::kProbeSecureDnsCanaryDomain);

  canary_domain_service_ = std::make_unique<CanaryDomainService>(
      resolve_context_.AsSafeRef(), host_resolver_.AsSafeRef());

  canary_domain_service_->Start();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNotStarted);
  EXPECT_EQ(host_resolver_.num_resolve(), 0u);

  OnDohServerUnavailableAndWaitForProbeComplete();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNotStarted);
  EXPECT_EQ(host_resolver_.num_resolve(), 0u);
}

TEST_F(CanaryDomainServiceDisabledTest, NoProbeWhenHostEmpty) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kProbeSecureDnsCanaryDomain,
      {{features::kSecureDnsCanaryDomainHost.name, ""}});

  canary_domain_service_ = std::make_unique<CanaryDomainService>(
      resolve_context_.AsSafeRef(), host_resolver_.AsSafeRef());

  canary_domain_service_->Start();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNotStarted);
  EXPECT_EQ(host_resolver_.num_resolve(), 0u);

  OnDohServerUnavailableAndWaitForProbeComplete();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNotStarted);
  EXPECT_EQ(host_resolver_.num_resolve(), 0u);
}

TEST_F(CanaryDomainServiceDisabledTest, NoProbeWhenConfigDisallows) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kProbeSecureDnsCanaryDomain,
      {{features::kSecureDnsCanaryDomainHost.name, "test.test"}});

  // Set up a session with mode SECURE (not AUTOMATIC).
  DnsConfig config;
  config.secure_dns_mode = SecureDnsMode::kSecure;
  config.should_perform_doh_fallback_upgrade = true;
  session_ = base::MakeRefCounted<DnsSession>(
      config, base::BindRepeating([](int min, int max) -> int { return 0; }),
      /*net_log=*/nullptr);
  resolve_context_.InvalidateCachesAndPerSessionData(session_.get(), false);

  canary_domain_service_ = std::make_unique<CanaryDomainService>(
      resolve_context_.AsSafeRef(), host_resolver_.AsSafeRef());

  canary_domain_service_->Start();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNotStarted);
  EXPECT_EQ(host_resolver_.num_resolve(), 0u);

  OnDohServerUnavailableAndWaitForProbeComplete();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNotStarted);
  EXPECT_EQ(host_resolver_.num_resolve(), 0u);
}

}  // namespace net
