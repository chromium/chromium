// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "net/base/cache_type.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/sql/cache_entry_key.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
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
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->Initialize(future.GetCallback());
    return future.Get();
  }

  void CreateAndInitStore() {
    CreateStore();
    ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  }

  void ClearStore() {
    CHECK(store_);
    store_.reset();
    FlushPendingTask();
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
    base::test::TestFuture<int32_t> future;
    store_->GetEntryCount(future.GetCallback());
    return future.Get();
  }

  // Synchronously gets the total size of all entries.
  int64_t GetSizeOfAllEntries() {
    base::test::TestFuture<int64_t> future;
    store_->GetSizeOfAllEntries(future.GetCallback());
    return future.Get();
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

  // Synchronous wrapper for CreateEntry.
  SqlPersistentStore::EntryInfoOrError CreateEntry(const CacheEntryKey& key) {
    base::test::TestFuture<SqlPersistentStore::EntryInfoOrError> future;
    store_->CreateEntry(key, future.GetCallback());
    return future.Take();
  }

  // Synchronous wrapper for OpenEntry.
  SqlPersistentStore::OptionalEntryInfoOrError OpenEntry(
      const CacheEntryKey& key) {
    base::test::TestFuture<SqlPersistentStore::OptionalEntryInfoOrError> future;
    store_->OpenEntry(key, future.GetCallback());
    return future.Take();
  }

  // Synchronous wrapper for OpenOrCreateEntry.
  SqlPersistentStore::EntryInfoOrError OpenOrCreateEntry(
      const CacheEntryKey& key) {
    base::test::TestFuture<SqlPersistentStore::EntryInfoOrError> future;
    store_->OpenOrCreateEntry(key, future.GetCallback());
    return future.Take();
  }

  // Synchronous wrapper for DoomEntry.
  SqlPersistentStore::Error DoomEntry(const CacheEntryKey& key,
                                      const base::UnguessableToken& token) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->DoomEntry(key, token, future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for DeleteDoomedEntry.
  SqlPersistentStore::Error DeleteDoomedEntry(
      const CacheEntryKey& key,
      const base::UnguessableToken& token) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->DeleteDoomedEntry(key, token, future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for DeleteLiveEntry.
  SqlPersistentStore::Error DeleteLiveEntry(const CacheEntryKey& key) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->DeleteLiveEntry(key, future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for DeleteAllEntries.
  SqlPersistentStore::Error DeleteAllEntries() {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->DeleteAllEntries(future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for OpenLatestEntryBeforeResId.
  SqlPersistentStore::OptionalEntryInfoWithIdAndKey OpenLatestEntryBeforeResId(
      int64_t res_id) {
    base::test::TestFuture<SqlPersistentStore::OptionalEntryInfoWithIdAndKey>
        future;
    store_->OpenLatestEntryBeforeResId(res_id, future.GetCallback());
    return future.Take();
  }

  // Synchronous wrapper for DeleteLiveEntriesBetween.
  SqlPersistentStore::Error DeleteLiveEntriesBetween(
      base::Time initial_time,
      base::Time end_time,
      std::set<CacheEntryKey> excluded_keys = {}) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->DeleteLiveEntriesBetween(
        initial_time, end_time, std::move(excluded_keys), future.GetCallback());
    return future.Get();
  }

  // Helper to count rows in the resource table.
  int64_t CountResourcesTable() {
    auto db = ManuallyOpenDatabase();
    sql::Statement s(db->GetUniqueStatement("SELECT COUNT(*) FROM resources"));
    CHECK(s.Step());
    return s.ColumnInt(0);
  }

  // Helper to count doomed rows in the resource table.
  int64_t CountDoomedResourcesTable(const CacheEntryKey& key) {
    auto db = ManuallyOpenDatabase();
    sql::Statement s(db->GetUniqueStatement(
        "SELECT COUNT(*) FROM resources WHERE cache_key=? AND doomed=?"));
    s.BindString(0, key.string());
    s.BindBool(1, true);  // doomed = true
    CHECK(s.Step());
    return s.ColumnInt64(0);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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
  EXPECT_EQ(GetSizeOfAllEntries(),
            kTestTotalSize + kTestEntryCount * kSqlBackendStaticResourceSize);
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
  EXPECT_EQ(GetSizeOfAllEntries(), 12345);
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
  EXPECT_EQ(GetSizeOfAllEntries(),
            12345 + static_cast<int64_t>(std::numeric_limits<int32_t>::max()) *
                        kSqlBackendStaticResourceSize);
  store_.reset();
  FlushPendingTask();

  // Test with a negative total size. The entry count should still be valid.
  {
    auto db = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount, 10));
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize, -1));
  }
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  EXPECT_EQ(GetSizeOfAllEntries(), 10 * kSqlBackendStaticResourceSize);
  store_.reset();
  FlushPendingTask();

  // Test with a total size at the int64_t limit with no entries.
  {
    auto db = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount, 0));
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize,
                                     std::numeric_limits<int64_t>::max()));
  }
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  EXPECT_EQ(GetSizeOfAllEntries(), std::numeric_limits<int64_t>::max());
  store_.reset();
  FlushPendingTask();

  // Test with a total size at the int64_t limit with one entry.
  {
    auto db = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount, 1));
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize,
                                     std::numeric_limits<int64_t>::max()));
  }
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  // Adding the static size for the one entry would overflow. The implementation
  // should clamp the result at the maximum value.
  EXPECT_EQ(GetSizeOfAllEntries(), std::numeric_limits<int64_t>::max());
}

TEST_F(SqlPersistentStoreTest, CreateEntry) {
  CreateAndInitStore();
  ASSERT_EQ(GetEntryCount(), 0);
  ASSERT_EQ(GetSizeOfAllEntries(), 0);

  const CacheEntryKey kKey("my-key");
  auto result = CreateEntry(kKey);

  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->token.is_empty());
  EXPECT_FALSE(result->opened);
  EXPECT_EQ(result->body_end, 0);
  EXPECT_EQ(result->head, nullptr);

  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());

  ClearStore();

  EXPECT_EQ(CountResourcesTable(), 1);
}

TEST_F(SqlPersistentStoreTest, CreateEntryAlreadyExists) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  // Create the entry for the first time.
  auto first_result = CreateEntry(kKey);
  ASSERT_TRUE(first_result.has_value());
  ASSERT_EQ(GetEntryCount(), 1);
  ASSERT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());

  // Attempt to create it again.
  auto second_result = CreateEntry(kKey);
  ASSERT_FALSE(second_result.has_value());
  EXPECT_EQ(second_result.error(), SqlPersistentStore::Error::kAlreadyExists);

  // The counts should not have changed.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());

  ClearStore();

  EXPECT_EQ(CountResourcesTable(), 1);
}

TEST_F(SqlPersistentStoreTest, OpenEntrySuccess) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());
  const auto created_token = create_result->token;
  ASSERT_FALSE(created_token.is_empty());

  auto open_result = OpenEntry(kKey);
  ASSERT_TRUE(open_result.has_value());
  ASSERT_TRUE(open_result->has_value());
  EXPECT_EQ((*open_result)->token, created_token);
  EXPECT_TRUE((*open_result)->opened);
  EXPECT_EQ((*open_result)->body_end, 0);
  ASSERT_NE((*open_result)->head, nullptr);
  EXPECT_EQ((*open_result)->head->size(), 0);

  // Opening an entry should not change the store's stats.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());
}

TEST_F(SqlPersistentStoreTest, OpenEntryNotFound) {
  CreateAndInitStore();
  const CacheEntryKey kKey("non-existent-key");

  auto result = OpenEntry(kKey);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->has_value());
}

TEST_F(SqlPersistentStoreTest, OpenOrCreateEntryCreatesNew) {
  CreateAndInitStore();
  const CacheEntryKey kKey("new-key");

  auto result = OpenOrCreateEntry(kKey);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->token.is_empty());
  EXPECT_FALSE(result->opened);  // Should be like a created entry.

  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());
}

TEST_F(SqlPersistentStoreTest, OpenOrCreateEntryOpensExisting) {
  CreateAndInitStore();
  const CacheEntryKey kKey("existing-key");

  // Create an entry first.
  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());
  const auto created_token = create_result->token;

  // Now, open it with OpenOrCreateEntry.
  auto open_result = OpenOrCreateEntry(kKey);
  ASSERT_TRUE(open_result.has_value());
  EXPECT_EQ(open_result->token, created_token);
  EXPECT_TRUE(open_result->opened);  // Should be like an opened entry.

  // Stats should not have changed from the initial creation.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());
}

// Tests that OpenEntry fails when an entry's token is invalid in the database.
// This is simulated by manually setting the token's high and low parts to 0,
// which is the only value that UnguessableToken::Deserialize() considers to
// be an invalid, uninitialized token.
TEST_F(SqlPersistentStoreTest, OpenEntryInvalidToken) {
  CreateAndInitStore();
  const CacheEntryKey kKey("invalid-token-key");

  // Create an entry with a valid token.
  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());
  EXPECT_FALSE(create_result->token.is_empty());

  // Close the store's connection to modify the database directly.
  ClearStore();

  // Manually open the database and corrupt the `token_high` and `token_low` for
  // the entry.
  {
    auto db = ManuallyOpenDatabase();
    static constexpr char kSqlUpdateTokenLow[] =
        "UPDATE resources SET token_high=0, token_low=0 WHERE cache_key=?";
    sql::Statement statement(
        db->GetCachedStatement(SQL_FROM_HERE, kSqlUpdateTokenLow));
    statement.BindString(0, kKey.string());
    ASSERT_TRUE(statement.Run());
  }

  // Re-initialize the store, which will now try to read the corrupted data.
  CreateAndInitStore();

  // Attempt to open the entry. It should now fail with kInvalidData.
  auto open_result = OpenEntry(kKey);
  ASSERT_FALSE(open_result.has_value());
  EXPECT_EQ(open_result.error(), SqlPersistentStore::Error::kInvalidData);

  // Attempt to open the entry with OpenOrCreateEntry(). It should fail with
  // kInvalidData.
  auto open_or_create_result = OpenOrCreateEntry(kKey);
  ASSERT_FALSE(open_or_create_result.has_value());
  EXPECT_EQ(open_or_create_result.error(),
            SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, DoomEntrySuccess) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToDoom("key-to-doom");
  const CacheEntryKey kKeyToKeep("key-to-keep");
  const int64_t size_to_doom =
      kSqlBackendStaticResourceSize + kKeyToDoom.string().size();
  const int64_t size_to_keep =
      kSqlBackendStaticResourceSize + kKeyToKeep.string().size();

  // Create two entries.
  auto create_result_to_doom = CreateEntry(kKeyToDoom);
  ASSERT_TRUE(create_result_to_doom.has_value());
  auto create_result_to_keep = CreateEntry(kKeyToKeep);
  ASSERT_TRUE(create_result_to_keep.has_value());

  const auto token_to_doom = create_result_to_doom->token;
  ASSERT_EQ(GetEntryCount(), 2);
  ASSERT_EQ(GetSizeOfAllEntries(), size_to_doom + size_to_keep);

  // Doom one of the entries.
  ASSERT_EQ(DoomEntry(kKeyToDoom, token_to_doom),
            SqlPersistentStore::Error::kOk);

  // Verify that the entry count and size are updated, reflecting that one entry
  // was logically removed.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), size_to_keep);

  // Verify the doomed entry can no longer be opened.
  auto open_doomed_result = OpenEntry(kKeyToDoom);
  ASSERT_TRUE(open_doomed_result.has_value());
  EXPECT_FALSE(open_doomed_result->has_value());

  // Verify the other entry can still be opened.
  auto open_kept_result = OpenEntry(kKeyToKeep);
  ASSERT_TRUE(open_kept_result.has_value());
  ASSERT_TRUE(open_kept_result->has_value());
  EXPECT_EQ((*open_kept_result)->token, create_result_to_keep->token);

  // Verify the doomed entry still exists in the table but is marked as doomed,
  // and the other entry is unaffected.
  ClearStore();
  EXPECT_EQ(CountResourcesTable(), 2);
  EXPECT_EQ(CountDoomedResourcesTable(kKeyToDoom), 1);
  EXPECT_EQ(CountDoomedResourcesTable(kKeyToKeep), 0);
}

TEST_F(SqlPersistentStoreTest, DoomEntryFailsNotFound) {
  CreateAndInitStore();
  const CacheEntryKey kKey("non-existent-key");
  ASSERT_EQ(GetEntryCount(), 0);

  // Attempt to doom an entry that doesn't exist.
  auto result = DoomEntry(kKey, base::UnguessableToken::Create());
  ASSERT_EQ(result, SqlPersistentStore::Error::kNotFound);

  // Verify that the counts remain unchanged.
  EXPECT_EQ(GetEntryCount(), 0);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);
}

TEST_F(SqlPersistentStoreTest, DoomEntryFailsWrongToken) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");
  const int64_t size1 = kSqlBackendStaticResourceSize + kKey1.string().size();
  const int64_t size2 = kSqlBackendStaticResourceSize + kKey2.string().size();

  // Create two entries.
  auto create_result1 = CreateEntry(kKey1);
  ASSERT_TRUE(create_result1.has_value());
  auto create_result2 = CreateEntry(kKey2);
  ASSERT_TRUE(create_result2.has_value());
  ASSERT_EQ(GetEntryCount(), 2);

  // Attempt to doom key1 with an incorrect token.
  auto result = DoomEntry(kKey1, base::UnguessableToken::Create());
  ASSERT_EQ(result, SqlPersistentStore::Error::kNotFound);

  // Verify that the counts remain unchanged and both entries can still be
  // opened.
  EXPECT_EQ(GetEntryCount(), 2);
  EXPECT_EQ(GetSizeOfAllEntries(), size1 + size2);

  auto open_result1 = OpenEntry(kKey1);
  ASSERT_TRUE(open_result1.has_value());
  ASSERT_TRUE(open_result1->has_value());
  EXPECT_EQ((*open_result1)->token, create_result1->token);

  auto open_result2 = OpenEntry(kKey2);
  ASSERT_TRUE(open_result2.has_value());
  ASSERT_TRUE(open_result2->has_value());
  EXPECT_EQ((*open_result2)->token, create_result2->token);
}

TEST_F(SqlPersistentStoreTest, DoomEntryWithCorruptSizeRecovers) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToCorrupt("key-to-corrupt");
  const CacheEntryKey kKeyToKeep("key-to-keep");
  const int64_t keep_key_size = kKeyToKeep.string().size();
  const int64_t expected_size_after_recovery =
      kSqlBackendStaticResourceSize + keep_key_size;

  // Create one entry to keep, and one to corrupt and doom.
  auto create_corrupt_result = CreateEntry(kKeyToCorrupt);
  ASSERT_TRUE(create_corrupt_result.has_value());
  ASSERT_TRUE(CreateEntry(kKeyToKeep).has_value());
  ASSERT_EQ(GetEntryCount(), 2);
  const auto token_to_doom = create_corrupt_result->token;
  ClearStore();

  // Manually open the database and corrupt the `bytes_usage` for one entry
  // to an extreme value that will cause an overflow during calculation.
  {
    auto db = ManuallyOpenDatabase();
    sql::Statement statement(db->GetUniqueStatement(
        "UPDATE resources SET bytes_usage = ? WHERE cache_key = ?"));
    statement.BindInt64(0, std::numeric_limits<int64_t>::min());
    statement.BindString(1, kKeyToCorrupt.string());
    ASSERT_TRUE(statement.Run());
  }

  // Re-initialize the store with the corrupted database.
  CreateAndInitStore();

  // Doom the entry with the corrupted size. This will trigger an overflow in
  // `total_size_delta`, causing `!total_size_delta.IsValid()` to be true.
  // The store should recover by recalculating its state from the database.
  ASSERT_EQ(DoomEntry(kKeyToCorrupt, token_to_doom),
            SqlPersistentStore::Error::kOk);

  // Verify that recovery was successful. The entry count should be 1 (for the
  // entry we kept), and the total size should be correctly calculated for
  // that single remaining entry, ignoring the corrupted value.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), expected_size_after_recovery);

  // Verify the state on disk.
  ClearStore();
  // Both entries should still exist in the table.
  EXPECT_EQ(CountResourcesTable(), 2);
  // The corrupted entry should be marked as doomed.
  EXPECT_EQ(CountDoomedResourcesTable(kKeyToCorrupt), 1);
  // The other entry should be unaffected.
  EXPECT_EQ(CountDoomedResourcesTable(kKeyToKeep), 0);
}

TEST_F(SqlPersistentStoreTest, DeleteDoomedEntrySuccess) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  // Create and doom an entry.
  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());
  const auto token = create_result->token;
  ASSERT_EQ(DoomEntry(kKey, token), SqlPersistentStore::Error::kOk);
  ASSERT_EQ(GetEntryCount(), 0);
  ClearStore();
  ASSERT_EQ(CountResourcesTable(), 1);
  CreateAndInitStore();

  // Delete the doomed entry.
  ASSERT_EQ(DeleteDoomedEntry(kKey, token), SqlPersistentStore::Error::kOk);

  // Verify the entry is now physically gone from the database.
  ClearStore();
  EXPECT_EQ(CountResourcesTable(), 0);
}

TEST_F(SqlPersistentStoreTest, DeleteDoomedEntryFailsOnLiveEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  // Create a live entry.
  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());
  const auto token = create_result->token;
  ASSERT_EQ(GetEntryCount(), 1);

  // Attempt to delete it with DeleteDoomedEntry. This should fail because the
  // entry is not marked as doomed.
  auto result = DeleteDoomedEntry(kKey, token);
  ASSERT_EQ(result, SqlPersistentStore::Error::kNotFound);

  // Verify the entry still exists.
  EXPECT_EQ(GetEntryCount(), 1);
  ClearStore();
  EXPECT_EQ(CountResourcesTable(), 1);
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntrySuccess) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToDelete("key-to-delete");
  const CacheEntryKey kKeyToKeep("key-to-keep");
  const int64_t size_to_delete =
      kSqlBackendStaticResourceSize + kKeyToDelete.string().size();
  const int64_t size_to_keep =
      kSqlBackendStaticResourceSize + kKeyToKeep.string().size();

  // Create two entries.
  ASSERT_TRUE(CreateEntry(kKeyToDelete).has_value());
  auto create_result_to_keep = CreateEntry(kKeyToKeep);
  ASSERT_TRUE(create_result_to_keep.has_value());
  ASSERT_EQ(GetEntryCount(), 2);
  ASSERT_EQ(GetSizeOfAllEntries(), size_to_delete + size_to_keep);

  // Delete one of the live entries.
  ASSERT_EQ(DeleteLiveEntry(kKeyToDelete), SqlPersistentStore::Error::kOk);

  // Verify the cache is updated correctly.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), size_to_keep);

  // Verify the deleted entry cannot be opened.
  auto open_deleted_result = OpenEntry(kKeyToDelete);
  ASSERT_TRUE(open_deleted_result.has_value());
  EXPECT_FALSE(open_deleted_result->has_value());

  // Verify the other entry can still be opened.
  auto open_kept_result = OpenEntry(kKeyToKeep);
  ASSERT_TRUE(open_kept_result.has_value());
  ASSERT_TRUE(open_kept_result->has_value());
  EXPECT_EQ((*open_kept_result)->token, create_result_to_keep->token);

  // Verify the entry is physically gone from the database.
  ClearStore();
  EXPECT_EQ(CountResourcesTable(), 1);
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntryFailsNotFound) {
  CreateAndInitStore();
  const CacheEntryKey kKey("non-existent-key");
  ASSERT_EQ(GetEntryCount(), 0);

  // Attempt to delete an entry that doesn't exist.
  auto result = DeleteLiveEntry(kKey);
  ASSERT_EQ(result, SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntryFailsOnDoomedEntry) {
  CreateAndInitStore();
  const CacheEntryKey kDoomedKey("doomed-key");
  const CacheEntryKey kLiveKey("live-key");
  const int64_t live_key_size =
      kSqlBackendStaticResourceSize + kLiveKey.string().size();

  // Create one live entry and one entry that will be doomed.
  auto create_doomed_result = CreateEntry(kDoomedKey);
  ASSERT_TRUE(create_doomed_result.has_value());
  ASSERT_TRUE(CreateEntry(kLiveKey).has_value());

  // Doom one of the entries.
  ASSERT_EQ(DoomEntry(kDoomedKey, create_doomed_result->token),
            SqlPersistentStore::Error::kOk);
  // After dooming, one entry is live, one is doomed (logically removed).
  ASSERT_EQ(GetEntryCount(), 1);
  ASSERT_EQ(GetSizeOfAllEntries(), live_key_size);

  // Attempt to delete the doomed entry with DeleteLiveEntry. This should fail
  // because it's not "live".
  auto result = DeleteLiveEntry(kDoomedKey);
  ASSERT_EQ(result, SqlPersistentStore::Error::kNotFound);

  // Verify that the live entry was not affected.
  EXPECT_EQ(GetEntryCount(), 1);
  auto open_live_result = OpenEntry(kLiveKey);
  ASSERT_TRUE(open_live_result.has_value());
  ASSERT_TRUE(open_live_result->has_value());

  // Verify the doomed entry still exists in the table (as doomed), and the
  // live entry is also present.
  ClearStore();
  EXPECT_EQ(CountResourcesTable(), 2);
  EXPECT_EQ(CountDoomedResourcesTable(kDoomedKey), 1);
  EXPECT_EQ(CountDoomedResourcesTable(kLiveKey), 0);
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntryWithCorruptTokenRecovers) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToCorrupt("key-to-corrupt-token");
  const CacheEntryKey kKeyToKeep("key-to-keep");
  const int64_t keep_key_size = kKeyToKeep.string().size();
  const int64_t expected_size_after_recovery =
      kSqlBackendStaticResourceSize + keep_key_size;

  // Create one entry to keep, and one to corrupt and delete.
  ASSERT_TRUE(CreateEntry(kKeyToCorrupt).has_value());
  ASSERT_TRUE(CreateEntry(kKeyToKeep).has_value());
  ASSERT_EQ(GetEntryCount(), 2);
  ClearStore();

  // Manually open the database and corrupt the token for one entry so that
  // it becomes invalid.
  {
    auto db = ManuallyOpenDatabase();
    sql::Statement statement(db->GetUniqueStatement(
        "UPDATE resources SET token_high = 0, token_low = 0 WHERE cache_key = "
        "?"));
    statement.BindString(0, kKeyToCorrupt.string());
    ASSERT_TRUE(statement.Run());
  }

  // Re-initialize the store with the corrupted database.
  CreateAndInitStore();

  // Delete the entry with the corrupted token. This will trigger the
  // `corruption_detected` path, forcing a full recalculation.
  ASSERT_EQ(DeleteLiveEntry(kKeyToCorrupt), SqlPersistentStore::Error::kOk);

  // Verify that recovery was successful. The entry count and total size
  // should now reflect only the entry that was kept.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), expected_size_after_recovery);

  // Verify the state on disk. Only the un-corrupted entry should remain.
  ClearStore();
  EXPECT_EQ(CountResourcesTable(), 1);
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntryWithCorruptSizeRecovers) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToCorrupt("key-to-corrupt-size");
  const CacheEntryKey kKeyToKeep("key-to-keep");
  const int64_t keep_key_size = kKeyToKeep.string().size();
  const int64_t expected_size_after_recovery =
      kSqlBackendStaticResourceSize + keep_key_size;

  // Create one entry to keep, and one to corrupt and delete.
  ASSERT_TRUE(CreateEntry(kKeyToCorrupt).has_value());
  ASSERT_TRUE(CreateEntry(kKeyToKeep).has_value());
  ASSERT_EQ(GetEntryCount(), 2);
  ClearStore();

  // Manually open the database and corrupt the `bytes_usage` for one entry
  // to an extreme value that will cause an underflow during calculation.
  {
    auto db = ManuallyOpenDatabase();
    sql::Statement statement(db->GetUniqueStatement(
        "UPDATE resources SET bytes_usage = ? WHERE cache_key = ?"));
    statement.BindInt64(0, std::numeric_limits<int64_t>::max());
    statement.BindString(1, kKeyToCorrupt.string());
    ASSERT_TRUE(statement.Run());
  }

  // Re-initialize the store with the corrupted database.
  CreateAndInitStore();

  // Delete the entry with the corrupted size. This will trigger an underflow
  // in `total_size_delta`, causing `!total_size_delta.IsValid()` to be true.
  // The store should recover by recalculating its state from the database.
  ASSERT_EQ(DeleteLiveEntry(kKeyToCorrupt), SqlPersistentStore::Error::kOk);

  // Verify that recovery was successful. The entry count and total size
  // should now reflect only the entry that was kept.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), expected_size_after_recovery);

  // Verify the state on disk. Only the un-corrupted entry should remain.
  ClearStore();
  EXPECT_EQ(CountResourcesTable(), 1);
}

TEST_F(SqlPersistentStoreTest, DeleteAllEntriesNonEmpty) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");
  const int64_t expected_size =
      (kSqlBackendStaticResourceSize + kKey1.string().size()) +
      (kSqlBackendStaticResourceSize + kKey2.string().size());

  // Create two entries.
  ASSERT_TRUE(CreateEntry(kKey1).has_value());
  ASSERT_TRUE(CreateEntry(kKey2).has_value());
  ASSERT_EQ(GetEntryCount(), 2);
  ASSERT_EQ(GetSizeOfAllEntries(), expected_size);

  ClearStore();
  ASSERT_EQ(CountResourcesTable(), 2);
  CreateAndInitStore();

  // Delete all entries.
  ASSERT_EQ(DeleteAllEntries(), SqlPersistentStore::Error::kOk);

  // Verify the cache is empty.
  EXPECT_EQ(GetEntryCount(), 0);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);

  ClearStore();
  EXPECT_EQ(CountResourcesTable(), 0);
  CreateAndInitStore();

  // Verify the old entries cannot be opened.
  auto open_result = OpenEntry(kKey1);
  ASSERT_TRUE(open_result.has_value());
  EXPECT_FALSE(open_result->has_value());
}

TEST_F(SqlPersistentStoreTest, DeleteAllEntriesEmpty) {
  CreateAndInitStore();
  ASSERT_EQ(GetEntryCount(), 0);
  ASSERT_EQ(GetSizeOfAllEntries(), 0);

  // Delete all entries from an already empty cache.
  ASSERT_EQ(DeleteAllEntries(), SqlPersistentStore::Error::kOk);

  // Verify the cache is still empty.
  EXPECT_EQ(GetEntryCount(), 0);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);
}

TEST_F(SqlPersistentStoreTest, ChangeEntryCountOverflowRecovers) {
  // Create and initialize a store to have a valid DB file.
  CreateAndInitStore();
  ClearStore();

  // Manually set the entry count to INT32_MAX.
  {
    auto db = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount,
                                     std::numeric_limits<int32_t>::max()));
  }

  // Re-open the store. It should load the manipulated count.
  CreateAndInitStore();
  ASSERT_EQ(GetEntryCount(), std::numeric_limits<int32_t>::max());

  // Create a new entry. This will attempt to increment the counter, causing
  // an overflow. The store should recover by recalculating the count from
  // the `resources` table (which will be 1).
  const CacheEntryKey kKey("my-key");
  auto result = CreateEntry(kKey);
  ASSERT_TRUE(result.has_value());

  // The new count should be 1 (the one entry we just created), not an
  // overflowed value. The size should also be correct for one entry.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());

  // Verify by closing and re-opening that the correct value was persisted.
  ClearStore();
  CreateAndInitStore();
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());
}

TEST_F(SqlPersistentStoreTest, ChangeTotalSizeOverflowRecovers) {
  // Create and initialize a store.
  CreateAndInitStore();
  ClearStore();

  // Manually set the total size to INT64_MAX.
  {
    auto db = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize,
                                     std::numeric_limits<int64_t>::max()));
  }

  // Re-open the store and confirm it loaded the manipulated size.
  CreateAndInitStore();
  ASSERT_EQ(GetSizeOfAllEntries(), std::numeric_limits<int64_t>::max());
  ASSERT_EQ(GetEntryCount(), 0);

  // Create a new entry. This will attempt to increment the total size,
  // causing an overflow. The store should recover by recalculating.
  const CacheEntryKey kKey("my-key");
  auto result = CreateEntry(kKey);
  ASSERT_TRUE(result.has_value());

  // The new total size should be just the size of the new entry.
  // The entry count should have been incremented from its initial state (0).
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());

  // Verify that the correct values were persisted to the database.
  ClearStore();
  CreateAndInitStore();
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());
}

// This test validates that the `kSqlBackendStaticResourceSize` constant
// provides a reasonable estimate for the per-entry overhead in the database. It
// creates a number of entries and compares the calculated size from the store
// with the actual size of the database file on disk.
TEST_F(SqlPersistentStoreTest, StaticResourceSizeEstimation) {
  CreateAndInitStore();

  const int kNumEntries = 1000;
  const int kKeySize = 100;
  int64_t total_key_size = 0;

  for (int i = 0; i < kNumEntries; ++i) {
    // Create a key of a fixed size.
    std::string key_str = base::StringPrintf("key-%04d", i);
    key_str.resize(kKeySize, ' ');
    const CacheEntryKey key(key_str);

    ASSERT_TRUE(CreateEntry(key).has_value());
    total_key_size += key.string().size();
  }

  ASSERT_EQ(GetEntryCount(), kNumEntries);

  // The size calculated by the store.
  const int64_t calculated_size = GetSizeOfAllEntries();
  EXPECT_EQ(calculated_size,
            total_key_size + kNumEntries * kSqlBackendStaticResourceSize);

  // Close the store to ensure all data is flushed to the main database file,
  // making the file size measurement more stable and predictable.
  ClearStore();

  std::optional<int64_t> db_file_size =
      base::GetFileSize(GetDatabaseFilePath());
  ASSERT_TRUE(db_file_size);

  // Calculate the actual overhead per entry based on the final file size.
  // This includes all SQLite overhead (page headers, b-tree structures, etc.)
  // for the data stored in the `resources` table, minus the raw key data.
  const int64_t actual_overhead = *db_file_size - total_key_size;
  ASSERT_GT(actual_overhead, 0);
  const int64_t actual_overhead_per_entry = actual_overhead / kNumEntries;

  LOG(INFO) << "kSqlBackendStaticResourceSize (estimate): "
            << kSqlBackendStaticResourceSize;
  LOG(INFO) << "Actual overhead per entry (from file size): "
            << actual_overhead_per_entry;

  // This is a loose validation. We check that our estimate is in the correct
  // order of magnitude. The actual overhead can vary based on SQLite version,
  // page size, and other factors.
  // We expect the actual overhead to be positive.
  EXPECT_GT(actual_overhead_per_entry, 0);

  // A loose upper bound to catch if the overhead becomes excessively larger
  // than our estimate. A factor of 4 should be sufficient.
  EXPECT_LT(actual_overhead_per_entry, kSqlBackendStaticResourceSize * 4)
      << "Actual overhead is much larger than estimated. The constant might "
         "need updating.";

  // A loose lower bound. It's unlikely to be smaller than this.
  EXPECT_GT(actual_overhead_per_entry, kSqlBackendStaticResourceSize / 8)
      << "Actual overhead is much smaller than estimated. The constant might "
         "be too conservative.";
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntriesBetween) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2-excluded");
  const CacheEntryKey kKey3("key3");
  const CacheEntryKey kKey4("key4-before");
  const CacheEntryKey kKey5("key5-after");

  const base::Time kBaseTime = base::Time::Now();

  // Create entries with different last_used times.
  task_environment_.AdvanceClock(base::Minutes(1));
  ASSERT_TRUE(CreateEntry(kKey1).has_value());
  const base::Time kTime1 = base::Time::Now();

  task_environment_.AdvanceClock(base::Minutes(1));
  ASSERT_TRUE(CreateEntry(kKey2).has_value());

  task_environment_.AdvanceClock(base::Minutes(1));
  ASSERT_TRUE(CreateEntry(kKey3).has_value());
  const base::Time kTime3 = base::Time::Now();

  // Create kKey4 and then manually set its last_used time to kBaseTime,
  // which is before kTime1.
  ASSERT_TRUE(CreateEntry(kKey4).has_value());
  ClearStore();
  {
    auto db = ManuallyOpenDatabase();
    sql::Statement statement(db->GetUniqueStatement(
        "UPDATE resources SET last_used = ? WHERE cache_key = ?"));
    statement.BindTime(0, kBaseTime);
    statement.BindString(1, kKey4.string());
    ASSERT_TRUE(statement.Run());
  }
  CreateAndInitStore();
  // kKey4's last_used time in DB is now kBaseTime. kBaseTime < kTime1 is true.

  // Create kKey5, ensuring its time is after kTime3.
  // At this point, Time::Now() is effectively kTime3.
  task_environment_.AdvanceClock(base::Minutes(1));
  ASSERT_TRUE(CreateEntry(kKey5).has_value());
  const base::Time kTime5 = base::Time::Now();
  ASSERT_GT(kTime5, kTime3);

  ASSERT_EQ(GetEntryCount(), 5);
  int64_t initial_total_size = GetSizeOfAllEntries();

  // Delete entries between kTime1 (inclusive) and kTime3 (exclusive).
  // kKey2 should be excluded.
  // Expected to delete: kKey1.
  // Expected to keep: kKey2, kKey3, kKey4, kKey5.
  std::set<CacheEntryKey> excluded_keys = {kKey2};
  ASSERT_EQ(DeleteLiveEntriesBetween(kTime1, kTime3, excluded_keys),
            SqlPersistentStore::Error::kOk);

  EXPECT_EQ(GetEntryCount(), 4);
  const int64_t expected_size_after_delete =
      initial_total_size -
      (kSqlBackendStaticResourceSize + kKey1.string().size());
  EXPECT_EQ(GetSizeOfAllEntries(), expected_size_after_delete);

  // Verify kKey1 is deleted.
  auto open_key1 = OpenEntry(kKey1);
  ASSERT_TRUE(open_key1.has_value());
  EXPECT_FALSE(open_key1->has_value());

  // Verify other keys are still present.
  EXPECT_TRUE(OpenEntry(kKey2).value().has_value());
  EXPECT_TRUE(OpenEntry(kKey3).value().has_value());
  EXPECT_TRUE(OpenEntry(kKey4).value().has_value());
  EXPECT_TRUE(OpenEntry(kKey5).value().has_value());

  ClearStore();
  EXPECT_EQ(CountResourcesTable(), 4);
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntriesBetweenEmptyCache) {
  CreateAndInitStore();
  ASSERT_EQ(GetEntryCount(), 0);
  ASSERT_EQ(GetSizeOfAllEntries(), 0);

  ASSERT_EQ(DeleteLiveEntriesBetween(base::Time(), base::Time::Max()),
            SqlPersistentStore::Error::kOk);

  EXPECT_EQ(GetEntryCount(), 0);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntriesBetweenNoMatchingEntries) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");

  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time kTime1 = base::Time::Now();
  ASSERT_TRUE(CreateEntry(kKey1).has_value());

  ASSERT_EQ(GetEntryCount(), 1);
  int64_t initial_total_size = GetSizeOfAllEntries();

  // Delete entries in a range that doesn't include kKey1.
  ASSERT_EQ(DeleteLiveEntriesBetween(kTime1 + base::Minutes(1),
                                     kTime1 + base::Minutes(2)),
            SqlPersistentStore::Error::kOk);

  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), initial_total_size);
  EXPECT_TRUE(OpenEntry(kKey1).value().has_value());
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntriesBetweenWithCorruptSize) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToCorrupt("key-to-corrupt-size");
  const CacheEntryKey kKeyToKeep("key-to-keep");

  // Create an entry that will be corrupted and fall within the deletion range.
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time kTimeCorrupt = base::Time::Now();
  ASSERT_TRUE(CreateEntry(kKeyToCorrupt).has_value());

  // Create an entry that will be kept (outside the deletion range).
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time kTimeKeep = base::Time::Now();
  ASSERT_TRUE(CreateEntry(kKeyToKeep).has_value());

  ASSERT_EQ(GetEntryCount(), 2);

  ClearStore();
  {
    auto db = ManuallyOpenDatabase();
    // Set bytes_usage for kKeyToCorrupt to cause overflow when subtracted
    // during deletion.
    sql::Statement update_corrupt_stmt(db->GetUniqueStatement(
        "UPDATE resources SET bytes_usage=? WHERE cache_key=?"));
    update_corrupt_stmt.BindInt64(0, std::numeric_limits<int64_t>::min());
    update_corrupt_stmt.BindString(1, kKeyToCorrupt.string());
    ASSERT_TRUE(update_corrupt_stmt.Run());
  }
  CreateAndInitStore();  // Re-initialize with modified DB

  base::HistogramTester histogram_tester;

  // Delete entries in a range that includes kKeyToCorrupt [kTimeCorrupt,
  // kTimeKeep). kKeyToKeep's last_used time is kTimeKeep, so it's not <
  // kTimeKeep.
  ASSERT_EQ(DeleteLiveEntriesBetween(kTimeCorrupt, kTimeKeep),
            SqlPersistentStore::Error::kOk);

  // Verify that kInvalidData was recorded due to the corrupted bytes_usage.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.Backend.DeleteLiveEntriesBetween.Result",
      SqlPersistentStore::Error::kInvalidData, 1);

  // kKeyToCorrupt should be deleted.
  // kKeyToKeep should remain.
  // The store should have recovered from the size overflow.
  EXPECT_EQ(GetEntryCount(), 1);
  const int64_t expected_size_after_delete =
      kSqlBackendStaticResourceSize + kKeyToKeep.string().size();
  EXPECT_EQ(GetSizeOfAllEntries(), expected_size_after_delete);

  EXPECT_FALSE(OpenEntry(kKeyToCorrupt).value().has_value());
  EXPECT_TRUE(OpenEntry(kKeyToKeep).value().has_value());
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntriesBetweenWithCorruptToken) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToCorrupt("key-to-corrupt");
  const CacheEntryKey kKeyToKeep("key-to-keep");

  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time kTimeCorrupt = base::Time::Now();
  ASSERT_TRUE(CreateEntry(kKeyToCorrupt).has_value());

  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time kTimeKeep = base::Time::Now();
  ASSERT_TRUE(CreateEntry(kKeyToKeep).has_value());

  ASSERT_EQ(GetEntryCount(), 2);

  ClearStore();
  {
    // Manually corrupt the token of kKeyToCorrupt in the database.
    // This simulates a scenario where the token data is invalid.
    auto db = ManuallyOpenDatabase();
    sql::Statement statement(
        db->GetUniqueStatement("UPDATE resources SET token_high=0, "
                               "token_low=0 WHERE cache_key=?"));
    statement.BindString(0, kKeyToCorrupt.string());
    ASSERT_TRUE(statement.Run());
  }
  CreateAndInitStore();

  base::HistogramTester histogram_tester;
  ASSERT_EQ(DeleteLiveEntriesBetween(kTimeCorrupt, kTimeKeep),
            SqlPersistentStore::Error::kOk);
  // Verify that kInvalidData was recorded due to the corrupted token.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.Backend.DeleteLiveEntriesBetween.Result",
      SqlPersistentStore::Error::kInvalidData, 1);

  EXPECT_EQ(GetEntryCount(), 1);  // kKeyToKeep should remain
  const int64_t expected_size_after_delete =
      kSqlBackendStaticResourceSize + kKeyToKeep.string().size();
  EXPECT_EQ(GetSizeOfAllEntries(), expected_size_after_delete);

  EXPECT_FALSE(OpenEntry(kKeyToCorrupt).value().has_value());
  EXPECT_TRUE(OpenEntry(kKeyToKeep).value().has_value());
}

TEST_F(SqlPersistentStoreTest, OpenLatestEntryBeforeResIdEmptyCache) {
  CreateAndInitStore();
  auto result = OpenLatestEntryBeforeResId(std::numeric_limits<int64_t>::max());
  EXPECT_FALSE(result.has_value());
}

TEST_F(SqlPersistentStoreTest, OpenLatestEntryBeforeResIdSingleEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());

  // Open the first (and only) entry.
  auto next_result1 =
      OpenLatestEntryBeforeResId(std::numeric_limits<int64_t>::max());
  ASSERT_TRUE(next_result1.has_value());
  EXPECT_EQ(next_result1->key, kKey);
  EXPECT_EQ(next_result1->info.token, create_result->token);
  EXPECT_TRUE(next_result1->info.opened);
  EXPECT_EQ(next_result1->info.body_end, 0);
  ASSERT_NE(next_result1->info.head, nullptr);
  EXPECT_EQ(next_result1->info.head->size(), 0);

  // Try to open again, should be no more entries.
  auto next_result2 = OpenLatestEntryBeforeResId(next_result1->res_id);
  EXPECT_FALSE(next_result2.has_value());
}

TEST_F(SqlPersistentStoreTest, OpenLatestEntryBeforeResIdMultipleEntries) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");
  const CacheEntryKey kKey3("key3");

  auto create_result1 = CreateEntry(kKey1);
  ASSERT_TRUE(create_result1.has_value());
  auto create_result2 = CreateEntry(kKey2);
  ASSERT_TRUE(create_result2.has_value());
  auto create_result3 = CreateEntry(kKey3);
  ASSERT_TRUE(create_result3.has_value());

  // Entries should be returned in reverse order of creation (descending
  // res_id).
  auto next_result =
      OpenLatestEntryBeforeResId(std::numeric_limits<int64_t>::max());
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey3);
  EXPECT_EQ(next_result->info.token, create_result3->token);
  const int64_t res_id3 = next_result->res_id;

  next_result = OpenLatestEntryBeforeResId(res_id3);
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey2);
  EXPECT_EQ(next_result->info.token, create_result2->token);
  const int64_t res_id2 = next_result->res_id;

  next_result = OpenLatestEntryBeforeResId(res_id2);
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey1);
  EXPECT_EQ(next_result->info.token, create_result1->token);
  const int64_t res_id1 = next_result->res_id;

  next_result = OpenLatestEntryBeforeResId(res_id1);
  EXPECT_FALSE(next_result.has_value());
}

TEST_F(SqlPersistentStoreTest, OpenLatestEntryBeforeResIdSkipsDoomed) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKeyToDoom("key-to-doom");
  const CacheEntryKey kKey3("key3");

  auto create_result1 = CreateEntry(kKey1);
  ASSERT_TRUE(create_result1.has_value());
  auto create_result_doomed = CreateEntry(kKeyToDoom);
  ASSERT_TRUE(create_result_doomed.has_value());
  auto create_result3 = CreateEntry(kKey3);
  ASSERT_TRUE(create_result3.has_value());

  // Doom the middle entry.
  ASSERT_EQ(DoomEntry(kKeyToDoom, create_result_doomed->token),
            SqlPersistentStore::Error::kOk);

  // OpenLatestEntryBeforeResId should skip the doomed entry.
  auto next_result =
      OpenLatestEntryBeforeResId(std::numeric_limits<int64_t>::max());
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey3);  // Should be kKey3
  const int64_t res_id3 = next_result->res_id;

  next_result = OpenLatestEntryBeforeResId(res_id3);
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey1);  // Should skip kKeyToDoom and get kKey1
  const int64_t res_id1 = next_result->res_id;

  next_result = OpenLatestEntryBeforeResId(res_id1);
  EXPECT_FALSE(next_result.has_value());
}

TEST_F(SqlPersistentStoreTest, OpenLatestEntryBeforeResIdSkipsInvalidToken) {
  CreateAndInitStore();
  const CacheEntryKey kKeyValidBefore("valid-before");
  const CacheEntryKey kKeyInvalid("invalid-token-key");
  const CacheEntryKey kKeyValidAfter("valid-after");

  ASSERT_TRUE(CreateEntry(kKeyValidBefore).has_value());
  auto create_invalid_result = CreateEntry(kKeyInvalid);
  ASSERT_TRUE(create_invalid_result.has_value());
  auto create_valid_after_result = CreateEntry(kKeyValidAfter);
  ASSERT_TRUE(create_valid_after_result.has_value());

  ClearStore();  // Close the store to modify DB directly.

  // Manually corrupt the token for kKeyInvalid.
  {
    auto db = ManuallyOpenDatabase();
    sql::Statement statement(db->GetUniqueStatement(
        "UPDATE resources SET token_high=0, token_low=0 WHERE cache_key=?"));
    statement.BindString(0, kKeyInvalid.string());
    ASSERT_TRUE(statement.Run());
  }

  CreateAndInitStore();  // Re-open the store.

  // kKeyValidAfter should be returned first.
  auto next_result =
      OpenLatestEntryBeforeResId(std::numeric_limits<int64_t>::max());
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKeyValidAfter);

  base::HistogramTester histogram_tester;
  // kKeyInvalid should be skipped, kKeyValidBefore should be next.
  next_result = OpenLatestEntryBeforeResId(next_result->res_id);
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKeyValidBefore);
  // Verify that kInvalidData was recorded in the histogram when skipping.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.Backend.OpenLatestEntryBeforeResId.Result",
      SqlPersistentStore::Error::kInvalidData, 1);

  // No more valid entries.
  next_result = OpenLatestEntryBeforeResId(next_result->res_id);
  EXPECT_FALSE(next_result.has_value());
}

TEST_F(SqlPersistentStoreTest, InitializeCallbackNotRunOnStoreDestruction) {
  CreateStore();
  bool callback_run = false;
  store_->Initialize(base::BindLambdaForTesting(
      [&](SqlPersistentStore::Error result) { callback_run = true; }));

  // Destroy the store, which invalidates the WeakPtr.
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest, CreateEntryCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  bool callback_run = false;

  store_->CreateEntry(kKey, base::BindLambdaForTesting(
                                [&](SqlPersistentStore::EntryInfoOrError) {
                                  callback_run = true;
                                }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest, OpenEntryCallbackNotRunOnStoreDestruction) {
  const CacheEntryKey kKey("my-key");
  CreateAndInitStore();
  ASSERT_TRUE(CreateEntry(kKey).has_value());
  ClearStore();
  CreateAndInitStore();

  bool callback_run = false;
  store_->OpenEntry(kKey,
                    base::BindLambdaForTesting(
                        [&](SqlPersistentStore::OptionalEntryInfoOrError) {
                          callback_run = true;
                        }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest,
       OpenOrCreateEntryCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  bool callback_run = false;

  store_->OpenOrCreateEntry(
      kKey,
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::EntryInfoOrError) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest, DoomEntryCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());

  bool callback_run = false;
  store_->DoomEntry(kKey, create_result->token,
                    base::BindLambdaForTesting([&](SqlPersistentStore::Error) {
                      callback_run = true;
                    }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest,
       DeleteDoomedEntryCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());
  ASSERT_EQ(DoomEntry(kKey, create_result->token),
            SqlPersistentStore::Error::kOk);

  bool callback_run = false;
  store_->DeleteDoomedEntry(
      kKey, create_result->token,
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::Error) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest,
       DeleteLiveEntryCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  ASSERT_TRUE(CreateEntry(kKey).has_value());

  bool callback_run = false;
  store_->DeleteLiveEntry(
      kKey, base::BindLambdaForTesting(
                [&](SqlPersistentStore::Error) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest,
       DeleteAllEntriesCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  bool callback_run = false;

  store_->DeleteAllEntries(base::BindLambdaForTesting(
      [&](SqlPersistentStore::Error) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest,
       OpenLatestEntryBeforeResIdCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  bool callback_run = false;

  store_->OpenLatestEntryBeforeResId(
      std::numeric_limits<int64_t>::max(),
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::OptionalEntryInfoWithIdAndKey) {
            callback_run = true;
          }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest,
       DeleteLiveEntriesBetweenCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  bool callback_run = false;

  store_->DeleteLiveEntriesBetween(
      base::Time(), base::Time::Max(), {},
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::Error) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

}  // namespace disk_cache
