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
}  // namespace

class CanaryDomainServiceDisabledTest : public testing::Test,
                                        public WithTaskEnvironment {
 protected:
  CanaryDomainServiceDisabledTest()
      : url_request_context_(CreateTestURLRequestContextBuilder()->Build()),
        resolve_context_(url_request_context_.get(), /*enable_caching=*/false) {
    host_resolver_.SetResolveContextForTesting(&resolve_context_);
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

  std::unique_ptr<URLRequestContext> url_request_context_;
  ResolveContext resolve_context_;
  MockHostResolver host_resolver_;
  scoped_refptr<DnsSession> session_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CanaryDomainServiceDisabledTest, NoProbeWhenFeatureDisabled) {
  feature_list_.InitAndDisableFeature(features::kProbeSecureDnsCanaryDomain);

  std::unique_ptr<CanaryDomainService> canary_domain_service =
      host_resolver_.CreateCanaryDomainService();
  ASSERT_TRUE(canary_domain_service);
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kInactive);

  canary_domain_service->Start();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kInactive);
  EXPECT_EQ(host_resolver_.num_resolve(), 0u);
}

TEST_F(CanaryDomainServiceDisabledTest, NoProbeWhenHostEmpty) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kProbeSecureDnsCanaryDomain,
      {{features::kSecureDnsCanaryDomainHost.name, ""}});

  std::unique_ptr<CanaryDomainService> canary_domain_service =
      host_resolver_.CreateCanaryDomainService();
  ASSERT_TRUE(canary_domain_service);
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kInactive);

  canary_domain_service->Start();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kInactive);
  EXPECT_EQ(host_resolver_.num_resolve(), 0u);
}

TEST_F(CanaryDomainServiceDisabledTest, NoProbeWhenConfigDisallows) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kProbeSecureDnsCanaryDomain,
      {{features::kSecureDnsCanaryDomainHost.name, "test.test"}});

  // Set up a session with mode SECURE (not AUTOMATIC).
  DnsConfig config;
  config.secure_dns_mode = SecureDnsMode::kSecure;
  session_ = base::MakeRefCounted<DnsSession>(
      config, base::BindRepeating([](int min, int max) -> int { return 0; }),
      /*net_log=*/nullptr);
  resolve_context_.InvalidateCachesAndPerSessionData(session_.get(), false);

  std::unique_ptr<CanaryDomainService> canary_domain_service =
      host_resolver_.CreateCanaryDomainService();
  ASSERT_TRUE(canary_domain_service);

  canary_domain_service->Start();
  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNotStarted);
  EXPECT_EQ(host_resolver_.num_resolve(), 0u);

  base::test::TestFuture<void> probe_complete_future;
  canary_domain_service->SetOnProbeCompleteCallbackForTesting(
      probe_complete_future.GetCallback());
  canary_domain_service->OnDohServerUnavailable(/*network_change=*/false);
  EXPECT_TRUE(probe_complete_future.Wait());

  EXPECT_EQ(resolve_context_.doh_fallback_canary_domain_check_status(),
            CanaryDomainCheckStatus::kNotStarted);
  EXPECT_EQ(host_resolver_.num_resolve(), 0u);
}

}  // namespace net
