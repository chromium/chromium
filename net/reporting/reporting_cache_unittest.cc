// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_cache.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/reporting/mock_persistent_reporting_store.h"
#include "net/reporting/reporting_cache_impl.h"
#include "net/reporting/reporting_cache_observer.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_target_type.h"
#include "net/reporting/reporting_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

using CommandType = MockPersistentReportingStore::Command::Type;

class TestReportingCacheObserver : public ReportingCacheObserver {
 public:
  TestReportingCacheObserver() = default;

  void OnReportsUpdated() override { ++cached_reports_update_count_; }
  void OnClientsUpdated() override { ++cached_clients_update_count_; }

  int cached_reports_update_count() const {
    return cached_reports_update_count_;
  }
  int cached_clients_update_count() const {
    return cached_clients_update_count_;
  }

 private:
  int cached_reports_update_count_ = 0;
  int cached_clients_update_count_ = 0;
};

// The tests are parametrized on a boolean value which represents whether or not
// to use a MockPersistentReportingStore.
class ReportingCacheTest : public ReportingTestBase,
                           public ::testing::WithParamInterface<bool> {
 protected:
  ReportingCacheTest() {
    // This is a private API of the reporting service, so no need to test the
    // case kPartitionConnectionsByNetworkIsolationKey is disabled - the
    // feature is only applied at the entry points of the service.
    feature_list_.InitAndEnableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);

    ReportingPolicy policy;
    policy.max_report_count = 5;
    policy.max_endpoints_per_origin = 3;
    policy.max_endpoint_count = 5;
    policy.max_group_staleness = base::Days(3);
    UsePolicy(policy);

    std::unique_ptr<MockPersistentReportingStore> store;
    if (GetParam()) {
      store = std::make_unique<MockPersistentReportingStore>();
    }
    store_ = store.get();
    UseStore(std::move(store));

    context()->AddCacheObserver(&observer_);
  }

  ~ReportingCacheTest() override { context()->RemoveCacheObserver(&observer_); }

  void LoadReportingClients() {
    // All ReportingCache methods assume that the store has been initialized.
    if (store()) {
      store()->LoadReportingClients(
          base::BindOnce(&ReportingCache::AddClientsLoadedFromStore,
                         base::Unretained(cache())));
      store()->FinishLoading(true);
    }
  }

  TestReportingCacheObserver* observer() { return &observer_; }

  size_t report_count() {
    std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
    cache()->GetReports(&reports);
    return reports.size();
  }

  MockPersistentReportingStore* store() { return store_.get(); }

  // Adds a new report to the cache, and returns it.
  const ReportingReport* AddAndReturnReport(
      const NetworkAnonymizationKey& network_anonymization_key,
      const GURL& url,
      const std::string& user_agent,
      const std::string& group,
      const std::string& type,
      base::Value::Dict body,
      int depth,
      base::TimeTicks queued,
      int attempts) {
    const base::Value::Dict body_clone(body.Clone());

    // The public API will only give us the (unordered) full list of reports in
    // the cache.  So we need to grab the list before we add, and the list after
    // we add, and return the one element that's different.  This is only used
    // in test cases, so I've optimized for readability over execution speed.
    std::vector<raw_ptr<const ReportingReport, VectorExperimental>> before;
    cache()->GetReports(&before);
    cache()->AddReport(std::nullopt, network_anonymization_key, url, user_agent,
                       group, type, std::move(body), depth, queued, attempts,
                       ReportingTargetType::kDeveloper);
    std::vector<raw_ptr<const ReportingReport, VectorExperimental>> after;
    cache()->GetReports(&after);

    for (const ReportingReport* report : after) {
      // If report isn't in before, we've found the new instance.
      if (!base::Contains(before, report)) {
        EXPECT_EQ(network_anonymization_key, report->network_anonymization_key);
        EXPECT_EQ(url, report->url);
        EXPECT_EQ(user_agent, report->user_agent);
        EXPECT_EQ(group, report->group);
        EXPECT_EQ(type, report->type);
        EXPECT_EQ(body_clone, report->body);
        EXPECT_EQ(depth, report->depth);
        EXPECT_EQ(queued, report->queued);
        EXPECT_EQ(attempts, report->attempts);
        return report;
      }
    }

    // This can actually happen!  If the newly created report isn't in the after
    // vector, that means that we had to evict a report, and the new report was
    // the only one eligible for eviction!
    return nullptr;
  }

  // Creates a new endpoint group by way of adding two endpoints.
  void CreateGroupAndEndpoints(const ReportingEndpointGroupKey& group) {
    EXPECT_FALSE(EndpointGroupExistsInCache(group, OriginSubdomains::DEFAULT));
    ASSERT_TRUE(SetEndpointInCache(group, kEndpoint1_, kExpires1_));
    ASSERT_TRUE(SetEndpointInCache(group, kEndpoint2_, kExpires1_));
  }

  // If |exist| is true, expect that the given group exists and has two
  // endpoints, and its client exists. If |exist| is false, expect that the
  // group and its endpoints don't exist (does not check the client in that
  // case).
  void ExpectExistence(const ReportingEndpointGroupKey& group, bool exist) {
    ReportingEndpoint endpoint1 = FindEndpointInCache(group, kEndpoint1_);
    ReportingEndpoint endpoint2 = FindEndpointInCache(group, kEndpoint2_);
    EXPECT_EQ(exist, endpoint1.is_valid());
    EXPECT_EQ(exist, endpoint2.is_valid());
    if (exist) {
      EXPECT_EQ(endpoint1.group_key, group);
      EXPECT_EQ(endpoint2.group_key, group);
      EXPECT_TRUE(cache()->ClientExistsForTesting(
          group.network_anonymization_key, group.origin.value()));
    }
    EXPECT_EQ(exist,
              EndpointGroupExistsInCache(group, OriginSubdomains::DEFAULT));
  }

  base::test::ScopedFeatureList feature_list_;

  const GURL kUrl1_ = GURL("https://origin1/path");
  const GURL kUrl2_ = GURL("https://origin2/path");
  const url::Origin kOrigin1_ = url::Origin::Create(GURL("https://origin1/"));
  const url::Origin kOrigin2_ = url::Origin::Create(GURL("https://origin2/"));
  const std::optional<base::UnguessableToken> kReportingSource_ =
      base::UnguessableToken::Create();
  const NetworkAnonymizationKey kNak_;
  const NetworkAnonymizationKey kOtherNak_ =
      NetworkAnonymizationKey::CreateCrossSite(SchemefulSite(kOrigin1_));
  const IsolationInfo kIsolationInfo1_ =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther,
                            kOrigin1_,
                            kOrigin1_,
                            SiteForCookies::FromOrigin(kOrigin1_));
  const IsolationInfo kIsolationInfo2_ =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther,
                            kOrigin2_,
                            kOrigin2_,
                            SiteForCookies::FromOrigin(kOrigin2_));
  const GURL kEndpoint1_ = GURL("https://endpoint1/");
  const GURL kEndpoint2_ = GURL("https://endpoint2/");
  const GURL kEndpoint3_ = GURL("https://endpoint3/");
  const GURL kEndpoint4_ = GURL("https://endpoint4/");
  const std::string kUserAgent_ = "Mozilla/1.0";
  const std::string kGroup1_ = "group1";
  const std::string kGroup2_ = "group2";
  const std::string kType_ = "default";
  const base::TimeTicks kNowTicks_ = tick_clock()->NowTicks();
  const base::Time kNow_ = clock()->Now();
  const base::Time kExpires1_ = kNow_ + base::Days(7);
  const base::Time kExpires2_ = kExpires1_ + base::Days(7);
  // There are 2^3 = 8 of these to test the different combinations of matching
  // vs mismatching NAK, origin, and group.
  const ReportingEndpointGroupKey kGroupKey11_ =
      ReportingEndpointGroupKey(kNak_,
                                kOrigin1_,
                                kGroup1_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroupKey21_ =
      ReportingEndpointGroupKey(kNak_,
                                kOrigin2_,
                                kGroup1_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroupKey12_ =
      ReportingEndpointGroupKey(kNak_,
                                kOrigin1_,
                                kGroup2_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroupKey22_ =
      ReportingEndpointGroupKey(kNak_,
                                kOrigin2_,
                                kGroup2_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kOtherGroupKey11_ =
      ReportingEndpointGroupKey(kOtherNak_,
                                kOrigin1_,
                                kGroup1_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kOtherGroupKey21_ =
      ReportingEndpointGroupKey(kOtherNak_,
                                kOrigin2_,
                                kGroup1_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kOtherGroupKey12_ =
      ReportingEndpointGroupKey(kOtherNak_,
                                kOrigin1_,
                                kGroup2_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kOtherGroupKey22_ =
      ReportingEndpointGroupKey(kOtherNak_,
                                kOrigin2_,
                                kGroup2_,
                                ReportingTargetType::kDeveloper);

  TestReportingCacheObserver observer_;
  raw_ptr<MockPersistentReportingStore> store_;
};

// Note: These tests exercise both sides of the cache (reports and clients),
// aside from header parsing (i.e. OnParsedHeader(), AddOrUpdate*(),
// Remove*OtherThan() methods) which are exercised in the unittests for the
// header parser.

TEST_P(ReportingCacheTest, Reports) {
  LoadReportingClients();

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());

  cache()->AddReport(kReportingSource_, kNak_, kUrl1_, kUserAgent_, kGroup1_,
                     kType_, base::Value::Dict(), 0, kNowTicks_, 0,
                     ReportingTargetType::kEnterprise);
  EXPECT_EQ(1, observer()->cached_reports_update_count());

  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  const ReportingReport* report = reports[0];
  ASSERT_TRUE(report);
  EXPECT_EQ(kNak_, report->network_anonymization_key);
  EXPECT_EQ(kUrl1_, report->url);
  EXPECT_EQ(kUserAgent_, report->user_agent);
  EXPECT_EQ(kGroup1_, report->group);
  EXPECT_EQ(kType_, report->type);
  EXPECT_EQ(ReportingTargetType::kEnterprise, report->target_type);
  // TODO(juliatuttle): Check body?
  EXPECT_EQ(kNowTicks_, report->queued);
  EXPECT_EQ(0, report->attempts);
  EXPECT_FALSE(cache()->IsReportPendingForTesting(report));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(report));

  cache()->IncrementReportsAttempts(reports);
  EXPECT_EQ(2, observer()->cached_reports_update_count());

  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  report = reports[0];
  ASSERT_TRUE(report);
  EXPECT_EQ(1, report->attempts);

  cache()->RemoveReports(reports);
  EXPECT_EQ(3, observer()->cached_reports_update_count());

  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_P(ReportingCacheTest, RemoveAllReports) {
  LoadReportingClients();

  cache()->AddReport(kReportingSource_, kNak_, kUrl1_, kUserAgent_, kGroup1_,
                     kType_, base::Value::Dict(), 0, kNowTicks_, 0,
                     ReportingTargetType::kDeveloper);
  cache()->AddReport(kReportingSource_, kNak_, kUrl1_, kUserAgent_, kGroup1_,
                     kType_, base::Value::Dict(), 0, kNowTicks_, 0,
                     ReportingTargetType::kDeveloper);
  EXPECT_EQ(2, observer()->cached_reports_update_count());

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(2u, reports.size());

  cache()->RemoveAllReports();
  EXPECT_EQ(3, observer()->cached_reports_update_count());

  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_P(ReportingCacheTest, RemovePendingReports) {
  LoadReportingClients();

  cache()->AddReport(kReportingSource_, kNak_, kUrl1_, kUserAgent_, kGroup1_,
                     kType_, base::Value::Dict(), 0, kNowTicks_, 0,
                     ReportingTargetType::kDeveloper);
  EXPECT_EQ(1, observer()->cached_reports_update_count());

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_FALSE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  EXPECT_EQ(reports, cache()->GetReportsToDeliver());
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  // After getting reports to deliver, everything in the cache should be
  // pending, so another call to GetReportsToDeliver should return nothing.
  EXPECT_EQ(0u, cache()->GetReportsToDeliver().size());

  cache()->RemoveReports(reports);
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_TRUE(cache()->IsReportDoomedForTesting(reports[0]));
  EXPECT_EQ(2, observer()->cached_reports_update_count());

  // After removing report, future calls to GetReports should not return it.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
      visible_reports;
  cache()->GetReports(&visible_reports);
  EXPECT_TRUE(visible_reports.empty());
  EXPECT_EQ(1u, cache()->GetFullReportCountForTesting());

  // After clearing pending flag, report should be deleted.
  cache()->ClearReportsPending(reports);
  EXPECT_EQ(0u, cache()->GetFullReportCountForTesting());
}

TEST_P(ReportingCacheTest, RemoveAllPendingReports) {
  LoadReportingClients();

  cache()->AddReport(kReportingSource_, kNak_, kUrl1_, kUserAgent_, kGroup1_,
                     kType_, base::Value::Dict(), 0, kNowTicks_, 0,
                     ReportingTargetType::kDeveloper);
  EXPECT_EQ(1, observer()->cached_reports_update_count());

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_FALSE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  EXPECT_EQ(reports, cache()->GetReportsToDeliver());
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  // After getting reports to deliver, everything in the cache should be
  // pending, so another call to GetReportsToDeliver should return nothing.
  EXPECT_EQ(0u, cache()->GetReportsToDeliver().size());

  cache()->RemoveAllReports();
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_TRUE(cache()->IsReportDoomedForTesting(reports[0]));
  EXPECT_EQ(2, observer()->cached_reports_update_count());

  // After removing report, future calls to GetReports should not return it.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
      visible_reports;
  cache()->GetReports(&visible_reports);
  EXPECT_TRUE(visible_reports.empty());
  EXPECT_EQ(1u, cache()->GetFullReportCountForTesting());

  // After clearing pending flag, report should be deleted.
  cache()->ClearReportsPending(reports);
  EXPECT_EQ(0u, cache()->GetFullReportCountForTesting());
}

TEST_P(ReportingCacheTest, GetReportsAsValue) {
  LoadReportingClients();

  // We need a reproducible expiry timestamp for this test case.
  const base::TimeTicks now = base::TimeTicks();
  const ReportingReport* report1 =
      AddAndReturnReport(kNak_, kUrl1_, kUserAgent_, kGroup1_, kType_,
                         base::Value::Dict(), 0, now + base::Seconds(200), 0);
  const ReportingReport* report2 =
      AddAndReturnReport(kOtherNak_, kUrl1_, kUserAgent_, kGroup2_, kType_,
                         base::Value::Dict(), 0, now + base::Seconds(100), 1);
  // Mark report1 and report2 as pending.
  EXPECT_THAT(cache()->GetReportsToDeliver(),
              ::testing::UnorderedElementsAre(report1, report2));
  // Mark report2 as doomed.
  cache()->RemoveReports({report2});

  base::Value actual = cache()->GetReportsAsValue();
  base::Value expected = base::test::ParseJson(base::StringPrintf(
      R"json(
      [
        {
          "url": "https://origin1/path",
          "group": "group2",
          "network_anonymization_key": "%s",
          "type": "default",
          "status": "doomed",
          "body": {},
          "attempts": 1,
          "depth": 0,
          "queued": "100000",
        },
        {
          "url": "https://origin1/path",
          "group": "group1",
          "network_anonymization_key": "%s",
          "type": "default",
          "status": "pending",
          "body": {},
          "attempts": 0,
          "depth": 0,
          "queued": "200000",
        },
      ]
      )json",
      kOtherNak_.ToDebugString().c_str(), kNak_.ToDebugString().c_str()));
  EXPECT_EQ(expected, actual);

  // Add two new reports that will show up as "queued".
  const ReportingReport* report3 =
      AddAndReturnReport(kNak_, kUrl2_, kUserAgent_, kGroup1_, kType_,
                         base::Value::Dict(), 2, now + base::Seconds(200), 0);
  const ReportingReport* report4 =
      AddAndReturnReport(kOtherNak_, kUrl1_, kUserAgent_, kGroup1_, kType_,
                         base::Value::Dict(), 0, now + base::Seconds(300), 0);
  actual = cache()->GetReportsAsValue();
  expected = base::test::ParseJson(base::StringPrintf(
      R"json(
      [
        {
          "url": "https://origin1/path",
          "group": "group2",
          "network_anonymization_key": "%s",
          "type": "default",
          "status": "doomed",
          "body": {},
          "attempts": 1,
          "depth": 0,
          "queued": "100000",
        },
        {
          "url": "https://origin1/path",
          "group": "group1",
          "network_anonymization_key": "%s",
          "type": "default",
          "status": "pending",
          "body": {},
          "attempts": 0,
          "depth": 0,
          "queued": "200000",
        },
        {
          "url": "https://origin2/path",
          "group": "group1",
          "network_anonymization_key": "%s",
          "type": "default",
          "status": "queued",
          "body": {},
          "attempts": 0,
          "depth": 2,
          "queued": "200000",
        },
        {
          "url": "https://origin1/path",
          "group": "group1",
          "network_anonymization_key": "%s",
          "type": "default",
          "status": "queued",
          "body": {},
          "attempts": 0,
          "depth": 0,
          "queued": "300000",
        },
      ]
      )json",
      kOtherNak_.ToDebugString().c_str(), kNak_.ToDebugString().c_str(),
      kNak_.ToDebugString().c_str(), kOtherNak_.ToDebugString().c_str()));
  EXPECT_EQ(expected, actual);

  // GetReportsToDeliver only returns the non-pending reports.
  EXPECT_THAT(cache()->GetReportsToDeliver(),
              ::testing::UnorderedElementsAre(report3, report4));
}

TEST_P(ReportingCacheTest, GetReportsToDeliverForSource) {
  LoadReportingClients();

  auto source1 = base::UnguessableToken::Create();
  auto source2 = base::UnguessableToken::Create();

  // Queue a V1 report for each of these sources, and a V0 report (with a null
  // source) for the same URL.
  cache()->AddReport(source1, kNak_, kUrl1_, kUserAgent_, kGroup1_, kType_,
                     base::Value::Dict(), 0, kNowTicks_, 0,
                     ReportingTargetType::kDeveloper);
  cache()->AddReport(source2, kNak_, kUrl1_, kUserAgent_, kGroup1_, kType_,
                     base::Value::Dict(), 0, kNowTicks_, 0,
                     ReportingTargetType::kDeveloper);
  cache()->AddReport(std::nullopt, kNak_, kUrl1_, kUserAgent_, kGroup1_, kType_,
                     base::Value::Dict(), 0, kNowTicks_, 0,
                     ReportingTargetType::kDeveloper);
  EXPECT_EQ(3, observer()->cached_reports_update_count());

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(3u, reports.size());

  const auto report1 =
      base::ranges::find(reports, source1, &ReportingReport::reporting_source);
  CHECK(report1 != reports.end());
  const auto report2 =
      base::ranges::find(reports, source2, &ReportingReport::reporting_source);
  CHECK(report2 != reports.end());
  const auto report3 = base::ranges::find(reports, std::nullopt,
                                          &ReportingReport::reporting_source);
  CHECK(report3 != reports.end());

  // Get the reports for Source 1 and check the status of all reports.
  EXPECT_EQ((std::vector<raw_ptr<const ReportingReport, VectorExperimental>>{
                *report1}),
            cache()->GetReportsToDeliverForSource(source1));
  EXPECT_TRUE(cache()->IsReportPendingForTesting(*report1));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(*report1));
  EXPECT_FALSE(cache()->IsReportPendingForTesting(*report2));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(*report2));
  EXPECT_FALSE(cache()->IsReportPendingForTesting(*report3));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(*report3));

  // There should be one pending and two cached reports at this point.
  EXPECT_EQ(1u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::PENDING));
  EXPECT_EQ(2u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));

  // Calling the method again should not retrieve any more reports, and should
  // not change the status of any other reports in the cache.
  EXPECT_EQ(0u, cache()->GetReportsToDeliverForSource(source1).size());
  EXPECT_EQ(1u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::PENDING));
  EXPECT_EQ(2u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));

  // Get the reports for Source 2 and check the status again.
  EXPECT_EQ((std::vector<raw_ptr<const ReportingReport, VectorExperimental>>{
                *report2}),
            cache()->GetReportsToDeliverForSource(source2));
  EXPECT_TRUE(cache()->IsReportPendingForTesting(*report1));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(*report1));
  EXPECT_TRUE(cache()->IsReportPendingForTesting(*report2));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(*report2));
  EXPECT_FALSE(cache()->IsReportPendingForTesting(*report3));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(*report3));

  EXPECT_EQ(2u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::PENDING));
  EXPECT_EQ(1u, cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));
}

TEST_P(ReportingCacheTest, Endpoints) {
  LoadReportingClients();

  EXPECT_EQ(0u, cache()->GetEndpointCount());
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint1_, kExpires1_));
  EXPECT_EQ(1u, cache()->GetEndpointCount());

  const ReportingEndpoint endpoint1 =
      FindEndpointInCache(kGroupKey11_, kEndpoint1_);
  ASSERT_TRUE(endpoint1);
  EXPECT_EQ(kOrigin1_, endpoint1.group_key.origin);
  EXPECT_EQ(kEndpoint1_, endpoint1.info.url);
  EXPECT_EQ(kGroup1_, endpoint1.group_key.group_name);

  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey11_, OriginSubdomains::DEFAULT, kExpires1_));

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));

  // Insert another endpoint in the same group.
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint2_, kExpires1_));
  EXPECT_EQ(2u, cache()->GetEndpointCount());

  const ReportingEndpoint endpoint2 =
      FindEndpointInCache(kGroupKey11_, kEndpoint2_);
  ASSERT_TRUE(endpoint2);
  EXPECT_EQ(kOrigin1_, endpoint2.group_key.origin);
  EXPECT_EQ(kEndpoint2_, endpoint2.info.url);
  EXPECT_EQ(kGroup1_, endpoint2.group_key.group_name);

  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey11_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  std::set<url::Origin> origins_in_cache = cache()->GetAllOrigins();
  EXPECT_EQ(1u, origins_in_cache.size());

  // Insert another endpoint for a different origin with same group name.
  ASSERT_TRUE(SetEndpointInCache(kGroupKey21_, kEndpoint2_, kExpires1_));
  EXPECT_EQ(3u, cache()->GetEndpointCount());

  const ReportingEndpoint endpoint3 =
      FindEndpointInCache(kGroupKey21_, kEndpoint2_);
  ASSERT_TRUE(endpoint3);
  EXPECT_EQ(kOrigin2_, endpoint3.group_key.origin);
  EXPECT_EQ(kEndpoint2_, endpoint3.info.url);
  EXPECT_EQ(kGroup1_, endpoint3.group_key.group_name);

  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey21_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_EQ(2u, cache()->GetEndpointGroupCountForTesting());

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin2_));
  origins_in_cache = cache()->GetAllOrigins();
  EXPECT_EQ(2u, origins_in_cache.size());
}

TEST_P(ReportingCacheTest, SetEnterpriseReportingEndpointsWithFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kReportingApiEnableEnterpriseCookieIssues);
  EXPECT_EQ(0u, cache()->GetEnterpriseEndpointsForTesting().size());
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };

  std::vector<ReportingEndpoint> expected_enterprise_endpoints = {
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-1",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://example.com/reports")}},
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-2",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://reporting.example/cookie-issues")}},
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-3",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://report-collector.example")}}};

  cache()->SetEnterpriseReportingEndpoints(test_enterprise_endpoints);
  EXPECT_EQ(expected_enterprise_endpoints,
            cache()->GetEnterpriseEndpointsForTesting());
}

TEST_P(ReportingCacheTest, SetEnterpriseReportingEndpointsWithFeatureDisabled) {
  EXPECT_EQ(0u, cache()->GetEnterpriseEndpointsForTesting().size());
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };

  std::vector<ReportingEndpoint> expected_enterprise_endpoints = {
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-1",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://example.com/reports")}},
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-2",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://reporting.example/cookie-issues")}},
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-3",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://report-collector.example")}}};

  cache()->SetEnterpriseReportingEndpoints(test_enterprise_endpoints);
  EXPECT_EQ(0u, cache()->GetEnterpriseEndpointsForTesting().size());
}

TEST_P(ReportingCacheTest, ReportingCacheImplConstructionWithFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kReportingApiEnableEnterpriseCookieIssues);
  EXPECT_EQ(0u, cache()->GetEnterpriseEndpointsForTesting().size());
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };
  std::unique_ptr<ReportingCache> reporting_cache_impl =
      ReportingCache::Create(context(), test_enterprise_endpoints);

  std::vector<ReportingEndpoint> expected_enterprise_endpoints = {
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-1",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://example.com/reports")}},
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-2",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://reporting.example/cookie-issues")}},
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-3",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://report-collector.example")}}};

  EXPECT_EQ(expected_enterprise_endpoints,
            reporting_cache_impl->GetEnterpriseEndpointsForTesting());
}

TEST_P(ReportingCacheTest, ReportingCacheImplConstructionWithFeatureDisabled) {
  EXPECT_EQ(0u, cache()->GetEnterpriseEndpointsForTesting().size());
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };
  std::unique_ptr<ReportingCache> reporting_cache_impl =
      ReportingCache::Create(context(), test_enterprise_endpoints);

  std::vector<ReportingEndpoint> expected_enterprise_endpoints = {
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-1",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://example.com/reports")}},
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-2",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://reporting.example/cookie-issues")}},
      {ReportingEndpointGroupKey(
           NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
           /*origin=*/std::nullopt, "endpoint-3",
           ReportingTargetType::kEnterprise),
       {.url = GURL("https://report-collector.example")}}};

  EXPECT_EQ(0u,
            reporting_cache_impl->GetEnterpriseEndpointsForTesting().size());
}

TEST_P(ReportingCacheTest, ClientsKeyedByEndpointGroupKey) {
  // Raise the endpoint limits for this test.
  // (This needs to first remove the cache observer because this destroys the
  // old ReportingContext, which must not have any observers upon destruction.)
  context()->RemoveCacheObserver(&observer_);
  ReportingPolicy policy;
  policy.max_endpoints_per_origin = 5;  // This test should use 4.
  policy.max_endpoint_count = 20;       // This test should use 16.
  UsePolicy(policy);
  context()->AddCacheObserver(&observer_);

  LoadReportingClients();

  const ReportingEndpointGroupKey kGroupKeys[] = {
      kGroupKey11_,      kGroupKey12_,      kGroupKey21_,
      kGroupKey22_,      kOtherGroupKey11_, kOtherGroupKey12_,
      kOtherGroupKey21_, kOtherGroupKey22_,
  };

  size_t endpoint_group_count = 0u;
  size_t endpoint_count = 0u;

  // Check that the group keys are all considered distinct, and nothing is
  // overwritten.
  for (const auto& group : kGroupKeys) {
    CreateGroupAndEndpoints(group);
    ExpectExistence(group, true);
    ++endpoint_group_count;
    EXPECT_EQ(endpoint_group_count, cache()->GetEndpointGroupCountForTesting());
    endpoint_count += 2u;
    EXPECT_EQ(endpoint_count, cache()->GetEndpointCount());
  }

  // Check that everything is there at the end.
  for (const auto& group : kGroupKeys) {
    ExpectExistence(group, true);
  }

  size_t client_count = 4u;
  EXPECT_EQ(client_count, cache()->GetClientCountForTesting());

  // Test that Clients with different NAKs are considered different, and test
  // RemoveEndpointGroup() and RemoveClient().
  const std::pair<NetworkAnonymizationKey, url::Origin> kNakOriginPairs[] = {
      {kNak_, kOrigin1_},
      {kNak_, kOrigin2_},
      {kOtherNak_, kOrigin1_},
      {kOtherNak_, kOrigin2_},
  };

  // SetEndpointInCache doesn't update store counts, which is why we start from
  // zero and they go negative.
  // TODO(crbug.com/40598339): Populate the cache via the store so we don't
  // need negative counts.
  MockPersistentReportingStore::CommandList expected_commands;
  int stored_group_count = 0;
  int stored_endpoint_count = 0;
  int store_remove_group_count = 0;
  int store_remove_endpoint_count = 0;

  for (const auto& pair : kNakOriginPairs) {
    EXPECT_TRUE(cache()->ClientExistsForTesting(pair.first, pair.second));
    ReportingEndpointGroupKey group1(pair.first, pair.second, kGroup1_,
                                     ReportingTargetType::kDeveloper);
    ReportingEndpointGroupKey group2(pair.first, pair.second, kGroup2_,
                                     ReportingTargetType::kDeveloper);
    ExpectExistence(group1, true);
    ExpectExistence(group2, true);

    cache()->RemoveEndpointGroup(group1);
    ExpectExistence(group1, false);
    ExpectExistence(group2, true);
    EXPECT_TRUE(cache()->ClientExistsForTesting(pair.first, pair.second));

    cache()->RemoveClient(pair.first, pair.second);
    ExpectExistence(group1, false);
    ExpectExistence(group2, false);
    EXPECT_FALSE(cache()->ClientExistsForTesting(pair.first, pair.second));

    --client_count;
    EXPECT_EQ(client_count, cache()->GetClientCountForTesting());
    endpoint_group_count -= 2u;
    stored_group_count -= 2;
    EXPECT_EQ(endpoint_group_count, cache()->GetEndpointGroupCountForTesting());
    endpoint_count -= 4u;
    stored_endpoint_count -= 4;
    EXPECT_EQ(endpoint_count, cache()->GetEndpointCount());

    if (store()) {
      store()->Flush();
      EXPECT_EQ(stored_endpoint_count, store()->StoredEndpointsCount());
      EXPECT_EQ(stored_group_count, store()->StoredEndpointGroupsCount());
      store_remove_group_count += 2u;
      expected_commands.emplace_back(
          CommandType::DELETE_REPORTING_ENDPOINT_GROUP, group1);
      expected_commands.emplace_back(
          CommandType::DELETE_REPORTING_ENDPOINT_GROUP, group2);
      store_remove_endpoint_count += 4u;
      expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                     group1, kEndpoint1_);
      expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                     group1, kEndpoint2_);
      expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                     group2, kEndpoint1_);
      expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                     group2, kEndpoint2_);
      EXPECT_EQ(
          store_remove_group_count,
          store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
      EXPECT_EQ(store_remove_endpoint_count,
                store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
      EXPECT_THAT(store()->GetAllCommands(),
                  testing::IsSupersetOf(expected_commands));
    }
  }
}

TEST_P(ReportingCacheTest, RemoveClientsForOrigin) {
  LoadReportingClients();

  // Origin 1
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOtherGroupKey11_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOtherGroupKey12_, kEndpoint1_, kExpires1_));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  // Origin 2
  ASSERT_TRUE(SetEndpointInCache(kGroupKey21_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOtherGroupKey22_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(ClientExistsInCacheForOrigin(kOrigin2_));

  EXPECT_EQ(5u, cache()->GetEndpointCount());

  cache()->RemoveClientsForOrigin(kOrigin1_);

  EXPECT_EQ(2u, cache()->GetEndpointCount());
  EXPECT_FALSE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin2_));

  if (store()) {
    store()->Flush();
    // SetEndpointInCache doesn't update store counts, which is why they go
    // negative here.
    // TODO(crbug.com/40598339): Populate the cache via the store so we don't
    // need negative counts.
    EXPECT_EQ(-3, store()->StoredEndpointsCount());
    EXPECT_EQ(-3, store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    EXPECT_EQ(3,
              store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(3, store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kOtherGroupKey11_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kOtherGroupKey12_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kOtherGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kOtherGroupKey12_, kEndpoint1_);
    EXPECT_THAT(store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingCacheTest, RemoveAllClients) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey21_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey22_, kEndpoint2_, kExpires1_));
  EXPECT_EQ(4u, cache()->GetEndpointCount());
  ASSERT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  ASSERT_TRUE(ClientExistsInCacheForOrigin(kOrigin2_));

  cache()->RemoveAllClients();

  EXPECT_EQ(0u, cache()->GetEndpointCount());
  EXPECT_FALSE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_FALSE(ClientExistsInCacheForOrigin(kOrigin2_));

  if (store()) {
    store()->Flush();
    // SetEndpointInCache doesn't update store counts, which is why they go
    // negative here.
    // TODO(crbug.com/40598339): Populate the cache via the store so we don't
    // need negative counts.
    EXPECT_EQ(-4, store()->StoredEndpointsCount());
    EXPECT_EQ(-3, store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    EXPECT_EQ(4,
              store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(3, store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey21_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey22_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey21_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey22_);
    EXPECT_THAT(store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingCacheTest, RemoveEndpointGroup) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey21_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey22_, kEndpoint2_, kExpires1_));
  EXPECT_EQ(4u, cache()->GetEndpointCount());
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin2_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey11_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey21_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey22_, OriginSubdomains::DEFAULT, kExpires1_));

  cache()->RemoveEndpointGroup(kGroupKey21_);
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey11_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_FALSE(EndpointGroupExistsInCache(
      kGroupKey21_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey22_, OriginSubdomains::DEFAULT, kExpires1_));

  cache()->RemoveEndpointGroup(kGroupKey22_);
  EXPECT_FALSE(EndpointGroupExistsInCache(
      kGroupKey22_, OriginSubdomains::DEFAULT, kExpires1_));
  // Removal of the last group for an origin also removes the client.
  EXPECT_FALSE(ClientExistsInCacheForOrigin(kOrigin2_));
  // Other origins are not affected.
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey11_, OriginSubdomains::DEFAULT, kExpires1_));

  if (store()) {
    store()->Flush();
    // SetEndpointInCache doesn't update store counts, which is why they go
    // negative here.
    // TODO(crbug.com/40598339): Populate the cache via the store so we don't
    // need negative counts.
    EXPECT_EQ(-2, store()->StoredEndpointsCount());
    EXPECT_EQ(-2, store()->StoredEndpointGroupsCount());
    EXPECT_EQ(2,
              store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(2, store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey21_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey22_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey21_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey22_);
    EXPECT_THAT(store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingCacheTest, RemoveEndpointsForUrl) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey21_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey22_, kEndpoint2_, kExpires1_));
  EXPECT_EQ(4u, cache()->GetEndpointCount());
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin2_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey11_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey21_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey22_, OriginSubdomains::DEFAULT, kExpires1_));

  cache()->RemoveEndpointsForUrl(kEndpoint1_);
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin2_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey11_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_FALSE(EndpointGroupExistsInCache(
      kGroupKey21_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey22_, OriginSubdomains::DEFAULT, kExpires1_));

  EXPECT_EQ(2u, cache()->GetEndpointCount());
  EXPECT_FALSE(FindEndpointInCache(kGroupKey11_, kEndpoint1_));
  EXPECT_TRUE(FindEndpointInCache(kGroupKey11_, kEndpoint2_));
  EXPECT_FALSE(FindEndpointInCache(kGroupKey21_, kEndpoint1_));
  EXPECT_TRUE(FindEndpointInCache(kGroupKey22_, kEndpoint2_));

  if (store()) {
    store()->Flush();
    // SetEndpointInCache doesn't update store counts, which is why they go
    // negative here.
    // TODO(crbug.com/40598339): Populate the cache via the store so we don't
    // need negative counts.
    EXPECT_EQ(-2, store()->StoredEndpointsCount());
    EXPECT_EQ(-1, store()->StoredEndpointGroupsCount());
    EXPECT_EQ(2,
              store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(1, store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey21_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey21_);
    EXPECT_THAT(store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingCacheTest, RemoveSourceAndEndpoints) {
  const base::UnguessableToken reporting_source_2 =
      base::UnguessableToken::Create();
  LoadReportingClients();

  NetworkAnonymizationKey network_anonymization_key_1 =
      kIsolationInfo1_.network_anonymization_key();
  NetworkAnonymizationKey network_anonymization_key_2 =
      kIsolationInfo2_.network_anonymization_key();

  cache()->SetV1EndpointForTesting(
      ReportingEndpointGroupKey(network_anonymization_key_1, *kReportingSource_,
                                kOrigin1_, kGroup1_,
                                ReportingTargetType::kDeveloper),
      *kReportingSource_, kIsolationInfo1_, kUrl1_);
  cache()->SetV1EndpointForTesting(
      ReportingEndpointGroupKey(network_anonymization_key_1, *kReportingSource_,
                                kOrigin1_, kGroup2_,
                                ReportingTargetType::kDeveloper),
      *kReportingSource_, kIsolationInfo1_, kUrl2_);
  cache()->SetV1EndpointForTesting(
      ReportingEndpointGroupKey(network_anonymization_key_2, reporting_source_2,
                                kOrigin2_, kGroup1_,
                                ReportingTargetType::kDeveloper),
      reporting_source_2, kIsolationInfo2_, kUrl2_);

  EXPECT_EQ(2u, cache()->GetReportingSourceCountForTesting());
  EXPECT_TRUE(cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup1_));
  EXPECT_TRUE(cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup2_));
  EXPECT_TRUE(cache()->GetV1EndpointForTesting(reporting_source_2, kGroup1_));
  EXPECT_FALSE(cache()->GetExpiredSources().contains(*kReportingSource_));
  EXPECT_FALSE(cache()->GetExpiredSources().contains(reporting_source_2));

  cache()->SetExpiredSource(*kReportingSource_);

  EXPECT_EQ(2u, cache()->GetReportingSourceCountForTesting());
  EXPECT_TRUE(cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup1_));
  EXPECT_TRUE(cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup2_));
  EXPECT_TRUE(cache()->GetV1EndpointForTesting(reporting_source_2, kGroup1_));
  EXPECT_TRUE(cache()->GetExpiredSources().contains(*kReportingSource_));
  EXPECT_FALSE(cache()->GetExpiredSources().contains(reporting_source_2));

  cache()->RemoveSourceAndEndpoints(*kReportingSource_);

  EXPECT_EQ(1u, cache()->GetReportingSourceCountForTesting());
  EXPECT_FALSE(cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup1_));
  EXPECT_FALSE(cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup2_));
  EXPECT_TRUE(cache()->GetV1EndpointForTesting(reporting_source_2, kGroup1_));
  EXPECT_FALSE(cache()->GetExpiredSources().contains(*kReportingSource_));
  EXPECT_FALSE(cache()->GetExpiredSources().contains(reporting_source_2));
}

TEST_P(ReportingCacheTest, GetClientsAsValue) {
  LoadReportingClients();

  // These times are bogus but we need a reproducible expiry timestamp for this
  // test case.
  const base::TimeTicks expires_ticks = base::TimeTicks() + base::Days(7);
  const base::Time expires =
      base::Time::UnixEpoch() + (expires_ticks - base::TimeTicks::UnixEpoch());
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint1_, expires,
                                 OriginSubdomains::EXCLUDE));
  ASSERT_TRUE(SetEndpointInCache(kOtherGroupKey21_, kEndpoint2_, expires,
                                 OriginSubdomains::INCLUDE));

  cache()->IncrementEndpointDeliveries(kGroupKey11_, kEndpoint1_,
                                       /* reports */ 2, /* succeeded */ true);
  cache()->IncrementEndpointDeliveries(kOtherGroupKey21_, kEndpoint2_,
                                       /* reports */ 1, /* succeeded */ false);

  base::Value actual = cache()->GetClientsAsValue();
  base::Value expected = base::test::ParseJson(base::StringPrintf(
      R"json(
      [
        {
          "network_anonymization_key": "%s",
          "origin": "https://origin1",
          "groups": [
            {
              "name": "group1",
              "expires": "604800000",
              "includeSubdomains": false,
              "endpoints": [
                {"url": "https://endpoint1/", "priority": 1, "weight": 1,
                 "successful": {"uploads": 1, "reports": 2},
                 "failed": {"uploads": 0, "reports": 0}},
              ],
            },
          ],
        },
        {
          "network_anonymization_key": "%s",
          "origin": "https://origin2",
          "groups": [
            {
              "name": "group1",
              "expires": "604800000",
              "includeSubdomains": true,
              "endpoints": [
                {"url": "https://endpoint2/", "priority": 1, "weight": 1,
                 "successful": {"uploads": 0, "reports": 0},
                 "failed": {"uploads": 1, "reports": 1}},
              ],
            },
          ],
        },
      ]
      )json",
      kNak_.ToDebugString().c_str(), kOtherNak_.ToDebugString().c_str()));

  // Compare disregarding order.
  base::Value::List& expected_list = expected.GetList();
  base::Value::List& actual_list = actual.GetList();
  std::sort(expected_list.begin(), expected_list.end());
  std::sort(actual_list.begin(), actual_list.end());
  EXPECT_EQ(expected, actual);
}

TEST_P(ReportingCacheTest, GetCandidateEndpointsForDelivery) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey21_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey22_, kEndpoint2_, kExpires1_));
  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(kGroupKey11_);
  ASSERT_EQ(2u, candidate_endpoints.size());
  EXPECT_EQ(kGroupKey11_, candidate_endpoints[0].group_key);
  EXPECT_EQ(kGroupKey11_, candidate_endpoints[1].group_key);

  candidate_endpoints = cache()->GetCandidateEndpointsForDelivery(kGroupKey21_);
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kGroupKey21_, candidate_endpoints[0].group_key);
}

TEST_P(ReportingCacheTest, GetCandidateEnterpriseEndpointsForDelivery) {
  const ReportingEndpointGroupKey kEnterpriseGroupKey_ =
      ReportingEndpointGroupKey(kIsolationInfo1_.network_anonymization_key(),
                                *kReportingSource_, /*origin=*/std::nullopt,
                                kGroup1_, ReportingTargetType::kEnterprise);

  cache()->SetEnterpriseEndpointForTesting(kEnterpriseGroupKey_, kUrl1_);
  cache()->SetEnterpriseEndpointForTesting(kEnterpriseGroupKey_, kUrl2_);

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(kEnterpriseGroupKey_);
  ASSERT_EQ(2u, candidate_endpoints.size());
  EXPECT_EQ(kEnterpriseGroupKey_, candidate_endpoints[0].group_key);
  EXPECT_EQ(kUrl1_, candidate_endpoints[0].info.url);
  EXPECT_EQ(kEnterpriseGroupKey_, candidate_endpoints[1].group_key);
  EXPECT_EQ(kUrl2_, candidate_endpoints[1].info.url);
}

TEST_P(ReportingCacheTest, GetCandidateEndpointsFromDocumentForDelivery) {
  const base::UnguessableToken reporting_source_1 =
      base::UnguessableToken::Create();
  const base::UnguessableToken reporting_source_2 =
      base::UnguessableToken::Create();

  NetworkAnonymizationKey network_anonymization_key =
      kIsolationInfo1_.network_anonymization_key();
  const ReportingEndpointGroupKey document_group_key_1 =
      ReportingEndpointGroupKey(network_anonymization_key, reporting_source_1,
                                /*origin=*/std::nullopt, kGroup1_,
                                ReportingTargetType::kEnterprise);
  const ReportingEndpointGroupKey document_group_key_2 =
      ReportingEndpointGroupKey(network_anonymization_key, reporting_source_1,
                                kOrigin1_, kGroup2_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey document_group_key_3 =
      ReportingEndpointGroupKey(network_anonymization_key, reporting_source_2,
                                kOrigin1_, kGroup1_,
                                ReportingTargetType::kDeveloper);

  SetEnterpriseEndpointInCache(document_group_key_1, kEndpoint1_);
  SetV1EndpointInCache(document_group_key_2, reporting_source_1,
                       kIsolationInfo1_, kEndpoint2_);
  SetV1EndpointInCache(document_group_key_3, reporting_source_2,
                       kIsolationInfo1_, kEndpoint1_);
  const ReportingEndpointGroupKey kReportGroupKey = ReportingEndpointGroupKey(
      network_anonymization_key, reporting_source_1, /*origin=*/std::nullopt,
      kGroup1_, ReportingTargetType::kEnterprise);
  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(kReportGroupKey);
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(document_group_key_1, candidate_endpoints[0].group_key);
}

// V1 reporting endpoints must not be returned in response to a request for
// endpoints for network reports (with no reporting source).
TEST_P(ReportingCacheTest, GetCandidateEndpointsFromDocumentForNetworkReports) {
  const base::UnguessableToken reporting_source =
      base::UnguessableToken::Create();

  NetworkAnonymizationKey network_anonymization_key =
      kIsolationInfo1_.network_anonymization_key();

  const ReportingEndpointGroupKey kDocumentGroupKey = ReportingEndpointGroupKey(
      network_anonymization_key, reporting_source, kOrigin1_, kGroup1_,
      ReportingTargetType::kDeveloper);

  SetV1EndpointInCache(kDocumentGroupKey, reporting_source, kIsolationInfo1_,
                       kEndpoint1_);
  const ReportingEndpointGroupKey kNetworkReportGroupKey =
      ReportingEndpointGroupKey(network_anonymization_key, std::nullopt,
                                kOrigin1_, kGroup1_,
                                ReportingTargetType::kDeveloper);
  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(kNetworkReportGroupKey);
  ASSERT_EQ(0u, candidate_endpoints.size());
}

// V1 reporting endpoints must not be returned in response to a request for
// endpoints for a different source.
TEST_P(ReportingCacheTest, GetCandidateEndpointsFromDifferentDocument) {
  const base::UnguessableToken reporting_source =
      base::UnguessableToken::Create();

  NetworkAnonymizationKey network_anonymization_key =
      kIsolationInfo1_.network_anonymization_key();

  const ReportingEndpointGroupKey kDocumentGroupKey = ReportingEndpointGroupKey(
      network_anonymization_key, reporting_source, kOrigin1_, kGroup1_,
      ReportingTargetType::kDeveloper);

  SetV1EndpointInCache(kDocumentGroupKey, reporting_source, kIsolationInfo1_,
                       kEndpoint1_);
  const ReportingEndpointGroupKey kOtherGroupKey = ReportingEndpointGroupKey(
      network_anonymization_key, base::UnguessableToken::Create(), kOrigin1_,
      kGroup1_, ReportingTargetType::kDeveloper);
  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(kOtherGroupKey);
  ASSERT_EQ(0u, candidate_endpoints.size());
}

// When both V0 and V1 endpoints are present, V1 endpoints must only be
// returned when the reporting source matches. Only when no reporting source is
// given, or if there is no V1 endpoint with a matching source and name defined
// should a V0 endpoint be used.
TEST_P(ReportingCacheTest, GetMixedCandidateEndpointsForDelivery) {
  LoadReportingClients();

  // This test relies on proper NAKs being used, so set those up, and endpoint
  // group keys to go with them.
  NetworkAnonymizationKey network_anonymization_key1 =
      kIsolationInfo1_.network_anonymization_key();
  NetworkAnonymizationKey network_anonymization_key2 =
      kIsolationInfo2_.network_anonymization_key();
  ReportingEndpointGroupKey group_key_11 =
      ReportingEndpointGroupKey(network_anonymization_key1, kOrigin1_, kGroup1_,
                                ReportingTargetType::kDeveloper);
  ReportingEndpointGroupKey group_key_12 =
      ReportingEndpointGroupKey(network_anonymization_key1, kOrigin1_, kGroup2_,
                                ReportingTargetType::kDeveloper);
  ReportingEndpointGroupKey group_key_21 =
      ReportingEndpointGroupKey(network_anonymization_key2, kOrigin2_, kGroup1_,
                                ReportingTargetType::kDeveloper);

  // Set up V0 endpoint groups for this origin.
  ASSERT_TRUE(SetEndpointInCache(group_key_11, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(group_key_11, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(group_key_12, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(group_key_21, kEndpoint1_, kExpires1_));

  // Set up a V1 endpoint for a document at the same origin.
  NetworkAnonymizationKey network_anonymization_key =
      kIsolationInfo1_.network_anonymization_key();
  const base::UnguessableToken reporting_source =
      base::UnguessableToken::Create();
  const ReportingEndpointGroupKey document_group_key =
      ReportingEndpointGroupKey(network_anonymization_key1, reporting_source,
                                kOrigin1_, kGroup1_,
                                ReportingTargetType::kDeveloper);
  SetV1EndpointInCache(document_group_key, reporting_source, kIsolationInfo1_,
                       kEndpoint1_);

  // This group key will match both the V1 endpoint, and two V0 endpoints. Only
  // the V1 endpoint should be returned.
  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(ReportingEndpointGroupKey(
          network_anonymization_key1, reporting_source, kOrigin1_, kGroup1_,
          ReportingTargetType::kDeveloper));
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(document_group_key, candidate_endpoints[0].group_key);

  // This group key has no reporting source, so only V0 endpoints can be
  // returned.
  candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(ReportingEndpointGroupKey(
          network_anonymization_key1, std::nullopt, kOrigin1_, kGroup1_,
          ReportingTargetType::kDeveloper));
  ASSERT_EQ(2u, candidate_endpoints.size());
  EXPECT_EQ(group_key_11, candidate_endpoints[0].group_key);
  EXPECT_EQ(group_key_11, candidate_endpoints[1].group_key);

  // This group key has a reporting source, but no matching V1 endpoints have
  // been configured, so we should fall back to the V0 endpoints.
  candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(ReportingEndpointGroupKey(
          network_anonymization_key1, reporting_source, kOrigin1_, kGroup2_,
          ReportingTargetType::kDeveloper));
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(group_key_12, candidate_endpoints[0].group_key);
}

TEST_P(ReportingCacheTest, GetCandidateEndpointsDifferentNak) {
  LoadReportingClients();

  // Test that NAKs are respected by using 2 groups with the same origin and
  // group name but different NAKs.
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOtherGroupKey11_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOtherGroupKey11_, kEndpoint2_, kExpires1_));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(kGroupKey11_);
  ASSERT_EQ(2u, candidate_endpoints.size());
  EXPECT_EQ(kGroupKey11_, candidate_endpoints[0].group_key);
  EXPECT_EQ(kGroupKey11_, candidate_endpoints[1].group_key);

  candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(kOtherGroupKey11_);
  ASSERT_EQ(2u, candidate_endpoints.size());
  EXPECT_EQ(kOtherGroupKey11_, candidate_endpoints[0].group_key);
  EXPECT_EQ(kOtherGroupKey11_, candidate_endpoints[1].group_key);
}

TEST_P(ReportingCacheTest, GetCandidateEndpointsExcludesExpired) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey21_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey22_, kEndpoint2_, kExpires2_));
  // Make kExpires1_ expired but not kExpires2_.
  clock()->Advance(base::Days(8));
  ASSERT_GT(clock()->Now(), kExpires1_);
  ASSERT_LT(clock()->Now(), kExpires2_);

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(kGroupKey11_);
  ASSERT_EQ(0u, candidate_endpoints.size());

  candidate_endpoints = cache()->GetCandidateEndpointsForDelivery(kGroupKey21_);
  ASSERT_EQ(0u, candidate_endpoints.size());

  candidate_endpoints = cache()->GetCandidateEndpointsForDelivery(kGroupKey22_);
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kEndpoint2_, candidate_endpoints[0].info.url);
}

TEST_P(ReportingCacheTest, ExcludeSubdomainsDifferentPort) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://example/"));
  const url::Origin kDifferentPortOrigin =
      url::Origin::Create(GURL("https://example:444/"));

  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kNak_, kDifferentPortOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::EXCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(ReportingEndpointGroupKey(
          kNak_, kOrigin, kGroup1_, ReportingTargetType::kDeveloper));
  ASSERT_EQ(0u, candidate_endpoints.size());
}

TEST_P(ReportingCacheTest, ExcludeSubdomainsSuperdomain) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.example/"));
  const url::Origin kSuperOrigin =
      url::Origin::Create(GURL("https://example/"));

  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kNak_, kSuperOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::EXCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(ReportingEndpointGroupKey(
          kNak_, kOrigin, kGroup1_, ReportingTargetType::kDeveloper));
  ASSERT_EQ(0u, candidate_endpoints.size());
}

TEST_P(ReportingCacheTest, IncludeSubdomainsDifferentPort) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://example/"));
  const url::Origin kDifferentPortOrigin =
      url::Origin::Create(GURL("https://example:444/"));

  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kNak_, kDifferentPortOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::INCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(ReportingEndpointGroupKey(
          kNak_, kOrigin, kGroup1_, ReportingTargetType::kDeveloper));
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kDifferentPortOrigin, candidate_endpoints[0].group_key.origin);
}

TEST_P(ReportingCacheTest, IncludeSubdomainsSuperdomain) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.example/"));
  const url::Origin kSuperOrigin =
      url::Origin::Create(GURL("https://example/"));

  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kNak_, kSuperOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::INCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(ReportingEndpointGroupKey(
          kNak_, kOrigin, kGroup1_, ReportingTargetType::kDeveloper));
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kSuperOrigin, candidate_endpoints[0].group_key.origin);
}

TEST_P(ReportingCacheTest, IncludeSubdomainsPreferOriginToDifferentPort) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.example/"));
  const url::Origin kDifferentPortOrigin =
      url::Origin::Create(GURL("https://example:444/"));

  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kNak_, kOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::INCLUDE));
  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kNak_, kDifferentPortOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::INCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(ReportingEndpointGroupKey(
          kNak_, kOrigin, kGroup1_, ReportingTargetType::kDeveloper));
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kOrigin, candidate_endpoints[0].group_key.origin);
}

TEST_P(ReportingCacheTest, IncludeSubdomainsPreferOriginToSuperdomain) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.example/"));
  const url::Origin kSuperOrigin =
      url::Origin::Create(GURL("https://example/"));

  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kNak_, kOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::INCLUDE));
  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kNak_, kSuperOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::INCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(ReportingEndpointGroupKey(
          kNak_, kOrigin, kGroup1_, ReportingTargetType::kDeveloper));
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kOrigin, candidate_endpoints[0].group_key.origin);
}

TEST_P(ReportingCacheTest, IncludeSubdomainsPreferMoreSpecificSuperdomain) {
  LoadReportingClients();

  const url::Origin kOrigin =
      url::Origin::Create(GURL("https://foo.bar.example/"));
  const url::Origin kSuperOrigin =
      url::Origin::Create(GURL("https://bar.example/"));
  const url::Origin kSuperSuperOrigin =
      url::Origin::Create(GURL("https://example/"));

  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kNak_, kSuperOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::INCLUDE));
  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kNak_, kSuperSuperOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::INCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(ReportingEndpointGroupKey(
          kNak_, kOrigin, kGroup1_, ReportingTargetType::kDeveloper));
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kSuperOrigin, candidate_endpoints[0].group_key.origin);
}

TEST_P(ReportingCacheTest, IncludeSubdomainsPreserveNak) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.example/"));
  const url::Origin kSuperOrigin =
      url::Origin::Create(GURL("https://example/"));

  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kNak_, kSuperOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::INCLUDE));
  ASSERT_TRUE(SetEndpointInCache(
      ReportingEndpointGroupKey(kOtherNak_, kSuperOrigin, kGroup1_,
                                ReportingTargetType::kDeveloper),
      kEndpoint1_, kExpires1_, OriginSubdomains::INCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(ReportingEndpointGroupKey(
          kOtherNak_, kOrigin, kGroup1_, ReportingTargetType::kDeveloper));
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kOtherNak_,
            candidate_endpoints[0].group_key.network_anonymization_key);
}

TEST_P(ReportingCacheTest, EvictOldestReport) {
  LoadReportingClients();

  size_t max_report_count = policy().max_report_count;

  ASSERT_LT(0u, max_report_count);
  ASSERT_GT(std::numeric_limits<size_t>::max(), max_report_count);

  base::TimeTicks earliest_queued = tick_clock()->NowTicks();

  // Enqueue the maximum number of reports, spaced apart in time.
  for (size_t i = 0; i < max_report_count; ++i) {
    cache()->AddReport(kReportingSource_, kNak_, kUrl1_, kUserAgent_, kGroup1_,
                       kType_, base::Value::Dict(), 0, tick_clock()->NowTicks(),
                       0, ReportingTargetType::kDeveloper);
    tick_clock()->Advance(base::Minutes(1));
  }
  EXPECT_EQ(max_report_count, report_count());

  // Add one more report to force the cache to evict one.
  cache()->AddReport(kReportingSource_, kNak_, kUrl1_, kUserAgent_, kGroup1_,
                     kType_, base::Value::Dict(), 0, tick_clock()->NowTicks(),
                     0, ReportingTargetType::kDeveloper);

  // Make sure the cache evicted a report to make room for the new one, and make
  // sure the report evicted was the earliest-queued one.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(max_report_count, reports.size());
  for (const ReportingReport* report : reports)
    EXPECT_NE(earliest_queued, report->queued);
}

TEST_P(ReportingCacheTest, DontEvictPendingReports) {
  LoadReportingClients();

  size_t max_report_count = policy().max_report_count;

  ASSERT_LT(0u, max_report_count);
  ASSERT_GT(std::numeric_limits<size_t>::max(), max_report_count);

  // Enqueue the maximum number of reports, spaced apart in time.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  for (size_t i = 0; i < max_report_count; ++i) {
    reports.push_back(AddAndReturnReport(kNak_, kUrl1_, kUserAgent_, kGroup1_,
                                         kType_, base::Value::Dict(), 0,
                                         tick_clock()->NowTicks(), 0));
    tick_clock()->Advance(base::Minutes(1));
  }
  EXPECT_EQ(max_report_count, report_count());

  // Mark all of the queued reports pending.
  EXPECT_THAT(cache()->GetReportsToDeliver(),
              ::testing::UnorderedElementsAreArray(reports));

  // Add one more report to force the cache to evict one. Since the cache has
  // only pending reports, it will be forced to evict the *new* report!
  cache()->AddReport(kReportingSource_, kNak_, kUrl1_, kUserAgent_, kGroup1_,
                     kType_, base::Value::Dict(), 0, kNowTicks_, 0,
                     ReportingTargetType::kDeveloper);

  // Make sure the cache evicted a report, and make sure the report evicted was
  // the new, non-pending one.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
      reports_after_eviction;
  cache()->GetReports(&reports_after_eviction);
  EXPECT_EQ(max_report_count, reports_after_eviction.size());
  for (const ReportingReport* report : reports_after_eviction) {
    EXPECT_TRUE(cache()->IsReportPendingForTesting(report));
  }

  EXPECT_THAT(reports_after_eviction,
              ::testing::UnorderedElementsAreArray(reports));
}

TEST_P(ReportingCacheTest, EvictEndpointsOverPerOriginLimit) {
  LoadReportingClients();

  for (size_t i = 0; i < policy().max_endpoints_per_origin; ++i) {
    ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, MakeURL(i), kExpires1_));
    EXPECT_EQ(i + 1, cache()->GetEndpointCount());
  }
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());
  // Insert one more endpoint; eviction should be triggered.
  SetEndpointInCache(kGroupKey11_, kEndpoint1_, kExpires1_);
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());
}

TEST_P(ReportingCacheTest, EvictExpiredGroups) {
  LoadReportingClients();

  for (size_t i = 0; i < policy().max_endpoints_per_origin; ++i) {
    ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, MakeURL(i), kExpires1_));
    EXPECT_EQ(i + 1, cache()->GetEndpointCount());
  }
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  // Make the group expired (but not stale).
  clock()->SetNow(kExpires1_ - base::Minutes(1));
  cache()->GetCandidateEndpointsForDelivery(kGroupKey11_);
  clock()->SetNow(kExpires1_ + base::Minutes(1));

  // Insert one more endpoint in a different group (not expired); eviction
  // should be triggered and the expired group should be deleted.
  SetEndpointInCache(kGroupKey12_, kEndpoint1_, kExpires2_);
  EXPECT_GE(policy().max_endpoints_per_origin, cache()->GetEndpointCount());
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_FALSE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey12_, OriginSubdomains::DEFAULT));
}

TEST_P(ReportingCacheTest, EvictStaleGroups) {
  LoadReportingClients();

  for (size_t i = 0; i < policy().max_endpoints_per_origin; ++i) {
    ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, MakeURL(i), kExpires1_));
    EXPECT_EQ(i + 1, cache()->GetEndpointCount());
  }
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  // Make the group stale (but not expired).
  clock()->Advance(2 * policy().max_group_staleness);
  ASSERT_LT(clock()->Now(), kExpires1_);

  // Insert one more endpoint in a different group; eviction should be
  // triggered and the stale group should be deleted.
  SetEndpointInCache(kGroupKey12_, kEndpoint1_, kExpires1_);
  EXPECT_GE(policy().max_endpoints_per_origin, cache()->GetEndpointCount());
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_FALSE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey12_, OriginSubdomains::DEFAULT));
}

TEST_P(ReportingCacheTest, EvictFromStalestGroup) {
  LoadReportingClients();

  for (size_t i = 0; i < policy().max_endpoints_per_origin; ++i) {
    ReportingEndpointGroupKey group_key(kNak_, kOrigin1_,
                                        base::NumberToString(i),
                                        ReportingTargetType::kDeveloper);
    ASSERT_TRUE(SetEndpointInCache(group_key, MakeURL(i), kExpires1_));
    EXPECT_EQ(i + 1, cache()->GetEndpointCount());
    EXPECT_TRUE(
        EndpointGroupExistsInCache(group_key, OriginSubdomains::DEFAULT));
    // Mark group used.
    cache()->GetCandidateEndpointsForDelivery(group_key);
    clock()->Advance(base::Minutes(1));
  }
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  // Insert one more endpoint in a different group; eviction should be
  // triggered and (only) the stalest group should be evicted from (and in this
  // case deleted).
  SetEndpointInCache(kGroupKey12_, kEndpoint1_, kExpires1_);
  EXPECT_GE(policy().max_endpoints_per_origin, cache()->GetEndpointCount());
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_FALSE(EndpointGroupExistsInCache(
      ReportingEndpointGroupKey(kNak_, kOrigin1_, "0",
                                ReportingTargetType::kDeveloper),
      OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey12_, OriginSubdomains::DEFAULT));
  for (size_t i = 1; i < policy().max_endpoints_per_origin; ++i) {
    ReportingEndpointGroupKey group_key(kNak_, kOrigin1_,
                                        base::NumberToString(i),
                                        ReportingTargetType::kDeveloper);
    EXPECT_TRUE(
        EndpointGroupExistsInCache(group_key, OriginSubdomains::DEFAULT));
  }
}

TEST_P(ReportingCacheTest, EvictFromLargestGroup) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, MakeURL(0), kExpires1_));
  // This group should be evicted from because it has 2 endpoints.
  ASSERT_TRUE(SetEndpointInCache(kGroupKey12_, MakeURL(1), kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey12_, MakeURL(2), kExpires1_));

  // max_endpoints_per_origin is set to 3.
  ASSERT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  // Insert one more endpoint in a different group; eviction should be
  // triggered.
  SetEndpointInCache(ReportingEndpointGroupKey(kNak_, kOrigin1_, "default",
                                               ReportingTargetType::kDeveloper),
                     kEndpoint1_, kExpires1_);
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey12_, OriginSubdomains::DEFAULT));
  // Count the number of endpoints remaining in kGroupKey12_.
  std::vector<ReportingEndpoint> endpoints_in_group =
      cache()->GetCandidateEndpointsForDelivery(kGroupKey12_);
  EXPECT_EQ(1u, endpoints_in_group.size());
}

TEST_P(ReportingCacheTest, EvictLeastImportantEndpoint) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, MakeURL(0), kExpires1_,
                                 OriginSubdomains::DEFAULT, 1 /* priority*/,
                                 1 /* weight */));
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, MakeURL(1), kExpires1_,
                                 OriginSubdomains::DEFAULT, 2 /* priority */,
                                 2 /* weight */));
  // This endpoint will be evicted because it is lowest priority and lowest
  // weight.
  ASSERT_TRUE(SetEndpointInCache(kGroupKey11_, MakeURL(2), kExpires1_,
                                 OriginSubdomains::DEFAULT, 2 /* priority */,
                                 1 /* weight */));

  // max_endpoints_per_origin is set to 3.
  ASSERT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  // Insert one more endpoint in a different group; eviction should be
  // triggered and the least important endpoint should be deleted.
  SetEndpointInCache(kGroupKey12_, kEndpoint1_, kExpires1_);
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  EXPECT_TRUE(FindEndpointInCache(kGroupKey11_, MakeURL(0)));
  EXPECT_TRUE(FindEndpointInCache(kGroupKey11_, MakeURL(1)));
  EXPECT_FALSE(FindEndpointInCache(kGroupKey11_, MakeURL(2)));
  EXPECT_TRUE(FindEndpointInCache(kGroupKey12_, kEndpoint1_));
}

TEST_P(ReportingCacheTest, EvictEndpointsOverGlobalLimitFromStalestClient) {
  LoadReportingClients();

  // Set enough endpoints to reach the global endpoint limit.
  for (size_t i = 0; i < policy().max_endpoint_count; ++i) {
    ReportingEndpointGroupKey group_key(kNak_, url::Origin::Create(MakeURL(i)),
                                        kGroup1_,
                                        ReportingTargetType::kDeveloper);
    ASSERT_TRUE(SetEndpointInCache(group_key, MakeURL(i), kExpires1_));
    EXPECT_EQ(i + 1, cache()->GetEndpointCount());
    clock()->Advance(base::Minutes(1));
  }
  EXPECT_EQ(policy().max_endpoint_count, cache()->GetEndpointCount());

  // Insert one more endpoint for a different origin; eviction should be
  // triggered and the stalest client should be evicted from (and in this case
  // deleted).
  SetEndpointInCache(kGroupKey11_, kEndpoint1_, kExpires1_);
  EXPECT_EQ(policy().max_endpoint_count, cache()->GetEndpointCount());
  EXPECT_FALSE(ClientExistsInCacheForOrigin(url::Origin::Create(MakeURL(0))));
  for (size_t i = 1; i < policy().max_endpoint_count; ++i) {
    EXPECT_TRUE(ClientExistsInCacheForOrigin(url::Origin::Create(MakeURL(i))));
  }
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
}

TEST_P(ReportingCacheTest, AddClientsLoadedFromStore) {
  if (!store())
    return;

  base::Time now = clock()->Now();

  std::vector<ReportingEndpoint> endpoints;
  endpoints.emplace_back(kGroupKey11_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kGroupKey22_,
                         ReportingEndpoint::EndpointInfo{kEndpoint2_});
  endpoints.emplace_back(kGroupKey11_,
                         ReportingEndpoint::EndpointInfo{kEndpoint2_});
  endpoints.emplace_back(kGroupKey21_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  std::vector<CachedReportingEndpointGroup> groups;
  groups.emplace_back(kGroupKey21_, OriginSubdomains::DEFAULT,
                      now + base::Minutes(2) /* expires */,
                      now /* last_used */);
  groups.emplace_back(kGroupKey11_, OriginSubdomains::DEFAULT,
                      now + base::Minutes(1) /* expires */,
                      now /* last_used */);
  groups.emplace_back(kGroupKey22_, OriginSubdomains::DEFAULT,
                      now + base::Minutes(3) /* expires */,
                      now /* last_used */);
  store()->SetPrestoredClients(endpoints, groups);

  LoadReportingClients();

  EXPECT_EQ(4u, cache()->GetEndpointCount());
  EXPECT_EQ(3u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey11_, kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey11_, kEndpoint2_));
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey21_, kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey22_, kEndpoint2_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey11_, OriginSubdomains::DEFAULT, now + base::Minutes(1)));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey21_, OriginSubdomains::DEFAULT, now + base::Minutes(2)));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey22_, OriginSubdomains::DEFAULT, now + base::Minutes(3)));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin2_));
}

TEST_P(ReportingCacheTest,
       AddStoredClientsWithDifferentNetworkAnonymizationKeys) {
  if (!store())
    return;

  base::Time now = clock()->Now();

  // This should create 4 different clients, for (2 origins) x (2 NAKs).
  // Intentionally in a weird order to check sorting.
  std::vector<ReportingEndpoint> endpoints;
  endpoints.emplace_back(kGroupKey11_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kGroupKey21_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kOtherGroupKey21_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kOtherGroupKey11_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  std::vector<CachedReportingEndpointGroup> groups;
  groups.emplace_back(kGroupKey21_, OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  groups.emplace_back(kOtherGroupKey21_, OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  groups.emplace_back(kOtherGroupKey11_, OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  groups.emplace_back(kGroupKey11_, OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);

  store()->SetPrestoredClients(endpoints, groups);

  LoadReportingClients();

  EXPECT_EQ(4u, cache()->GetEndpointCount());
  EXPECT_EQ(4u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_EQ(4u, cache()->GetClientCountForTesting());
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey11_, kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey21_, kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(kOtherGroupKey11_, kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(kOtherGroupKey21_, kEndpoint1_));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey21_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kOtherGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kOtherGroupKey21_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(cache()->ClientExistsForTesting(
      kGroupKey11_.network_anonymization_key, kGroupKey11_.origin.value()));
  EXPECT_TRUE(cache()->ClientExistsForTesting(
      kGroupKey21_.network_anonymization_key, kGroupKey21_.origin.value()));
  EXPECT_TRUE(cache()->ClientExistsForTesting(
      kOtherGroupKey11_.network_anonymization_key,
      kOtherGroupKey11_.origin.value()));
  EXPECT_TRUE(cache()->ClientExistsForTesting(
      kOtherGroupKey21_.network_anonymization_key,
      kOtherGroupKey21_.origin.value()));
}

TEST_P(ReportingCacheTest, DoNotStoreMoreThanLimits) {
  if (!store())
    return;

  base::Time now = clock()->Now();

  // We hardcode the number of endpoints in this test, so we need to manually
  // update the test when |max_endpoint_count| changes. You'll need to
  // add/remove elements to |endpoints| when that happens.
  EXPECT_EQ(5u, policy().max_endpoint_count) << "You need to update this test "
                                             << "to reflect a change in "
                                             << "max_endpoint_count";

  std::vector<ReportingEndpoint> endpoints;
  endpoints.emplace_back(kGroupKey11_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kGroupKey11_,
                         ReportingEndpoint::EndpointInfo{kEndpoint2_});
  endpoints.emplace_back(kGroupKey11_,
                         ReportingEndpoint::EndpointInfo{kEndpoint3_});
  endpoints.emplace_back(kGroupKey11_,
                         ReportingEndpoint::EndpointInfo{kEndpoint4_});
  endpoints.emplace_back(kGroupKey22_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kGroupKey22_,
                         ReportingEndpoint::EndpointInfo{kEndpoint2_});
  endpoints.emplace_back(kGroupKey22_,
                         ReportingEndpoint::EndpointInfo{kEndpoint3_});
  endpoints.emplace_back(kGroupKey22_,
                         ReportingEndpoint::EndpointInfo{kEndpoint4_});
  std::vector<CachedReportingEndpointGroup> groups;
  groups.emplace_back(kGroupKey11_, OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  groups.emplace_back(kGroupKey22_, OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  store()->SetPrestoredClients(endpoints, groups);

  LoadReportingClients();

  EXPECT_GE(5u, cache()->GetEndpointCount());
  EXPECT_GE(2u, cache()->GetEndpointGroupCountForTesting());
}

TEST_P(ReportingCacheTest, DoNotLoadMismatchedGroupsAndEndpoints) {
  if (!store())
    return;

  base::Time now = clock()->Now();

  std::vector<ReportingEndpoint> endpoints;
  // This endpoint has no corresponding endpoint group
  endpoints.emplace_back(kGroupKey11_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kGroupKey21_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  // This endpoint has no corresponding endpoint group
  endpoints.emplace_back(kGroupKey22_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  std::vector<CachedReportingEndpointGroup> groups;
  // This endpoint group has no corresponding endpoint
  groups.emplace_back(kGroupKey12_, OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  groups.emplace_back(kGroupKey21_, OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  // This endpoint group has no corresponding endpoint
  groups.emplace_back(
      ReportingEndpointGroupKey(kNak_, kOrigin2_, "last_group",
                                ReportingTargetType::kDeveloper),
      OriginSubdomains::DEFAULT, now /* expires */, now /* last_used */);
  store()->SetPrestoredClients(endpoints, groups);

  LoadReportingClients();

  EXPECT_GE(1u, cache()->GetEndpointCount());
  EXPECT_GE(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey21_, kEndpoint1_));
}

// This test verifies that we preserve the last_used field when storing clients
// loaded from disk. We don't have direct access into individual cache elements,
// so we test this indirectly by triggering a cache eviction and verifying that
// a stale element (i.e., one older than a week, by default) is selected for
// eviction. If last_used weren't populated then presumably that element
// wouldn't be evicted. (Or rather, it would only have a 25% chance of being
// evicted and this test would then be flaky.)
TEST_P(ReportingCacheTest, StoreLastUsedProperly) {
  if (!store())
    return;

  base::Time now = clock()->Now();

  // We hardcode the number of endpoints in this test, so we need to manually
  // update the test when |max_endpoints_per_origin| changes. You'll need to
  // add/remove elements to |endpoints| and |grups| when that happens.
  EXPECT_EQ(3u, policy().max_endpoints_per_origin)
      << "You need to update this test to reflect a change in "
         "max_endpoints_per_origin";

  // We need more than three endpoints to trigger eviction.
  std::vector<ReportingEndpoint> endpoints;
  ReportingEndpointGroupKey group1(kNak_, kOrigin1_, "1",
                                   ReportingTargetType::kDeveloper);
  ReportingEndpointGroupKey group2(kNak_, kOrigin1_, "2",
                                   ReportingTargetType::kDeveloper);
  ReportingEndpointGroupKey group3(kNak_, kOrigin1_, "3",
                                   ReportingTargetType::kDeveloper);
  ReportingEndpointGroupKey group4(kNak_, kOrigin1_, "4",
                                   ReportingTargetType::kDeveloper);
  endpoints.emplace_back(group1, ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(group2, ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(group3, ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(group4, ReportingEndpoint::EndpointInfo{kEndpoint1_});
  std::vector<CachedReportingEndpointGroup> groups;
  groups.emplace_back(group1, OriginSubdomains::DEFAULT, now /* expires */,
                      now /* last_used */);
  groups.emplace_back(group2, OriginSubdomains::DEFAULT, now /* expires */,
                      now /* last_used */);
  // Stale last_used on group "3" should cause us to select it for eviction
  groups.emplace_back(group3, OriginSubdomains::DEFAULT, now /* expires */,
                      base::Time() /* last_used */);
  groups.emplace_back(group4, OriginSubdomains::DEFAULT, now /* expires */,
                      now /* last_used */);
  store()->SetPrestoredClients(endpoints, groups);

  LoadReportingClients();

  EXPECT_TRUE(EndpointExistsInCache(group1, kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(group2, kEndpoint1_));
  EXPECT_FALSE(EndpointExistsInCache(group3, kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(group4, kEndpoint1_));
}

TEST_P(ReportingCacheTest, DoNotAddDuplicatedEntriesFromStore) {
  if (!store())
    return;

  base::Time now = clock()->Now();

  std::vector<ReportingEndpoint> endpoints;
  endpoints.emplace_back(kGroupKey11_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kGroupKey22_,
                         ReportingEndpoint::EndpointInfo{kEndpoint2_});
  endpoints.emplace_back(kGroupKey11_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  std::vector<CachedReportingEndpointGroup> groups;
  groups.emplace_back(kGroupKey11_, OriginSubdomains::DEFAULT,
                      now + base::Minutes(1) /* expires */,
                      now /* last_used */);
  groups.emplace_back(kGroupKey22_, OriginSubdomains::DEFAULT,
                      now + base::Minutes(3) /* expires */,
                      now /* last_used */);
  groups.emplace_back(kGroupKey11_, OriginSubdomains::DEFAULT,
                      now + base::Minutes(1) /* expires */,
                      now /* last_used */);
  store()->SetPrestoredClients(endpoints, groups);

  LoadReportingClients();

  EXPECT_EQ(2u, cache()->GetEndpointCount());
  EXPECT_EQ(2u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey11_, kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey22_, kEndpoint2_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey11_, OriginSubdomains::DEFAULT, now + base::Minutes(1)));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kGroupKey22_, OriginSubdomains::DEFAULT, now + base::Minutes(3)));
}

TEST_P(ReportingCacheTest, GetIsolationInfoForEndpoint) {
  LoadReportingClients();

  NetworkAnonymizationKey network_anonymization_key1 =
      kIsolationInfo1_.network_anonymization_key();

  // Set up a V1 endpoint for this origin.
  cache()->SetV1EndpointForTesting(
      ReportingEndpointGroupKey(network_anonymization_key1, *kReportingSource_,
                                kOrigin1_, kGroup1_,
                                ReportingTargetType::kDeveloper),
      *kReportingSource_, kIsolationInfo1_, kUrl1_);

  // Set up a V0 endpoint group for this origin.
  ReportingEndpointGroupKey group_key_11 =
      ReportingEndpointGroupKey(network_anonymization_key1, kOrigin1_, kGroup1_,
                                ReportingTargetType::kDeveloper);
  ASSERT_TRUE(SetEndpointInCache(group_key_11, kEndpoint1_, kExpires1_));

  // For a V1 endpoint, ensure that the isolation info matches exactly what was
  // passed in.
  ReportingEndpoint endpoint =
      cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup1_);
  EXPECT_TRUE(endpoint);
  IsolationInfo isolation_info_for_document =
      cache()->GetIsolationInfoForEndpoint(endpoint);
  EXPECT_TRUE(isolation_info_for_document.IsEqualForTesting(kIsolationInfo1_));
  EXPECT_EQ(isolation_info_for_document.request_type(),
            IsolationInfo::RequestType::kOther);

  // For a V0 endpoint, ensure that site_for_cookies is null and that the NAK
  // matches the cached endpoint.
  ReportingEndpoint network_endpoint =
      cache()->GetEndpointForTesting(group_key_11, kEndpoint1_);
  EXPECT_TRUE(network_endpoint);
  IsolationInfo isolation_info_for_network =
      cache()->GetIsolationInfoForEndpoint(network_endpoint);
  EXPECT_EQ(isolation_info_for_network.request_type(),
            IsolationInfo::RequestType::kOther);
  EXPECT_EQ(isolation_info_for_network.network_anonymization_key(),
            network_endpoint.group_key.network_anonymization_key);
  EXPECT_TRUE(isolation_info_for_network.site_for_cookies().IsNull());
}

TEST_P(ReportingCacheTest, GetV1ReportingEndpointsForOrigin) {
  const base::UnguessableToken reporting_source_2 =
      base::UnguessableToken::Create();
  LoadReportingClients();

  NetworkAnonymizationKey network_anonymization_key_1 =
      kIsolationInfo1_.network_anonymization_key();
  NetworkAnonymizationKey network_anonymization_key_2 =
      kIsolationInfo2_.network_anonymization_key();

  // Store endpoints from different origins in cache
  cache()->SetV1EndpointForTesting(
      ReportingEndpointGroupKey(network_anonymization_key_1, *kReportingSource_,
                                kOrigin1_, kGroup1_,
                                ReportingTargetType::kDeveloper),
      *kReportingSource_, kIsolationInfo1_, kUrl1_);
  cache()->SetV1EndpointForTesting(
      ReportingEndpointGroupKey(network_anonymization_key_1, *kReportingSource_,
                                kOrigin1_, kGroup2_,
                                ReportingTargetType::kDeveloper),
      *kReportingSource_, kIsolationInfo1_, kUrl2_);
  cache()->SetV1EndpointForTesting(
      ReportingEndpointGroupKey(network_anonymization_key_2, reporting_source_2,
                                kOrigin2_, kGroup1_,
                                ReportingTargetType::kDeveloper),
      reporting_source_2, kIsolationInfo2_, kUrl2_);

  // Retrieve endpoints by origin and ensure they match expectations
  auto endpoints = cache()->GetV1ReportingEndpointsByOrigin();
  EXPECT_EQ(2u, endpoints.size());
  auto origin_1_endpoints = endpoints.at(kOrigin1_);
  EXPECT_EQ(2u, origin_1_endpoints.size());
  EXPECT_EQ(ReportingEndpointGroupKey(network_anonymization_key_1,
                                      *kReportingSource_, kOrigin1_, kGroup1_,
                                      ReportingTargetType::kDeveloper),
            origin_1_endpoints[0].group_key);
  EXPECT_EQ(kUrl1_, origin_1_endpoints[0].info.url);
  EXPECT_EQ(ReportingEndpointGroupKey(network_anonymization_key_1,
                                      *kReportingSource_, kOrigin1_, kGroup2_,
                                      ReportingTargetType::kDeveloper),
            origin_1_endpoints[1].group_key);
  EXPECT_EQ(kUrl2_, origin_1_endpoints[1].info.url);
  auto origin_2_endpoints = endpoints.at(kOrigin2_);
  EXPECT_EQ(1u, origin_2_endpoints.size());
  EXPECT_EQ(ReportingEndpointGroupKey(network_anonymization_key_2,
                                      reporting_source_2, kOrigin2_, kGroup1_,
                                      ReportingTargetType::kDeveloper),
            origin_2_endpoints[0].group_key);
  EXPECT_EQ(kUrl2_, origin_2_endpoints[0].info.url);
}

TEST_P(ReportingCacheTest, ReportingTargetType) {
  const ReportingEndpointGroupKey kDeveloperGroupKey_ =
      ReportingEndpointGroupKey(kIsolationInfo1_.network_anonymization_key(),
                                *kReportingSource_, kOrigin1_, kGroup1_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kEnterpriseGroupKey_ =
      ReportingEndpointGroupKey(kIsolationInfo1_.network_anonymization_key(),
                                *kReportingSource_, /*origin=*/std::nullopt,
                                kGroup1_, ReportingTargetType::kEnterprise);

  cache()->SetV1EndpointForTesting(kDeveloperGroupKey_, *kReportingSource_,
                                   kIsolationInfo1_, kUrl1_);
  cache()->SetEnterpriseEndpointForTesting(kEnterpriseGroupKey_, kUrl1_);

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(kDeveloperGroupKey_);
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(ReportingTargetType::kDeveloper,
            candidate_endpoints[0].group_key.target_type);

  candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(kEnterpriseGroupKey_);
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(ReportingTargetType::kEnterprise,
            candidate_endpoints[0].group_key.target_type);
}

INSTANTIATE_TEST_SUITE_P(ReportingCacheStoreTest,
                         ReportingCacheTest,
                         testing::Bool());

}  // namespace
}  // namespace net
