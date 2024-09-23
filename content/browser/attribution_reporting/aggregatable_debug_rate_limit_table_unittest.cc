// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_debug_rate_limit_table.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/aggregatable_debug_report.h"
#include "content/browser/attribution_reporting/test/configurable_storage_delegate.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"

namespace content {
namespace {

using ::attribution_reporting::SuitableOrigin;

using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

struct RateLimitData {
  std::string context_site;
  std::string reporting_origin;
  std::string reporting_site;
  base::Time time;
  int consumed_budget;

  friend bool operator==(const RateLimitData&, const RateLimitData&) = default;
};

AggregatableDebugReport CreateAggregatableDebugReport(
    const std::string& context_site,
    std::string_view reporting_origin,
    base::Time time,
    int consumed_budget) {
  return AggregatableDebugReport::CreateForTesting(
      /*contributions=*/{blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/consumed_budget, /*filtering_id=*/std::nullopt)},
      net::SchemefulSite::Deserialize(context_site),
      *SuitableOrigin::Deserialize(reporting_origin),
      /*effective_destination=*/
      net::SchemefulSite::Deserialize("https://d.test"),
      /*aggregation_coordinator_origin=*/std::nullopt, time);
}

class AggregatableDebugRateLimitTableTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(db_.OpenInMemory());
    ASSERT_TRUE(table_.CreateTable(&db_));

    // Prevent any rows from being deleted during the test by default.
    delegate_.set_delete_expired_rate_limits_frequency(base::TimeDelta::Max());
  }

  std::vector<RateLimitData> GetRateLimitRows() {
    std::vector<RateLimitData> rows;

    static constexpr char kSelectSql[] =
        "SELECT context_site,reporting_origin,reporting_site,time,"
        "consumed_budget FROM aggregatable_debug_rate_limits";
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));

    while (statement.Step()) {
      rows.emplace_back(/*context_site=*/statement.ColumnString(0),
                        /*reporting_origin=*/statement.ColumnString(1),
                        /*reporting_site=*/statement.ColumnString(2),
                        /*time=*/statement.ColumnTime(3),
                        /*consumed_budget=*/statement.ColumnInt(4));
    }

    EXPECT_TRUE(statement.Succeeded());
    return rows;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  sql::Database db_;
  ConfigurableStorageDelegate delegate_;
  AggregatableDebugRateLimitTable table_{&delegate_};
};

TEST_F(AggregatableDebugRateLimitTableTest, AddRateLimit) {
  base::Time now = base::Time::Now();

  ASSERT_TRUE(
      table_.AddRateLimit(&db_, CreateAggregatableDebugReport(
                                    /*context_site=*/"https://c.test",
                                    /*reporting_origin=*/"https://a.r.test",
                                    now, /*consumed_budget=*/123)));

  EXPECT_THAT(GetRateLimitRows(), UnorderedElementsAre(RateLimitData(
                                      /*context_site=*/"https://c.test",
                                      /*reporting_origin=*/"https://a.r.test",
                                      /*reporting_site=*/"https://r.test", now,
                                      /*consumed_budget=*/123)));
}

TEST_F(AggregatableDebugRateLimitTableTest,
       AllowedForRateLimitPerContextReportingSite) {
  base::Time now = base::Time::Now();

  ASSERT_TRUE(table_.AddRateLimit(
      &db_, CreateAggregatableDebugReport(/*context_site=*/"https://c.test",
                                          /*reporting_origin=*/"https://r.test",
                                          now, /*consumed_budget=*/65536)));

  EXPECT_EQ(table_.AllowedForRateLimit(
                &db_, CreateAggregatableDebugReport(
                          /*context_site=*/"https://c.test",
                          /*reporting_origin=*/"https://r.test", now,
                          /*consumed_budget=*/1)),
            AggregatableDebugRateLimitTable::Result::kHitReportingLimit);

  // Different reporting origin but same reporting site, not allowed.
  EXPECT_EQ(table_.AllowedForRateLimit(
                &db_, CreateAggregatableDebugReport(
                          /*context_site=*/"https://c.test",
                          /*reporting_origin=*/"https://a.r.test", now,
                          /*consumed_budget=*/1)),
            AggregatableDebugRateLimitTable::Result::kHitReportingLimit);

  // Different reporting site, allowed.
  EXPECT_EQ(table_.AllowedForRateLimit(
                &db_, CreateAggregatableDebugReport(
                          /*context_site=*/"https://c.test",
                          /*reporting_origin=*/"https://r1.test", now,
                          /*consumed_budget=*/1)),
            AggregatableDebugRateLimitTable::Result::kAllowed);

  // Different context origin but same context site, not allowed.
  EXPECT_EQ(table_.AllowedForRateLimit(
                &db_, CreateAggregatableDebugReport(
                          /*context_site=*/"https://a.c.test",
                          /*reporting_origin=*/"https://r.test", now,
                          /*consumed_budget=*/1)),
            AggregatableDebugRateLimitTable::Result::kHitReportingLimit);

  // Different context site, allowed.
  EXPECT_EQ(table_.AllowedForRateLimit(
                &db_, CreateAggregatableDebugReport(
                          /*context_site=*/"https://c1.test",
                          /*reporting_origin=*/"https://r.test", now,
                          /*consumed_budget=*/1)),
            AggregatableDebugRateLimitTable::Result::kAllowed);

  // The original row has fallen out of the time window.
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(
      table_.AllowedForRateLimit(
          &db_, CreateAggregatableDebugReport(
                    /*context_site=*/"https://c.test",
                    /*reporting_origin=*/"https://r.test", base::Time::Now(),
                    /*consumed_budget=*/1)),
      AggregatableDebugRateLimitTable::Result::kAllowed);
}

TEST_F(AggregatableDebugRateLimitTableTest, AllowedForRateLimitPerContextSite) {
  delegate_.set_aggregatable_debug_rate_limit({
      .max_budget_per_context_site = 65536,
  });

  base::Time now = base::Time::Now();

  ASSERT_TRUE(table_.AddRateLimit(
      &db_, CreateAggregatableDebugReport(/*context_site=*/"https://c.test",
                                          /*reporting_origin=*/"https://r.test",
                                          now, /*consumed_budget=*/65536)));

  EXPECT_EQ(table_.AllowedForRateLimit(
                &db_, CreateAggregatableDebugReport(
                          /*context_site=*/"https://c.test",
                          /*reporting_origin=*/"https://r.test", now,
                          /*consumed_budget=*/1)),
            AggregatableDebugRateLimitTable::Result::kHitBothLimits);

  EXPECT_EQ(table_.AllowedForRateLimit(
                &db_, CreateAggregatableDebugReport(
                          /*context_site=*/"https://c.test",
                          /*reporting_origin=*/"https://r1.test", now,
                          /*consumed_budget=*/1)),
            AggregatableDebugRateLimitTable::Result::kHitGlobalLimit);

  // Different context origin but same context site, not allowed.
  EXPECT_EQ(table_.AllowedForRateLimit(
                &db_, CreateAggregatableDebugReport(
                          /*context_site=*/"https://a.c.test",
                          /*reporting_origin=*/"https://r1.test", now,
                          /*consumed_budget=*/1)),
            AggregatableDebugRateLimitTable::Result::kHitGlobalLimit);

  // Different context site, allowed.
  EXPECT_EQ(table_.AllowedForRateLimit(
                &db_, CreateAggregatableDebugReport(
                          /*context_site=*/"https://c1.test",
                          /*reporting_origin=*/"https://r1.test", now,
                          /*consumed_budget=*/1)),
            AggregatableDebugRateLimitTable::Result::kAllowed);

  // The original row has fallen out of the time window.
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(
      table_.AllowedForRateLimit(
          &db_, CreateAggregatableDebugReport(
                    /*context_site=*/"https://c.test",
                    /*reporting_origin=*/"https://r1.test", base::Time::Now(),
                    /*consumed_budget=*/1)),
      AggregatableDebugRateLimitTable::Result::kAllowed);
}

TEST_F(AggregatableDebugRateLimitTableTest, AllowedForRateLimit_Overflow) {
  const auto create_report = []() {
    return CreateAggregatableDebugReport(
        /*context_site=*/"https://c.test",
        /*reporting_origin=*/"https://r.test", base::Time::Now(),
        /*consumed_budget=*/65536);
  };

  for (int i = 0; i <= std::numeric_limits<int>::max() / 65536; i++) {
    ASSERT_TRUE(table_.AddRateLimit(&db_, create_report()));
  }

  EXPECT_EQ(table_.AllowedForRateLimit(&db_, create_report()),
            AggregatableDebugRateLimitTable::Result::kHitBothLimits);
}

TEST_F(AggregatableDebugRateLimitTableTest, ClearAllDataAllTime) {
  base::Time now = base::Time::Now();

  ASSERT_TRUE(
      table_.AddRateLimit(&db_, CreateAggregatableDebugReport(
                                    /*context_site=*/"https://c1.test",
                                    /*reporting_origin=*/"https://r1.test", now,
                                    /*consumed_budget=*/123)));
  ASSERT_TRUE(table_.AddRateLimit(
      &db_, CreateAggregatableDebugReport(
                /*context_site=*/"https://c2.test",
                /*reporting_origin=*/"https://r2.test", now - base::Days(1),
                /*consumed_budget=*/456)));

  ASSERT_THAT(GetRateLimitRows(), SizeIs(2));

  ASSERT_TRUE(table_.ClearAllDataAllTime(&db_));
  EXPECT_THAT(GetRateLimitRows(), IsEmpty());
}

TEST_F(AggregatableDebugRateLimitTableTest, ClearAllDataInRange) {
  base::Time now = base::Time::Now();

  ASSERT_TRUE(
      table_.AddRateLimit(&db_, CreateAggregatableDebugReport(
                                    /*context_site=*/"https://c1.test",
                                    /*reporting_origin=*/"https://r1.test", now,
                                    /*consumed_budget=*/123)));
  ASSERT_TRUE(table_.AddRateLimit(
      &db_, CreateAggregatableDebugReport(
                /*context_site=*/"https://c2.test",
                /*reporting_origin=*/"https://r2.test", now - base::Days(1),
                /*consumed_budget=*/456)));

  ASSERT_THAT(GetRateLimitRows(), SizeIs(2));

  ASSERT_TRUE(table_.ClearDataForOriginsInRange(
      &db_, /*delete_begin=*/now, /*delete_end=*/base::Time::Max(),
      /*filter=*/base::NullCallback()));
  EXPECT_THAT(GetRateLimitRows(),
              UnorderedElementsAre(
                  Field(&RateLimitData::context_site, "https://c2.test")));
}

TEST_F(AggregatableDebugRateLimitTableTest, ClearDataForOriginsInRange) {
  base::Time now = base::Time::Now();

  ASSERT_TRUE(table_.AddRateLimit(
      &db_, CreateAggregatableDebugReport(
                /*context_site=*/"https://c1.test",
                /*reporting_origin=*/"https://a.r.test", now,
                /*consumed_budget=*/123)));
  ASSERT_TRUE(
      table_.AddRateLimit(&db_, CreateAggregatableDebugReport(
                                    /*context_site=*/"https://c2.test",
                                    /*reporting_origin=*/"https://r.test", now,
                                    /*consumed_budget=*/321)));
  ASSERT_TRUE(table_.AddRateLimit(
      &db_, CreateAggregatableDebugReport(
                /*context_site=*/"https://c3.test",
                /*reporting_origin=*/"https://r.test", now - base::Days(1),
                /*consumed_budget=*/456)));

  ASSERT_THAT(GetRateLimitRows(), SizeIs(3));

  ASSERT_TRUE(table_.ClearDataForOriginsInRange(
      &db_, /*delete_begin=*/now, /*delete_end=*/base::Time::Max(),
      /*filter=*/base::BindRepeating([](const blink::StorageKey& storage_key) {
        return storage_key ==
               blink::StorageKey::CreateFromStringForTesting("https://r.test");
      })));
  EXPECT_THAT(GetRateLimitRows(),
              UnorderedElementsAre(
                  Field(&RateLimitData::context_site, "https://c1.test"),
                  Field(&RateLimitData::context_site, "https://c3.test")));
}

TEST_F(AggregatableDebugRateLimitTableTest, DeleteExpiredRows) {
  delegate_.set_delete_expired_rate_limits_frequency(base::Hours(12));

  ASSERT_TRUE(table_.AddRateLimit(
      &db_, CreateAggregatableDebugReport(
                /*context_site=*/"https://c1.test",
                /*reporting_origin=*/"https://r1.test", base::Time::Now(),
                /*consumed_budget=*/1)));

  task_environment_.FastForwardBy(base::Hours(12));

  // No record has expired yet.
  ASSERT_TRUE(table_.AddRateLimit(
      &db_, CreateAggregatableDebugReport(
                /*context_site=*/"https://c2.test",
                /*reporting_origin=*/"https://r2.test", base::Time::Now(),
                /*consumed_budget=*/2)));

  task_environment_.FastForwardBy(base::Hours(12));

  // This should delete the first record.
  ASSERT_TRUE(table_.AddRateLimit(
      &db_, CreateAggregatableDebugReport(
                /*context_site=*/"https://c3.test",
                /*reporting_origin=*/"https://r3.test", base::Time::Now(),
                /*consumed_budget=*/3)));

  EXPECT_THAT(GetRateLimitRows(),
              UnorderedElementsAre(
                  Field(&RateLimitData::context_site, "https://c2.test"),
                  Field(&RateLimitData::context_site, "https://c3.test")));
}

}  // namespace
}  // namespace content
