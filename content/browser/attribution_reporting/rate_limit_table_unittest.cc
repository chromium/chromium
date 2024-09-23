// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/rate_limit_table.h"

#include <stdint.h>

#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
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
constexpr AttributionReport::Id kReportId(1);

struct RateLimitInput {
  template <typename... Args>
  static RateLimitInput Source(Args&&... args) {
    return RateLimitInput(RateLimitScope::kSource, args...);
  }

  template <typename... Args>
  static RateLimitInput EventLevelAttribution(Args&&... args) {
    return RateLimitInput(RateLimitScope::kEventLevelAttribution, args...);
  }

  RateLimitInput(RateLimitScope scope,
                 std::string source_origin,
                 std::string destination_origin,
                 std::string reporting_origin,
                 base::Time time,
                 base::TimeDelta source_expiry = kExpiry,
                 std::optional<base::Time> attribution_time = std::nullopt,
                 int64_t report_id = -1,
                 int64_t source_id = 0,
                 int64_t destination_limit_priority = 0)
      : scope(scope),
        source_origin(std::move(source_origin)),
        destination_origin(std::move(destination_origin)),
        reporting_origin(std::move(reporting_origin)),
        time(time),
        source_expiry(source_expiry),
        attribution_time(attribution_time),
        report_id(report_id),
        source_id(source_id),
        destination_limit_priority(destination_limit_priority) {}

  RateLimitScope scope;
  std::string source_origin;
  std::string destination_origin;
  std::string reporting_origin;
  base::Time time;
  base::TimeDelta source_expiry;
  std::optional<base::Time> attribution_time;
  int64_t report_id;
  int64_t source_id;
  int64_t destination_limit_priority;

  SourceBuilder NewSourceBuilder() const {
    // Ensure that operations involving attributions use the trigger time, not
    // the source time.
    auto builder = SourceBuilder(time);

    builder.SetSourceOrigin(*SuitableOrigin::Deserialize(source_origin));
    builder.SetDestinationSites(
        {net::SchemefulSite::Deserialize(destination_origin)});
    builder.SetReportingOrigin(*SuitableOrigin::Deserialize(reporting_origin));
    builder.SetExpiry(source_expiry);
    builder.SetSourceId(StoredSource::Id(source_id));
    builder.SetDestinationLimitPriority(destination_limit_priority);

    return builder;
  }

  AttributionInfo BuildAttributionInfo() const {
    CHECK_NE(scope, RateLimitScope::kSource);
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
  static RateLimitRow EventLevelAttribution(Args&&... args) {
    return RateLimitRow(RateLimitScope::kEventLevelAttribution, args...);
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

  friend bool operator==(const RateLimitRow&, const RateLimitRow&) = default;
};

std::ostream& operator<<(std::ostream& out, const RateLimitScope scope) {
  switch (scope) {
    case RateLimitScope::kSource:
      return out << "kSource";
    case RateLimitScope::kEventLevelAttribution:
      return out << "kEventLevelAttribution";
    case RateLimitScope::kAggregatableAttribution:
      return out << "kAggregatableAttribution";
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
                                        input.NewSourceBuilder().BuildStored(),
                                        input.destination_limit_priority);
  }

  [[nodiscard]] bool AddRateLimitForAttribution(const RateLimitInput& input) {
    return table_.AddRateLimitForAttribution(
        &db_, input.BuildAttributionInfo(),
        input.NewSourceBuilder().BuildStored(), input.scope,
        AttributionReport::Id(input.report_id));
  }

  [[nodiscard]] RateLimitResult SourceAllowedForReportingOriginLimit(
      const RateLimitInput& input) {
    CHECK_EQ(input.scope, RateLimitScope::kSource);
    return table_.SourceAllowedForReportingOriginLimit(
        &db_, input.NewSourceBuilder().Build(), input.time);
  }

  [[nodiscard]] RateLimitResult SourceAllowedForReportingOriginPerSiteLimit(
      const RateLimitInput& input) {
    CHECK_EQ(input.scope, RateLimitScope::kSource);
    return table_.SourceAllowedForReportingOriginPerSiteLimit(
        &db_, input.NewSourceBuilder().Build(), input.time);
  }

  [[nodiscard]] base::expected<std::vector<StoredSource::Id>,
                               RateLimitTable::Error>
  GetSourcesToDeactivateForDestinationLimit(const RateLimitInput& input) {
    CHECK_EQ(input.scope, RateLimitScope::kSource);
    return table_.GetSourcesToDeactivateForDestinationLimit(
        &db_, input.NewSourceBuilder().Build(), input.time);
  }

  [[nodiscard]] RateLimitTable::DestinationRateLimitResult
  SourceAllowedForDestinationRateLimit(const RateLimitInput& input) {
    CHECK_EQ(input.scope, RateLimitScope::kSource);
    return table_.SourceAllowedForDestinationRateLimit(
        &db_, input.NewSourceBuilder().Build(), input.time);
  }

  [[nodiscard]] RateLimitResult SourceAllowedForDestinationPerDayRateLimit(
      const RateLimitInput& input) {
    CHECK_EQ(input.scope, RateLimitScope::kSource);
    return table_.SourceAllowedForDestinationPerDayRateLimit(
        &db_, input.NewSourceBuilder().Build(), input.time);
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
        input.NewSourceBuilder().BuildStored(), input.scope);
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
  delegate_.set_rate_limits([kTimeWindow]() {
    AttributionConfig::RateLimitConfig r;
    r.time_window = kTimeWindow;
    r.max_source_registration_reporting_origins =
        std::numeric_limits<int64_t>::max();
    r.max_attribution_reporting_origins = std::numeric_limits<int64_t>::max();
    r.max_attributions = 2;
    return r;
  }());

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

      {RateLimitInput::EventLevelAttribution(
           "https://a.s1.test", "https://a.d1.test", "https://a.r.test", now),
       RateLimitResult::kAllowed},

      {RateLimitInput::EventLevelAttribution(
           "https://b.s1.test", "https://b.d1.test", "https://b.r.test", now),
       RateLimitResult::kAllowed},

      // This is not allowed because
      // <https://s1.test, https://d1.test, https://r.test> already has the
      // maximum of 2 attributions.
      {RateLimitInput::EventLevelAttribution(
           "https://b.s1.test", "https://b.d1.test", "https://b.r.test", now),
       RateLimitResult::kNotAllowed},

      // This is allowed because the source site is different.
      {RateLimitInput::EventLevelAttribution(
           "https://s2.test", "https://a.d1.test", "https://a.r.test", now),
       RateLimitResult::kAllowed},

      // This is allowed because the destination site is different.
      {RateLimitInput::EventLevelAttribution(
           "https://a.s1.test", "https://d2.test", "https://a.r.test", now),
       RateLimitResult::kAllowed},

      // This is allowed because the reporting site is different.
      {RateLimitInput::EventLevelAttribution(
           "https://a.s1.test", "https://d2.test", "https://r2.test", now),
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
  const auto input = RateLimitInput::EventLevelAttribution(
      "https://a.s1.test", "https://a.d1.test", "https://a.r.test",
      base::Time::Now());
  ASSERT_EQ(RateLimitResult::kAllowed,
            AttributionAllowedForAttributionLimit(input));
}

TEST_F(RateLimitTableTest,
       AttributionAllowedForAttributionCountLimit_SourceTypesCombined) {
  delegate_.set_rate_limits([]() {
    AttributionConfig::RateLimitConfig r;
    r.time_window = base::Days(1);
    r.max_source_registration_reporting_origins =
        std::numeric_limits<int64_t>::max();
    r.max_attribution_reporting_origins = std::numeric_limits<int64_t>::max();
    r.max_attributions = 2;
    return r;
  }());

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
            table_.AttributionAllowedForAttributionLimit(
                &db_, attribution_info, navigation_source,
                RateLimitScope::kEventLevelAttribution));

  ASSERT_EQ(RateLimitResult::kAllowed,
            table_.AttributionAllowedForAttributionLimit(
                &db_, attribution_info, event_source,
                RateLimitScope::kEventLevelAttribution));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, attribution_info, navigation_source,
      RateLimitScope::kEventLevelAttribution, kReportId));
  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, attribution_info, event_source,
      RateLimitScope::kEventLevelAttribution, kReportId));

  ASSERT_EQ(RateLimitResult::kNotAllowed,
            table_.AttributionAllowedForAttributionLimit(
                &db_, attribution_info, navigation_source,
                RateLimitScope::kEventLevelAttribution));

  ASSERT_EQ(RateLimitResult::kNotAllowed,
            table_.AttributionAllowedForAttributionLimit(
                &db_, attribution_info, event_source,
                RateLimitScope::kEventLevelAttribution));
}

TEST_F(RateLimitTableTest,
       AttributionAllowedForAttributionRateLimit_SeparateReportTypes) {
  delegate_.set_rate_limits([]() {
    AttributionConfig::RateLimitConfig r;
    r.time_window = base::Days(1);
    r.max_source_registration_reporting_origins =
        std::numeric_limits<int64_t>::max();
    r.max_attribution_reporting_origins = std::numeric_limits<int64_t>::max();
    r.max_attributions = 1;
    return r;
  }());

  const auto source = SourceBuilder().BuildStored();

  const auto attribution_info = AttributionInfoBuilder().Build();

  ASSERT_EQ(RateLimitResult::kAllowed,
            table_.AttributionAllowedForAttributionLimit(
                &db_, attribution_info, source,
                RateLimitScope::kEventLevelAttribution));
  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, attribution_info, source, RateLimitScope::kEventLevelAttribution,
      kReportId));

  ASSERT_EQ(RateLimitResult::kAllowed,
            table_.AttributionAllowedForAttributionLimit(
                &db_, attribution_info, source,
                RateLimitScope::kAggregatableAttribution));
  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, attribution_info, source, RateLimitScope::kAggregatableAttribution,
      kReportId));

  ASSERT_EQ(RateLimitResult::kNotAllowed,
            table_.AttributionAllowedForAttributionLimit(
                &db_, attribution_info, source,
                RateLimitScope::kEventLevelAttribution));

  ASSERT_EQ(RateLimitResult::kNotAllowed,
            table_.AttributionAllowedForAttributionLimit(
                &db_, attribution_info, source,
                RateLimitScope::kAggregatableAttribution));
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
  delegate_.set_rate_limits([kTimeWindow]() {
    AttributionConfig::RateLimitConfig r;
    r.time_window = kTimeWindow;
    r.max_source_registration_reporting_origins = 2;
    r.max_attribution_reporting_origins = std::numeric_limits<int64_t>::max();
    r.max_attributions = std::numeric_limits<int64_t>::max();
    return r;
  }());

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
            table_.SourceAllowedForReportingOriginLimit(&db_, input_1, now))
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

TEST_F(RateLimitTableTest,
       SourceAllowedForReportingOriginPerSourceReportingSiteLimit) {
  constexpr base::TimeDelta kTimeWindow = base::Days(1);
  delegate_.set_rate_limits([kTimeWindow]() {
    AttributionConfig::RateLimitConfig r;
    r.max_attribution_reporting_origins = std::numeric_limits<int64_t>::max();
    r.max_attributions = std::numeric_limits<int64_t>::max();
    r.origins_per_site_window = kTimeWindow;
    return r;
  }());

  const base::Time now = base::Time::Now();

  const struct {
    const char* source_origin;
    const char* destination_origin;
    const char* reporting_origin;
    RateLimitResult expected;
  } kRateLimitsToAdd[] = {
      {"https://a.s1.test", "https://d1.test", "https://a.r.test",
       RateLimitResult::kAllowed},
      // Different reporting origin, same reporting site.
      {"https://a.s1.test", "https://d1.test", "https://b.r.test",
       RateLimitResult::kNotAllowed},
      // Different source origin, same source site.
      {"https://b.s1.test", "https://d1.test", "https://b.r.test",
       RateLimitResult::kNotAllowed},
      // Different destination, destination not part of limit.
      {"https://a.s1.test", "https://d2.test", "https://c.r.test",
       RateLimitResult::kNotAllowed},
      // Different source site.
      {"https://s2.test", "https://d1.test", "https://b.r.test",
       RateLimitResult::kAllowed},
      // Different reporting site.
      {"https://a.s1.test", "https://d1.test", "https://b.r2.test",
       RateLimitResult::kAllowed}};

  for (const auto& rate_limit : kRateLimitsToAdd) {
    auto input = RateLimitInput::Source(rate_limit.source_origin,
                                        rate_limit.destination_origin,
                                        rate_limit.reporting_origin, now);

    ASSERT_EQ(rate_limit.expected,
              SourceAllowedForReportingOriginPerSiteLimit(input))
        << input;

    if (rate_limit.expected == RateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(input)) << input;
    }
  }

  task_environment_.FastForwardBy(kTimeWindow);

  // This is allowed because the original rows for the tuple have fallen out of
  // the time window.
  const auto input_1 =
      RateLimitInput::Source("https://a.s1.test", "https://d1.test",
                             "https://b.r.test", base::Time::Now());
  ASSERT_EQ(RateLimitResult::kAllowed,
            SourceAllowedForReportingOriginPerSiteLimit(input_1));
  ASSERT_TRUE(AddRateLimitForSource(input_1)) << input_1;

  // The reporting origin from the first window is not allowed after a new
  // origin was used in this window.
  const auto input_2 =
      RateLimitInput::Source("https://a.s1.test", "https://d1.test",
                             "https://a.r.test", base::Time::Now());
  ASSERT_EQ(RateLimitResult::kNotAllowed,
            SourceAllowedForReportingOriginPerSiteLimit(input_2));
}

TEST_F(RateLimitTableTest, AttributionAllowedForReportingOriginLimit) {
  constexpr base::TimeDelta kTimeWindow = base::Days(1);
  delegate_.set_rate_limits([kTimeWindow]() {
    AttributionConfig::RateLimitConfig r;
    r.time_window = kTimeWindow;
    r.max_source_registration_reporting_origins =
        std::numeric_limits<int64_t>::max();
    r.max_attribution_reporting_origins = 2;
    r.max_attributions = std::numeric_limits<int64_t>::max();
    return r;
  }());

  const base::Time now = base::Time::Now();

  for (const auto& rate_limit : kReportingOriginRateLimitsToAdd) {
    auto input = RateLimitInput::EventLevelAttribution(
        rate_limit.source_origin, rate_limit.destination_origin,
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
  const auto input = RateLimitInput::EventLevelAttribution(
      "https://a.s1.test", "https://a.d1.test", "https://c.r.test",
      base::Time::Now());
  ASSERT_EQ(RateLimitResult::kAllowed,
            AttributionAllowedForReportingOriginLimit(input));
}

TEST_F(RateLimitTableTest,
       ReportingOriginLimits_IndependentForSourcesAndAttributions) {
  delegate_.set_rate_limits([]() {
    AttributionConfig::RateLimitConfig r;
    r.time_window = base::Days(1);
    r.max_source_registration_reporting_origins = 2;
    r.max_attribution_reporting_origins = 1;
    r.max_attributions = std::numeric_limits<int64_t>::max();
    return r;
  }());

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
          RateLimitScope::kEventLevelAttribution,
          "https://r2.test",
          RateLimitResult::kAllowed,
      },
      {
          RateLimitScope::kEventLevelAttribution,
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
      case RateLimitScope::kEventLevelAttribution:
      case RateLimitScope::kAggregatableAttribution:
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

TEST_F(RateLimitTableTest, DeleteAttributionRateLimit) {
  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, AttributionInfoBuilder().Build(), SourceBuilder().BuildStored(),
      RateLimitScope::kEventLevelAttribution, AttributionReport::Id(1)));
  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, AttributionInfoBuilder().Build(), SourceBuilder().BuildStored(),
      RateLimitScope::kAggregatableAttribution, AttributionReport::Id(2)));
  ASSERT_THAT(GetRateLimitRows(), SizeIs(2));

  ASSERT_TRUE(table_.DeleteAttributionRateLimit(
      &db_, RateLimitScope::kEventLevelAttribution, AttributionReport::Id(1)));
  ASSERT_THAT(GetRateLimitRows(), SizeIs(1));

  ASSERT_TRUE(table_.DeleteAttributionRateLimit(
      &db_, RateLimitScope::kAggregatableAttribution,
      AttributionReport::Id(2)));
  ASSERT_THAT(GetRateLimitRows(), IsEmpty());
}

TEST_F(RateLimitTableTest, ClearAllDataAllTime) {
  for (int i = 0; i < 2; i++) {
    ASSERT_TRUE(table_.AddRateLimitForAttribution(
        &db_, AttributionInfoBuilder().Build(), SourceBuilder().BuildStored(),
        RateLimitScope::kEventLevelAttribution, kReportId));
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
        {1, RateLimitInput::EventLevelAttribution("https://a.s1.test",
                                                  "https://a.d1.test",
                                                  "https://a.r.test", now)},
        {2, RateLimitInput::Source("https://b.s1.test", "https://b.d1.test",
                                   "https://b.r.test", now)},
        {3, RateLimitInput::EventLevelAttribution(
                "https://a.s1.test", "https://a.d1.test", "https://c.r.test",
                now + base::Days(1))},
        {4, RateLimitInput::Source("https://b.s1.test", "https://b.d1.test",
                                   "https://d.r.test", now + base::Days(1))},
        {5, RateLimitInput::EventLevelAttribution(
                "https://a.s1.test", "https://a.d1.test", "https://c.r.test",
                now + base::Days(1), kExpiry,
                now + base::Days(1) + base::Milliseconds(10))},
        {6, RateLimitInput::EventLevelAttribution(
                "https://a.s1.test", "https://a.d1.test", "https://d.r.test",
                now + base::Days(1), kExpiry,
                now + base::Days(1) + base::Milliseconds(10))},
    };

    for (const auto& [key, input] : inputs) {
      switch (input.scope) {
        case RateLimitScope::kSource:
          ASSERT_TRUE(AddRateLimitForSource(input)) << input;
          break;
        case RateLimitScope::kEventLevelAttribution:
        case RateLimitScope::kAggregatableAttribution:
          ASSERT_TRUE(AddRateLimitForAttribution(input)) << input;
          break;
      }
    }

    base::flat_map<int64_t, RateLimitRow> rows = {
        {1, RateLimitRow::EventLevelAttribution(
                "https://s1.test", "https://d1.test", "https://a.r.test",
                "https://a.d1.test", now, now)},
        {2, RateLimitRow::Source("https://s1.test", "https://d1.test",
                                 "https://b.r.test", "https://b.s1.test", now,
                                 now + kExpiry)},
        {3, RateLimitRow::EventLevelAttribution(
                "https://s1.test", "https://d1.test", "https://c.r.test",
                "https://a.d1.test", now + base::Days(1), now + base::Days(1))},
        {4, RateLimitRow::Source("https://s1.test", "https://d1.test",
                                 "https://d.r.test", "https://b.s1.test",
                                 now + base::Days(1),
                                 now + base::Days(1) + kExpiry)},
        {5, RateLimitRow::EventLevelAttribution(
                "https://s1.test", "https://d1.test", "https://c.r.test",
                "https://a.d1.test", now + base::Days(1),
                now + base::Days(1) + base::Milliseconds(10))},
        {6, RateLimitRow::EventLevelAttribution(
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
  delegate_.set_rate_limits([]() {
    AttributionConfig::RateLimitConfig r;
    r.time_window = base::Minutes(2);
    r.max_source_registration_reporting_origins =
        std::numeric_limits<int64_t>::max();
    r.max_attribution_reporting_origins = std::numeric_limits<int64_t>::max();
    r.max_attributions = INT_MAX;
    return r;
  }());

  delegate_.set_delete_expired_rate_limits_frequency(base::Minutes(4));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, AttributionInfoBuilder().SetTime(base::Time::Now()).Build(),
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s1.test"))
          .BuildStored(),
      RateLimitScope::kEventLevelAttribution, kReportId));

  task_environment_.FastForwardBy(base::Minutes(4) - base::Milliseconds(1));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, AttributionInfoBuilder().SetTime(base::Time::Now()).Build(),
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s2.test"))
          .BuildStored(),
      RateLimitScope::kEventLevelAttribution, kReportId));

  // Neither row has expired at this point.
  ASSERT_THAT(GetRateLimitRows(), SizeIs(2));

  // Advance to the next expiry period.
  task_environment_.FastForwardBy(base::Milliseconds(1));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, AttributionInfoBuilder().SetTime(base::Time::Now()).Build(),
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s3.test"))
          .BuildStored(),
      RateLimitScope::kEventLevelAttribution, kReportId));

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

  ASSERT_TRUE(
      table_.AddRateLimitForSource(&db_, s1, /*destination_limit_priority=*/0));

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
          .BuildStored(),
      RateLimitScope::kEventLevelAttribution, kReportId));

  ASSERT_THAT(GetRateLimitRows(), SizeIs(3));
  ASSERT_THAT(
      GetRateLimitRows(),
      ElementsAre(
          Pair(_, Field(&RateLimitRow::destination_site, "https://a.test")),
          Pair(_, Field(&RateLimitRow::destination_site, "https://b.test")),
          Pair(_, Field(&RateLimitRow::destination_site, "https://c.test"))));
}

TEST_F(RateLimitTableTest, AddRateLimitSource_DeletesExpiredRows) {
  delegate_.set_rate_limits([]() {
    AttributionConfig::RateLimitConfig r;
    r.time_window = base::Minutes(2);
    r.max_source_registration_reporting_origins =
        std::numeric_limits<int64_t>::max();
    r.max_attribution_reporting_origins = std::numeric_limits<int64_t>::max();
    r.max_attributions = INT_MAX;
    return r;
  }());

  delegate_.set_delete_expired_rate_limits_frequency(base::Minutes(4));

  ASSERT_TRUE(table_.AddRateLimitForSource(
      &db_,
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s1.test"))
          .SetExpiry(base::Milliseconds(30))
          .BuildStored(),
      /*destination_limit_priority=*/0));

  ASSERT_TRUE(table_.AddRateLimitForSource(
      &db_,
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s2.test"))
          .SetExpiry(base::Minutes(5))
          .BuildStored(),
      /*destination_limit_priority=*/0));

  task_environment_.FastForwardBy(base::Minutes(4) - base::Milliseconds(1));

  ASSERT_TRUE(table_.AddRateLimitForSource(
      &db_,
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s3.test"))
          .SetExpiry(base::Milliseconds(30))
          .BuildStored(),
      /*destination_limit_priority=*/0));

  // No row has expired at this point.
  ASSERT_THAT(GetRateLimitRows(), SizeIs(3));

  // Advance to the next expiry period.
  task_environment_.FastForwardBy(base::Milliseconds(1));

  ASSERT_TRUE(table_.AddRateLimitForSource(
      &db_,
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://s4.test"))
          .BuildStored(),
      /*destination_limit_priority=*/0));

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
        &db_, SourceBuilder().SetSourceId(StoredSource::Id(id)).BuildStored(),
        /*destination_limit_priority=*/0));
  }

  for (int64_t id = 7; id <= 9; id++) {
    ASSERT_TRUE(table_.AddRateLimitForAttribution(
        &db_, AttributionInfoBuilder().Build(),
        SourceBuilder().SetSourceId(StoredSource::Id(id)).BuildStored(),
        RateLimitScope::kEventLevelAttribution, kReportId));
  }

  ASSERT_THAT(GetRateLimitRows(),
              ElementsAre(Pair(1, _), Pair(2, _), Pair(3, _), Pair(4, _),
                          Pair(5, _), Pair(6, _)));

  ASSERT_TRUE(table_.ClearDataForSourceIds(
      &db_, base::span({StoredSource::Id(5), StoredSource::Id(7),
                        StoredSource::Id(9)})));

  ASSERT_THAT(GetRateLimitRows(),
              ElementsAre(Pair(1, _), Pair(3, _), Pair(5, _)));
}

TEST_F(RateLimitTableTest, DestinationRateLimitSourceLimits) {
  delegate_.set_destination_rate_limit(
      {.max_total = 2,
       .max_per_reporting_site = 100,  // irrelevant
       .rate_limit_window = base::Minutes(1)});

  const base::Time now = base::Time::Now();

  const struct {
    RateLimitInput input;
    RateLimitTable::DestinationRateLimitResult expected;
  } kRateLimitsToAdd[] = {
      // First source site:
      {RateLimitInput::Source("https://source1.test", "https://foo1.test",
                              "https://report.test", now),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source1.test", "https://foo2.test",
                              "https://report.test", now),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source1.test", "https://foo3.test",
                              "https://report.test", now),
       RateLimitTable::DestinationRateLimitResult::kHitGlobalLimit},
      // Second source site should be independent:
      {RateLimitInput::Source("https://source2.test", "https://foo1.test",
                              "https://report.test", now),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source2.test", "https://foo2.test",
                              "https://report.test", now),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source2.test", "https://foo3.test",
                              "https://report.test", now),
       RateLimitTable::DestinationRateLimitResult::kHitGlobalLimit},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    ASSERT_EQ(rate_limit.expected,
              SourceAllowedForDestinationRateLimit(rate_limit.input))
        << rate_limit.input;

    if (rate_limit.expected ==
        RateLimitTable::DestinationRateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(rate_limit.input)) << rate_limit.input;
    }
  }
}

TEST_F(RateLimitTableTest, DestinationRateLimitReportingSitesLimits) {
  delegate_.set_destination_rate_limit({.max_total = 100,  // irrelevant
                                        .max_per_reporting_site = 2,
                                        .rate_limit_window = base::Minutes(1)});

  const base::Time now = base::Time::Now();

  const struct {
    RateLimitInput input;
    RateLimitTable::DestinationRateLimitResult expected;
  } kRateLimitsToAdd[] = {
      // First reporting site:
      {RateLimitInput::Source("https://source.test", "https://foo1.test",
                              "https://report1.test", now),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source.test", "https://foo2.test",
                              "https://report1.test", now),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source.test", "https://foo3.test",
                              "https://report1.test", now),
       RateLimitTable::DestinationRateLimitResult::kHitReportingLimit},
      // Second reporting site should be independent:
      {RateLimitInput::Source("https://source.test", "https://foo1.test",
                              "https://report2.test", now),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source.test", "https://foo2.test",
                              "https://report2.test", now),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source.test", "https://foo3.test",
                              "https://report2.test", now),
       RateLimitTable::DestinationRateLimitResult::kHitReportingLimit},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    ASSERT_EQ(rate_limit.expected,
              SourceAllowedForDestinationRateLimit(rate_limit.input))
        << rate_limit.input;

    if (rate_limit.expected ==
        RateLimitTable::DestinationRateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(rate_limit.input)) << rate_limit.input;
    }
  }
}

TEST_F(RateLimitTableTest, DestinationRateLimitMultipleOverLimit) {
  delegate_.set_destination_rate_limit(
      {.max_total = 2,
       .max_per_reporting_site = 100,  // irrelevant
       .rate_limit_window = base::Minutes(1)});

  const base::Time now = base::Time::Now();

  const struct {
    const char* description;
    base::flat_set<net::SchemefulSite> destination_sites;
    RateLimitTable::DestinationRateLimitResult expected;
  } kRateLimitsToAdd[] = {
      {"multiple",
       {net::SchemefulSite::Deserialize("https://foo1.test"),
        net::SchemefulSite::Deserialize("https://foo2.test"),
        net::SchemefulSite::Deserialize("https://foo3.test")},
       RateLimitTable::DestinationRateLimitResult::kHitGlobalLimit},
      {"first",
       {net::SchemefulSite::Deserialize("https://foo1.test")},
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {"second",
       {net::SchemefulSite::Deserialize("https://foo2.test")},
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {"third",
       {net::SchemefulSite::Deserialize("https://foo3.test")},
       RateLimitTable::DestinationRateLimitResult::kHitGlobalLimit},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    SCOPED_TRACE(rate_limit.description);
    auto builder = SourceBuilder(now)
                       .SetSourceOrigin(
                           *SuitableOrigin::Deserialize("https://source.test"))
                       .SetReportingOrigin(
                           *SuitableOrigin::Deserialize("https://report.test"))
                       .SetDestinationSites(rate_limit.destination_sites);

    ASSERT_EQ(rate_limit.expected,
              table_.SourceAllowedForDestinationRateLimit(&db_, builder.Build(),
                                                          /*source_time=*/now));

    if (rate_limit.expected ==
        RateLimitTable::DestinationRateLimitResult::kAllowed) {
      ASSERT_TRUE(table_.AddRateLimitForSource(
          &db_, builder.BuildStored(), /*destination_limit_priority=*/0));
    }
  }
}

TEST_F(RateLimitTableTest, DestinationRateLimitRollingWindow) {
  delegate_.set_destination_rate_limit(
      {.max_total = 2,
       .max_per_reporting_site = 100,  // irrelevant
       .rate_limit_window = base::Minutes(1)});

  const base::Time now = base::Time::Now();
  const base::TimeDelta expiry = base::Minutes(30);

  const struct {
    RateLimitInput input;
    RateLimitTable::DestinationRateLimitResult expected;
  } kRateLimitsToAdd[] = {
      // Time now.
      {RateLimitInput::Source("https://source.test", "https://foo1.test",
                              "https://report.test", now, expiry),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      // Time now + 30s.
      {RateLimitInput::Source("https://source.test", "https://foo2.test",
                              "https://report.test", now + base::Seconds(30),
                              expiry),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      // Time now + 1:01. foo1.test should be outside the window.
      {RateLimitInput::Source("https://source.test", "https://foo3.test",
                              "https://report.test", now + base::Seconds(61),
                              expiry),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source.test", "https://foo4.test",
                              "https://report.test", now + base::Seconds(61),
                              expiry),
       RateLimitTable::DestinationRateLimitResult::kHitGlobalLimit},
      // Time now + 1:31. foo2.test should be outside the window.
      {RateLimitInput::Source("https://source.test", "https://foo5.test",
                              "https://report.test", now + base::Seconds(91),
                              expiry),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source.test", "https://foo6.test",
                              "https://report.test", now + base::Seconds(91),
                              expiry),
       RateLimitTable::DestinationRateLimitResult::kHitGlobalLimit},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    ASSERT_EQ(rate_limit.expected,
              SourceAllowedForDestinationRateLimit(rate_limit.input))
        << rate_limit.input;

    if (rate_limit.expected ==
        RateLimitTable::DestinationRateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(rate_limit.input)) << rate_limit.input;
    }
  }
}

TEST_F(RateLimitTableTest, DestinationRateLimitSourceExpiry) {
  delegate_.set_destination_rate_limit(
      {.max_total = 1,
       .max_per_reporting_site = 100,  // irrelevant
       .rate_limit_window = base::Minutes(1)});

  const base::Time now = base::Time::Now();
  const base::TimeDelta expiry = base::Seconds(30);

  const struct {
    RateLimitInput input;
    RateLimitTable::DestinationRateLimitResult expected;
  } kRateLimitsToAdd[] = {
      // Time now.
      {RateLimitInput::Source("https://source.test", "https://foo1.test",
                              "https://report.test", now, expiry),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      // Time now + 29s.
      {RateLimitInput::Source("https://source.test", "https://foo2.test",
                              "https://report.test", now + base::Seconds(29),
                              expiry),
       RateLimitTable::DestinationRateLimitResult::kHitGlobalLimit},
      // Time now + 30s. foo1.test should have expired.
      {RateLimitInput::Source("https://source.test", "https://foo3.test",
                              "https://report.test", now + base::Seconds(30),
                              expiry),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    ASSERT_EQ(rate_limit.expected,
              SourceAllowedForDestinationRateLimit(rate_limit.input))
        << rate_limit.input;

    if (rate_limit.expected ==
        RateLimitTable::DestinationRateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(rate_limit.input)) << rate_limit.input;
    }
  }
}

TEST_F(RateLimitTableTest, DestinationRateLimitHitBothLimits) {
  delegate_.set_destination_rate_limit({.max_total = 1,
                                        .max_per_reporting_site = 1,
                                        .rate_limit_window = base::Minutes(1)});

  const base::Time now = base::Time::Now();

  const struct {
    RateLimitInput input;
    RateLimitTable::DestinationRateLimitResult expected;
  } kRateLimitsToAdd[] = {
      {RateLimitInput::Source("https://source.test", "https://foo1.test",
                              "https://report.test", now),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source.test", "https://foo1.test",
                              "https://report.test", now),
       RateLimitTable::DestinationRateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source.test", "https://foo2.test",
                              "https://report.test", now),
       RateLimitTable::DestinationRateLimitResult::kHitBothLimits},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    ASSERT_EQ(rate_limit.expected,
              SourceAllowedForDestinationRateLimit(rate_limit.input))
        << rate_limit.input;

    if (rate_limit.expected ==
        RateLimitTable::DestinationRateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(rate_limit.input)) << rate_limit.input;
    }
  }
}

TEST_F(RateLimitTableTest, SourceDestinationLimit) {
  delegate_.set_max_destinations_per_source_site_reporting_site(3);

  const base::Time now = base::Time::Now();
  const base::TimeDelta expiry = base::Milliseconds(30);

  const struct {
    RateLimitInput input;
    std::vector<StoredSource::Id> expected;
  } kRateLimitsToAdd[] = {
      {RateLimitInput::Source("https://a.s1.test", "https://a.d1.test",
                              "https://a.r1.test", now, expiry,
                              /*attribution_time=*/std::nullopt,
                              /*report_id=*/-1, /*source_id=*/1),
       {}},
      {RateLimitInput::Source("https://a.s1.test", "https://a.d1.test",
                              "https://a.r1.test", now, expiry,
                              /*attribution_time=*/std::nullopt,
                              /*report_id=*/-1, /*source_id=*/2),
       {}},
      {RateLimitInput::Source("https://a.s1.test", "https://a.d3.test",
                              "https://a.r1.test", now, expiry,
                              /*attribution_time=*/std::nullopt,
                              /*report_id=*/-1, /*source_id=*/2),
       {}},
      {RateLimitInput::Source("https://a.s1.test", "https://a.d3.test",
                              "https://a.r1.test", now, expiry,
                              /*attribution_time=*/std::nullopt,
                              /*report_id=*/-1, /*source_id=*/3),
       {}},
      {RateLimitInput::Source("https://a.s1.test", "https://a.d2.test",
                              "https://a.r1.test", now, expiry,
                              /*attribution_time=*/std::nullopt,
                              /*report_id=*/-1, /*source_id=*/4),
       {}},
      {RateLimitInput::Source("https://a.s1.test", "https://a.d2.test",
                              "https://a.r1.test", now + base::Milliseconds(1),
                              kExpiry,
                              /*attribution_time=*/std::nullopt,
                              /*report_id=*/-1, /*source_id=*/5),
       {}},
      {RateLimitInput::Source("https://a.s1.test", "https://a.d4.test",
                              "https://a.r1.test", now + base::Milliseconds(2),
                              kExpiry,
                              /*attribution_time=*/std::nullopt,
                              /*report_id=*/-1, /*source_id=*/6),
       {StoredSource::Id(1), StoredSource::Id(2)}},
      {RateLimitInput::Source("https://a.s2.test", "https://a.d5.test",
                              "https://a.r1.test", now, kExpiry,
                              /*attribution_time=*/std::nullopt,
                              /*report_id=*/-1, /*source_id=*/7),
       {}},
      {RateLimitInput::Source("https://a.s1.test", "https://a.d5.test",
                              "https://a.r2.test", now, kExpiry,
                              /*attribution_time=*/std::nullopt,
                              /*report_id=*/-1, /*source_id=*/8),
       {}},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    SCOPED_TRACE(rate_limit.input);

    ASSERT_EQ(
        rate_limit.expected,
        GetSourcesToDeactivateForDestinationLimit(rate_limit.input).value());
    ASSERT_TRUE(AddRateLimitForSource(rate_limit.input));

    if (!rate_limit.expected.empty()) {
      ASSERT_TRUE(table_.DeactivateSourcesForDestinationLimit(
          &db_, rate_limit.expected));
    }
  }

  const auto input_1 =
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://a.s2.test"))
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://a.r1.test"))
          .SetDestinationSites(
              {net::SchemefulSite::Deserialize("https://d1.test"),
               net::SchemefulSite::Deserialize("https://d2.test"),
               net::SchemefulSite::Deserialize("https://d3.test")})
          .Build();

  ASSERT_EQ(std::vector<StoredSource::Id>({StoredSource::Id(7)}),
            table_
                .GetSourcesToDeactivateForDestinationLimit(
                    &db_, input_1, now + base::Milliseconds(5))
                .value())
      << input_1;

  task_environment_.FastForwardBy(expiry);

  // This is allowed because the original sources have expired.
  const auto input_2 =
      RateLimitInput::Source("https://a.s1.test", "https://a.d5.test",
                             "https://a.r1.test", base::Time::Now());
  ASSERT_TRUE(GetSourcesToDeactivateForDestinationLimit(input_2)->empty());
}

TEST_F(RateLimitTableTest, SourceDestinationLimitPriority) {
  delegate_.set_max_destinations_per_source_site_reporting_site(2);

  const auto create_input = [](std::string destination_origin,
                               int64_t source_id,
                               int64_t destination_limit_priority) {
    static const base::Time now = base::Time::Now();
    static int offset = 0;

    return RateLimitInput::Source(
        "https://a.s1.test", std::move(destination_origin), "https://a.r1.test",
        now + base::Milliseconds(offset++), kExpiry,
        /*attribution_time=*/std::nullopt,
        /*report_id=*/-1, source_id, destination_limit_priority);
  };

  const struct {
    RateLimitInput input;
    std::vector<StoredSource::Id> expected;
  } kRateLimitsToAdd[] = {
      {create_input("https://d1.test", /*source_id=*/1,
                    /*destination_limit_priority=*/3),
       {}},
      {create_input("https://d2.test",
                    /*source_id=*/2, /*destination_limit_priority=*/1),
       {}},
      {create_input("https://d1.test",
                    /*source_id=*/3, /*destination_limit_priority=*/1),
       {}},
      {create_input("https://d2.test",
                    /*source_id=*/4, /*destination_limit_priority=*/2),
       {}},
      {create_input("https://d0.test",
                    /*source_id=*/5, /*destination_limit_priority=*/3),
       {StoredSource::Id(2), StoredSource::Id(4)}},
      {create_input("https://d2.test",
                    /*source_id=*/6, /*destination_limit_priority=*/4),
       {StoredSource::Id(1), StoredSource::Id(3)}},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    SCOPED_TRACE(rate_limit.input);

    ASSERT_EQ(
        rate_limit.expected,
        GetSourcesToDeactivateForDestinationLimit(rate_limit.input).value());
    ASSERT_TRUE(AddRateLimitForSource(rate_limit.input));

    if (!rate_limit.expected.empty()) {
      ASSERT_TRUE(table_.DeactivateSourcesForDestinationLimit(
          &db_, rate_limit.expected));
    }
  }
}

TEST_F(RateLimitTableTest, DeactivateSourcesForDestinationLimit) {
  delegate_.set_max_destinations_per_source_site_reporting_site(1);
  delegate_.set_rate_limits([]() {
    AttributionConfig::RateLimitConfig r;
    r.max_reporting_origins_per_source_reporting_site = 1;
    return r;
  }());

  ASSERT_TRUE(table_.AddRateLimitForSource(
      &db_,
      SourceBuilder()
          .SetSourceId(StoredSource::Id(1))
          .SetDestinationSites(
              {net::SchemefulSite::Deserialize("https://d1.test")})
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://a.r.test"))
          .BuildStored(),
      /*destination_limit_priority=*/0));

  StorableSource new_source =
      SourceBuilder()
          .SetDestinationSites(
              {net::SchemefulSite::Deserialize("https://d2.test")})
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://b.r.test"))
          .Build();

  ASSERT_FALSE(table_
                   .GetSourcesToDeactivateForDestinationLimit(
                       &db_, new_source, /*source_time=*/base::Time::Now())
                   ->empty());
  ASSERT_TRUE(table_.DeactivateSourcesForDestinationLimit(
      &db_, base::span({StoredSource::Id(1)})));
  EXPECT_TRUE(table_
                  .GetSourcesToDeactivateForDestinationLimit(
                      &db_, new_source, /*source_time=*/base::Time::Now())
                  ->empty());
  // This is still not allowed as the rate-limit record is not deleted.
  EXPECT_EQ(table_.SourceAllowedForReportingOriginPerSiteLimit(
                &db_, new_source, /*source_time=*/base::Time::Now()),
            RateLimitResult::kNotAllowed);
}

TEST_F(RateLimitTableTest, DestinationPerDayRateLimit) {
  delegate_.set_destination_rate_limit([] {
    AttributionConfig::DestinationRateLimit limit;
    limit.max_per_reporting_site_per_day = 2;
    return limit;
  }());

  const base::Time now = base::Time::Now();
  const base::TimeDelta expiry = base::Days(2);

  const struct {
    RateLimitInput input;
    RateLimitResult expected;
  } kRateLimitsToAdd[] = {
      // Time now.
      {RateLimitInput::Source("https://source.test", "https://foo1.test",
                              "https://report.test", now, expiry),
       RateLimitResult::kAllowed},
      // Time now + 12 hrs.
      {RateLimitInput::Source("https://source.test", "https://foo2.test",
                              "https://report.test", now + base::Hours(12),
                              expiry),
       RateLimitResult::kAllowed},
      // Time now + 1 day - 1s.
      {RateLimitInput::Source("https://source.test", "https://foo3.test",
                              "https://report.test",
                              now + base::Days(1) - base::Seconds(1), expiry),
       RateLimitResult::kNotAllowed},
      // Time now + 1 day. foo1.test should be outside the window.
      {RateLimitInput::Source("https://source.test", "https://foo3.test",
                              "https://report.test", now + base::Days(1),
                              expiry),
       RateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source.test", "https://foo4.test",
                              "https://report.test", now + base::Days(1),
                              expiry),
       RateLimitResult::kNotAllowed},
      // Time now + 1 day + 12 hrs. foo2.test should be outside the
      // window.
      {RateLimitInput::Source("https://source.test", "https://foo5.test",
                              "https://report.test", now + base::Hours(36),
                              expiry),
       RateLimitResult::kAllowed},
      {RateLimitInput::Source("https://source.test", "https://foo6.test",
                              "https://report.test", now + base::Hours(36),
                              expiry),
       RateLimitResult::kNotAllowed},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    SCOPED_TRACE(rate_limit.input);

    ASSERT_EQ(rate_limit.expected,
              SourceAllowedForDestinationPerDayRateLimit(rate_limit.input));

    if (rate_limit.expected == RateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(rate_limit.input));
    }
  }
}

TEST_F(RateLimitTableTest, DestinationPerDayRateLimitSourceExpiry) {
  delegate_.set_destination_rate_limit([] {
    AttributionConfig::DestinationRateLimit limit;
    limit.max_per_reporting_site_per_day = 1;
    return limit;
  }());

  const base::Time now = base::Time::Now();
  const base::TimeDelta expiry = base::Hours(12);

  const struct {
    RateLimitInput input;
    RateLimitResult expected;
  } kRateLimitsToAdd[] = {
      // Time now.
      {RateLimitInput::Source("https://source.test", "https://foo1.test",
                              "https://report.test", now, expiry),
       RateLimitResult::kAllowed},
      // Time now + 11 hrs.
      {RateLimitInput::Source("https://source.test", "https://foo2.test",
                              "https://report.test", now + base::Hours(11),
                              expiry),
       RateLimitResult::kNotAllowed},
      // Time now + 12 hrs. foo1.test should have expired.
      {RateLimitInput::Source("https://source.test", "https://foo3.test",
                              "https://report.test", now + base::Hours(12),
                              expiry),
       RateLimitResult::kAllowed},
  };

  for (const auto& rate_limit : kRateLimitsToAdd) {
    SCOPED_TRACE(rate_limit.input);

    ASSERT_EQ(rate_limit.expected,
              SourceAllowedForDestinationPerDayRateLimit(rate_limit.input));

    if (rate_limit.expected == RateLimitResult::kAllowed) {
      ASSERT_TRUE(AddRateLimitForSource(rate_limit.input));
    }
  }
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
          .BuildStored(),
      /*destination_limit_priority=*/0));

  ASSERT_TRUE(table_.AddRateLimitForAttribution(
      &db_, AttributionInfoBuilder().Build(),
      SourceBuilder()
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://b.r.test"))
          .BuildStored(),
      RateLimitScope::kEventLevelAttribution, kReportId));

  std::set<AttributionDataModel::DataKey> keys;
  table_.AppendRateLimitDataKeys(&db_, keys);

  EXPECT_THAT(keys, ElementsAre(expected_1, expected_2));
}

}  // namespace content
