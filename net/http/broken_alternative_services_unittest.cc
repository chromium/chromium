// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/broken_alternative_services.h"

#include <algorithm>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

// Initial delay for broken alternative services.
const uint64_t kBrokenAlternativeProtocolDelaySecs = 300;

class BrokenAlternativeServicesTest
    : public BrokenAlternativeServices::Delegate,
      public ::testing::Test {
 public:
  BrokenAlternativeServicesTest()
      : test_task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()),
        test_task_runner_context_(test_task_runner_),
        broken_services_clock_(test_task_runner_->GetMockTickClock()),
        broken_services_(50, this, broken_services_clock_) {
    SchemefulSite site1(GURL("http://foo.test"));
    SchemefulSite site2(GURL("http://bar.test"));
    network_anonymization_key1_ =
        NetworkAnonymizationKey::CreateSameSite(site1);
    network_anonymization_key2_ =
        NetworkAnonymizationKey::CreateSameSite(site2);
  }

  // BrokenAlternativeServices::Delegate implementation
  void OnExpireBrokenAlternativeService(
      const AlternativeService& expired_alternative_service,
      const NetworkAnonymizationKey& network_anonymization_key) override {
    expired_alt_svcs_.emplace_back(expired_alternative_service,
                                   network_anonymization_key,
                                   true /* use_network_anonymization_key */);
  }

  void TestExponentialBackoff(base::TimeDelta initial_delay,
                              bool exponential_backoff_on_initial_delay);

  // All tests will run inside the scope of |test_task_runner_context_|, which
  // means any task posted to the main message loop will run on
  // |test_task_runner_|.
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  base::TestMockTimeTaskRunner::ScopedContext test_task_runner_context_;

  raw_ptr<const base::TickClock> broken_services_clock_;
  BrokenAlternativeServices broken_services_;

  std::vector<BrokenAlternativeService> expired_alt_svcs_;

  NetworkAnonymizationKey network_anonymization_key1_;
  NetworkAnonymizationKey network_anonymization_key2_;
};

TEST_F(BrokenAlternativeServicesTest, MarkBroken) {
  const BrokenAlternativeService alternative_service1(
      AlternativeService(kProtoHTTP2, "foo", 443), network_anonymization_key1_,
      true /* use_network_anonymization_key */);
  const BrokenAlternativeService alternative_service2(
      AlternativeService(kProtoHTTP2, "foo", 1234), network_anonymization_key1_,
      true /* use_network_anonymization_key */);
  const BrokenAlternativeService alternative_service3(
      AlternativeService(kProtoHTTP2, "foo", 443), network_anonymization_key2_,
      true /* use_network_anonymization_key */);

  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));

  broken_services_.MarkBroken(alternative_service1);

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));

  broken_services_.MarkBroken(alternative_service2);

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));

  broken_services_.MarkBroken(alternative_service3);

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service3));

  broken_services_.Confirm(alternative_service1);

  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service3));

  broken_services_.Confirm(alternative_service2);

  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service3));

  broken_services_.Confirm(alternative_service3);

  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));

  EXPECT_EQ(0u, expired_alt_svcs_.size());
}

TEST_F(BrokenAlternativeServicesTest, MarkBrokenUntilDefaultNetworkChanges) {
  const BrokenAlternativeService alternative_service1(
      AlternativeService(kProtoHTTP2, "foo", 443), network_anonymization_key1_,
      true /* use_network_anonymization_key */);
  const BrokenAlternativeService alternative_service2(
      AlternativeService(kProtoHTTP2, "foo", 1234), network_anonymization_key1_,
      true /* use_network_anonymization_key */);
  const BrokenAlternativeService alternative_service3(
      AlternativeService(kProtoHTTP2, "foo", 443), network_anonymization_key2_,
      true /* use_network_anonymization_key */);
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service3));

  broken_services_.MarkBrokenUntilDefaultNetworkChanges(alternative_service1);
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service3));

  broken_services_.MarkBrokenUntilDefaultNetworkChanges(alternative_service2);
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service3));

  broken_services_.MarkBrokenUntilDefaultNetworkChanges(alternative_service3);
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service3));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service3));

  broken_services_.Confirm(alternative_service1);
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service3));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service3));

  broken_services_.Confirm(alternative_service2);
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service3));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service3));

  broken_services_.Confirm(alternative_service3);
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service3));

  EXPECT_EQ(0u, expired_alt_svcs_.size());
}

TEST_F(BrokenAlternativeServicesTest, MarkRecentlyBroken) {
  const BrokenAlternativeService alternative_service1(
      AlternativeService(kProtoHTTP2, "foo", 443), network_anonymization_key1_,
      true /* use_network_anonymization_key */);
  const BrokenAlternativeService alternative_service2(
      AlternativeService(kProtoHTTP2, "foo", 443), network_anonymization_key2_,
      true /* use_network_anonymization_key */);

  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));

  broken_services_.MarkRecentlyBroken(alternative_service1);
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));

  broken_services_.MarkRecentlyBroken(alternative_service2);
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service2));

  broken_services_.Confirm(alternative_service1);
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service2));

  broken_services_.Confirm(alternative_service2);
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));
}

TEST_F(BrokenAlternativeServicesTest, OnDefaultNetworkChanged) {
  BrokenAlternativeService alternative_service1(
      AlternativeService(kProtoQUIC, "foo", 443), network_anonymization_key1_,
      true /* use_network_anonymization_key */);
  BrokenAlternativeService alternative_service2(
      AlternativeService(kProtoQUIC, "bar", 443), network_anonymization_key1_,
      true /* use_network_anonymization_key */);
  BrokenAlternativeService alternative_service3(
      AlternativeService(kProtoQUIC, "foo", 443), network_anonymization_key2_,
      true /* use_network_anonymization_key */);

  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service3));

  // Mark |alternative_service1| as broken until default network changes.
  broken_services_.MarkBrokenUntilDefaultNetworkChanges(alternative_service1);
  // |alternative_service1| should be considered as currently broken and
  // recently broken.
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service3));
  // |broken_services_| should have posted task to expire the brokenness of
  // |alternative_service1|.
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Advance time until one second before |alternative_service1|'s brokenness
  // expires.
  test_task_runner_->FastForwardBy(base::Minutes(5) - base::Seconds(1));
  // |alternative_service1| should still be considered as currently broken and
  // recently broken.
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));

  // Advance another second and |alternative_service1|'s brokenness expires.
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));

  // Mark |alternative_service2| as broken until default network changes.
  broken_services_.MarkBrokenUntilDefaultNetworkChanges(alternative_service2);
  // |alternative_service2| should be considered as currently broken and
  // recently broken.
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service3));

  // Mark |alternative_service3| as broken.
  broken_services_.MarkBroken(alternative_service3);
  // |alternative_service2| should be considered as currently broken and
  // recently broken.
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service3));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service3));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));

  // Deliver the message that a default network has changed.
  broken_services_.OnDefaultNetworkChanged();
  // Recently broken until default network change alternative service is moved
  // to working state.
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service1));
  // Currently broken until default network change alternative service is moved
  // to working state.
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));
  // Broken alternative service is not affected by the default network change.
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service3));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service3));
}

TEST_F(BrokenAlternativeServicesTest,
       ExpireBrokenAlternativeServiceOnDefaultNetwork) {
  BrokenAlternativeService alternative_service(
      AlternativeService(kProtoQUIC, "foo", 443), network_anonymization_key1_,
      true /* use_network_anonymization_key */);

  broken_services_.MarkBrokenUntilDefaultNetworkChanges(alternative_service);

  // |broken_services_| should have posted task to expire the brokenness of
  // |alternative_service|.
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Advance time until one time quantum before |alternative_service1|'s
  // brokenness expires.
  test_task_runner_->FastForwardBy(base::Minutes(5) - base::Seconds(1));

  // Ensure |alternative_service| is still marked broken.
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_EQ(0u, expired_alt_svcs_.size());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Advance time by one time quantum.
  test_task_runner_->FastForwardBy(base::Seconds(1));

  // Ensure |alternative_service| brokenness has expired but is still
  // considered recently broken.
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_FALSE(test_task_runner_->HasPendingTask());
  EXPECT_EQ(1u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service.alternative_service,
            expired_alt_svcs_[0].alternative_service);
  EXPECT_EQ(alternative_service.network_anonymization_key,
            expired_alt_svcs_[0].network_anonymization_key);
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));
}

TEST_F(BrokenAlternativeServicesTest, ExpireBrokenAlternateProtocolMappings) {
  BrokenAlternativeService alternative_service(
      AlternativeService(kProtoQUIC, "foo", 443), network_anonymization_key1_,
      true /* use_network_anonymization_key */);

  broken_services_.MarkBroken(alternative_service);

  // |broken_services_| should have posted task to expire the brokenness of
  // |alternative_service|.
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Advance time until one time quantum before |alternative_service1|'s
  // brokenness expires
  test_task_runner_->FastForwardBy(base::Minutes(5) - base::Seconds(1));

  // Ensure |alternative_service| is still marked broken.
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_EQ(0u, expired_alt_svcs_.size());
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());

  // Advance time by one time quantum.
  test_task_runner_->FastForwardBy(base::Seconds(1));

  // Ensure |alternative_service| brokenness has expired but is still
  // considered recently broken
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_FALSE(test_task_runner_->HasPendingTask());
  EXPECT_EQ(1u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service.alternative_service,
            expired_alt_svcs_[0].alternative_service);
  EXPECT_EQ(alternative_service.network_anonymization_key,
            expired_alt_svcs_[0].network_anonymization_key);
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));
}

TEST_F(BrokenAlternativeServicesTest, IsBroken) {
  // Tests the IsBroken() methods.
  BrokenAlternativeService alternative_service(
      AlternativeService(kProtoQUIC, "foo", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);
  base::TimeTicks brokenness_expiration;

  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_FALSE(
      broken_services_.IsBroken(alternative_service, &brokenness_expiration));

  broken_services_.MarkBroken(alternative_service);
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(
      broken_services_.IsBroken(alternative_service, &brokenness_expiration));
  EXPECT_EQ(broken_services_clock_->NowTicks() + base::Minutes(5),
            brokenness_expiration);

  // Fast forward time until |alternative_service|'s brokenness expires.
  test_task_runner_->FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_FALSE(
      broken_services_.IsBroken(alternative_service, &brokenness_expiration));

  broken_services_.MarkBroken(alternative_service);
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(
      broken_services_.IsBroken(alternative_service, &brokenness_expiration));
  EXPECT_EQ(broken_services_clock_->NowTicks() + base::Minutes(10),
            brokenness_expiration);

  broken_services_.Confirm(alternative_service);
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_FALSE(
      broken_services_.IsBroken(alternative_service, &brokenness_expiration));
}

// This test verifies that exponential backoff is applied to the expiration of
// broken alternative service regardless of which MarkBroken method was used.
// In particular, the alternative service's brokenness state is as follows:
// - marked broken on the default network;
// - brokenness expires after one delay;
// - marked broken;
// - (signal received that default network changes);
// - brokenness expires after two intervals.
TEST_F(BrokenAlternativeServicesTest, BrokenAfterBrokenOnDefaultNetwork) {
  BrokenAlternativeService alternative_service(
      AlternativeService(kProtoQUIC, "foo", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);

  // Mark the alternative service broken on the default network.
  broken_services_.MarkBrokenUntilDefaultNetworkChanges(alternative_service);
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  test_task_runner_->FastForwardBy(
      base::Seconds(kBrokenAlternativeProtocolDelaySecs) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));
  // Expire the brokenness after the initial delay.
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  // Mark the alternative service broken.
  broken_services_.MarkBroken(alternative_service);
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  // Verify that the expiration delay has been doubled.
  test_task_runner_->FastForwardBy(
      base::Seconds(kBrokenAlternativeProtocolDelaySecs * 2) -
      base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  // Receive the message that the default network changes.
  broken_services_.OnDefaultNetworkChanged();
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  // Advance one more second so that the second expiration delay is reached.
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));
}

// This test verifies that exponentail backoff is applied to the expiration of
// broken alternative service regardless of which MarkBroken method was used.
// In particular, the alternative service's brokenness state is as follows:
// - marked broken;
// - brokenness expires after one delay;
// - marked broken on the default network;
// - broknenss expires after two intervals;
// - (signal received that default network changes);
TEST_F(BrokenAlternativeServicesTest, BrokenOnDefaultNetworkAfterBroken) {
  BrokenAlternativeService alternative_service(
      AlternativeService(kProtoQUIC, "foo", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);

  // Mark the alternative service broken.
  broken_services_.MarkBroken(alternative_service);
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  test_task_runner_->FastForwardBy(
      base::Seconds(kBrokenAlternativeProtocolDelaySecs) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  // Mark the alternative service broken on the default network.
  broken_services_.MarkBrokenUntilDefaultNetworkChanges(alternative_service);
  // Verify the expiration delay has been doubled.
  test_task_runner_->FastForwardBy(
      base::Seconds(kBrokenAlternativeProtocolDelaySecs * 2) -
      base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  // Receive the message that the default network changes. The alternative
  // servicve is moved to working state.
  broken_services_.OnDefaultNetworkChanged();
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service));
}

// This test verifies that exponentail backoff is applied to expire alternative
// service that's marked broken until the default network changes. When default
// network changes, the exponential backoff is cleared.
TEST_F(BrokenAlternativeServicesTest,
       BrokenUntilDefaultNetworkChangeWithExponentialBackoff) {
  BrokenAlternativeService alternative_service(
      AlternativeService(kProtoQUIC, "foo", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);

  // Mark the alternative service broken on the default network.
  broken_services_.MarkBrokenUntilDefaultNetworkChanges(alternative_service);
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  EXPECT_EQ(base::Seconds(kBrokenAlternativeProtocolDelaySecs),
            test_task_runner_->NextPendingTaskDelay());
  // Expire the brokenness for the 1st time.
  test_task_runner_->FastForwardBy(
      base::Seconds(kBrokenAlternativeProtocolDelaySecs) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  // Mark the alternative service broken on the default network.
  broken_services_.MarkBrokenUntilDefaultNetworkChanges(alternative_service);
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  EXPECT_EQ(base::Seconds(kBrokenAlternativeProtocolDelaySecs * 2),
            test_task_runner_->NextPendingTaskDelay());

  // Expire the brokenness for the 2nd time.
  test_task_runner_->FastForwardBy(
      base::Seconds(kBrokenAlternativeProtocolDelaySecs * 2) -
      base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));

  // Receive the message that the default network changes. The alternative
  // servicve is moved to working state.
  broken_services_.OnDefaultNetworkChanged();
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service));

  // Mark the alternative service broken on the default network.
  // Exponential delay is cleared.
  broken_services_.MarkBrokenUntilDefaultNetworkChanges(alternative_service);
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service));
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  EXPECT_EQ(base::Seconds(kBrokenAlternativeProtocolDelaySecs),
            test_task_runner_->NextPendingTaskDelay());
}

TEST_F(BrokenAlternativeServicesTest, ExponentialBackoff) {
  // Tests the exponential backoff of the computed expiration delay when an
  // alt svc is marked broken. After being marked broken 10 times, the max
  // expiration delay will have been reached and exponential backoff will no
  // longer apply.

  BrokenAlternativeService alternative_service(
      AlternativeService(kProtoQUIC, "foo", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(5) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(10) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(20) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(40) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(80) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(160) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(320) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(640) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(1280) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(2560) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  // Max expiration delay has been reached; subsequent expiration delays from
  // this point forward should not increase further.
  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(2880) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(base::Minutes(2880) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
}

void BrokenAlternativeServicesTest::TestExponentialBackoff(
    base::TimeDelta initial_delay,
    bool exponential_backoff_on_initial_delay) {
  // Tests the exponential backoff of the computed expiration delay when an
  // alt svc is marked broken. After being marked broken 10 times, the max
  // expiration delay will have been reached and exponential backoff will no
  // longer apply.
  broken_services_.SetDelayParams(initial_delay,
                                  exponential_backoff_on_initial_delay);

  BrokenAlternativeService alternative_service(
      AlternativeService(kProtoQUIC, "foo", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);

  broken_services_.MarkBroken(alternative_service);
  test_task_runner_->FastForwardBy(initial_delay - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service));

  for (size_t broken_count = 1; broken_count < 20; ++broken_count) {
    broken_services_.MarkBroken(alternative_service);
    base::TimeDelta broken_delay;
    if (exponential_backoff_on_initial_delay) {
      broken_delay = initial_delay * (1 << broken_count);
    } else {
      broken_delay = base::Seconds(kBrokenAlternativeProtocolDelaySecs) *
                     (1 << (broken_count - 1));
    }
    if (broken_delay > base::Days(2)) {
      broken_delay = base::Days(2);
    }
    test_task_runner_->FastForwardBy(broken_delay - base::Seconds(1));
    EXPECT_TRUE(broken_services_.IsBroken(alternative_service));
    test_task_runner_->FastForwardBy(base::Seconds(1));
    EXPECT_FALSE(broken_services_.IsBroken(alternative_service));
  }
}

TEST_F(BrokenAlternativeServicesTest, ExponentialBackoff_OneSecond_True) {
  TestExponentialBackoff(base::Seconds(1), true);
}

TEST_F(BrokenAlternativeServicesTest, ExponentialBackoff_OneSecond_False) {
  TestExponentialBackoff(base::Seconds(1), false);
}

TEST_F(BrokenAlternativeServicesTest, ExponentialBackoff_FiveSeconds_True) {
  TestExponentialBackoff(base::Seconds(5), true);
}

TEST_F(BrokenAlternativeServicesTest, ExponentialBackoff_FiveSeconds_False) {
  TestExponentialBackoff(base::Seconds(5), false);
}

TEST_F(BrokenAlternativeServicesTest, ExponentialBackoff_TenSeconds_True) {
  TestExponentialBackoff(base::Seconds(10), true);
}

TEST_F(BrokenAlternativeServicesTest, ExponentialBackoff_TenSeconds_False) {
  TestExponentialBackoff(base::Seconds(10), false);
}

TEST_F(BrokenAlternativeServicesTest, ExponentialBackoff_FiveMinutes_True) {
  TestExponentialBackoff(base::Seconds(kBrokenAlternativeProtocolDelaySecs),
                         true);
}

TEST_F(BrokenAlternativeServicesTest, ExponentialBackoff_FiveMinutes_False) {
  TestExponentialBackoff(base::Seconds(kBrokenAlternativeProtocolDelaySecs),
                         false);
}

TEST_F(BrokenAlternativeServicesTest, RemoveExpiredBrokenAltSvc) {
  // This test will mark broken an alternative service A that has already been
  // marked broken many times, then immediately mark another alternative service
  // B as broken for the first time. Because A's been marked broken many times
  // already, its brokenness will be scheduled to expire much further in the
  // future than B, even though it was marked broken before B. This test makes
  // sure that even though A was marked broken before B, B's brokenness should
  // expire before A.

  BrokenAlternativeService alternative_service1(
      AlternativeService(kProtoQUIC, "foo", 443), network_anonymization_key1_,
      true /* use_network_anonymization_key */);
  BrokenAlternativeService alternative_service2(
      AlternativeService(kProtoQUIC, "bar", 443), network_anonymization_key2_,
      true /* use_network_anonymization_key */);

  // Repeately mark |alternative_service1| broken and let brokenness expire.
  // Do this a few times.

  broken_services_.MarkBroken(alternative_service1);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  test_task_runner_->FastForwardBy(base::Minutes(5));
  EXPECT_EQ(1u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service1.alternative_service,
            expired_alt_svcs_.back().alternative_service);
  EXPECT_EQ(alternative_service1.network_anonymization_key,
            expired_alt_svcs_.back().network_anonymization_key);

  broken_services_.MarkBroken(alternative_service1);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  test_task_runner_->FastForwardBy(base::Minutes(10));
  EXPECT_EQ(2u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service1.alternative_service,
            expired_alt_svcs_.back().alternative_service);
  EXPECT_EQ(alternative_service1.network_anonymization_key,
            expired_alt_svcs_.back().network_anonymization_key);

  broken_services_.MarkBroken(alternative_service1);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  test_task_runner_->FastForwardBy(base::Minutes(20));
  EXPECT_EQ(3u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service1.alternative_service,
            expired_alt_svcs_.back().alternative_service);
  EXPECT_EQ(alternative_service1.network_anonymization_key,
            expired_alt_svcs_.back().network_anonymization_key);

  expired_alt_svcs_.clear();

  // Mark |alternative_service1| broken (will be given longer expiration delay),
  // then mark |alternative_service2| broken (will be given shorter expiration
  // delay).
  broken_services_.MarkBroken(alternative_service1);
  broken_services_.MarkBroken(alternative_service2);

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));

  // Advance time until one time quantum before |alternative_service2|'s
  // brokenness expires.
  test_task_runner_->FastForwardBy(base::Minutes(5) - base::Seconds(1));

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_EQ(0u, expired_alt_svcs_.size());

  // Advance time by one time quantum. |alternative_service2| should no longer
  // be broken.
  test_task_runner_->FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_EQ(1u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service2.alternative_service,
            expired_alt_svcs_[0].alternative_service);
  EXPECT_EQ(alternative_service2.network_anonymization_key,
            expired_alt_svcs_[0].network_anonymization_key);

  // Advance time until one time quantum before |alternative_service1|'s
  // brokenness expires
  test_task_runner_->FastForwardBy(base::Minutes(40) - base::Minutes(5) -
                                   base::Seconds(1));

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_EQ(1u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service2.alternative_service,
            expired_alt_svcs_[0].alternative_service);
  EXPECT_EQ(alternative_service2.network_anonymization_key,
            expired_alt_svcs_[0].network_anonymization_key);

  // Advance time by one time quantum.  |alternative_service1| should no longer
  // be broken.
  test_task_runner_->FastForwardBy(base::Seconds(1));

  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_EQ(2u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service2.alternative_service,
            expired_alt_svcs_[0].alternative_service);
  EXPECT_EQ(alternative_service2.network_anonymization_key,
            expired_alt_svcs_[0].network_anonymization_key);
  EXPECT_EQ(alternative_service1.alternative_service,
            expired_alt_svcs_[1].alternative_service);
  EXPECT_EQ(alternative_service1.network_anonymization_key,
            expired_alt_svcs_[1].network_anonymization_key);
}

// Same as above, but checks a single alternative service with two different
// NetworkAnonymizationKeys.
TEST_F(BrokenAlternativeServicesTest,
       RemoveExpiredBrokenAltSvcWithNetworkAnonymizationKey) {
  BrokenAlternativeService alternative_service1(
      AlternativeService(kProtoQUIC, "foo", 443), network_anonymization_key1_,
      true /* use_network_anonymization_key */);
  BrokenAlternativeService alternative_service2(
      AlternativeService(kProtoQUIC, "foo", 443), network_anonymization_key2_,
      true /* use_network_anonymization_key */);

  // Repeately mark |alternative_service1| broken and let brokenness expire.
  // Do this a few times.

  broken_services_.MarkBroken(alternative_service1);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  test_task_runner_->FastForwardBy(base::Minutes(5));
  EXPECT_EQ(1u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service1.alternative_service,
            expired_alt_svcs_.back().alternative_service);
  EXPECT_EQ(alternative_service1.network_anonymization_key,
            expired_alt_svcs_.back().network_anonymization_key);

  broken_services_.MarkBroken(alternative_service1);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  test_task_runner_->FastForwardBy(base::Minutes(10));
  EXPECT_EQ(2u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service1.alternative_service,
            expired_alt_svcs_.back().alternative_service);
  EXPECT_EQ(alternative_service1.network_anonymization_key,
            expired_alt_svcs_.back().network_anonymization_key);

  broken_services_.MarkBroken(alternative_service1);
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  test_task_runner_->FastForwardBy(base::Minutes(20));
  EXPECT_EQ(3u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service1.alternative_service,
            expired_alt_svcs_.back().alternative_service);
  EXPECT_EQ(alternative_service1.network_anonymization_key,
            expired_alt_svcs_.back().network_anonymization_key);

  expired_alt_svcs_.clear();

  // Mark |alternative_service1| broken (will be given longer expiration delay),
  // then mark |alternative_service2| broken (will be given shorter expiration
  // delay).
  broken_services_.MarkBroken(alternative_service1);
  broken_services_.MarkBroken(alternative_service2);

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));

  // Advance time until one time quantum before |alternative_service2|'s
  // brokenness expires.
  test_task_runner_->FastForwardBy(base::Minutes(5) - base::Seconds(1));

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_EQ(0u, expired_alt_svcs_.size());

  // Advance time by one time quantum. |alternative_service2| should no longer
  // be broken.
  test_task_runner_->FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_EQ(1u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service2.alternative_service,
            expired_alt_svcs_[0].alternative_service);
  EXPECT_EQ(alternative_service2.network_anonymization_key,
            expired_alt_svcs_[0].network_anonymization_key);

  // Advance time until one time quantum before |alternative_service1|'s
  // brokenness expires
  test_task_runner_->FastForwardBy(base::Minutes(40) - base::Minutes(5) -
                                   base::Seconds(1));

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_EQ(1u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service2.alternative_service,
            expired_alt_svcs_[0].alternative_service);
  EXPECT_EQ(alternative_service2.network_anonymization_key,
            expired_alt_svcs_[0].network_anonymization_key);

  // Advance time by one time quantum.  |alternative_service1| should no longer
  // be broken.
  test_task_runner_->FastForwardBy(base::Seconds(1));

  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_EQ(2u, expired_alt_svcs_.size());
  EXPECT_EQ(alternative_service2.alternative_service,
            expired_alt_svcs_[0].alternative_service);
  EXPECT_EQ(alternative_service2.network_anonymization_key,
            expired_alt_svcs_[0].network_anonymization_key);
  EXPECT_EQ(alternative_service1.alternative_service,
            expired_alt_svcs_[1].alternative_service);
  EXPECT_EQ(alternative_service1.network_anonymization_key,
            expired_alt_svcs_[1].network_anonymization_key);
}

TEST_F(BrokenAlternativeServicesTest, SetBrokenAlternativeServices) {
  BrokenAlternativeService alternative_service1(
      AlternativeService(kProtoQUIC, "foo1", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);
  BrokenAlternativeService alternative_service2(
      AlternativeService(kProtoQUIC, "foo2", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);

  base::TimeDelta delay1 = base::Minutes(1);

  std::unique_ptr<BrokenAlternativeServiceList> broken_list =
      std::make_unique<BrokenAlternativeServiceList>();
  broken_list->push_back(
      {alternative_service1, broken_services_clock_->NowTicks() + delay1});

  std::unique_ptr<RecentlyBrokenAlternativeServices> recently_broken_map =
      std::make_unique<RecentlyBrokenAlternativeServices>(10);
  recently_broken_map->Put(alternative_service1, 1);
  recently_broken_map->Put(alternative_service2, 2);

  broken_services_.SetBrokenAndRecentlyBrokenAlternativeServices(
      std::move(broken_list), std::move(recently_broken_map));

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));

  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service2));

  // Make sure |alternative_service1| expires after the delay in |broken_list|.
  test_task_runner_->FastForwardBy(delay1 - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));

  // Make sure the broken counts in |recently_broken_map| translate to the
  // correct expiration delays if the alternative services are marked broken.
  broken_services_.MarkBroken(alternative_service2);
  broken_services_.MarkBroken(alternative_service1);

  test_task_runner_->FastForwardBy(base::Minutes(10) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));

  test_task_runner_->FastForwardBy(base::Minutes(20) - base::Minutes(10) -
                                   base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
}

TEST_F(BrokenAlternativeServicesTest,
       SetBrokenAlternativeServicesWithExisting) {
  BrokenAlternativeService alternative_service1(
      AlternativeService(kProtoQUIC, "foo1", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);
  BrokenAlternativeService alternative_service2(
      AlternativeService(kProtoQUIC, "foo2", 443), network_anonymization_key1_,
      true /* use_network_anonymization_key */);
  BrokenAlternativeService alternative_service3(
      AlternativeService(kProtoQUIC, "foo3", 443), network_anonymization_key2_,
      true /* use_network_anonymization_key */);

  std::unique_ptr<BrokenAlternativeServiceList> broken_list =
      std::make_unique<BrokenAlternativeServiceList>();
  broken_list->push_back(
      {alternative_service1,
       broken_services_clock_->NowTicks() + base::Minutes(3)});
  broken_list->push_back(
      {alternative_service3,
       broken_services_clock_->NowTicks() + base::Minutes(1)});

  std::unique_ptr<RecentlyBrokenAlternativeServices> recently_broken_map =
      std::make_unique<RecentlyBrokenAlternativeServices>(10);
  recently_broken_map->Put(alternative_service1, 1);
  recently_broken_map->Put(alternative_service3, 1);

  broken_services_.MarkBroken(alternative_service1);
  broken_services_.MarkBroken(alternative_service2);

  // At this point, |alternative_service1| and |alternative_service2| are marked
  // broken and should expire in 5 minutes.
  // Adding |broken_list| should overwrite |alternative_service1|'s expiration
  // time to 3 minutes, and additionally mark |alternative_service3|
  // broken with an expiration time of 1 minute.
  broken_services_.SetBrokenAndRecentlyBrokenAlternativeServices(
      std::move(broken_list), std::move(recently_broken_map));

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service3));

  // Make sure |alternative_service3|'s brokenness expires in 1 minute.
  test_task_runner_->FastForwardBy(base::Minutes(1) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service3));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));

  // Make sure |alternative_service1|'s brokenness expires in 2 more minutes.
  test_task_runner_->FastForwardBy(base::Minutes(2) - base::Seconds(1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));

  // Make sure |alternative_service2|'s brokenness expires in 2 more minutes.
  test_task_runner_->FastForwardBy(base::Minutes(2) - base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service3));

  // Make sure recently broken alternative services are in most-recently-used
  // order. SetBrokenAndRecentlyBrokenAlternativeServices() will add
  // entries in |recently_broken_map| (that aren't already marked recently
  // broken in |broken_services_|) to the back of |broken_services_|'s
  // recency list; in this case, only |alternative_service3| is added as
  // recently broken.
  auto it = broken_services_.recently_broken_alternative_services().begin();
  EXPECT_EQ(alternative_service2.alternative_service,
            it->first.alternative_service);
  EXPECT_EQ(alternative_service2.network_anonymization_key,
            it->first.network_anonymization_key);
  ++it;
  EXPECT_EQ(alternative_service1.alternative_service,
            it->first.alternative_service);
  EXPECT_EQ(alternative_service1.network_anonymization_key,
            it->first.network_anonymization_key);
  ++it;
  EXPECT_EQ(alternative_service3.alternative_service,
            it->first.alternative_service);
  EXPECT_EQ(alternative_service3.network_anonymization_key,
            it->first.network_anonymization_key);
}

TEST_F(BrokenAlternativeServicesTest, ScheduleExpireTaskAfterExpire) {
  // This test will check that when a broken alt svc expires, an expiration task
  // is scheduled for the next broken alt svc in the expiration queue.

  BrokenAlternativeService alternative_service1(
      AlternativeService(kProtoQUIC, "foo", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);
  BrokenAlternativeService alternative_service2(
      AlternativeService(kProtoQUIC, "bar", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);

  // Mark |alternative_service1| broken and let brokenness expire. This will
  // increase its expiration delay the next time it's marked broken.
  broken_services_.MarkBroken(alternative_service1);
  test_task_runner_->FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(test_task_runner_->HasPendingTask());

  // Mark |alternative_service1| and |alternative_service2| broken and
  // let |alternative_service2|'s brokenness expire.
  broken_services_.MarkBroken(alternative_service1);
  broken_services_.MarkBroken(alternative_service2);

  test_task_runner_->FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(broken_services_.IsBroken(alternative_service2));
  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));

  // Make sure an expiration task has been scheduled for expiring the brokenness
  // of |alternative_service1|.
  EXPECT_TRUE(test_task_runner_->HasPendingTask());
}

TEST_F(BrokenAlternativeServicesTest, Clear) {
  BrokenAlternativeService alternative_service1(
      AlternativeService(kProtoQUIC, "foo", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);
  BrokenAlternativeService alternative_service2(
      AlternativeService(kProtoQUIC, "bar", 443), NetworkAnonymizationKey(),
      true /* use_network_anonymization_key */);

  broken_services_.MarkBroken(alternative_service1);
  broken_services_.MarkRecentlyBroken(alternative_service2);

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service2));

  broken_services_.Clear();

  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));

  std::unique_ptr<BrokenAlternativeServiceList> broken_list =
      std::make_unique<BrokenAlternativeServiceList>();
  broken_list->push_back(
      {alternative_service1,
       broken_services_clock_->NowTicks() + base::Minutes(1)});

  std::unique_ptr<RecentlyBrokenAlternativeServices> recently_broken_map =
      std::make_unique<RecentlyBrokenAlternativeServices>(10);
  recently_broken_map->Put(alternative_service2, 2);

  broken_services_.SetBrokenAndRecentlyBrokenAlternativeServices(
      std::move(broken_list), std::move(recently_broken_map));

  EXPECT_TRUE(broken_services_.IsBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_TRUE(broken_services_.WasRecentlyBroken(alternative_service2));

  broken_services_.Clear();

  EXPECT_FALSE(broken_services_.IsBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service1));
  EXPECT_FALSE(broken_services_.WasRecentlyBroken(alternative_service2));
}

}  // namespace

}  // namespace net
