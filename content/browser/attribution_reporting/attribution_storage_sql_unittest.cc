// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include <functional>
#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/conversion_test_utils.h"
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
    storage_->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
    storage_->MaybeCreateAndStoreConversionReport(DefaultConversion());
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

  CreateReportStatus MaybeCreateAndStoreConversionReport(
      const StorableTrigger& conversion) {
    return storage_->MaybeCreateAndStoreConversionReport(conversion).status();
  }

 protected:
  base::ScopedTempDir temp_directory_;

 private:
  std::unique_ptr<AttributionStorage> storage_;
  ConfigurableStorageDelegate* delegate_ = nullptr;
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
  EXPECT_EQ(0u, storage()->GetConversionsToReport(clock()->Now()).size());
  CloseDatabase();

  EXPECT_FALSE(base::PathExists(db_path()));

  // DB init UMA should not be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);

  // Storing an impression should create and initialize the database.
  OpenDatabase();
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
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
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());
  CloseDatabase();
  OpenDatabase();
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());
}

TEST_F(AttributionStorageSqlTest, CorruptDatabase_RecoveredOnOpen) {
  OpenDatabase();
  AddReportToStorage();
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());
  CloseDatabase();

  // Corrupt the database.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(db_path()));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CORRUPT);

  // Open that database and ensure that it does not fail.
  EXPECT_NO_FATAL_FAILURE(OpenDatabase());

  // Data should be recovered.
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());

  EXPECT_TRUE(expecter.SawExpectedErrors());
}

//  Create an impression with two conversions (C1 and C2). Craft a query that
//  will target C2, which will in turn delete the impression. We should ensure
//  that C1 is properly deleted (conversions should not be stored unattributed).
TEST_F(AttributionStorageSqlTest, ClearDataWithVestigialConversion) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = clock()->Now();
  auto impression = ImpressionBuilder(start).SetExpiry(base::Days(30)).Build();
  storage()->StoreImpression(impression);

  clock()->Advance(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));

  // Use a time range that only intersects the last conversion.
  storage()->ClearData(clock()->Now(), clock()->Now(),
                       base::BindRepeating(std::equal_to<url::Origin>(),
                                           impression.impression_origin()));
  EXPECT_TRUE(storage()->GetConversionsToReport(base::Time::Max()).empty());

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
  auto impression = ImpressionBuilder(start).SetExpiry(base::Days(30)).Build();
  storage()->StoreImpression(impression);

  clock()->Advance(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));

  // Use a time range that only intersects the last conversion.
  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(clock()->Now(), clock()->Now(), null_filter);
  EXPECT_TRUE(storage()->GetConversionsToReport(base::Time::Max()).empty());

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
    auto impression =
        ImpressionBuilder(start).SetExpiry(base::Days(30)).Build();
    storage()->StoreImpression(impression);
    clock()->Advance(base::Days(1));
  }

  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));
  clock()->Advance(base::Days(1));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time::Min(), base::Time::Max(), null_filter);
  EXPECT_TRUE(storage()->GetConversionsToReport(base::Time::Max()).empty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation", 2, 1);
}

TEST_F(AttributionStorageSqlTest, MaxImpressionsPerOrigin) {
  OpenDatabase();
  delegate()->set_max_impressions_per_origin(2);
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));

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

TEST_F(AttributionStorageSqlTest, MaxConversionsPerOrigin) {
  OpenDatabase();
  delegate()->set_max_conversions_per_origin(2);
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));
  EXPECT_EQ(CreateReportStatus::kNoCapacityForConversionDestination,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));

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
  delegate()->set_max_conversions_per_impression(1);
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
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Days(30))
                                 .SetImpressionOrigin(impression_origin)
                                 .SetReportingOrigin(reporting_origin)
                                 .SetConversionOrigin(conversion_origin)
                                 .Build());

  clock()->Advance(base::Days(1));
  EXPECT_EQ(
      CreateReportStatus::kSuccess,
      MaybeCreateAndStoreConversionReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_EQ(1u, storage()->GetActiveImpressions().size());

  // Force the impression to be deactivated by ensuring that the next report is
  // in a different window.
  delegate()->set_report_time_ms(1);
  EXPECT_EQ(
      CreateReportStatus::kPriorityTooLow,
      MaybeCreateAndStoreConversionReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_EQ(0u, storage()->GetActiveImpressions().size());

  clock()->Advance(base::Days(1));
  EXPECT_TRUE(storage()->DeleteConversion(AttributionReport::Id(1)));
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
  delegate()->set_max_conversions_per_impression(1);
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
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Days(30))
                                 .SetImpressionOrigin(impression_origin)
                                 .SetReportingOrigin(reporting_origin)
                                 .SetConversionOrigin(conversion_origin)
                                 .Build());

  clock()->Advance(base::Days(1));
  EXPECT_EQ(
      CreateReportStatus::kSuccess,
      MaybeCreateAndStoreConversionReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_EQ(1u, storage()->GetActiveImpressions().size());

  // Force the impression to be deactivated by ensuring that the next report is
  // in a different window.
  delegate()->set_report_time_ms(1);
  EXPECT_EQ(
      CreateReportStatus::kPriorityTooLow,
      MaybeCreateAndStoreConversionReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_EQ(0u, storage()->GetActiveImpressions().size());

  clock()->Advance(base::Days(1));
  EXPECT_TRUE(storage()->DeleteConversion(AttributionReport::Id(1)));
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
  storage->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(CreateReportStatus::kNoMatchingImpressions,
            storage->MaybeCreateAndStoreConversionReport(DefaultConversion())
                .status());
}

TEST_F(AttributionStorageSqlTest, DatabaseDirDoesExist_CreateDirAndOpenDB) {
  // Give the storage layer a database directory that doesn't exist.
  std::unique_ptr<AttributionStorage> storage =
      std::make_unique<AttributionStorageSql>(
          temp_directory_.GetPath().Append(
              FILE_PATH_LITERAL("ConversionFolder/")),
          std::make_unique<ConfigurableStorageDelegate>(), clock());

  // The directory should be created, and the database opened.
  storage->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            storage->MaybeCreateAndStoreConversionReport(DefaultConversion())
                .status());
}

TEST_F(AttributionStorageSqlTest, DBinitializationSucceeds_HistogramRecorded) {
  base::HistogramTester histograms;

  OpenDatabase();
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
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
      ImpressionBuilder(clock()->Now()).SetData(kMaxUint64).Build();
  storage()->StoreImpression(impression);
  std::vector<StorableSource> impressions = storage()->GetActiveImpressions();
  EXPECT_EQ(1u, impressions.size());
  EXPECT_EQ(kMaxUint64, impressions[0].impression_data());

  EXPECT_EQ(
      CreateReportStatus::kSuccess,
      MaybeCreateAndStoreConversionReport(StorableTrigger(
          /*conversion_data=*/kMaxUint64, impression.ConversionDestination(),
          impression.reporting_origin(), /*event_source_trigger_data=*/0,
          /*priority=*/0, /*dedup_key=*/absl::nullopt)));

  std::vector<AttributionReport> reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(1u, reports.size());
  EXPECT_EQ(kMaxUint64, reports[0].conversion_data);
}

TEST_F(AttributionStorageSqlTest, ImpressionNotExpired_NotDeleted) {
  OpenDatabase();

  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());
  // Store another impression to trigger the expiry logic.
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, ImpressionExpired_Deleted) {
  OpenDatabase();

  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());
  clock()->Advance(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());

  CloseDatabase();
  ExpectImpressionRows(1u);
}

TEST_F(AttributionStorageSqlTest, ImpressionExpired_TooFrequent_NotDeleted) {
  OpenDatabase();

  delegate()->set_delete_expired_impressions_frequency(base::Milliseconds(4));

  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());
  clock()->Advance(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest,
       ExpiredImpressionWithPendingConversion_NotDeleted) {
  OpenDatabase();

  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, TwoImpressionsOneExpired_OneDeleted) {
  OpenDatabase();

  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(4))
                                 .Build());

  clock()->Advance(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, ExpiredImpressionWithSentConversion_Deleted) {
  OpenDatabase();

  const int kReportTime = 5;
  delegate()->set_report_time_ms(kReportTime);

  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());
  EXPECT_EQ(CreateReportStatus::kSuccess,
            MaybeCreateAndStoreConversionReport(DefaultConversion()));

  clock()->Advance(base::Milliseconds(3));
  // Advance past the default report time.
  clock()->Advance(base::Milliseconds(kReportTime));

  std::vector<AttributionReport> reports =
      storage()->GetConversionsToReport(clock()->Now());
  EXPECT_EQ(1u, reports.size());
  EXPECT_TRUE(storage()->DeleteConversion(*reports[0].conversion_id));
  // Store another impression to trigger the expiry logic.
  storage()->StoreImpression(ImpressionBuilder(clock()->Now())
                                 .SetExpiry(base::Milliseconds(3))
                                 .Build());

  CloseDatabase();
  ExpectImpressionRows(1u);
}

}  // namespace content
