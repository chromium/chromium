// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_cache.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/network_isolation_key.h"
#include "net/reporting/mock_persistent_reporting_store.h"
#include "net/reporting/reporting_cache_impl.h"
#include "net/reporting/reporting_cache_observer.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_report.h"
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
  TestReportingCacheObserver()
      : cached_reports_update_count_(0), cached_clients_update_count_(0) {}

  void OnReportsUpdated() override { ++cached_reports_update_count_; }
  void OnClientsUpdated() override { ++cached_clients_update_count_; }

  int cached_reports_update_count() const {
    return cached_reports_update_count_;
  }
  int cached_clients_update_count() const {
    return cached_clients_update_count_;
  }

 private:
  int cached_reports_update_count_;
  int cached_clients_update_count_;
};

// The tests are parametrized on a boolean value which represents whether or not
// to use a MockPersistentReportingStore.
class ReportingCacheTest : public ReportingTestBase,
                           public ::testing::WithParamInterface<bool> {
 protected:
  ReportingCacheTest() : ReportingTestBase() {
    ReportingPolicy policy;
    policy.max_report_count = 5;
    policy.max_endpoints_per_origin = 3;
    policy.max_endpoint_count = 5;
    policy.max_group_staleness = base::TimeDelta::FromDays(3);
    UsePolicy(policy);

    if (GetParam())
      store_ = std::make_unique<MockPersistentReportingStore>();
    else
      store_ = nullptr;

    UseStore(store_.get());

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
    std::vector<const ReportingReport*> reports;
    cache()->GetReports(&reports);
    return reports.size();
  }

  MockPersistentReportingStore* store() { return store_.get(); }

  // Adds a new report to the cache, and returns it.
  const ReportingReport* AddAndReturnReport(
      const GURL& url,
      const std::string& user_agent,
      const std::string& group,
      const std::string& type,
      std::unique_ptr<const base::Value> body,
      int depth,
      base::TimeTicks queued,
      int attempts) {
    const base::Value* body_unowned = body.get();

    // The public API will only give us the (unordered) full list of reports in
    // the cache.  So we need to grab the list before we add, and the list after
    // we add, and return the one element that's different.  This is only used
    // in test cases, so I've optimized for readability over execution speed.
    std::vector<const ReportingReport*> before;
    cache()->GetReports(&before);
    cache()->AddReport(url, user_agent, group, type, std::move(body), depth,
                       queued, attempts);
    std::vector<const ReportingReport*> after;
    cache()->GetReports(&after);

    for (const ReportingReport* report : after) {
      // If report isn't in before, we've found the new instance.
      if (std::find(before.begin(), before.end(), report) == before.end()) {
        // Sanity check the result before we return it.
        EXPECT_EQ(url, report->url);
        EXPECT_EQ(user_agent, report->user_agent);
        EXPECT_EQ(group, report->group);
        EXPECT_EQ(type, report->type);
        EXPECT_EQ(*body_unowned, *report->body);
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

  const GURL kUrl1_ = GURL("https://origin1/path");
  const GURL kUrl2_ = GURL("https://origin2/path");
  const url::Origin kOrigin1_ = url::Origin::Create(GURL("https://origin1/"));
  const url::Origin kOrigin2_ = url::Origin::Create(GURL("https://origin2/"));
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
  const base::Time kExpires1_ = kNow_ + base::TimeDelta::FromDays(7);
  const base::Time kExpires2_ = kExpires1_ + base::TimeDelta::FromDays(7);

 private:
  TestReportingCacheObserver observer_;
  std::unique_ptr<MockPersistentReportingStore> store_;
};

// Note: These tests exercise both sides of the cache (reports and clients),
// aside from header parsing (i.e. OnParsedHeader(), AddOrUpdate*(),
// Remove*OtherThan() methods) which are exercised in the unittests for the
// header parser.

TEST_P(ReportingCacheTest, Reports) {
  LoadReportingClients();

  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());

  cache()->AddReport(kUrl1_, kUserAgent_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0, kNowTicks_,
                     0);
  EXPECT_EQ(1, observer()->cached_reports_update_count());

  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  const ReportingReport* report = reports[0];
  ASSERT_TRUE(report);
  EXPECT_EQ(kUrl1_, report->url);
  EXPECT_EQ(kUserAgent_, report->user_agent);
  EXPECT_EQ(kGroup1_, report->group);
  EXPECT_EQ(kType_, report->type);
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

  cache()->RemoveReports(reports, ReportingReport::Outcome::UNKNOWN);
  EXPECT_EQ(3, observer()->cached_reports_update_count());

  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_P(ReportingCacheTest, RemoveAllReports) {
  LoadReportingClients();

  cache()->AddReport(kUrl1_, kUserAgent_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0, kNowTicks_,
                     0);
  cache()->AddReport(kUrl1_, kUserAgent_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0, kNowTicks_,
                     0);
  EXPECT_EQ(2, observer()->cached_reports_update_count());

  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(2u, reports.size());

  cache()->RemoveAllReports(ReportingReport::Outcome::UNKNOWN);
  EXPECT_EQ(3, observer()->cached_reports_update_count());

  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_P(ReportingCacheTest, RemovePendingReports) {
  LoadReportingClients();

  cache()->AddReport(kUrl1_, kUserAgent_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0, kNowTicks_,
                     0);
  EXPECT_EQ(1, observer()->cached_reports_update_count());

  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_FALSE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  cache()->SetReportsPending(reports);
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  cache()->RemoveReports(reports, ReportingReport::Outcome::UNKNOWN);
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_TRUE(cache()->IsReportDoomedForTesting(reports[0]));
  EXPECT_EQ(2, observer()->cached_reports_update_count());

  // After removing report, future calls to GetReports should not return it.
  std::vector<const ReportingReport*> visible_reports;
  cache()->GetReports(&visible_reports);
  EXPECT_TRUE(visible_reports.empty());
  EXPECT_EQ(1u, cache()->GetFullReportCountForTesting());

  // After clearing pending flag, report should be deleted.
  cache()->ClearReportsPending(reports);
  EXPECT_EQ(0u, cache()->GetFullReportCountForTesting());
}

TEST_P(ReportingCacheTest, RemoveAllPendingReports) {
  LoadReportingClients();

  cache()->AddReport(kUrl1_, kUserAgent_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0, kNowTicks_,
                     0);
  EXPECT_EQ(1, observer()->cached_reports_update_count());

  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_FALSE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  cache()->SetReportsPending(reports);
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  cache()->RemoveAllReports(ReportingReport::Outcome::UNKNOWN);
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_TRUE(cache()->IsReportDoomedForTesting(reports[0]));
  EXPECT_EQ(2, observer()->cached_reports_update_count());

  // After removing report, future calls to GetReports should not return it.
  std::vector<const ReportingReport*> visible_reports;
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
      AddAndReturnReport(kUrl1_, kUserAgent_, kGroup1_, kType_,
                         std::make_unique<base::DictionaryValue>(), 0,
                         now + base::TimeDelta::FromSeconds(200), 0);
  const ReportingReport* report2 =
      AddAndReturnReport(kUrl1_, kUserAgent_, kGroup2_, kType_,
                         std::make_unique<base::DictionaryValue>(), 0,
                         now + base::TimeDelta::FromSeconds(100), 1);
  cache()->AddReport(kUrl2_, kUserAgent_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), 2,
                     now + base::TimeDelta::FromSeconds(200), 0);
  cache()->AddReport(kUrl1_, kUserAgent_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     now + base::TimeDelta::FromSeconds(300), 0);
  // Mark report1 as pending as report2 as doomed
  cache()->SetReportsPending({report1, report2});
  cache()->RemoveReports({report2}, ReportingReport::Outcome::UNKNOWN);

  base::Value actual = cache()->GetReportsAsValue();
  std::unique_ptr<base::Value> expected =
      base::test::ParseJsonDeprecated(R"json(
      [
        {
          "url": "https://origin1/path",
          "group": "group2",
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
          "type": "default",
          "status": "queued",
          "body": {},
          "attempts": 0,
          "depth": 0,
          "queued": "300000",
        },
      ]
      )json");
  EXPECT_EQ(*expected, actual);
}

TEST_P(ReportingCacheTest, Endpoints) {
  LoadReportingClients();

  EXPECT_EQ(0u, cache()->GetEndpointCount());
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_, kExpires1_));
  EXPECT_EQ(1u, cache()->GetEndpointCount());

  const ReportingEndpoint endpoint1 =
      FindEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_);
  ASSERT_TRUE(endpoint1);
  EXPECT_EQ(kOrigin1_, endpoint1.group_key.origin);
  EXPECT_EQ(kEndpoint1_, endpoint1.info.url);
  EXPECT_EQ(kGroup1_, endpoint1.group_key.group_name);

  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));

  EXPECT_TRUE(OriginClientExistsInCache(kOrigin1_));

  // Insert another endpoint in the same group.
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint2_, kExpires1_));
  EXPECT_EQ(2u, cache()->GetEndpointCount());

  const ReportingEndpoint endpoint2 =
      FindEndpointInCache(kOrigin1_, kGroup1_, kEndpoint2_);
  ASSERT_TRUE(endpoint2);
  EXPECT_EQ(kOrigin1_, endpoint2.group_key.origin);
  EXPECT_EQ(kEndpoint2_, endpoint2.info.url);
  EXPECT_EQ(kGroup1_, endpoint2.group_key.group_name);

  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());

  EXPECT_TRUE(OriginClientExistsInCache(kOrigin1_));
  std::vector<url::Origin> origins_in_cache = cache()->GetAllOrigins();
  EXPECT_EQ(1u, origins_in_cache.size());

  // Insert another endpoint for a different origin.
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup1_, kEndpoint2_, kExpires1_));
  EXPECT_EQ(3u, cache()->GetEndpointCount());

  const ReportingEndpoint endpoint3 =
      FindEndpointInCache(kOrigin2_, kGroup1_, kEndpoint2_);
  ASSERT_TRUE(endpoint3);
  EXPECT_EQ(kOrigin2_, endpoint3.group_key.origin);
  EXPECT_EQ(kEndpoint2_, endpoint3.info.url);
  EXPECT_EQ(kGroup1_, endpoint3.group_key.group_name);

  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin2_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_EQ(2u, cache()->GetEndpointGroupCountForTesting());

  EXPECT_TRUE(OriginClientExistsInCache(kOrigin2_));
  origins_in_cache = cache()->GetAllOrigins();
  EXPECT_EQ(2u, origins_in_cache.size());
}

TEST_P(ReportingCacheTest, RemoveClient) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup2_, kEndpoint2_, kExpires1_));
  EXPECT_EQ(4u, cache()->GetEndpointCount());
  ASSERT_TRUE(OriginClientExistsInCache(kOrigin1_));
  ASSERT_TRUE(OriginClientExistsInCache(kOrigin2_));

  cache()->RemoveClient(kOrigin1_);

  EXPECT_EQ(2u, cache()->GetEndpointCount());
  EXPECT_FALSE(OriginClientExistsInCache(kOrigin1_));
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin2_));

  if (store()) {
    store()->Flush();
    // SetEndpointInCache doesn't update store counts, which is why they go
    // negative here.
    // TODO(crbug.com/895821): Populate the cache via the store so we don't need
    // negative counts.
    EXPECT_EQ(-2, store()->StoredEndpointsCount());
    EXPECT_EQ(-1, store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    EXPECT_EQ(2,
              store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(1, store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
        CachedReportingEndpointGroup(
            kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT /* irrelevant */,
            base::Time() /* irrelevant */, base::Time() /* irrelevant */));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT,
        ReportingEndpoint(kOrigin1_, kGroup1_,
                          ReportingEndpoint::EndpointInfo{kEndpoint2_}));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT,
        ReportingEndpoint(kOrigin1_, kGroup1_,
                          ReportingEndpoint::EndpointInfo{kEndpoint1_}));
    EXPECT_THAT(store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingCacheTest, RemoveAllClients) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup2_, kEndpoint2_, kExpires1_));
  EXPECT_EQ(4u, cache()->GetEndpointCount());
  ASSERT_TRUE(OriginClientExistsInCache(kOrigin1_));
  ASSERT_TRUE(OriginClientExistsInCache(kOrigin2_));

  cache()->RemoveAllClients();

  EXPECT_EQ(0u, cache()->GetEndpointCount());
  EXPECT_FALSE(OriginClientExistsInCache(kOrigin1_));
  EXPECT_FALSE(OriginClientExistsInCache(kOrigin2_));

  if (store()) {
    store()->Flush();
    // SetEndpointInCache doesn't update store counts, which is why they go
    // negative here.
    // TODO(crbug.com/895821): Populate the cache via the store so we don't need
    // negative counts.
    EXPECT_EQ(-4, store()->StoredEndpointsCount());
    EXPECT_EQ(-3, store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    EXPECT_EQ(4,
              store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(3, store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT,
        ReportingEndpoint(kOrigin1_, kGroup1_,
                          ReportingEndpoint::EndpointInfo{kEndpoint1_}));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT,
        ReportingEndpoint(kOrigin1_, kGroup1_,
                          ReportingEndpoint::EndpointInfo{kEndpoint2_}));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT,
        ReportingEndpoint(kOrigin2_, kGroup1_,
                          ReportingEndpoint::EndpointInfo{kEndpoint1_}));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT,
        ReportingEndpoint(kOrigin2_, kGroup2_,
                          ReportingEndpoint::EndpointInfo{kEndpoint2_}));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
        CachedReportingEndpointGroup(
            kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT /* irrelevant */,
            base::Time() /* irrelevant */, base::Time() /* irrelevant */));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
        CachedReportingEndpointGroup(
            kOrigin2_, kGroup1_, OriginSubdomains::DEFAULT /* irrelevant */,
            base::Time() /* irrelevant */, base::Time() /* irrelevant */));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
        CachedReportingEndpointGroup(
            kOrigin2_, kGroup2_, OriginSubdomains::DEFAULT /* irrelevant */,
            base::Time() /* irrelevant */, base::Time() /* irrelevant */));
    EXPECT_THAT(store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingCacheTest, RemoveEndpointGroup) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup2_, kEndpoint2_, kExpires1_));
  EXPECT_EQ(4u, cache()->GetEndpointCount());
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin1_));
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin2_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin2_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin2_, kGroup2_, OriginSubdomains::DEFAULT, kExpires1_));

  cache()->RemoveEndpointGroup(kOrigin2_, kGroup1_);
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_FALSE(EndpointGroupExistsInCache(
      kOrigin2_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin2_, kGroup2_, OriginSubdomains::DEFAULT, kExpires1_));

  cache()->RemoveEndpointGroup(kOrigin2_, kGroup2_);
  EXPECT_FALSE(EndpointGroupExistsInCache(
      kOrigin2_, kGroup2_, OriginSubdomains::DEFAULT, kExpires1_));
  // Removal of the last group for an origin also removes the client.
  EXPECT_FALSE(OriginClientExistsInCache(kOrigin2_));
  // Other origins are not affected.
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));

  if (store()) {
    store()->Flush();
    // SetEndpointInCache doesn't update store counts, which is why they go
    // negative here.
    // TODO(crbug.com/895821): Populate the cache via the store so we don't need
    // negative counts.
    EXPECT_EQ(-2, store()->StoredEndpointsCount());
    EXPECT_EQ(-2, store()->StoredEndpointGroupsCount());
    EXPECT_EQ(2,
              store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(2, store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT,
        ReportingEndpoint(kOrigin2_, kGroup1_,
                          ReportingEndpoint::EndpointInfo{kEndpoint1_}));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT,
        ReportingEndpoint(kOrigin2_, kGroup2_,
                          ReportingEndpoint::EndpointInfo{kEndpoint2_}));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
        CachedReportingEndpointGroup(
            kOrigin2_, kGroup1_, OriginSubdomains::DEFAULT /* irrelevant */,
            base::Time() /* irrelevant */, base::Time() /* irrelevant */));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
        CachedReportingEndpointGroup(
            kOrigin2_, kGroup2_, OriginSubdomains::DEFAULT /* irrelevant */,
            base::Time() /* irrelevant */, base::Time() /* irrelevant */));
    EXPECT_THAT(store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingCacheTest, RemoveEndpointsForUrl) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup2_, kEndpoint2_, kExpires1_));
  EXPECT_EQ(4u, cache()->GetEndpointCount());
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin1_));
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin2_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin2_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin2_, kGroup2_, OriginSubdomains::DEFAULT, kExpires1_));

  cache()->RemoveEndpointsForUrl(kEndpoint1_);
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin1_));
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin2_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_FALSE(EndpointGroupExistsInCache(
      kOrigin2_, kGroup1_, OriginSubdomains::DEFAULT, kExpires1_));
  EXPECT_TRUE(EndpointGroupExistsInCache(
      kOrigin2_, kGroup2_, OriginSubdomains::DEFAULT, kExpires1_));

  EXPECT_EQ(2u, cache()->GetEndpointCount());
  EXPECT_FALSE(FindEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_));
  EXPECT_TRUE(FindEndpointInCache(kOrigin1_, kGroup1_, kEndpoint2_));
  EXPECT_FALSE(FindEndpointInCache(kOrigin2_, kGroup1_, kEndpoint1_));
  EXPECT_TRUE(FindEndpointInCache(kOrigin2_, kGroup2_, kEndpoint2_));

  if (store()) {
    store()->Flush();
    // SetEndpointInCache doesn't update store counts, which is why they go
    // negative here.
    // TODO(crbug.com/895821): Populate the cache via the store so we don't need
    // negative counts.
    EXPECT_EQ(-2, store()->StoredEndpointsCount());
    EXPECT_EQ(-1, store()->StoredEndpointGroupsCount());
    EXPECT_EQ(2,
              store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(1, store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT,
        ReportingEndpoint(kOrigin1_, kGroup1_,
                          ReportingEndpoint::EndpointInfo{kEndpoint1_}));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT,
        ReportingEndpoint(kOrigin2_, kGroup1_,
                          ReportingEndpoint::EndpointInfo{kEndpoint1_}));
    expected_commands.emplace_back(
        CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
        CachedReportingEndpointGroup(
            kOrigin2_, kGroup1_, OriginSubdomains::DEFAULT /* irrelevant */,
            base::Time() /* irrelevant */, base::Time() /* irrelevant */));
    EXPECT_THAT(store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingCacheTest, GetClientsAsValue) {
  LoadReportingClients();

  // These times are bogus but we need a reproducible expiry timestamp for this
  // test case.
  const base::TimeTicks expires_ticks =
      base::TimeTicks() + base::TimeDelta::FromDays(7);
  const base::Time expires =
      base::Time::UnixEpoch() + (expires_ticks - base::TimeTicks::UnixEpoch());
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_, expires,
                                 OriginSubdomains::EXCLUDE));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup1_, kEndpoint2_, expires,
                                 OriginSubdomains::INCLUDE));

  cache()->IncrementEndpointDeliveries(kOrigin1_, kGroup1_, kEndpoint1_,
                                       /* reports */ 2, /* succeeded */ true);
  cache()->IncrementEndpointDeliveries(kOrigin2_, kGroup1_, kEndpoint2_,
                                       /* reports */ 1, /* succeeded */ false);

  base::Value actual = cache()->GetClientsAsValue();
  std::unique_ptr<base::Value> expected =
      base::test::ParseJsonDeprecated(R"json(
      [
        {
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
      )json");

  // Compare disregarding order.
  std::vector<base::Value> expected_list = std::move(expected->GetList());
  std::vector<base::Value> actual_list = std::move(actual.GetList());
  std::sort(expected_list.begin(), expected_list.end());
  std::sort(actual_list.begin(), actual_list.end());
  EXPECT_EQ(expected_list, actual_list);
}

TEST_P(ReportingCacheTest, GetCandidateEndpointsForDelivery) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup2_, kEndpoint2_, kExpires1_));
  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(),
                                                kOrigin1_, kGroup1_);
  ASSERT_EQ(2u, candidate_endpoints.size());
  for (const ReportingEndpoint& endpoint : candidate_endpoints) {
    EXPECT_EQ(kOrigin1_, endpoint.group_key.origin);
    EXPECT_EQ(kGroup1_, endpoint.group_key.group_name);
  }

  candidate_endpoints = cache()->GetCandidateEndpointsForDelivery(
      NetworkIsolationKey(), kOrigin2_, kGroup1_);
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kOrigin2_, candidate_endpoints[0].group_key.origin);
  EXPECT_EQ(kGroup1_, candidate_endpoints[0].group_key.group_name);
}

TEST_P(ReportingCacheTest, GetCandidateEndpointsExcludesExpired) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint2_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup1_, kEndpoint1_, kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin2_, kGroup2_, kEndpoint2_, kExpires2_));
  // Make kExpires1_ expired but not kExpires2_.
  clock()->Advance(base::TimeDelta::FromDays(8));
  ASSERT_GT(clock()->Now(), kExpires1_);
  ASSERT_LT(clock()->Now(), kExpires2_);

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(),
                                                kOrigin1_, kGroup1_);
  ASSERT_EQ(0u, candidate_endpoints.size());

  candidate_endpoints = cache()->GetCandidateEndpointsForDelivery(
      NetworkIsolationKey(), kOrigin2_, kGroup1_);
  ASSERT_EQ(0u, candidate_endpoints.size());

  candidate_endpoints = cache()->GetCandidateEndpointsForDelivery(
      NetworkIsolationKey(), kOrigin2_, kGroup2_);
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kEndpoint2_, candidate_endpoints[0].info.url);
}

TEST_P(ReportingCacheTest, ExcludeSubdomainsDifferentPort) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://example/"));
  const url::Origin kDifferentPortOrigin =
      url::Origin::Create(GURL("https://example:444/"));

  ASSERT_TRUE(SetEndpointInCache(kDifferentPortOrigin, kGroup1_, kEndpoint1_,
                                 kExpires1_, OriginSubdomains::EXCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(), kOrigin,
                                                kGroup1_);
  ASSERT_EQ(0u, candidate_endpoints.size());
}

TEST_P(ReportingCacheTest, ExcludeSubdomainsSuperdomain) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.example/"));
  const url::Origin kSuperOrigin =
      url::Origin::Create(GURL("https://example/"));

  ASSERT_TRUE(SetEndpointInCache(kSuperOrigin, kGroup1_, kEndpoint1_,
                                 kExpires1_, OriginSubdomains::EXCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(), kOrigin,
                                                kGroup1_);
  ASSERT_EQ(0u, candidate_endpoints.size());
}

TEST_P(ReportingCacheTest, IncludeSubdomainsDifferentPort) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://example/"));
  const url::Origin kDifferentPortOrigin =
      url::Origin::Create(GURL("https://example:444/"));

  ASSERT_TRUE(SetEndpointInCache(kDifferentPortOrigin, kGroup1_, kEndpoint1_,
                                 kExpires1_, OriginSubdomains::INCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(), kOrigin,
                                                kGroup1_);
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kDifferentPortOrigin, candidate_endpoints[0].group_key.origin);
}

TEST_P(ReportingCacheTest, IncludeSubdomainsSuperdomain) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.example/"));
  const url::Origin kSuperOrigin =
      url::Origin::Create(GURL("https://example/"));

  ASSERT_TRUE(SetEndpointInCache(kSuperOrigin, kGroup1_, kEndpoint1_,
                                 kExpires1_, OriginSubdomains::INCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(), kOrigin,
                                                kGroup1_);
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kSuperOrigin, candidate_endpoints[0].group_key.origin);
}

TEST_P(ReportingCacheTest, IncludeSubdomainsPreferOriginToDifferentPort) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.example/"));
  const url::Origin kDifferentPortOrigin =
      url::Origin::Create(GURL("https://example:444/"));

  ASSERT_TRUE(SetEndpointInCache(kOrigin, kGroup1_, kEndpoint1_, kExpires1_,
                                 OriginSubdomains::INCLUDE));
  ASSERT_TRUE(SetEndpointInCache(kDifferentPortOrigin, kGroup1_, kEndpoint1_,
                                 kExpires1_, OriginSubdomains::INCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(), kOrigin,
                                                kGroup1_);
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kOrigin, candidate_endpoints[0].group_key.origin);
}

TEST_P(ReportingCacheTest, IncludeSubdomainsPreferOriginToSuperdomain) {
  LoadReportingClients();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.example/"));
  const url::Origin kSuperOrigin =
      url::Origin::Create(GURL("https://example/"));

  ASSERT_TRUE(SetEndpointInCache(kOrigin, kGroup1_, kEndpoint1_, kExpires1_,
                                 OriginSubdomains::INCLUDE));
  ASSERT_TRUE(SetEndpointInCache(kSuperOrigin, kGroup1_, kEndpoint1_,
                                 kExpires1_, OriginSubdomains::INCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(), kOrigin,
                                                kGroup1_);
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

  ASSERT_TRUE(SetEndpointInCache(kSuperOrigin, kGroup1_, kEndpoint1_,
                                 kExpires1_, OriginSubdomains::INCLUDE));
  ASSERT_TRUE(SetEndpointInCache(kSuperSuperOrigin, kGroup1_, kEndpoint1_,
                                 kExpires1_, OriginSubdomains::INCLUDE));

  std::vector<ReportingEndpoint> candidate_endpoints =
      cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(), kOrigin,
                                                kGroup1_);
  ASSERT_EQ(1u, candidate_endpoints.size());
  EXPECT_EQ(kSuperOrigin, candidate_endpoints[0].group_key.origin);
}

TEST_P(ReportingCacheTest, EvictOldestReport) {
  LoadReportingClients();

  size_t max_report_count = policy().max_report_count;

  ASSERT_LT(0u, max_report_count);
  ASSERT_GT(std::numeric_limits<size_t>::max(), max_report_count);

  base::TimeTicks earliest_queued = tick_clock()->NowTicks();

  // Enqueue the maximum number of reports, spaced apart in time.
  for (size_t i = 0; i < max_report_count; ++i) {
    cache()->AddReport(kUrl1_, kUserAgent_, kGroup1_, kType_,
                       std::make_unique<base::DictionaryValue>(), 0,
                       tick_clock()->NowTicks(), 0);
    tick_clock()->Advance(base::TimeDelta::FromMinutes(1));
  }
  EXPECT_EQ(max_report_count, report_count());

  // Add one more report to force the cache to evict one.
  cache()->AddReport(kUrl1_, kUserAgent_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  // Make sure the cache evicted a report to make room for the new one, and make
  // sure the report evicted was the earliest-queued one.
  std::vector<const ReportingReport*> reports;
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
  for (size_t i = 0; i < max_report_count; ++i) {
    cache()->AddReport(kUrl1_, kUserAgent_, kGroup1_, kType_,
                       std::make_unique<base::DictionaryValue>(), 0,
                       tick_clock()->NowTicks(), 0);
    tick_clock()->Advance(base::TimeDelta::FromMinutes(1));
  }
  EXPECT_EQ(max_report_count, report_count());

  // Mark all of the queued reports pending.
  std::vector<const ReportingReport*> queued_reports;
  cache()->GetReports(&queued_reports);
  cache()->SetReportsPending(queued_reports);

  // Add one more report to force the cache to evict one. Since the cache has
  // only pending reports, it will be forced to evict the *new* report!
  cache()->AddReport(kUrl1_, kUserAgent_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0, kNowTicks_,
                     0);

  // Make sure the cache evicted a report, and make sure the report evicted was
  // the new, non-pending one.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(max_report_count, reports.size());
  for (const ReportingReport* report : reports)
    EXPECT_TRUE(cache()->IsReportPendingForTesting(report));
}

TEST_P(ReportingCacheTest, EvictEndpointsOverPerOriginLimit) {
  LoadReportingClients();

  for (size_t i = 0; i < policy().max_endpoints_per_origin; ++i) {
    ASSERT_TRUE(
        SetEndpointInCache(kOrigin1_, kGroup1_, MakeURL(i), kExpires1_));
    EXPECT_EQ(i + 1, cache()->GetEndpointCount());
  }
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());
  // Insert one more endpoint; eviction should be triggered.
  SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_, kExpires1_);
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());
}

TEST_P(ReportingCacheTest, EvictExpiredGroups) {
  LoadReportingClients();

  for (size_t i = 0; i < policy().max_endpoints_per_origin; ++i) {
    ASSERT_TRUE(
        SetEndpointInCache(kOrigin1_, kGroup1_, MakeURL(i), kExpires1_));
    EXPECT_EQ(i + 1, cache()->GetEndpointCount());
  }
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  // Make the group expired (but not stale).
  clock()->SetNow(kExpires1_ - base::TimeDelta::FromMinutes(1));
  cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(), kOrigin1_,
                                            kGroup1_);
  clock()->SetNow(kExpires1_ + base::TimeDelta::FromMinutes(1));

  // Insert one more endpoint in a different group (not expired); eviction
  // should be triggered and the expired group should be deleted.
  SetEndpointInCache(kOrigin1_, kGroup2_, kEndpoint1_, kExpires2_);
  EXPECT_GE(policy().max_endpoints_per_origin, cache()->GetEndpointCount());
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin1_));
  EXPECT_FALSE(EndpointGroupExistsInCache(kOrigin1_, kGroup1_,
                                          OriginSubdomains::DEFAULT));
  EXPECT_TRUE(EndpointGroupExistsInCache(kOrigin1_, kGroup2_,
                                         OriginSubdomains::DEFAULT));
}

TEST_P(ReportingCacheTest, EvictStaleGroups) {
  LoadReportingClients();

  for (size_t i = 0; i < policy().max_endpoints_per_origin; ++i) {
    ASSERT_TRUE(
        SetEndpointInCache(kOrigin1_, kGroup1_, MakeURL(i), kExpires1_));
    EXPECT_EQ(i + 1, cache()->GetEndpointCount());
  }
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  // Make the group stale (but not expired).
  clock()->Advance(2 * policy().max_group_staleness);
  ASSERT_LT(clock()->Now(), kExpires1_);

  // Insert one more endpoint in a different group; eviction should be
  // triggered and the stale group should be deleted.
  SetEndpointInCache(kOrigin1_, kGroup2_, kEndpoint1_, kExpires1_);
  EXPECT_GE(policy().max_endpoints_per_origin, cache()->GetEndpointCount());
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin1_));
  EXPECT_FALSE(EndpointGroupExistsInCache(kOrigin1_, kGroup1_,
                                          OriginSubdomains::DEFAULT));
  EXPECT_TRUE(EndpointGroupExistsInCache(kOrigin1_, kGroup2_,
                                         OriginSubdomains::DEFAULT));
}

TEST_P(ReportingCacheTest, EvictFromStalestGroup) {
  LoadReportingClients();

  for (size_t i = 0; i < policy().max_endpoints_per_origin; ++i) {
    ASSERT_TRUE(SetEndpointInCache(kOrigin1_, base::NumberToString(i),
                                   MakeURL(i), kExpires1_));
    EXPECT_EQ(i + 1, cache()->GetEndpointCount());
    EXPECT_TRUE(EndpointGroupExistsInCache(kOrigin1_, base::NumberToString(i),
                                           OriginSubdomains::DEFAULT));
    // Mark group used.
    cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(), kOrigin1_,
                                              base::NumberToString(i));
    clock()->Advance(base::TimeDelta::FromMinutes(1));
  }
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  // Insert one more endpoint in a different group; eviction should be
  // triggered and (only) the stalest group should be evicted from (and in this
  // case deleted).
  SetEndpointInCache(kOrigin1_, kGroup2_, kEndpoint1_, kExpires1_);
  EXPECT_GE(policy().max_endpoints_per_origin, cache()->GetEndpointCount());
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin1_));
  EXPECT_FALSE(
      EndpointGroupExistsInCache(kOrigin1_, "0", OriginSubdomains::DEFAULT));
  EXPECT_TRUE(EndpointGroupExistsInCache(kOrigin1_, kGroup2_,
                                         OriginSubdomains::DEFAULT));
  for (size_t i = 1; i < policy().max_endpoints_per_origin; ++i) {
    EXPECT_TRUE(EndpointGroupExistsInCache(kOrigin1_, base::NumberToString(i),
                                           OriginSubdomains::DEFAULT));
  }
}

TEST_P(ReportingCacheTest, EvictFromLargestGroup) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, MakeURL(0), kExpires1_));
  // This group should be evicted from because it has 2 endpoints.
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup2_, MakeURL(1), kExpires1_));
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup2_, MakeURL(2), kExpires1_));

  // max_endpoints_per_origin is set to 3.
  ASSERT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  // Insert one more endpoint in a different group; eviction should be
  // triggered.
  SetEndpointInCache(kOrigin1_, "default", kEndpoint1_, kExpires1_);
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  EXPECT_TRUE(EndpointGroupExistsInCache(kOrigin1_, kGroup1_,
                                         OriginSubdomains::DEFAULT));
  EXPECT_TRUE(EndpointGroupExistsInCache(kOrigin1_, kGroup2_,
                                         OriginSubdomains::DEFAULT));
  // Count the number of endpoints remaining in kGroup2_.
  std::vector<ReportingEndpoint> endpoints_in_group =
      cache()->GetCandidateEndpointsForDelivery(NetworkIsolationKey(),
                                                kOrigin1_, kGroup2_);
  EXPECT_EQ(1u, endpoints_in_group.size());
}

TEST_P(ReportingCacheTest, EvictLeastImportantEndpoint) {
  LoadReportingClients();

  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, MakeURL(0), kExpires1_,
                                 OriginSubdomains::DEFAULT, 1 /* priority*/,
                                 1 /* weight */));
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, MakeURL(1), kExpires1_,
                                 OriginSubdomains::DEFAULT, 2 /* priority */,
                                 2 /* weight */));
  // This endpoint will be evicted because it is lowest priority and lowest
  // weight.
  ASSERT_TRUE(SetEndpointInCache(kOrigin1_, kGroup1_, MakeURL(2), kExpires1_,
                                 OriginSubdomains::DEFAULT, 2 /* priority */,
                                 1 /* weight */));

  // max_endpoints_per_origin is set to 3.
  ASSERT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  // Insert one more endpoint in a different group; eviction should be
  // triggered and the least important endpoint should be deleted.
  SetEndpointInCache(kOrigin1_, kGroup2_, kEndpoint1_, kExpires1_);
  EXPECT_EQ(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  EXPECT_TRUE(FindEndpointInCache(kOrigin1_, kGroup1_, MakeURL(0)));
  EXPECT_TRUE(FindEndpointInCache(kOrigin1_, kGroup1_, MakeURL(1)));
  EXPECT_FALSE(FindEndpointInCache(kOrigin1_, kGroup1_, MakeURL(2)));
  EXPECT_TRUE(FindEndpointInCache(kOrigin1_, kGroup2_, kEndpoint1_));
}

TEST_P(ReportingCacheTest, EvictEndpointsOverGlobalLimitFromStalestClient) {
  LoadReportingClients();

  // Set enough endpoints to reach the global endpoint limit.
  for (size_t i = 0; i < policy().max_endpoint_count; ++i) {
    ASSERT_TRUE(SetEndpointInCache(url::Origin::Create(MakeURL(i)), kGroup1_,
                                   MakeURL(i), kExpires1_));
    EXPECT_EQ(i + 1, cache()->GetEndpointCount());
    clock()->Advance(base::TimeDelta::FromMinutes(1));
  }
  EXPECT_EQ(policy().max_endpoint_count, cache()->GetEndpointCount());

  // Insert one more endpoint for a different origin; eviction should be
  // triggered and the stalest client should be evicted from (and in this case
  // deleted).
  SetEndpointInCache(kOrigin1_, kGroup1_, kEndpoint1_, kExpires1_);
  EXPECT_EQ(policy().max_endpoint_count, cache()->GetEndpointCount());
  EXPECT_FALSE(OriginClientExistsInCache(url::Origin::Create(MakeURL(0))));
  for (size_t i = 1; i < policy().max_endpoint_count; ++i) {
    EXPECT_TRUE(OriginClientExistsInCache(url::Origin::Create(MakeURL(i))));
  }
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin1_));
}

TEST_P(ReportingCacheTest, AddClientsLoadedFromStore) {
  if (!store())
    return;

  base::Time now = clock()->Now();

  std::vector<ReportingEndpoint> endpoints;
  endpoints.emplace_back(kOrigin1_, kGroup1_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kOrigin2_, kGroup2_,
                         ReportingEndpoint::EndpointInfo{kEndpoint2_});
  endpoints.emplace_back(kOrigin1_, kGroup1_,
                         ReportingEndpoint::EndpointInfo{kEndpoint2_});
  endpoints.emplace_back(kOrigin2_, kGroup1_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  std::vector<CachedReportingEndpointGroup> groups;
  groups.emplace_back(kOrigin2_, kGroup1_, OriginSubdomains::DEFAULT,
                      now + base::TimeDelta::FromMinutes(2) /* expires */,
                      now /* last_used */);
  groups.emplace_back(kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT,
                      now + base::TimeDelta::FromMinutes(1) /* expires */,
                      now /* last_used */);
  groups.emplace_back(kOrigin2_, kGroup2_, OriginSubdomains::DEFAULT,
                      now + base::TimeDelta::FromMinutes(3) /* expires */,
                      now /* last_used */);
  store()->SetPrestoredClients(endpoints, groups);

  LoadReportingClients();

  EXPECT_EQ(4u, cache()->GetEndpointCount());
  EXPECT_EQ(3u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(EndpointExistsInCache(kOrigin1_, kGroup1_, kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(kOrigin1_, kGroup1_, kEndpoint2_));
  EXPECT_TRUE(EndpointExistsInCache(kOrigin2_, kGroup1_, kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(kOrigin2_, kGroup2_, kEndpoint2_));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT,
                                 now + base::TimeDelta::FromMinutes(1)));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kOrigin2_, kGroup1_, OriginSubdomains::DEFAULT,
                                 now + base::TimeDelta::FromMinutes(2)));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kOrigin2_, kGroup2_, OriginSubdomains::DEFAULT,
                                 now + base::TimeDelta::FromMinutes(3)));
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin1_));
  EXPECT_TRUE(OriginClientExistsInCache(kOrigin2_));
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
  endpoints.emplace_back(kOrigin1_, kGroup1_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kOrigin1_, kGroup1_,
                         ReportingEndpoint::EndpointInfo{kEndpoint2_});
  endpoints.emplace_back(kOrigin1_, kGroup1_,
                         ReportingEndpoint::EndpointInfo{kEndpoint3_});
  endpoints.emplace_back(kOrigin1_, kGroup1_,
                         ReportingEndpoint::EndpointInfo{kEndpoint4_});
  endpoints.emplace_back(kOrigin2_, kGroup2_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kOrigin2_, kGroup2_,
                         ReportingEndpoint::EndpointInfo{kEndpoint2_});
  endpoints.emplace_back(kOrigin2_, kGroup2_,
                         ReportingEndpoint::EndpointInfo{kEndpoint3_});
  endpoints.emplace_back(kOrigin2_, kGroup2_,
                         ReportingEndpoint::EndpointInfo{kEndpoint4_});
  std::vector<CachedReportingEndpointGroup> groups;
  groups.emplace_back(kOrigin1_, kGroup1_, OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  groups.emplace_back(kOrigin2_, kGroup2_, OriginSubdomains::DEFAULT,
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
  endpoints.emplace_back(kOrigin1_, kGroup1_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kOrigin2_, kGroup1_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  // This endpoint has no corresponding endpoint group
  endpoints.emplace_back(kOrigin2_, kGroup2_,
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  std::vector<CachedReportingEndpointGroup> groups;
  // This endpoint group has no corresponding endpoint
  groups.emplace_back(kOrigin1_, kGroup2_, OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  groups.emplace_back(kOrigin2_, kGroup1_, OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  // This endpoint group has no corresponding endpoint
  groups.emplace_back(kOrigin2_, "last_group", OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  store()->SetPrestoredClients(endpoints, groups);

  LoadReportingClients();

  EXPECT_GE(1u, cache()->GetEndpointCount());
  EXPECT_GE(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(EndpointExistsInCache(kOrigin2_, kGroup1_, kEndpoint1_));
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
  endpoints.emplace_back(kOrigin1_, "1",
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kOrigin1_, "2",
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kOrigin1_, "3",
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  endpoints.emplace_back(kOrigin1_, "4",
                         ReportingEndpoint::EndpointInfo{kEndpoint1_});
  std::vector<CachedReportingEndpointGroup> groups;
  groups.emplace_back(kOrigin1_, "1", OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  groups.emplace_back(kOrigin1_, "2", OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  // Stale last_used on group "3" should cause us to select it for eviction
  groups.emplace_back(kOrigin1_, "3", OriginSubdomains::DEFAULT,
                      now /* expires */, base::Time() /* last_used */);
  groups.emplace_back(kOrigin1_, "4", OriginSubdomains::DEFAULT,
                      now /* expires */, now /* last_used */);
  store()->SetPrestoredClients(endpoints, groups);

  LoadReportingClients();

  EXPECT_TRUE(EndpointExistsInCache(kOrigin1_, "1", kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(kOrigin1_, "2", kEndpoint1_));
  EXPECT_FALSE(EndpointExistsInCache(kOrigin1_, "3", kEndpoint1_));
  EXPECT_TRUE(EndpointExistsInCache(kOrigin1_, "4", kEndpoint1_));
}

INSTANTIATE_TEST_SUITE_P(ReportingCacheStoreTest,
                         ReportingCacheTest,
                         testing::Bool());

}  // namespace
}  // namespace net
