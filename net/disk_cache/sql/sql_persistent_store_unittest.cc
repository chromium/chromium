// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store.h"

#include <cstdint>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "net/base/cache_type.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

// Default max cache size for tests, 10 MB.
inline constexpr int64_t kDefaultMaxBytes = 10 * 1024 * 1024;

// Test fixture for SqlPersistentStore tests.
class SqlPersistentStoreTest : public testing::Test {
 public:
  // Sets up a temporary directory and a background task runner for each test.
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    background_task_runner_ =
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  }

  // Cleans up the store and ensures all background tasks are completed.
  void TearDown() override {
    store_.reset();
    // Make sure all background tasks are done before returning.
    FlushPendingTask();
  }

 protected:
  // Returns the path to the temporary directory.
  base::FilePath GetTempPath() const { return temp_dir_.GetPath(); }

  // Returns the full path to the SQLite database file.
  base::FilePath GetDatabaseFilePath() const {
    return GetTempPath().Append(kSqlBackendDatabaseFileName);
  }

  // Creates a SqlPersistentStore instance.
  void CreateStore(int64_t max_bytes = kDefaultMaxBytes) {
    store_ = SqlPersistentStore::Create(GetTempPath(), max_bytes,
                                        net::CacheType::DISK_CACHE,
                                        background_task_runner_);
  }

  // Initializes the store and waits for the operation to complete.
  SqlPersistentStore::Error Init() {
    base::RunLoop run_loop;
    SqlPersistentStore::Error result;
    store_->Initialize(base::BindOnce(
        [](base::RunLoop* run_loop, SqlPersistentStore::Error* result,
           SqlPersistentStore::Error error) {
          *result = error;
          run_loop->Quit();
        },
        &run_loop, &result));
    run_loop.Run();
    return result;
  }

  // Helper function to create, initialize, and then close a store.
  void InitializeTestStore() {
    CreateStore();
    ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
    store_.reset();
    FlushPendingTask();
  }

  // Makes the database file unwritable to test error handling.
  void MakeFileUnwritable() {
    file_permissions_restorer_ =
        std::make_unique<base::FilePermissionRestorer>(GetDatabaseFilePath());
    ASSERT_TRUE(base::MakeFileUnwritable(GetDatabaseFilePath()));
  }

  // Synchronously gets the entry count.
  int32_t GetEntryCount() {
    base::RunLoop run_loop;
    int32_t result = 0;
    store_->GetEntryCount(base::BindOnce(
        [](base::RunLoop* run_loop, int32_t* result, int32_t count) {
          *result = count;
          run_loop->Quit();
        },
        &run_loop, &result));
    run_loop.Run();
    return result;
  }

  // Synchronously gets the total size of all entries.
  int64_t GetSizeOfAllEntries() {
    base::RunLoop run_loop;
    int64_t result = 0;
    store_->GetSizeOfAllEntries(base::BindOnce(
        [](base::RunLoop* run_loop, int64_t* result, int64_t size) {
          *result = size;
          run_loop->Quit();
        },
        &run_loop, &result));
    run_loop.Run();
    return result;
  }

  // Ensures all tasks on the background thread have completed.
  void FlushPendingTask() {
    base::RunLoop run_loop;
    background_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Manually opens the SQLite database for direct inspection.
  std::unique_ptr<sql::Database> ManuallyOpenDatabase() {
    auto db = std::make_unique<sql::Database>(
        sql::DatabaseOptions()
            .set_exclusive_locking(true)
#if BUILDFLAG(IS_WIN)
            .set_exclusive_database_file_lock(true)
#endif  // IS_WIN
            .set_preload(true)
            .set_wal_mode(true),
        sql::Database::Tag("HttpCacheDiskCache"));
    CHECK(db->Open(GetDatabaseFilePath()));
    return db;
  }

  // Manually opens the meta table within the database.
  std::unique_ptr<sql::MetaTable> ManuallyOpenMetaTable(sql::Database* db) {
    auto mata_table = std::make_unique<sql::MetaTable>();
    CHECK(mata_table->Init(db, kSqlBackendCurrentDatabaseVersion,
                           kSqlBackendCurrentDatabaseVersion));
    return mata_table;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::DEFAULT};
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  std::unique_ptr<SqlPersistentStore> store_;
  std::unique_ptr<base::FilePermissionRestorer> file_permissions_restorer_;
};

// Tests that a new database is created and initialized successfully.
TEST_F(SqlPersistentStoreTest, InitNew) {
  const int64_t kMaxBytes = 10 * 1024 * 1024;
  CreateStore(kMaxBytes);
  EXPECT_EQ(Init(), SqlPersistentStore::Error::kOk);
  EXPECT_EQ(store_->MaxSize(), kMaxBytes);
  EXPECT_EQ(store_->MaxFileSize(), kSqlBackendMinFileSizeLimit);
}

// Tests initialization when max_bytes is zero. This should trigger automatic
// sizing based on available disk space.
TEST_F(SqlPersistentStoreTest, InitWithZeroMaxBytes) {
  CreateStore(0);
  EXPECT_EQ(Init(), SqlPersistentStore::Error::kOk);
  // When `max_bytes` is zero, the following values are calculated using the
  // free disk space.
  EXPECT_GT(store_->MaxSize(), 0);
  EXPECT_GT(store_->MaxFileSize(), 0);
}

// Tests that an existing, valid database can be opened and initialized.
TEST_F(SqlPersistentStoreTest, InitExisting) {
  InitializeTestStore();

  // Create a new store with the same path, which should open the existing DB.
  CreateStore();
  EXPECT_EQ(Init(), SqlPersistentStore::Error::kOk);
}

// Tests that a database with a future (incompatible) version is razed
// (deleted and recreated).
TEST_F(SqlPersistentStoreTest, InitRazedTooNew) {
  InitializeTestStore();

  {
    // Manually open the database and set a future version number.
    auto db = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db.get());
    ASSERT_TRUE(
        meta_table->SetVersionNumber(kSqlBackendCurrentDatabaseVersion + 1));
    ASSERT_TRUE(meta_table->SetCompatibleVersionNumber(
        kSqlBackendCurrentDatabaseVersion + 1));
    // Add some data to verify it gets deleted.
    meta_table->SetValue("SomeNewData", 1);
    int64_t value = 0;
    EXPECT_TRUE(meta_table->GetValue("SomeNewData", &value));
    EXPECT_EQ(value, 1);
  }

  // Re-initializing the store should detect the future version and raze the DB.
  InitializeTestStore();

  // Verify that the old data is gone.
  auto db = ManuallyOpenDatabase();
  auto meta_table = ManuallyOpenMetaTable(db.get());
  int64_t value = 0;
  EXPECT_FALSE(meta_table->GetValue("SomeNewData", &value));
}

// Tests that initialization fails if the target directory path is obstructed
// by a file.
TEST_F(SqlPersistentStoreTest, InitFailsWithCreationDirectoryFailure) {
  // Create a file where the database directory is supposed to be.
  base::FilePath db_dir_path = GetTempPath().Append(FILE_PATH_LITERAL("db"));
  ASSERT_TRUE(base::WriteFile(db_dir_path, ""));

  store_ = SqlPersistentStore::Create(db_dir_path, kDefaultMaxBytes,
                                      net::CacheType::DISK_CACHE,
                                      background_task_runner_);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kFailedToCreateDirectory);
}

// Tests that initialization fails if the database file is not writable.
TEST_F(SqlPersistentStoreTest, InitFailsWithUnwritableFile) {
  InitializeTestStore();

  // Make the database file read-only.
  MakeFileUnwritable();

  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kFailedToOpenDatabase);
}

// Tests the recovery mechanism when the database file is corrupted.
TEST_F(SqlPersistentStoreTest, InitWithCorruptDatabase) {
  InitializeTestStore();

  // Corrupt the database file by overwriting its header.
  CHECK(sql::test::CorruptSizeInHeader(GetDatabaseFilePath()));

  // Initializing again should trigger recovery, which razes and rebuilds the
  // DB.
  CreateStore();
  EXPECT_EQ(Init(), SqlPersistentStore::Error::kOk);
}

// Verifies the logic for calculating the maximum size of individual cache files
// based on the total cache size (`max_bytes`).
TEST_F(SqlPersistentStoreTest, MaxFileSizeCalculation) {
  // With a large `max_bytes`, the max file size is a fraction of the total
  // size.
  const int64_t large_max_bytes = 100 * 1024 * 1024;
  CreateStore(large_max_bytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  EXPECT_EQ(store_->MaxSize(), large_max_bytes);
  EXPECT_EQ(store_->MaxFileSize(),
            large_max_bytes / kSqlBackendMaxFileRatioDenominator);
  store_.reset();

  // With a small `max_bytes` (20 MB), the max file size is clamped at the
  // fixed value (5 MB).
  const int64_t small_max_bytes = 20 * 1024 * 1024;
  CreateStore(small_max_bytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  EXPECT_EQ(store_->MaxSize(), small_max_bytes);
  // 20 MB / 8 (kSqlBackendMaxFileRatioDenominator) = 2.5 MB, which is less than
  // the 5 MB minimum limit (kSqlBackendMinFileSizeLimit), so the result is
  // clamped to the minimum.
  EXPECT_EQ(store_->MaxFileSize(), kSqlBackendMinFileSizeLimit);
}

// Tests that GetEntryCount() and GetSizeOfAllEntries() return correct values
// based on the metadata stored in the database.
TEST_F(SqlPersistentStoreTest, GetEntryAndSize) {
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  // A new store should have zero entries and zero total size.
  EXPECT_EQ(GetEntryCount(), 0);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);
  store_.reset();
  FlushPendingTask();

  // Manually set metadata.
  const int64_t kTestEntryCount = 123;
  const int64_t kTestTotalSize = 456789;
  {
    auto db = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount,
                                     kTestEntryCount));
    ASSERT_TRUE(
        meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize, kTestTotalSize));
  }

  // Re-initializing the store should load the new metadata values.
  InitializeTestStore();
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  EXPECT_EQ(GetEntryCount(), kTestEntryCount);
  EXPECT_EQ(GetSizeOfAllEntries(), kTestTotalSize);
}

// Tests that GetEntryCount() and GetSizeOfAllEntries() handle invalid
// (e.g., negative) metadata by treating it as zero.
TEST_F(SqlPersistentStoreTest, GetEntryAndSizeWithInvalidMetadata) {
  InitializeTestStore();

  // Test with a negative entry count. The total size should still be valid.
  {
    auto db = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount, -1));
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize, 12345));
  }
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  EXPECT_EQ(GetEntryCount(), 0);
  EXPECT_EQ(GetSizeOfAllEntries(), 12345);
  store_.reset();
  FlushPendingTask();

  // Test with an entry count that exceeds the int32_t limit.
  {
    auto db = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db.get());
    ASSERT_TRUE(meta_table->SetValue(
        kSqlBackendMetaTableKeyEntryCount,
        static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1));
  }
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  EXPECT_EQ(GetEntryCount(), 0);
  store_.reset();
  FlushPendingTask();

  // Test with an entry count with the int32_t limit.
  {
    auto db = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db.get());
    ASSERT_TRUE(meta_table->SetValue(
        kSqlBackendMetaTableKeyEntryCount,
        static_cast<int64_t>(std::numeric_limits<int32_t>::max())));
  }
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  EXPECT_EQ(GetEntryCount(), std::numeric_limits<int32_t>::max());
  store_.reset();
  FlushPendingTask();

  // Test with a negative total size. The entry count should still be valid.
  {
    auto db = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize, -1));
  }
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);
}

}  // namespace disk_cache
