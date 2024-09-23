// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "net/dns/host_resolver_manager_unittest.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net {

class HostResolverManagerIPv6ReachabilityOverrideTest
    : public HostResolverManagerDnsTest,
      public testing::WithParamInterface<bool> {
 public:
  static constexpr const char kTargetHost[] = "host.test";

  HostResolverManagerIPv6ReachabilityOverrideTest() {
    std::map<std::string, std::string> field_trial_params;
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          features::kEnableIPv6ReachabilityOverride);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kEnableIPv6ReachabilityOverride);
    }
  }

 protected:
  void SetUp() override {
    HostResolverManagerDnsTest::SetUp();
    // Make the global reachiability probe failed.
    CreateResolverWithLimitsAndParams(
        /*max_concurrent_resolves=*/10,
        HostResolverSystemTask::Params(proc_, /*max_retry_attempts=*/4),
        /*ipv6_reachable=*/false,
        /*check_ipv6_on_wifi=*/true);
    ChangeDnsConfig(CreateValidDnsConfig());
    // Wait until ongoing probe tasks finish.
    RunUntilIdle();

    // This rule is used when only A record is queried.
    proc_->AddRule(kTargetHost, ADDRESS_FAMILY_IPV4, "192.0.2.1",
                   HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6);
    // This rule is used when A and AAAA records are queried.
    proc_->AddRule(kTargetHost, ADDRESS_FAMILY_UNSPECIFIED,
                   "192.0.2.1,2001:db8::1");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HostResolverManagerIPv6ReachabilityOverrideTest,
                         testing::Bool());

TEST_P(HostResolverManagerIPv6ReachabilityOverrideTest, Request) {
  proc_->SignalMultiple(1u);
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver_->CreateRequest(
          url::SchemeHostPort(url::kHttpScheme, kTargetHost, 80),
          NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
          resolve_context_.get());

  int result = ERR_IO_PENDING;
  base::RunLoop run_loop;
  request->Start(base::BindLambdaForTesting([&](int rv) {
    result = rv;
    run_loop.Quit();
  }));
  run_loop.Run();
  EXPECT_THAT(result, IsOk());

  if (GetParam()) {
    EXPECT_THAT(
        request->GetAddressResults()->endpoints(),
        testing::UnorderedElementsAre(CreateExpected("192.0.2.1", 80),
                                      CreateExpected("2001:db8::1", 80)));
  } else {
    EXPECT_THAT(request->GetAddressResults()->endpoints(),
                testing::UnorderedElementsAre(CreateExpected("192.0.2.1", 80)));
  }
}

}  // namespace net
