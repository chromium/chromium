// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

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
  void CreateAndCloseInitializedStore() {
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

  // Custom deleter for the unique_ptr returned by ManuallyOpenDatabase.
  // It ensures that the store is re-initialized after manual DB access if it
  // was open before.
  struct DatabaseReopener {
    void operator()(sql::Database* db) const {
      delete db;
      if (test_fixture_to_reinit) {
        test_fixture_to_reinit->CreateAndInitStore();
      }
    }
    // Use raw_ptr to avoid ownership cycles and for safety.
    // This is null if the store doesn't need to be re-initialized.
    raw_ptr<SqlPersistentStoreTest> test_fixture_to_reinit = nullptr;
  };

  using DatabaseHandle = std::unique_ptr<sql::Database, DatabaseReopener>;

  // This will close the current store connection and automatically reopen it
  // when the returned handle goes out of scope.
  DatabaseHandle ManuallyOpenDatabase() {
    bool should_reopen = false;
    if (store_) {
      ClearStore();
      should_reopen = true;
    }

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
    return DatabaseHandle(db.release(), {should_reopen ? this : nullptr});
  }

  // Manually opens the meta table within the database.
  std::unique_ptr<sql::MetaTable> ManuallyOpenMetaTable(
      sql::Database* db_handle) {
    auto mata_table = std::make_unique<sql::MetaTable>();
    CHECK(mata_table->Init(db_handle, kSqlBackendCurrentDatabaseVersion,
                           kSqlBackendCurrentDatabaseVersion));
    return mata_table;
  }

  // Synchronous wrapper for CreateEntry.
  SqlPersistentStore::EntryInfoOrError CreateEntry(const CacheEntryKey& key) {
    base::test::TestFuture<SqlPersistentStore::EntryInfoOrError> future;
    store_->CreateEntry(key, future.GetCallback());
    return future.Take();
  }

  // Helper to create an entry and return its token, asserting success.
  base::UnguessableToken CreateEntryAndGetToken(const CacheEntryKey& key) {
    auto create_result = CreateEntry(key);
    CHECK(create_result.has_value())
        << "Failed to create entry for key: " << key.string();
    return create_result->token;
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

  // Synchronous wrapper for DeleteDoomedEntries.
  SqlPersistentStore::Error DeleteDoomedEntries(
      base::flat_set<base::UnguessableToken> excluded_tokens) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->DeleteDoomedEntries(std::move(excluded_tokens),
                                future.GetCallback());
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
      base::flat_set<CacheEntryKey> excluded_keys = {}) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->DeleteLiveEntriesBetween(
        initial_time, end_time, std::move(excluded_keys), future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for UpdateEntryLastUsed.
  SqlPersistentStore::Error UpdateEntryLastUsed(const CacheEntryKey& key,
                                                base::Time last_used) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->UpdateEntryLastUsed(key, last_used, future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for UpdateEntryHeaderAndLastUsed.
  SqlPersistentStore::Error UpdateEntryHeaderAndLastUsed(
      const CacheEntryKey& key,
      const base::UnguessableToken& token,
      base::Time last_used,
      scoped_refptr<net::IOBuffer> buffer,
      int64_t header_size_delta) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->UpdateEntryHeaderAndLastUsed(key, token, last_used,
                                         std::move(buffer), header_size_delta,
                                         future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for WriteEntryData.
  SqlPersistentStore::Error WriteEntryData(const CacheEntryKey& key,
                                           const base::UnguessableToken& token,
                                           int64_t old_body_end,
                                           int64_t offset,
                                           scoped_refptr<net::IOBuffer> buffer,
                                           int buf_len,
                                           bool truncate) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->WriteEntryData(key, token, old_body_end, offset, std::move(buffer),
                           buf_len, truncate, future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for ReadEntryData.
  SqlPersistentStore::IntOrError ReadEntryData(
      const base::UnguessableToken& token,
      int64_t offset,
      scoped_refptr<net::IOBuffer> buffer,
      int buf_len,
      int64_t body_end,
      bool sparse_reading) {
    base::test::TestFuture<SqlPersistentStore::IntOrError> future;
    store_->ReadEntryData(token, offset, std::move(buffer), buf_len, body_end,
                          sparse_reading, future.GetCallback());
    return future.Take();
  }

  // Helper to write data from a string_view and assert success.
  void WriteDataAndAssertSuccess(const CacheEntryKey& key,
                                 const base::UnguessableToken& token,
                                 int64_t old_body_end,
                                 int64_t offset,
                                 std::string_view data,
                                 bool truncate) {
    auto buffer = base::MakeRefCounted<net::StringIOBuffer>(std::string(data));
    ASSERT_EQ(WriteEntryData(key, token, old_body_end, offset,
                             std::move(buffer), data.size(), truncate),
              SqlPersistentStore::Error::kOk);
  }

  // Helper to fill a range with a repeated character and write it to the store.
  void FillDataInRange(const CacheEntryKey& key,
                       const base::UnguessableToken& token,
                       int64_t old_body_end,
                       int64_t start,
                       int64_t len,
                       char fill_char) {
    const std::string data(len, fill_char);
    WriteDataAndAssertSuccess(key, token, old_body_end, start, data,
                              /*truncate=*/false);
  }

  // Helper to write a sequence of single-byte blobs from a string_view and
  // verify the result.
  void WriteAndVerifySingleByteBlobs(const CacheEntryKey& key,
                                     const base::UnguessableToken& token,
                                     std::string_view content) {
    for (size_t i = 0; i < content.size(); ++i) {
      std::string data(1, content[i]);
      WriteDataAndAssertSuccess(key, token, i, i, data,
                                /*truncate=*/false);
    }
    ReadAndVerifyData(token, 0, content.size(), content.size(), false,
                      std::string(content));

    std::vector<BlobData> actual_blobs = GetAllBlobData(token);
    for (size_t i = 0; i < content.size(); ++i) {
      EXPECT_EQ(actual_blobs[i].start, i);
      EXPECT_EQ(actual_blobs[i].end, i + 1);
      ASSERT_THAT(actual_blobs[i].data, ElementsAre(content[i]));
    }
  }

  // Synchronous wrapper for GetEntryAvailableRange.
  RangeResult GetEntryAvailableRange(const base::UnguessableToken& token,
                                     int64_t offset,
                                     int len) {
    base::test::TestFuture<const RangeResult&> future;
    store_->GetEntryAvailableRange(token, offset, len, future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for CalculateSizeOfEntriesBetween.
  SqlPersistentStore::Int64OrError CalculateSizeOfEntriesBetween(
      base::Time initial_time,
      base::Time end_time) {
    base::test::TestFuture<SqlPersistentStore::Int64OrError> future;
    store_->CalculateSizeOfEntriesBetween(initial_time, end_time,
                                          future.GetCallback());
    return future.Take();
  }

  // Helper to read data and verify its content.
  void ReadAndVerifyData(const base::UnguessableToken& token,
                         int64_t offset,
                         int buffer_len,
                         int64_t body_end,
                         bool sparse_reading,
                         std::string_view expected_data) {
    auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(buffer_len);
    auto read_result = ReadEntryData(token, offset, read_buffer, buffer_len,
                                     body_end, sparse_reading);
    ASSERT_TRUE(read_result.has_value());
    EXPECT_EQ(read_result.value(), static_cast<int>(expected_data.size()));
    EXPECT_EQ(std::string_view(read_buffer->data(), read_result.value()),
              expected_data);
  }

  // Helper to count rows in the resource table.
  int64_t CountResourcesTable() {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement s(
        db_handle->GetUniqueStatement("SELECT COUNT(*) FROM resources"));
    CHECK(s.Step());
    return s.ColumnInt(0);
  }

  // Helper to count doomed rows in the resource table.
  int64_t CountDoomedResourcesTable(const CacheEntryKey& key) {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement s(db_handle->GetUniqueStatement(
        "SELECT COUNT(*) FROM resources WHERE cache_key=? AND doomed=?"));
    s.BindString(0, key.string());
    s.BindBool(1, true);  // doomed = true
    CHECK(s.Step());
    return s.ColumnInt64(0);
  }

  struct ResourceEntryDetails {
    base::Time last_used;
    int64_t bytes_usage;
    std::string head_data;
    bool doomed;
    int64_t body_end;
  };

  // Helper to read entry details from the resources table.
  std::optional<ResourceEntryDetails> GetResourceEntryDetails(
      const CacheEntryKey& key) {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement s(db_handle->GetUniqueStatement(
        "SELECT last_used, bytes_usage, head, doomed, body_end "
        "FROM resources WHERE cache_key=?"));
    s.BindString(0, key.string());
    if (s.Step()) {
      ResourceEntryDetails details;
      details.last_used = s.ColumnTime(0);
      details.bytes_usage = s.ColumnInt64(1);
      details.head_data =
          std::string(reinterpret_cast<const char*>(s.ColumnBlob(2).data()),
                      s.ColumnBlob(2).size());
      details.doomed = s.ColumnBool(3);
      details.body_end = s.ColumnInt64(4);
      return details;
    }
    return std::nullopt;
  }

  // Helper to verify the body_end and bytes_usage of a resource entry.
  void VerifyBodyEndAndBytesUsage(const CacheEntryKey& key,
                                  int64_t expected_body_end,
                                  int64_t expected_bytes_usage) {
    auto details = GetResourceEntryDetails(key);
    ASSERT_TRUE(details.has_value());
    EXPECT_EQ(details->body_end, expected_body_end);
    EXPECT_EQ(details->bytes_usage, expected_bytes_usage);
  }

  struct BlobData {
    int64_t start;
    int64_t end;
    std::vector<uint8_t> data;

    BlobData(int64_t s, int64_t e, std::vector<uint8_t> d)
        : start(s), end(e), data(std::move(d)) {}

    auto operator<=>(const BlobData&) const = default;
  };

  // Helper to create BlobData from a string_view for easier testing.
  BlobData MakeBlobData(int64_t start, std::string_view data) {
    return BlobData(start, start + data.size(),
                    std::vector<uint8_t>(data.begin(), data.end()));
  }

  // Helper to retrieve all blob data for a given entry token.
  std::vector<BlobData> GetAllBlobData(const base::UnguessableToken& token) {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement s(db_handle->GetUniqueStatement(
        "SELECT start, end, blob FROM blobs WHERE token_high=? AND token_low=? "
        "ORDER BY start"));
    s.BindInt64(0, static_cast<int64_t>(token.GetHighForSerialization()));
    s.BindInt64(1, static_cast<int64_t>(token.GetLowForSerialization()));

    std::vector<BlobData> blobs;
    while (s.Step()) {
      blobs.emplace_back(
          s.ColumnInt64(0), s.ColumnInt64(1),
          std::vector<uint8_t>(s.ColumnBlob(2).begin(), s.ColumnBlob(2).end()));
    }
    return blobs;
  }

  // Helper to check the blob data for a given token.
  void CheckBlobData(const base::UnguessableToken& token,
                     std::initializer_list<std::pair<int64_t, std::string_view>>
                         expected_blobs) {
    std::vector<BlobData> expected;
    for (const auto& blob_pair : expected_blobs) {
      expected.push_back(MakeBlobData(blob_pair.first, blob_pair.second));
    }
    EXPECT_THAT(GetAllBlobData(token), testing::ElementsAreArray(expected));
  }

  // Helper to corrupt the blob data for a given token.
  void CorruptBlobData(const base::UnguessableToken& token,
                       base::span<const uint8_t> new_data) {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE blobs SET blob = ? WHERE token_high = ? AND token_low = ?"));
    statement.BindBlob(0, new_data);
    statement.BindInt64(1, token.GetHighForSerialization());
    statement.BindInt64(2, token.GetLowForSerialization());
    ASSERT_TRUE(statement.Run());
  }

  // Helper to corrupt the start and end offsets for a given token.
  void CorruptBlobRange(const base::UnguessableToken& token,
                        int64_t new_start,
                        int64_t new_end) {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE blobs SET start = ?, end = ? WHERE token_high = ? AND "
        "token_low = ?"));
    statement.BindInt64(0, new_start);
    statement.BindInt64(1, new_end);
    statement.BindInt64(2, token.GetHighForSerialization());
    statement.BindInt64(3, token.GetLowForSerialization());
    ASSERT_TRUE(statement.Run());
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
  CreateAndCloseInitializedStore();

  // Create a new store with the same path, which should open the existing DB.
  CreateStore();
  EXPECT_EQ(Init(), SqlPersistentStore::Error::kOk);
}

// Tests that a database with a future (incompatible) version is razed
// (deleted and recreated).
TEST_F(SqlPersistentStoreTest, InitRazedTooNew) {
  CreateAndCloseInitializedStore();

  {
    // Manually open the database and set a future version number.
    auto db_handle = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db_handle.get());
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
  CreateAndCloseInitializedStore();

  // Verify that the old data is gone.
  auto db_handle = ManuallyOpenDatabase();
  auto meta_table = ManuallyOpenMetaTable(db_handle.get());
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
  CreateAndCloseInitializedStore();

  // Make the database file read-only.
  MakeFileUnwritable();

  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kFailedToOpenDatabase);
}

// Tests the recovery mechanism when the database file is corrupted.
TEST_F(SqlPersistentStoreTest, InitWithCorruptDatabase) {
  CreateAndCloseInitializedStore();

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

  // Manually set metadata.
  const int64_t kTestEntryCount = 123;
  const int64_t kTestTotalSize = 456789;
  {
    auto db_handle = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db_handle.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount,
                                     kTestEntryCount));
    ASSERT_TRUE(
        meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize, kTestTotalSize));
  }

  EXPECT_EQ(GetEntryCount(), kTestEntryCount);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kTestTotalSize + kTestEntryCount * kSqlBackendStaticResourceSize);
}

// Tests that GetEntryCount() and GetSizeOfAllEntries() handle invalid
// (e.g., negative) metadata by treating it as zero.
TEST_F(SqlPersistentStoreTest, GetEntryAndSizeWithInvalidMetadata) {
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  const CacheEntryKey kKey("my-key");
  const int64_t kEntrySize = kSqlBackendStaticResourceSize +
                             kKey.string().size() + kInitialData.size();
  const auto token = CreateEntryAndGetToken(kKey);
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), kEntrySize);
  ClearStore();

  // Test with a negative entry count. The total size should still be valid.
  {
    auto db_handle = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db_handle.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount, -1));
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize, 12345));
  }
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  // Both the entry count and size metadata must have been recalculated.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), kEntrySize);

  // Test with an entry count that exceeds the int32_t limit.
  {
    auto db_handle = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db_handle.get());
    ASSERT_TRUE(meta_table->SetValue(
        kSqlBackendMetaTableKeyEntryCount,
        static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1));
  }

  // Both the entry count and size metadata must have been recalculated.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), kEntrySize);

  // Test with an entry count with the int32_t limit.
  {
    auto db_handle = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db_handle.get());
    ASSERT_TRUE(meta_table->SetValue(
        kSqlBackendMetaTableKeyEntryCount,
        static_cast<int64_t>(std::numeric_limits<int32_t>::max())));
  }
  // Both the entry count and size metadata must not been recalculated.
  EXPECT_EQ(GetEntryCount(), std::numeric_limits<int32_t>::max());
  EXPECT_EQ(GetSizeOfAllEntries(),
            static_cast<int64_t>(std::numeric_limits<int32_t>::max()) *
                    kSqlBackendStaticResourceSize +
                kKey.string().size() + kInitialData.size());

  // Test with a negative total size. The entry count should still be valid.
  {
    auto db_handle = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db_handle.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount, 10));
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize, -1));
  }
  // Both the entry count and size metadata must have been recalculated.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), kEntrySize);

  // Test with a total size at the int64_t limit with no entries.
  {
    auto db_handle = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db_handle.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount, 0));
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize,
                                     std::numeric_limits<int64_t>::max()));
  }
  // Both the entry count and size metadata must have been recalculated.
  EXPECT_EQ(GetEntryCount(), 0);
  EXPECT_EQ(GetSizeOfAllEntries(), std::numeric_limits<int64_t>::max());

  // Test with a total size at the int64_t limit with one entry.
  {
    auto db_handle = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db_handle.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount, 1));
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize,
                                     std::numeric_limits<int64_t>::max()));
  }
  EXPECT_EQ(GetEntryCount(), 1);
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

  EXPECT_EQ(CountResourcesTable(), 1);
}

TEST_F(SqlPersistentStoreTest, OpenEntrySuccess) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  const auto created_token = CreateEntryAndGetToken(kKey);

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
  const auto created_token = CreateEntryAndGetToken(kKey);

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
  EXPECT_FALSE(CreateEntryAndGetToken(kKey).is_empty());

  // Manually open the database and corrupt the `token_high` and `token_low` for
  // the entry.
  {
    auto db_handle = ManuallyOpenDatabase();
    static constexpr char kSqlUpdateTokenLow[] =
        "UPDATE resources SET token_high=0, token_low=0 WHERE cache_key=?";
    sql::Statement statement(
        db_handle->GetCachedStatement(SQL_FROM_HERE, kSqlUpdateTokenLow));
    statement.BindString(0, kKey.string());
    ASSERT_TRUE(statement.Run());
  }

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
  const auto token_to_doom = CreateEntryAndGetToken(kKeyToDoom);
  const auto token_to_keep = CreateEntryAndGetToken(kKeyToKeep);
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
  EXPECT_EQ((*open_kept_result)->token, token_to_keep);

  // Verify the doomed entry still exists in the table but is marked as doomed,
  // and the other entry is unaffected.
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
  const auto token1 = CreateEntryAndGetToken(kKey1);
  const auto token2 = CreateEntryAndGetToken(kKey2);
  ASSERT_EQ(GetEntryCount(), 2);

  // Attempt to doom key1 with an incorrect token.
  ASSERT_EQ(DoomEntry(kKey1, base::UnguessableToken::Create()),
            SqlPersistentStore::Error::kNotFound);

  // Verify that the counts remain unchanged and both entries can still be
  // opened.
  EXPECT_EQ(GetEntryCount(), 2);
  EXPECT_EQ(GetSizeOfAllEntries(), size1 + size2);

  auto open_result1 = OpenEntry(kKey1);
  ASSERT_TRUE(open_result1.has_value());
  ASSERT_TRUE(open_result1->has_value());
  EXPECT_EQ((*open_result1)->token, token1);

  auto open_result2 = OpenEntry(kKey2);
  ASSERT_TRUE(open_result2.has_value());
  ASSERT_TRUE(open_result2->has_value());
  EXPECT_EQ((*open_result2)->token, token2);
}

TEST_F(SqlPersistentStoreTest, DoomEntryWithCorruptSizeRecovers) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToCorrupt("key-to-corrupt");
  const CacheEntryKey kKeyToKeep("key-to-keep");
  const int64_t keep_key_size = kKeyToKeep.string().size();
  const int64_t expected_size_after_recovery =
      kSqlBackendStaticResourceSize + keep_key_size;

  // Create one entry to keep, and one to corrupt and doom.
  const auto token_to_doom = CreateEntryAndGetToken(kKeyToCorrupt);
  ASSERT_TRUE(CreateEntry(kKeyToKeep).has_value());
  ASSERT_EQ(GetEntryCount(), 2);

  // Manually open the database and corrupt the `bytes_usage` for one entry
  // to an extreme value that will cause an overflow during calculation.
  {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE resources SET bytes_usage = ? WHERE cache_key = ?"));
    statement.BindInt64(0, std::numeric_limits<int64_t>::min());
    statement.BindString(1, kKeyToCorrupt.string());
    ASSERT_TRUE(statement.Run());
  }

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
  const auto token = CreateEntryAndGetToken(kKey);
  ASSERT_EQ(DoomEntry(kKey, token), SqlPersistentStore::Error::kOk);
  ASSERT_EQ(GetEntryCount(), 0);
  ASSERT_EQ(CountResourcesTable(), 1);

  // Delete the doomed entry.
  ASSERT_EQ(DeleteDoomedEntry(kKey, token), SqlPersistentStore::Error::kOk);

  // Verify the entry is now physically gone from the database.
  EXPECT_EQ(CountResourcesTable(), 0);
}

TEST_F(SqlPersistentStoreTest, DeleteDoomedEntryDeletesBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  const std::string kData = "some data";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kData, /*truncate=*/false);
  CheckBlobData(token, {{0, kData}});
  EXPECT_EQ(GetSizeOfAllEntries(), kSqlBackendStaticResourceSize +
                                       kKey.string().size() + kData.size());

  // DoomEntry is responsible for updating the total size of the cache.
  ASSERT_EQ(DoomEntry(kKey, token), SqlPersistentStore::Error::kOk);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);

  ASSERT_EQ(DeleteDoomedEntry(kKey, token), SqlPersistentStore::Error::kOk);
  CheckBlobData(token, {});
}

TEST_F(SqlPersistentStoreTest, DeleteDoomedEntryFailsOnLiveEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  // Create a live entry.
  const auto token = CreateEntryAndGetToken(kKey);
  ASSERT_EQ(GetEntryCount(), 1);

  // Attempt to delete it with DeleteDoomedEntry. This should fail because the
  // entry is not marked as doomed.
  auto result = DeleteDoomedEntry(kKey, token);
  ASSERT_EQ(result, SqlPersistentStore::Error::kNotFound);

  // Verify the entry still exists.
  EXPECT_EQ(GetEntryCount(), 1);
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
  const auto token_to_keep = CreateEntryAndGetToken(kKeyToKeep);
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
  EXPECT_EQ((*open_kept_result)->token, token_to_keep);

  // Verify the entry is physically gone from the database.
  EXPECT_EQ(CountResourcesTable(), 1);
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntryDeletesBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  const std::string kData = "some data";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kData, /*truncate=*/false);
  CheckBlobData(token, {{0, kData}});
  ASSERT_EQ(DeleteLiveEntry(kKey), SqlPersistentStore::Error::kOk);
  CheckBlobData(token, {});
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
  const auto doomed_token = CreateEntryAndGetToken(kDoomedKey);
  ASSERT_TRUE(CreateEntry(kLiveKey).has_value());

  // Doom one of the entries.
  ASSERT_EQ(DoomEntry(kDoomedKey, doomed_token),
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

  // Manually open the database and corrupt the token for one entry so that
  // it becomes invalid.
  {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE resources SET token_high = 0, token_low = 0 WHERE cache_key = "
        "?"));
    statement.BindString(0, kKeyToCorrupt.string());
    ASSERT_TRUE(statement.Run());
  }

  // Delete the entry with the corrupted token. This will trigger the
  // `corruption_detected` path, forcing a full recalculation.
  ASSERT_EQ(DeleteLiveEntry(kKeyToCorrupt), SqlPersistentStore::Error::kOk);

  // Verify that recovery was successful. The entry count and total size
  // should now reflect only the entry that was kept.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), expected_size_after_recovery);

  // Verify the state on disk. Only the un-corrupted entry should remain.
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

  // Manually open the database and corrupt the `bytes_usage` for one entry
  // to an extreme value that will cause an underflow during calculation.
  {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE resources SET bytes_usage = ? WHERE cache_key = ?"));
    statement.BindInt64(0, std::numeric_limits<int64_t>::max());
    statement.BindString(1, kKeyToCorrupt.string());
    ASSERT_TRUE(statement.Run());
  }

  // Delete the entry with the corrupted size. This will trigger an underflow
  // in `total_size_delta`, causing `!total_size_delta.IsValid()` to be true.
  // The store should recover by recalculating its state from the database.
  ASSERT_EQ(DeleteLiveEntry(kKeyToCorrupt), SqlPersistentStore::Error::kOk);

  // Verify that recovery was successful. The entry count and total size
  // should now reflect only the entry that was kept.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(), expected_size_after_recovery);

  // Verify the state on disk. Only the un-corrupted entry should remain.
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

  ASSERT_EQ(CountResourcesTable(), 2);

  // Delete all entries.
  ASSERT_EQ(DeleteAllEntries(), SqlPersistentStore::Error::kOk);

  // Verify the cache is empty.
  EXPECT_EQ(GetEntryCount(), 0);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);
  EXPECT_EQ(CountResourcesTable(), 0);

  // Verify the old entries cannot be opened.
  auto open_result = OpenEntry(kKey1);
  ASSERT_TRUE(open_result.has_value());
  EXPECT_FALSE(open_result->has_value());
}

TEST_F(SqlPersistentStoreTest, DeleteAllEntriesDeletesBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");
  const auto token1 = CreateEntryAndGetToken(kKey1);
  const std::string kData1 = "data1";
  WriteDataAndAssertSuccess(kKey1, token1, 0, 0, kData1, /*truncate=*/false);
  const CacheEntryKey kKey2("key2");
  const auto token2 = CreateEntryAndGetToken(kKey2);
  const std::string kData2 = "data2";
  WriteDataAndAssertSuccess(kKey2, token2, 0, 0, kData2, /*truncate=*/false);
  CheckBlobData(token1, {{0, kData1}});
  CheckBlobData(token2, {{0, kData2}});
  ASSERT_EQ(DeleteAllEntries(), SqlPersistentStore::Error::kOk);
  CheckBlobData(token1, {});
  CheckBlobData(token2, {});
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

TEST_F(SqlPersistentStoreTest, DeleteDoomedEntries) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToDoom1("key-to-doom1");
  const CacheEntryKey kKeyToDoom2("key-to-doom2");
  const CacheEntryKey kKeyToDoomActive("key-to-doom-active");
  const CacheEntryKey kKeyToKeep("key-to-keep");

  // Create entries that will be doomed.
  auto create_result1 = CreateEntry(kKeyToDoom1);
  ASSERT_TRUE(create_result1.has_value());
  const auto token_to_doom1 = create_result1->token;

  // Create entries that will be doomed.
  auto create_result2 = CreateEntry(kKeyToDoom2);
  ASSERT_TRUE(create_result2.has_value());
  const auto token_to_doom2 = create_result2->token;

  // Create an entry that will be doomed but also excluded from deletion.
  auto create_result3 = CreateEntry(kKeyToDoomActive);
  ASSERT_TRUE(create_result3.has_value());
  const auto token_to_doom_active = create_result3->token;

  // Create an entry that will be kept.
  auto create_result4 = CreateEntry(kKeyToKeep);
  // There should be 4 created entries.
  ASSERT_TRUE(create_result4.has_value());
  const auto token_to_keep = create_result4->token;

  // There should be 4 created entries.
  ASSERT_EQ(GetEntryCount(), 4);

  // Write data to the entries that will be doomed.
  const std::string kData1 = "doomed_data1";
  WriteDataAndAssertSuccess(kKeyToDoom1, token_to_doom1, /*old_body_end=*/0,
                            /*offset=*/0, kData1, /*truncate=*/false);
  const std::string kData2 = "doomed_data2";
  WriteDataAndAssertSuccess(kKeyToDoom2, token_to_doom2, /*old_body_end=*/0,
                            /*offset=*/0, kData2, /*truncate=*/false);
  const std::string kData3 = "doomed_active_data";
  WriteDataAndAssertSuccess(kKeyToDoomActive, token_to_doom_active,
                            /*old_body_end=*/0,
                            /*offset=*/0, kData3, /*truncate=*/false);
  const std::string kData4 = "keep-data";
  WriteDataAndAssertSuccess(kKeyToKeep, token_to_keep, /*old_body_end=*/0,
                            /*offset=*/0, kData4, /*truncate=*/false);
  // Doom all the entries that will be doomed.
  ASSERT_EQ(DoomEntry(kKeyToDoom1, token_to_doom1),
            SqlPersistentStore::Error::kOk);
  ASSERT_EQ(DoomEntry(kKeyToDoom2, token_to_doom2),
            SqlPersistentStore::Error::kOk);
  ASSERT_EQ(DoomEntry(kKeyToDoomActive, token_to_doom_active),
            SqlPersistentStore::Error::kOk);
  // The entry count after dooming 3 entries should be 1.
  ASSERT_EQ(GetEntryCount(), 1);

  // All resource blobs should be still available.
  EXPECT_EQ(CountResourcesTable(), 4);
  CheckBlobData(token_to_doom1, {{0, kData1}});
  CheckBlobData(token_to_doom2, {{0, kData2}});
  CheckBlobData(token_to_doom_active, {{0, kData3}});
  CheckBlobData(token_to_keep, {{0, kData4}});

  base::HistogramTester histogram_tester;

  // Delete all doomed entries except for kKeyToDoomActive.
  ASSERT_EQ(DeleteDoomedEntries({token_to_doom_active}),
            SqlPersistentStore::Error::kOk);

  // Verify that `DeleteDoomedEntriesCount` UMA was recorded in the histogram.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.DeleteDoomedEntriesCount", 2, 1);

  // Verify the entries for kKeyToDoom1 and kKeyToDoom2 are physically gone from
  // the database.
  EXPECT_EQ(CountResourcesTable(), 2);
  CheckBlobData(token_to_doom1, {});
  CheckBlobData(token_to_doom2, {});
  CheckBlobData(token_to_doom_active, {{0, kData3}});
  CheckBlobData(token_to_keep, {{0, kData4}});

  // Verify the live entry is still present.
  auto open_result1 = OpenEntry(kKeyToKeep);
  ASSERT_TRUE(open_result1.has_value());
  ASSERT_TRUE(open_result1->has_value());
  EXPECT_EQ(open_result1.value()->token, token_to_keep);
}

TEST_F(SqlPersistentStoreTest, DeleteDoomedEntriesNoDeletion) {
  CreateAndInitStore();

  // Scenario 1: No doomed entries exist.
  base::HistogramTester histogram_tester;
  ASSERT_EQ(DeleteDoomedEntries({}), SqlPersistentStore::Error::kOk);
  // Verify that the count histogram recorded a value of 0.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.DeleteDoomedEntriesCount", 0, 1);
  EXPECT_EQ(CountResourcesTable(), 0);

  // Scenario 2: All doomed entries are excluded.
  const CacheEntryKey kKeyToDoom1("key-to-doom1");
  const CacheEntryKey kKeyToDoom2("key-to-doom2");
  auto create_result1 = CreateEntry(kKeyToDoom1);
  ASSERT_TRUE(create_result1.has_value());
  const auto token_to_doom1 = create_result1->token;
  auto create_result2 = CreateEntry(kKeyToDoom2);
  ASSERT_TRUE(create_result2.has_value());
  const auto token_to_doom2 = create_result2->token;
  ASSERT_EQ(DoomEntry(kKeyToDoom1, token_to_doom1),
            SqlPersistentStore::Error::kOk);
  ASSERT_EQ(DoomEntry(kKeyToDoom2, token_to_doom2),
            SqlPersistentStore::Error::kOk);
  ASSERT_EQ(CountResourcesTable(), 2);

  base::HistogramTester histogram_tester2;
  // Exclude all doomed entries from deletion.
  ASSERT_EQ(DeleteDoomedEntries({token_to_doom1, token_to_doom2}),
            SqlPersistentStore::Error::kOk);
  // Verify that the count histogram recorded a value of 0.
  histogram_tester2.ExpectUniqueSample(
      "Net.SqlDiskCache.DeleteDoomedEntriesCount", 0, 1);
  // Verify that no entries were deleted.
  EXPECT_EQ(CountResourcesTable(), 2);
}

TEST_F(SqlPersistentStoreTest, DeleteDoomedEntriesWithCorruptToken) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToDoom("key-to-doom");

  // Create an entry that will be doomed.
  auto create_result = CreateEntry(kKeyToDoom);
  ASSERT_TRUE(create_result.has_value());
  const auto token_to_doom = create_result->token;

  // Doom the entry.
  ASSERT_EQ(DoomEntry(kKeyToDoom, token_to_doom),
            SqlPersistentStore::Error::kOk);
  ASSERT_EQ(GetEntryCount(), 0);
  ASSERT_EQ(CountResourcesTable(), 1);

  // Manually corrupt the token in the database.
  {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE resources SET token_high = 0, token_low = 0 WHERE cache_key = "
        "?"));
    statement.BindString(0, kKeyToDoom.string());
    ASSERT_TRUE(statement.Run());
  }

  base::HistogramTester histogram_tester;

  // Delete all doomed entries.
  ASSERT_EQ(DeleteDoomedEntries({}), SqlPersistentStore::Error::kOk);

  // Verify that the corruption was detected.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.Backend.DeleteDoomedEntries.ResultWithCorruption",
      SqlPersistentStore::Error::kOk, 1);
}

TEST_F(SqlPersistentStoreTest, ChangeEntryCountOverflowRecovers) {
  // Create and initialize a store to have a valid DB file.
  CreateAndInitStore();

  // Manually set the entry count to INT32_MAX.
  {
    auto db_handle = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db_handle.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyEntryCount,
                                     std::numeric_limits<int32_t>::max()));
  }

  // After re-openning the store. The manipulated count should be loaded.
  ASSERT_EQ(GetEntryCount(), std::numeric_limits<int32_t>::max());

  // Create a new entry. This will attempt to increment the counter, causing
  // an overflow. The store should recover by recalculating the count from
  // the `resources` table (which will be 1).
  const CacheEntryKey kKey("my-key");
  ASSERT_TRUE(CreateEntry(kKey).has_value());

  // The new count should be 1 (the one entry we just created), not an
  // overflowed value. The size should also be correct for one entry.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());

  // Verify by closing and re-opening that the correct value was persisted.
  CreateAndInitStore();
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());
}

TEST_F(SqlPersistentStoreTest, ChangeTotalSizeOverflowRecovers) {
  // Create and initialize a store.
  CreateAndInitStore();

  // Manually set the total size to INT64_MAX.
  {
    auto db_handle = ManuallyOpenDatabase();
    auto meta_table = ManuallyOpenMetaTable(db_handle.get());
    ASSERT_TRUE(meta_table->SetValue(kSqlBackendMetaTableKeyTotalSize,
                                     std::numeric_limits<int64_t>::max()));
  }

  // Re-open the store and confirm it loaded the manipulated size.
  ASSERT_EQ(GetSizeOfAllEntries(), std::numeric_limits<int64_t>::max());
  ASSERT_EQ(GetEntryCount(), 0);

  // Create a new entry. This will attempt to increment the total size,
  // causing an overflow. The store should recover by recalculating.
  const CacheEntryKey kKey("my-key");
  ASSERT_TRUE(CreateEntry(kKey).has_value());

  // The new total size should be just the size of the new entry.
  // The entry count should have been incremented from its initial state (0).
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());

  // Verify that the correct values were persisted to the database.
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
  {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE resources SET last_used = ? WHERE cache_key = ?"));
    statement.BindTime(0, kBaseTime);
    statement.BindString(1, kKey4.string());
    ASSERT_TRUE(statement.Run());
  }
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
  base::flat_set<CacheEntryKey> excluded_keys = {kKey2};
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

  EXPECT_EQ(CountResourcesTable(), 4);
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntriesBetweenDeletesBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");
  const auto token1 = CreateEntryAndGetToken(kKey1);
  const std::string kData1 = "data1";
  WriteDataAndAssertSuccess(kKey1, token1, 0, 0, kData1, /*truncate=*/false);
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time time1 = base::Time::Now();
  const CacheEntryKey kKey2("key2");
  const auto token2 = CreateEntryAndGetToken(kKey2);
  const std::string kData2 = "data2";
  WriteDataAndAssertSuccess(kKey2, token2, 0, 0, kData2, /*truncate=*/false);
  CheckBlobData(token1, {{0, kData1}});
  CheckBlobData(token2, {{0, kData2}});
  ASSERT_EQ(DeleteLiveEntriesBetween(time1, base::Time::Max()),
            SqlPersistentStore::Error::kOk);
  CheckBlobData(token1, {{0, kData1}});  // Should not be deleted
  CheckBlobData(token2, {});             // Should be deleted
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

  {
    auto db_handle = ManuallyOpenDatabase();
    // Set bytes_usage for kKeyToCorrupt to cause overflow when subtracted
    // during deletion.
    sql::Statement update_corrupt_stmt(db_handle->GetUniqueStatement(
        "UPDATE resources SET bytes_usage=? WHERE cache_key=?"));
    update_corrupt_stmt.BindInt64(0, std::numeric_limits<int64_t>::min());
    update_corrupt_stmt.BindString(1, kKeyToCorrupt.string());
    ASSERT_TRUE(update_corrupt_stmt.Run());
  }

  base::HistogramTester histogram_tester;

  // Delete entries in a range that includes kKeyToCorrupt [kTimeCorrupt,
  // kTimeKeep). kKeyToKeep's last_used time is kTimeKeep, so it's not <
  // kTimeKeep.
  ASSERT_EQ(DeleteLiveEntriesBetween(kTimeCorrupt, kTimeKeep),
            SqlPersistentStore::Error::kOk);

  // Verify that `ResultWithCorruption` UMA was recorded in the histogram due to
  // the corrupted bytes_usage.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.Backend.DeleteLiveEntriesBetween.ResultWithCorruption",
      SqlPersistentStore::Error::kOk, 1);

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

  {
    // Manually corrupt the token of kKeyToCorrupt in the database.
    // This simulates a scenario where the token data is invalid.
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(
        db_handle->GetUniqueStatement("UPDATE resources SET token_high=0, "
                                      "token_low=0 WHERE cache_key=?"));
    statement.BindString(0, kKeyToCorrupt.string());
    ASSERT_TRUE(statement.Run());
  }

  base::HistogramTester histogram_tester;
  ASSERT_EQ(DeleteLiveEntriesBetween(kTimeCorrupt, kTimeKeep),
            SqlPersistentStore::Error::kOk);
  // Verify that `ResultWithCorruption` UMA was recorded in the histogram.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.Backend.DeleteLiveEntriesBetween.ResultWithCorruption",
      SqlPersistentStore::Error::kOk, 1);

  EXPECT_EQ(GetEntryCount(), 1);  // kKeyToKeep should remain
  const int64_t expected_size_after_delete =
      kSqlBackendStaticResourceSize + kKeyToKeep.string().size();
  EXPECT_EQ(GetSizeOfAllEntries(), expected_size_after_delete);

  EXPECT_FALSE(OpenEntry(kKeyToCorrupt).value().has_value());
  EXPECT_TRUE(OpenEntry(kKeyToKeep).value().has_value());
}

TEST_F(SqlPersistentStoreTest, UpdateEntryLastUsedSuccess) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());
  const base::Time create_time = create_result->last_used;

  // Open to verify initial time.
  auto open_result1 = OpenEntry(kKey);
  ASSERT_TRUE(open_result1.has_value() && open_result1->has_value());
  EXPECT_EQ((*open_result1)->last_used, create_time);

  // Advance time and update.
  task_environment_.AdvanceClock(base::Minutes(5));
  const base::Time kNewTime = base::Time::Now();
  ASSERT_NE(kNewTime, create_time);

  ASSERT_EQ(UpdateEntryLastUsed(kKey, kNewTime),
            SqlPersistentStore::Error::kOk);

  // Open again to verify the updated time.
  auto open_result2 = OpenEntry(kKey);
  ASSERT_TRUE(open_result2.has_value() && open_result2->has_value());
  EXPECT_EQ((*open_result2)->last_used, kNewTime);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryLastUsedOnNonExistentEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("non-existent-key");
  ASSERT_EQ(UpdateEntryLastUsed(kKey, base::Time::Now()),
            SqlPersistentStore::Error::kNotFound);
  EXPECT_EQ(GetEntryCount(), 0);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryLastUsedOnDoomedEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("doomed-key");

  // Create and then doom the entry.
  const auto token = CreateEntryAndGetToken(kKey);
  ASSERT_EQ(DoomEntry(kKey, token), SqlPersistentStore::Error::kOk);

  // Attempting to update a doomed entry should fail as if it's not found.
  ASSERT_EQ(UpdateEntryLastUsed(kKey, base::Time::Now()),
            SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryHeaderAndLastUsedSuccessInitial) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Initial bytes_usage is just the key size.
  const int64_t initial_bytes_usage = kKey.string().size();
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + initial_bytes_usage);

  // Prepare new header data.
  const std::string kNewHeadData = "new_header_data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kNewHeadData);

  // Advance time for new last_used.
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time new_last_used = base::Time::Now();

  // Update the entry. Previous header size is 0 as it was null.
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, token, new_last_used, buffer,
                                   /*header_size_delta=*/kNewHeadData.size()),
      SqlPersistentStore::Error::kOk);

  // Verify in-memory stats.
  const int64_t expected_bytes_usage =
      initial_bytes_usage + kNewHeadData.size();
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + expected_bytes_usage);

  // Verify database content.
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(details->last_used, new_last_used);
  EXPECT_EQ(details->bytes_usage, expected_bytes_usage);
  EXPECT_EQ(details->head_data, kNewHeadData);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryHeaderAndLastUsedSuccessReplace) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Initial update with some header data.
  const std::string kInitialHeadData = "initial_data";
  auto initial_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kInitialHeadData);
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, token, base::Time::Now(),
                                   initial_buffer, kInitialHeadData.size()),
      SqlPersistentStore::Error::kOk);

  const int64_t initial_bytes_usage =
      kKey.string().size() + kInitialHeadData.size();
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + initial_bytes_usage);

  // Prepare new header data of the same size.
  const std::string kNewHeadData = "updated_data";
  ASSERT_EQ(kNewHeadData.size(), kInitialHeadData.size());
  auto new_buffer = base::MakeRefCounted<net::StringIOBuffer>(kNewHeadData);

  // Advance time for new last_used.
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time new_last_used = base::Time::Now();

  // Update the entry.
  ASSERT_EQ(UpdateEntryHeaderAndLastUsed(kKey, token, new_last_used, new_buffer,
                                         /*header_size_delta=*/0),
            SqlPersistentStore::Error::kOk);

  // Verify in-memory stats (should be unchanged as size is same).
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + initial_bytes_usage);

  // Verify database content.
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(details->last_used, new_last_used);
  EXPECT_EQ(details->bytes_usage, initial_bytes_usage);
  EXPECT_EQ(details->head_data, kNewHeadData);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryHeaderAndLastUsedSuccessGrow) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Initial update with some header data.
  const std::string kInitialHeadData = "short";
  auto initial_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kInitialHeadData);
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, token, base::Time::Now(),
                                   initial_buffer, kInitialHeadData.size()),
      SqlPersistentStore::Error::kOk);

  const int64_t initial_bytes_usage =
      kKey.string().size() + kInitialHeadData.size();
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + initial_bytes_usage);

  // Prepare new, larger header data.
  const std::string kNewHeadData = "much_longer_header_data";
  ASSERT_GT(kNewHeadData.size(), kInitialHeadData.size());
  auto new_buffer = base::MakeRefCounted<net::StringIOBuffer>(kNewHeadData);

  // Update the entry.
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(
          kKey, token, base::Time::Now(), new_buffer,
          static_cast<int64_t>(kNewHeadData.size()) - kInitialHeadData.size()),
      SqlPersistentStore::Error::kOk);

  // Verify in-memory stats.
  const int64_t expected_bytes_usage =
      kKey.string().size() + kNewHeadData.size();
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + expected_bytes_usage);

  // Verify database content.
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(details->bytes_usage, expected_bytes_usage);
  EXPECT_EQ(details->head_data, kNewHeadData);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryHeaderAndLastUsedSuccessShrink) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Initial update with large header data.
  const std::string kInitialHeadData = "much_longer_header_data";
  auto initial_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kInitialHeadData);
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, token, base::Time::Now(),
                                   initial_buffer, kInitialHeadData.size()),
      SqlPersistentStore::Error::kOk);

  const int64_t initial_bytes_usage =
      kKey.string().size() + kInitialHeadData.size();
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + initial_bytes_usage);

  // Prepare new, smaller header data.
  const std::string kNewHeadData = "short";
  ASSERT_LT(kNewHeadData.size(), kInitialHeadData.size());
  auto new_buffer = base::MakeRefCounted<net::StringIOBuffer>(kNewHeadData);

  // Update the entry.
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(
          kKey, token, base::Time::Now(), new_buffer,
          static_cast<int64_t>(kNewHeadData.size()) - kInitialHeadData.size()),
      SqlPersistentStore::Error::kOk);

  // Verify in-memory stats.
  const int64_t expected_bytes_usage =
      kKey.string().size() + kNewHeadData.size();
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + expected_bytes_usage);

  // Verify database content.
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(details->bytes_usage, expected_bytes_usage);
  EXPECT_EQ(details->head_data, kNewHeadData);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryHeaderAndLastUsedNotFound) {
  CreateAndInitStore();
  const CacheEntryKey kKey("non-existent-key");
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("data");

  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, base::UnguessableToken::Create(),
                                   base::Time::Now(), buffer, buffer->size()),
      SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryHeaderAndLastUsedWrongToken) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  ASSERT_TRUE(CreateEntry(kKey).has_value());
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("data");

  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, base::UnguessableToken::Create(),
                                   base::Time::Now(), buffer, buffer->size()),
      SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryHeaderAndLastUsedDoomedEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("doomed-key");
  const auto token = CreateEntryAndGetToken(kKey);
  ASSERT_EQ(DoomEntry(kKey, token), SqlPersistentStore::Error::kOk);

  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  ASSERT_EQ(UpdateEntryHeaderAndLastUsed(kKey, token, base::Time::Now(), buffer,
                                         buffer->size()),
            SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest,
       UpdateEntryHeaderAndLastUsedCorruptionDetectedAndRolledBack) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());
  const auto token = create_result->token;
  const base::Time initial_last_used = create_result->last_used;
  const int64_t initial_size_of_all_entries = GetSizeOfAllEntries();
  const int32_t initial_entry_count = GetEntryCount();

  // Manually corrupt the bytes_usage to a very small value.
  const int64_t corrupted_bytes_usage = 1;
  {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE resources SET bytes_usage = ? WHERE cache_key = ?"));
    statement.BindInt64(0, corrupted_bytes_usage);
    statement.BindString(1, kKey.string());
    ASSERT_TRUE(statement.Run());
  }

  ASSERT_EQ(GetSizeOfAllEntries(), initial_size_of_all_entries);
  ASSERT_EQ(GetEntryCount(), initial_entry_count);

  // Prepare a new header.
  const std::string kNewHeadData = "new_header_data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kNewHeadData);

  base::HistogramTester histogram_tester;

  // Update the entry. This should trigger corruption detection because
  // `bytes_usage` in the DB is inconsistent. The operation should fail and the
  // transaction should be rolled back.
  ASSERT_EQ(UpdateEntryHeaderAndLastUsed(kKey, token, base::Time::Now(), buffer,
                                         /*header_size_delta=*/buffer->size()),
            SqlPersistentStore::Error::kInvalidData);

  // Verify that `ResultWithCorruption` UMA was recorded in the histogram.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.Backend.UpdateEntryHeaderAndLastUsed."
      "ResultWithCorruption",
      SqlPersistentStore::Error::kInvalidData, 1);

  // Verify that the store status was NOT changed due to rollback.
  EXPECT_EQ(GetEntryCount(), initial_entry_count);
  EXPECT_EQ(GetSizeOfAllEntries(), initial_size_of_all_entries);

  // Verify database content was rolled back to its state before the UPDATE
  // call.
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(details->last_used, initial_last_used);
  EXPECT_EQ(details->bytes_usage, corrupted_bytes_usage);
  EXPECT_EQ(details->head_data, "");  // Header should remain empty.
}

TEST_F(SqlPersistentStoreTest, WriteAndReadData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  const std::string kData = "hello world";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kData, /*truncate=*/false);
  EXPECT_EQ(GetSizeOfAllEntries(), kSqlBackendStaticResourceSize +
                                       kKey.string().size() + kData.size());
  // Read data back.
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/kData.size(),
                    /*body_end=*/kData.size(), /*sparse_reading=*/false, kData);
  // Verify blob data in the database.
  CheckBlobData(token, {{0, kData}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, kData.size(),
                             kKey.string().size() + kData.size());
}

TEST_F(SqlPersistentStoreTest, ReadEntryDataInvalidDataSizeMismatch) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob data size so it doesn't match its start/end
  // offsets.
  CorruptBlobData(token, base::as_byte_span("short"));

  // This read will try to read the corrupted blob, which should be detected.
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kInitialData.size());
  auto read_result =
      ReadEntryData(token, /*offset=*/0, read_buffer, kInitialData.size(),
                    /*body_end=*/kInitialData.size(), /*sparse_reading=*/false);
  ASSERT_FALSE(read_result.has_value());
  EXPECT_EQ(read_result.error(), SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, ReadEntryDataInvalidDataRangeOverflow) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob's start and end to cause an overflow.
  CorruptBlobRange(token, std::numeric_limits<int64_t>::min(), 10);

  // This read will try to read the corrupted blob, which should be detected.
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kInitialData.size());
  auto read_result =
      ReadEntryData(token, /*offset=*/0, read_buffer, kInitialData.size(),
                    /*body_end=*/kInitialData.size(), /*sparse_reading=*/false);
  ASSERT_FALSE(read_result.has_value());
  EXPECT_EQ(read_result.error(), SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, TrimOverlappingBlobsInvalidDataSizeMismatch) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob data size so it doesn't match its start/end
  // offsets.
  CorruptBlobData(token, base::as_byte_span("short"));

  // This write will overlap with the corrupted blob, triggering
  // TrimOverlappingBlobs, which should detect the inconsistency.
  const std::string kOverwriteData = "abc";
  auto overwrite_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kOverwriteData);
  EXPECT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/2, overwrite_buffer,
                           kOverwriteData.size(), /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, TrimOverlappingBlobsInvalidDataRangeOverflow) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob's start and end to cause an overflow.
  CorruptBlobRange(token, std::numeric_limits<int64_t>::min(), 10);

  // This write will overlap with the corrupted blob, triggering
  // TrimOverlappingBlobs, which should detect the overflow.
  const std::string kOverwriteData = "abc";
  auto overwrite_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kOverwriteData);
  EXPECT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/5, overwrite_buffer,
                           kOverwriteData.size(), /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, TruncateExistingBlobsInvalidDataSizeMismatch) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob data size so it doesn't match its start/end
  // offsets.
  CorruptBlobData(token, base::as_byte_span("short"));

  // This write will truncate the entry, triggering TruncateExistingBlobs,
  // which should detect the inconsistency.
  EXPECT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/5, /*buffer=*/nullptr, /*buf_len=*/0,
                           /*truncate=*/true),
            SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, TruncateExistingBlobsInvalidDataRangeOverflow) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob's start and end to cause an overflow.
  CorruptBlobRange(token, std::numeric_limits<int64_t>::min(), 10);

  // This write will truncate the entry, triggering TruncateExistingBlobs,
  // which should detect the overflow.
  EXPECT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/0, /*buffer=*/nullptr, /*buf_len=*/0,
                           /*truncate=*/true),
            SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataInvalidArgument) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  const int buf_len = buffer->size();

  // Test with negative old_body_end.
  EXPECT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/-1, /*offset=*/0,
                           buffer, buf_len, /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidArgument);

  // Test with negative offset.
  EXPECT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/0, /*offset=*/-1,
                           buffer, buf_len, /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidArgument);

  // Test with offset + buf_len overflow.
  EXPECT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/0,
                           /*offset=*/std::numeric_limits<int64_t>::max(),
                           buffer, buf_len, /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidArgument);

  // Test with negative buf_len.
  EXPECT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                           buffer, /*buf_len=*/-1, /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidArgument);

  // Test with null buffer but positive buf_len.
  EXPECT_EQ(
      WriteEntryData(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                     /*buffer=*/nullptr, /*buf_len=*/1, /*truncate=*/false),
      SqlPersistentStore::Error::kInvalidArgument);

  // Test with buf_len > buffer->size().
  EXPECT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                           buffer, buf_len + 1, /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidArgument);
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataInvalidDataBodyEndMismatch) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Write initial data to set body_end.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  // The body_end in the database is now kInitialData.size().
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  ASSERT_EQ(details->body_end, kInitialData.size());

  // Now, try to write again, but provide an incorrect old_body_end.
  const std::string kOverwriteData = "abc";
  auto overwrite_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kOverwriteData);
  EXPECT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/5, /*offset=*/8,
                           overwrite_buffer, kOverwriteData.size(),
                           /*truncate=*/false),
            SqlPersistentStore::Error::kBodyEndMismatch);
}

TEST_F(SqlPersistentStoreTest, ReadEntryDataInvalidArgument) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  const int buf_len = buffer->size();

  // Test with negative offset.
  auto result = ReadEntryData(token, /*offset=*/-1, buffer, buf_len,
                              /*body_end=*/10, /*sparse_reading=*/false);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SqlPersistentStore::Error::kInvalidArgument);

  // Test with negative buf_len.
  result = ReadEntryData(token, /*offset=*/0, buffer, /*buf_len=*/-1,
                         /*body_end=*/10, /*sparse_reading=*/false);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SqlPersistentStore::Error::kInvalidArgument);

  // Test with null buffer.
  result = ReadEntryData(token, /*offset=*/0, /*buffer=*/nullptr, buf_len,
                         /*body_end=*/10, /*sparse_reading=*/false);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SqlPersistentStore::Error::kInvalidArgument);

  // Test with buf_len > buffer->size().
  result = ReadEntryData(token, /*offset=*/0, buffer, buf_len + 1,
                         /*body_end=*/10, /*sparse_reading=*/false);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SqlPersistentStore::Error::kInvalidArgument);
}

TEST_F(SqlPersistentStoreTest, OverwriteEntryData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  const std::string kOverwriteData = "abc";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/kInitialData.size(),
                            /*offset=*/2, kOverwriteData, /*truncate=*/false);
  // Verify size updates.
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(details->body_end, kInitialData.size());
  EXPECT_EQ(details->bytes_usage, kKey.string().size() + kInitialData.size());
  // Read back and verify.
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/kInitialData.size(),
                    /*body_end=*/kInitialData.size(), /*sparse_reading=*/false,
                    "12abc67890");
  // Verify blob data in the database.
  CheckBlobData(token, {{0, "12"}, {2, "abc"}, {5, "67890"}});
}

TEST_F(SqlPersistentStoreTest, AppendEntryData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  const std::string kInitialData = "initial";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  const std::string kAppendData = "-appended";
  const int64_t new_body_end = kInitialData.size() + kAppendData.size();
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/kInitialData.size(),
                            /*offset=*/kInitialData.size(), kAppendData,
                            /*truncate=*/false);
  // Read back and verify.
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/new_body_end,
                    new_body_end, /*sparse_reading=*/false, "initial-appended");
  // Verify blob data in the database.
  CheckBlobData(token, {{0, kInitialData}, {kInitialData.size(), kAppendData}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, new_body_end,
                             kKey.string().size() + new_body_end);
}

TEST_F(SqlPersistentStoreTest, TruncateEntryData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  const std::string kTruncateData = "abc";
  const int64_t new_body_end = 2 + kTruncateData.size();
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/kInitialData.size(),
                            /*offset=*/2, kTruncateData, /*truncate=*/true);
  // Read back and verify.
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/new_body_end,
                    new_body_end, /*sparse_reading=*/false, "12abc");
  // Verify blob data in the database.
  CheckBlobData(token, {{0, "12"}, {2, "abc"}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, new_body_end,
                             kKey.string().size() + new_body_end);
}

TEST_F(SqlPersistentStoreTest, TruncateWithNullBuffer) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Write some initial data.
  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  VerifyBodyEndAndBytesUsage(kKey, kInitialData.size(),
                             kKey.string().size() + kInitialData.size());

  // Now, truncate the entry to a smaller size using a null buffer.
  const int64_t kTruncateOffset = 5;
  ASSERT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/kTruncateOffset, /*buffer=*/nullptr,
                           /*buf_len=*/0, /*truncate=*/true),
            SqlPersistentStore::Error::kOk);

  // Read back and verify.
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/kTruncateOffset,
                    kTruncateOffset, /*sparse_reading=*/false, "12345");
  // Verify blob data in the database.
  CheckBlobData(token, {{0, "12345"}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, kTruncateOffset,
                             kKey.string().size() + kTruncateOffset);
}

TEST_F(SqlPersistentStoreTest, TruncateOverlappingMultipleBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  WriteAndVerifySingleByteBlobs(kKey, token, "01234");

  // Overwrite with a 2-byte truncate write in the middle.
  const std::string kOverwriteData = "XX";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/5, /*offset=*/1,
                            kOverwriteData, /*truncate=*/true);

  // The new body end should be offset + length = 1 + 2 = 3.
  const int64_t new_body_end = 3;

  // Verify the content.
  ReadAndVerifyData(token, 0, new_body_end, new_body_end, false, "0XX");

  // Verify the underlying blobs.
  // The original blob for "0" should be trimmed.
  // The original blobs for "1", "2", "3", "4" should be gone.
  // A new blob for "XX" should be present.
  CheckBlobData(token, {{0, "0"}, {1, "XX"}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, new_body_end,
                             kKey.string().size() + new_body_end);
}

TEST_F(SqlPersistentStoreTest, TruncateMultipleBlobsWithZeroLengthWrite) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  WriteAndVerifySingleByteBlobs(kKey, token, "01234");

  // Truncate at offset 2 with a zero-length write.
  ASSERT_EQ(
      WriteEntryData(kKey, token, /*old_body_end=*/5, /*offset=*/2,
                     /*buffer=*/nullptr, /*buf_len=*/0, /*truncate=*/true),
      SqlPersistentStore::Error::kOk);

  // The new body end should be the offset = 2.
  const int64_t new_body_end = 2;

  // Verify the content.
  ReadAndVerifyData(token, 0, new_body_end, new_body_end, false, "01");
  // Verify the underlying blobs.
  // The original blobs for "2", "3", "4" should be gone.
  // The original blob for "0" and "1" should remain.
  CheckBlobData(token, {{0, "0"}, {1, "1"}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, new_body_end,
                             kKey.string().size() + new_body_end);
}

TEST_F(SqlPersistentStoreTest, OverwriteMultipleBlobsWithoutTruncate) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  WriteAndVerifySingleByteBlobs(kKey, token, "01234");

  // Overwrite with a 2-byte write in the middle, without truncating.
  const std::string kOverwriteData = "AB";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/5, /*offset=*/1,
                            kOverwriteData, /*truncate=*/false);

  // The body end should remain 5.
  const int64_t new_body_end = 5;

  // Verify the content.
  ReadAndVerifyData(token, 0, new_body_end, new_body_end, false, "0AB34");
  // Verify the underlying blobs.
  CheckBlobData(token, {{0, "0"}, {1, "AB"}, {3, "3"}, {4, "4"}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, new_body_end,
                             kKey.string().size() + new_body_end);
}

TEST_F(SqlPersistentStoreTest, WriteToDoomedEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  ASSERT_EQ(DoomEntry(kKey, token), SqlPersistentStore::Error::kOk);
  const std::string kData = "hello world";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kData, /*truncate=*/false);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);
  // Verify blob data in the database.
  CheckBlobData(token, {{0, kData}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, kData.size(),
                             kKey.string().size() + kData.size());
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataNotFound) {
  CreateAndInitStore();
  const CacheEntryKey kKey("non-existent-key");
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  ASSERT_EQ(WriteEntryData(kKey, base::UnguessableToken::Create(),
                           /*old_body_end=*/0, /*offset=*/0, write_buffer,
                           write_buffer->size(), /*truncate=*/false),
            SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataWrongToken) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  ASSERT_TRUE(CreateEntry(kKey).has_value());

  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  ASSERT_EQ(WriteEntryData(kKey, base::UnguessableToken::Create(),
                           /*old_body_end=*/0, /*offset=*/0, write_buffer,
                           write_buffer->size(), /*truncate=*/false),
            SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataNullBufferNoTruncate) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  CheckBlobData(token, {{0, kInitialData}});

  const int64_t initial_body_end = kInitialData.size();
  const int64_t initial_size_of_all_entries = GetSizeOfAllEntries();

  // Writing a null buffer with truncate=false should be a no-op.
  ASSERT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/initial_body_end,
                           /*offset=*/5, /*buffer=*/nullptr, /*buf_len=*/0,
                           /*truncate=*/false),
            SqlPersistentStore::Error::kOk);

  // Verify size and content are unchanged.
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(details->body_end, initial_body_end);
  EXPECT_EQ(GetSizeOfAllEntries(), initial_size_of_all_entries);
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/initial_body_end,
                    initial_body_end, /*sparse_reading=*/false, kInitialData);
  CheckBlobData(token, {{0, kInitialData}});
  VerifyBodyEndAndBytesUsage(kKey, kInitialData.size(),
                             kKey.string().size() + kInitialData.size());
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataZeroLengthBufferNoTruncate) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  CheckBlobData(token, {{0, kInitialData}});

  const int64_t initial_body_end = kInitialData.size();
  const int64_t initial_size_of_all_entries = GetSizeOfAllEntries();

  // Writing a zero-length buffer with truncate=false should be a no-op.
  auto zero_buffer = base::MakeRefCounted<net::IOBufferWithSize>(0);
  ASSERT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/initial_body_end,
                           /*offset=*/5, zero_buffer, /*buf_len=*/0,
                           /*truncate=*/false),
            SqlPersistentStore::Error::kOk);

  // Verify size and content are unchanged.
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(details->body_end, initial_body_end);
  EXPECT_EQ(GetSizeOfAllEntries(), initial_size_of_all_entries);
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/initial_body_end,
                    initial_body_end, /*sparse_reading=*/false, kInitialData);
  CheckBlobData(token, {{0, kInitialData}});
  VerifyBodyEndAndBytesUsage(kKey, kInitialData.size(),
                             kKey.string().size() + kInitialData.size());
}

TEST_F(SqlPersistentStoreTest, TruncateWithNullBufferExtendingBody) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Write some initial data.
  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Now, truncate the entry to a larger size using a null buffer.
  const int64_t kTruncateOffset = 20;
  ASSERT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/kTruncateOffset, /*buffer=*/nullptr,
                           /*buf_len=*/0, /*truncate=*/true),
            SqlPersistentStore::Error::kOk);

  // Read back and verify. The new space should be zero-filled.
  std::string expected_data = kInitialData;
  expected_data.append(kTruncateOffset - kInitialData.size(), '\0');
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/kTruncateOffset,
                    kTruncateOffset, /*sparse_reading=*/false, expected_data);
  // Verify blob data in the database.
  CheckBlobData(token, {{0, kInitialData}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, kTruncateOffset,
                             kKey.string().size() + kInitialData.size());
}

TEST_F(SqlPersistentStoreTest, ExtendWithNullBufferNoTruncate) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Write some initial data.
  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Now, extend the entry to a larger size using a null buffer without
  // truncate.
  const int64_t kExtendOffset = 20;
  ASSERT_EQ(WriteEntryData(kKey, token, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/kExtendOffset, /*buffer=*/nullptr,
                           /*buf_len=*/0, /*truncate=*/false),
            SqlPersistentStore::Error::kOk);

  // Read back and verify. The new space should be zero-filled.
  std::string expected_data = kInitialData;
  expected_data.append(kExtendOffset - kInitialData.size(), '\0');
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/kExtendOffset,
                    kExtendOffset, /*sparse_reading=*/false, expected_data);
  // Verify blob data in the database.
  CheckBlobData(token, {{0, kInitialData}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, kExtendOffset,
                             kKey.string().size() + kInitialData.size());
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataComplexOverlap) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // 1. Initial write.
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            "0123456789", /*truncate=*/false);
  ReadAndVerifyData(token, 0, 10, 10, false, "0123456789");
  CheckBlobData(token, {{0, "0123456789"}});
  VerifyBodyEndAndBytesUsage(kKey, 10, kKey.string().size() + 10);

  // 2. Overwrite middle.
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/10, /*offset=*/2,
                            "AAAA", /*truncate=*/false);
  ReadAndVerifyData(token, 0, 10, 10, false, "01AAAA6789");
  CheckBlobData(token, {{0, "01"}, {2, "AAAA"}, {6, "6789"}});
  VerifyBodyEndAndBytesUsage(kKey, 10, kKey.string().size() + 10);

  // 3. Overwrite end.
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/10, /*offset=*/8,
                            "BB", /*truncate=*/false);
  ReadAndVerifyData(token, 0, 10, 10, false, "01AAAA67BB");
  CheckBlobData(token, {{0, "01"}, {2, "AAAA"}, {6, "67"}, {8, "BB"}});
  VerifyBodyEndAndBytesUsage(kKey, 10, kKey.string().size() + 10);

  // 4. Overwrite beginning.
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/10, /*offset=*/0, "C",
                            /*truncate=*/false);
  ReadAndVerifyData(token, 0, 10, 10, false, "C1AAAA67BB");
  CheckBlobData(token, {{0, "C"}, {1, "1"}, {2, "AAAA"}, {6, "67"}, {8, "BB"}});
  VerifyBodyEndAndBytesUsage(kKey, 10, kKey.string().size() + 10);

  // 5. Overwrite all.
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/10, /*offset=*/0,
                            "DDDDDDDDDD", /*truncate=*/false);
  ReadAndVerifyData(token, 0, 10, 10, false, "DDDDDDDDDD");
  CheckBlobData(token, {{0, "DDDDDDDDDD"}});
  VerifyBodyEndAndBytesUsage(kKey, 10, kKey.string().size() + 10);

  // 6. Append.
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/10, /*offset=*/10,
                            "E", /*truncate=*/false);
  ReadAndVerifyData(token, 0, 11, 11, false, "DDDDDDDDDDE");
  CheckBlobData(token, {{0, "DDDDDDDDDD"}, {10, "E"}});
  VerifyBodyEndAndBytesUsage(kKey, 11, kKey.string().size() + 11);

  // 7. Sparse write.
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/11, /*offset=*/12,
                            "F", /*truncate=*/false);
  ReadAndVerifyData(token, 0, 13, 13, false,
                    base::MakeStringViewWithNulChars("DDDDDDDDDDE\0F"));
  CheckBlobData(token, {{0, "DDDDDDDDDD"}, {10, "E"}, {12, "F"}});
  VerifyBodyEndAndBytesUsage(kKey, 13, kKey.string().size() + 12);

  // 8. Overwrite with truncate.
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/13, /*offset=*/5,
                            "GG", /*truncate=*/true);
  ReadAndVerifyData(token, 0, 7, 7, false, "DDDDDGG");
  CheckBlobData(token, {{0, "DDDDD"}, {5, "GG"}});
  VerifyBodyEndAndBytesUsage(kKey, 7, kKey.string().size() + 7);

  // 9. Null buffer truncate.
  ASSERT_EQ(
      WriteEntryData(kKey, token, /*old_body_end=*/7, /*offset=*/5,
                     /*buffer=*/nullptr, /*buf_len=*/0, /*truncate=*/true),
      SqlPersistentStore::Error::kOk);
  ReadAndVerifyData(token, 0, 5, 5, false, "DDDDD");
  CheckBlobData(token, {{0, "DDDDD"}});
  VerifyBodyEndAndBytesUsage(kKey, 5, kKey.string().size() + 5);

  // 10. Write into a sparse region.
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/5, /*offset=*/10,
                            "SPARSE", /*truncate=*/false);
  ReadAndVerifyData(token, 0, 16, 16, false,
                    base::MakeStringViewWithNulChars("DDDDD\0\0\0\0\0SPARSE"));
  CheckBlobData(token, {{0, "DDDDD"}, {10, "SPARSE"}});
  VerifyBodyEndAndBytesUsage(kKey, 16, kKey.string().size() + 11);
}

TEST_F(SqlPersistentStoreTest, SparseRead) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  const std::string kData1 = "chunk1";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/0,
                            kData1, /*truncate=*/false);
  const std::string kData2 = "chunk2";
  const int64_t offset2 = 10;
  const int64_t new_body_end = offset2 + kData2.size();
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/kData1.size(),
                            /*offset=*/offset2, kData2, /*truncate=*/false);
  // Read with zero-filling.
  std::string expected_data = "chunk1";
  expected_data.append(offset2 - kData1.size(), '\0');
  expected_data.append("chunk2");
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/new_body_end,
                    new_body_end, /*sparse_reading=*/false, expected_data);

  // Read with sparse_reading=true.
  // A sparse read that encounters a gap should stop at the end of the first
  // chunk.
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/new_body_end,
                    new_body_end, /*sparse_reading=*/true, kData1);

  // A sparse read that extends into the gap should still stop at the end of
  // the first chunk.
  ReadAndVerifyData(token, /*offset=*/0, /*buffer_len=*/kData1.size() + 1,
                    new_body_end, /*sparse_reading=*/true, kData1);

  // Read from the middle of chunk2.
  const int64_t read_offset = offset2 + 2;  // Start at 'u' in "chunk2"
  const int read_len = 2;
  ReadAndVerifyData(token, read_offset, /*buffer_len=*/read_len, new_body_end,
                    /*sparse_reading=*/false, "un");

  // Read from the middle of chunk2, past the end of the data.
  const int long_read_len = 20;
  ReadAndVerifyData(token, read_offset, /*buffer_len=*/long_read_len,
                    new_body_end, /*sparse_reading=*/false, "unk2");

  // Verify blob data in the database.
  CheckBlobData(token, {{0, kData1}, {offset2, kData2}});
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeNoData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  auto result = GetEntryAvailableRange(token, 0, 100);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 0);
  EXPECT_EQ(result.available_len, 0);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeNoOverlap) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  const std::string kData = "some data";
  WriteDataAndAssertSuccess(kKey, token, /*old_body_end=*/0, /*offset=*/100,
                            kData, /*truncate=*/false);

  // Query before the data.
  auto result1 = GetEntryAvailableRange(token, 0, 50);
  EXPECT_EQ(result1.net_error, net::OK);
  EXPECT_EQ(result1.start, 0);
  EXPECT_EQ(result1.available_len, 0);

  // Query after the data.
  auto result2 = GetEntryAvailableRange(token, 200, 50);
  EXPECT_EQ(result2.net_error, net::OK);
  EXPECT_EQ(result2.start, 200);
  EXPECT_EQ(result2.available_len, 0);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeFullOverlap) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  FillDataInRange(kKey, token, /*old_body_end=*/0, 100, 100, 'a');

  auto result = GetEntryAvailableRange(token, 100, 100);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 100);
}

// Tests a query range that ends within the existing data.
// Query: [50, 150), Data: [100, 200) -> Overlap: [100, 150)
TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeQueryEndsInData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  FillDataInRange(kKey, token, /*old_body_end=*/0, 100, 100, 'a');

  auto result = GetEntryAvailableRange(token, 50, 100);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 50);
}

// Tests a query range that starts within the existing data.
// Query: [150, 250), Data: [100, 200) -> Overlap: [150, 200)
TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeQueryStartsInData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  FillDataInRange(kKey, token, /*old_body_end=*/0, 100, 100, 'a');

  auto result = GetEntryAvailableRange(token, 150, 100);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 150);
  EXPECT_EQ(result.available_len, 50);
}

// Tests a query range that fully contains the existing data.
// Query: [50, 250), Data: [100, 200) -> Overlap: [100, 200)
TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeQueryContainsData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  FillDataInRange(kKey, token, /*old_body_end=*/0, 100, 100, 'a');

  auto result = GetEntryAvailableRange(token, 50, 200);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 100);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeContained) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  FillDataInRange(kKey, token, /*old_body_end=*/0, 50, 200, 'a');

  auto result = GetEntryAvailableRange(token, 100, 100);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 100);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeContiguousBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  FillDataInRange(kKey, token, /*old_body_end=*/0, 100, 100, 'a');
  FillDataInRange(kKey, token, /*old_body_end=*/200, 200, 100, 'b');

  auto result = GetEntryAvailableRange(token, 100, 200);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 200);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeNonContiguousBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  FillDataInRange(kKey, token, /*old_body_end=*/0, 100, 100, 'a');
  FillDataInRange(kKey, token, /*old_body_end=*/200, 300, 100, 'b');

  auto result = GetEntryAvailableRange(token, 100, 300);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 100);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeMultipleBlobsStopsAtGap) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  // Write three blobs: [100, 200), [200, 300), [400, 500)
  FillDataInRange(kKey, token, 0, 100, 100, 'a');
  FillDataInRange(kKey, token, 200, 200, 100, 'a');
  FillDataInRange(kKey, token, 300, 400, 100, 'a');

  // Query for [150, 450). Should return [150, 300), which has length 150.
  auto result = GetEntryAvailableRange(token, 150, 300);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 150);
  EXPECT_EQ(result.available_len, 150);
}

TEST_F(SqlPersistentStoreTest, OpenLatestEntryBeforeResIdEmptyCache) {
  CreateAndInitStore();
  auto result = OpenLatestEntryBeforeResId(std::numeric_limits<int64_t>::max());
  EXPECT_FALSE(result.has_value());
}

TEST_F(SqlPersistentStoreTest, OpenLatestEntryBeforeResIdSingleEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  const auto created_token = CreateEntryAndGetToken(kKey);

  // Open the first (and only) entry.
  auto next_result1 =
      OpenLatestEntryBeforeResId(std::numeric_limits<int64_t>::max());
  ASSERT_TRUE(next_result1.has_value());
  EXPECT_EQ(next_result1->key, kKey);
  EXPECT_EQ(next_result1->info.token, created_token);
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

  const auto token1 = CreateEntryAndGetToken(kKey1);
  const auto token2 = CreateEntryAndGetToken(kKey2);
  const auto token3 = CreateEntryAndGetToken(kKey3);

  // Entries should be returned in reverse order of creation (descending
  // res_id).
  auto next_result =
      OpenLatestEntryBeforeResId(std::numeric_limits<int64_t>::max());
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey3);
  EXPECT_EQ(next_result->info.token, token3);
  const int64_t res_id3 = next_result->res_id;

  next_result = OpenLatestEntryBeforeResId(res_id3);
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey2);
  EXPECT_EQ(next_result->info.token, token2);
  const int64_t res_id2 = next_result->res_id;

  next_result = OpenLatestEntryBeforeResId(res_id2);
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey1);
  EXPECT_EQ(next_result->info.token, token1);
  const int64_t res_id1 = next_result->res_id;

  next_result = OpenLatestEntryBeforeResId(res_id1);
  EXPECT_FALSE(next_result.has_value());
}

TEST_F(SqlPersistentStoreTest, OpenLatestEntryBeforeResIdSkipsDoomed) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKeyToDoom("key-to-doom");
  const CacheEntryKey kKey3("key3");

  ASSERT_TRUE(CreateEntry(kKey1).has_value());
  const auto token_to_doom = CreateEntryAndGetToken(kKeyToDoom);
  ASSERT_TRUE(CreateEntry(kKey3).has_value());

  // Doom the middle entry.
  ASSERT_EQ(DoomEntry(kKeyToDoom, token_to_doom),
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
  ASSERT_TRUE(CreateEntry(kKeyInvalid).has_value());
  ASSERT_TRUE(CreateEntry(kKeyValidAfter).has_value());

  // Manually corrupt the token for kKeyInvalid.
  {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE resources SET token_high=0, token_low=0 WHERE cache_key=?"));
    statement.BindString(0, kKeyInvalid.string());
    ASSERT_TRUE(statement.Run());
  }

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
  // Verify that `ResultWithCorruption` UMA was recorded in the histogram.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.Backend.OpenLatestEntryBeforeResId."
      "ResultWithCorruption",
      SqlPersistentStore::Error::kOk, 1);

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
  const auto token = CreateEntryAndGetToken(kKey);
  bool callback_run = false;
  store_->DoomEntry(kKey, token,
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
  const auto token = CreateEntryAndGetToken(kKey);
  ASSERT_EQ(DoomEntry(kKey, token), SqlPersistentStore::Error::kOk);

  bool callback_run = false;
  store_->DeleteDoomedEntry(
      kKey, token, base::BindLambdaForTesting([&](SqlPersistentStore::Error) {
        callback_run = true;
      }));
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

TEST_F(SqlPersistentStoreTest,
       UpdateEntryLastUsedCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  ASSERT_TRUE(CreateEntry(kKey).has_value());

  bool callback_run = false;
  store_->UpdateEntryLastUsed(
      kKey, base::Time::Now(),
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::Error) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest,
       UpdateEntryHeaderAndLastUsedCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);

  bool callback_run = false;
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  store_->UpdateEntryHeaderAndLastUsed(
      kKey, token, base::Time::Now(), buffer, buffer->size(),
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::Error) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest, WriteDataCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  const std::string kData = "hello world";
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  bool callback_run = false;
  store_->WriteEntryData(
      kKey, token, /*old_body_end=*/0, /*offset=*/0, write_buffer, kData.size(),
      /*truncate=*/false,
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::Error) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();
  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest, ReadDataCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  bool callback_run = false;
  store_->ReadEntryData(
      token, /*offset=*/0, read_buffer, read_buffer->size(),
      /*body_end=*/10, /*sparse_reading=*/false,
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::IntOrError) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();
  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest, CalculateSizeOfEntriesBetween) {
  CreateAndInitStore();

  // Empty cache.
  auto result =
      CalculateSizeOfEntriesBetween(base::Time::Min(), base::Time::Max());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 0);

  const CacheEntryKey kKey1("key1");
  const std::string kData1 = "apple";
  const CacheEntryKey kKey2("key2");
  const std::string kData2 = "orange";
  const CacheEntryKey kKey3("key3");
  const std::string kData3 = "pineapple";

  const int64_t size1 =
      kSqlBackendStaticResourceSize + kKey1.string().size() + kData1.size();
  const int64_t size2 =
      kSqlBackendStaticResourceSize + kKey2.string().size() + kData2.size();
  const int64_t size3 =
      kSqlBackendStaticResourceSize + kKey3.string().size() + kData3.size();

  // Create entry 1.
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time time1 = base::Time::Now();
  const auto token1 = CreateEntryAndGetToken(kKey1);
  WriteDataAndAssertSuccess(kKey1, token1, 0, 0, kData1, /*truncate=*/false);

  // Create entry 2.
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time time2 = base::Time::Now();
  const auto token2 = CreateEntryAndGetToken(kKey2);
  WriteDataAndAssertSuccess(kKey2, token2, 0, 0, kData2, /*truncate=*/false);

  // Create entry 3.
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time time3 = base::Time::Now();
  const auto token3 = CreateEntryAndGetToken(kKey3);
  WriteDataAndAssertSuccess(kKey3, token3, 0, 0, kData3, /*truncate=*/false);

  // Total size.
  EXPECT_EQ(GetSizeOfAllEntries(), size1 + size2 + size3);

  // All entries (fast path).
  result = CalculateSizeOfEntriesBetween(base::Time::Min(), base::Time::Max());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), size1 + size2 + size3);

  // All entries (regular path).
  result = CalculateSizeOfEntriesBetween(base::Time::Min(),
                                         base::Time::Max() - base::Seconds(1));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), size1 + size2 + size3);

  // Only entry 2.
  result = CalculateSizeOfEntriesBetween(time2, time3);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), size2);

  // Entries 1 and 2.
  result = CalculateSizeOfEntriesBetween(time1, time3);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), size1 + size2);

  // Entries 2 and 3.
  result = CalculateSizeOfEntriesBetween(time2, base::Time::Max());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), size2 + size3);

  // No entries.
  result = CalculateSizeOfEntriesBetween(time3 + base::Minutes(1),
                                         base::Time::Max());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 0);
}

TEST_F(SqlPersistentStoreTest, CalculateSizeOfEntriesBetweenOverflow) {
  CreateAndInitStore();

  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");

  const base::Time time1 = base::Time::Now();
  task_environment_.AdvanceClock(base::Minutes(1));
  ASSERT_TRUE(CreateEntry(kKey1).has_value());

  task_environment_.AdvanceClock(base::Minutes(1));
  ASSERT_TRUE(CreateEntry(kKey2).has_value());

  task_environment_.AdvanceClock(base::Minutes(1));

  // Manually set bytes_usage to large values for both entries.
  {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement update_stmt(db_handle->GetUniqueStatement(
        "UPDATE resources SET bytes_usage = ? WHERE cache_key = ?"));
    update_stmt.BindInt64(0, std::numeric_limits<int64_t>::max() / 2);
    update_stmt.BindString(1, kKey1.string());
    ASSERT_TRUE(update_stmt.Run());
    update_stmt.Reset(/*clear_bound_vars=*/true);

    update_stmt.BindInt64(0, std::numeric_limits<int64_t>::max() / 2 + 100);
    update_stmt.BindString(1, kKey2.string());
    ASSERT_TRUE(update_stmt.Run());
  }

  // The sum of bytes_usage for both entries plus the static overhead will
  // overflow int64_t. The ClampedNumeric should saturate at max().
  auto result = CalculateSizeOfEntriesBetween(time1, base::Time::Max());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), std::numeric_limits<int64_t>::max());
}

TEST_F(SqlPersistentStoreTest, CalculateSizeOfEntriesBetweenExcludesDoomed) {
  CreateAndInitStore();

  const CacheEntryKey kKey1("key1");
  const std::string kData1 = "apple";
  const CacheEntryKey kKey2("key2");
  const std::string kData2 = "orange";

  const int64_t size2 =
      kSqlBackendStaticResourceSize + kKey2.string().size() + kData2.size();

  // Create entry 1.
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time time1 = base::Time::Now();
  const auto token1 = CreateEntryAndGetToken(kKey1);
  WriteDataAndAssertSuccess(kKey1, token1, 0, 0, kData1, /*truncate=*/false);

  // Create entry 2.
  task_environment_.AdvanceClock(base::Minutes(1));
  const auto token2 = CreateEntryAndGetToken(kKey2);
  WriteDataAndAssertSuccess(kKey2, token2, 0, 0, kData2, /*truncate=*/false);

  // Doom entry 1.
  ASSERT_EQ(DoomEntry(kKey1, token1), SqlPersistentStore::Error::kOk);

  // Calculate size of all entries. Should only include entry 2.
  auto result =
      CalculateSizeOfEntriesBetween(base::Time::Min(), base::Time::Max());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), size2);

  // Calculate the size of the range, including the doomed entry, but the result
  // should not include the doomed entry.
  result = CalculateSizeOfEntriesBetween(time1, base::Time::Max());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), size2);
}

TEST_F(SqlPersistentStoreTest,
       GetEntryAvailableRangeCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto token = CreateEntryAndGetToken(kKey);
  bool callback_run = false;
  store_->GetEntryAvailableRange(
      token, 0, 100, base::BindLambdaForTesting([&](const RangeResult&) {
        callback_run = true;
      }));

  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest,
       CalculateSizeOfEntriesBetweenCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  bool callback_run = false;

  store_->CalculateSizeOfEntriesBetween(
      base::Time(), base::Time::Max(),
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::Int64OrError) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest,
       ShouldStartEvictionReturnsTrueWhenSizeExceedsHighWatermark) {
  // Use a small max_bytes to make it easy to cross the high watermark.
  const int64_t kMaxBytes = 10000;
  const int64_t kHighWatermark =
      kMaxBytes - kMaxBytes / kSqlBackendEvictionMarginDivisor;  // 9500
  CreateStore(kMaxBytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  EXPECT_FALSE(store_->ShouldStartEviction());

  // Add entries until the size is just over the high watermark.
  int i = 0;
  while (GetSizeOfAllEntries() <= kHighWatermark) {
    const CacheEntryKey key(base::StringPrintf("key%d", i++));
    auto create_result = CreateEntry(key);
    ASSERT_TRUE(create_result.has_value());
    // Before the size exceeds the watermark, ShouldStartEviction should be
    // false.
    if (GetSizeOfAllEntries() <= kHighWatermark) {
      EXPECT_FALSE(store_->ShouldStartEviction());
    }
  }

  // The last CreateEntry() pushed the size over the high watermark.
  EXPECT_TRUE(store_->ShouldStartEviction());
}

TEST_F(SqlPersistentStoreTest, StartEvictionReducesSizeToLowWatermark) {
  const int64_t kMaxBytes = 10000;
  const int64_t kHighWatermark =
      kMaxBytes - kMaxBytes / kSqlBackendEvictionMarginDivisor;  // 9500
  const int64_t kLowWatermark =
      kMaxBytes - 2 * (kMaxBytes / kSqlBackendEvictionMarginDivisor);  // 9000

  CreateStore(kMaxBytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  // Add entries until size > high watermark.
  std::vector<CacheEntryKey> keys;
  int i = 0;
  while (GetSizeOfAllEntries() <= kHighWatermark) {
    const CacheEntryKey key(base::StringPrintf("key%d", i++));
    keys.push_back(key);
    auto create_result = CreateEntry(key);
    ASSERT_TRUE(create_result.has_value());
    task_environment_.AdvanceClock(
        base::Seconds(1));  // To distinguish last_used
  }

  const int64_t size_before_eviction = GetSizeOfAllEntries();
  const int32_t count_before_eviction = GetEntryCount();
  EXPECT_GT(size_before_eviction, kHighWatermark);
  EXPECT_TRUE(store_->ShouldStartEviction());

  // Start eviction.
  base::test::TestFuture<SqlPersistentStore::Error> future;
  store_->StartEviction({}, future.GetCallback());
  ASSERT_EQ(future.Get(), SqlPersistentStore::Error::kOk);

  // After eviction, size should be <= low watermark.
  const int64_t size_after_eviction = GetSizeOfAllEntries();
  const int32_t count_after_eviction = GetEntryCount();
  EXPECT_LE(size_after_eviction, kLowWatermark);
  EXPECT_LT(count_after_eviction, count_before_eviction);

  // Verify oldest entries are gone.
  int evicted_count = count_before_eviction - count_after_eviction;
  for (int j = 0; j < evicted_count; ++j) {
    auto result = OpenEntry(keys[j]);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
  }

  // Verify newest entries are still there.
  for (size_t j = evicted_count; j < keys.size(); ++j) {
    auto result = OpenEntry(keys[j]);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->has_value());
  }

  EXPECT_FALSE(store_->ShouldStartEviction());
}

TEST_F(SqlPersistentStoreTest, StartEvictionExcludesGivenKeys) {
  const int64_t kMaxBytes = 10000;
  const int64_t kHighWatermark =
      kMaxBytes - kMaxBytes / kSqlBackendEvictionMarginDivisor;  // 9500
  const int64_t kLowWatermark =
      kMaxBytes - 2 * (kMaxBytes / kSqlBackendEvictionMarginDivisor);  // 9000

  CreateStore(kMaxBytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  // Add entries until size > high watermark.
  std::vector<CacheEntryKey> keys;
  int i = 0;
  while (GetSizeOfAllEntries() <= kHighWatermark) {
    const CacheEntryKey key(base::StringPrintf("key%d", i++));
    keys.push_back(key);
    auto create_result = CreateEntry(key);
    ASSERT_TRUE(create_result.has_value());
    task_environment_.AdvanceClock(
        base::Seconds(1));  // To distinguish last_used
  }

  const int64_t size_before_eviction = GetSizeOfAllEntries();
  const int32_t count_before_eviction = GetEntryCount();
  EXPECT_GT(size_before_eviction, kHighWatermark);
  EXPECT_TRUE(store_->ShouldStartEviction());

  // Exclude the oldest entry.
  base::flat_set<CacheEntryKey> excluded_keys = {keys[0]};

  // Start eviction.
  base::test::TestFuture<SqlPersistentStore::Error> future;
  store_->StartEviction(std::move(excluded_keys), future.GetCallback());
  ASSERT_EQ(future.Get(), SqlPersistentStore::Error::kOk);

  // After eviction, size should be <= low watermark.
  const int64_t size_after_eviction = GetSizeOfAllEntries();
  const int32_t count_after_eviction = GetEntryCount();
  EXPECT_LE(size_after_eviction, kLowWatermark);
  EXPECT_LT(count_after_eviction, count_before_eviction);

  // Verify the excluded entry is still there.
  auto result = OpenEntry(keys[0]);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->has_value());

  // Verify some other old entries are gone.
  // The number of evicted entries will be different now.
  int evicted_count = count_before_eviction - count_after_eviction;
  // keys[0] was not evicted. So keys[1]...keys[evicted_count] should be
  // evicted.
  for (int j = 1; j <= evicted_count; ++j) {
    result = OpenEntry(keys[j]);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
  }

  EXPECT_FALSE(store_->ShouldStartEviction());
}

TEST_F(SqlPersistentStoreTest, ShouldStartEvictionReturnsFalseWhileInProgress) {
  const int64_t kMaxBytes = 10000;
  const int64_t kHighWatermark =
      kMaxBytes - kMaxBytes / kSqlBackendEvictionMarginDivisor;  // 9500

  CreateStore(kMaxBytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  // Add entries until size > high watermark.
  int i = 0;
  while (GetSizeOfAllEntries() <= kHighWatermark) {
    const CacheEntryKey key(base::StringPrintf("key%d", i++));
    auto create_result = CreateEntry(key);
    ASSERT_TRUE(create_result.has_value());
  }

  EXPECT_TRUE(store_->ShouldStartEviction());

  base::test::TestFuture<SqlPersistentStore::Error> future;
  store_->StartEviction({}, future.GetCallback());

  // While eviction is in progress, ShouldStartEviction should return false.
  EXPECT_FALSE(store_->ShouldStartEviction());

  // Let eviction finish.
  ASSERT_EQ(future.Get(), SqlPersistentStore::Error::kOk);

  // After eviction, size is below watermark, so it should still be false.
  EXPECT_FALSE(store_->ShouldStartEviction());
}

}  // namespace disk_cache
