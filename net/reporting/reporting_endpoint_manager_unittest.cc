// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_endpoint_manager.h"

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "net/base/backoff_entry.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_target_type.h"
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

  TestReportingCache(const TestReportingCache&) = delete;
  TestReportingCache& operator=(const TestReportingCache&) = delete;

  ~TestReportingCache() override = default;

  void SetEndpoint(const ReportingEndpoint& reporting_endpoint) {
    reporting_endpoints_[reporting_endpoint.group_key.network_anonymization_key]
        .push_back(reporting_endpoint);
  }

  // ReportingCache implementation:

  std::vector<ReportingEndpoint> GetCandidateEndpointsForDelivery(
      const ReportingEndpointGroupKey& group_key) override {
    // Enterprise endpoints don't have an origin.
    if (group_key.target_type == ReportingTargetType::kDeveloper) {
      EXPECT_EQ(expected_origin_, group_key.origin);
    }
    EXPECT_EQ(expected_group_, group_key.group_name);
    return reporting_endpoints_[group_key.network_anonymization_key];
  }

  // Everything below is NOTREACHED.
  void AddReport(const std::optional<base::UnguessableToken>& reporting_source,
                 const NetworkAnonymizationKey& network_anonymization_key,
                 const GURL& url,
                 const std::string& user_agent,
                 const std::string& group_name,
                 const std::string& type,
                 base::Value::Dict body,
                 int depth,
                 base::TimeTicks queued,
                 int attempts,
                 ReportingTargetType target_type) override {
    NOTREACHED_IN_MIGRATION();
  }
  void GetReports(
      std::vector<raw_ptr<const ReportingReport, VectorExperimental>>*
          reports_out) const override {
    NOTREACHED_IN_MIGRATION();
  }
  base::Value GetReportsAsValue() const override {
    NOTREACHED_IN_MIGRATION();
    return base::Value();
  }
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
  GetReportsToDeliver() override {
    NOTREACHED_IN_MIGRATION();
    return {};
  }
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
  GetReportsToDeliverForSource(
      const base::UnguessableToken& reporting_source) override {
    NOTREACHED_IN_MIGRATION();
    return {};
  }
  void ClearReportsPending(
      const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
          reports) override {
    NOTREACHED_IN_MIGRATION();
  }
  void IncrementReportsAttempts(
      const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
          reports) override {
    NOTREACHED_IN_MIGRATION();
  }
  base::flat_map<url::Origin, std::vector<ReportingEndpoint>>
  GetV1ReportingEndpointsByOrigin() const override {
    NOTREACHED_IN_MIGRATION();
    return base::flat_map<url::Origin, std::vector<ReportingEndpoint>>();
  }
  void IncrementEndpointDeliveries(const ReportingEndpointGroupKey& group_key,
                                   const GURL& url,
                                   int reports_delivered,
                                   bool successful) override {
    NOTREACHED_IN_MIGRATION();
  }
  void SetExpiredSource(
      const base::UnguessableToken& reporting_source) override {
    NOTREACHED_IN_MIGRATION();
  }
  const base::flat_set<base::UnguessableToken>& GetExpiredSources()
      const override {
    NOTREACHED_IN_MIGRATION();
    return expired_sources_;
  }
  void RemoveReports(
      const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
          reports) override {
    NOTREACHED_IN_MIGRATION();
  }
  void RemoveReports(
      const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
          reports,
      bool delivery_success) override {
    NOTREACHED_IN_MIGRATION();
  }
  void RemoveAllReports() override { NOTREACHED_IN_MIGRATION(); }
  size_t GetFullReportCountForTesting() const override {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }
  size_t GetReportCountWithStatusForTesting(
      ReportingReport::Status status) const override {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }
  bool IsReportPendingForTesting(const ReportingReport* report) const override {
    NOTREACHED_IN_MIGRATION();
    return false;
  }
  bool IsReportDoomedForTesting(const ReportingReport* report) const override {
    NOTREACHED_IN_MIGRATION();
    return false;
  }
  void OnParsedHeader(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      std::vector<ReportingEndpointGroup> parsed_header) override {
    NOTREACHED_IN_MIGRATION();
  }
  void OnParsedReportingEndpointsHeader(
      const base::UnguessableToken& reporting_source,
      const IsolationInfo& isolation_info,
      std::vector<ReportingEndpoint> endpoints) override {
    NOTREACHED_IN_MIGRATION();
  }
  void SetEnterpriseReportingEndpoints(
      const base::flat_map<std::string, GURL>& endpoints) override {
    NOTREACHED();
  }
  std::set<url::Origin> GetAllOrigins() const override {
    NOTREACHED_IN_MIGRATION();
    return std::set<url::Origin>();
  }
  void RemoveClient(const NetworkAnonymizationKey& network_anonymization_key,
                    const url::Origin& origin) override {
    NOTREACHED_IN_MIGRATION();
  }
  void RemoveClientsForOrigin(const url::Origin& origin) override {
    NOTREACHED_IN_MIGRATION();
  }
  void RemoveAllClients() override { NOTREACHED_IN_MIGRATION(); }
  void RemoveEndpointGroup(
      const ReportingEndpointGroupKey& group_key) override {
    NOTREACHED_IN_MIGRATION();
  }
  void RemoveEndpointsForUrl(const GURL& url) override {
    NOTREACHED_IN_MIGRATION();
  }
  void RemoveSourceAndEndpoints(
      const base::UnguessableToken& reporting_source) override {
    NOTREACHED_IN_MIGRATION();
  }
  void AddClientsLoadedFromStore(
      std::vector<ReportingEndpoint> loaded_endpoints,
      std::vector<CachedReportingEndpointGroup> loaded_endpoint_groups)
      override {
    NOTREACHED_IN_MIGRATION();
  }
  base::Value GetClientsAsValue() const override {
    NOTREACHED_IN_MIGRATION();
    return base::Value();
  }
  size_t GetEndpointCount() const override {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }
  void Flush() override { NOTREACHED_IN_MIGRATION(); }
  ReportingEndpoint GetV1EndpointForTesting(
      const base::UnguessableToken& reporting_source,
      const std::string& endpoint_name) const override {
    NOTREACHED_IN_MIGRATION();
    return ReportingEndpoint();
  }
  ReportingEndpoint GetEndpointForTesting(
      const ReportingEndpointGroupKey& group_key,
      const GURL& url) const override {
    NOTREACHED_IN_MIGRATION();
    return ReportingEndpoint();
  }
  std::vector<ReportingEndpoint> GetEnterpriseEndpointsForTesting()
      const override {
    NOTREACHED();
  }
  bool EndpointGroupExistsForTesting(const ReportingEndpointGroupKey& group_key,
                                     OriginSubdomains include_subdomains,
                                     base::Time expires) const override {
    NOTREACHED_IN_MIGRATION();
    return false;
  }
  bool ClientExistsForTesting(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin) const override {
    NOTREACHED_IN_MIGRATION();
    return false;
  }
  size_t GetEndpointGroupCountForTesting() const override {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }
  size_t GetClientCountForTesting() const override {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }
  size_t GetReportingSourceCountForTesting() const override {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }
  void SetEndpointForTesting(const ReportingEndpointGroupKey& group_key,
                             const GURL& url,
                             OriginSubdomains include_subdomains,
                             base::Time expires,
                             int priority,
                             int weight) override {
    NOTREACHED_IN_MIGRATION();
  }
  void SetV1EndpointForTesting(const ReportingEndpointGroupKey& group_key,
                               const base::UnguessableToken& reporting_source,
                               const IsolationInfo& isolation_info,
                               const GURL& url) override {
    NOTREACHED_IN_MIGRATION();
  }
  void SetEnterpriseEndpointForTesting(
      const ReportingEndpointGroupKey& group_key,
      const GURL& url) override {
    NOTREACHED();
  }
  IsolationInfo GetIsolationInfoForEndpoint(
      const ReportingEndpoint& endpoint) const override {
    NOTREACHED_IN_MIGRATION();
    return IsolationInfo();
  }

 private:
  const url::Origin expected_origin_;
  const std::string expected_group_;

  std::map<NetworkAnonymizationKey, std::vector<ReportingEndpoint>>
      reporting_endpoints_;
  base::flat_set<base::UnguessableToken> expired_sources_;
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
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey()) {
    ReportingEndpointGroupKey group_key(kGroupKey);
    group_key.network_anonymization_key = network_anonymization_key;
    cache_.SetEndpoint(ReportingEndpoint(
        group_key,
        ReportingEndpoint::EndpointInfo{endpoint, priority, weight}));
  }

  void SetEnterpriseEndpoint(
      const GURL& endpoint,
      int priority = ReportingEndpoint::EndpointInfo::kDefaultPriority,
      int weight = ReportingEndpoint::EndpointInfo::kDefaultWeight,
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey()) {
    ReportingEndpointGroupKey group_key(kEnterpriseGroupKey);
    group_key.network_anonymization_key = network_anonymization_key;
    cache_.SetEndpoint(ReportingEndpoint(
        group_key,
        ReportingEndpoint::EndpointInfo{endpoint, priority, weight}));
  }

  const NetworkAnonymizationKey kNak;
  const url::Origin kOrigin = url::Origin::Create(GURL("https://origin/"));
  const SchemefulSite kSite = SchemefulSite(kOrigin);
  const std::string kGroup = "group";
  const ReportingEndpointGroupKey kGroupKey =
      ReportingEndpointGroupKey(kNak,
                                kOrigin,
                                kGroup,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kEnterpriseGroupKey =
      ReportingEndpointGroupKey(kNak,
                                /*origin=*/std::nullopt,
                                kGroup,
                                ReportingTargetType::kEnterprise);
  const GURL kEndpoint = GURL("https://endpoint/");

  ReportingPolicy policy_;
  base::SimpleTestTickClock clock_;
  TestReportingDelegate delegate_;
  TestReportingCache cache_;
  std::unique_ptr<ReportingEndpointManager> endpoint_manager_;
};

TEST_F(ReportingEndpointManagerTest, NoEndpoint) {
  ReportingEndpoint endpoint =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
  EXPECT_FALSE(endpoint);
}

TEST_F(ReportingEndpointManagerTest, DeveloperEndpoint) {
  SetEndpoint(kEndpoint);

  ReportingEndpoint endpoint =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint, endpoint.info.url);
  EXPECT_EQ(ReportingTargetType::kDeveloper, endpoint.group_key.target_type);
}

TEST_F(ReportingEndpointManagerTest, EnterpriseEndpoint) {
  SetEnterpriseEndpoint(kEndpoint);

  ReportingEndpoint endpoint =
      endpoint_manager_->FindEndpointForDelivery(kEnterpriseGroupKey);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint, endpoint.info.url);
  EXPECT_EQ(ReportingTargetType::kEnterprise, endpoint.group_key.target_type);
}

TEST_F(ReportingEndpointManagerTest, BackedOffEndpoint) {
  ASSERT_EQ(2.0, policy_.endpoint_backoff_policy.multiply_factor);

  base::TimeDelta initial_delay =
      base::Milliseconds(policy_.endpoint_backoff_policy.initial_delay_ms);

  SetEndpoint(kEndpoint);

  endpoint_manager_->InformOfEndpointRequest(NetworkAnonymizationKey(),
                                             kEndpoint, false);

  // After one failure, endpoint is in exponential backoff.
  ReportingEndpoint endpoint =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
  EXPECT_FALSE(endpoint);

  // After initial delay, endpoint is usable again.
  clock_.Advance(initial_delay);

  ReportingEndpoint endpoint2 =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
  ASSERT_TRUE(endpoint2);
  EXPECT_EQ(kEndpoint, endpoint2.info.url);

  endpoint_manager_->InformOfEndpointRequest(NetworkAnonymizationKey(),
                                             kEndpoint, false);

  // After a second failure, endpoint is backed off again.
  ReportingEndpoint endpoint3 =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
  EXPECT_FALSE(endpoint3);

  clock_.Advance(initial_delay);

  // Next backoff is longer -- 2x the first -- so endpoint isn't usable yet.
  ReportingEndpoint endpoint4 =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
  EXPECT_FALSE(endpoint4);

  clock_.Advance(initial_delay);

  // After 2x the initial delay, the endpoint is usable again.
  ReportingEndpoint endpoint5 =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
  ASSERT_TRUE(endpoint5);
  EXPECT_EQ(kEndpoint, endpoint5.info.url);

  endpoint_manager_->InformOfEndpointRequest(NetworkAnonymizationKey(),
                                             kEndpoint, true);
  endpoint_manager_->InformOfEndpointRequest(NetworkAnonymizationKey(),
                                             kEndpoint, true);

  // Two more successful requests should reset the backoff to the initial delay
  // again.
  endpoint_manager_->InformOfEndpointRequest(NetworkAnonymizationKey(),
                                             kEndpoint, false);

  ReportingEndpoint endpoint6 =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
  EXPECT_FALSE(endpoint6);

  clock_.Advance(initial_delay);

  ReportingEndpoint endpoint7 =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
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
    ReportingEndpoint endpoint =
        endpoint_manager_->FindEndpointForDelivery(kGroupKey);
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

  ReportingEndpoint endpoint =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kPrimaryEndpoint, endpoint.info.url);

  // The backoff policy we set up in the constructor means that a single failed
  // upload will take the primary endpoint out of contention.  This should cause
  // us to choose the backend endpoint.
  endpoint_manager_->InformOfEndpointRequest(NetworkAnonymizationKey(),
                                             kPrimaryEndpoint, false);
  ReportingEndpoint endpoint2 =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
  ASSERT_TRUE(endpoint2);
  EXPECT_EQ(kBackupEndpoint, endpoint2.info.url);

  // Advance the current time far enough to clear out the primary endpoint's
  // backoff clock.  This should bring the primary endpoint back into play.
  clock_.Advance(base::Minutes(2));
  ReportingEndpoint endpoint3 =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
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
    ReportingEndpoint endpoint =
        endpoint_manager_->FindEndpointForDelivery(kGroupKey);
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
    ReportingEndpoint endpoint =
        endpoint_manager_->FindEndpointForDelivery(kGroupKey);
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

// Check that ReportingEndpointManager distinguishes NetworkAnonymizationKeys.
TEST_F(ReportingEndpointManagerTest, NetworkAnonymizationKey) {
  const SchemefulSite kSite2(GURL("https://origin2/"));

  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);
  const ReportingEndpointGroupKey kGroupKey1(kNetworkAnonymizationKey1, kOrigin,
                                             kGroup,
                                             ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroupKey2(kNetworkAnonymizationKey2, kOrigin,
                                             kGroup,
                                             ReportingTargetType::kDeveloper);

  // An Endpoint set for kNetworkAnonymizationKey1 should not affect
  // kNetworkAnonymizationKey2.
  SetEndpoint(kEndpoint, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              0 /* weight */, kNetworkAnonymizationKey1);
  ReportingEndpoint endpoint =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey1);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint, endpoint.info.url);
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(kGroupKey2));
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(kGroupKey));

  // Set the same Endpoint for kNetworkAnonymizationKey2, so both should be
  // reporting to the same URL.
  SetEndpoint(kEndpoint, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              0 /* weight */, kNetworkAnonymizationKey2);
  endpoint = endpoint_manager_->FindEndpointForDelivery(kGroupKey1);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint, endpoint.info.url);
  endpoint = endpoint_manager_->FindEndpointForDelivery(kGroupKey2);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint, endpoint.info.url);
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(kGroupKey));

  // An error reporting to that URL in the context of kNetworkAnonymizationKey1
  // should only affect the Endpoint retrieved in the context of
  // kNetworkAnonymizationKey1.
  endpoint_manager_->InformOfEndpointRequest(kNetworkAnonymizationKey1,
                                             kEndpoint, false);
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(kGroupKey1));
  endpoint = endpoint_manager_->FindEndpointForDelivery(kGroupKey2);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kEndpoint, endpoint.info.url);
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(kGroupKey));
}

TEST_F(ReportingEndpointManagerTest,
       NetworkAnonymizationKeyWithMultipleEndpoints) {
  const SchemefulSite kSite2(GURL("https://origin2/"));

  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);
  const ReportingEndpointGroupKey kGroupKey1(kNetworkAnonymizationKey1, kOrigin,
                                             kGroup,
                                             ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroupKey2(kNetworkAnonymizationKey2, kOrigin,
                                             kGroup,
                                             ReportingTargetType::kDeveloper);

  const GURL kEndpoint1("https://endpoint1/");
  const GURL kEndpoint2("https://endpoint2/");
  const GURL kEndpoint3("https://endpoint3/");
  const int kMaxAttempts = 20;

  // Add two Endpoints for kNetworkAnonymizationKey1, and a different one for
  // kNetworkAnonymizationKey2.
  SetEndpoint(kEndpoint1, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              ReportingEndpoint::EndpointInfo::kDefaultWeight,
              kNetworkAnonymizationKey1);
  SetEndpoint(kEndpoint2, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              ReportingEndpoint::EndpointInfo::kDefaultWeight,
              kNetworkAnonymizationKey1);
  SetEndpoint(kEndpoint3, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              ReportingEndpoint::EndpointInfo::kDefaultWeight,
              kNetworkAnonymizationKey2);

  bool endpoint1_seen = false;
  bool endpoint2_seen = false;

  // Make sure that calling FindEndpointForDelivery() with
  // kNetworkAnonymizationKey1 can return both of its endpoints, but not
  // kNetworkAnonymizationKey2's endpoint.
  for (int i = 0; i < kMaxAttempts; ++i) {
    ReportingEndpoint endpoint =
        endpoint_manager_->FindEndpointForDelivery(kGroupKey1);
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

  ReportingEndpoint endpoint =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey2);
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
  // all endpoints, and as a consistency check.
  std::set<GURL> seen_endpoints;
  for (int i = 0; i < ReportingEndpointManager::kMaxEndpointBackoffCacheSize;
       ++i) {
    ReportingEndpoint endpoint =
        endpoint_manager_->FindEndpointForDelivery(kGroupKey);
    EXPECT_TRUE(endpoint);
    EXPECT_FALSE(seen_endpoints.count(endpoint.info.url));
    seen_endpoints.insert(endpoint.info.url);
    endpoint_manager_->InformOfEndpointRequest(NetworkAnonymizationKey(),
                                               endpoint.info.url, false);
  }
  // All endpoints should now be marked as bad.
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(kGroupKey));

  // Add another endpoint with a different NetworkAnonymizationKey;
  const auto kDifferentNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);
  const ReportingEndpointGroupKey kDifferentGroupKey(
      kDifferentNetworkAnonymizationKey, kOrigin, kGroup,
      ReportingTargetType::kDeveloper);
  SetEndpoint(kEndpoint, ReportingEndpoint::EndpointInfo::kDefaultPriority,
              ReportingEndpoint::EndpointInfo::kDefaultWeight,
              kDifferentNetworkAnonymizationKey);
  // All endpoints associated with the empty NetworkAnonymizationKey should
  // still be marked as bad.
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(kGroupKey));

  // Make the endpoint added for the kDifferentNetworkAnonymizationKey as bad.
  endpoint_manager_->InformOfEndpointRequest(kDifferentNetworkAnonymizationKey,
                                             kEndpoint, false);
  // The only endpoint for kDifferentNetworkAnonymizationKey should still be
  // marked as bad.
  EXPECT_FALSE(endpoint_manager_->FindEndpointForDelivery(kDifferentGroupKey));
  // One of the endpoints for the empty NetworkAnonymizationKey should no longer
  // be marked as bad, due to eviction.
  ReportingEndpoint endpoint =
      endpoint_manager_->FindEndpointForDelivery(kGroupKey);
  EXPECT_TRUE(endpoint);

  // Reporting a success for the (only) good endpoint for the empty
  // NetworkAnonymizationKey should evict the entry for
  // kNetworkAnonymizationKey, since the most recent FindEndpointForDelivery()
  // call visited all of the empty NetworkAnonymizationKey's cached bad entries.
  endpoint_manager_->InformOfEndpointRequest(NetworkAnonymizationKey(),
                                             endpoint.info.url, true);

  EXPECT_TRUE(endpoint_manager_->FindEndpointForDelivery(kDifferentGroupKey));
}

}  // namespace
}  // namespace net
