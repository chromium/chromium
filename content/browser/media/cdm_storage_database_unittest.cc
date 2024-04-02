// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/media/cdm_storage_database.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "media/cdm/cdm_type.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

const media::CdmType kCdmType{1234, 5678};

const char kFileName[] = "file.txt";
const char kFileNameTwo[] = "file2.txt";

const std::vector<uint8_t> kPopulatedFileValue = {1, 2, 3};
const std::vector<uint8_t> kPopulatedFileValueTwo = {1, 2, 3, 4};
const std::vector<uint8_t> kPopulatedFileValueThree = {1, 2, 3, 4, 5};
}  // namespace

class CdmStorageDatabaseTest : public testing::Test {
 public:
  CdmStorageDatabaseTest() = default;
  ~CdmStorageDatabaseTest() override = default;

  void SetUp() override { ASSERT_TRUE(profile_path_.CreateUniqueTempDir()); }

 protected:
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/");
  const blink::StorageKey kTestStorageKeyTwo =
      blink::StorageKey::CreateFromStringForTesting("https://exampletwo.com/");

  std::unique_ptr<CdmStorageDatabase> cdm_storage_database_;

  void SetUpDatabase(base::FilePath file_path) {
    cdm_storage_database_ = std::make_unique<CdmStorageDatabase>(file_path);
  }

  base::ScopedTempDir profile_path_;

  base::HistogramTester histogram_tester_;
};

// This class is to test for when db_.Open() fails.
class CdmStorageDatabaseInvalidOpenTest : public CdmStorageDatabaseTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_path_.CreateUniqueTempDir());
    SetUpDatabase(profile_path_.GetPath());
  }

  void TearDown() override { ASSERT_TRUE(profile_path_.Delete()); }
};

// This class tests the CdmStorageDatabase when the path is empty.
class CdmStorageDatabaseInMemoryTest : public CdmStorageDatabaseTest {
 public:
  void SetUp() override { SetUpDatabase(base::FilePath()); }
};

// This class tests the CdmStorageDatabase with the CdmStorage path.
class CdmStorageDatabaseValidPathTest : public CdmStorageDatabaseTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_path_.CreateUniqueTempDir());
    const base::FilePath cdm_storage_path =
        profile_path_.GetPath().Append(kCdmStorageDatabaseFileName);
    SetUpDatabase(cdm_storage_path);
  }

  void TearDown() override {
    cdm_storage_database_->ClearDatabase();
    ASSERT_TRUE(profile_path_.Delete());
  }
};

class MockCdmStorageDatabaseV1 {
 public:
  // The database will be in-memory if `path` is empty.
  explicit MockCdmStorageDatabaseV1()
      : db_(sql::DatabaseOptions{.page_size = 32768, .cache_size = 8}) {}
  ~MockCdmStorageDatabaseV1() = default;

  void OpenDatabase(const base::FilePath& path) {
    ASSERT_TRUE(db_.Open(path));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db_, 1, 1));

    // Set up the table.
    static constexpr char kCreateTableSql[] =
        // clang-format off
      "CREATE TABLE IF NOT EXISTS cdm_storage("
          "storage_key TEXT NOT NULL,"
          "cdm_type BLOB NOT NULL,"
          "file_name TEXT NOT NULL,"
          "data BLOB NOT NULL,"
          "PRIMARY KEY(storage_key,cdm_type,file_name))";
    // clang-format on

    ASSERT_TRUE(db_.Execute(kCreateTableSql));
  }

  void WriteFile(const blink::StorageKey& storage_key,
                 const media::CdmType& cdm_type,
                 const std::string& file_name,
                 const std::vector<uint8_t>& data) {
    // clang-format off
    static constexpr char kInsertSql[] =
      "INSERT OR REPLACE INTO "
         "cdm_storage(storage_key,cdm_type,file_name,data) "
         "VALUES(?,?,?,?) ";
    // clang-format on

    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
    statement.BindString(0, storage_key.Serialize());
    statement.BindBlob(1, cdm_type.AsBytes());
    statement.BindString(2, file_name);
    statement.BindBlob(3, data);
    ASSERT_TRUE(statement.Run());
  }

  void CloseDatabase() { db_.Close(); }

 private:
  // Empty if the database is in-memory.
  sql::Database db_;
};

// This class is to test for the migration from v1 to v2 of the
// CdmStorageDatabase.
class CdmStorageDatabaseV1Test : public CdmStorageDatabaseTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_path_.CreateUniqueTempDir());
    const base::FilePath cdm_storage_path =
        profile_path_.GetPath().Append(kCdmStorageDatabaseFileName);
    SetUpDatabase(cdm_storage_path);
    cdm_storage_database_v1_ = std::make_unique<MockCdmStorageDatabaseV1>();
    SetUpV1Database(cdm_storage_path);
  }

  void SetUpV1Database(const base::FilePath& path) {
    cdm_storage_database_v1_->OpenDatabase(path);
    cdm_storage_database_v1_->WriteFile(kTestStorageKey, kCdmType, kFileName,
                                        kPopulatedFileValue);
    cdm_storage_database_v1_->WriteFile(kTestStorageKeyTwo, kCdmType,
                                        kFileNameTwo, kPopulatedFileValueTwo);
    cdm_storage_database_v1_->CloseDatabase();
  }

  void TearDown() override {
    ASSERT_TRUE(cdm_storage_database_->ClearDatabase());
    ASSERT_TRUE(profile_path_.Delete());
  }

 private:
  std::unique_ptr<MockCdmStorageDatabaseV1> cdm_storage_database_v1_;
};

// Update this test and follow CdmStorageDatabaseV1Test when V3 comes out.
class CdmStorageDatabaseV2Test : public CdmStorageDatabaseTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_path_.CreateUniqueTempDir());
    const base::FilePath cdm_storage_path =
        profile_path_.GetPath().Append(kCdmStorageDatabaseFileName);
    SetUpDatabase(cdm_storage_path);
    SetUpV2Database();
  }

  void SetUpV2Database() {
    cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType, kFileName,
                                     kPopulatedFileValue);
    cdm_storage_database_->WriteFile(kTestStorageKeyTwo, kCdmType, kFileNameTwo,
                                     kPopulatedFileValueTwo);
  }

  void TearDown() override {
    ASSERT_TRUE(cdm_storage_database_->ClearDatabase());
    ASSERT_TRUE(profile_path_.Delete());
  }
};

TEST_F(CdmStorageDatabaseV1Test, V1Works) {
  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValueTwo);
}

TEST_F(CdmStorageDatabaseV1Test, V1UpgradeWorks) {
  // Although it should be 6, when upgrading from v1->v2, we ALTER to add the
  // file_size column, and set the default as one. When the CDM rewrites to the
  // same file, the file_size will be updated to the proper value.
  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            2u);

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  // Now that we have written the same value to one of the files, we see that
  // the size is now equivalent to 3 (the size of kPopulatedFileValue) + 1,
  // which is 4.
  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            4u);

  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKeyTwo, base::Time::Min(), base::Time::Max()),
            1u);
  EXPECT_EQ(cdm_storage_database_->GetSizeForFile(kTestStorageKeyTwo, kCdmType,
                                                  kFileNameTwo),
            1u);

  cdm_storage_database_->CloseDatabaseForTesting();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(cdm_storage_database_->GetSizeForFile(kTestStorageKey, kCdmType,
                                                  kFileName),
            kPopulatedFileValue.size());
}

TEST_F(CdmStorageDatabaseV2Test, V2Works) {
  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValueTwo);
  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            kPopulatedFileValue.size() + kPopulatedFileValueTwo.size());

  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKeyTwo, base::Time::Min(), base::Time::Max()),
            kPopulatedFileValueTwo.size());
  EXPECT_EQ(cdm_storage_database_->GetSizeForFile(kTestStorageKeyTwo, kCdmType,
                                                  kFileNameTwo),
            kPopulatedFileValueTwo.size());
}

TEST_F(CdmStorageDatabaseInvalidOpenTest, EnsureOpenFails) {
  auto error = cdm_storage_database_->EnsureOpen();

  // The database cannot be opened in a temporary directory, as it requires to
  // be opened at a file, thus we get an SQL open error. We return this as a
  // `kDatabaseOpenError`.
  EXPECT_EQ(error, CdmStorageOpenError::kDatabaseOpenError);
}

TEST_F(CdmStorageDatabaseInMemoryTest, EnsureOpenWithoutErrors) {
  auto error = cdm_storage_database_->EnsureOpen();

  EXPECT_EQ(error, CdmStorageOpenError::kOk);
}

TEST_F(CdmStorageDatabaseInMemoryTest, FileManipulation) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  EXPECT_TRUE(
      cdm_storage_database_->DeleteFile(kTestStorageKey, kCdmType, kFileName));

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

TEST_F(CdmStorageDatabaseInMemoryTest, DeleteDatabase) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->ClearDatabase());

  auto error = cdm_storage_database_->EnsureOpen();

  EXPECT_EQ(error, CdmStorageOpenError::kOk);

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

TEST_F(CdmStorageDatabaseInMemoryTest, DeleteForStorageKey) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKey, kCdmType, kFileNameTwo, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKeyTwo, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileNameTwo),
      kPopulatedFileValue);

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValueTwo);

  EXPECT_TRUE(cdm_storage_database_->DeleteData(
      base::NullCallback(), kTestStorageKey, time_now, base::Time::Max()));

  // Expect that for the storage key, all of the file content returned is empty.
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileNameTwo)
          ->empty());

  // Expect that its not deleted for other storage keys.
  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValueTwo);
}

TEST_F(CdmStorageDatabaseInMemoryTest, DeleteForStorageKeyWithNoData) {
  auto time_now = base::Time::Now();

  // Even if there is no data for the storage key, the SQL statement should
  // still run properly.
  EXPECT_TRUE(cdm_storage_database_->DeleteData(
      base::NullCallback(), kTestStorageKey, time_now, base::Time::Max()));

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

TEST_F(CdmStorageDatabaseInMemoryTest,
       DeleteForTimeFrameMultipleFilesAndStorageKeysTimeFrame) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKey, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValueThree));

  EXPECT_EQ(
      cdm_storage_database_->GetSizeForTimeFrame(time_now, base::Time::Max()),
      kPopulatedFileValue.size() + kPopulatedFileValueTwo.size() +
          kPopulatedFileValueThree.size());

  EXPECT_TRUE(cdm_storage_database_->DeleteData(
      base::NullCallback(), blink::StorageKey(), time_now, base::Time::Max()));

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileNameTwo)
          ->empty());

  EXPECT_TRUE(cdm_storage_database_
                  ->ReadFile(kTestStorageKeyTwo, kCdmType, kFileNameTwo)
                  ->empty());

  EXPECT_EQ(
      cdm_storage_database_->GetSizeForTimeFrame(time_now, base::Time::Max()),
      0u);

  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            0u);
}

TEST_F(CdmStorageDatabaseInMemoryTest, DeleteForTimeFrameWithNoData) {
  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            0u);

  // Even if there is no data for the storage key, the SQL statement should
  // still run properly.
  EXPECT_TRUE(cdm_storage_database_->DeleteData(
      base::NullCallback(), blink::StorageKey(), base::Time::Min(),
      base::Time::Max()));
}

TEST_F(CdmStorageDatabaseInMemoryTest, WriteFileForBigData) {
  std::vector<uint8_t> big_data;
  big_data.resize(1000000);

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, big_data));

  histogram_tester_.ExpectTotalCount(
      "Media.EME.CdmStorageDatabase.WriteFileForBigData", 1);

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      big_data);
}

TEST_F(CdmStorageDatabaseInMemoryTest, GetUsageForFile) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(cdm_storage_database_->GetSizeForFile(kTestStorageKey, kCdmType,
                                                  kFileName),
            kPopulatedFileValue.size());
}

TEST_F(CdmStorageDatabaseInMemoryTest, GetUsageForNonExistentFile) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(cdm_storage_database_->GetSizeForFile(kTestStorageKeyTwo, kCdmType,
                                                  kFileName),
            0u);
}

TEST_F(CdmStorageDatabaseInMemoryTest, GetUsageForStorageKey) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKey, base::Time::Min(), time_now),
            0u);
  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKey, time_now, base::Time::Max()),
            kPopulatedFileValue.size());
  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKey, base::Time::Min(), base::Time::Max()),
            kPopulatedFileValue.size());
}

TEST_F(CdmStorageDatabaseInMemoryTest, GetUsageForNonExistentStorageKey) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKeyTwo, base::Time::Min(), base::Time::Max()),
            0u);
}

TEST_F(CdmStorageDatabaseInMemoryTest, GetUsageForMultipleFilesInStorageKey) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKey, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKey, time_now, base::Time::Max()),
            kPopulatedFileValue.size() + kPopulatedFileValueTwo.size());
  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKey, base::Time::Min(), base::Time::Max()),
            kPopulatedFileValue.size() + kPopulatedFileValueTwo.size());
}

TEST_F(CdmStorageDatabaseInMemoryTest, GetUsageForTimeFrame) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->GetSizeForTimeFrame(time_now, base::Time::Max()),
      kPopulatedFileValue.size());
  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            kPopulatedFileValue.size());
}

TEST_F(CdmStorageDatabaseInMemoryTest, GetUsageForNonExistentTimeFrame) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Now(),
                                                       base::Time::Max()),
            0u);
  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            kPopulatedFileValue.size());
}

TEST_F(CdmStorageDatabaseInMemoryTest,
       GetUsageForMultipleFilesAndStorageKeysTimeFrame) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKey, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValueThree));

  EXPECT_EQ(
      cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(), time_now),
      0u);

  EXPECT_EQ(
      cdm_storage_database_->GetSizeForTimeFrame(time_now, base::Time::Max()),
      kPopulatedFileValue.size() + kPopulatedFileValueTwo.size() +
          kPopulatedFileValueThree.size());

  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            kPopulatedFileValue.size() + kPopulatedFileValueTwo.size() +
                kPopulatedFileValueThree.size());
}

TEST_F(CdmStorageDatabaseInMemoryTest, GetUsagePerAllStorageKeys) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileName, kPopulatedFileValueThree));

  auto storage_keys = cdm_storage_database_->GetUsagePerAllStorageKeys();

  const CdmStorageKeyUsageSize& expected_storage_keys = {
      {kTestStorageKey, kPopulatedFileValue.size()},
      {kTestStorageKeyTwo,
       kPopulatedFileValueTwo.size() + kPopulatedFileValueThree.size()}};

  EXPECT_EQ(storage_keys, expected_storage_keys);
}

TEST_F(CdmStorageDatabaseInMemoryTest, GetUsagePerAllStorageKeysTimeBound) {
  auto now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileName, kPopulatedFileValueThree));

  auto storage_keys_not_populated =
      cdm_storage_database_->GetUsagePerAllStorageKeys(base::Time::Min(), now);

  auto storage_keys_in_time_frame =
      cdm_storage_database_->GetUsagePerAllStorageKeys(now, base::Time::Now());

  auto all_storage_keys = cdm_storage_database_->GetUsagePerAllStorageKeys();

  const CdmStorageKeyUsageSize& expected_storage_keys = {
      {kTestStorageKey, kPopulatedFileValue.size()},
      {kTestStorageKeyTwo,
       kPopulatedFileValueTwo.size() + kPopulatedFileValueThree.size()}};

  EXPECT_TRUE(storage_keys_not_populated.size() == 0);

  EXPECT_EQ(storage_keys_in_time_frame, expected_storage_keys);

  EXPECT_EQ(all_storage_keys, expected_storage_keys);
}

TEST_F(CdmStorageDatabaseInMemoryTest, DeleteDataForFilter) {
  auto now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  // Try to remove the data using a deletelist that doesn't include
  // the current URL. Data should not be deleted.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_not_included =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder_not_included->AddOrigin(kTestStorageKeyTwo.origin());

  cdm_storage_database_->DeleteData(
      filter_builder_not_included->BuildStorageKeyFilter(), blink::StorageKey(),
      now, base::Time::Max());

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  // When on kDelete mode, the storage key should  be deleted.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_delete =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);

  filter_builder_delete->AddOrigin(kTestStorageKey.origin());

  // Should not apply as time ranges do not match.
  cdm_storage_database_->DeleteData(
      filter_builder_delete->BuildStorageKeyFilter(), blink::StorageKey(),
      base::Time::Min(), now);

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  // Should delete in the time range.
  cdm_storage_database_->DeleteData(
      filter_builder_delete->BuildStorageKeyFilter(), blink::StorageKey(), now,
      base::Time::Max());

  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(kTestStorageKey, now,
                                                        base::Time::Max()),
            0u);
}

TEST_F(CdmStorageDatabaseInMemoryTest, PreserveDataForFilter) {
  auto now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  // When on kPreserve mode, the storage keys should not be deleted.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_preserve_all =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve);

  filter_builder_preserve_all->AddOrigin(kTestStorageKey.origin());
  filter_builder_preserve_all->AddOrigin(kTestStorageKeyTwo.origin());

  cdm_storage_database_->DeleteData(
      filter_builder_preserve_all->BuildStorageKeyFilter(), blink::StorageKey(),
      now, base::Time::Max());

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValueTwo);

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_preserve_one =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve);

  filter_builder_preserve_one->AddOrigin(kTestStorageKey.origin());

  // Even with the filter builder only preserving `kTestStorageKey`, the time
  // frame specified should make the cdm storage database not delete anything at
  // all.
  cdm_storage_database_->DeleteData(
      filter_builder_preserve_one->BuildStorageKeyFilter(), blink::StorageKey(),
      base::Time::Min(), now);

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValueTwo);

  cdm_storage_database_->DeleteData(
      filter_builder_preserve_one->BuildStorageKeyFilter(), blink::StorageKey(),
      now, base::Time::Max());

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->GetSizeForFile(kTestStorageKeyTwo, kCdmType,
                                                  kFileNameTwo),
            0);
}

TEST_F(CdmStorageDatabaseValidPathTest, EnsureOpenWithoutErrors) {
  auto error = cdm_storage_database_->EnsureOpen();

  EXPECT_EQ(error, CdmStorageOpenError::kOk);
}

TEST_F(CdmStorageDatabaseValidPathTest, FileManipulation) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  EXPECT_TRUE(
      cdm_storage_database_->DeleteFile(kTestStorageKey, kCdmType, kFileName));

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

TEST_F(CdmStorageDatabaseValidPathTest, DeleteDatabase) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->ClearDatabase());

  auto error = cdm_storage_database_->EnsureOpen();

  EXPECT_EQ(error, CdmStorageOpenError::kOk);

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

TEST_F(CdmStorageDatabaseValidPathTest, DeleteForStorageKey) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKey, kCdmType, kFileNameTwo, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKeyTwo, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileNameTwo),
      kPopulatedFileValue);

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValue);

  EXPECT_TRUE(cdm_storage_database_->DeleteData(
      base::NullCallback(), kTestStorageKey, time_now, base::Time::Max()));

  // Expect that for the storage key, all of the file content returned is empty.
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileNameTwo)
          ->empty());

  // Expect that its not deleted for other storage keys.
  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValue);
}

TEST_F(CdmStorageDatabaseValidPathTest, DeleteForStorageKeyWithNoData) {
  auto time_now = base::Time::Now();

  // Even if there is no data for the storage key, the SQL statement should
  // still run properly.
  EXPECT_TRUE(cdm_storage_database_->DeleteData(
      base::NullCallback(), kTestStorageKey, time_now, base::Time::Max()));

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());
}

TEST_F(CdmStorageDatabaseValidPathTest,
       DeleteForTimeFrameMultipleFilesAndStorageKeysTimeFrame) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKey, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValueThree));

  EXPECT_EQ(
      cdm_storage_database_->GetSizeForTimeFrame(time_now, base::Time::Max()),
      kPopulatedFileValue.size() + kPopulatedFileValueTwo.size() +
          kPopulatedFileValueThree.size());

  EXPECT_TRUE(cdm_storage_database_->DeleteData(
      base::NullCallback(), blink::StorageKey(), time_now, base::Time::Max()));

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName)
          ->empty());

  EXPECT_TRUE(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileNameTwo)
          ->empty());

  EXPECT_TRUE(cdm_storage_database_
                  ->ReadFile(kTestStorageKeyTwo, kCdmType, kFileNameTwo)
                  ->empty());

  EXPECT_EQ(
      cdm_storage_database_->GetSizeForTimeFrame(time_now, base::Time::Max()),
      0u);

  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            0u);
}

TEST_F(CdmStorageDatabaseValidPathTest, DeleteForTimeFrameWithNoData) {
  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            0u);

  // Even if there is no data for the storage key, the SQL statement should
  // still run properly.
  EXPECT_TRUE(cdm_storage_database_->DeleteData(
      base::NullCallback(), blink::StorageKey(), base::Time::Min(),
      base::Time::Max()));
}

TEST_F(CdmStorageDatabaseValidPathTest, WriteFileForBigData) {
  std::vector<uint8_t> big_data;
  big_data.resize(1000000);

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, big_data));

  const char uma_name[] = "Media.EME.CdmStorageDatabase.WriteFileForBigData";
  histogram_tester_.ExpectTotalCount(uma_name, 1);
  histogram_tester_.ExpectBucketCount(uma_name, true, 1);

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      big_data);
}

TEST_F(CdmStorageDatabaseValidPathTest, GetUsageForFile) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(cdm_storage_database_->GetSizeForFile(kTestStorageKey, kCdmType,
                                                  kFileName),
            kPopulatedFileValue.size());
}

TEST_F(CdmStorageDatabaseValidPathTest, GetUsageForNonExistentFile) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(cdm_storage_database_->GetSizeForFile(kTestStorageKeyTwo, kCdmType,
                                                  kFileName),
            0u);
}

TEST_F(CdmStorageDatabaseValidPathTest, GetUsageForStorageKey) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKey, base::Time::Min(), time_now),
            0u);
  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKey, time_now, base::Time::Max()),
            kPopulatedFileValue.size());
  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKey, base::Time::Min(), base::Time::Max()),
            kPopulatedFileValue.size());
}

TEST_F(CdmStorageDatabaseValidPathTest, GetUsageForNonExistentStorageKey) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKeyTwo, base::Time::Min(), base::Time::Max()),
            0u);
}

TEST_F(CdmStorageDatabaseValidPathTest, GetUsageForMultipleFilesInStorageKey) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKey, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKey, time_now, base::Time::Max()),
            kPopulatedFileValue.size() + kPopulatedFileValueTwo.size());
  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(
                kTestStorageKey, base::Time::Min(), base::Time::Max()),
            kPopulatedFileValue.size() + kPopulatedFileValueTwo.size());
}

TEST_F(CdmStorageDatabaseValidPathTest, GetUsageForTimeFrame) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(
      cdm_storage_database_->GetSizeForTimeFrame(time_now, base::Time::Max()),
      kPopulatedFileValue.size());
  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            kPopulatedFileValue.size());
}

TEST_F(CdmStorageDatabaseValidPathTest, GetUsageForNonExistentTimeFrame) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Now(),
                                                       base::Time::Max()),
            0u);
  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            kPopulatedFileValue.size());
}

TEST_F(CdmStorageDatabaseValidPathTest,
       GetUsageForMultipleFilesAndStorageKeysTimeFrame) {
  auto time_now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKey, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValueThree));

  EXPECT_EQ(
      cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(), time_now),
      0u);

  EXPECT_EQ(
      cdm_storage_database_->GetSizeForTimeFrame(time_now, base::Time::Max()),
      kPopulatedFileValue.size() + kPopulatedFileValueTwo.size() +
          kPopulatedFileValueThree.size());

  EXPECT_EQ(cdm_storage_database_->GetSizeForTimeFrame(base::Time::Min(),
                                                       base::Time::Max()),
            kPopulatedFileValue.size() + kPopulatedFileValueTwo.size() +
                kPopulatedFileValueThree.size());
}

TEST_F(CdmStorageDatabaseValidPathTest, GetUsagePerAllStorageKeys) {
  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileName, kPopulatedFileValueThree));

  auto storage_keys = cdm_storage_database_->GetUsagePerAllStorageKeys();

  const CdmStorageKeyUsageSize& expected_storage_keys = {
      {kTestStorageKey, kPopulatedFileValue.size()},
      {kTestStorageKeyTwo,
       kPopulatedFileValueTwo.size() + kPopulatedFileValueThree.size()}};

  EXPECT_EQ(storage_keys, expected_storage_keys);
}

TEST_F(CdmStorageDatabaseValidPathTest, GetUsagePerAllStorageKeysTimeBound) {
  auto now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));
  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileName, kPopulatedFileValueThree));

  auto storage_keys_not_populated =
      cdm_storage_database_->GetUsagePerAllStorageKeys(base::Time::Min(), now);

  auto storage_keys_in_time_frame =
      cdm_storage_database_->GetUsagePerAllStorageKeys(now, base::Time::Now());

  auto all_storage_keys = cdm_storage_database_->GetUsagePerAllStorageKeys();

  const CdmStorageKeyUsageSize& expected_storage_keys = {
      {kTestStorageKey, kPopulatedFileValue.size()},
      {kTestStorageKeyTwo,
       kPopulatedFileValueTwo.size() + kPopulatedFileValueThree.size()}};

  EXPECT_TRUE(storage_keys_not_populated.size() == 0);

  EXPECT_EQ(storage_keys_in_time_frame, expected_storage_keys);

  EXPECT_EQ(all_storage_keys, expected_storage_keys);
}

TEST_F(CdmStorageDatabaseValidPathTest, DeleteDataForFilter) {
  auto now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  // Try to remove the data using a deletelist that doesn't include
  // the current URL. Data should not be deleted.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_not_included =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder_not_included->AddOrigin(kTestStorageKeyTwo.origin());

  cdm_storage_database_->DeleteData(
      filter_builder_not_included->BuildStorageKeyFilter(), blink::StorageKey(),
      now, base::Time::Max());

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  // When on kDelete mode, the storage key should  be deleted.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_delete =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete);

  filter_builder_delete->AddOrigin(kTestStorageKey.origin());

  // Should not apply as time ranges do not match.
  cdm_storage_database_->DeleteData(
      filter_builder_delete->BuildStorageKeyFilter(), blink::StorageKey(),
      base::Time::Min(), now);

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);

  // Should delete in the time range.
  cdm_storage_database_->DeleteData(
      filter_builder_delete->BuildStorageKeyFilter(), blink::StorageKey(), now,
      base::Time::Max());

  EXPECT_EQ(cdm_storage_database_->GetSizeForStorageKey(kTestStorageKey, now,
                                                        base::Time::Max()),
            0u);
}

TEST_F(CdmStorageDatabaseValidPathTest, PreserveDataForFilter) {
  auto now = base::Time::Now();

  EXPECT_TRUE(cdm_storage_database_->WriteFile(kTestStorageKey, kCdmType,
                                               kFileName, kPopulatedFileValue));

  EXPECT_TRUE(cdm_storage_database_->WriteFile(
      kTestStorageKeyTwo, kCdmType, kFileNameTwo, kPopulatedFileValueTwo));

  // When on kPreserve mode, the storage keys should not be deleted.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_preserve_all =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve);

  filter_builder_preserve_all->AddOrigin(kTestStorageKey.origin());
  filter_builder_preserve_all->AddOrigin(kTestStorageKeyTwo.origin());

  cdm_storage_database_->DeleteData(
      filter_builder_preserve_all->BuildStorageKeyFilter(), blink::StorageKey(),
      now, base::Time::Max());

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValueTwo);

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_preserve_one =
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve);

  filter_builder_preserve_one->AddOrigin(kTestStorageKey.origin());

  // Even with the filter builder only preserving `kTestStorageKey`, the time
  // frame specified should make the cdm storage database not delete anything at
  // all.
  cdm_storage_database_->DeleteData(
      filter_builder_preserve_one->BuildStorageKeyFilter(), blink::StorageKey(),
      base::Time::Min(), now);

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->ReadFile(kTestStorageKeyTwo, kCdmType,
                                            kFileNameTwo),
            kPopulatedFileValueTwo);

  cdm_storage_database_->DeleteData(
      filter_builder_preserve_one->BuildStorageKeyFilter(), blink::StorageKey(),
      now, base::Time::Max());

  EXPECT_EQ(
      cdm_storage_database_->ReadFile(kTestStorageKey, kCdmType, kFileName),
      kPopulatedFileValue);
  EXPECT_EQ(cdm_storage_database_->GetSizeForFile(kTestStorageKeyTwo, kCdmType,
                                                  kFileNameTwo),
            0);
}

}  // namespace content
