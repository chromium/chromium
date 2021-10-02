// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/rate_limit_table.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/conversion_test_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using AttributionAllowedStatus =
    ::content::RateLimitTable::AttributionAllowedStatus;
using ::testing::ElementsAre;

class RateLimitTableTest : public testing::Test {
 public:
  RateLimitTableTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    delegate_ = std::make_unique<ConfigurableStorageDelegate>();
    table_ = std::make_unique<RateLimitTable>(delegate_.get(), &clock_);
  }

  AttributionReport NewConversionReport(
      url::Origin impression_origin,
      url::Origin conversion_origin,
      StorableSource::Id impression_id = StorableSource::Id(0),
      StorableSource::SourceType source_type =
          StorableSource::SourceType::kNavigation) {
    return AttributionReport(
        ImpressionBuilder(clock()->Now())
            .SetImpressionOrigin(std::move(impression_origin))
            .SetConversionOrigin(std::move(conversion_origin))
            .SetImpressionId(impression_id)
            .SetSourceType(source_type)
            .Build(),
        /*conversion_data=*/0,
        /*conversion_time=*/clock()->Now(),
        /*report_time=*/clock()->Now(),
        /*priority=*/0,
        /*conversion_id=*/absl::nullopt);
  }

  size_t GetRateLimitRows(sql::Database* db) {
    size_t rows = 0;
    EXPECT_TRUE(sql::test::CountTableRows(db, "rate_limits", &rows));
    return rows;
  }

  std::vector<std::string> GetRateLimitImpressionOrigins(sql::Database* db) {
    const char kSelectSql[] =
        "SELECT impression_origin FROM rate_limits ORDER BY rate_limit_id ASC";
    sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
    std::vector<std::string> impression_origins;
    while (statement.Step()) {
      impression_origins.push_back(statement.ColumnString(0));
    }
    return impression_origins;
  }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions"));
  }

  base::SimpleTestClock* clock() { return &clock_; }

  RateLimitTable* table() { return table_.get(); }

  ConfigurableStorageDelegate* delegate() { return delegate_.get(); }

 protected:
  base::ScopedTempDir temp_directory_;

 private:
  std::unique_ptr<ConfigurableStorageDelegate> delegate_;
  base::SimpleTestClock clock_;
  std::unique_ptr<RateLimitTable> table_;
};

}  // namespace

TEST_F(RateLimitTableTest, TableCreated_TableAndIndicesInitialized) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_FALSE(db.DoesTableExist("rate_limits"));
  EXPECT_FALSE(db.DoesIndexExist("rate_limit_impression_site_type_idx"));
  EXPECT_FALSE(
      db.DoesIndexExist("rate_limit_attribution_type_conversion_time_idx"));
  EXPECT_TRUE(table()->CreateTable(&db));
  EXPECT_TRUE(db.DoesTableExist("rate_limits"));
  EXPECT_TRUE(db.DoesIndexExist("rate_limit_impression_site_type_idx"));
  EXPECT_TRUE(
      db.DoesIndexExist("rate_limit_attribution_type_conversion_time_idx"));
  EXPECT_EQ(0u, GetRateLimitRows(&db));
}

TEST_F(RateLimitTableTest, AddRateLimit) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::Days(3),
      .max_contributions_per_window = INT_MAX,
  });

  EXPECT_TRUE(table()->AddRateLimit(
      &db,
      NewConversionReport(url::Origin::Create(GURL("https://a.example/")),
                          url::Origin::Create(GURL("https://b.example/")))));

  EXPECT_EQ(1u, GetRateLimitRows(&db));

  // The above report should be deleted, as it expires after the clock is
  // advanced.
  clock()->Advance(base::Days(3));
  EXPECT_TRUE(table()->AddRateLimit(
      &db,
      NewConversionReport(url::Origin::Create(GURL("https://c.example/")),
                          url::Origin::Create(GURL("https://d.example/")))));

  EXPECT_EQ(1u, GetRateLimitRows(&db));
}

TEST_F(RateLimitTableTest, AttributionAllowed) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      // Set this to >9d so |AddRateLimit|'s calls to |DeleteExpiredRateLimits|
      // don't delete any of the rows we're adding.
      .time_window = base::Days(10),
      .max_contributions_per_window = 2,
  });

  const url::Origin example_a = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin example_b = url::Origin::Create(GURL("https://b.example/"));
  const url::Origin example_c = url::Origin::Create(GURL("https://c.example/"));
  const url::Origin example_d = url::Origin::Create(GURL("https://d.example/"));

  // We will expire this row by advancing the clock to +10d below.
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_c)));

  clock()->Advance(base::Days(3));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_d)));

  clock()->Advance(base::Days(3));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_c)));

  EXPECT_EQ(3u, GetRateLimitRows(&db));

  // impression and conversion match
  const auto report_a_c = NewConversionReport(example_a, example_c);
  const auto report_a_d = NewConversionReport(example_a, example_d);
  // impression doesn't match
  const auto report_b_c = NewConversionReport(example_b, example_c);
  // conversion doesn't match
  const auto report_a_b = NewConversionReport(example_a, example_b);
  // neither impression nor conversion match
  const auto report_b_a = NewConversionReport(example_b, example_a);

  base::Time now = clock()->Now();
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(&db, report_a_c, now));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_a_d, now));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_b_c, now));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_a_b, now));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_b_a, now));

  // Expire the first row above by advancing to +10d.
  clock()->Advance(base::Days(4));
  now = clock()->Now();
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_a_c, now));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_a_d, now));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_b_c, now));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_a_b, now));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_b_a, now));

  EXPECT_EQ(3u, GetRateLimitRows(&db));
}

TEST_F(RateLimitTableTest, CheckAttributionAllowed_SourceTypesIndependent) {
  // Tests that limits are calculated independently for each
  // `StorableSource::SourceType`. In the future, we may change this so that
  // there is a combined calculation but each source type is weighted
  // differently.

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::Days(2),
      .max_contributions_per_window = 2,
  });

  const url::Origin example_a = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin example_c = url::Origin::Create(GURL("https://c.example/"));

  const auto report_navigation =
      NewConversionReport(example_a, example_c, StorableSource::Id(0),
                          StorableSource::SourceType::kNavigation);
  const auto report_event =
      NewConversionReport(example_a, example_c, StorableSource::Id(0),
                          StorableSource::SourceType::kEvent);

  // Add distinct source types on the same origin to ensure independence.
  EXPECT_TRUE(table()->AddRateLimit(&db, report_navigation));
  EXPECT_TRUE(table()->AddRateLimit(&db, report_event));
  EXPECT_EQ(2u, GetRateLimitRows(&db));

  base::Time now = clock()->Now();
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_navigation, now));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_event, now));

  EXPECT_TRUE(table()->AddRateLimit(&db, report_navigation));
  EXPECT_EQ(3u, GetRateLimitRows(&db));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(&db, report_navigation, now));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(&db, report_event, now));

  EXPECT_TRUE(table()->AddRateLimit(&db, report_event));
  EXPECT_EQ(4u, GetRateLimitRows(&db));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(&db, report_navigation, now));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(&db, report_event, now));
}

TEST_F(RateLimitTableTest,
       CheckAttributionAllowed_ConversionDestinationSubdomains) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::Days(4),
      .max_contributions_per_window = 2,
  });

  const url::Origin example_a = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin example_c = url::Origin::Create(GURL("https://c.example/"));
  const url::Origin example_c_sub_a =
      url::Origin::Create(GURL("https://a.c.example/"));
  const url::Origin example_c_sub_b =
      url::Origin::Create(GURL("https://b.c.example/"));

  // Add distinct subdomains on the same origin to ensure correct use of
  // ConversionDestination.
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_a, example_c_sub_a)));
  clock()->Advance(base::Days(3));
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_a, example_c_sub_b)));

  base::Time now = clock()->Now();
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_a, example_c_sub_a), now));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_a, example_c_sub_b), now));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_a, example_c), now));
}

TEST_F(RateLimitTableTest, CheckAttributionAllowed_ImpressionSiteSubdomains) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::Days(4),
      .max_contributions_per_window = 2,
  });

  const url::Origin example_a = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin example_c = url::Origin::Create(GURL("https://c.example/"));
  const url::Origin example_c_sub_a =
      url::Origin::Create(GURL("https://a.c.example/"));
  const url::Origin example_c_sub_b =
      url::Origin::Create(GURL("https://b.c.example/"));

  // Add distinct subdomains on the same origin to ensure correct use of
  // impression_site.
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_c_sub_a, example_a)));
  clock()->Advance(base::Days(3));
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_c_sub_b, example_a)));

  base::Time now = clock()->Now();
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_c_sub_a, example_a), now));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_c_sub_b, example_a), now));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_c, example_a), now));
}

TEST_F(RateLimitTableTest, ClearAllDataAllTime) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  const url::Origin example_a = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin example_b = url::Origin::Create(GURL("https://b.example/"));

  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_b)));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_b, example_a)));
  EXPECT_EQ(2u, GetRateLimitRows(&db));

  EXPECT_TRUE(table()->ClearAllDataAllTime(&db));
  EXPECT_EQ(0u, GetRateLimitRows(&db));
}

TEST_F(RateLimitTableTest, ClearAllDataInRange) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_contributions_per_window = 2,
  });

  const url::Origin example_a = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin example_b = url::Origin::Create(GURL("https://b.example/"));
  const url::Origin example_c = url::Origin::Create(GURL("https://c.example/"));

  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_b)));
  clock()->Advance(base::Days(2));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_b)));
  clock()->Advance(base::Days(2));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_b, example_c)));
  clock()->Advance(base::Days(2));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_b, example_c)));
  EXPECT_EQ(4u, GetRateLimitRows(&db));

  base::Time now = clock()->Now();

  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_a, example_b), now));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_b, example_c), now));

  // Delete the first row: attribution should now be allowed for the site,
  // but the other rows should not be deleted.
  EXPECT_TRUE(table()->ClearAllDataInRange(&db, now - base::Days(7),
                                           now - base::Days(6)));
  EXPECT_EQ(3u, GetRateLimitRows(&db));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_a, example_b), now));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_b, example_c), now));
}

TEST_F(RateLimitTableTest, ClearDataForOriginsInRange) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_contributions_per_window = 2,
  });

  const url::Origin example_a = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin example_b = url::Origin::Create(GURL("https://b.example/"));
  const url::Origin example_ba =
      url::Origin::Create(GURL("https://a.b.example/"));
  const url::Origin example_bb =
      url::Origin::Create(GURL("https://b.b.example/"));
  const url::Origin example_c = url::Origin::Create(GURL("https://c.example/"));
  const url::Origin example_d = url::Origin::Create(GURL("https://d.example/"));

  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_ba)));
  clock()->Advance(base::Days(2));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_bb)));
  clock()->Advance(base::Days(2));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_d, example_c)));

  EXPECT_EQ(3u, GetRateLimitRows(&db));

  base::Time now = clock()->Now();
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_a, example_b), now));

  // Should delete nothing, because (example_d, example_c) is at now.
  EXPECT_TRUE(table()->ClearDataForOriginsInRange(
      &db, base::Time(), now - base::Days(1),
      base::BindRepeating(std::equal_to<url::Origin>(), example_c)));
  EXPECT_EQ(3u, GetRateLimitRows(&db));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_a, example_b), now));

  // Should delete (example_a, example_ba).
  EXPECT_TRUE(table()->ClearDataForOriginsInRange(
      &db, base::Time(), base::Time::Max(),
      base::BindRepeating(std::equal_to<url::Origin>(), example_ba)));
  EXPECT_EQ(2u, GetRateLimitRows(&db));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_a, example_b), now));

  // Should delete (example_d, example_c), the only report >= now.
  EXPECT_TRUE(table()->ClearDataForOriginsInRange(
      &db, now, base::Time::Max(),
      base::BindRepeating([](const url::Origin& origin) { return true; })));
  EXPECT_EQ(1u, GetRateLimitRows(&db));

  // Should delete (example_a, example_bb).
  EXPECT_TRUE(table()->ClearDataForOriginsInRange(
      &db, base::Time(), base::Time::Max(),
      base::BindRepeating(std::equal_to<url::Origin>(), example_a)));
  EXPECT_EQ(0u, GetRateLimitRows(&db));

  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AddAggregateHistogramContributionsForTesting(
                &db,
                ImpressionBuilder(clock()->Now())
                    .SetImpressionOrigin(example_a)
                    .SetConversionOrigin(example_b)
                    .SetImpressionId(StorableSource::Id(1))
                    .Build(),
                {{.bucket = "a", .value = 2}}));
  EXPECT_EQ(1u, GetRateLimitRows(&db));

  // Should delete (example_a, example_b).
  EXPECT_TRUE(table()->ClearDataForOriginsInRange(
      &db, base::Time(), base::Time::Max(),
      base::BindRepeating(std::equal_to<url::Origin>(), example_a)));
  EXPECT_EQ(0u, GetRateLimitRows(&db));
}

TEST_F(RateLimitTableTest, AddRateLimit_DeletesExpiredRateLimits) {
  delegate()->set_delete_expired_rate_limits_frequency(base::Minutes(5));

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  EXPECT_TRUE(table()->AddRateLimit(
      &db,
      NewConversionReport(url::Origin::Create(GURL("https://a.example/")),
                          url::Origin::Create(GURL("https://b.example/")))));
  EXPECT_TRUE(table()->AddRateLimit(
      &db,
      NewConversionReport(url::Origin::Create(GURL("https://c.example/")),
                          url::Origin::Create(GURL("https://d.example/")))));
  EXPECT_THAT(GetRateLimitImpressionOrigins(&db),
              ElementsAre("https://a.example", "https://c.example"));

  delegate()->set_rate_limits({
      .time_window = base::Minutes(2),
      .max_contributions_per_window = INT_MAX,
  });
  clock()->Advance(base::Minutes(1));
  EXPECT_TRUE(table()->AddRateLimit(
      &db,
      NewConversionReport(url::Origin::Create(GURL("https://e.example/")),
                          url::Origin::Create(GURL("https://f.example/")))));
  EXPECT_THAT(GetRateLimitImpressionOrigins(&db),
              ElementsAre("https://a.example", "https://c.example",
                          "https://e.example"));

  clock()->Advance(base::Minutes(3));
  EXPECT_TRUE(table()->AddRateLimit(
      &db,
      NewConversionReport(url::Origin::Create(GURL("https://g.example/")),
                          url::Origin::Create(GURL("https://h.example/")))));
  EXPECT_EQ(4u, GetRateLimitRows(&db));
  EXPECT_THAT(GetRateLimitImpressionOrigins(&db),
              ElementsAre("https://a.example", "https://c.example",
                          "https://e.example", "https://g.example"));

  clock()->Advance(base::Minutes(1));
  EXPECT_TRUE(table()->AddRateLimit(
      &db,
      NewConversionReport(url::Origin::Create(GURL("https://i.example/")),
                          url::Origin::Create(GURL("https://j.example/")))));
  EXPECT_THAT(GetRateLimitImpressionOrigins(&db),
              ElementsAre("https://g.example", "https://i.example"));
}

TEST_F(RateLimitTableTest, ClearDataForImpressionIds) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_contributions_per_window = 2,
  });

  const url::Origin example_a = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin example_b = url::Origin::Create(GURL("https://b.example/"));
  const url::Origin example_c = url::Origin::Create(GURL("https://c.example/"));
  const url::Origin example_d = url::Origin::Create(GURL("https://d.example/"));

  base::Time now = clock()->Now();

  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_a, example_b, StorableSource::Id(1))));
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_a, example_b, StorableSource::Id(2))));
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_c, example_d, StorableSource::Id(3))));
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_c, example_d, StorableSource::Id(4))));
  EXPECT_EQ(4u, GetRateLimitRows(&db));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_a, example_b), now));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_c, example_d), now));

  EXPECT_TRUE(table()->ClearDataForImpressionIds(
      &db, {StorableSource::Id(1), StorableSource::Id(4)}));
  EXPECT_EQ(2u, GetRateLimitRows(&db));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_a, example_b), now));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AttributionAllowed(
                &db, NewConversionReport(example_c, example_d), now));
}

TEST_F(RateLimitTableTest, Aggregate) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::Days(7),
      .max_contributions_per_window = 16,
  });

  const auto impression =
      ImpressionBuilder(clock()->Now())
          .SetImpressionOrigin(url::Origin::Create(GURL("https://a.example/")))
          .SetConversionOrigin(url::Origin::Create(GURL("https://b.example/")))
          .SetImpressionId(StorableSource::Id(1))
          .Build();

  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AddAggregateHistogramContributionsForTesting(
                &db, impression,
                {
                    {.bucket = "a", .value = 2},
                    {.bucket = "b", .value = 5},
                }));

  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AddAggregateHistogramContributionsForTesting(
                &db, impression,
                {
                    {.bucket = "a", .value = 10},
                }));

  clock()->Advance(base::Days(7) - base::Milliseconds(1));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AddAggregateHistogramContributionsForTesting(
                &db, impression,
                {
                    {.bucket = "a", .value = 9},
                }));
  EXPECT_EQ(AttributionAllowedStatus::kNotAllowed,
            table()->AddAggregateHistogramContributionsForTesting(
                &db, impression,
                {
                    {.bucket = "b", .value = 1},
                }));

  // This is checking expiry behavior.
  clock()->Advance(base::Days(1));
  EXPECT_EQ(AttributionAllowedStatus::kAllowed,
            table()->AddAggregateHistogramContributionsForTesting(
                &db, impression,
                {
                    {.bucket = "a", .value = 7},
                }));
}

}  // namespace content
