// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/rate_limit_table.h"

#include <stdint.h>

#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/storage_partition.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

using RateLimitScope = ::content::RateLimitTable::Scope;

using ::attribution_reporting::SuitableOrigin;

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;

struct RateLimitRow {
  template <typename... Args>
  static RateLimitRow Source(Args&&... args) {
    return RateLimitRow(RateLimitScope::kSource, args...);
  }

  template <typename... Args>
  static RateLimitRow Attribution(Args&&... args) {
    return RateLimitRow(RateLimitScope::kAttribution, args...);
  }

  RateLimitRow(RateLimitScope scope,
               std::string source_origin,
               std::string destination_origin,
               std::string reporting_origin,
               base::Time time,
               base::TimeDelta source_expiry = base::Milliseconds(30))
      : scope(scope),
        source_origin(std::move(source_origin)),
        destination_origin(std::move(destination_origin)),
        reporting_origin(std::move(reporting_origin)),
        time(time),
        source_expiry(source_expiry) {}

  RateLimitScope scope;
  std::string source_origin;
  std::string destination_origin;
  std::string reporting_origin;
  base::Time time;
  base::TimeDelta source_expiry;

  SourceBuilder NewSourceBuilder() const {
    // Ensure that operations involving attributions use the trigger time, not
    // the source time.
    auto source_time = scope == RateLimitScope::kSource ? time : base::Time();
    auto builder = SourceBuilder(source_time);

    builder.SetSourceOrigin(*SuitableOrigin::Deserialize(source_origin));
    builder.SetDestinationOrigin(
        *SuitableOrigin::Deserialize(destination_origin));
    builder.SetReportingOrigin(*SuitableOrigin::Deserialize(reporting_origin));
    builder.SetExpiry(source_expiry);

    return builder;
  }

  AttributionInfo BuildAttributionInfo() const {
    CHECK_EQ(scope, RateLimitScope::kAttribution);
    auto source = NewSourceBuilder().BuildStored();
    return AttributionInfoBuilder(std::move(source)).SetTime(time).Build();
  }
};

bool operator==(const RateLimitRow& a, const RateLimitRow& b) {
  const auto tie = [](const RateLimitRow& row) {
    return std::make_tuple(row.scope, row.source_origin, row.destination_origin,
                           row.reporting_origin, row.time);
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out, const RateLimitScope scope) {
  switch (scope) {
    case RateLimitScope::kSource:
      return out << "kSource";
    case RateLimitScope::kAttribution:
      return out << "kAttribution";
  }
}

std::ostream& operator<<(std::ostream& out, const RateLimitRow& row) {
  return out << "{" << row.scope << "," << row.source_origin << ","
             << row.destination_origin << "," << row.reporting_origin << ","
             << row.time << "}";
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
        "SELECT id,scope,source_origin,destination_origin,"
        "reporting_origin,time FROM rate_limits";
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));

    while (statement.Step()) {
      rows.emplace_back(
          statement.ColumnInt64(0),
          RateLimitRow(static_cast<RateLimitScope>(statement.ColumnInt(1)),
                       /*source_origin=*/statement.ColumnString(2),
                       /*destination_origin=*/statement.ColumnString(3),
                       /*reporting_origin=*/statement.ColumnString(4),
                       statement.ColumnTime(5)));
    }

    EXPECT_TRUE(statement.Succeeded());
    return base::flat_map<int64_t, RateLimitRow>(std::move(rows));
  }

  [[nodiscard]] bool AddRateLimitForSource(const RateLimitRow& row) {
    CHECK_EQ(row.scope, RateLimitScope::kSource);
    return table_.AddRateLimitForSource(&db_,
                                        row.NewSourceBuilder().BuildStored());
  }

  [[nodiscard]] bool AddRateLimitForAttribution(const RateLimitRow& row) {
    return table_.AddRateLimitForAttribution(&db_, row.BuildAttributionInfo());
  }

  [[nodiscard]] RateLimitResult SourceAllowedForReportingOriginLimit(
      const RateLimitRow& row) {
    CHECK_EQ(row.scope, RateLimitScope::kSource);
    return table_.SourceAllowedForReportingOriginLimit(
        &db_, row.NewSourceBuilder().Build());
  }

  [[nodiscard]] RateLimitResult SourceAllowedForDestinationLimit(
      const RateLimitRow& row) {
    CHECK_EQ(row.scope, RateLimitScope::kSource);
    return table_.SourceAllowedForDestinationLimit(
        &db_, row.NewSourceBuilder().Build());
  }

  [[nodiscard]] RateLimitResult AttributionAllowedForReportingOriginLimit(
      const RateLimitRow& row) {
    return table_.AttributionAllowedForReportingOriginLimit(
        &db_, row.BuildAttributionInfo());
  }

  [[nodiscard]] RateLimitResult AttributionAllowedForAttributionLimit(
      const RateLimitRow& row) {
    return table_.AttributionAllowedForAttributionLimit(
        &db_, row.BuildAttributionInfo());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  sql::Database db_;
  ConfigurableStorageDelegate delegate_;
  RateLimitTable table_{&delegate_};
};

}  // namespace

// Tests that attribution counts are scoped to <source site, destination site,
// reporting origin> in the correct time window.
TEST_F(RateLimitTableTest,
       AttributionAllowedForAttributionCountLimit_ScopedCorrectly) {
  constexpr base::TimeDelta kTimeWindow = base::Days(1);
  delegate_.set_rate_limits({
      .time_window = kTimeWindow,
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = 2,
  });

  const base::Time now = base::Time::Now();

  // The following loop iterations are *not* independent: Each one depends on
  // the correct handling of the previous one.
  const struct {
    RateLimitRow row;
    RateLimitResult expected;
  } kRateLimitsToAdd[] = {
      // Add the limit of 2 attributions for this tuple. Note that although the
      // first
      // two *origins* for each row are different, they share the same *sites*,
      // that is, https://s1.test and https://d1.test, respectively.

      {RateLimitRow::Attribution("https://a.s1.test", "https://a.d1.test",
                                 "https://a.r.test", now),
       RateLimitResult::kAllowed},

      {RateLimitRow::Attribution("https://b.s1.test", "https://b.d1.test",
                                 "https://a.r.test", now),
       RateLimitResult::kAllowed},

      // This is not allowed because
      // <https://s1.test, https://d1.test, https://a.r.test> already has the
      // maximum of 2 attributions.
      {RateLimitRow::Attribution("https://b.s1.test", "https://b.d1.test",
                                 "https://a.r.test", now),
       RateLimitResult::kNotAllowed},

      // This is allowed because the source site is different.
      {RateLimitRow::Attribution("https://s2.test", "https://a.d1.test",
                                 "https://a.r.test", now),
       RateLimitResult::kAllowed},

      // This is allowed because the destination site is different.
      {RateLimitRow::Attribution("https://a.s1.test", "https://d2.test",
                                 "https://a.r.test", now),
       RateLimitResult::kAllowed},

      // This is allowed because the reporting origin is different.
      {RateLimitRow::Attribution("https://a.s1.test", "https://d2.test",
                                 "https://b.r.test", now),
       RateLimitResult::kAllowed},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    auto attribution = rate_limit.row.BuildAttributionInfo();

    ASSERT_EQ(rate_limit.expected,
              AttributionAllowedForAttributionLimit(rate_limit.row))
        << rate_limit.row;

    if (rate_limit.expected == RateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForAttribution(rate_limit.row)) << rate_limit.row;
    }
  }

  task_environment_.FastForwardBy(kTimeWindow);

  // This is allowed because the original rows for the tuple have fallen out of
  // the time window.
  const auto row =
      RateLimitRow::Attribution("https://a.s1.test", "https://a.d1.test",
                                "https://a.r.test", base::Time::Now());
  ASSERT_EQ(RateLimitResult::kAllowed,
            AttributionAllowedForAttributionLimit(row));
}

TEST_F(RateLimitTableTest,
       AttributionAllowedForAttributionCountLimit_SourceTypesCombined) {
  delegate_.set_rate_limits({
      .time_window = base::Days(1),
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = 2,
  });

  const auto navigation_attribution =
      AttributionInfoBuilder(
          SourceBuilder()
              .SetSourceType(AttributionSourceType::kNavigation)
              .BuildStored())
          .Build();

  const auto event_attribution =
      AttributionInfoBuilder(SourceBuilder()
                                 .SetSourceType(AttributionSourceType::kEvent)
                                 .BuildStored())
          .Build();

  ASSERT_EQ(RateLimitResult::kAllowed,
            table_.AttributionAllowedForAttributionLimit(
                &db_, navigation_attribution));

  ASSERT_EQ(
      RateLimitResult::kAllowed,
      table_.AttributionAllowedForAttributionLimit(&db_, event_attribution));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(&db_, navigation_attribution));
  ASSERT_TRUE(table_.AddRateLimitForAttribution(&db_, event_attribution));

  ASSERT_EQ(RateLimitResult::kNotAllowed,
            table_.AttributionAllowedForAttributionLimit(
                &db_, navigation_attribution));

  ASSERT_EQ(
      RateLimitResult::kNotAllowed,
      table_.AttributionAllowedForAttributionLimit(&db_, event_attribution));
}

namespace {

// The following loop iterations are *not* independent: Each one depends on
// the correct handling of the previous one.
const struct {
  const char* source_origin;
  const char* destination_origin;
  const char* reporting_origin;
  RateLimitResult expected;
} kReportingOriginRateLimitsToAdd[] = {
    // Add the limit of 2 distinct reporting origins for this tuple. Note that
    // although the first two *origins* for each row are different, they share
    // the same *sites*, that is, https://s1.test and https://d1.test,
    // respectively.

    {"https://a.s1.test", "https://a.d1.test", "https://a.r.test",
     RateLimitResult::kAllowed},

    {"https://b.s1.test", "https://b.d1.test", "https://b.r.test",
     RateLimitResult::kAllowed},

    // This is allowed because the reporting origin is not new.
    {"https://b.s1.test", "https://b.d1.test", "https://b.r.test",
     RateLimitResult::kAllowed},

    // This is not allowed because
    // <https://s1.test, https://d1.test> already has the max of 2 distinct
    // reporting origins: https://a.r.test and https://b.r.test.
    {"https://b.s1.test", "https://b.d1.test", "https://c.r.test",
     RateLimitResult::kNotAllowed},

    // This is allowed because the source site is different.
    {"https://s2.test", "https://a.d1.test", "https://c.r.test",
     RateLimitResult::kAllowed},

    // This is allowed because the destination site is different.
    {"https://a.s1.test", "https://d2.test", "https://c.r.test",
     RateLimitResult::kAllowed},
};

}  // namespace

TEST_F(RateLimitTableTest, SourceAllowedForReportingOriginLimit) {
  constexpr base::TimeDelta kTimeWindow = base::Days(1);
  delegate_.set_rate_limits({
      .time_window = kTimeWindow,
      .max_source_registration_reporting_origins = 2,
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = std::numeric_limits<int64_t>::max(),
  });

  const base::Time now = base::Time::Now();

  for (const auto& rate_limit : kReportingOriginRateLimitsToAdd) {
    auto row = RateLimitRow::Source(rate_limit.source_origin,
                                    rate_limit.destination_origin,
                                    rate_limit.reporting_origin, now);

    ASSERT_EQ(rate_limit.expected, SourceAllowedForReportingOriginLimit(row))
        << row;

    if (rate_limit.expected == RateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(row)) << row;
    }
  }

  task_environment_.FastForwardBy(kTimeWindow);

  // This is allowed because the original rows for the tuple have fallen out of
  // the time window.
  const auto row =
      RateLimitRow::Source("https://a.s1.test", "https://a.d1.test",
                           "https://c.r.test", base::Time::Now());
  ASSERT_EQ(RateLimitResult::kAllowed,
            SourceAllowedForReportingOriginLimit(row));
}

TEST_F(RateLimitTableTest, AttributionAllowedForReportingOriginLimit) {
  constexpr base::TimeDelta kTimeWindow = base::Days(1);
  delegate_.set_rate_limits({
      .time_window = kTimeWindow,
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = 2,
      .max_attributions = std::numeric_limits<int64_t>::max(),
  });

  const base::Time now = base::Time::Now();

  for (const auto& rate_limit : kReportingOriginRateLimitsToAdd) {
    auto row = RateLimitRow::Attribution(rate_limit.source_origin,
                                         rate_limit.destination_origin,
                                         rate_limit.reporting_origin, now);

    ASSERT_EQ(rate_limit.expected,
              AttributionAllowedForReportingOriginLimit(row))
        << row;

    if (rate_limit.expected == RateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForAttribution(row)) << row;
    }
  }

  task_environment_.FastForwardBy(kTimeWindow);

  // This is allowed because the original rows for the tuple have fallen out of
  // the time window.
  const auto row =
      RateLimitRow::Attribution("https://a.s1.test", "https://a.d1.test",
                                "https://c.r.test", base::Time::Now());
  ASSERT_EQ(RateLimitResult::kAllowed,
            AttributionAllowedForReportingOriginLimit(row));
}

TEST_F(RateLimitTableTest,
       ReportingOriginLimits_IndependentForSourcesAndAttributions) {
  delegate_.set_rate_limits({
      .time_window = base::Days(1),
      .max_source_registration_reporting_origins = 2,
      .max_attribution_reporting_origins = 1,
      .max_attributions = std::numeric_limits<int64_t>::max(),
  });

  const base::Time now = base::Time::Now();

  // The following loop iterations are *not* independent: Each one depends on
  // the correct handling of the previous one.
  const struct {
    RateLimitScope scope;
    const char* reporting_origin;
    RateLimitResult expected;
  } kRateLimitsToAdd[] = {
      {
          RateLimitScope::kSource,
          "https://r1.test",
          RateLimitResult::kAllowed,
      },
      {
          RateLimitScope::kAttribution,
          "https://r2.test",
          RateLimitResult::kAllowed,
      },
      {
          RateLimitScope::kAttribution,
          "https://r3.test",
          RateLimitResult::kNotAllowed,
      },
      {
          RateLimitScope::kSource,
          "https://r4.test",
          RateLimitResult::kAllowed,
      },
      {
          RateLimitScope::kSource,
          "https://r5.test",
          RateLimitResult::kNotAllowed,
      },
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    RateLimitRow row(rate_limit.scope, "https://s.test", "https://d.test",
                     rate_limit.reporting_origin, now);

    switch (row.scope) {
      case RateLimitScope::kSource:
        ASSERT_EQ(rate_limit.expected,
                  SourceAllowedForReportingOriginLimit(row))
            << row;

        if (rate_limit.expected == RateLimitResult::kAllowed) {
          ASSERT_TRUE(AddRateLimitForSource(row)) << row;
        }

        break;
      case RateLimitScope::kAttribution:
        ASSERT_EQ(rate_limit.expected,
                  AttributionAllowedForReportingOriginLimit(row))
            << row;

        if (rate_limit.expected == RateLimitResult::kAllowed) {
          ASSERT_TRUE(AddRateLimitForAttribution(row)) << row;
        }

        break;
    }
  }
}

TEST_F(RateLimitTableTest, ClearAllDataAllTime) {
  for (int i = 0; i < 2; i++) {
    ASSERT_TRUE(table_.AddRateLimitForAttribution(
        &db_, AttributionInfoBuilder(SourceBuilder().BuildStored()).Build()));
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
    StoragePartition::StorageKeyMatcherFunction filter;
    std::vector<int64_t> expect_deleted;
  } kTestCases[] = {
      {
          "no deletions: filter never matches",
          base::Time::Min(),
          base::Time::Max(),
          base::BindRepeating([](const blink::StorageKey&) { return false; }),
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
          base::BindRepeating([](const blink::StorageKey& storage_key) {
            return storage_key == blink::StorageKey::CreateFromStringForTesting(
                                      "https://a.s1.test");
          }),
          {3},
      },
      {
          "2 deletions: filter matches for destination origin",
          base::Time::Min(),
          base::Time::Max(),
          base::BindRepeating([](const blink::StorageKey& storage_key) {
            return storage_key == blink::StorageKey::CreateFromStringForTesting(
                                      "https://b.d1.test");
          }),
          {2, 4},
      },
      {
          "1 deletion: filter matches for reporting origin",
          base::Time::Min(),
          base::Time::Max(),
          base::BindRepeating([](const blink::StorageKey& storage_key) {
            return storage_key == blink::StorageKey::CreateFromStringForTesting(
                                      "https://c.r.test");
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
        {1, RateLimitRow::Attribution("https://a.s1.test", "https://a.d1.test",
                                      "https://a.r.test", now)},
        {2, RateLimitRow::Source("https://b.s1.test", "https://b.d1.test",
                                 "https://b.r.test", now)},
        {3, RateLimitRow::Attribution("https://a.s1.test", "https://a.d1.test",
                                      "https://c.r.test", now + base::Days(1))},
        {4, RateLimitRow::Source("https://b.s1.test", "https://b.d1.test",
                                 "https://d.r.test", now + base::Days(1))},
    };

    for (const auto& [key, row] : rows) {
      switch (row.scope) {
        case RateLimitScope::kSource:
          ASSERT_TRUE(AddRateLimitForSource(row)) << row;
          break;
        case RateLimitScope::kAttribution:
          ASSERT_TRUE(AddRateLimitForAttribution(row)) << row;
          break;
      }
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
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = INT_MAX,
  });

  delegate_.set_delete_expired_rate_limits_frequency(base::Minutes(4));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_,
      AttributionInfoBuilder(
          SourceBuilder()
              .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s1.test"))
              .BuildStored())
          .SetTime(base::Time::Now())
          .Build()));

  task_environment_.FastForwardBy(base::Minutes(4) - base::Milliseconds(1));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_,
      AttributionInfoBuilder(
          SourceBuilder()
              .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s2.test"))
              .BuildStored())
          .SetTime(base::Time::Now())
          .Build()));

  // Neither row has expired at this point.
  ASSERT_THAT(GetRateLimitRows(), SizeIs(2));

  // Advance to the next expiry period.
  task_environment_.FastForwardBy(base::Milliseconds(1));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_,
      AttributionInfoBuilder(
          SourceBuilder()
              .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s3.test"))
              .BuildStored())
          .SetTime(base::Time::Now())
          .Build()));

  // The first row should be expired at this point.
  ASSERT_THAT(
      GetRateLimitRows(),
      ElementsAre(
          Pair(_, Field(&RateLimitRow::source_origin, "https://s2.test")),
          Pair(_, Field(&RateLimitRow::source_origin, "https://s3.test"))));
}

TEST_F(RateLimitTableTest, AddRateLimitSource_DeletesExpiredRows) {
  delegate_.set_rate_limits({
      .time_window = base::Minutes(2),
      .max_source_registration_reporting_origins =
          std::numeric_limits<int64_t>::max(),
      .max_attribution_reporting_origins = std::numeric_limits<int64_t>::max(),
      .max_attributions = INT_MAX,
  });

  delegate_.set_delete_expired_rate_limits_frequency(base::Minutes(4));

  ASSERT_TRUE(table_.AddRateLimitForSource(
      &db_,
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s1.test"))
          .BuildStored()));

  ASSERT_TRUE(table_.AddRateLimitForSource(
      &db_,
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s2.test"))
          .SetExpiry(base::Minutes(5))
          .BuildStored()));

  task_environment_.FastForwardBy(base::Minutes(4) - base::Milliseconds(1));

  ASSERT_TRUE(table_.AddRateLimitForSource(
      &db_,
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s3.test"))
          .BuildStored()));

  // No row has expired at this point.
  ASSERT_THAT(GetRateLimitRows(), SizeIs(3));

  // Advance to the next expiry period.
  task_environment_.FastForwardBy(base::Milliseconds(1));

  ASSERT_TRUE(table_.AddRateLimitForSource(
      &db_,
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s4.test"))
          .BuildStored()));

  // The first row should be expired at this point. The second row is not
  // expired since the source is not expired yet.
  ASSERT_THAT(
      GetRateLimitRows(),
      ElementsAre(
          Pair(_, Field(&RateLimitRow::source_origin, "https://s2.test")),
          Pair(_, Field(&RateLimitRow::source_origin, "https://s3.test")),
          Pair(_, Field(&RateLimitRow::source_origin, "https://s4.test"))));
}

TEST_F(RateLimitTableTest, ClearDataForSourceIds) {
  for (int64_t id = 4; id <= 6; id++) {
    ASSERT_TRUE(table_.AddRateLimitForSource(
        &db_, SourceBuilder().SetSourceId(StoredSource::Id(id)).BuildStored()));
  }

  for (int64_t id = 7; id <= 9; id++) {
    ASSERT_TRUE(table_.AddRateLimitForAttribution(
        &db_,
        AttributionInfoBuilder(
            SourceBuilder().SetSourceId(StoredSource::Id(id)).BuildStored())
            .Build()));
  }

  ASSERT_THAT(GetRateLimitRows(),
              ElementsAre(Pair(1, _), Pair(2, _), Pair(3, _), Pair(4, _),
                          Pair(5, _), Pair(6, _)));

  ASSERT_TRUE(table_.ClearDataForSourceIds(
      &db_, {StoredSource::Id(5), StoredSource::Id(7), StoredSource::Id(9)}));

  ASSERT_THAT(GetRateLimitRows(),
              ElementsAre(Pair(1, _), Pair(3, _), Pair(5, _)));
}

TEST_F(RateLimitTableTest, SourceAllowedForDestinationLimit) {
  delegate_.set_max_destinations_per_source_site_reporting_origin(2);

  const base::Time now = base::Time::Now();
  const base::TimeDelta expiry = base::Milliseconds(30);

  const struct {
    RateLimitRow row;
    RateLimitResult expected;
  } kRateLimitsToAdd[] = {
      {RateLimitRow::Source("https://a.s1.test", "https://a.d1.test",
                            "https://a.r1.test", now, expiry),
       RateLimitResult::kAllowed},
      {RateLimitRow::Source("https://a.s1.test", "https://a.d2.test",
                            "https://a.r1.test", now, expiry),
       RateLimitResult::kAllowed},
      {RateLimitRow::Source("https://a.s1.test", "https://a.d2.test",
                            "https://a.r1.test", now, expiry),
       RateLimitResult::kAllowed},
      {RateLimitRow::Source("https://a.s1.test", "https://a.d3.test",
                            "https://a.r1.test", now),
       RateLimitResult::kNotAllowed},
      {RateLimitRow::Source("https://a.s2.test", "https://a.d2.test",
                            "https://a.r1.test", now),
       RateLimitResult::kAllowed},
      {RateLimitRow::Source("https://a.s1.test", "https://a.d2.test",
                            "https://a.r2.test", now),
       RateLimitResult::kAllowed},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    ASSERT_EQ(rate_limit.expected,
              SourceAllowedForDestinationLimit(rate_limit.row))
        << rate_limit.row;

    if (rate_limit.expected == RateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(rate_limit.row)) << rate_limit.row;
    }
  }

  task_environment_.FastForwardBy(expiry);

  // This is allowed because the original sources have expired.
  const auto row =
      RateLimitRow::Source("https://a.s1.test", "https://a.d3.test",
                           "https://a.r1.test", base::Time::Now());
  EXPECT_EQ(RateLimitResult::kAllowed, SourceAllowedForDestinationLimit(row));
}

}  // namespace content
