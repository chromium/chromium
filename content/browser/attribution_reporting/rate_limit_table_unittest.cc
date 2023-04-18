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

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/attribution_reporting/test/configurable_storage_delegate.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/schemeful_site.h"
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

constexpr base::TimeDelta kExpiry = base::Milliseconds(30);

struct RateLimitInput {
  template <typename... Args>
  static RateLimitInput Source(Args&&... args) {
    return RateLimitInput(RateLimitScope::kSource, args...);
  }

  template <typename... Args>
  static RateLimitInput Attribution(Args&&... args) {
    return RateLimitInput(RateLimitScope::kAttribution, args...);
  }

  RateLimitInput(RateLimitScope scope,
                 std::string source_origin,
                 std::string destination_origin,
                 std::string reporting_origin,
                 base::Time time,
                 base::TimeDelta source_expiry = kExpiry,
                 absl::optional<base::Time> attribution_time = absl::nullopt)
      : scope(scope),
        source_origin(std::move(source_origin)),
        destination_origin(std::move(destination_origin)),
        reporting_origin(std::move(reporting_origin)),
        time(time),
        source_expiry(source_expiry),
        attribution_time(attribution_time) {}

  RateLimitScope scope;
  std::string source_origin;
  std::string destination_origin;
  std::string reporting_origin;
  base::Time time;
  base::TimeDelta source_expiry;
  absl::optional<base::Time> attribution_time;

  SourceBuilder NewSourceBuilder() const {
    // Ensure that operations involving attributions use the trigger time, not
    // the source time.
    auto builder = SourceBuilder(time);

    builder.SetSourceOrigin(*SuitableOrigin::Deserialize(source_origin));
    builder.SetDestinationSites(
        {net::SchemefulSite::Deserialize(destination_origin)});
    builder.SetReportingOrigin(*SuitableOrigin::Deserialize(reporting_origin));
    builder.SetExpiry(source_expiry);

    return builder;
  }

  AttributionInfo BuildAttributionInfo() const {
    CHECK_EQ(scope, RateLimitScope::kAttribution);
    return AttributionInfoBuilder(
               *SuitableOrigin::Deserialize(destination_origin))
        .SetTime(attribution_time.value_or(time))
        .Build();
  }
};

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
               std::string source_site,
               std::string destination_site,
               std::string reporting_origin,
               std::string context_origin,
               base::Time time,
               base::Time source_expiry_or_attribution_time)
      : scope(scope),
        source_site(std::move(source_site)),
        destination_site(std::move(destination_site)),
        reporting_origin(std::move(reporting_origin)),
        context_origin(std::move(context_origin)),
        time(time),
        source_expiry_or_attribution_time(source_expiry_or_attribution_time) {}

  RateLimitScope scope;
  std::string source_site;
  std::string destination_site;
  std::string reporting_origin;
  std::string context_origin;
  base::Time time;
  base::Time source_expiry_or_attribution_time;
};

bool operator==(const RateLimitRow& a, const RateLimitRow& b) {
  const auto tie = [](const RateLimitRow& row) {
    return std::make_tuple(row.scope, row.source_site, row.destination_site,
                           row.reporting_origin, row.context_origin, row.time,
                           row.source_expiry_or_attribution_time);
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

std::ostream& operator<<(std::ostream& out, const RateLimitInput& i) {
  return out << "{" << i.scope << "," << i.source_origin << ","
             << i.destination_origin << "," << i.reporting_origin << ","
             << "," << i.time << "," << i.source_expiry << ","
             << i.attribution_time.value_or(base::Time()) << "}";
}

std::ostream& operator<<(std::ostream& out, const RateLimitRow& row) {
  return out << "{" << row.scope << "," << row.source_site << ","
             << row.destination_site << "," << row.reporting_origin << ","
             << row.context_origin << "," << row.time << ","
             << row.source_expiry_or_attribution_time << "}";
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
        "SELECT id,scope,source_site,destination_site,"
        "reporting_origin,context_origin,time,"
        "source_expiry_or_attribution_time FROM rate_limits";
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));

    while (statement.Step()) {
      rows.emplace_back(
          statement.ColumnInt64(0),
          RateLimitRow(static_cast<RateLimitScope>(statement.ColumnInt(1)),
                       /*source_site=*/statement.ColumnString(2),
                       /*destination_site=*/statement.ColumnString(3),
                       /*reporting_origin=*/statement.ColumnString(4),
                       /*context_origin=*/statement.ColumnString(5),
                       statement.ColumnTime(6), statement.ColumnTime(7)));
    }

    EXPECT_TRUE(statement.Succeeded());
    return base::flat_map<int64_t, RateLimitRow>(std::move(rows));
  }

  [[nodiscard]] bool AddRateLimitForSource(const RateLimitInput& input) {
    CHECK_EQ(input.scope, RateLimitScope::kSource);
    return table_.AddRateLimitForSource(&db_,
                                        input.NewSourceBuilder().BuildStored());
  }

  [[nodiscard]] bool AddRateLimitForAttribution(const RateLimitInput& input) {
    return table_.AddRateLimitForAttribution(
        &db_, input.BuildAttributionInfo(),
        input.NewSourceBuilder().BuildStored());
  }

  [[nodiscard]] RateLimitResult SourceAllowedForReportingOriginLimit(
      const RateLimitInput& input) {
    CHECK_EQ(input.scope, RateLimitScope::kSource);
    return table_.SourceAllowedForReportingOriginLimit(
        &db_, input.NewSourceBuilder().Build());
  }

  [[nodiscard]] RateLimitResult SourceAllowedForDestinationLimit(
      const RateLimitInput& input) {
    CHECK_EQ(input.scope, RateLimitScope::kSource);
    return table_.SourceAllowedForDestinationLimit(
        &db_, input.NewSourceBuilder().Build());
  }

  [[nodiscard]] RateLimitResult AttributionAllowedForReportingOriginLimit(
      const RateLimitInput& input) {
    return table_.AttributionAllowedForReportingOriginLimit(
        &db_, input.BuildAttributionInfo(),
        input.NewSourceBuilder().BuildStored());
  }

  [[nodiscard]] RateLimitResult AttributionAllowedForAttributionLimit(
      const RateLimitInput& input) {
    return table_.AttributionAllowedForAttributionLimit(
        &db_, input.BuildAttributionInfo(),
        input.NewSourceBuilder().BuildStored());
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
    RateLimitInput input;
    RateLimitResult expected;
  } kRateLimitsToAdd[] = {
      // Add the limit of 2 attributions for this tuple. Note that although the
      // first
      // two *origins* for each row are different, they share the same *sites*,
      // that is, https://s1.test and https://d1.test, respectively.

      {RateLimitInput::Attribution("https://a.s1.test", "https://a.d1.test",
                                   "https://a.r.test", now),
       RateLimitResult::kAllowed},

      {RateLimitInput::Attribution("https://b.s1.test", "https://b.d1.test",
                                   "https://a.r.test", now),
       RateLimitResult::kAllowed},

      // This is not allowed because
      // <https://s1.test, https://d1.test, https://a.r.test> already has the
      // maximum of 2 attributions.
      {RateLimitInput::Attribution("https://b.s1.test", "https://b.d1.test",
                                   "https://a.r.test", now),
       RateLimitResult::kNotAllowed},

      // This is allowed because the source site is different.
      {RateLimitInput::Attribution("https://s2.test", "https://a.d1.test",
                                   "https://a.r.test", now),
       RateLimitResult::kAllowed},

      // This is allowed because the destination site is different.
      {RateLimitInput::Attribution("https://a.s1.test", "https://d2.test",
                                   "https://a.r.test", now),
       RateLimitResult::kAllowed},

      // This is allowed because the reporting origin is different.
      {RateLimitInput::Attribution("https://a.s1.test", "https://d2.test",
                                   "https://b.r.test", now),
       RateLimitResult::kAllowed},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    auto attribution = rate_limit.input.BuildAttributionInfo();

    ASSERT_EQ(rate_limit.expected,
              AttributionAllowedForAttributionLimit(rate_limit.input))
        << rate_limit.input;

    if (rate_limit.expected == RateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForAttribution(rate_limit.input))
          << rate_limit.input;
    }
  }

  task_environment_.FastForwardBy(kTimeWindow);

  // This is allowed because the original rows for the tuple have fallen out of
  // the time window.
  const auto input =
      RateLimitInput::Attribution("https://a.s1.test", "https://a.d1.test",
                                  "https://a.r.test", base::Time::Now());
  ASSERT_EQ(RateLimitResult::kAllowed,
            AttributionAllowedForAttributionLimit(input));
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

  const auto navigation_source =
      SourceBuilder()
          .SetSourceType(attribution_reporting::mojom::SourceType::kNavigation)
          .BuildStored();

  const auto event_source =
      SourceBuilder()
          .SetSourceType(attribution_reporting::mojom::SourceType::kEvent)
          .BuildStored();

  const auto attribution_info = AttributionInfoBuilder().Build();

  ASSERT_EQ(RateLimitResult::kAllowed,
            table_.AttributionAllowedForAttributionLimit(&db_, attribution_info,
                                                         navigation_source));

  ASSERT_EQ(RateLimitResult::kAllowed,
            table_.AttributionAllowedForAttributionLimit(&db_, attribution_info,
                                                         event_source));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(&db_, attribution_info,
                                                navigation_source));
  ASSERT_TRUE(
      table_.AddRateLimitForAttribution(&db_, attribution_info, event_source));

  ASSERT_EQ(RateLimitResult::kNotAllowed,
            table_.AttributionAllowedForAttributionLimit(&db_, attribution_info,
                                                         navigation_source));

  ASSERT_EQ(RateLimitResult::kNotAllowed,
            table_.AttributionAllowedForAttributionLimit(&db_, attribution_info,
                                                         event_source));
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
    auto input = RateLimitInput::Source(rate_limit.source_origin,
                                        rate_limit.destination_origin,
                                        rate_limit.reporting_origin, now);

    ASSERT_EQ(rate_limit.expected, SourceAllowedForReportingOriginLimit(input))
        << input;

    if (rate_limit.expected == RateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(input)) << input;
    }
  }

  const auto input_1 =
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://b.s1.test"))
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://d.r.test"))
          .SetDestinationSites(
              {net::SchemefulSite::Deserialize("https://d2.test"),
               net::SchemefulSite::Deserialize("https://d1.test")})
          .Build();

  // This is not allowed because
  // <https://s1.test, https://d1.test> already has the max of 2 distinct
  // reporting origins: https://a.r.test and https://b.r.test, even though
  // the other destination site is unique.
  ASSERT_EQ(RateLimitResult::kNotAllowed,
            table_.SourceAllowedForReportingOriginLimit(&db_, input_1))
      << input_1;

  task_environment_.FastForwardBy(kTimeWindow);

  // This is allowed because the original rows for the tuple have fallen out of
  // the time window.
  const auto input_2 =
      RateLimitInput::Source("https://a.s1.test", "https://a.d1.test",
                             "https://c.r.test", base::Time::Now());
  ASSERT_EQ(RateLimitResult::kAllowed,
            SourceAllowedForReportingOriginLimit(input_2));
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
    auto input = RateLimitInput::Attribution(rate_limit.source_origin,
                                             rate_limit.destination_origin,
                                             rate_limit.reporting_origin, now);

    ASSERT_EQ(rate_limit.expected,
              AttributionAllowedForReportingOriginLimit(input))
        << input;

    if (rate_limit.expected == RateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForAttribution(input)) << input;
    }
  }

  task_environment_.FastForwardBy(kTimeWindow);

  // This is allowed because the original rows for the tuple have fallen out of
  // the time window.
  const auto input =
      RateLimitInput::Attribution("https://a.s1.test", "https://a.d1.test",
                                  "https://c.r.test", base::Time::Now());
  ASSERT_EQ(RateLimitResult::kAllowed,
            AttributionAllowedForReportingOriginLimit(input));
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
    RateLimitInput input(rate_limit.scope, "https://s.test", "https://d.test",
                         rate_limit.reporting_origin, now);

    switch (input.scope) {
      case RateLimitScope::kSource:
        ASSERT_EQ(rate_limit.expected,
                  SourceAllowedForReportingOriginLimit(input))
            << input;

        if (rate_limit.expected == RateLimitResult::kAllowed) {
          ASSERT_TRUE(AddRateLimitForSource(input)) << input;
        }

        break;
      case RateLimitScope::kAttribution:
        ASSERT_EQ(rate_limit.expected,
                  AttributionAllowedForReportingOriginLimit(input))
            << input;

        if (rate_limit.expected == RateLimitResult::kAllowed) {
          ASSERT_TRUE(AddRateLimitForAttribution(input)) << input;
        }

        break;
    }
  }
}

TEST_F(RateLimitTableTest, ClearAllDataAllTime) {
  for (int i = 0; i < 2; i++) {
    ASSERT_TRUE(table_.AddRateLimitForAttribution(
        &db_, AttributionInfoBuilder().Build(), SourceBuilder().BuildStored()));
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
          now + base::Days(1) + base::Milliseconds(11),
          base::Time::Max(),
          base::NullCallback(),
          {},
      },
      {
          "no deletions: filter doesn't match for source origin",
          base::Time::Min(),
          base::Time::Max(),
          base::BindRepeating([](const blink::StorageKey& storage_key) {
            return storage_key == blink::StorageKey::CreateFromStringForTesting(
                                      "https://a.s1.test");
          }),
          {},
      },
      {
          "no deletions: filter doesn't match for destination origin",
          base::Time::Min(),
          base::Time::Max(),
          base::BindRepeating([](const blink::StorageKey& storage_key) {
            return storage_key == blink::StorageKey::CreateFromStringForTesting(
                                      "https://b.d1.test");
          }),
          {},
      },
      {
          "2 deletions: time range and filter match for reporting origin",
          now + base::Milliseconds(1),
          now + base::Days(1) + base::Milliseconds(5),
          base::BindRepeating([](const blink::StorageKey& storage_key) {
            return storage_key == blink::StorageKey::CreateFromStringForTesting(
                                      "https://c.r.test");
          }),
          {3, 5},
      },
      {
          "6 deletions: null filter matches everything",
          now,
          base::Time::Max(),
          base::NullCallback(),
          {1, 2, 3, 4, 5, 6},
      },
      {
          "1 deletion: attribution time range and filter match for reporting "
          "origin"
          "origin",
          now + base::Days(1) + base::Milliseconds(5),
          now + base::Days(1) + base::Milliseconds(10),
          base::BindRepeating([](const blink::StorageKey& storage_key) {
            return storage_key == blink::StorageKey::CreateFromStringForTesting(
                                      "https://c.r.test");
          }),
          {5},
      },
      {
          "2 deletions: attribution time range and null filter",
          now + base::Days(1) + base::Milliseconds(5),
          now + base::Days(1) + base::Milliseconds(10),
          base::NullCallback(),
          {5, 6},
      },
  };

  for (const auto& test_case : kTestCases) {
    base::flat_map<int64_t, RateLimitInput> inputs = {
        {1,
         RateLimitInput::Attribution("https://a.s1.test", "https://a.d1.test",
                                     "https://a.r.test", now)},
        {2, RateLimitInput::Source("https://b.s1.test", "https://b.d1.test",
                                   "https://b.r.test", now)},
        {3,
         RateLimitInput::Attribution("https://a.s1.test", "https://a.d1.test",
                                     "https://c.r.test", now + base::Days(1))},
        {4, RateLimitInput::Source("https://b.s1.test", "https://b.d1.test",
                                   "https://d.r.test", now + base::Days(1))},
        {5, RateLimitInput::Attribution(
                "https://a.s1.test", "https://a.d1.test", "https://c.r.test",
                now + base::Days(1), kExpiry,
                now + base::Days(1) + base::Milliseconds(10))},
        {6, RateLimitInput::Attribution(
                "https://a.s1.test", "https://a.d1.test", "https://d.r.test",
                now + base::Days(1), kExpiry,
                now + base::Days(1) + base::Milliseconds(10))},
    };

    for (const auto& [key, input] : inputs) {
      switch (input.scope) {
        case RateLimitScope::kSource:
          ASSERT_TRUE(AddRateLimitForSource(input)) << input;
          break;
        case RateLimitScope::kAttribution:
          ASSERT_TRUE(AddRateLimitForAttribution(input)) << input;
          break;
      }
    }

    base::flat_map<int64_t, RateLimitRow> rows = {
        {1, RateLimitRow::Attribution("https://s1.test", "https://d1.test",
                                      "https://a.r.test", "https://a.d1.test",
                                      now, now)},
        {2, RateLimitRow::Source("https://s1.test", "https://d1.test",
                                 "https://b.r.test", "https://b.s1.test", now,
                                 now + kExpiry)},
        {3, RateLimitRow::Attribution(
                "https://s1.test", "https://d1.test", "https://c.r.test",
                "https://a.d1.test", now + base::Days(1), now + base::Days(1))},
        {4, RateLimitRow::Source("https://s1.test", "https://d1.test",
                                 "https://d.r.test", "https://b.s1.test",
                                 now + base::Days(1),
                                 now + base::Days(1) + kExpiry)},
        {5, RateLimitRow::Attribution(
                "https://s1.test", "https://d1.test", "https://c.r.test",
                "https://a.d1.test", now + base::Days(1),
                now + base::Days(1) + base::Milliseconds(10))},
        {6, RateLimitRow::Attribution(
                "https://s1.test", "https://d1.test", "https://d.r.test",
                "https://a.d1.test", now + base::Days(1),
                now + base::Days(1) + base::Milliseconds(10))},
    };

    ASSERT_EQ(GetRateLimitRows(), rows) << test_case.desc;

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
      &db_, AttributionInfoBuilder().SetTime(base::Time::Now()).Build(),
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s1.test"))
          .BuildStored()));

  task_environment_.FastForwardBy(base::Minutes(4) - base::Milliseconds(1));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, AttributionInfoBuilder().SetTime(base::Time::Now()).Build(),
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s2.test"))
          .BuildStored()));

  // Neither row has expired at this point.
  ASSERT_THAT(GetRateLimitRows(), SizeIs(2));

  // Advance to the next expiry period.
  task_environment_.FastForwardBy(base::Milliseconds(1));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, AttributionInfoBuilder().SetTime(base::Time::Now()).Build(),
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s3.test"))
          .BuildStored()));

  // The first row should be expired at this point.
  ASSERT_THAT(
      GetRateLimitRows(),
      ElementsAre(
          Pair(_, Field(&RateLimitRow::source_site, "https://s2.test")),
          Pair(_, Field(&RateLimitRow::source_site, "https://s3.test"))));
}

TEST_F(RateLimitTableTest, AddRateLimitSource_OneRowPerDestination) {
  auto s1 =
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s1.test"))
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://r1.test"))
          .SetDestinationSites(
              {net::SchemefulSite::Deserialize("https://a.test"),
               net::SchemefulSite::Deserialize("https://b.test"),
               net::SchemefulSite::Deserialize("https://c.test")})
          .BuildStored();

  ASSERT_TRUE(table_.AddRateLimitForSource(&db_, s1));

  ASSERT_THAT(GetRateLimitRows(), SizeIs(3));
  ASSERT_THAT(
      GetRateLimitRows(),
      ElementsAre(
          Pair(_, Field(&RateLimitRow::destination_site, "https://a.test")),
          Pair(_, Field(&RateLimitRow::destination_site, "https://b.test")),
          Pair(_, Field(&RateLimitRow::destination_site, "https://c.test"))));
}

TEST_F(RateLimitTableTest, AddFakeSourceForAttribution_OneRowPerDestination) {
  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, AttributionInfoBuilder().Build(),
      SourceBuilder()
          .SetDestinationSites(
              {net::SchemefulSite::Deserialize("https://a.test"),
               net::SchemefulSite::Deserialize("https://b.test"),
               net::SchemefulSite::Deserialize("https://c.test")})
          .SetAttributionLogic(StoredSource::AttributionLogic::kFalsely)
          .BuildStored()));

  ASSERT_THAT(GetRateLimitRows(), SizeIs(3));
  ASSERT_THAT(
      GetRateLimitRows(),
      ElementsAre(
          Pair(_, Field(&RateLimitRow::destination_site, "https://a.test")),
          Pair(_, Field(&RateLimitRow::destination_site, "https://b.test")),
          Pair(_, Field(&RateLimitRow::destination_site, "https://c.test"))));
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
          Pair(_, Field(&RateLimitRow::source_site, "https://s2.test")),
          Pair(_, Field(&RateLimitRow::source_site, "https://s3.test")),
          Pair(_, Field(&RateLimitRow::source_site, "https://s4.test"))));
}

TEST_F(RateLimitTableTest, ClearDataForSourceIds) {
  for (int64_t id = 4; id <= 6; id++) {
    ASSERT_TRUE(table_.AddRateLimitForSource(
        &db_, SourceBuilder().SetSourceId(StoredSource::Id(id)).BuildStored()));
  }

  for (int64_t id = 7; id <= 9; id++) {
    ASSERT_TRUE(table_.AddRateLimitForAttribution(
        &db_, AttributionInfoBuilder().Build(),
        SourceBuilder().SetSourceId(StoredSource::Id(id)).BuildStored()));
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
    RateLimitInput input;
    RateLimitResult expected;
  } kRateLimitsToAdd[] = {
      {RateLimitInput::Source("https://a.s1.test", "https://a.d1.test",
                              "https://a.r1.test", now, expiry),
       RateLimitResult::kAllowed},
      {RateLimitInput::Source("https://a.s1.test", "https://a.d2.test",
                              "https://a.r1.test", now, expiry),
       RateLimitResult::kAllowed},
      {RateLimitInput::Source("https://a.s1.test", "https://a.d2.test",
                              "https://a.r1.test", now, expiry),
       RateLimitResult::kAllowed},
      {RateLimitInput::Source("https://a.s1.test", "https://a.d3.test",
                              "https://a.r1.test", now),
       RateLimitResult::kNotAllowed},
      {RateLimitInput::Source("https://a.s2.test", "https://a.d2.test",
                              "https://a.r1.test", now),
       RateLimitResult::kAllowed},
      {RateLimitInput::Source("https://a.s1.test", "https://a.d2.test",
                              "https://a.r2.test", now),
       RateLimitResult::kAllowed},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    ASSERT_EQ(rate_limit.expected,
              SourceAllowedForDestinationLimit(rate_limit.input))
        << rate_limit.input;

    if (rate_limit.expected == RateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(rate_limit.input)) << rate_limit.input;
    }
  }

  const auto input_1 =
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://a.s2.test"))
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://a.r1.test"))
          .SetDestinationSites(
              {net::SchemefulSite::Deserialize("https://d1.test"),
               net::SchemefulSite::Deserialize("https://d3.test")})
          .Build();

  ASSERT_EQ(RateLimitResult::kNotAllowed,
            table_.SourceAllowedForDestinationLimit(&db_, input_1))
      << input_1;

  task_environment_.FastForwardBy(expiry);

  // This is allowed because the original sources have expired.
  const auto input_2 =
      RateLimitInput::Source("https://a.s1.test", "https://a.d3.test",
                             "https://a.r1.test", base::Time::Now());
  EXPECT_EQ(RateLimitResult::kAllowed,
            SourceAllowedForDestinationLimit(input_2));
}

TEST_F(RateLimitTableTest, GetAttributionDataKeyList) {
  auto expected_1 = AttributionDataModel::DataKey(
      url::Origin::Create(GURL("https://a.r.test")));
  auto expected_2 = AttributionDataModel::DataKey(
      url::Origin::Create(GURL("https://b.r.test")));

  ASSERT_TRUE(table_.AddRateLimitForSource(
      &db_,
      SourceBuilder()
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://a.r.test"))
          .BuildStored()));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, AttributionInfoBuilder().Build(),
      SourceBuilder()
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://b.r.test"))
          .BuildStored()));

  std::vector<AttributionDataModel::DataKey> keys;
  table_.AppendRateLimitDataKeys(&db_, keys);

  EXPECT_THAT(keys, ElementsAre(expected_1, expected_2));
}

}  // namespace content
