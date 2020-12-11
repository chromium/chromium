// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_sql.h"

#include <functional>
#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "sql/database.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ConversionStorageSqlTest : public testing::Test {
 public:
  ConversionStorageSqlTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void OpenDatabase() {
    storage_.reset();
    auto delegate = std::make_unique<ConfigurableStorageDelegate>();
    delegate_ = delegate.get();
    storage_ = std::make_unique<ConversionStorageSql>(
        temp_directory_.GetPath(), std::move(delegate), &clock_);
  }

  void CloseDatabase() { storage_.reset(); }

  void AddReportToStorage() {
    storage_->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
    storage_->MaybeCreateAndStoreConversionReports(DefaultConversion());
  }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions"));
  }

  base::SimpleTestClock* clock() { return &clock_; }

  ConversionStorage* storage() { return storage_.get(); }

  ConfigurableStorageDelegate* delegate() { return delegate_; }

 protected:
  base::ScopedTempDir temp_directory_;

 private:
  std::unique_ptr<ConversionStorage> storage_;
  ConfigurableStorageDelegate* delegate_ = nullptr;
  base::SimpleTestClock clock_;
};

TEST_F(ConversionStorageSqlTest,
       DatabaseInitialized_TablesAndIndexesLazilyInitialized) {
  OpenDatabase();
  CloseDatabase();

  // An unused ConversionStorageSql instance should not create the database.
  EXPECT_FALSE(base::PathExists(db_path()));

  // Operations which don't need to run on an empty database should not create
  // the database.
  OpenDatabase();
  EXPECT_EQ(0u, storage()->GetConversionsToReport(clock()->Now()).size());
  CloseDatabase();

  EXPECT_FALSE(base::PathExists(db_path()));

  // Storing an impression should create and initialize the database.
  OpenDatabase();
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  CloseDatabase();

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [impressions] and [conversions].
    EXPECT_EQ(2u, sql::test::CountSQLTables(&raw_db));

    // [conversion_origin_idx], [impression_expiry_idx],
    // [impression_origin_idx], [conversion_report_time_idx],
    // [conversion_impression_id_idx].
    EXPECT_EQ(5u, sql::test::CountSQLIndices(&raw_db));
  }
}

TEST_F(ConversionStorageSqlTest, DatabaseReopened_DataPersisted) {
  OpenDatabase();
  AddReportToStorage();
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());
  CloseDatabase();
  OpenDatabase();
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());
}

TEST_F(ConversionStorageSqlTest, CorruptDatabase_RecoveredOnOpen) {
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
TEST_F(ConversionStorageSqlTest, ClearDataWithVestigialConversion) {
  OpenDatabase();

  base::Time start = clock()->Now();
  auto impression =
      ImpressionBuilder(start).SetExpiry(base::TimeDelta::FromDays(30)).Build();
  storage()->StoreImpression(impression);

  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  // Use a time range that only intersects the last conversion.
  storage()->ClearData(clock()->Now(), clock()->Now(),
                       base::BindRepeating(std::equal_to<url::Origin>(),
                                           impression.impression_origin()));
  EXPECT_TRUE(storage()->GetConversionsToReport(base::Time::Max()).empty());

  CloseDatabase();

  // Verify that everything is deleted.
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  size_t conversion_rows;
  size_t impression_rows;
  sql::test::CountTableRows(&raw_db, "conversions", &conversion_rows);
  sql::test::CountTableRows(&raw_db, "impressions", &impression_rows);

  EXPECT_EQ(0u, conversion_rows);
  EXPECT_EQ(0u, impression_rows);
}

// Same as the above test, but with a null filter.
TEST_F(ConversionStorageSqlTest, ClearAllDataWithVestigialConversion) {
  OpenDatabase();

  base::Time start = clock()->Now();
  auto impression =
      ImpressionBuilder(start).SetExpiry(base::TimeDelta::FromDays(30)).Build();
  storage()->StoreImpression(impression);

  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  // Use a time range that only intersects the last conversion.
  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(clock()->Now(), clock()->Now(), null_filter);
  EXPECT_TRUE(storage()->GetConversionsToReport(base::Time::Max()).empty());

  CloseDatabase();

  // Verify that everything is deleted.
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  size_t conversion_rows;
  size_t impression_rows;
  sql::test::CountTableRows(&raw_db, "conversions", &conversion_rows);
  sql::test::CountTableRows(&raw_db, "impressions", &impression_rows);

  EXPECT_EQ(0u, conversion_rows);
  EXPECT_EQ(0u, impression_rows);
}

// The max time range with a null filter should delete everything.
TEST_F(ConversionStorageSqlTest, DeleteEverything) {
  OpenDatabase();

  base::Time start = clock()->Now();
  for (int i = 0; i < 10; i++) {
    auto impression = ImpressionBuilder(start)
                          .SetExpiry(base::TimeDelta::FromDays(30))
                          .Build();
    storage()->StoreImpression(impression);
    clock()->Advance(base::TimeDelta::FromDays(1));
  }

  EXPECT_EQ(
      10, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(
      10, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time::Min(), base::Time::Max(), null_filter);
  EXPECT_TRUE(storage()->GetConversionsToReport(base::Time::Max()).empty());

  CloseDatabase();

  // Verify that everything is deleted.
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  size_t conversion_rows;
  size_t impression_rows;
  sql::test::CountTableRows(&raw_db, "conversions", &conversion_rows);
  sql::test::CountTableRows(&raw_db, "impressions", &impression_rows);

  EXPECT_EQ(0u, conversion_rows);
  EXPECT_EQ(0u, impression_rows);
}

TEST_F(ConversionStorageSqlTest, MaxImpressionsPerOrigin) {
  OpenDatabase();
  delegate()->set_max_impressions_per_origin(2);
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(
      2, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t impression_rows;
  sql::test::CountTableRows(&raw_db, "impressions", &impression_rows);
  EXPECT_EQ(2u, impression_rows);
}

TEST_F(ConversionStorageSqlTest, MaxConversionsPerOrigin) {
  OpenDatabase();
  delegate()->set_max_conversions_per_origin(2);
  storage()->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  EXPECT_EQ(
      1, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));
  EXPECT_EQ(
      0, storage()->MaybeCreateAndStoreConversionReports(DefaultConversion()));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t conversion_rows;
  sql::test::CountTableRows(&raw_db, "conversions", &conversion_rows);
  EXPECT_EQ(2u, conversion_rows);
}

TEST_F(ConversionStorageSqlTest, CantOpenDb_FailsSilentlyInRelease) {
  base::CreateDirectoryAndGetError(db_path(), nullptr);

  auto sql_storage = std::make_unique<ConversionStorageSql>(
      temp_directory_.GetPath(),
      std::make_unique<ConfigurableStorageDelegate>(), clock());
  sql_storage->set_ignore_errors_for_testing(true);

  std::unique_ptr<ConversionStorage> storage = std::move(sql_storage);

  // These calls should be no-ops.
  storage->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(0,
            storage->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

TEST_F(ConversionStorageSqlTest, DatabaseDirDoesExist_CreateDirAndOpenDB) {
  // Give the storage layer a database directory that doesn't exist.
  std::unique_ptr<ConversionStorage> storage =
      std::make_unique<ConversionStorageSql>(
          temp_directory_.GetPath().Append(
              FILE_PATH_LITERAL("ConversionFolder/")),
          std::make_unique<ConfigurableStorageDelegate>(), clock());

  // The directory should be created, and the database opened.
  storage->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
  EXPECT_EQ(1,
            storage->MaybeCreateAndStoreConversionReports(DefaultConversion()));
}

}  // namespace content
