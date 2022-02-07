// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/rate_limit_table.h"

#include <stdint.h>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using AttributionAllowedStatus =
    ::content::RateLimitTable::AttributionAllowedStatus;

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;

struct RateLimitRow {
  std::string impression_origin;
  std::string conversion_origin;
  std::string reporting_origin;
  base::Time time;

  AttributionReport BuildReport() const {
    auto source =
        SourceBuilder()
            .SetImpressionOrigin(url::Origin::Create(GURL(impression_origin)))
            .SetConversionOrigin(url::Origin::Create(GURL(conversion_origin)))
            .SetReportingOrigin(url::Origin::Create(GURL(reporting_origin)))
            .BuildStored();

    return ReportBuilder(std::move(source)).SetTriggerTime(time).Build();
  }
};

bool operator==(const RateLimitRow& a, const RateLimitRow& b) {
  const auto tie = [](const RateLimitRow& row) {
    return std::make_tuple(row.impression_origin, row.conversion_origin,
                           row.reporting_origin, row.time);
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out, const RateLimitRow& row) {
  return out << "{" << row.impression_origin << "," << row.conversion_origin
             << "," << row.reporting_origin << "," << row.time << "}";
}

class RateLimitTableTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(db_.OpenInMemory());
    ASSERT_TRUE(table_.CreateTable(&db_));

    // Prevent any rows from being deleted during the test by default.
    delegate_.set_delete_expired_rate_limits_frequency(base::TimeDelta::Max());
  }

  base::flat_map<int64_t, RateLimitRow> GetRateLimitRows() {
    std::vector<std::pair<int64_t, RateLimitRow>> rows;

    static constexpr char kSelectSql[] =
        "SELECT rate_limit_id,impression_origin,conversion_origin,"
        "reporting_origin,conversion_time FROM rate_limits";
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));

    while (statement.Step()) {
      rows.emplace_back(statement.ColumnInt64(0),
                        RateLimitRow{
                            .impression_origin = statement.ColumnString(1),
                            .conversion_origin = statement.ColumnString(2),
                            .reporting_origin = statement.ColumnString(3),
                            .time = statement.ColumnTime(4),
                        });
    }

    EXPECT_TRUE(statement.Succeeded());
    return base::flat_map<int64_t, RateLimitRow>(std::move(rows));
  }

  [[nodiscard]] bool AddRateLimit(const AttributionReport& report) {
    return table_.AddRateLimit(&db_, report);
  }

  [[nodiscard]] RateLimitTable::AttributionAllowedStatus AttributionAllowed(
      const AttributionReport& report) {
    return table_.AttributionAllowed(&db_, report);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  sql::Database db_;
  ConfigurableStorageDelegate delegate_;
  RateLimitTable table_{&delegate_};
};

}  // namespace

TEST_F(
    RateLimitTableTest,
    AttributionAllowed_ScopedToSourceSiteDestinationSiteReportingOriginTimeWindow) {
  constexpr base::TimeDelta kTimeWindow = base::Days(1);
  delegate_.set_rate_limits({
      .time_window = kTimeWindow,
      .max_attributions_per_window = 2,
  });

  const base::Time now = base::Time::Now();

  // The following loop iterations are *not* independent: Each one depends on
  // the correct handling of the previous one.
  const struct {
    RateLimitRow row;
    AttributionAllowedStatus expected;
  } kRateLimitsToAdd[] = {
      // Add the limit of 2 reports for this tuple. Note that although the first
      // two *origins* for each row are different, they share the same *sites*,
      // that is, https://s1.test and https://d1.test, respectively.

      {{"https://a.s1.test", "https://a.d1.test", "https://a.r.test", now},
       AttributionAllowedStatus::kAllowed},

      {{"https://b.s1.test", "https://b.d1.test", "https://a.r.test", now},
       AttributionAllowedStatus::kAllowed},

      // This is not allowed because
      // <https://s1.test, https://d1.test, https://a.r.test> already has the
      // maximum of 2
      // reports.
      {{"https://b.s1.test", "https://b.d1.test", "https://a.r.test", now},
       AttributionAllowedStatus::kNotAllowed},

      // This is allowed because the source site is different.
      {{"https://s2.test", "https://a.d1.test", "https://a.r.test", now},
       AttributionAllowedStatus::kAllowed},

      // This is allowed because the destination site is different.
      {{"https://a.s1.test", "https://d2.test", "https://a.r.test", now},
       AttributionAllowedStatus::kAllowed},

      // This is allowed because the reporting origin is different.
      {{"https://a.s1.test", "https://d2.test", "https://b.r.test", now},
       AttributionAllowedStatus::kAllowed},
  };

  for (const auto& rate_limit_to_add : kRateLimitsToAdd) {
    auto report = rate_limit_to_add.row.BuildReport();

    ASSERT_EQ(rate_limit_to_add.expected, AttributionAllowed(report)) << report;

    if (rate_limit_to_add.expected == AttributionAllowedStatus::kAllowed) {
      ASSERT_TRUE(AddRateLimit(report)) << report;
    }
  }

  task_environment_.FastForwardBy(kTimeWindow);

  // This is allowed because the original rows for the tuple have fallen out of
  // the time window.
  const RateLimitRow row{"https://a.s1.test", "https://a.d1.test",
                         "https://a.r.test", base::Time::Now()};
  ASSERT_EQ(AttributionAllowedStatus::kAllowed,
            AttributionAllowed(row.BuildReport()));
}

TEST_F(RateLimitTableTest, AttributionAllowed_SourceTypesCombined) {
  delegate_.set_rate_limits({
      .time_window = base::Days(1),
      .max_attributions_per_window = 2,
  });

  const auto navigation_report =
      ReportBuilder(
          SourceBuilder()
              .SetSourceType(CommonSourceInfo::SourceType::kNavigation)
              .BuildStored())
          .Build();

  const auto event_report =
      ReportBuilder(SourceBuilder()
                        .SetSourceType(CommonSourceInfo::SourceType::kEvent)
                        .BuildStored())
          .Build();

  ASSERT_EQ(AttributionAllowedStatus::kAllowed,
            AttributionAllowed(navigation_report));

  ASSERT_EQ(AttributionAllowedStatus::kAllowed,
            AttributionAllowed(event_report));

  ASSERT_TRUE(AddRateLimit(navigation_report));
  ASSERT_TRUE(AddRateLimit(event_report));

  ASSERT_EQ(AttributionAllowedStatus::kNotAllowed,
            AttributionAllowed(navigation_report));

  ASSERT_EQ(AttributionAllowedStatus::kNotAllowed,
            AttributionAllowed(event_report));
}

TEST_F(RateLimitTableTest, ClearAllDataAllTime) {
  for (int i = 0; i < 2; i++) {
    ASSERT_TRUE(
        AddRateLimit(ReportBuilder(SourceBuilder().BuildStored()).Build()));
  }
  ASSERT_THAT(GetRateLimitRows(), SizeIs(2));

  ASSERT_TRUE(table_.ClearAllDataAllTime(&db_));
  ASSERT_THAT(GetRateLimitRows(), IsEmpty());
}

TEST_F(RateLimitTableTest, ClearDataForOriginsInRange) {
  const base::Time now = base::Time::Now();

  const struct {
    const char* desc;
    base::Time delete_min;
    base::Time delete_max;
    base::RepeatingCallback<bool(const url::Origin&)> filter;
    std::vector<int64_t> expect_deleted;
  } kTestCases[] = {
      {
          "no deletions: filter never matches",
          base::Time::Min(),
          base::Time::Max(),
          base::BindRepeating([](const url::Origin&) { return false; }),
          {},
      },
      {
          "no deletions: no rows in time range",
          now + base::Days(1) + base::Milliseconds(1),
          base::Time::Max(),
          base::NullCallback(),
          {},
      },
      {
          "1 deletion: time range and filter match for source origin",
          now + base::Milliseconds(1),
          base::Time::Max(),
          base::BindRepeating([](const url::Origin& origin) {
            return origin == url::Origin::Create(GURL("https://a.s1.test"));
          }),
          {3},
      },
      {
          "2 deletions: filter matches for destination origin",
          base::Time::Min(),
          base::Time::Max(),
          base::BindRepeating([](const url::Origin& origin) {
            return origin == url::Origin::Create(GURL("https://b.d1.test"));
          }),
          {2, 4},
      },
      {
          "1 deletion: filter matches for reporting origin",
          base::Time::Min(),
          base::Time::Max(),
          base::BindRepeating([](const url::Origin& origin) {
            return origin == url::Origin::Create(GURL("https://c.r.test"));
          }),
          {3},
      },
      {
          "4 deletions: null filter matches everything",
          now,
          base::Time::Max(),
          base::NullCallback(),
          {1, 2, 3, 4},
      },
  };

  for (const auto& test_case : kTestCases) {
    base::flat_map<int64_t, RateLimitRow> rows = {
        {1,
         {"https://a.s1.test", "https://a.d1.test", "https://a.r.test", now}},
        {2,
         {"https://b.s1.test", "https://b.d1.test", "https://b.r.test", now}},
        {3,
         {"https://a.s1.test", "https://a.d1.test", "https://c.r.test",
          now + base::Days(1)}},
        {4,
         {"https://b.s1.test", "https://b.d1.test", "https://d.r.test",
          now + base::Days(1)}},
    };

    for (const auto& [_, row] : rows) {
      const auto report = row.BuildReport();
      ASSERT_TRUE(AddRateLimit(report)) << report;
    }

    ASSERT_EQ(GetRateLimitRows(), rows);

    ASSERT_TRUE(table_.ClearDataForOriginsInRange(
        &db_, test_case.delete_min, test_case.delete_max, test_case.filter))
        << test_case.desc;

    for (int64_t rate_limit_id : test_case.expect_deleted) {
      ASSERT_EQ(1u, rows.erase(rate_limit_id)) << test_case.desc;
    }

    ASSERT_EQ(GetRateLimitRows(), rows) << test_case.desc;

    ASSERT_TRUE(table_.ClearAllDataAllTime(&db_)) << test_case.desc;
    ASSERT_THAT(GetRateLimitRows(), IsEmpty()) << test_case.desc;
  }
}

TEST_F(RateLimitTableTest, AddRateLimit_DeletesExpiredRows) {
  delegate_.set_rate_limits({
      .time_window = base::Minutes(2),
      .max_attributions_per_window = INT_MAX,
  });

  delegate_.set_delete_expired_rate_limits_frequency(base::Minutes(4));

  ASSERT_TRUE(AddRateLimit(
      ReportBuilder(
          SourceBuilder()
              .SetImpressionOrigin(url::Origin::Create(GURL("https://s1.test")))
              .BuildStored())
          .SetTriggerTime(base::Time::Now())
          .Build()));

  task_environment_.FastForwardBy(base::Minutes(4) - base::Milliseconds(1));

  ASSERT_TRUE(AddRateLimit(
      ReportBuilder(
          SourceBuilder()
              .SetImpressionOrigin(url::Origin::Create(GURL("https://s2.test")))
              .BuildStored())
          .SetTriggerTime(base::Time::Now())
          .Build()));

  // Neither row has expired at this point.
  ASSERT_THAT(GetRateLimitRows(), SizeIs(2));

  // Advance to the next expiry period.
  task_environment_.FastForwardBy(base::Milliseconds(1));

  ASSERT_TRUE(AddRateLimit(
      ReportBuilder(
          SourceBuilder()
              .SetImpressionOrigin(url::Origin::Create(GURL("https://s3.test")))
              .BuildStored())
          .SetTriggerTime(base::Time::Now())
          .Build()));

  // The first row should be expired at this point.
  ASSERT_THAT(
      GetRateLimitRows(),
      ElementsAre(
          Pair(_, Field(&RateLimitRow::impression_origin, "https://s2.test")),
          Pair(_, Field(&RateLimitRow::impression_origin, "https://s3.test"))));
}

TEST_F(RateLimitTableTest, ClearDataForSourceIds) {
  for (int64_t id = 7; id <= 9; id++) {
    ASSERT_TRUE(AddRateLimit(
        ReportBuilder(
            SourceBuilder().SetSourceId(StoredSource::Id(id)).BuildStored())
            .Build()));
  }

  ASSERT_THAT(GetRateLimitRows(),
              ElementsAre(Pair(1, _), Pair(2, _), Pair(3, _)));

  ASSERT_TRUE(table_.ClearDataForSourceIds(
      &db_, {StoredSource::Id(7), StoredSource::Id(9)}));

  ASSERT_THAT(GetRateLimitRows(), ElementsAre(Pair(2, _)));
}

}  // namespace content
