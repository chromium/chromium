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
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "components/performance_manager/scenario_api/performance_scenario_test_support.h"
#include "net/base/cache_type.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/simple/simple_util.h"
#include "net/disk_cache/sql/cache_entry_key.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using performance_scenarios::InputScenario;
using performance_scenarios::LoadingScenario;
using performance_scenarios::PerformanceScenarioTestHelper;
using performance_scenarios::ScenarioScope;
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
    background_task_runners_.emplace_back(
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
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
    return GetTempPath().Append(kSqlBackendDatabaseShard0FileName);
  }

  // Creates a SqlPersistentStore instance.
  void CreateStore(int64_t max_bytes = kDefaultMaxBytes) {
    store_ = std::make_unique<SqlPersistentStore>(
        GetTempPath(), max_bytes, net::CacheType::DISK_CACHE,
        std::vector<scoped_refptr<base::SequencedTaskRunner>>(
            background_task_runners_));
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

  bool LoadInMemoryIndex(SqlPersistentStore::Error expected_result =
                             SqlPersistentStore::Error::kOk) {
    CHECK(store_);
    base::test::TestFuture<SqlPersistentStore::Error> future;
    auto ret = store_->MaybeLoadInMemoryIndex(future.GetCallback());
    if (ret) {
      CHECK_EQ(future.Get(), expected_result);
      return true;
    }
    return false;
  }

  // Gets the entry count.
  int32_t GetEntryCount() { return store_->GetEntryCount(); }

  // Gets the total size of all entries.
  int64_t GetSizeOfAllEntries() { return store_->GetSizeOfAllEntries(); }

  // Ensures all tasks on the background thread have completed.
  void FlushPendingTask() {
    for (auto background_task_runner : background_task_runners_) {
      base::RunLoop run_loop;
      background_task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();
    }
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
    store_->CreateEntry(key, base::Time::Now(), future.GetCallback());
    return future.Take();
  }

  // Helper to create an entry and return its ResId, asserting success.
  SqlPersistentStore::ResId CreateEntryAndGetResId(const CacheEntryKey& key) {
    auto create_result = CreateEntry(key);
    CHECK(create_result.has_value())
        << "Failed to create entry for key: " << key.string();
    return create_result->res_id;
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
                                      SqlPersistentStore::ResId res_id) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->DoomEntry(key, res_id, future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for DeleteDoomedEntry.
  SqlPersistentStore::Error DeleteDoomedEntry(
      const CacheEntryKey& key,
      SqlPersistentStore::ResId res_id) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->DeleteDoomedEntry(key, res_id, future.GetCallback());
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

  // Synchronous wrapper for OpenNextEntry.
  SqlPersistentStore::OptionalEntryInfoWithKeyAndIterator OpenNextEntry(
      const SqlPersistentStore::EntryIterator& entry_coursor) {
    base::test::TestFuture<
        SqlPersistentStore::OptionalEntryInfoWithKeyAndIterator>
        future;
    store_->OpenNextEntry(entry_coursor, future.GetCallback());
    return future.Take();
  }

  // Synchronous wrapper for DeleteLiveEntriesBetween.
  SqlPersistentStore::Error DeleteLiveEntriesBetween(
      base::Time initial_time,
      base::Time end_time,
      std::vector<SqlPersistentStore::ResIdAndShardId> excluded_list = {}) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->DeleteLiveEntriesBetween(
        initial_time, end_time, std::move(excluded_list), future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for UpdateEntryLastUsedByKey.
  SqlPersistentStore::Error UpdateEntryLastUsedByKey(const CacheEntryKey& key,
                                                     base::Time last_used) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->UpdateEntryLastUsedByKey(key, last_used, future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for UpdateEntryLastUsedByResId.
  SqlPersistentStore::Error UpdateEntryLastUsedByResId(
      const CacheEntryKey& key,
      SqlPersistentStore::ResId res_id,
      base::Time last_used) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->UpdateEntryLastUsedByResId(key, res_id, last_used,
                                       future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for UpdateEntryHeaderAndLastUsed.
  SqlPersistentStore::Error UpdateEntryHeaderAndLastUsed(
      const CacheEntryKey& key,
      SqlPersistentStore::ResId res_id,
      base::Time last_used,
      scoped_refptr<net::IOBuffer> buffer,
      int64_t header_size_delta) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->UpdateEntryHeaderAndLastUsed(
        key, res_id, last_used, /*new_hints=*/std::nullopt, std::move(buffer),
        header_size_delta, future.GetCallback());
    return future.Get();
  }

  SqlPersistentStore::Error UpdateEntryHeaderAndLastUsed(
      const CacheEntryKey& key,
      SqlPersistentStore::ResId res_id,
      base::Time last_used,
      scoped_refptr<net::IOBuffer> buffer,
      int64_t header_size_delta,
      const std::optional<MemoryEntryDataHints>& new_hints) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->UpdateEntryHeaderAndLastUsed(key, res_id, last_used, new_hints,
                                         std::move(buffer), header_size_delta,
                                         future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for WriteEntryData.
  SqlPersistentStore::Error WriteEntryData(const CacheEntryKey& key,
                                           SqlPersistentStore::ResId res_id,
                                           int64_t old_body_end,
                                           int64_t offset,
                                           scoped_refptr<net::IOBuffer> buffer,
                                           int buf_len,
                                           bool truncate) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->WriteEntryData(key, res_id, old_body_end, offset, std::move(buffer),
                           buf_len, truncate, future.GetCallback());
    return future.Get();
  }

  // Synchronous wrapper for ReadEntryData.
  SqlPersistentStore::IntOrError ReadEntryData(
      const CacheEntryKey& key,
      SqlPersistentStore::ResId res_id,
      int64_t offset,
      scoped_refptr<net::IOBuffer> buffer,
      int buf_len,
      int64_t body_end,
      bool sparse_reading) {
    base::test::TestFuture<SqlPersistentStore::IntOrError> future;
    store_->ReadEntryData(key, res_id, offset, std::move(buffer), buf_len,
                          body_end, sparse_reading, future.GetCallback());
    return future.Take();
  }

  // Helper to write data from a string_view and assert success.
  void WriteDataAndAssertSuccess(const CacheEntryKey& key,
                                 SqlPersistentStore::ResId res_id,
                                 int64_t old_body_end,
                                 int64_t offset,
                                 std::string_view data,
                                 bool truncate) {
    auto buffer = base::MakeRefCounted<net::StringIOBuffer>(std::string(data));
    ASSERT_EQ(WriteEntryData(key, res_id, old_body_end, offset,
                             std::move(buffer), data.size(), truncate),
              SqlPersistentStore::Error::kOk);
  }

  // Helper to fill a range with a repeated character and write it to the store.
  void FillDataInRange(const CacheEntryKey& key,
                       SqlPersistentStore::ResId res_id,
                       int64_t old_body_end,
                       int64_t start,
                       int64_t len,
                       char fill_char) {
    const std::string data(len, fill_char);
    WriteDataAndAssertSuccess(key, res_id, old_body_end, start, data,
                              /*truncate=*/false);
  }

  // Helper to write a sequence of single-byte blobs from a string_view and
  // verify the result.
  void WriteAndVerifySingleByteBlobs(const CacheEntryKey& key,
                                     SqlPersistentStore::ResId res_id,
                                     std::string_view content) {
    for (size_t i = 0; i < content.size(); ++i) {
      std::string data(1, content[i]);
      WriteDataAndAssertSuccess(key, res_id, i, i, data,
                                /*truncate=*/false);
    }
    ReadAndVerifyData(key, res_id, 0, content.size(), content.size(), false,
                      std::string(content));

    std::vector<BlobData> actual_blobs = GetAllBlobData(res_id);
    for (size_t i = 0; i < content.size(); ++i) {
      EXPECT_EQ(actual_blobs[i].start, i);
      EXPECT_EQ(actual_blobs[i].end, i + 1);
      ASSERT_THAT(actual_blobs[i].data, ElementsAre(content[i]));
    }
  }

  // Synchronous wrapper for GetEntryAvailableRange.
  RangeResult GetEntryAvailableRange(const CacheEntryKey& key,
                                     SqlPersistentStore::ResId res_id,
                                     int64_t offset,
                                     int len) {
    base::test::TestFuture<const RangeResult&> future;
    store_->GetEntryAvailableRange(key, res_id, offset, len,
                                   future.GetCallback());
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

  // Synchronous wrapper for StartEviction.
  SqlPersistentStore::Error StartEviction(
      std::vector<SqlPersistentStore::ResIdAndShardId> excluded_list,
      bool is_idle_time_eviction) {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->StartEviction(std::move(excluded_list), is_idle_time_eviction,
                          future.GetCallback());
    return future.Take();
  }

  // Helper to read data and verify its content.
  void ReadAndVerifyData(const CacheEntryKey& key,
                         SqlPersistentStore::ResId res_id,
                         int64_t offset,
                         int buffer_len,
                         int64_t body_end,
                         bool sparse_reading,
                         std::string_view expected_data) {
    auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(buffer_len);
    auto read_result = ReadEntryData(key, res_id, offset, read_buffer,
                                     buffer_len, body_end, sparse_reading);
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

  // Helper to read hints from the resources table.
  std::optional<uint8_t> GetResourceHints(const CacheEntryKey& key) {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement s(db_handle->GetUniqueStatement(
        "SELECT hints FROM resources WHERE cache_key=?"));
    s.BindString(0, key.string());
    if (s.Step()) {
      return static_cast<uint8_t>(s.ColumnInt(0));
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

  // Helper to retrieve all blob data for a given entry res_id.
  std::vector<BlobData> GetAllBlobData(SqlPersistentStore::ResId res_id) {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement s(db_handle->GetUniqueStatement(
        "SELECT start, end, blob FROM blobs WHERE res_id=? "
        "ORDER BY start"));
    s.BindInt64(0, res_id.value());

    std::vector<BlobData> blobs;
    while (s.Step()) {
      blobs.emplace_back(
          s.ColumnInt64(0), s.ColumnInt64(1),
          std::vector<uint8_t>(s.ColumnBlob(2).begin(), s.ColumnBlob(2).end()));
    }
    return blobs;
  }

  // Helper to check the blob data for a given res_id.
  void CheckBlobData(SqlPersistentStore::ResId res_id,
                     std::initializer_list<std::pair<int64_t, std::string_view>>
                         expected_blobs) {
    std::vector<BlobData> expected;
    for (const auto& blob_pair : expected_blobs) {
      expected.push_back(MakeBlobData(blob_pair.first, blob_pair.second));
    }
    EXPECT_THAT(GetAllBlobData(res_id), testing::ElementsAreArray(expected));
  }

  // Helper to overwrite the blob data for a given entry_key and res_id.
  void OverwriteBlobData(const CacheEntryKey& entry_key,
                         SqlPersistentStore::ResId res_id,
                         std::string_view new_data,
                         int32_t new_check_sum) {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE blobs SET check_sum = ?, blob = ? WHERE res_id = ?"));
    statement.BindInt(0, new_check_sum);
    statement.BindBlob(1, base::as_byte_span(new_data));
    statement.BindInt64(2, res_id.value());
    ASSERT_TRUE(statement.Run());
  }

  // Helper to corrupt the start and end offsets for a given res_id.
  void CorruptBlobRange(SqlPersistentStore::ResId res_id,
                        int64_t new_start,
                        int64_t new_end) {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE blobs SET start = ?, end = ? WHERE res_id = ?"));
    statement.BindInt64(0, new_start);
    statement.BindInt64(1, new_end);
    statement.BindInt64(2, res_id.value());
    ASSERT_TRUE(statement.Run());
  }

  int64_t GetResourceCheckSum(SqlPersistentStore::ResId res_id) {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "SELECT check_sum FROM resources WHERE res_id = ?"));
    statement.BindInt64(0, res_id.value());
    CHECK(statement.Step());
    return statement.ColumnInt(0);
  }

  // Returns the number of writes required for a checkpoint.
  int GetNumberForWritesRequiredForCheckpoint(const CacheEntryKey& entry_key,
                                              std::string_view data);

  static int32_t CalculateCheckSum(base::span<const uint8_t> data,
                                   CacheEntryKey::Hash key_hash) {
    uint32_t hash_value_net_order =
        base::HostToNet32(static_cast<uint32_t>(key_hash.value()));
    uint32_t crc32_value = simple_util::IncrementalCrc32(
        simple_util::Crc32(data),
        base::byte_span_from_ref(hash_value_net_order));
    return static_cast<int32_t>(crc32_value);
  }

  void MaybeRunCheckpoint(bool expected_result) {
    base::test::TestFuture<bool> future;
    base::HistogramTester histogram_tester;
    store_->MaybeRunCheckpoint(future.GetCallback());
    EXPECT_EQ(future.Get(), expected_result);
    histogram_tester.ExpectTotalCount(
        "Net.SqlDiskCache.Backend.IdleEventCheckpoint.SuccessTime",
        expected_result ? 1 : 0);
    histogram_tester.ExpectTotalCount(
        "Net.SqlDiskCache.Backend.IdleEventCheckpoint.SuccessPages",
        expected_result ? 1 : 0);
  }

  void RunCleanupDoomedEntriesTest(base::OnceClosure trigger_cleanup);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::vector<scoped_refptr<base::SequencedTaskRunner>>
      background_task_runners_;
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

  store_ = std::make_unique<SqlPersistentStore>(
      db_dir_path, kDefaultMaxBytes, net::CacheType::DISK_CACHE,
      std::vector<scoped_refptr<base::SequencedTaskRunner>>(
          background_task_runners_));
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
  const auto res_id = CreateEntryAndGetResId(kKey);
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
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

  const auto created_res_id = CreateEntryAndGetResId(kKey);

  auto open_result = OpenEntry(kKey);
  ASSERT_TRUE(open_result.has_value());
  ASSERT_TRUE(open_result->has_value());
  EXPECT_EQ((*open_result)->res_id, created_res_id);
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
  EXPECT_FALSE(result->opened);  // Should be like a created entry.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());
}

TEST_F(SqlPersistentStoreTest, OpenOrCreateEntryOpensExisting) {
  CreateAndInitStore();
  const CacheEntryKey kKey("existing-key");

  // Create an entry first.
  const auto created_res_id = CreateEntryAndGetResId(kKey);

  // Now, open it with OpenOrCreateEntry.
  auto open_result = OpenOrCreateEntry(kKey);
  ASSERT_TRUE(open_result.has_value());
  EXPECT_EQ(open_result->res_id, created_res_id);
  EXPECT_TRUE(open_result->opened);  // Should be like an opened entry.

  // Stats should not have changed from the initial creation.
  EXPECT_EQ(GetEntryCount(), 1);
  EXPECT_EQ(GetSizeOfAllEntries(),
            kSqlBackendStaticResourceSize + kKey.string().size());
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
  const auto res_id_to_doom = CreateEntryAndGetResId(kKeyToDoom);
  const auto res_id_to_keep = CreateEntryAndGetResId(kKeyToKeep);
  ASSERT_EQ(GetEntryCount(), 2);
  ASSERT_EQ(GetSizeOfAllEntries(), size_to_doom + size_to_keep);

  // Doom one of the entries.
  ASSERT_EQ(DoomEntry(kKeyToDoom, res_id_to_doom),
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
  EXPECT_EQ((*open_kept_result)->res_id, res_id_to_keep);

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
  auto result = DoomEntry(kKey, SqlPersistentStore::ResId(123));
  ASSERT_EQ(result, SqlPersistentStore::Error::kNotFound);

  // Verify that the counts remain unchanged.
  EXPECT_EQ(GetEntryCount(), 0);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);
}

TEST_F(SqlPersistentStoreTest, DoomEntryFailsWrongResId) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");
  const int64_t size1 = kSqlBackendStaticResourceSize + kKey1.string().size();
  const int64_t size2 = kSqlBackendStaticResourceSize + kKey2.string().size();

  // Create two entries.
  const auto res_id1 = CreateEntryAndGetResId(kKey1);
  const auto res_id2 = CreateEntryAndGetResId(kKey2);
  ASSERT_EQ(GetEntryCount(), 2);

  // Attempt to doom key1 with an incorrect res_id.
  ASSERT_EQ(DoomEntry(kKey1, SqlPersistentStore::ResId(res_id2.value() + 1)),
            SqlPersistentStore::Error::kNotFound);

  // Verify that the counts remain unchanged and both entries can still be
  // opened.
  EXPECT_EQ(GetEntryCount(), 2);
  EXPECT_EQ(GetSizeOfAllEntries(), size1 + size2);

  auto open_result1 = OpenEntry(kKey1);
  ASSERT_TRUE(open_result1.has_value());
  ASSERT_TRUE(open_result1->has_value());
  EXPECT_EQ((*open_result1)->res_id, res_id1);

  auto open_result2 = OpenEntry(kKey2);
  ASSERT_TRUE(open_result2.has_value());
  ASSERT_TRUE(open_result2->has_value());
  EXPECT_EQ((*open_result2)->res_id, res_id2);
}

TEST_F(SqlPersistentStoreTest, DoomEntryWithCorruptSizeRecovers) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToCorrupt("key-to-corrupt");
  const CacheEntryKey kKeyToKeep("key-to-keep");
  const int64_t keep_key_size = kKeyToKeep.string().size();
  const int64_t expected_size_after_recovery =
      kSqlBackendStaticResourceSize + keep_key_size;

  // Create one entry to keep, and one to corrupt and doom.
  const auto res_id_to_doom = CreateEntryAndGetResId(kKeyToCorrupt);
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
  ASSERT_EQ(DoomEntry(kKeyToCorrupt, res_id_to_doom),
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
  const auto res_id = CreateEntryAndGetResId(kKey);
  ASSERT_EQ(DoomEntry(kKey, res_id), SqlPersistentStore::Error::kOk);
  ASSERT_EQ(GetEntryCount(), 0);
  ASSERT_EQ(CountResourcesTable(), 1);

  // Delete the doomed entry.
  ASSERT_EQ(DeleteDoomedEntry(kKey, res_id), SqlPersistentStore::Error::kOk);

  // Verify the entry is now physically gone from the database.
  EXPECT_EQ(CountResourcesTable(), 0);
}

TEST_F(SqlPersistentStoreTest, DeleteDoomedEntryDeletesBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  const std::string kData = "some data";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kData, /*truncate=*/false);
  CheckBlobData(res_id, {{0, kData}});
  EXPECT_EQ(GetSizeOfAllEntries(), kSqlBackendStaticResourceSize +
                                       kKey.string().size() + kData.size());

  // DoomEntry is responsible for updating the total size of the cache.
  ASSERT_EQ(DoomEntry(kKey, res_id), SqlPersistentStore::Error::kOk);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);

  ASSERT_EQ(DeleteDoomedEntry(kKey, res_id), SqlPersistentStore::Error::kOk);
  CheckBlobData(res_id, {});
}

TEST_F(SqlPersistentStoreTest, DeleteDoomedEntryFailsOnLiveEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  // Create a live entry.
  const auto res_id = CreateEntryAndGetResId(kKey);
  ASSERT_EQ(GetEntryCount(), 1);

  // Attempt to delete it with DeleteDoomedEntry. This should fail because the
  // entry is not marked as doomed.
  auto result = DeleteDoomedEntry(kKey, res_id);
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
  const auto res_id_to_keep = CreateEntryAndGetResId(kKeyToKeep);
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
  EXPECT_EQ((*open_kept_result)->res_id, res_id_to_keep);

  // Verify the entry is physically gone from the database.
  EXPECT_EQ(CountResourcesTable(), 1);
}

TEST_F(SqlPersistentStoreTest, DeleteLiveEntryDeletesBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  const std::string kData = "some data";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kData, /*truncate=*/false);
  CheckBlobData(res_id, {{0, kData}});
  ASSERT_EQ(DeleteLiveEntry(kKey), SqlPersistentStore::Error::kOk);
  CheckBlobData(res_id, {});
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
  const auto doomed_res_id = CreateEntryAndGetResId(kDoomedKey);
  ASSERT_TRUE(CreateEntry(kLiveKey).has_value());

  // Doom one of the entries.
  ASSERT_EQ(DoomEntry(kDoomedKey, doomed_res_id),
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

TEST_F(SqlPersistentStoreTest, DeleteLiveEntryNonExistentWithIndex) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  CreateEntryAndGetResId(kKey);

  // Load the index.
  ASSERT_TRUE(LoadInMemoryIndex());

  const CacheEntryKey kNonExistentKey("non-existent-key");
  // With the index loaded, this should synchronously return kNotFound without a
  // DB lookup.
  std::optional<SqlPersistentStore::Error> error;
  store_->DeleteLiveEntry(
      kNonExistentKey,
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::Error result) { error = result; }));
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(*error, SqlPersistentStore::Error::kNotFound);
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
  const auto res_id1 = CreateEntryAndGetResId(kKey1);
  const std::string kData1 = "data1";
  WriteDataAndAssertSuccess(kKey1, res_id1, 0, 0, kData1, /*truncate=*/false);
  const CacheEntryKey kKey2("key2");
  const auto res_id2 = CreateEntryAndGetResId(kKey2);
  const std::string kData2 = "data2";
  WriteDataAndAssertSuccess(kKey2, res_id2, 0, 0, kData2, /*truncate=*/false);
  CheckBlobData(res_id1, {{0, kData1}});
  CheckBlobData(res_id2, {{0, kData2}});
  ASSERT_EQ(DeleteAllEntries(), SqlPersistentStore::Error::kOk);
  CheckBlobData(res_id1, {});
  CheckBlobData(res_id2, {});
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

void SqlPersistentStoreTest::RunCleanupDoomedEntriesTest(
    base::OnceClosure trigger_cleanup) {
  CreateAndInitStore();
  const CacheEntryKey kKeyToDoom1("key-to-doom1");
  const CacheEntryKey kKeyToDoom2("key-to-doom2");
  const CacheEntryKey kKeyToDoomActive("key-to-doom-active");
  const CacheEntryKey kKeyToKeep("key-to-keep");

  // Create entries that will be doomed.
  auto create_result1 = CreateEntry(kKeyToDoom1);
  ASSERT_TRUE(create_result1.has_value());
  const auto res_id_to_doom1 = create_result1->res_id;

  // Create entries that will be doomed.
  auto create_result2 = CreateEntry(kKeyToDoom2);
  ASSERT_TRUE(create_result2.has_value());
  const auto res_id_to_doom2 = create_result2->res_id;

  // Create an entry that will be kept.
  auto create_result3 = CreateEntry(kKeyToKeep);
  ASSERT_TRUE(create_result3.has_value());
  const auto res_id_to_keep = create_result3->res_id;

  // There should be 3 created entries.
  ASSERT_EQ(GetEntryCount(), 3);

  // Write data to the entries that will be doomed.
  const std::string kData1 = "doomed_data1";
  WriteDataAndAssertSuccess(kKeyToDoom1, res_id_to_doom1, /*old_body_end=*/0,
                            /*offset=*/0, kData1, /*truncate=*/false);
  const std::string kData2 = "doomed_data2";
  WriteDataAndAssertSuccess(kKeyToDoom2, res_id_to_doom2, /*old_body_end=*/0,
                            /*offset=*/0, kData2, /*truncate=*/false);
  const std::string kData3 = "keep-data";
  WriteDataAndAssertSuccess(kKeyToKeep, res_id_to_keep, /*old_body_end=*/0,
                            /*offset=*/0, kData3, /*truncate=*/false);
  // Doom all the entries that will be doomed.
  ASSERT_EQ(DoomEntry(kKeyToDoom1, res_id_to_doom1),
            SqlPersistentStore::Error::kOk);
  ASSERT_EQ(DoomEntry(kKeyToDoom2, res_id_to_doom2),
            SqlPersistentStore::Error::kOk);
  // The entry count after dooming 2 entries should be 1.
  ASSERT_EQ(GetEntryCount(), 1);

  // All resource blobs should be still available.
  EXPECT_EQ(CountResourcesTable(), 3);
  CheckBlobData(res_id_to_doom1, {{0, kData1}});
  CheckBlobData(res_id_to_doom2, {{0, kData2}});
  CheckBlobData(res_id_to_keep, {{0, kData3}});

  // Reload the store and the doomed entries will be marked for deletion.
  ClearStore();
  CreateAndInitStore();

  base::HistogramTester histogram_tester;
  std::move(trigger_cleanup).Run();
  // Verify that `DeleteDoomedEntriesCount` UMA was recorded in the histogram.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.DeleteDoomedEntriesCount", 2, 1);

  // Verify the entries for kKeyToDoom1 and kKeyToDoom2 are physically gone from
  // the database.
  EXPECT_EQ(CountResourcesTable(), 1);
  CheckBlobData(res_id_to_doom1, {});
  CheckBlobData(res_id_to_doom2, {});
  CheckBlobData(res_id_to_keep, {{0, kData3}});

  // Verify the live entry is still present.
  auto open_result1 = OpenEntry(kKeyToKeep);
  ASSERT_TRUE(open_result1.has_value());
  ASSERT_TRUE(open_result1->has_value());
  EXPECT_EQ(open_result1.value()->res_id, res_id_to_keep);
}

TEST_F(SqlPersistentStoreTest,
       MaybeRunCleanupDoomedEntriesAfterLoadInMemoryIndex) {
  RunCleanupDoomedEntriesTest(base::BindLambdaForTesting([&]() {
    // Load the in-memory index to get the list of doomed entry.
    EXPECT_TRUE(this->LoadInMemoryIndex());

    base::test::TestFuture<SqlPersistentStore::Error> future;
    EXPECT_TRUE(
        this->store_->MaybeRunCleanupDoomedEntries(future.GetCallback()));
    EXPECT_EQ(future.Get(), SqlPersistentStore::Error::kOk);
  }));
}

TEST_F(SqlPersistentStoreTest,
       MaybeRunCleanupDoomedEntriesWithLoadIndexOnInitFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{net::features::kDiskCacheBackendExperiment,
        {{net::features::kDiskCacheBackendParam.name, "sql"},
         {net::features::kSqlDiskCacheLoadIndexOnInit.name, "true"}}}},
      {});

  RunCleanupDoomedEntriesTest(base::BindLambdaForTesting([&]() {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    EXPECT_TRUE(
        this->store_->MaybeRunCleanupDoomedEntries(future.GetCallback()));
    EXPECT_EQ(future.Get(), SqlPersistentStore::Error::kOk);
  }));
}

TEST_F(SqlPersistentStoreTest, MaybeRunCleanupDoomedEntriesMultipleShards) {
  // Add more task runners to have more shards.
  background_task_runners_.emplace_back(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  background_task_runners_.emplace_back(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  EXPECT_EQ(background_task_runners_.size(), 3);

  CreateAndInitStore();

  const CacheEntryKey kKeyToDoom1("key-to-doom1");
  const auto shared1 = store_->GetShardIdForHash(kKeyToDoom1.hash());

  // Find a key that belongs to a different shard.
  CacheEntryKey key_to_doom_2;
  for (int key_prefix = 2;; key_prefix++) {
    key_to_doom_2 = CacheEntryKey(base::NumberToString(key_prefix));
    if (store_->GetShardIdForHash(key_to_doom_2.hash()) != shared1) {
      break;
    }
  }

  // Create and doom the first entry.
  auto create_result1 = CreateEntry(kKeyToDoom1);
  ASSERT_TRUE(create_result1.has_value());
  const auto res_id_to_doom1 = create_result1->res_id;
  ASSERT_EQ(DoomEntry(kKeyToDoom1, res_id_to_doom1),
            SqlPersistentStore::Error::kOk);

  // Create and doom the second entry.
  auto create_result2 = CreateEntry(key_to_doom_2);
  ASSERT_TRUE(create_result2.has_value());
  const auto res_id_to_doom2 = create_result2->res_id;
  ASSERT_EQ(DoomEntry(key_to_doom_2, res_id_to_doom2),
            SqlPersistentStore::Error::kOk);

  // Reload the store and the doomed entries will be marked for deletion.
  ClearStore();
  CreateAndInitStore();

  // Load the in-memory index to get the list of doomed entry.
  EXPECT_TRUE(LoadInMemoryIndex());

  // Cleanup the doomed eintries.
  base::test::TestFuture<SqlPersistentStore::Error> future;
  EXPECT_TRUE(store_->MaybeRunCleanupDoomedEntries(future.GetCallback()));
  EXPECT_EQ(future.Get(), SqlPersistentStore::Error::kOk);
}

TEST_F(SqlPersistentStoreTest, MaybeRunCleanupDoomedEntriesNoDeletion) {
  CreateAndInitStore();

  // Scenario 1: No entries exist.
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(store_->MaybeRunCleanupDoomedEntries(
      base::BindOnce([](SqlPersistentStore::Error) { NOTREACHED(); })));

  EXPECT_EQ(CountResourcesTable(), 0);

  // Scenario 2: All entries are not doomed.
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");
  auto create_result1 = CreateEntry(kKey1);
  ASSERT_TRUE(create_result1.has_value());
  auto create_result2 = CreateEntry(kKey2);
  ASSERT_TRUE(create_result2.has_value());
  ASSERT_EQ(CountResourcesTable(), 2);

  EXPECT_FALSE(store_->MaybeRunCleanupDoomedEntries(
      base::BindOnce([](SqlPersistentStore::Error) { NOTREACHED(); })));

  // Verify that no entries were deleted.
  EXPECT_EQ(CountResourcesTable(), 2);
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

// Regression test for crbug.com/447751287.
TEST_F(SqlPersistentStoreTest, DeleteLiveEntriesBetweenOneEntry) {
  CreateAndInitStore();
  store_->EnableStrictCorruptionCheckForTesting();
  const base::Time kBaseTime = base::Time::Now();
  task_environment_.AdvanceClock(base::Minutes(1));
  const CacheEntryKey kKey("key");
  ASSERT_TRUE(CreateEntry(kKey).has_value());
  task_environment_.AdvanceClock(base::Minutes(1));
  ASSERT_EQ(DeleteLiveEntriesBetween(kBaseTime, base::Time::Now(), {}),
            SqlPersistentStore::Error::kOk);
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
  auto create_result = CreateEntry(kKey2);
  ASSERT_TRUE(create_result.has_value());
  SqlPersistentStore::ResId res_id2 = create_result->res_id;

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

  EXPECT_TRUE(LoadInMemoryIndex());
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey3.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey4.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey5.hash()),
            SqlPersistentStore::IndexState::kHashFound);

  // Delete entries between kTime1 (inclusive) and kTime3 (exclusive).
  // kKey2 should be excluded.
  // Expected to delete: kKey1.
  // Expected to keep: kKey2, kKey3, kKey4, kKey5.
  std::vector<SqlPersistentStore::ResIdAndShardId> excluded_list = {
      SqlPersistentStore::ResIdAndShardId(
          res_id2, store_->GetShardIdForHash(kKey2.hash()))};
  ASSERT_EQ(DeleteLiveEntriesBetween(kTime1, kTime3, excluded_list),
            SqlPersistentStore::Error::kOk);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey3.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey4.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey5.hash()),
            SqlPersistentStore::IndexState::kHashFound);

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
  const auto res_id1 = CreateEntryAndGetResId(kKey1);
  const std::string kData1 = "data1";
  WriteDataAndAssertSuccess(kKey1, res_id1, 0, 0, kData1, /*truncate=*/false);
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time time1 = base::Time::Now();
  const CacheEntryKey kKey2("key2");
  const auto res_id2 = CreateEntryAndGetResId(kKey2);
  const std::string kData2 = "data2";
  WriteDataAndAssertSuccess(kKey2, res_id2, 0, 0, kData2, /*truncate=*/false);
  CheckBlobData(res_id1, {{0, kData1}});
  CheckBlobData(res_id2, {{0, kData2}});
  ASSERT_EQ(DeleteLiveEntriesBetween(time1, base::Time::Max()),
            SqlPersistentStore::Error::kOk);
  CheckBlobData(res_id1, {{0, kData1}});  // Should not be deleted
  CheckBlobData(res_id2, {});             // Should be deleted
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

TEST_F(SqlPersistentStoreTest, UpdateEntryLastUsedByKeySuccess) {
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

  ASSERT_EQ(UpdateEntryLastUsedByKey(kKey, kNewTime),
            SqlPersistentStore::Error::kOk);

  // Setting the same time should succeed.
  ASSERT_EQ(UpdateEntryLastUsedByKey(kKey, kNewTime),
            SqlPersistentStore::Error::kOk);

  // Open again to verify the updated time.
  auto open_result2 = OpenEntry(kKey);
  ASSERT_TRUE(open_result2.has_value() && open_result2->has_value());
  EXPECT_EQ((*open_result2)->last_used, kNewTime);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryLastUsedByKeyOnNonExistentEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("non-existent-key");
  ASSERT_EQ(UpdateEntryLastUsedByKey(kKey, base::Time::Now()),
            SqlPersistentStore::Error::kNotFound);
  EXPECT_EQ(GetEntryCount(), 0);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryLastUsedByKeyOnDoomedEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("doomed-key");

  // Create and then doom the entry.
  const auto res_id = CreateEntryAndGetResId(kKey);
  ASSERT_EQ(DoomEntry(kKey, res_id), SqlPersistentStore::Error::kOk);

  // Attempting to update a doomed entry should fail as if it's not found.
  ASSERT_EQ(UpdateEntryLastUsedByKey(kKey, base::Time::Now()),
            SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryLastUsedByKeyNonExistentWithIndex) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  CreateEntryAndGetResId(kKey);

  // Load the index.
  ASSERT_TRUE(LoadInMemoryIndex());

  const CacheEntryKey kNonExistentKey("non-existent-key");
  // With the index loaded, this should synchronously return kNotFound without a
  // DB lookup.
  std::optional<SqlPersistentStore::Error> error;
  store_->UpdateEntryLastUsedByKey(
      kNonExistentKey, base::Time::Now(),
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::Error result) { error = result; }));
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(*error, SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryLastUsedByResIdSuccess) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());
  const base::Time create_time = create_result->last_used;
  const auto res_id = create_result->res_id;

  // Open to verify initial time.
  auto open_result1 = OpenEntry(kKey);
  ASSERT_TRUE(open_result1.has_value() && open_result1->has_value());
  EXPECT_EQ((*open_result1)->last_used, create_time);

  // Advance time and update.
  task_environment_.AdvanceClock(base::Minutes(5));
  const base::Time kNewTime = base::Time::Now();
  ASSERT_NE(kNewTime, create_time);

  ASSERT_EQ(UpdateEntryLastUsedByResId(kKey, res_id, kNewTime),
            SqlPersistentStore::Error::kOk);

  // Setting the same time should succeed.
  ASSERT_EQ(UpdateEntryLastUsedByResId(kKey, res_id, kNewTime),
            SqlPersistentStore::Error::kOk);

  // Open again to verify the updated time.
  auto open_result2 = OpenEntry(kKey);
  ASSERT_TRUE(open_result2.has_value() && open_result2->has_value());
  EXPECT_EQ((*open_result2)->last_used, kNewTime);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryLastUsedByResIdOnNonExistentEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("key");
  ASSERT_EQ(UpdateEntryLastUsedByResId(kKey, SqlPersistentStore::ResId(123),
                                       base::Time::Now()),
            SqlPersistentStore::Error::kNotFound);
  EXPECT_EQ(GetEntryCount(), 0);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryLastUsedByResIdOnDoomedEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("doomed-key");

  // Create and then doom the entry.
  const auto res_id = CreateEntryAndGetResId(kKey);
  ASSERT_EQ(DoomEntry(kKey, res_id), SqlPersistentStore::Error::kOk);

  // Attempting to update a doomed entry should fail as if it's not found.
  ASSERT_EQ(UpdateEntryLastUsedByResId(kKey, res_id, base::Time::Now()),
            SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryHeaderAndLastUsedSuccessInitial) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

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
      UpdateEntryHeaderAndLastUsed(kKey, res_id, new_last_used, buffer,
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
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Initial update with some header data.
  const std::string kInitialHeadData = "initial_data";
  auto initial_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kInitialHeadData);
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, res_id, base::Time::Now(),
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
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, res_id, new_last_used, new_buffer,
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
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Initial update with some header data.
  const std::string kInitialHeadData = "short";
  auto initial_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kInitialHeadData);
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, res_id, base::Time::Now(),
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
          kKey, res_id, base::Time::Now(), new_buffer,
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
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Initial update with large header data.
  const std::string kInitialHeadData = "much_longer_header_data";
  auto initial_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kInitialHeadData);
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, res_id, base::Time::Now(),
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
          kKey, res_id, base::Time::Now(), new_buffer,
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
      UpdateEntryHeaderAndLastUsed(kKey, SqlPersistentStore::ResId(100),
                                   base::Time::Now(), buffer, buffer->size()),
      SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest, UpdateEntryHeaderAndLastUsedDoomedEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("doomed-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  ASSERT_EQ(DoomEntry(kKey, res_id), SqlPersistentStore::Error::kOk);

  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  ASSERT_EQ(UpdateEntryHeaderAndLastUsed(kKey, res_id, base::Time::Now(),
                                         buffer, buffer->size()),
            SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest,
       UpdateEntryHeaderAndLastUsedCorruptionDetectedAndRolledBack) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  auto create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());
  const auto res_id = create_result->res_id;
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
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, res_id, base::Time::Now(), buffer,
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

TEST_F(SqlPersistentStoreTest, OpenEntryCheckSumError) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  EXPECT_EQ(GetResourceCheckSum(res_id), CalculateCheckSum({}, kKey.hash()));

  // Prepare header data.
  const std::string kHeadData = "header_data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kHeadData);

  // Update the entry. Previous header size is 0 as it was null.
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, res_id, base::Time::Now(), buffer,
                                   /*header_size_delta=*/kHeadData.size()),
      SqlPersistentStore::Error::kOk);

  EXPECT_EQ(GetResourceCheckSum(res_id),
            CalculateCheckSum(buffer->span(), kKey.hash()));

  // Corrupt the head data.
  {
    const std::string kCorruptedData = "_corrupted_";
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "UPDATE resources SET head = ? WHERE res_id = ?"));
    statement.BindBlob(0, base::as_byte_span(kCorruptedData));
    statement.BindInt64(1, res_id.value());
    ASSERT_TRUE(statement.Run());
  }

  auto result = OpenEntry(kKey);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SqlPersistentStore::Error::kCheckSumError);
}

TEST_F(SqlPersistentStoreTest, WriteAndReadData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  const std::string kData = "hello world";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kData, /*truncate=*/false);
  EXPECT_EQ(GetSizeOfAllEntries(), kSqlBackendStaticResourceSize +
                                       kKey.string().size() + kData.size());
  // Read data back.
  ReadAndVerifyData(kKey, res_id, /*offset=*/0, /*buffer_len=*/kData.size(),
                    /*body_end=*/kData.size(), /*sparse_reading=*/false, kData);
  // Verify blob data in the database.
  CheckBlobData(res_id, {{0, kData}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, kData.size(),
                             kKey.string().size() + kData.size());
}

TEST_F(SqlPersistentStoreTest, BlobCheckSum) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  const std::string kData = "hello world";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kData, /*truncate=*/false);
  {
    auto db_handle = ManuallyOpenDatabase();
    sql::Statement statement(db_handle->GetUniqueStatement(
        "SELECT check_sum FROM blobs WHERE res_id = ?"));
    statement.BindInt64(0, res_id.value());
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ(statement.ColumnInt(0),
              CalculateCheckSum(base::as_byte_span(kData), kKey.hash()));
  }
}

TEST_F(SqlPersistentStoreTest, ReadEntryDataInvalidDataSizeMismatch) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob data size so it doesn't match its start/end
  // offsets.
  const std::string kCorruptedData = "short";
  OverwriteBlobData(
      kKey, res_id, kCorruptedData,
      CalculateCheckSum(base::as_byte_span(kCorruptedData), kKey.hash()));

  // This read will try to read the corrupted blob, which should be detected.
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kInitialData.size());
  auto read_result = ReadEntryData(
      kKey, res_id, /*offset=*/0, read_buffer, kInitialData.size(),
      /*body_end=*/kInitialData.size(), /*sparse_reading=*/false);
  ASSERT_FALSE(read_result.has_value());
  EXPECT_EQ(read_result.error(), SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, ReadEntryDataCheckSumError) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob data so check_sum mismatch occur
  const std::string kCorruptedData = "0123456780";
  OverwriteBlobData(
      kKey, res_id, kCorruptedData,
      CalculateCheckSum(base::as_byte_span(kInitialData), kKey.hash()));

  // This read will try to read the corrupted blob, which should be detected.
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kInitialData.size());
  auto read_result = ReadEntryData(
      kKey, res_id, /*offset=*/0, read_buffer, kInitialData.size(),
      /*body_end=*/kInitialData.size(), /*sparse_reading=*/false);
  ASSERT_FALSE(read_result.has_value());
  EXPECT_EQ(read_result.error(), SqlPersistentStore::Error::kCheckSumError);
}

TEST_F(SqlPersistentStoreTest, ReadEntryDataInvalidDataRangeOverflow) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob's start and end to cause an overflow.
  CorruptBlobRange(res_id, std::numeric_limits<int64_t>::min(), 10);

  // This read will try to read the corrupted blob, which should be detected.
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kInitialData.size());
  auto read_result = ReadEntryData(
      kKey, res_id, /*offset=*/0, read_buffer, kInitialData.size(),
      /*body_end=*/kInitialData.size(), /*sparse_reading=*/false);
  ASSERT_FALSE(read_result.has_value());
  EXPECT_EQ(read_result.error(), SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, TrimOverlappingBlobsInvalidDataSizeMismatch) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob data size so it doesn't match its start/end
  // offsets.
  const std::string kCorruptedData = "short";
  OverwriteBlobData(
      kKey, res_id, kCorruptedData,
      CalculateCheckSum(base::as_byte_span(kCorruptedData), kKey.hash()));

  // This write will overlap with the corrupted blob, triggering
  // TrimOverlappingBlobs, which should detect the inconsistency.
  const std::string kOverwriteData = "abc";
  auto overwrite_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kOverwriteData);
  EXPECT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/2, overwrite_buffer,
                           kOverwriteData.size(), /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, TrimOverlappingBlobsCheckSumError) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob data so check_sum mismatch occur
  const std::string kCorruptedData = "0123456780";
  OverwriteBlobData(
      kKey, res_id, kCorruptedData,
      CalculateCheckSum(base::as_byte_span(kInitialData), kKey.hash()));

  // This write will overlap with the corrupted blob, triggering
  // TrimOverlappingBlobs, which should detect the inconsistency.
  const std::string kOverwriteData = "abc";
  auto overwrite_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kOverwriteData);
  EXPECT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/2, overwrite_buffer,
                           kOverwriteData.size(), /*truncate=*/false),
            SqlPersistentStore::Error::kCheckSumError);
}

TEST_F(SqlPersistentStoreTest, TrimOverlappingBlobsInvalidDataRangeOverflow) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob's start and end to cause an overflow.
  CorruptBlobRange(res_id, std::numeric_limits<int64_t>::min(), 10);

  // This write will overlap with the corrupted blob, triggering
  // TrimOverlappingBlobs, which should detect the overflow.
  const std::string kOverwriteData = "abc";
  auto overwrite_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kOverwriteData);
  EXPECT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/5, overwrite_buffer,
                           kOverwriteData.size(), /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, TruncateExistingBlobsInvalidDataSizeMismatch) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob data size so it doesn't match its start/end
  // offsets.
  const std::string kCorruptedData = "short";
  OverwriteBlobData(
      kKey, res_id, kCorruptedData,
      CalculateCheckSum(base::as_byte_span(kCorruptedData), kKey.hash()));

  // This write will truncate the entry, triggering TruncateExistingBlobs,
  // which should detect the inconsistency.
  EXPECT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/5, /*buffer=*/nullptr, /*buf_len=*/0,
                           /*truncate=*/true),
            SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, TruncateExistingBlobsInvalidDataRangeOverflow) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write initial data to create a blob.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Manually corrupt the blob's start and end to cause an overflow.
  CorruptBlobRange(res_id, std::numeric_limits<int64_t>::min(), 10);

  // This write will truncate the entry, triggering TruncateExistingBlobs,
  // which should detect the overflow.
  EXPECT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/1, /*buffer=*/nullptr, /*buf_len=*/0,
                           /*truncate=*/true),
            SqlPersistentStore::Error::kInvalidData);
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataInvalidArgument) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  const int buf_len = buffer->size();

  // Test with negative old_body_end.
  EXPECT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/-1, /*offset=*/0,
                           buffer, buf_len, /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidArgument);

  // Test with negative offset.
  EXPECT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/0, /*offset=*/-1,
                           buffer, buf_len, /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidArgument);

  // Test with offset + buf_len overflow.
  EXPECT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/0,
                           /*offset=*/std::numeric_limits<int64_t>::max(),
                           buffer, buf_len, /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidArgument);

  // Test with negative buf_len.
  EXPECT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                           buffer, /*buf_len=*/-1, /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidArgument);

  // Test with null buffer but positive buf_len.
  EXPECT_EQ(
      WriteEntryData(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                     /*buffer=*/nullptr, /*buf_len=*/1, /*truncate=*/false),
      SqlPersistentStore::Error::kInvalidArgument);

  // Test with buf_len > buffer->size().
  EXPECT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                           buffer, buf_len + 1, /*truncate=*/false),
            SqlPersistentStore::Error::kInvalidArgument);
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataInvalidDataBodyEndMismatch) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write initial data to set body_end.
  const std::string kInitialData = "0123456789";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  // The body_end in the database is now kInitialData.size().
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  ASSERT_EQ(details->body_end, kInitialData.size());

  // Now, try to write again, but provide an incorrect old_body_end.
  const std::string kOverwriteData = "abc";
  auto overwrite_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(kOverwriteData);
  EXPECT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/5, /*offset=*/8,
                           overwrite_buffer, kOverwriteData.size(),
                           /*truncate=*/false),
            SqlPersistentStore::Error::kBodyEndMismatch);
}

TEST_F(SqlPersistentStoreTest, ReadEntryDataInvalidArgument) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  const int buf_len = buffer->size();

  // Test with negative offset.
  auto result = ReadEntryData(kKey, res_id, /*offset=*/-1, buffer, buf_len,
                              /*body_end=*/10, /*sparse_reading=*/false);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SqlPersistentStore::Error::kInvalidArgument);

  // Test with negative buf_len.
  result = ReadEntryData(kKey, res_id, /*offset=*/0, buffer, /*buf_len=*/-1,
                         /*body_end=*/10, /*sparse_reading=*/false);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SqlPersistentStore::Error::kInvalidArgument);

  // Test with null buffer.
  result =
      ReadEntryData(kKey, res_id, /*offset=*/0, /*buffer=*/nullptr, buf_len,
                    /*body_end=*/10, /*sparse_reading=*/false);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SqlPersistentStore::Error::kInvalidArgument);

  // Test with buf_len > buffer->size().
  result = ReadEntryData(kKey, res_id, /*offset=*/0, buffer, buf_len + 1,
                         /*body_end=*/10, /*sparse_reading=*/false);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SqlPersistentStore::Error::kInvalidArgument);
}

TEST_F(SqlPersistentStoreTest, OverwriteEntryData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  const std::string kOverwriteData = "abc";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/kInitialData.size(),
                            /*offset=*/2, kOverwriteData, /*truncate=*/false);
  // Verify size updates.
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(details->body_end, kInitialData.size());
  EXPECT_EQ(details->bytes_usage, kKey.string().size() + kInitialData.size());
  // Read back and verify.
  ReadAndVerifyData(
      kKey, res_id, /*offset=*/0, /*buffer_len=*/kInitialData.size(),
      /*body_end=*/kInitialData.size(), /*sparse_reading=*/false, "12abc67890");
  // Verify blob data in the database.
  CheckBlobData(res_id, {{0, "12"}, {2, "abc"}, {5, "67890"}});
}

TEST_F(SqlPersistentStoreTest, AppendEntryData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  const std::string kInitialData = "initial";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  const std::string kAppendData = "-appended";
  const int64_t new_body_end = kInitialData.size() + kAppendData.size();
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/kInitialData.size(),
                            /*offset=*/kInitialData.size(), kAppendData,
                            /*truncate=*/false);
  // Read back and verify.
  ReadAndVerifyData(kKey, res_id, /*offset=*/0, /*buffer_len=*/new_body_end,
                    new_body_end, /*sparse_reading=*/false, "initial-appended");
  // Verify blob data in the database.
  CheckBlobData(res_id,
                {{0, kInitialData}, {kInitialData.size(), kAppendData}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, new_body_end,
                             kKey.string().size() + new_body_end);
}

TEST_F(SqlPersistentStoreTest, TruncateEntryData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  const std::string kTruncateData = "abc";
  const int64_t new_body_end = 2 + kTruncateData.size();
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/kInitialData.size(),
                            /*offset=*/2, kTruncateData, /*truncate=*/true);
  // Read back and verify.
  ReadAndVerifyData(kKey, res_id, /*offset=*/0, /*buffer_len=*/new_body_end,
                    new_body_end, /*sparse_reading=*/false, "12abc");
  // Verify blob data in the database.
  CheckBlobData(res_id, {{0, "12"}, {2, "abc"}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, new_body_end,
                             kKey.string().size() + new_body_end);
}

TEST_F(SqlPersistentStoreTest, TruncateWithNullBuffer) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write some initial data.
  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  VerifyBodyEndAndBytesUsage(kKey, kInitialData.size(),
                             kKey.string().size() + kInitialData.size());

  // Now, truncate the entry to a smaller size using a null buffer.
  const int64_t kTruncateOffset = 5;
  ASSERT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/kTruncateOffset, /*buffer=*/nullptr,
                           /*buf_len=*/0, /*truncate=*/true),
            SqlPersistentStore::Error::kOk);

  // Read back and verify.
  ReadAndVerifyData(kKey, res_id, /*offset=*/0, /*buffer_len=*/kTruncateOffset,
                    kTruncateOffset, /*sparse_reading=*/false, "12345");
  // Verify blob data in the database.
  CheckBlobData(res_id, {{0, "12345"}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, kTruncateOffset,
                             kKey.string().size() + kTruncateOffset);
}

TEST_F(SqlPersistentStoreTest, TruncateOverlappingMultipleBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  WriteAndVerifySingleByteBlobs(kKey, res_id, "01234");

  // Overwrite with a 2-byte truncate write in the middle.
  const std::string kOverwriteData = "XX";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/5, /*offset=*/1,
                            kOverwriteData, /*truncate=*/true);

  // The new body end should be offset + length = 1 + 2 = 3.
  const int64_t new_body_end = 3;

  // Verify the content.
  ReadAndVerifyData(kKey, res_id, 0, new_body_end, new_body_end, false, "0XX");

  // Verify the underlying blobs.
  // The original blob for "0" should be trimmed.
  // The original blobs for "1", "2", "3", "4" should be gone.
  // A new blob for "XX" should be present.
  CheckBlobData(res_id, {{0, "0"}, {1, "XX"}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, new_body_end,
                             kKey.string().size() + new_body_end);
}

TEST_F(SqlPersistentStoreTest, TruncateMultipleBlobsWithZeroLengthWrite) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  WriteAndVerifySingleByteBlobs(kKey, res_id, "01234");

  // Truncate at offset 2 with a zero-length write.
  ASSERT_EQ(
      WriteEntryData(kKey, res_id, /*old_body_end=*/5, /*offset=*/2,
                     /*buffer=*/nullptr, /*buf_len=*/0, /*truncate=*/true),
      SqlPersistentStore::Error::kOk);

  // The new body end should be the offset = 2.
  const int64_t new_body_end = 2;

  // Verify the content.
  ReadAndVerifyData(kKey, res_id, 0, new_body_end, new_body_end, false, "01");
  // Verify the underlying blobs.
  // The original blobs for "2", "3", "4" should be gone.
  // The original blob for "0" and "1" should remain.
  CheckBlobData(res_id, {{0, "0"}, {1, "1"}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, new_body_end,
                             kKey.string().size() + new_body_end);
}

TEST_F(SqlPersistentStoreTest, OverwriteMultipleBlobsWithoutTruncate) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  WriteAndVerifySingleByteBlobs(kKey, res_id, "01234");

  // Overwrite with a 2-byte write in the middle, without truncating.
  const std::string kOverwriteData = "AB";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/5, /*offset=*/1,
                            kOverwriteData, /*truncate=*/false);

  // The body end should remain 5.
  const int64_t new_body_end = 5;

  // Verify the content.
  ReadAndVerifyData(kKey, res_id, 0, new_body_end, new_body_end, false,
                    "0AB34");
  // Verify the underlying blobs.
  CheckBlobData(res_id, {{0, "0"}, {1, "AB"}, {3, "3"}, {4, "4"}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, new_body_end,
                             kKey.string().size() + new_body_end);
}

TEST_F(SqlPersistentStoreTest, WriteToDoomedEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  ASSERT_EQ(DoomEntry(kKey, res_id), SqlPersistentStore::Error::kOk);
  const std::string kData = "hello world";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kData, /*truncate=*/false);
  EXPECT_EQ(GetSizeOfAllEntries(), 0);
  // Verify blob data in the database.
  CheckBlobData(res_id, {{0, kData}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, kData.size(),
                             kKey.string().size() + kData.size());
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataNotFound) {
  CreateAndInitStore();
  const CacheEntryKey kKey("non-existent-key");
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  ASSERT_EQ(WriteEntryData(kKey, SqlPersistentStore::ResId(100),
                           /*old_body_end=*/0, /*offset=*/0, write_buffer,
                           write_buffer->size(), /*truncate=*/false),
            SqlPersistentStore::Error::kNotFound);
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataNullBufferNoTruncate) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  CheckBlobData(res_id, {{0, kInitialData}});

  const int64_t initial_body_end = kInitialData.size();
  const int64_t initial_size_of_all_entries = GetSizeOfAllEntries();

  // Writing a null buffer with truncate=false should be a no-op.
  ASSERT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/initial_body_end,
                           /*offset=*/5, /*buffer=*/nullptr, /*buf_len=*/0,
                           /*truncate=*/false),
            SqlPersistentStore::Error::kOk);

  // Verify size and content are unchanged.
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(details->body_end, initial_body_end);
  EXPECT_EQ(GetSizeOfAllEntries(), initial_size_of_all_entries);
  ReadAndVerifyData(kKey, res_id, /*offset=*/0, /*buffer_len=*/initial_body_end,
                    initial_body_end, /*sparse_reading=*/false, kInitialData);
  CheckBlobData(res_id, {{0, kInitialData}});
  VerifyBodyEndAndBytesUsage(kKey, kInitialData.size(),
                             kKey.string().size() + kInitialData.size());
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataZeroLengthBufferNoTruncate) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);
  CheckBlobData(res_id, {{0, kInitialData}});

  const int64_t initial_body_end = kInitialData.size();
  const int64_t initial_size_of_all_entries = GetSizeOfAllEntries();

  // Writing a zero-length buffer with truncate=false should be a no-op.
  auto zero_buffer = base::MakeRefCounted<net::IOBufferWithSize>(0);
  ASSERT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/initial_body_end,
                           /*offset=*/5, zero_buffer, /*buf_len=*/0,
                           /*truncate=*/false),
            SqlPersistentStore::Error::kOk);

  // Verify size and content are unchanged.
  auto details = GetResourceEntryDetails(kKey);
  ASSERT_TRUE(details.has_value());
  EXPECT_EQ(details->body_end, initial_body_end);
  EXPECT_EQ(GetSizeOfAllEntries(), initial_size_of_all_entries);
  ReadAndVerifyData(kKey, res_id, /*offset=*/0, /*buffer_len=*/initial_body_end,
                    initial_body_end, /*sparse_reading=*/false, kInitialData);
  CheckBlobData(res_id, {{0, kInitialData}});
  VerifyBodyEndAndBytesUsage(kKey, kInitialData.size(),
                             kKey.string().size() + kInitialData.size());
}

TEST_F(SqlPersistentStoreTest, TruncateWithNullBufferExtendingBody) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write some initial data.
  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Now, truncate the entry to a larger size using a null buffer.
  const int64_t kTruncateOffset = 20;
  ASSERT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/kTruncateOffset, /*buffer=*/nullptr,
                           /*buf_len=*/0, /*truncate=*/true),
            SqlPersistentStore::Error::kOk);

  // Read back and verify. The new space should be zero-filled.
  std::string expected_data = kInitialData;
  expected_data.append(kTruncateOffset - kInitialData.size(), '\0');
  ReadAndVerifyData(kKey, res_id, /*offset=*/0, /*buffer_len=*/kTruncateOffset,
                    kTruncateOffset, /*sparse_reading=*/false, expected_data);
  // Verify blob data in the database.
  CheckBlobData(res_id, {{0, kInitialData}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, kTruncateOffset,
                             kKey.string().size() + kInitialData.size());
}

TEST_F(SqlPersistentStoreTest, ExtendWithNullBufferNoTruncate) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write some initial data.
  const std::string kInitialData = "1234567890";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kInitialData, /*truncate=*/false);

  // Now, extend the entry to a larger size using a null buffer without
  // truncate.
  const int64_t kExtendOffset = 20;
  ASSERT_EQ(WriteEntryData(kKey, res_id, /*old_body_end=*/kInitialData.size(),
                           /*offset=*/kExtendOffset, /*buffer=*/nullptr,
                           /*buf_len=*/0, /*truncate=*/false),
            SqlPersistentStore::Error::kOk);

  // Read back and verify. The new space should be zero-filled.
  std::string expected_data = kInitialData;
  expected_data.append(kExtendOffset - kInitialData.size(), '\0');
  ReadAndVerifyData(kKey, res_id, /*offset=*/0, /*buffer_len=*/kExtendOffset,
                    kExtendOffset, /*sparse_reading=*/false, expected_data);
  // Verify blob data in the database.
  CheckBlobData(res_id, {{0, kInitialData}});
  // Verify size updates.
  VerifyBodyEndAndBytesUsage(kKey, kExtendOffset,
                             kKey.string().size() + kInitialData.size());
}

TEST_F(SqlPersistentStoreTest, WriteEntryDataComplexOverlap) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // 1. Initial write.
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            "0123456789", /*truncate=*/false);
  ReadAndVerifyData(kKey, res_id, 0, 10, 10, false, "0123456789");
  CheckBlobData(res_id, {{0, "0123456789"}});
  VerifyBodyEndAndBytesUsage(kKey, 10, kKey.string().size() + 10);

  // 2. Overwrite middle.
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/10, /*offset=*/2,
                            "AAAA", /*truncate=*/false);
  ReadAndVerifyData(kKey, res_id, 0, 10, 10, false, "01AAAA6789");
  CheckBlobData(res_id, {{0, "01"}, {2, "AAAA"}, {6, "6789"}});
  VerifyBodyEndAndBytesUsage(kKey, 10, kKey.string().size() + 10);

  // 3. Overwrite end.
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/10, /*offset=*/8,
                            "BB", /*truncate=*/false);
  ReadAndVerifyData(kKey, res_id, 0, 10, 10, false, "01AAAA67BB");
  CheckBlobData(res_id, {{0, "01"}, {2, "AAAA"}, {6, "67"}, {8, "BB"}});
  VerifyBodyEndAndBytesUsage(kKey, 10, kKey.string().size() + 10);

  // 4. Overwrite beginning.
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/10, /*offset=*/0,
                            "C",
                            /*truncate=*/false);
  ReadAndVerifyData(kKey, res_id, 0, 10, 10, false, "C1AAAA67BB");
  CheckBlobData(res_id,
                {{0, "C"}, {1, "1"}, {2, "AAAA"}, {6, "67"}, {8, "BB"}});
  VerifyBodyEndAndBytesUsage(kKey, 10, kKey.string().size() + 10);

  // 5. Overwrite all.
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/10, /*offset=*/0,
                            "DDDDDDDDDD", /*truncate=*/false);
  ReadAndVerifyData(kKey, res_id, 0, 10, 10, false, "DDDDDDDDDD");
  CheckBlobData(res_id, {{0, "DDDDDDDDDD"}});
  VerifyBodyEndAndBytesUsage(kKey, 10, kKey.string().size() + 10);

  // 6. Append.
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/10, /*offset=*/10,
                            "E", /*truncate=*/false);
  ReadAndVerifyData(kKey, res_id, 0, 11, 11, false, "DDDDDDDDDDE");
  CheckBlobData(res_id, {{0, "DDDDDDDDDD"}, {10, "E"}});
  VerifyBodyEndAndBytesUsage(kKey, 11, kKey.string().size() + 11);

  // 7. Sparse write.
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/11, /*offset=*/12,
                            "F", /*truncate=*/false);
  ReadAndVerifyData(kKey, res_id, 0, 13, 13, false,
                    base::MakeStringViewWithNulChars("DDDDDDDDDDE\0F"));
  CheckBlobData(res_id, {{0, "DDDDDDDDDD"}, {10, "E"}, {12, "F"}});
  VerifyBodyEndAndBytesUsage(kKey, 13, kKey.string().size() + 12);

  // 8. Overwrite with truncate.
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/13, /*offset=*/5,
                            "GG", /*truncate=*/true);
  ReadAndVerifyData(kKey, res_id, 0, 7, 7, false, "DDDDDGG");
  CheckBlobData(res_id, {{0, "DDDDD"}, {5, "GG"}});
  VerifyBodyEndAndBytesUsage(kKey, 7, kKey.string().size() + 7);

  // 9. Null buffer truncate.
  ASSERT_EQ(
      WriteEntryData(kKey, res_id, /*old_body_end=*/7, /*offset=*/5,
                     /*buffer=*/nullptr, /*buf_len=*/0, /*truncate=*/true),
      SqlPersistentStore::Error::kOk);
  ReadAndVerifyData(kKey, res_id, 0, 5, 5, false, "DDDDD");
  CheckBlobData(res_id, {{0, "DDDDD"}});
  VerifyBodyEndAndBytesUsage(kKey, 5, kKey.string().size() + 5);

  // 10. Write into a sparse region.
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/5, /*offset=*/10,
                            "SPARSE", /*truncate=*/false);
  ReadAndVerifyData(kKey, res_id, 0, 16, 16, false,
                    base::MakeStringViewWithNulChars("DDDDD\0\0\0\0\0SPARSE"));
  CheckBlobData(res_id, {{0, "DDDDD"}, {10, "SPARSE"}});
  VerifyBodyEndAndBytesUsage(kKey, 16, kKey.string().size() + 11);
}

TEST_F(SqlPersistentStoreTest, SparseRead) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  const std::string kData1 = "chunk1";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/0,
                            kData1, /*truncate=*/false);
  const std::string kData2 = "chunk2";
  const int64_t offset2 = 10;
  const int64_t new_body_end = offset2 + kData2.size();
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/kData1.size(),
                            /*offset=*/offset2, kData2, /*truncate=*/false);
  // Read with zero-filling.
  std::string expected_data = "chunk1";
  expected_data.append(offset2 - kData1.size(), '\0');
  expected_data.append("chunk2");
  ReadAndVerifyData(kKey, res_id, /*offset=*/0, /*buffer_len=*/new_body_end,
                    new_body_end, /*sparse_reading=*/false, expected_data);

  // Read with sparse_reading=true.
  // A sparse read that encounters a gap should stop at the end of the first
  // chunk.
  ReadAndVerifyData(kKey, res_id, /*offset=*/0, /*buffer_len=*/new_body_end,
                    new_body_end, /*sparse_reading=*/true, kData1);

  // A sparse read that extends into the gap should still stop at the end of
  // the first chunk.
  ReadAndVerifyData(kKey, res_id, /*offset=*/0,
                    /*buffer_len=*/kData1.size() + 1, new_body_end,
                    /*sparse_reading=*/true, kData1);

  // Read from the middle of chunk2.
  const int64_t read_offset = offset2 + 2;  // Start at 'u' in "chunk2"
  const int read_len = 2;
  ReadAndVerifyData(kKey, res_id, read_offset, /*buffer_len=*/read_len,
                    new_body_end,
                    /*sparse_reading=*/false, "un");

  // Read from the middle of chunk2, past the end of the data.
  const int long_read_len = 20;
  ReadAndVerifyData(kKey, res_id, read_offset, /*buffer_len=*/long_read_len,
                    new_body_end, /*sparse_reading=*/false, "unk2");

  // Verify blob data in the database.
  CheckBlobData(res_id, {{0, kData1}, {offset2, kData2}});
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeNoData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  auto result = GetEntryAvailableRange(kKey, res_id, 0, 100);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 0);
  EXPECT_EQ(result.available_len, 0);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeNoOverlap) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  const std::string kData = "some data";
  WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/0, /*offset=*/100,
                            kData, /*truncate=*/false);

  // Query before the data.
  auto result1 = GetEntryAvailableRange(kKey, res_id, 0, 50);
  EXPECT_EQ(result1.net_error, net::OK);
  EXPECT_EQ(result1.start, 0);
  EXPECT_EQ(result1.available_len, 0);

  // Query after the data.
  auto result2 = GetEntryAvailableRange(kKey, res_id, 200, 50);
  EXPECT_EQ(result2.net_error, net::OK);
  EXPECT_EQ(result2.start, 200);
  EXPECT_EQ(result2.available_len, 0);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeFullOverlap) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  FillDataInRange(kKey, res_id, /*old_body_end=*/0, 100, 100, 'a');

  auto result = GetEntryAvailableRange(kKey, res_id, 100, 100);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 100);
}

// Tests a query range that ends within the existing data.
// Query: [50, 150), Data: [100, 200) -> Overlap: [100, 150)
TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeQueryEndsInData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  FillDataInRange(kKey, res_id, /*old_body_end=*/0, 100, 100, 'a');

  auto result = GetEntryAvailableRange(kKey, res_id, 50, 100);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 50);
}

// Tests a query range that starts within the existing data.
// Query: [150, 250), Data: [100, 200) -> Overlap: [150, 200)
TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeQueryStartsInData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  FillDataInRange(kKey, res_id, /*old_body_end=*/0, 100, 100, 'a');

  auto result = GetEntryAvailableRange(kKey, res_id, 150, 100);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 150);
  EXPECT_EQ(result.available_len, 50);
}

// Tests a query range that fully contains the existing data.
// Query: [50, 250), Data: [100, 200) -> Overlap: [100, 200)
TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeQueryContainsData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  FillDataInRange(kKey, res_id, /*old_body_end=*/0, 100, 100, 'a');

  auto result = GetEntryAvailableRange(kKey, res_id, 50, 200);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 100);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeContained) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  FillDataInRange(kKey, res_id, /*old_body_end=*/0, 50, 200, 'a');

  auto result = GetEntryAvailableRange(kKey, res_id, 100, 100);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 100);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeContiguousBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  FillDataInRange(kKey, res_id, /*old_body_end=*/0, 100, 100, 'a');
  FillDataInRange(kKey, res_id, /*old_body_end=*/200, 200, 100, 'b');

  auto result = GetEntryAvailableRange(kKey, res_id, 100, 200);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 200);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeNonContiguousBlobs) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  FillDataInRange(kKey, res_id, /*old_body_end=*/0, 100, 100, 'a');
  FillDataInRange(kKey, res_id, /*old_body_end=*/200, 300, 100, 'b');

  auto result = GetEntryAvailableRange(kKey, res_id, 100, 300);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.available_len, 100);
}

TEST_F(SqlPersistentStoreTest, GetEntryAvailableRangeMultipleBlobsStopsAtGap) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write three blobs: [100, 200), [200, 300), [400, 500)
  FillDataInRange(kKey, res_id, 0, 100, 100, 'a');
  FillDataInRange(kKey, res_id, 200, 200, 100, 'a');
  FillDataInRange(kKey, res_id, 300, 400, 100, 'a');

  // Query for [150, 450). Should return [150, 300), which has length 150.
  auto result = GetEntryAvailableRange(kKey, res_id, 150, 300);
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.start, 150);
  EXPECT_EQ(result.available_len, 150);
}

TEST_F(SqlPersistentStoreTest, OpenNextEntryEmptyCache) {
  CreateAndInitStore();
  auto result = OpenNextEntry(SqlPersistentStore::EntryIterator());
  EXPECT_FALSE(result.has_value());
}

TEST_F(SqlPersistentStoreTest, OpenNextEntrySingleEntry) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  const auto created_res_id = CreateEntryAndGetResId(kKey);

  // Open the first (and only) entry.
  auto next_result1 = OpenNextEntry(SqlPersistentStore::EntryIterator());
  ASSERT_TRUE(next_result1.has_value());
  EXPECT_EQ(next_result1->key, kKey);
  EXPECT_EQ(next_result1->info.res_id, created_res_id);
  EXPECT_TRUE(next_result1->info.opened);
  EXPECT_EQ(next_result1->info.body_end, 0);
  ASSERT_NE(next_result1->info.head, nullptr);
  EXPECT_EQ(next_result1->info.head->size(), 0);

  // Try to open again, should be no more entries.
  auto next_result2 = OpenNextEntry(next_result1->iterator);
  EXPECT_FALSE(next_result2.has_value());
}

TEST_F(SqlPersistentStoreTest, OpenNextEntryMultipleEntries) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");
  const CacheEntryKey kKey3("key3");

  const auto res_id1 = CreateEntryAndGetResId(kKey1);
  const auto res_id2 = CreateEntryAndGetResId(kKey2);
  const auto res_id3 = CreateEntryAndGetResId(kKey3);

  // Entries should be returned in reverse order of creation (descending
  // res_id).
  auto next_result = OpenNextEntry(SqlPersistentStore::EntryIterator());
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey3);
  EXPECT_EQ(next_result->info.res_id, res_id3);

  next_result = OpenNextEntry(next_result->iterator);
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey2);
  EXPECT_EQ(next_result->info.res_id, res_id2);

  next_result = OpenNextEntry(next_result->iterator);
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey1);
  EXPECT_EQ(next_result->info.res_id, res_id1);

  next_result = OpenNextEntry(next_result->iterator);
  EXPECT_FALSE(next_result.has_value());
}

TEST_F(SqlPersistentStoreTest, OpenNextEntrySkipsDoomed) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKeyToDoom("key-to-doom");
  const CacheEntryKey kKey3("key3");

  ASSERT_TRUE(CreateEntry(kKey1).has_value());
  const auto res_id_to_doom = CreateEntryAndGetResId(kKeyToDoom);
  ASSERT_TRUE(CreateEntry(kKey3).has_value());

  // Doom the middle entry.
  ASSERT_EQ(DoomEntry(kKeyToDoom, res_id_to_doom),
            SqlPersistentStore::Error::kOk);

  // OpenNextEntry should skip the doomed entry.
  auto next_result = OpenNextEntry(SqlPersistentStore::EntryIterator());
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey3);  // Should be kKey3

  next_result = OpenNextEntry(next_result->iterator);
  ASSERT_TRUE(next_result.has_value());
  EXPECT_EQ(next_result->key, kKey1);  // Should skip kKeyToDoom and get kKey1

  next_result = OpenNextEntry(next_result->iterator);
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

  store_->CreateEntry(
      kKey, base::Time::Now(),
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::EntryInfoOrError) { callback_run = true; }));
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
  const auto res_id = CreateEntryAndGetResId(kKey);
  bool callback_run = false;
  store_->DoomEntry(kKey, res_id,
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
  const auto res_id = CreateEntryAndGetResId(kKey);
  ASSERT_EQ(DoomEntry(kKey, res_id), SqlPersistentStore::Error::kOk);

  bool callback_run = false;
  store_->DeleteDoomedEntry(
      kKey, res_id, base::BindLambdaForTesting([&](SqlPersistentStore::Error) {
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

TEST_F(SqlPersistentStoreTest, OpenNextEntryCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  bool callback_run = false;

  store_->OpenNextEntry(
      SqlPersistentStore::EntryIterator(),
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::OptionalEntryInfoWithKeyAndIterator) {
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
       UpdateEntryLastUsedByKeyCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  ASSERT_TRUE(CreateEntry(kKey).has_value());

  bool callback_run = false;
  store_->UpdateEntryLastUsedByKey(
      kKey, base::Time::Now(),
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::Error) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest,
       UpdateEntryLastUsedByResIdCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  bool callback_run = false;
  store_->UpdateEntryLastUsedByResId(
      kKey, res_id, base::Time::Now(),
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
  const auto res_id = CreateEntryAndGetResId(kKey);

  bool callback_run = false;
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  store_->UpdateEntryHeaderAndLastUsed(
      kKey, res_id, base::Time::Now(), /*new_hints=*/std::nullopt, buffer,
      buffer->size(),
      base::BindLambdaForTesting(
          [&](SqlPersistentStore::Error) { callback_run = true; }));
  store_.reset();
  FlushPendingTask();

  EXPECT_FALSE(callback_run);
}

TEST_F(SqlPersistentStoreTest, WriteDataCallbackNotRunOnStoreDestruction) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  const std::string kData = "hello world";
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  bool callback_run = false;
  store_->WriteEntryData(
      kKey, res_id, /*old_body_end=*/0, /*offset=*/0, write_buffer,
      kData.size(),
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
  const auto res_id = CreateEntryAndGetResId(kKey);
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  bool callback_run = false;
  store_->ReadEntryData(
      kKey, res_id, /*offset=*/0, read_buffer, read_buffer->size(),
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
  const auto res_id1 = CreateEntryAndGetResId(kKey1);
  WriteDataAndAssertSuccess(kKey1, res_id1, 0, 0, kData1, /*truncate=*/false);

  // Create entry 2.
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time time2 = base::Time::Now();
  const auto res_id2 = CreateEntryAndGetResId(kKey2);
  WriteDataAndAssertSuccess(kKey2, res_id2, 0, 0, kData2, /*truncate=*/false);

  // Create entry 3.
  task_environment_.AdvanceClock(base::Minutes(1));
  const base::Time time3 = base::Time::Now();
  const auto res_id3 = CreateEntryAndGetResId(kKey3);
  WriteDataAndAssertSuccess(kKey3, res_id3, 0, 0, kData3, /*truncate=*/false);

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
  const auto res_id1 = CreateEntryAndGetResId(kKey1);
  WriteDataAndAssertSuccess(kKey1, res_id1, 0, 0, kData1, /*truncate=*/false);

  // Create entry 2.
  task_environment_.AdvanceClock(base::Minutes(1));
  const auto res_id2 = CreateEntryAndGetResId(kKey2);
  WriteDataAndAssertSuccess(kKey2, res_id2, 0, 0, kData2, /*truncate=*/false);

  // Doom entry 1.
  ASSERT_EQ(DoomEntry(kKey1, res_id1), SqlPersistentStore::Error::kOk);

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
  const auto res_id = CreateEntryAndGetResId(kKey);
  bool callback_run = false;
  store_->GetEntryAvailableRange(
      kKey, res_id, 0, 100, base::BindLambdaForTesting([&](const RangeResult&) {
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
      kMaxBytes * kSqlBackendEvictionHighWaterMarkPermille / 1000;  // 9500
  CreateStore(kMaxBytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  EXPECT_NE(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);

  // Add entries until the size is just over the high watermark.
  int i = 0;
  while (GetSizeOfAllEntries() <= kHighWatermark) {
    const CacheEntryKey key(base::StringPrintf("key%d", i++));
    auto create_result = CreateEntry(key);
    ASSERT_TRUE(create_result.has_value());
    // Before the size exceeds the watermark, ShouldStartEviction should be
    // false.
    if (GetSizeOfAllEntries() <= kHighWatermark) {
      EXPECT_NE(store_->GetEvictionUrgency(),
                SqlPersistentStore::EvictionUrgency::kNeeded);
    }
  }

  // The last CreateEntry() pushed the size over the high watermark.
  EXPECT_EQ(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);
}

TEST_F(SqlPersistentStoreTest, StartEvictionReducesSizeToLowWatermark) {
  const int64_t kMaxBytes = 10000;
  const int64_t kHighWatermark =
      kMaxBytes * kSqlBackendEvictionHighWaterMarkPermille / 1000;  // 9500
  const int64_t kLowWatermark =
      kMaxBytes * kSqlBackendEvictionLowWaterMarkPermille / 1000;  // 9000

  CreateStore(kMaxBytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  // Load the in memory index to make GetIndexStateForHash() work.
  EXPECT_TRUE(LoadInMemoryIndex());

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
  EXPECT_EQ(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);

  // Start eviction.
  ASSERT_EQ(StartEviction({}, /*is_idle_time_eviction=*/false),
            SqlPersistentStore::Error::kOk);

  // After eviction, size should be <= low watermark.
  const int64_t size_after_eviction = GetSizeOfAllEntries();
  const int32_t count_after_eviction = GetEntryCount();
  EXPECT_LE(size_after_eviction, kLowWatermark);
  EXPECT_LT(count_after_eviction, count_before_eviction);

  // Verify oldest entries are gone.
  int evicted_count = count_before_eviction - count_after_eviction;
  for (int j = 0; j < evicted_count; ++j) {
    EXPECT_EQ(store_->GetIndexStateForHash(keys[j].hash()),
              SqlPersistentStore::IndexState::kHashNotFound);
    auto result = OpenEntry(keys[j]);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
  }

  // Verify newest entries are still there.
  for (size_t j = evicted_count; j < keys.size(); ++j) {
    EXPECT_EQ(store_->GetIndexStateForHash(keys[j].hash()),
              SqlPersistentStore::IndexState::kHashFound);
    auto result = OpenEntry(keys[j]);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->has_value());
  }

  EXPECT_NE(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);
}

TEST_F(SqlPersistentStoreTest, StartEvictionExcludesGivenKeys) {
  const int64_t kMaxBytes = 10000;
  const int64_t kHighWatermark =
      kMaxBytes * kSqlBackendEvictionHighWaterMarkPermille / 1000;  // 9500
  const int64_t kLowWatermark =
      kMaxBytes * kSqlBackendEvictionLowWaterMarkPermille / 1000;  // 9000

  CreateStore(kMaxBytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  // Add entries until size > high watermark.
  std::vector<CacheEntryKey> keys;
  std::optional<SqlPersistentStore::ResId> first_res_id;
  int i = 0;
  while (GetSizeOfAllEntries() <= kHighWatermark) {
    const CacheEntryKey key(base::StringPrintf("key%d", i++));
    keys.push_back(key);
    auto create_result = CreateEntry(key);
    ASSERT_TRUE(create_result.has_value());
    if (!first_res_id.has_value()) {
      first_res_id = create_result->res_id;
    }
    task_environment_.AdvanceClock(
        base::Seconds(1));  // To distinguish last_used
  }

  const int64_t size_before_eviction = GetSizeOfAllEntries();
  const int32_t count_before_eviction = GetEntryCount();
  EXPECT_GT(size_before_eviction, kHighWatermark);
  EXPECT_EQ(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);

  // Exclude the oldest entry.
  std::vector<SqlPersistentStore::ResIdAndShardId> excluded_list = {
      SqlPersistentStore::ResIdAndShardId(
          *first_res_id,
          store_->GetShardIdForHash(CacheEntryKey("key0").hash()))};

  // Start eviction.
  ASSERT_EQ(StartEviction(std::move(excluded_list),
                          /*is_idle_time_eviction=*/false),
            SqlPersistentStore::Error::kOk);

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

  EXPECT_NE(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);
}

TEST_F(SqlPersistentStoreTest, ShouldStartEvictionReturnsFalseWhileInProgress) {
  const int64_t kMaxBytes = 10000;
  const int64_t kHighWatermark =
      kMaxBytes * kSqlBackendEvictionHighWaterMarkPermille / 1000;  // 9500

  CreateStore(kMaxBytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  // Add entries until size > high watermark.
  int i = 0;
  while (GetSizeOfAllEntries() <= kHighWatermark) {
    const CacheEntryKey key(base::StringPrintf("key%d", i++));
    auto create_result = CreateEntry(key);
    ASSERT_TRUE(create_result.has_value());
  }

  EXPECT_EQ(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);

  base::test::TestFuture<SqlPersistentStore::Error> future;
  store_->StartEviction({}, /*is_idle_time_eviction=*/false,
                        future.GetCallback());

  // While eviction is in progress, ShouldStartEviction should return false.
  EXPECT_NE(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);

  // Let eviction finish.
  ASSERT_EQ(future.Get(), SqlPersistentStore::Error::kOk);

  // After eviction, size is below watermark, so it should still be false.
  EXPECT_NE(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);
}

int64_t CheckedGetFileSize(const base::FilePath& file_path) {
  return base::GetFileSize(file_path).value();
}

int SqlPersistentStoreTest::GetNumberForWritesRequiredForCheckpoint(
    const CacheEntryKey& entry_key,
    std::string_view data) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  store_ = std::make_unique<SqlPersistentStore>(
      temp_dir.GetPath(), kDefaultMaxBytes, net::CacheType::DISK_CACHE,
      std::vector<scoped_refptr<base::SequencedTaskRunner>>(
          background_task_runners_));
  CHECK_EQ(Init(), SqlPersistentStore::Error::kOk);

  const base::FilePath db_path =
      temp_dir.GetPath().Append(kSqlBackendDatabaseShard0FileName);
  const base::FilePath wal_path = sql::Database::WriteAheadLogPath(db_path);

  int64_t db_size = CheckedGetFileSize(db_path);
  int64_t previous_db_size = db_size;
  int64_t wal_size = CheckedGetFileSize(wal_path);
  int64_t previous_wal_size = wal_size;

  int number_of_writes = 0;
  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);
  while (true) {
    WriteDataAndAssertSuccess(entry_key, res_id,
                              /*old_body_end=*/number_of_writes,
                              /*offset=*/number_of_writes, data,
                              /*truncate=*/false);
    number_of_writes++;
    FlushPendingTask();
    db_size = CheckedGetFileSize(db_path);
    wal_size = CheckedGetFileSize(wal_path);
    if (db_size != previous_db_size) {
      // Checkpoint has been executed
      EXPECT_GT(db_size, previous_db_size);
      break;
    }
    // Until the checkpoint is executed, the wal size should monotonically
    // increase.
    EXPECT_GT(wal_size, previous_wal_size);
    previous_wal_size = wal_size;
  }
  store_.reset();
  FlushPendingTask();
  return number_of_writes;
}

TEST_F(SqlPersistentStoreTest, WalCheckpoint) {
  // Set small thresholds to shorten the test execution time.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{net::features::kDiskCacheBackendExperiment,
        {{net::features::kDiskCacheBackendParam.name, "sql"},
         {net::features::kSqlDiskCacheForceCheckpointThreshold.name, "200"},
         {net::features::kSqlDiskCacheIdleCheckpointThreshold.name, "100"}}}},
      {});

  auto test_helper = PerformanceScenarioTestHelper::Create();

  // Set the state to idle.
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kNoPageLoading);
  test_helper->SetInputScenario(ScenarioScope::kGlobal,
                                InputScenario::kNoInput);

  const CacheEntryKey kKey("my-key");
  const std::string_view kData = "a";
  int idle_checkpoint_write_count = 0;
  int non_idle_checkpoint_write_count = 0;
  {
    base::HistogramTester histogram_tester;
    idle_checkpoint_write_count =
        GetNumberForWritesRequiredForCheckpoint(kKey, kData);
    histogram_tester.ExpectTotalCount(
        "Net.SqlDiskCache.Backend.IdleCheckpoint.SuccessTime", 1);
    histogram_tester.ExpectTotalCount(
        "Net.SqlDiskCache.Backend.IdleCheckpoint.SuccessPages", 1);
  }

  // Set the state to non-idle.
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kVisiblePageLoading);
  {
    base::HistogramTester histogram_tester;
    non_idle_checkpoint_write_count =
        GetNumberForWritesRequiredForCheckpoint(kKey, kData);
    histogram_tester.ExpectTotalCount(
        "Net.SqlDiskCache.Backend.ForceCheckpoint.SuccessTime", 1);
    histogram_tester.ExpectTotalCount(
        "Net.SqlDiskCache.Backend.ForceCheckpoint.SuccessPages", 1);
  }

  // The number of writes required for a checkpoint in a non-idle state is
  // greater than in an idle state.
  EXPECT_GT(non_idle_checkpoint_write_count, idle_checkpoint_write_count);

  store_ = std::make_unique<SqlPersistentStore>(
      GetTempPath(), kDefaultMaxBytes, net::CacheType::DISK_CACHE,
      std::vector<scoped_refptr<base::SequencedTaskRunner>>(
          background_task_runners_));
  CHECK_EQ(Init(), SqlPersistentStore::Error::kOk);
  const base::FilePath db_path = GetDatabaseFilePath();
  int64_t previous_db_size = CheckedGetFileSize(db_path);

  const auto res_id = CreateEntryAndGetResId(kKey);

  // Write one less time than the number of writes required to trigger a
  // checkpoint in the idle state.
  for (int i = 0; i < idle_checkpoint_write_count - 1; ++i) {
    WriteDataAndAssertSuccess(kKey, res_id, /*old_body_end=*/i,
                              /*offset=*/i, kData,
                              /*truncate=*/false);
    FlushPendingTask();

    ASSERT_EQ(CheckedGetFileSize(db_path), previous_db_size);
  }

  // Calling MaybeRunCheckpoint should not trigger a checkpoint.
  MaybeRunCheckpoint(/*expected_result=*/false);

  // Even in an idle state, calling MaybeRunCheckpoint should not trigger a
  // checkpoint.
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kNoPageLoading);
  MaybeRunCheckpoint(/*expected_result=*/false);

  // Set the state to non-idle.
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kVisiblePageLoading);
  // Write data again. This exceeds the number of writes required for a
  // checkpoint in the idle state.
  WriteDataAndAssertSuccess(kKey, res_id,
                            /*old_body_end=*/idle_checkpoint_write_count - 1,
                            /*offset=*/idle_checkpoint_write_count - 1, kData,
                            /*truncate=*/false);
  FlushPendingTask();
  // Since it's in a non-idle state, a checkpoint is not yet performed.
  ASSERT_EQ(CheckedGetFileSize(db_path), previous_db_size);

  // Calling MaybeRunCheckpoint should not trigger a checkpoint.
  MaybeRunCheckpoint(/*expected_result=*/false);

  // After setting the state to idle, calling MaybeRunCheckpoint should
  // trigger a checkpoint.
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kNoPageLoading);
  MaybeRunCheckpoint(/*expected_result=*/true);
}

TEST_F(SqlPersistentStoreTest, IndexState) {
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");

  CreateStore();

  // Before initialization, index is not ready.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kNotReady);

  base::test::TestFuture<SqlPersistentStore::Error> future;
  store_->Initialize(future.GetCallback());
  // During initialization, index is not ready.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kNotReady);
  ASSERT_EQ(future.Get(), SqlPersistentStore::Error::kOk);

  // Even after the initialization finished, index is not ready.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kNotReady);

  // Load the in memory index.
  EXPECT_TRUE(LoadInMemoryIndex());

  // In memory index load process should not be triggered twice.
  EXPECT_FALSE(LoadInMemoryIndex());

  // After loading the in memory index, returns kHashNotFound.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);

  // Create an entry.
  SqlPersistentStore::EntryInfoOrError result = this->CreateEntry(kKey1);
  ASSERT_TRUE(result.has_value());
  SqlPersistentStore::ResId res_id1 = result->res_id;

  // Now the hash for key1 should be found.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  // Key2 should still not be found.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);

  // Create another entry.
  result = this->CreateEntry(kKey2);
  ASSERT_TRUE(result.has_value());

  // Both hashes should be found.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashFound);

  // Doom the first entry.
  ASSERT_EQ(SqlPersistentStore::Error::kOk, this->DoomEntry(kKey1, res_id1));

  // The hash for the doomed entry should be removed from the index.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashFound);

  // Delete the second entry.
  ASSERT_EQ(SqlPersistentStore::Error::kOk, this->DeleteLiveEntry(kKey2));

  // The hash for the deleted entry should be removed.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);

  // Re-create entry for key1.
  result = this->CreateEntry(kKey1);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashFound);

  // Delete all entries.
  ASSERT_EQ(SqlPersistentStore::Error::kOk, this->DeleteAllEntries());
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);
}

// Test index reloading from a non-empty database.
TEST_F(SqlPersistentStoreTest, IndexReloads) {
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");

  CreateAndInitStore();
  // Load the in memory index.
  EXPECT_TRUE(LoadInMemoryIndex());

  // Create two entries.
  ASSERT_TRUE(this->CreateEntry(kKey1).has_value());
  ASSERT_TRUE(this->CreateEntry(kKey2).has_value());

  // The hashes should be found.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashFound);

  // Close and reopen the store.
  ClearStore();
  CreateAndInitStore();
  // Load the in memory index.
  EXPECT_TRUE(LoadInMemoryIndex());

  // The index should be re-populated.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(CacheEntryKey("other").hash()),
            SqlPersistentStore::IndexState::kHashNotFound);
}

TEST_F(SqlPersistentStoreTest, LoadIndexOnInitFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{net::features::kDiskCacheBackendExperiment,
        {{net::features::kDiskCacheBackendParam.name, "sql"},
         {net::features::kSqlDiskCacheLoadIndexOnInit.name, "true"}}}},
      {});

  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");

  CreateAndInitStore();
  // Create two entries.
  ASSERT_TRUE(this->CreateEntry(kKey1).has_value());
  ASSERT_TRUE(this->CreateEntry(kKey2).has_value());

  // Close and reopen the store.
  ClearStore();
  CreateStore();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  // The index should be loaded on init.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(CacheEntryKey("other").hash()),
            SqlPersistentStore::IndexState::kHashNotFound);

  // MaybeLoadInMemoryIndex() should do nothing.
  EXPECT_FALSE(LoadInMemoryIndex());
}

TEST_F(SqlPersistentStoreTest, IndexLoadNotInitializedFailure) {
  CreateStore();
  EXPECT_TRUE(LoadInMemoryIndex(SqlPersistentStore::Error::kNotInitialized));
}

TEST_F(SqlPersistentStoreTest, SimulateDbFailureInitializationFailure) {
  CreateStore();
  store_->SetSimulateDbFailureForTesting(true);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kFailedForTesting);
}

TEST_F(SqlPersistentStoreTest, SimulateDbFailure) {
  CreateAndInitStore();

  store_->SetSimulateDbFailureForTesting(true);

  const CacheEntryKey kKey("my-key");
  auto create_result = CreateEntry(kKey);
  ASSERT_FALSE(create_result.has_value());
  EXPECT_EQ(create_result.error(),
            SqlPersistentStore::Error::kFailedForTesting);

  auto open_result = OpenEntry(kKey);
  ASSERT_FALSE(open_result.has_value());
  EXPECT_EQ(open_result.error(), SqlPersistentStore::Error::kFailedForTesting);

  auto open_or_create_result = OpenOrCreateEntry(kKey);
  ASSERT_FALSE(open_or_create_result.has_value());
  EXPECT_EQ(open_or_create_result.error(),
            SqlPersistentStore::Error::kFailedForTesting);

  EXPECT_EQ(DoomEntry(kKey, SqlPersistentStore::ResId(1)),
            SqlPersistentStore::Error::kFailedForTesting);

  EXPECT_EQ(DeleteDoomedEntry(kKey, SqlPersistentStore::ResId(1)),
            SqlPersistentStore::Error::kFailedForTesting);

  EXPECT_EQ(DeleteLiveEntry(kKey),
            SqlPersistentStore::Error::kFailedForTesting);

  EXPECT_EQ(DeleteAllEntries(), SqlPersistentStore::Error::kFailedForTesting);

  EXPECT_EQ(DeleteLiveEntriesBetween(base::Time::Now(),
                                     base::Time::Now() + base::Seconds(1), {}),
            SqlPersistentStore::Error::kFailedForTesting);

  EXPECT_EQ(UpdateEntryLastUsedByKey(kKey, base::Time::Now()),
            SqlPersistentStore::Error::kFailedForTesting);

  EXPECT_EQ(UpdateEntryLastUsedByResId(kKey, SqlPersistentStore::ResId(1),
                                       base::Time::Now()),
            SqlPersistentStore::Error::kFailedForTesting);

  // Prepare new header data.
  const std::string kNewHeadData = "new_header_data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kNewHeadData);
  EXPECT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, SqlPersistentStore::ResId(1),
                                   base::Time::Now(), buffer, buffer->size()),
      SqlPersistentStore::Error::kFailedForTesting);

  EXPECT_EQ(WriteEntryData(kKey, SqlPersistentStore::ResId(1), 0, 0, buffer, 0,
                           false),
            SqlPersistentStore::Error::kFailedForTesting);

  auto read_data_result =
      ReadEntryData(kKey, SqlPersistentStore::ResId(1), 0, buffer, 0, 0, false);
  ASSERT_FALSE(read_data_result.has_value());
  EXPECT_EQ(read_data_result.error(),
            SqlPersistentStore::Error::kFailedForTesting);

  EXPECT_EQ(GetEntryAvailableRange(kKey, SqlPersistentStore::ResId(1), 0, 100)
                .net_error,
            net::Error::ERR_FAILED);

  EXPECT_EQ(CalculateSizeOfEntriesBetween(base::Time::Now(),
                                          base::Time::Now() + base::Seconds(1))
                .error(),
            SqlPersistentStore::Error::kFailedForTesting);

  EXPECT_EQ(StartEviction({}, /*is_idle_time_eviction=*/false),
            SqlPersistentStore::Error::kOk);

  EXPECT_FALSE(OpenNextEntry(SqlPersistentStore::EntryIterator()).has_value());

  EXPECT_TRUE(LoadInMemoryIndex(SqlPersistentStore::Error::kFailedForTesting));

  store_->SetSimulateDbFailureForTesting(false);

  create_result = CreateEntry(kKey);
  ASSERT_TRUE(create_result.has_value());

  open_result = OpenEntry(kKey);
  ASSERT_TRUE(open_result.has_value());
  ASSERT_TRUE(open_result->has_value());
}

TEST_F(SqlPersistentStoreTest, AfterRazeAndPoisoned) {
  CreateAndInitStore();

  store_->RazeAndPoisonForTesting();

  const CacheEntryKey kKey("my-key");
  auto create_result = CreateEntry(kKey);
  ASSERT_FALSE(create_result.has_value());
  EXPECT_EQ(create_result.error(), SqlPersistentStore::Error::kDatabaseClosed);

  auto open_result = OpenEntry(kKey);
  ASSERT_FALSE(open_result.has_value());
  EXPECT_EQ(open_result.error(), SqlPersistentStore::Error::kDatabaseClosed);

  auto open_or_create_result = OpenOrCreateEntry(kKey);
  ASSERT_FALSE(open_or_create_result.has_value());
  EXPECT_EQ(open_or_create_result.error(),
            SqlPersistentStore::Error::kDatabaseClosed);

  EXPECT_EQ(DoomEntry(kKey, SqlPersistentStore::ResId(1)),
            SqlPersistentStore::Error::kDatabaseClosed);

  EXPECT_EQ(DeleteDoomedEntry(kKey, SqlPersistentStore::ResId(1)),
            SqlPersistentStore::Error::kDatabaseClosed);

  EXPECT_EQ(DeleteLiveEntry(kKey), SqlPersistentStore::Error::kDatabaseClosed);

  EXPECT_EQ(DeleteAllEntries(), SqlPersistentStore::Error::kDatabaseClosed);

  EXPECT_EQ(DeleteLiveEntriesBetween(base::Time::Now(),
                                     base::Time::Now() + base::Seconds(1), {}),
            SqlPersistentStore::Error::kDatabaseClosed);

  EXPECT_EQ(UpdateEntryLastUsedByKey(kKey, base::Time::Now()),
            SqlPersistentStore::Error::kDatabaseClosed);

  EXPECT_EQ(UpdateEntryLastUsedByResId(kKey, SqlPersistentStore::ResId(1),
                                       base::Time::Now()),
            SqlPersistentStore::Error::kDatabaseClosed);

  // Prepare new header data.
  const std::string kNewHeadData = "new_header_data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kNewHeadData);
  EXPECT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, SqlPersistentStore::ResId(1),
                                   base::Time::Now(), buffer, buffer->size()),
      SqlPersistentStore::Error::kDatabaseClosed);

  EXPECT_EQ(WriteEntryData(kKey, SqlPersistentStore::ResId(1), 0, 0, buffer, 0,
                           false),
            SqlPersistentStore::Error::kDatabaseClosed);

  auto read_data_result =
      ReadEntryData(kKey, SqlPersistentStore::ResId(1), 0, buffer, 0, 0, false);
  ASSERT_FALSE(read_data_result.has_value());
  EXPECT_EQ(read_data_result.error(),
            SqlPersistentStore::Error::kDatabaseClosed);

  EXPECT_EQ(GetEntryAvailableRange(kKey, SqlPersistentStore::ResId(1), 0, 100)
                .net_error,
            net::Error::ERR_FAILED);

  EXPECT_EQ(CalculateSizeOfEntriesBetween(base::Time::Now(),
                                          base::Time::Now() + base::Seconds(1))
                .error(),
            SqlPersistentStore::Error::kDatabaseClosed);

  EXPECT_FALSE(OpenNextEntry(SqlPersistentStore::EntryIterator()).has_value());

  EXPECT_EQ(StartEviction({}, /*is_idle_time_eviction=*/false),
            SqlPersistentStore::Error::kOk);

  EXPECT_TRUE(LoadInMemoryIndex(SqlPersistentStore::Error::kDatabaseClosed));
}

TEST_F(SqlPersistentStoreTest,
       ShouldStartEvictionReturnsFalseAfterRazeAndPoisoned) {
  // Use a small max_bytes to make it easy to cross the high watermark.
  const int64_t kMaxBytes = 10000;
  CreateStore(kMaxBytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  EXPECT_NE(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);

  // Add entries until the size is just over the high watermark.
  int i = 0;
  while (store_->GetEvictionUrgency() !=
         SqlPersistentStore::EvictionUrgency::kNeeded) {
    const CacheEntryKey key(base::StringPrintf("key%d", i++));
    auto create_result = CreateEntry(key);
    ASSERT_TRUE(create_result.has_value());
  }

  EXPECT_EQ(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);

  store_->RazeAndPoisonForTesting();

  EXPECT_EQ(CreateEntry(CacheEntryKey("test")).error(),
            SqlPersistentStore::Error::kDatabaseClosed);

  EXPECT_NE(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);
}

TEST_F(SqlPersistentStoreTest, GetEvictionUrgency) {
  const int64_t kMaxBytes = 10000;
  const int64_t kHighWatermark =
      kMaxBytes * kSqlBackendEvictionHighWaterMarkPermille / 1000;  // 9500
  const int64_t kIdleTimeHighWatermark =
      kMaxBytes * kSqlBackendIdleTimeEvictionHighWaterMarkPermille /
      1000;  // 9250

  CreateStore(kMaxBytes);
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  EXPECT_EQ(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNotNeeded);

  // Add entries until the size is just over the idle time high watermark.
  int i = 0;
  while (GetSizeOfAllEntries() <= kIdleTimeHighWatermark) {
    const CacheEntryKey key(base::StringPrintf("key%d", i++));
    auto create_result = CreateEntry(key);
    ASSERT_TRUE(create_result.has_value());
  }

  EXPECT_EQ(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kIdleTime);

  // Add more entries until the size is just over the high watermark.
  while (GetSizeOfAllEntries() <= kHighWatermark) {
    const CacheEntryKey key(base::StringPrintf("key%d", i++));
    auto create_result = CreateEntry(key);
    ASSERT_TRUE(create_result.has_value());
  }

  EXPECT_EQ(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kNeeded);
}

TEST_F(SqlPersistentStoreTest, IdleTimeEviction) {
  const int64_t kMaxBytes = 10000;
  const int64_t kIdleTimeHighWatermark =
      kMaxBytes * kSqlBackendIdleTimeEvictionHighWaterMarkPermille /
      1000;  // 9250

  CreateStore(kMaxBytes);
  store_->EnableStrictCorruptionCheckForTesting();
  ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);

  // Add entries to trigger idle time eviction.
  int i = 0;
  while (GetSizeOfAllEntries() <= kIdleTimeHighWatermark) {
    const CacheEntryKey key(base::StringPrintf("key%d", i++));
    auto create_result = CreateEntry(key);
    ASSERT_TRUE(create_result.has_value());
  }

  EXPECT_EQ(store_->GetEvictionUrgency(),
            SqlPersistentStore::EvictionUrgency::kIdleTime);

  auto test_helper = PerformanceScenarioTestHelper::Create();

  // Set the state to non-idle.
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kVisiblePageLoading);
  test_helper->SetInputScenario(ScenarioScope::kGlobal,
                                InputScenario::kNoInput);

  // Idle time eviction should be aborted
  ASSERT_EQ(StartEviction({}, /*is_idle_time_eviction=*/true),
            SqlPersistentStore::Error::kAbortedDueToBrowserActivity);

  // Set the state to idle.
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kNoPageLoading);
  test_helper->SetInputScenario(ScenarioScope::kGlobal,
                                InputScenario::kNoInput);

  // Start idle time eviction.
  ASSERT_EQ(StartEviction({}, /*is_idle_time_eviction=*/true),
            SqlPersistentStore::Error::kOk);

  // Eviction should have run and reduced the size.
  const int64_t kLowWatermark =
      kMaxBytes * kSqlBackendEvictionLowWaterMarkPermille / 1000;  // 9000
  EXPECT_LE(GetSizeOfAllEntries(), kLowWatermark);
}

TEST_F(SqlPersistentStoreTest, DoomEntryWhileIndexLoading) {
  CreateAndInitStore();
  const CacheEntryKey kKey1("my-key1");
  const CacheEntryKey kKey2("my-key2");
  const CacheEntryKey kKey3("my-key3");

  // 1. Create three entries.
  SqlPersistentStore::ResId res_id1 = CreateEntryAndGetResId(kKey1);
  SqlPersistentStore::ResId res_id2 = CreateEntryAndGetResId(kKey2);
  SqlPersistentStore::ResId res_id3 = CreateEntryAndGetResId(kKey3);

  // 2. Ensure index is not loaded.
  ASSERT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kNotReady);
  ASSERT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kNotReady);
  ASSERT_EQ(store_->GetIndexStateForHash(kKey3.hash()),
            SqlPersistentStore::IndexState::kNotReady);

  // 3. Start loading the index.
  base::test::TestFuture<SqlPersistentStore::Error> load_index_future;
  ASSERT_TRUE(store_->MaybeLoadInMemoryIndex(load_index_future.GetCallback()));

  // 4. Doom two entries while index loading is in flight.
  base::test::TestFuture<SqlPersistentStore::Error> doom_future1;
  store_->DoomEntry(kKey1, res_id1, doom_future1.GetCallback());
  base::test::TestFuture<SqlPersistentStore::Error> doom_future3;
  store_->DoomEntry(kKey3, res_id3, doom_future3.GetCallback());

  // 5. Wait for index loading to complete.
  EXPECT_EQ(load_index_future.Get(), SqlPersistentStore::Error::kOk);

  // 6. The index is now loaded. The doomed entries should not be in the index
  //    because they were added to `pending_doomed_res_ids_` and removed after
  //    the index was loaded.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey3.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);

  // 7. Wait for the doom operations to complete.
  EXPECT_EQ(doom_future1.Get(), SqlPersistentStore::Error::kOk);
  EXPECT_EQ(doom_future3.Get(), SqlPersistentStore::Error::kOk);

  // 8. The index state should remain the same.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashFound);
  EXPECT_EQ(store_->GetIndexStateForHash(kKey3.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);

  // 9. Verify that the doomed entries are gone and the other entry is still
  //    accessible.
  auto open_result1 = OpenEntry(kKey1);
  ASSERT_TRUE(open_result1.has_value());
  EXPECT_FALSE(open_result1->has_value());
  auto open_result2 = OpenEntry(kKey2);
  ASSERT_TRUE(open_result2.has_value());
  ASSERT_TRUE(open_result2->has_value());
  EXPECT_EQ((*open_result2)->res_id, res_id2);
  auto open_result3 = OpenEntry(kKey3);
  ASSERT_TRUE(open_result3.has_value());
  EXPECT_FALSE(open_result3->has_value());
}

TEST_F(SqlPersistentStoreTest, DoomEntryRecoversIndexOnDbFailure) {
  CreateAndInitStore();

  // Load the in-memory index, which is necessary for the index recovery
  // mechanism.
  ASSERT_TRUE(LoadInMemoryIndex());

  const CacheEntryKey kKey("my-key");
  const auto res_id = CreateEntryAndGetResId(kKey);

  // The entry should be in the index.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey.hash()),
            SqlPersistentStore::IndexState::kHashFound);

  // Simulate a database failure.
  store_->SetSimulateDbFailureForTesting(true);

  // Try to doom the entry. The database operation will fail.
  ASSERT_EQ(DoomEntry(kKey, res_id),
            SqlPersistentStore::Error::kFailedForTesting);

  // Because the DB operation failed, the entry should have been re-added to
  // the in-memory index.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey.hash()),
            SqlPersistentStore::IndexState::kHashFound);

  // Disable the failure simulation.
  store_->SetSimulateDbFailureForTesting(false);

  // The entry should still be openable.
  auto open_result = OpenEntry(kKey);
  ASSERT_TRUE(open_result.has_value());
  ASSERT_TRUE(open_result->has_value());
  EXPECT_EQ(open_result.value()->res_id, res_id);

  // Doom the entry again. This time it should succeed.
  ASSERT_EQ(DoomEntry(kKey, res_id), SqlPersistentStore::Error::kOk);

  // The entry should now be gone from the index.
  EXPECT_EQ(store_->GetIndexStateForHash(kKey.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);

  // The entry should not be openable.
  open_result = OpenEntry(kKey);
  ASSERT_TRUE(open_result.has_value());
  EXPECT_FALSE(open_result->has_value());
}

TEST_F(SqlPersistentStoreTest, SetAndGetEntryInMemoryData) {
  CreateAndInitStore();
  const CacheEntryKey kKey("my-key");

  // 1. Create a new entry.
  auto res_id = CreateEntryAndGetResId(kKey);

  // 2. Load the index.
  EXPECT_TRUE(LoadInMemoryIndex());

  // 3. Set in-memory data hints.
  const uint8_t hints_value = 42;
  store_->SetInMemoryEntryDataHints(kKey.hash(), res_id,
                                    MemoryEntryDataHints(hints_value));

  // 4. Get the hints and verify.
  auto hints = store_->GetInMemoryEntryDataHints(kKey.hash());
  ASSERT_TRUE(hints.has_value());
  EXPECT_EQ(hints->value(), hints_value);

  // 5. Persist the hints to the database.
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>("");
  ASSERT_EQ(
      UpdateEntryHeaderAndLastUsed(kKey, res_id, base::Time::Now(), buffer, 0,
                                   MemoryEntryDataHints(hints_value)),
      SqlPersistentStore::Error::kOk);

  // Verify hints are in the database.
  auto db_hints = GetResourceHints(kKey);
  ASSERT_TRUE(db_hints.has_value());
  EXPECT_EQ(*db_hints, hints_value);

  // 6. Close and re-open the store.
  ClearStore();
  CreateAndInitStore();
  EXPECT_TRUE(LoadInMemoryIndex());

  // 7. Get the hints again and verify they were loaded from the DB.
  auto reloaded_hints = store_->GetInMemoryEntryDataHints(kKey.hash());
  ASSERT_TRUE(reloaded_hints.has_value());
  EXPECT_EQ(reloaded_hints->value(), hints_value);
}

}  // namespace disk_cache
