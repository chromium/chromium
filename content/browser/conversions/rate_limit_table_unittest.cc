// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/rate_limit_table.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class RateLimitTableTest : public testing::Test {
 public:
  RateLimitTableTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    delegate_ = std::make_unique<ConfigurableStorageDelegate>();
    table_ = std::make_unique<RateLimitTable>(delegate_.get(), &clock_);
  }

  ConversionReport NewConversionReport(const url::Origin& impression_origin,
                                       const url::Origin& conversion_origin,
                                       int64_t impression_id = 0) {
    return ConversionReport(ImpressionBuilder(clock()->Now())
                                .SetImpressionOrigin(impression_origin)
                                .SetConversionOrigin(conversion_origin)
                                .SetImpressionId(impression_id)
                                .Build(),
                            /*conversion_data=*/"",
                            /*conversion_time=*/clock()->Now(),
                            /*report_time=*/clock()->Now(),
                            /*conversion_id=*/base::nullopt);
  }

  size_t GetRateLimitRows(sql::Database* db) {
    size_t rows = 0;
    EXPECT_TRUE(sql::test::CountTableRows(db, "rate_limits", &rows));
    return rows;
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

TEST_F(RateLimitTableTest, TableCreated_TableAndIndicesInitialized) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_FALSE(db.DoesTableExist("rate_limits"));
  EXPECT_FALSE(db.DoesIndexExist("rate_limit_impression_site_type_idx"));
  EXPECT_FALSE(db.DoesIndexExist("rate_limit_conversion_time_idx"));
  EXPECT_TRUE(table()->CreateTable(&db));
  EXPECT_TRUE(db.DoesTableExist("rate_limits"));
  EXPECT_TRUE(db.DoesIndexExist("rate_limit_impression_site_type_idx"));
  EXPECT_TRUE(db.DoesIndexExist("rate_limit_conversion_time_idx"));
  EXPECT_EQ(0u, GetRateLimitRows(&db));
}

TEST_F(RateLimitTableTest, AddRateLimit) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::FromDays(3),
      .max_attributions_per_window = INT_MAX,
  });

  EXPECT_TRUE(table()->AddRateLimit(
      &db,
      NewConversionReport(url::Origin::Create(GURL("https://a.example/")),
                          url::Origin::Create(GURL("https://b.example/")))));

  EXPECT_EQ(1u, GetRateLimitRows(&db));

  // The above report should be deleted, as it expires after the clock is
  // advanced.
  clock()->Advance(base::TimeDelta::FromDays(3));
  EXPECT_TRUE(table()->AddRateLimit(
      &db,
      NewConversionReport(url::Origin::Create(GURL("https://c.example/")),
                          url::Origin::Create(GURL("https://d.example/")))));

  EXPECT_EQ(1u, GetRateLimitRows(&db));
}

TEST_F(RateLimitTableTest, IsAttributionAllowed) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      // Set this to >9d so |AddRateLimit|'s calls to |DeleteExpiredRateLimits|
      // don't delete any of the rows we're adding.
      .time_window = base::TimeDelta::FromDays(10),
      .max_attributions_per_window = 2,
  });

  const url::Origin example_a = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin example_b = url::Origin::Create(GURL("https://b.example/"));
  const url::Origin example_c = url::Origin::Create(GURL("https://c.example/"));
  const url::Origin example_d = url::Origin::Create(GURL("https://d.example/"));

  // We will expire this row by advancing the clock to +10d below.
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_c)));

  clock()->Advance(base::TimeDelta::FromDays(3));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_d)));

  clock()->Advance(base::TimeDelta::FromDays(3));
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
  // neither impression and conversion match
  const auto report_b_d = NewConversionReport(example_a, example_b);

  base::Time now = clock()->Now();
  EXPECT_FALSE(table()->IsAttributionAllowed(&db, report_a_c, now));
  EXPECT_TRUE(table()->IsAttributionAllowed(&db, report_a_d, now));
  EXPECT_TRUE(table()->IsAttributionAllowed(&db, report_b_c, now));
  EXPECT_TRUE(table()->IsAttributionAllowed(&db, report_a_b, now));
  EXPECT_TRUE(table()->IsAttributionAllowed(&db, report_b_d, now));

  // Expire the first row above by advancing to +10d.
  clock()->Advance(base::TimeDelta::FromDays(4));
  now = clock()->Now();
  EXPECT_TRUE(table()->IsAttributionAllowed(&db, report_a_c, now));
  EXPECT_TRUE(table()->IsAttributionAllowed(&db, report_a_d, now));
  EXPECT_TRUE(table()->IsAttributionAllowed(&db, report_b_c, now));
  EXPECT_TRUE(table()->IsAttributionAllowed(&db, report_a_b, now));
  EXPECT_TRUE(table()->IsAttributionAllowed(&db, report_b_d, now));

  EXPECT_EQ(3u, GetRateLimitRows(&db));
}

TEST_F(RateLimitTableTest,
       IsAttributionAllowed_ConversionDestinationSubdomains) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::FromDays(4),
      .max_attributions_per_window = 2,
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
  clock()->Advance(base::TimeDelta::FromDays(3));
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_a, example_c_sub_b)));

  base::Time now = clock()->Now();
  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_a, example_c_sub_a), now));
  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_a, example_c_sub_b), now));
  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_a, example_c), now));
}

TEST_F(RateLimitTableTest, IsAttributionAllowed_ImpressionSiteSubdomains) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::FromDays(4),
      .max_attributions_per_window = 2,
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
  clock()->Advance(base::TimeDelta::FromDays(3));
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_c_sub_b, example_a)));

  base::Time now = clock()->Now();
  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_c_sub_a, example_a), now));
  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_c_sub_b, example_a), now));
  EXPECT_FALSE(table()->IsAttributionAllowed(
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
      .max_attributions_per_window = 2,
  });

  const url::Origin example_a = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin example_b = url::Origin::Create(GURL("https://b.example/"));
  const url::Origin example_c = url::Origin::Create(GURL("https://c.example/"));

  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_b)));
  clock()->Advance(base::TimeDelta::FromDays(2));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_b)));
  clock()->Advance(base::TimeDelta::FromDays(2));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_b, example_c)));
  clock()->Advance(base::TimeDelta::FromDays(2));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_b, example_c)));
  EXPECT_EQ(4u, GetRateLimitRows(&db));

  base::Time now = clock()->Now();

  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_a, example_b), now));
  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_b, example_c), now));

  // Delete the first row: attribution should now be allowed for the site,
  // but the other rows should not be deleted.
  EXPECT_TRUE(table()->ClearAllDataInRange(&db,
                                           now - base::TimeDelta::FromDays(7),
                                           now - base::TimeDelta::FromDays(6)));
  EXPECT_EQ(3u, GetRateLimitRows(&db));
  EXPECT_TRUE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_a, example_b), now));
  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_b, example_c), now));
}

TEST_F(RateLimitTableTest, ClearDataForOriginsInRange) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_attributions_per_window = 2,
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
  clock()->Advance(base::TimeDelta::FromDays(2));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_a, example_bb)));
  clock()->Advance(base::TimeDelta::FromDays(2));
  EXPECT_TRUE(
      table()->AddRateLimit(&db, NewConversionReport(example_d, example_c)));

  EXPECT_EQ(3u, GetRateLimitRows(&db));

  base::Time now = clock()->Now();
  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_a, example_b), now));

  // Should delete nothing, because (example_d, example_c) is at now.
  EXPECT_TRUE(table()->ClearDataForOriginsInRange(
      &db, base::Time(), now - base::TimeDelta::FromDays(1),
      base::BindRepeating(std::equal_to<url::Origin>(), example_c)));
  EXPECT_EQ(3u, GetRateLimitRows(&db));
  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_a, example_b), now));

  // Should delete (example_a, example_ba).
  EXPECT_TRUE(table()->ClearDataForOriginsInRange(
      &db, base::Time(), base::Time::Max(),
      base::BindRepeating(std::equal_to<url::Origin>(), example_ba)));
  EXPECT_EQ(2u, GetRateLimitRows(&db));
  EXPECT_TRUE(table()->IsAttributionAllowed(
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
}

TEST_F(RateLimitTableTest, AddRateLimit_DeletesExpiredRateLimits) {
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

  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::FromDays(3),
      .max_attributions_per_window = INT_MAX,
  });
  clock()->Advance(base::TimeDelta::FromDays(4));
  EXPECT_TRUE(table()->AddRateLimit(
      &db,
      NewConversionReport(url::Origin::Create(GURL("https://e.example/")),
                          url::Origin::Create(GURL("https://f.example/")))));
  EXPECT_EQ(1u, GetRateLimitRows(&db));
}

TEST_F(RateLimitTableTest, ClearDataForImpressionIds) {
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_TRUE(table()->CreateTable(&db));

  delegate()->set_rate_limits({
      .time_window = base::TimeDelta::Max(),
      .max_attributions_per_window = 2,
  });

  const url::Origin example_a = url::Origin::Create(GURL("https://a.example/"));
  const url::Origin example_b = url::Origin::Create(GURL("https://b.example/"));
  const url::Origin example_c = url::Origin::Create(GURL("https://c.example/"));
  const url::Origin example_d = url::Origin::Create(GURL("https://d.example/"));

  base::Time now = clock()->Now();

  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_a, example_b, /*impression_id=*/1)));
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_a, example_b, /*impression_id=*/2)));
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_c, example_d, /*impression_id=*/3)));
  EXPECT_TRUE(table()->AddRateLimit(
      &db, NewConversionReport(example_c, example_d, /*impression_id=*/4)));
  EXPECT_EQ(4u, GetRateLimitRows(&db));
  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_a, example_b), now));
  EXPECT_FALSE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_c, example_d), now));

  EXPECT_TRUE(table()->ClearDataForImpressionIds(&db, {1, 4}));
  EXPECT_EQ(2u, GetRateLimitRows(&db));
  EXPECT_TRUE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_a, example_b), now));
  EXPECT_TRUE(table()->IsAttributionAllowed(
      &db, NewConversionReport(example_c, example_d), now));
}

}  // namespace content
