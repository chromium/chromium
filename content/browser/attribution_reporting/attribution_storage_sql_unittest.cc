// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include <functional>
#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/storable_trigger.h"
#include "sql/database.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using CreateReportStatus =
    ::content::AttributionStorage::CreateReportResult::Status;

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;

class AttributionStorageSqlTest : public testing::Test {
 public:
  AttributionStorageSqlTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void OpenDatabase() {
    storage_.reset();
    auto delegate = std::make_unique<ConfigurableStorageDelegate>();
    delegate_ = delegate.get();
    storage_ = std::make_unique<AttributionStorageSql>(
        temp_directory_.GetPath(), std::move(delegate), &clock_);
  }

  void CloseDatabase() { storage_.reset(); }

  void AddReportToStorage() {
    storage_->StoreSource(SourceBuilder(clock()->Now()).Build());
    storage_->MaybeCreateAndStoreReport(DefaultTrigger());
  }

  void ExpectAllTablesEmpty() {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    static constexpr const char* kTables[] = {
        "conversions",
        "impressions",
        "rate_limits",
        "dedup_keys",
    };

    for (const char* table : kTables) {
      size_t rows;
      sql::test::CountTableRows(&raw_db, table, &rows);
      EXPECT_EQ(0u, rows) << table;
    }
  }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions"));
  }

  base::SimpleTestClock* clock() { return &clock_; }

  AttributionStorage* storage() { return storage_.get(); }

  ConfigurableStorageDelegate* delegate() { return delegate_; }

  void ExpectImpressionRows(size_t expected) {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));
    size_t rows;
    sql::test::CountTableRows(&raw_db, "impressions", &rows);
    EXPECT_EQ(expected, rows);
  }

  CreateReportStatus MaybeCreateAndStoreReport(
      const StorableTrigger& conversion) {
    return storage_->MaybeCreateAndStoreReport(conversion).status();
  }

 protected:
  base::ScopedTempDir temp_directory_;

 private:
  std::unique_ptr<AttributionStorage> storage_;
  raw_ptr<ConfigurableStorageDelegate> delegate_ = nullptr;
  base::SimpleTestClock clock_;
};

}  // namespace

TEST_F(AttributionStorageSqlTest,
       DatabaseInitialized_TablesAndIndexesLazilyInitialized) {
  base::HistogramTester histograms;

  OpenDatabase();
  CloseDatabase();

  // An unused AttributionStorageSql instance should not create the database.
  EXPECT_FALSE(base::PathExists(db_path()));

  // Operations which don't need to run on an empty database should not create
  // the database.
  OpenDatabase();
  EXPECT_THAT(storage()->GetAttributionsToReport(clock()->Now()), IsEmpty());
  CloseDatabase();

  EXPECT_FALSE(base::PathExists(db_path()));

  // DB init UMA should not be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);

  // Storing an impression should create and initialize the database.
  OpenDatabase();
  storage()->StoreSource(SourceBuilder(clock()->Now()).Build());
  CloseDatabase();

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 1);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [impressions], [conversions], [meta], [rate_limits], [dedup_keys],
    // [sqlite_sequence] (for AUTOINCREMENT support).
    EXPECT_EQ(6u, sql::test::CountSQLTables(&raw_db));

    // [conversion_domain_idx], [impression_expiry_idx],
    // [impression_origin_idx], [impression_site_idx],
    // [conversion_report_time_idx], [conversion_impression_id_idx],
    // [rate_limit_origin_type_idx], [rate_limit_conversion_time_idx],
    // [rate_limit_impression_id_idx] and the meta table index.
    EXPECT_EQ(10u, sql::test::CountSQLIndices(&raw_db));
  }
}

TEST_F(AttributionStorageSqlTest, DatabaseReopened_DataPersisted) {
  OpenDatabase();
  AddReportToStorage();
  EXPECT_THAT(storage()->GetAttributionsToReport(clock()->Now()), SizeIs(1));
  CloseDatabase();
  OpenDatabase();
  EXPECT_THAT(storage()->GetAttributionsToReport(clock()->Now()), SizeIs(1));
}

TEST_F(AttributionStorageSqlTest, CorruptDatabase_RecoveredOnOpen) {
  OpenDatabase();
  AddReportToStorage();
  EXPECT_THAT(storage()->GetAttributionsToReport(clock()->Now()), SizeIs(1));
  CloseDatabase();

  // Corrupt the database.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(db_path()));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CORRUPT);

  // Open that database and ensure that it does not fail.
  EXPECT_NO_FATAL_FAILURE(OpenDatabase());

  // Data should be recovered.
  EXPECT_THAT(storage()->GetAttributionsToReport(clock()->Now()), SizeIs(1));

  EXPECT_TRUE(expecter.SawExpectedErrors());
}

//  Create an impression with two conversions (C1 and C2). Craft a query that
//  will target C2, which will in turn delete the impression. We should ensure
//  that C1 is properly deleted (conversions should not be stored unattributed).
TEST_F(AttributionStorageSqlTest, ClearDataWithVestigialConversion) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = clock()->Now();
  auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
  storage()->StoreSource(impression);

  clock()->Advance(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  clock()->Advance(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Use a time range that only intersects the last conversion.
  storage()->ClearData(clock()->Now(), clock()->Now(),
                       base::BindRepeating(std::equal_to<url::Origin>(),
                                           impression.impression_origin()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation", 2, 1);
}

// Same as the above test, but with a null filter.
TEST_F(AttributionStorageSqlTest, ClearAllDataWithVestigialConversion) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = clock()->Now();
  auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
  storage()->StoreSource(impression);

  clock()->Advance(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  clock()->Advance(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Use a time range that only intersects the last conversion.
  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(clock()->Now(), clock()->Now(), null_filter);
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation", 2, 1);
}

// The max time range with a null filter should delete everything.
TEST_F(AttributionStorageSqlTest, DeleteEverything) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = clock()->Now();
  for (int i = 0; i < 10; i++) {
    auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
    storage()->StoreSource(impression);
    clock()->Advance(base::Days(1));
  }

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  clock()->Advance(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time::Min(), base::Time::Max(), null_filter);
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation", 2, 1);
}

TEST_F(AttributionStorageSqlTest, MaxSourcesPerOrigin) {
  OpenDatabase();
  delegate()->set_max_sources_per_origin(2);
  storage()->StoreSource(SourceBuilder(clock()->Now()).Build());
  storage()->StoreSource(SourceBuilder(clock()->Now()).Build());
  storage()->StoreSource(SourceBuilder(clock()->Now()).Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t impression_rows;
  sql::test::CountTableRows(&raw_db, "impressions", &impression_rows);
  EXPECT_EQ(1u, impression_rows);
  size_t rate_limit_rows;
  sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
  EXPECT_EQ(1u, rate_limit_rows);
}

TEST_F(AttributionStorageSqlTest, MaxAttributionsPerOrigin) {
  OpenDatabase();
  delegate()->set_max_attributions_per_origin(2);
  storage()->StoreSource(SourceBuilder(clock()->Now()).Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_EQ(CreateReportStatus::kNoCapacityForConversionDestination,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t conversion_rows;
  sql::test::CountTableRows(&raw_db, "conversions", &conversion_rows);
  EXPECT_EQ(2u, conversion_rows);
  size_t rate_limit_rows;
  sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
  EXPECT_EQ(2u, rate_limit_rows);
}

TEST_F(AttributionStorageSqlTest,
       DeleteRateLimitRowsForSubdomainImpressionOrigin) {
  OpenDatabase();
  delegate()->set_max_attributions_per_source(1);
  delegate()->set_rate_limits({
      .time_window = base::Days(7),
      .max_contributions_per_window = INT_MAX,
  });
  const url::Origin impression_origin =
      url::Origin::Create(GURL("https://sub.impression.example/"));
  const url::Origin reporting_origin =
      url::Origin::Create(GURL("https://a.example/"));
  const url::Origin conversion_origin =
      url::Origin::Create(GURL("https://b.example/"));
  storage()->StoreSource(SourceBuilder(clock()->Now())
                             .SetExpiry(base::Days(30))
                             .SetImpressionOrigin(impression_origin)
                             .SetReportingOrigin(reporting_origin)
                             .SetConversionOrigin(conversion_origin)
                             .Build());

  clock()->Advance(base::Days(1));
  EXPECT_EQ(
      CreateReportStatus::kSuccess,
      MaybeCreateAndStoreReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  // Force the impression to be deactivated by ensuring that the next report is
  // in a different window.
  delegate()->set_report_time_ms(1);
  EXPECT_EQ(
      CreateReportStatus::kPriorityTooLow,
      MaybeCreateAndStoreReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());

  clock()->Advance(base::Days(1));
  EXPECT_TRUE(storage()->DeleteReport(AttributionReport::Id(1)));
  storage()->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating(std::equal_to<url::Origin>(), impression_origin));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t conversion_rows;
  sql::test::CountTableRows(&raw_db, "conversions", &conversion_rows);
  EXPECT_EQ(0u, conversion_rows);
  size_t rate_limit_rows;
  sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
  EXPECT_EQ(0u, rate_limit_rows);
}

TEST_F(AttributionStorageSqlTest,
       DeleteRateLimitRowsForSubdomainConversionOrigin) {
  OpenDatabase();
  delegate()->set_max_attributions_per_source(1);
  delegate()->set_rate_limits({
      .time_window = base::Days(7),
      .max_contributions_per_window = INT_MAX,
  });
  const url::Origin impression_origin =
      url::Origin::Create(GURL("https://b.example/"));
  const url::Origin reporting_origin =
      url::Origin::Create(GURL("https://a.example/"));
  const url::Origin conversion_origin =
      url::Origin::Create(GURL("https://sub.impression.example/"));
  storage()->StoreSource(SourceBuilder(clock()->Now())
                             .SetExpiry(base::Days(30))
                             .SetImpressionOrigin(impression_origin)
                             .SetReportingOrigin(reporting_origin)
                             .SetConversionOrigin(conversion_origin)
                             .Build());

  clock()->Advance(base::Days(1));
  EXPECT_EQ(
      CreateReportStatus::kSuccess,
      MaybeCreateAndStoreReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  // Force the impression to be deactivated by ensuring that the next report is
  // in a different window.
  delegate()->set_report_time_ms(1);
  EXPECT_EQ(
      CreateReportStatus::kPriorityTooLow,
      MaybeCreateAndStoreReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());

  clock()->Advance(base::Days(1));
  EXPECT_TRUE(storage()->DeleteReport(AttributionReport::Id(1)));
  storage()->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating(std::equal_to<url::Origin>(), conversion_origin));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t conversion_rows;
  sql::test::CountTableRows(&raw_db, "conversions", &conversion_rows);
  EXPECT_EQ(0u, conversion_rows);
  size_t rate_limit_rows;
  sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
  EXPECT_EQ(0u, rate_limit_rows);
}

TEST_F(AttributionStorageSqlTest, CantOpenDb_FailsSilentlyInRelease) {
  base::CreateDirectoryAndGetError(db_path(), nullptr);

  auto sql_storage = std::make_unique<AttributionStorageSql>(
      temp_directory_.GetPath(),
      std::make_unique<ConfigurableStorageDelegate>(), clock());
  sql_storage->set_ignore_errors_for_testing(true);

  std::unique_ptr<AttributionStorage> storage = std::move(sql_storage);

  // These calls should be no-ops.
  storage->StoreSource(SourceBuilder(clock()->Now()).Build());
  EXPECT_EQ(CreateReportStatus::kNoMatchingImpressions,
            storage->MaybeCreateAndStoreReport(DefaultTrigger()).status());
}

TEST_F(AttributionStorageSqlTest, DatabaseDirDoesExist_CreateDirAndOpenDB) {
  // Give the storage layer a database directory that doesn't exist.
  std::unique_ptr<AttributionStorage> storage =
      std::make_unique<AttributionStorageSql>(
          temp_directory_.GetPath().Append(
              FILE_PATH_LITERAL("ConversionFolder/")),
          std::make_unique<ConfigurableStorageDelegate>(), clock());

  // The directory should be created, and the database opened.
  storage->StoreSource(SourceBuilder(clock()->Now()).Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            storage->MaybeCreateAndStoreReport(DefaultTrigger()).status());
}

TEST_F(AttributionStorageSqlTest, DBinitializationSucceeds_HistogramRecorded) {
  base::HistogramTester histograms;

  OpenDatabase();
  storage()->StoreSource(SourceBuilder(clock()->Now()).Build());
  CloseDatabase();

  histograms.ExpectUniqueSample("Conversions.Storage.Sql.InitStatus2",
                                AttributionStorageSql::InitStatus::kSuccess, 1);
}

TEST_F(AttributionStorageSqlTest, MaxUint64StorageSucceeds) {
  constexpr uint64_t kMaxUint64 = std::numeric_limits<uint64_t>::max();

  OpenDatabase();

  // Ensure that reading and writing `uint64_t` fields via
  // `sql::Statement::ColumnInt64()` and `sql::Statement::BindInt64()` works
  // with the maximum value.

  const auto impression =
      SourceBuilder(clock()->Now()).SetSourceEventId(kMaxUint64).Build();
  storage()->StoreSource(impression);
  EXPECT_THAT(storage()->GetActiveSources(), ElementsAre(impression));

  EXPECT_EQ(
      CreateReportStatus::kSuccess,
      MaybeCreateAndStoreReport(
          TriggerBuilder()
              .SetTriggerData(kMaxUint64)
              .SetConversionDestination(impression.ConversionDestination())
              .SetReportingOrigin(impression.reporting_origin())
              .Build()));

  EXPECT_THAT(storage()->GetAttributionsToReport(clock()->Now()),
              ElementsAre(Field(&AttributionReport::trigger_data, kMaxUint64)));
}

TEST_F(AttributionStorageSqlTest, ImpressionNotExpired_NotDeleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, ImpressionExpired_Deleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());
  clock()->Advance(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(1u);
}

TEST_F(AttributionStorageSqlTest, ImpressionExpired_TooFrequent_NotDeleted) {
  OpenDatabase();

  delegate()->set_delete_expired_sources_frequency(base::Milliseconds(4));

  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());
  clock()->Advance(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest,
       ExpiredImpressionWithPendingConversion_NotDeleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  clock()->Advance(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, TwoImpressionsOneExpired_OneDeleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());
  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(4)).Build());

  clock()->Advance(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, ExpiredImpressionWithSentConversion_Deleted) {
  OpenDatabase();

  const int kReportTime = 5;
  delegate()->set_report_time_ms(kReportTime);

  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  clock()->Advance(base::Milliseconds(3));
  // Advance past the default report time.
  clock()->Advance(base::Milliseconds(kReportTime));

  std::vector<AttributionReport> reports =
      storage()->GetAttributionsToReport(clock()->Now());
  EXPECT_THAT(reports, SizeIs(1));
  EXPECT_TRUE(storage()->DeleteReport(*reports[0].conversion_id));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder(clock()->Now()).SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(1u);
}

}  // namespace content
