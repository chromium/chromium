// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_endpoint_manager.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "net/base/network_isolation_key.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

class TestReportingCache : public ReportingCache {
 public:
  class PersistentReportingStore;

  // Tests using this class only use one origin/group.
  TestReportingCache(const url::Origin& expected_origin,
                     const std::string& expected_group)
      : expected_origin_(expected_origin), expected_group_(expected_group) {}
  ~TestReportingCache() override = default;

  void SetEndpoint(const NetworkIsolationKey& network_isolation_key,
                   const ReportingEndpoint& reporting_endpoint) {
    reporting_endpoints_[network_isolation_key].push_back(reporting_endpoint);
  }

  // ReportingCache implementation:

  std::vector<ReportingEndpoint> GetCandidateEndpointsForDelivery(
      const NetworkIsolationKey& network_isolation_key,
      const url::Origin& origin,
      const std::string& group_name) override {
    EXPECT_EQ(expected_origin_, origin);
    EXPECT_EQ(expected_group_, group_name);
    return reporting_endpoints_[network_isolation_key];
  }

  // Everything below is NOTREACHED.
  void AddReport(const GURL& url,
                 const std::string& user_agent,
                 const std::string& group_name,
                 const std::string& type,
                 std::unique_ptr<const base::Value> body,
                 int depth,
                 base::TimeTicks queued,
                 int attempts) override {
    NOTREACHED();
  }
  void GetReports(
      std::vector<const ReportingReport*>* reports_out) const override {
    NOTREACHED();
  }
  base::Value GetReportsAsValue() const override {
    NOTREACHED();
    return base::Value();
  }
  void GetNonpendingReports(
      std::vector<const ReportingReport*>* reports_out) const override {
    NOTREACHED();
  }
  void SetReportsPending(
      const std::vector<const ReportingReport*>& reports) override {
    NOTREACHED();
  }
  void ClearReportsPending(
      const std::vector<const ReportingReport*>& reports) override {
    NOTREACHED();
  }
  void IncrementReportsAttempts(
      const std::vector<const ReportingReport*>& reports) override {
    NOTREACHED();
  }
  void IncrementEndpointDeliveries(const url::Origin& origin,
                                   const std::string& group_name,
                                   const GURL& url,
                                   int reports_delivered,
                                   bool successful) override {
    NOTREACHED();
  }
  void RemoveReports(const std::vector<const ReportingReport*>& reports,
                     ReportingReport::Outcome outcome) override {
    NOTREACHED();
  }
  void RemoveAllReports(ReportingReport::Outcome outcome) override {
    NOTREACHED();
  }
  size_t GetFullReportCountForTesting() const override {
    NOTREACHED();
    return 0;
  }
  bool IsReportPendingForTesting(const ReportingReport* report) const override {
    NOTREACHED();
    return false;
  }
  bool IsReportDoomedForTesting(const ReportingReport* report) const override {
    NOTREACHED();
    return false;
  }
  void OnParsedHeader(
      const url::Origin& origin,
      std::vector<ReportingEndpointGroup> parsed_header) override {
    NOTREACHED();
  }
  std::vector<url::Origin> GetAllOrigins() const override {
    NOTREACHED();
    return std::vector<url::Origin>();
  }
  void RemoveClient(const url::Origin& origin) override { NOTREACHED(); }
  void RemoveAllClients() override { NOTREACHED(); }
  void RemoveEndpointGroup(const url::Origin& origin,
                           const std::string& group_name) override {
    NOTREACHED();
  }
  void RemoveEndpointsForUrl(const GURL& url) override { NOTREACHED(); }
  void AddClientsLoadedFromStore(
      std::vector<ReportingEndpoint> loaded_endpoints,
      std::vector<CachedReportingEndpointGroup> loaded_endpoint_groups)
      override {
    NOTREACHED();
  }
  base::Value GetClientsAsValue() const override {
    NOTREACHED();
    return base::Value();
  }
  size_t GetEndpointCount() const override {
    NOTREACHED();
    return 0;
  }
  void Flush() override { NOTREACHED(); }
  ReportingEndpoint GetEndpointForTesting(const url::Origin& origin,
                                          const std::string& group_name,
                                          const GURL& url) const override {
    NOTREACHED();
    return ReportingEndpoint();
  }
  bool EndpointGroupExistsForTesting(const url::Origin& origin,
                                     const std::string& group_name,
                                     OriginSubdomains include_subdomains,
                                     base::Time expires) const override {
    NOTREACHED();
    return false;
  }
  size_t GetEndpointGroupCountForTesting() const override {
    NOTREACHED();
    return 0;
  }
  void SetEndpointForTesting(const url::Origin& origin,
                             const std::string& group_name,
                             const GURL& url,
                             OriginSubdomains include_subdomains,
                             base::Time expires,
                             int priority,
                             int weight) override {
    NOTREACHED();
  }

 private:
  const url::Origin expected_origin_;
  const std::string expected_group_;

  std::map<NetworkIsolationKey, std::vector<ReportingEndpoint>>
      reporting_endpoints_;

  DISALLOW_COPY_AND_ASSIGN(TestReportingCache);
};

class ReportingEndpointManagerTest : public testing::Test {
 public:
  ReportingEndpointManagerTest() : cache_(kOrigin, kGroup) {
    policy_.endpoint_backoff_policy.num_errors_to_ignore = 0;
    policy_.endpoint_backoff_policy.initial_delay_ms = 60000;
    policy_.endpoint_backoff_policy.multiply_factor = 2.0;
    policy_.endpoint_backoff_policy.jitter_factor = 0.0;
    policy_.endpoint_backoff_policy.maximum_backoff_ms = -1;
    policy_.endpoint_backoff_policy.entry_lifetime_ms = 0;
    policy_.endpoint_backoff_policy.always_use_initial_delay = false;

    clock_.SetNowTicks(base::TimeTicks());

    endpoint_manager_ = ReportingEndpointManager::Create(
        &policy_, &clock_, &delegate_, &cache_, TestReportingRandIntCallback());
  }

 protected:
  void SetEndpoint(
      const GURL& endpoint,
      int priority = ReportingEndpoint::EndpointInfo::kDefaultPriority,
      int weight = ReportingEndpoint::EndpointInfo::kDefaultWeight,
      const NetworkIsolationKey& network_isolation_key =
          NetworkIsolationKey()) {
    cache_.SetEndpoint(network_isolation_key,
                       ReportingEndpoint(kOrigin, kGroup,
                                         ReportingEndpoint::EndpointInfo{
                                             endpoint, priority, weight}));
  }

  const url::Origin kOrigin = url::Origin::Create(GURL("https://origin/"));
  const std::string kGroup = "group";
  const GURL kEndpoint = GURL("https://endpoint/");

  ReportingPolicy policy_;
  base::SimpleTestTickClock clock_;
  TestReportingDelegate delegate_;
  TestReportingCache cache_;
  std::unique_ptr<ReportingEndpointManager> endpoint_manager_;
};

TEST_F(ReportingEndpointManagerTest, NoEndpoint) {
  ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  EXPECT_FALSE(endpoint);
}

TEST_F(ReportingEndpointManagerTest, Endpoint) {
  SetEndpoint(kEndpoint);

  ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint, endpoint.info.url);
}

TEST_F(ReportingEndpointManagerTest, BackedOffEndpoint) {
  ASSERT_EQ(2.0, policy_.endpoint_backoff_policy.multiply_factor);

  base::TimeDelta initial_delay = base::TimeDelta::FromMilliseconds(
      policy_.endpoint_backoff_policy.initial_delay_ms);

  SetEndpoint(kEndpoint);

  endpoint_manager_->InformOfEndpointRequest(NetworkIsolationKey(), kEndpoint,
                                             false);

  // After one failure, endpoint is in exponential backoff.
  ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  EXPECT_FALSE(endpoint);

  // After initial delay, endpoint is usable again.
  clock_.Advance(initial_delay);

  ReportingEndpoint endpoint2 = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  ASSERT_TRUE(endpoint2);
  EXPECT_EQ(kEndpoint, endpoint2.info.url);

  endpoint_manager_->InformOfEndpointRequest(NetworkIsolationKey(), kEndpoint,
                                             false);

  // After a second failure, endpoint is backed off again.
  ReportingEndpoint endpoint3 = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  EXPECT_FALSE(endpoint3);

  clock_.Advance(initial_delay);

  // Next backoff is longer -- 2x the first -- so endpoint isn't usable yet.
  ReportingEndpoint endpoint4 = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  EXPECT_FALSE(endpoint4);

  clock_.Advance(initial_delay);

  // After 2x the initial delay, the endpoint is usable again.
  ReportingEndpoint endpoint5 = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  ASSERT_TRUE(endpoint5);
  EXPECT_EQ(kEndpoint, endpoint5.info.url);

  endpoint_manager_->InformOfEndpointRequest(NetworkIsolationKey(), kEndpoint,
                                             true);
  endpoint_manager_->InformOfEndpointRequest(NetworkIsolationKey(), kEndpoint,
                                             true);

  // Two more successful requests should reset the backoff to the initial delay
  // again.
  endpoint_manager_->InformOfEndpointRequest(NetworkIsolationKey(), kEndpoint,
                                             false);

  ReportingEndpoint endpoint6 = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  EXPECT_FALSE(endpoint6);

  clock_.Advance(initial_delay);

  ReportingEndpoint endpoint7 = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  EXPECT_TRUE(endpoint7);
}

// Make sure that multiple endpoints will all be returned at some point, to
// avoid accidentally or intentionally implementing any priority ordering.
TEST_F(ReportingEndpointManagerTest, RandomEndpoint) {
  static const GURL kEndpoint1("https://endpoint1/");
  static const GURL kEndpoint2("https://endpoint2/");
  static const int kMaxAttempts = 20;

  SetEndpoint(kEndpoint1);
  SetEndpoint(kEndpoint2);

  bool endpoint1_seen = false;
  bool endpoint2_seen = false;

  for (int i = 0; i < kMaxAttempts; ++i) {
    ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
        NetworkIsolationKey(), kOrigin, kGroup);
    ASSERT_TRUE(endpoint);
    ASSERT_TRUE(endpoint.info.url == kEndpoint1 ||
                endpoint.info.url == kEndpoint2);

    if (endpoint.info.url == kEndpoint1)
      endpoint1_seen = true;
    else if (endpoint.info.url == kEndpoint2)
      endpoint2_seen = true;

    if (endpoint1_seen && endpoint2_seen)
      break;
  }

  EXPECT_TRUE(endpoint1_seen);
  EXPECT_TRUE(endpoint2_seen);
}

TEST_F(ReportingEndpointManagerTest, Priority) {
  static const GURL kPrimaryEndpoint("https://endpoint1/");
  static const GURL kBackupEndpoint("https://endpoint2/");

  SetEndpoint(kPrimaryEndpoint, 10 /* priority */,
              ReportingEndpoint::EndpointInfo::kDefaultWeight);
  SetEndpoint(kBackupEndpoint, 20 /* priority */,
              ReportingEndpoint::EndpointInfo::kDefaultWeight);

  ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kPrimaryEndpoint, endpoint.info.url);

  // The backoff policy we set up in the constructor means that a single failed
  // upload will take the primary endpoint out of contention.  This should cause
  // us to choose the backend endpoint.
  endpoint_manager_->InformOfEndpointRequest(NetworkIsolationKey(),
                                             kPrimaryEndpoint, false);
  ReportingEndpoint endpoint2 = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  ASSERT_TRUE(endpoint2);
  EXPECT_EQ(kBackupEndpoint, endpoint2.info.url);

  // Advance the current time far enough to clear out the primary endpoint's
  // backoff clock.  This should bring the primary endpoint back into play.
  clock_.Advance(base::TimeDelta::FromMinutes(2));
  ReportingEndpoint endpoint3 = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  ASSERT_TRUE(endpoint3);
  EXPECT_EQ(kPrimaryEndpoint, endpoint3.info.url);
}

// Note: This test depends on the deterministic mock RandIntCallback set up in
// TestReportingContext, which returns consecutive integers starting at 0
// (modulo the requested range, plus the requested minimum).
TEST_F(ReportingEndpointManagerTest, Weight) {
  static const GURL kEndpoint1("https://endpoint1/");
  static const GURL kEndpoint2("https://endpoint2/");

  static const int kEndpoint1Weight = 5;
  static const int kEndpoint2Weight = 2;
  static const int kTotalEndpointWeight = kEndpoint1Weight + kEndpoint2Weight;

  SetEndpoint(kEndpoint1, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              kEndpoint1Weight);
  SetEndpoint(kEndpoint2, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              kEndpoint2Weight);

  int endpoint1_count = 0;
  int endpoint2_count = 0;

  for (int i = 0; i < kTotalEndpointWeight; ++i) {
    ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
        NetworkIsolationKey(), kOrigin, kGroup);
    ASSERT_TRUE(endpoint);
    ASSERT_TRUE(endpoint.info.url == kEndpoint1 ||
                endpoint.info.url == kEndpoint2);

    if (endpoint.info.url == kEndpoint1)
      ++endpoint1_count;
    else if (endpoint.info.url == kEndpoint2)
      ++endpoint2_count;
  }

  EXPECT_EQ(kEndpoint1Weight, endpoint1_count);
  EXPECT_EQ(kEndpoint2Weight, endpoint2_count);
}

TEST_F(ReportingEndpointManagerTest, ZeroWeights) {
  static const GURL kEndpoint1("https://endpoint1/");
  static const GURL kEndpoint2("https://endpoint2/");

  SetEndpoint(kEndpoint1, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              0 /* weight */);
  SetEndpoint(kEndpoint2, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              0 /* weight */);

  int endpoint1_count = 0;
  int endpoint2_count = 0;

  for (int i = 0; i < 10; ++i) {
    ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
        NetworkIsolationKey(), kOrigin, kGroup);
    ASSERT_TRUE(endpoint);
    ASSERT_TRUE(endpoint.info.url == kEndpoint1 ||
                endpoint.info.url == kEndpoint2);

    if (endpoint.info.url == kEndpoint1)
      ++endpoint1_count;
    else if (endpoint.info.url == kEndpoint2)
      ++endpoint2_count;
  }

  EXPECT_EQ(5, endpoint1_count);
  EXPECT_EQ(5, endpoint2_count);
}

// Check that ReportingEndpointManager distinguishes NetworkIsolationKeys.
TEST_F(ReportingEndpointManagerTest, NetworkIsolationKey) {
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://origin2/"));

  const NetworkIsolationKey kNetworkIsolationKey1(kOrigin, kOrigin);
  const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  // An Endpoint set for kNetworkIsolationKey1 should not affect
  // kNetworkIsolationKey2.
  SetEndpoint(kEndpoint, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              0 /* weight */, kNetworkIsolationKey1);
  ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
      kNetworkIsolationKey1, kOrigin, kGroup);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint, endpoint.info.url);
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(kNetworkIsolationKey2,
                                                          kOrigin, kGroup));
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(NetworkIsolationKey(),
                                                          kOrigin, kGroup));

  // Set the same Endpoint for kNetworkIsolationKey2, so both should be
  // reporting to the same URL.
  SetEndpoint(kEndpoint, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              0 /* weight */, kNetworkIsolationKey2);
  endpoint = endpoint_manager_->FindEndpointForDelivery(kNetworkIsolationKey1,
                                                        kOrigin, kGroup);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint, endpoint.info.url);
  endpoint = endpoint_manager_->FindEndpointForDelivery(kNetworkIsolationKey2,
                                                        kOrigin, kGroup);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint, endpoint.info.url);
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(NetworkIsolationKey(),
                                                          kOrigin, kGroup));

  // An error reporting to that URL in the context of kNetworkIsolationKey1
  // should only affect the Endpoint retrieved in the context of
  // kNetworkIsolationKey1.
  endpoint_manager_->InformOfEndpointRequest(kNetworkIsolationKey1, kEndpoint,
                                             false);
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(kNetworkIsolationKey1,
                                                          kOrigin, kGroup));
  endpoint = endpoint_manager_->FindEndpointForDelivery(kNetworkIsolationKey2,
                                                        kOrigin, kGroup);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint, endpoint.info.url);
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(NetworkIsolationKey(),
                                                          kOrigin, kGroup));
}

TEST_F(ReportingEndpointManagerTest, NetworkIsolationKeyWithMultipleEndpoints) {
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://origin2/"));

  const NetworkIsolationKey kNetworkIsolationKey1(kOrigin, kOrigin);
  const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  const GURL kEndpoint1("https://endpoint1/");
  const GURL kEndpoint2("https://endpoint2/");
  const GURL kEndpoint3("https://endpoint3/");
  const int kMaxAttempts = 20;

  // Add two Endpoints for kNetworkIsolationKey1, and a different one for
  // kNetworkIsolationKey2.
  SetEndpoint(kEndpoint1, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              ReportingEndpoint::EndpointInfo::kDefaultWeight,
              kNetworkIsolationKey1);
  SetEndpoint(kEndpoint2, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              ReportingEndpoint::EndpointInfo::kDefaultWeight,
              kNetworkIsolationKey1);
  SetEndpoint(kEndpoint3, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              ReportingEndpoint::EndpointInfo::kDefaultWeight,
              kNetworkIsolationKey2);

  bool endpoint1_seen = false;
  bool endpoint2_seen = false;

  // Make sure that calling FindEndpointForDelivery() with kNetworkIsolationKey1
  // can return both of its endpoints, but not kNetworkIsolationKey2's endpoint.
  for (int i = 0; i < kMaxAttempts; ++i) {
    ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
        kNetworkIsolationKey1, kOrigin, kGroup);
    ASSERT_TRUE(endpoint);
    ASSERT_TRUE(endpoint.info.url == kEndpoint1 ||
                endpoint.info.url == kEndpoint2);

    if (endpoint.info.url == kEndpoint1) {
      endpoint1_seen = true;
    } else if (endpoint.info.url == kEndpoint2) {
      endpoint2_seen = true;
    }
  }

  EXPECT_TRUE(endpoint1_seen);
  EXPECT_TRUE(endpoint2_seen);

  ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
      kNetworkIsolationKey2, kOrigin, kGroup);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint3, endpoint.info.url);
}

TEST_F(ReportingEndpointManagerTest, CacheEviction) {
  // Add |kMaxEndpointBackoffCacheSize| endpoints.
  for (int i = 0; i < ReportingEndpointManager::kMaxEndpointBackoffCacheSize;
       ++i) {
    SetEndpoint(GURL(base::StringPrintf("https://endpoint%i/", i)));
  }

  // Mark each endpoint as bad, one-at-a-time. Use FindEndpointForDelivery() to
  // pick which one to mark as bad, both to exercise the code walking through
  // all endpoints, and as a sanity check.
  std::set<GURL> seen_endpoints;
  for (int i = 0; i < ReportingEndpointManager::kMaxEndpointBackoffCacheSize;
       ++i) {
    ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
        NetworkIsolationKey(), kOrigin, kGroup);
    EXPECT_TRUE(endpoint);
    EXPECT_FALSE(seen_endpoints.count(endpoint.info.url));
    seen_endpoints.insert(endpoint.info.url);
    endpoint_manager_->InformOfEndpointRequest(NetworkIsolationKey(),
                                               endpoint.info.url, false);
  }
  // All endpoints should now be marked as bad.
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(NetworkIsolationKey(),
                                                          kOrigin, kGroup));

  // Add another endpoint with a different NetworkIsolationKey;
  const NetworkIsolationKey kNetworkIsolationKey(kOrigin, kOrigin);
  SetEndpoint(kEndpoint, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              ReportingEndpoint::EndpointInfo::kDefaultWeight,
              kNetworkIsolationKey);
  // All endpoints associated with the empty NetworkIsolationKey should still be
  // marked as bad.
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(NetworkIsolationKey(),
                                                          kOrigin, kGroup));

  // Make the endpoint added for the kNetworkIsolationKey as bad.
  endpoint_manager_->InformOfEndpointRequest(kNetworkIsolationKey, kEndpoint,
                                             false);
  // The only endpoint for kNetworkIsolationKey should still be marked as bad.
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(kNetworkIsolationKey,
                                                          kOrigin, kGroup));
  // One of the endpoints for the empty NetworkIsolationKey should no longer be
  // marked as bad, due to eviction.
  ReportingEndpoint endpoint = endpoint_manager_->FindEndpointForDelivery(
      NetworkIsolationKey(), kOrigin, kGroup);
  EXPECT_TRUE(endpoint);

  // Reporting a success for the (only) good endpoint for the empty
  // NetworkIsolationKey should evict the entry for kNetworkIsolationKey, since
  // the most recent FindEndpointForDelivery() call visited all of the empty
  // NetworkIsolationKey's cached bad entries.
  endpoint_manager_->InformOfEndpointRequest(NetworkIsolationKey(),
                                             endpoint.info.url, true);

  EXPECT_TRUE(endpoint_manager_->FindEndpointForDelivery(kNetworkIsolationKey,
                                                         kOrigin, kGroup));
}

}  // namespace
}  // namespace net
