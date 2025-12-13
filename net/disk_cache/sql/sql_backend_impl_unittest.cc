// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_backend_impl.h"

#include <cstdint>
#include <variant>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "components/performance_manager/scenario_api/performance_scenario_test_support.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_entry_impl.h"
#include "net/test/gtest_util.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

using performance_scenarios::InputScenario;
using performance_scenarios::LoadingScenario;
using performance_scenarios::PerformanceScenarioTestHelper;
using performance_scenarios::ScenarioScope;

namespace disk_cache {
namespace {

using testing::ElementsAre;
using testing::Pair;
using FakeIndexFileError = SqlBackendImpl::FakeIndexFileError;

// Default max cache size for tests, 10 MB.
inline constexpr int64_t kDefaultMaxBytes = 10 * 1024 * 1024;

// Helper to create a new entry and write a data.
Entry* CreateEntryAndWriteData(SqlBackendImpl* backend,
                               const std::string& key,
                               const std::string& data) {
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry(key, net::HIGHEST, cb_create.callback()));
  CHECK_EQ(create_result.net_error(), net::OK);
  auto* entry = create_result.ReleaseEntry();
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(data);
  net::TestCompletionCallback cb_write;
  EXPECT_EQ(
      cb_write.GetResult(entry->WriteData(1, 0, buffer.get(), buffer->size(),
                                          cb_write.callback(), false)),
      static_cast<int>(buffer->size()));
  return entry;
}

// Helper to read data and verify its content.
void ReadAndVerifyData(Entry* entry, std::string_view expected_data) {
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(expected_data.size() + 1);
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), static_cast<int>(expected_data.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), expected_data.size()),
            expected_data);
}

size_t GetShardCount() {
  return std::max(std::min(net::features::kSqlDiskCacheShardCount.Get(), 255),
                  1);
}
std::string GetExpectedFakeIndexContents() {
  return base::StrCat(
      {kSqlBackendFakeIndexPrefix, base::NumberToString(GetShardCount())});
}

class SqlBackendImplTest : public testing::Test {
 public:
  SqlBackendImplTest() = default;
  ~SqlBackendImplTest() override = default;

  // Sets up a temporary directory and a background task runner for each test.
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  std::unique_ptr<SqlBackendImpl> CreateBackend() {
    return std::make_unique<SqlBackendImpl>(
        temp_dir_.GetPath(), kDefaultMaxBytes, net::CacheType::DISK_CACHE);
  }

  std::unique_ptr<SqlBackendImpl> CreateBackendAndInit(
      int64_t max_bytes = kDefaultMaxBytes) {
    auto backend = std::make_unique<SqlBackendImpl>(
        temp_dir_.GetPath(), max_bytes, net::CacheType::DISK_CACHE);
    base::test::TestFuture<int> future;
    backend->Init(future.GetCallback());
    CHECK_EQ(future.Get(), net::OK);
    return backend;
  }
  void WaitUntilInitialized(
      SqlBackendImpl& backend,
      const scoped_refptr<SqlBackendImpl::ResIdOrErrorHolder>&
          res_id_or_error) {
    CHECK(res_id_or_error);
    while (!res_id_or_error->data.has_value()) {
      FlushQueue(backend);
    }
  }

  void FlushQueue(SqlBackendImpl& backend) {
    net::TestCompletionCallback flush_cb;
    backend.FlushQueueForTest(flush_cb.callback());
    EXPECT_THAT(flush_cb.WaitForResult(), IsOk());
  }

  void FlushQueueInTaskRunners(
      const std::vector<scoped_refptr<base::SequencedTaskRunner>>&
          task_runners) {
    for (auto& runner : task_runners) {
      base::RunLoop run_loop;
      runner->PostTask(FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  bool LoadInMemoryIndex(SqlBackendImpl& backend) {
    auto* store = backend.GetSqlStoreForTest();
    base::test::TestFuture<SqlPersistentStore::Error> future;
    auto ret = store->MaybeLoadInMemoryIndex(future.GetCallback());
    if (ret) {
      CHECK_EQ(future.Get(), SqlPersistentStore::Error::kOk);
      return true;
    }
    return false;
  }

  // Gets the total size of all entries.
  int64_t GetSizeOfAllEntries(SqlBackendImpl& backend) {
    return backend.GetSqlStoreForTest()->GetSizeOfAllEntries();
  }

  // Opens the database for a specific shard and returns the count of blobs
  // associated with a given resource ID.
  int64_t OpenDatabaseAndGetBlobsCount(SqlPersistentStore::ShardId shard_id,
                                       SqlPersistentStore::ResId res_id) {
    auto db = std::make_unique<sql::Database>(
        sql::DatabaseOptions()
            .set_exclusive_locking(true)
#if BUILDFLAG(IS_WIN)
            .set_exclusive_database_file_lock(true)
#endif  // IS_WIN
            .set_preload(true)
            .set_wal_mode(true),
        sql::Database::Tag("HttpCacheDiskCache"));
    CHECK(db->Open(temp_dir_.GetPath().AppendASCII(
        base::StrCat({kSqlBackendDatabaseFileNamePrefix,
                      base::NumberToString(shard_id.value())}))));
    sql::Statement s(
        db->GetUniqueStatement("SELECT COUNT(*) FROM blobs where res_id = ?"));
    s.BindInt64(0, res_id.value());
    CHECK(s.Step());
    return s.ColumnInt64(0);
  }

  void RunDelayedPostInitializationTasksTest();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
};

TEST_F(SqlBackendImplTest, InitWithNoFakeIndexFile) {
  const std::string expected_contents = GetExpectedFakeIndexContents();
  base::HistogramTester histogram_tester;
  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::OK);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kOkNew, 1);

  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  const std::optional<int64_t> file_size = base::GetFileSize(file_path);
  ASSERT_TRUE(file_size.has_value());
  EXPECT_EQ(*file_size, expected_contents.size());
  std::vector<uint8_t> contents(expected_contents.size());
  ASSERT_TRUE(base::ReadFile(file_path, contents));
  EXPECT_EQ(contents, base::as_byte_span(expected_contents));
}

TEST_F(SqlBackendImplTest, InitWithFakeIndexFile) {
  const std::string expected_contents = GetExpectedFakeIndexContents();
  base::HistogramTester histogram_tester;
  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  ASSERT_TRUE(
      base::WriteFile(file_path, base::as_byte_span(expected_contents)));

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::OK);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kOkExisting, 1);
}

TEST_F(SqlBackendImplTest, InitWithCorruptedFakeIndexFile) {
  std::string corrupted_contents = GetExpectedFakeIndexContents();
  base::span<uint8_t> corrupted_contents_span =
      base::as_writable_bytes(base::span(corrupted_contents));
  // Rewrite the last char to 'X'.
  corrupted_contents_span.subspan(corrupted_contents_span.size() - 1)
      .copy_from({'X'});
  base::HistogramTester histogram_tester;
  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  ASSERT_TRUE(base::WriteFile(file_path, corrupted_contents_span));

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kWrongMagicNumber, 1);
}

TEST_F(SqlBackendImplTest, InitWithWrongSizeFakeIndexFile) {
  base::HistogramTester histogram_tester;
  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  const int32_t kWrongMagicNumber = 0xDEADBEEF;
  ASSERT_TRUE(
      base::WriteFile(file_path, base::byte_span_from_ref(kWrongMagicNumber)));

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kWrongFileSize, 1);
}

TEST_F(SqlBackendImplTest, InitWithOpenFileFailed) {
  const std::string expected_contents = GetExpectedFakeIndexContents();
  base::HistogramTester histogram_tester;
  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  ASSERT_TRUE(
      base::WriteFile(file_path, base::as_byte_span(expected_contents)));
  base::FilePermissionRestorer permission_restorer(file_path);
  // Make the file unreadable.
  ASSERT_TRUE(base::MakeFileUnreadable(file_path));

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kOpenFileFailed, 1);
}

TEST_F(SqlBackendImplTest, InitWithCreateFileFailed) {
  base::HistogramTester histogram_tester;
  base::FilePermissionRestorer permission_restorer(temp_dir_.GetPath());
  // Make the directory unwrittable.
  ASSERT_TRUE(base::MakeFileUnwritable(temp_dir_.GetPath()));

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kCreateFileFailed, 1);
}

TEST_F(SqlBackendImplTest, InitWithFailedToCreateDirectory) {
  base::HistogramTester histogram_tester;
  base::FilePath cache_dir =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("cache"));
  // Create a file where the cache directory is supposed to be, to simulate a
  // directory creation failure.
  ASSERT_TRUE(base::WriteFile(cache_dir, ""));

  auto backend = std::make_unique<SqlBackendImpl>(cache_dir, kDefaultMaxBytes,
                                                  net::CacheType::DISK_CACHE);
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::ERR_FAILED);
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.FakeIndexFileError",
      FakeIndexFileError::kFailedToCreateDirectory, 1);
}

TEST_F(SqlBackendImplTest, MaxFileSizeSmallMax) {
  const int64_t kMaxBytes = 10 * 1024 * 1024;
  auto backend = CreateBackendAndInit(kMaxBytes);
  EXPECT_EQ(backend->MaxFileSize(), kSqlBackendMinFileSizeLimit);
}

TEST_F(SqlBackendImplTest, MaxFileSizeCalculation) {
  const int64_t kLargeMaxBytes = 100 * 1024 * 1024;
  auto backend = CreateBackendAndInit(kLargeMaxBytes);
  EXPECT_EQ(backend->MaxFileSize(),
            kLargeMaxBytes / kSqlBackendMaxFileRatioDenominator);
}

TEST_F(SqlBackendImplTest, GetStats) {
  auto backend = CreateBackendAndInit();
  base::StringPairs stats;
  backend->GetStats(&stats);
  EXPECT_THAT(stats, ElementsAre(Pair("Cache type", "SQL Cache")));
}

// Tests a race condition where an entry is doomed via `SqlEntryImpl::Doom()`
// while an iterator is in the process of opening it. The iterator should still
// successfully open the entry, but the entry should be marked as doomed. This
// works because `OpenNextEntry` is an exclusive operation that runs before the
// normal `Doom` operation.
TEST_F(SqlBackendImplTest, IteratorParallelEntryDoom) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry1 = create_result.ReleaseEntry();

  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  entry1->Doom();

  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  auto* entry2 = result_iter.ReleaseEntry();

  EXPECT_EQ(entry1, entry2);
  EXPECT_TRUE((static_cast<SqlEntryImpl*>(entry1))->doomed());

  entry1->Close();
  entry2->Close();
}

// Tests a race condition where an entry is doomed and closed while an iterator
// is opening it. The iterator should still get a handle to the doomed entry.
// This verifies that the backend correctly manages the lifecycle of an entry
// that is being operated on by multiple asynchronous calls.
TEST_F(SqlBackendImplTest, IteratorParallelEntryDoomAndClose) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry = create_result.ReleaseEntry();

  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  entry->Doom();
  // Doom() is asynchronous. The entry is not marked as doomed until the
  // callback of OpenNextEntry is called.
  EXPECT_FALSE((static_cast<SqlEntryImpl*>(entry))->doomed());
  entry->Close();

  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());

  entry = result_iter.ReleaseEntry();

  EXPECT_TRUE((static_cast<SqlEntryImpl*>(entry))->doomed());
  entry->Close();
}

// Tests that the iterator correctly skips over an entry that is doomed before
// the `OpenNextEntry` operation is executed.
TEST_F(SqlBackendImplTest, IteratorParallelEntryDoomOpenNext) {
  auto backend = CreateBackendAndInit();

  // Create the first entry.
  TestEntryResultCompletionCallback cb_create1;
  disk_cache::EntryResult create_result1 = cb_create1.GetResult(
      backend->CreateEntry("key1", net::HIGHEST, cb_create1.callback()));
  ASSERT_THAT(create_result1.net_error(), IsOk());
  create_result1.ReleaseEntry()->Close();

  // Create the second entry.
  TestEntryResultCompletionCallback cb_create2;
  disk_cache::EntryResult create_result2 = cb_create2.GetResult(
      backend->CreateEntry("key2", net::HIGHEST, cb_create2.callback()));
  ASSERT_THAT(create_result2.net_error(), IsOk());
  auto* entry = create_result2.ReleaseEntry();

  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb_iter;

  entry->Doom();
  entry->Close();

  // `entry->Doom()` is called before `OpenNextEntry()` is initiated.
  // The iterator starts from the newest entry, which is `key2`. However, `key2`
  // is doomed before the iterator's `OpenNextEntry` operation is posted. The
  // iterator should detect that `key2` is doomed in the database and skip it,
  // returning `key1` instead.
  EntryResult result =
      cb_iter.GetResult(iter->OpenNextEntry(cb_iter.callback()));
  ASSERT_THAT(result.net_error(), IsOk());
  entry = result.ReleaseEntry();
  EXPECT_EQ(entry->GetKey(), "key1");
  entry->Close();

  // There should be no more entries.
  EntryResult result2 =
      cb_iter.GetResult(iter->OpenNextEntry(cb_iter.callback()));
  ASSERT_THAT(result2.net_error(), IsError(net::ERR_FAILED));
}

// Tests a race condition between an iterator opening an entry and a direct call
// to `Backend::DoomEntry`.
TEST_F(SqlBackendImplTest, IteratorParallelDoom) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry and write some data to it.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry1 = create_result.ReleaseEntry();
  const std::string kData = "some data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  net::TestCompletionCallback cb_write;
  int rv_write = entry1->WriteData(1, 0, buffer.get(), buffer->size(),
                                   cb_write.callback(), false);
  EXPECT_EQ(cb_write.GetResult(rv_write), static_cast<int>(buffer->size()));

  entry1->Close();

  // 2. Start opening the entry via an iterator. This is an async operation.
  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  // 3. Immediately call `DoomEntry` for the same key. This is also async.
  net::TestCompletionCallback cb_doom;
  int rv_doom = backend->DoomEntry("key", net::HIGHEST, cb_doom.callback());
  EXPECT_EQ(net::OK, cb_doom.GetResult(rv_doom));

  // `OpenNextEntry()` is an exclusive operation, while `DoomEntry()` is a
  // normal operation. Since `OpenNextEntry()` is posted first, it will run
  // before the `DoomEntry()` operation, which gets queued. After the iterator
  // returns the entry, the `DoomEntry()` operation runs and marks the entry as
  // doomed.
  // 4. Wait for the iterator to finish opening the entry.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  auto* entry = result_iter.ReleaseEntry();
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());

  // 5. Verify that the data can still be read from the doomed entry.
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), static_cast<int>(kData.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), kData.size()), kData);
  entry->Close();
}

// Tests a race condition between an iterator opening an entry and a call to
// `Backend::DoomAllEntries`.
TEST_F(SqlBackendImplTest, IteratorParallelDoomAll) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry and write some data to it.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry1 = create_result.ReleaseEntry();
  const std::string kData = "some data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  net::TestCompletionCallback cb_write;
  int rv_write = entry1->WriteData(1, 0, buffer.get(), buffer->size(),
                                   cb_write.callback(), false);
  EXPECT_EQ(cb_write.GetResult(rv_write), static_cast<int>(buffer->size()));
  entry1->Close();

  // 2. Start opening the entry via an iterator. This is an async operation.
  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  // 3. Immediately call `DoomAllEntries`. This is also an async operation.
  net::TestCompletionCallback cb_doom;
  int rv_doom = backend->DoomAllEntries(cb_doom.callback());
  EXPECT_EQ(net::OK, cb_doom.GetResult(rv_doom));

  // Both `DoomAllEntries()` and `OpenNextEntry()` are exclusive operations and
  // are serialized. Since `OpenNextEntry()` is posted first, it will run
  // first, retrieving the entry. Then, `DoomAllEntries()` will run and doom all
  // entries, including the one just opened.
  // 4. Wait for the iterator to finish opening the entry.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  auto* entry = result_iter.ReleaseEntry();
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());

  // 5. Verify that the data can still be read from the doomed entry.
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), static_cast<int>(kData.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), kData.size()), kData);
  entry->Close();
}

// Tests that an entry's `last_used` time is updated correctly when data is
// written and the entry is closed, even if an iterator is concurrently active.
// Also verifies the written data can be read back.
TEST_F(SqlBackendImplTest, IteratorParallelWriteDataAndClose) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry = create_result.ReleaseEntry();

  // Advance clock to ensure `last_used` time is distinct from creation.
  task_environment_.AdvanceClock(base::Minutes(1));

  // Create an iterator and attempt to open the entry concurrently.
  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  // Record the time when data is written. This should be the new `last_used`
  // time.
  const base::Time kWriteTime = base::Time::Now();

  // Write data to stream 0 and close the entry.
  const std::string kHeadData = "header_data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kHeadData);
  net::TestCompletionCallback cb_write;
  int rv_write = entry->WriteData(0, 0, buffer.get(), buffer->size(),
                                  cb_write.callback(), false);
  entry->Close();
  EXPECT_EQ(cb_write.GetResult(rv_write), buffer->size());

  // Get the result from the iterator's open operation.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  entry = result_iter.ReleaseEntry();
  // Verify that the `last_used` time of the opened entry reflects the write
  // time.
  EXPECT_THAT(entry->GetLastUsed(), kWriteTime);

  // Read the data back from the entry opened via the iterator.
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kHeadData.size() * 2);
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(0, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), kHeadData.size());
  entry->Close();
}

// Tests that an entry's `body_end` is updated correctly when data is written to
// stream 1 and the entry is closed, even if an iterator is concurrently active.
// Also verifies the written data can be read back.
TEST_F(SqlBackendImplTest, IteratorParallelWriteBodyDataAndClose) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry = create_result.ReleaseEntry();

  // Create an iterator and attempt to open the entry concurrently.
  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  // Write data to stream 1 and close the entry.
  const std::string kBodyData = "body_data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kBodyData);
  net::TestCompletionCallback cb_write;
  int rv_write = entry->WriteData(1, 0, buffer.get(), buffer->size(),
                                  cb_write.callback(), false);
  entry->Close();
  EXPECT_EQ(cb_write.GetResult(rv_write), static_cast<int>(buffer->size()));

  // Get the result from the iterator's open operation.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  entry = result_iter.ReleaseEntry();
  // Verify that the `body_end` of the opened entry reflects the write.
  EXPECT_EQ(entry->GetDataSize(1), static_cast<int32_t>(kBodyData.size()));

  // Read the data back from the entry opened via the iterator.
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kBodyData.size() * 2);
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), static_cast<int>(kBodyData.size()));
  EXPECT_EQ(std::string_view(read_buffer->data(), kBodyData.size()), kBodyData);
  entry->Close();
}

// Tests that an entry's `last_used` time is updated correctly when data is read
// and the entry is closed, even if an iterator is concurrently active.
TEST_F(SqlBackendImplTest, IteratorParallelReadDataAndClose) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry = create_result.ReleaseEntry();

  // Advance clock to ensure `last_used` time is distinct from creation.
  task_environment_.AdvanceClock(base::Minutes(1));

  // Create an iterator and attempt to open the entry concurrently.
  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  // Record the time when data is read. This should be the new `last_used` time.
  const base::Time kReadTime = base::Time::Now();

  // Read data from stream 0 and close the entry.
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  net::TestCompletionCallback cb_read;
  int rv_read = entry->ReadData(0, 0, read_buffer.get(), read_buffer->size(),
                                cb_read.callback());
  EXPECT_EQ(cb_read.GetResult(rv_read), 0);
  entry->Close();

  // Get the result from the iterator's open operation.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  entry = result_iter.ReleaseEntry();

  // Verify that the `last_used` time of the opened entry reflects the read
  // time.
  EXPECT_THAT(entry->GetLastUsed(), kReadTime);
  entry->Close();
}

// Tests a race condition where an entry is opened simultaneously by an iterator
// and a direct `OpenEntry` call. The backend should correctly handle this by
// returning the same `SqlEntryImpl` instance for both operations, preventing
// duplicate in-memory representations of the same cache entry.
TEST_F(SqlBackendImplTest, IteratorAndOpenEntryParallelRace) {
  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  create_result.ReleaseEntry()->Close();

  base::test::TestFuture<EntryResult> future_iter;
  base::test::TestFuture<EntryResult> future_open;

  auto iter = backend->CreateIterator();
  // Start opening the entry via the iterator. This posts an async task.
  ASSERT_EQ(iter->OpenNextEntry(future_iter.GetCallback()).net_error(),
            net::ERR_IO_PENDING);

  // Immediately try to open the same entry directly. This also posts an async
  // task.
  ASSERT_EQ(backend->OpenEntry("key", net::HIGHEST, future_open.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);

  // 3. Wait for both operations to complete. This test is designed to expose a
  // race condition. The backend should handle this race correctly by ensuring
  // only one `SqlEntryImpl` is created for the same key.
  EntryResult iter_res = future_iter.Take();
  EntryResult open_res = future_open.Take();

  ASSERT_THAT(iter_res.net_error(), IsOk());
  ASSERT_THAT(open_res.net_error(), IsOk());
  auto* entry1 = iter_res.ReleaseEntry();
  auto* entry2 = open_res.ReleaseEntry();

  // Both the iterator and the direct open operation should resolve to the same
  // underlying `SqlEntryImpl` instance. The backend's logic for managing
  // active entries should prevent the creation of a second instance for the
  // same key.
  EXPECT_EQ(entry1, entry2);
  entry1->Close();
  entry2->Close();
}

// Tests a race condition where an entry is opened by an iterator, opened by a
// direct call, and doomed, all in parallel.
// The exclusive `OpenNextEntry` operation runs first. The normal `OpenEntry`
// and `DoomEntry` operations are queued and serialized by key. `OpenEntry`
// runs next, getting a reference to the active entry. Finally, `DoomEntry`
// runs and marks that same entry instance as doomed.
TEST_F(SqlBackendImplTest, IteratorAndOpenEntryAndDoomParallelRace) {
  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  create_result.ReleaseEntry()->Close();

  base::test::TestFuture<EntryResult> future_iter;
  base::test::TestFuture<EntryResult> future_open;
  base::test::TestFuture<int> future_doom;

  auto iter = backend->CreateIterator();
  ASSERT_EQ(iter->OpenNextEntry(future_iter.GetCallback()).net_error(),
            net::ERR_IO_PENDING);
  ASSERT_EQ(backend->OpenEntry("key", net::HIGHEST, future_open.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);
  ASSERT_EQ(backend->DoomEntry("key", net::HIGHEST, future_doom.GetCallback()),
            net::ERR_IO_PENDING);

  EntryResult iter_res = future_iter.Take();
  ASSERT_THAT(iter_res.net_error(), IsOk());
  auto* entry1 = iter_res.ReleaseEntry();

  EntryResult open_res = future_open.Take();
  ASSERT_THAT(open_res.net_error(), IsOk());
  auto* entry2 = open_res.ReleaseEntry();

  EXPECT_EQ(entry1, entry2);
  EXPECT_EQ(future_doom.Take(), net::OK);

  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry1)->doomed());

  entry1->Close();
  entry2->Close();
}

// Tests a race condition where an entry is opened via `OpenEntry` while it is
// simultaneously being opened and then doomed by an iterator.
TEST_F(SqlBackendImplTest, OpenEntryRacesWithIteratorAndDoom) {
  // This test simulates a race condition to verify the interaction between
  // opening an entry directly and an iterator that dooms the same entry in its
  // callback. The exclusive `OpenNextEntry` operation runs first. Its callback
  // then posts a normal `Doom` operation. The `OpenEntry` call (also a normal
  // operation) was posted before the `Doom` operation. Due to serialization by
  // key, `OpenEntry` gets a reference to the active entry first, and then the
  // `Doom` operation marks that same instance as doomed.
  auto backend = CreateBackendAndInit();

  // 1. Create an entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  create_result.ReleaseEntry()->Close();

  auto iter = backend->CreateIterator();
  // 2. Start opening the entry via an iterator. This is an async operation.
  ASSERT_EQ(iter->OpenNextEntry(
                    base::BindLambdaForTesting([&](EntryResult entry_result) {
                      // 4. Once the iterator gets the entry, doom and close it.
                      auto* entry = entry_result.ReleaseEntry();
                      entry->Doom();
                      entry->Close();
                    }))
                .net_error(),
            net::ERR_IO_PENDING);

  // 3. While the iterator operation is in-flight, attempt to open the same
  //    entry directly. This is also an async operation.
  base::test::TestFuture<EntryResult> open_future;
  ASSERT_EQ(backend->OpenEntry("key", net::HIGHEST, open_future.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);

  // 5. Wait for the direct `OpenEntry` call to complete.
  EntryResult open_result = open_future.Take();
  ASSERT_THAT(open_result.net_error(), IsOk());
  auto* entry = open_result.ReleaseEntry();
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());
  entry->Close();
}

// Tests a race condition where an entry is opened via `OpenOrCreateEntry` while
// it is simultaneously being opened and then doomed by an iterator.
// `OpenOrCreateEntry` should find the existing entry (which is being opened by
// the iterator) and not create a new one. The test verifies that the returned
// entry is correctly marked as doomed, demonstrating proper serialization and
// state management.
TEST_F(SqlBackendImplTest, OpenOrCreateEntryEntryRacesWithIteratorAndDoom) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry and record its creation time.
  base::Time first_entry_creation_time = base::Time::Now();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  EXPECT_EQ(entry->GetLastUsed(), first_entry_creation_time);
  entry->Close();

  task_environment_.AdvanceClock(base::Minutes(1));

  auto iter = backend->CreateIterator();
  // 2. Start opening the entry via an iterator. This is an async operation.
  ASSERT_EQ(iter->OpenNextEntry(
                    base::BindLambdaForTesting([&](EntryResult entry_result) {
                      // 4. Once the iterator gets the entry, doom and close it.
                      auto* entry = entry_result.ReleaseEntry();
                      EXPECT_EQ(entry->GetLastUsed(),
                                first_entry_creation_time);
                      entry->Doom();
                      entry->Close();
                    }))
                .net_error(),
            net::ERR_IO_PENDING);

  base::test::TestFuture<EntryResult> open_or_create_future;
  // 3. While the iterator operation is in-flight, call `OpenOrCreateEntry` for
  //    the same key.
  ASSERT_EQ(backend
                ->OpenOrCreateEntry("key", net::HIGHEST,
                                    open_or_create_future.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);

  EntryResult open_or_create_result = open_or_create_future.Take();
  ASSERT_THAT(open_or_create_result.net_error(), IsOk());
  entry = open_or_create_result.ReleaseEntry();
  EXPECT_EQ(entry->GetLastUsed(), first_entry_creation_time);
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());
  entry->Close();
}

// Tests a race condition where an entry is opened via `OpenEntry` while it is
// simultaneously being opened, written to, and closed by an iterator.
// This test verifies that in-flight modifications (like `last_used`
// time and header data updates) that are queued while an entry is not active
// are correctly applied to the entry's in-memory representation when it is
// next opened. This ensures that subsequent operations on the entry see the
// most up-to-date state.
TEST_F(SqlBackendImplTest, OpenEntryRacesWithIteratorAndWriteData) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry and record its creation time.
  base::Time first_entry_creation_time = base::Time::Now();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  EXPECT_EQ(entry->GetLastUsed(), first_entry_creation_time);
  entry->Close();

  task_environment_.AdvanceClock(base::Minutes(1));

  const std::string kHeadData = "header_data";
  // 2. Start opening the entry via an iterator. This is an async operation.
  auto iter = backend->CreateIterator();
  ASSERT_EQ(iter->OpenNextEntry(
                    base::BindLambdaForTesting([&](EntryResult entry_result) {
                      // 4. Once the iterator gets the entry, write data to it
                      //    and close it. This updates the entry's `last_used`
                      //    time and header data in memory, and queues a write
                      //    to the persistent store.
                      auto* entry = entry_result.ReleaseEntry();
                      EXPECT_EQ(entry->GetLastUsed(),
                                first_entry_creation_time);
                      auto buffer =
                          base::MakeRefCounted<net::StringIOBuffer>(kHeadData);
                      entry->WriteData(0, 0, buffer.get(), buffer->size(),
                                       base::DoNothing(), false);
                      entry->Close();
                    }))
                .net_error(),
            net::ERR_IO_PENDING);

  // 3. While the iterator operation is in-flight, attempt to open the same
  //    entry directly.
  base::test::TestFuture<EntryResult> open_future;
  ASSERT_EQ(backend->OpenEntry("key", net::HIGHEST, open_future.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);

  // 5. The `OpenEntry` operation should succeed. The backend should handle the
  //    race by applying the in-flight modifications (from the iterator's
  //    write and close) to the entry data before returning the new entry
  //    handle.
  EntryResult open_result = open_future.Take();
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();
  // The `last_used` time should reflect the time of the write.
  EXPECT_EQ(entry->GetLastUsed(), first_entry_creation_time + base::Minutes(1));
  // The data written by the iterator should be readable.
  auto buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kHeadData.size() * 2);
  ASSERT_EQ(
      entry->ReadData(0, 0, buffer.get(), buffer->size(), base::DoNothing()),
      kHeadData.size());
  EXPECT_EQ(buffer->first(kHeadData.size()), base::as_byte_span(kHeadData));
  entry->Close();
}

// Tests that OnExternalCacheHit correctly updates the last_used time, even when
// an OpenEntry operation is in-flight.
TEST_F(SqlBackendImplTest, OnExternalCacheHitRacesWithOpen) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry and close it.
  const std::string kKey = "my-key";
  TestEntryResultCompletionCallback create_cb;
  disk_cache::EntryResult create_result = create_cb.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, create_cb.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* created_entry = create_result.ReleaseEntry();
  base::Time create_time = created_entry->GetLastUsed();
  created_entry->Close();

  // 2. Advance time.
  task_environment_.AdvanceClock(base::Minutes(1));

  // 3. Start opening the entry. This is an async operation.
  base::test::TestFuture<EntryResult> open_future;
  ASSERT_EQ(backend->OpenEntry(kKey, net::HIGHEST, open_future.GetCallback())
                .net_error(),
            net::ERR_IO_PENDING);

  // 4. Call OnExternalCacheHit.
  base::Time hit_time = base::Time::Now();
  EXPECT_NE(create_time, hit_time);
  backend->OnExternalCacheHit(kKey);

  // 5. Wait for OpenEntry to complete.
  EntryResult open_result = open_future.Take();
  ASSERT_THAT(open_result.net_error(), IsOk());
  auto* entry = open_result.ReleaseEntry();

  // 6. The entry's last_used time should be the time of the external hit.
  EXPECT_EQ(entry->GetLastUsed(), hit_time);
  entry->Close();
}

TEST_F(SqlBackendImplTest, DoomEntryNonExistent) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  const std::string kNonExistentKey = "non-existent-key";
  net::TestCompletionCallback cb_doom;
  int rv_doom =
      backend->DoomEntry(kNonExistentKey, net::HIGHEST,
                         base::BindOnce([](int rv) { NOTREACHED(); }));
  // The operation should complete synchronously due to the in-memory index
  // check, so the callback should not be reached if the DB operation were to
  // be attempted.
  EXPECT_EQ(net::OK, rv_doom);
}

// Tests that calling Doom() multiple times on the same entry is safe and
// idempotent.
TEST_F(SqlBackendImplTest, MultipleDoomsOnSameEntry) {
  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Doom the entry multiple times. This should not cause any issues.
  entry->Doom();
  entry->Doom();

  // When the entry was created speculatively, the doomed flag is updated
  // asynchronously. So need to flush the pending database operations.
  FlushQueue(*backend);

  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());
  entry->Close();

  // Verify that the entry is gone after being doomed and closed.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry("key", net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

// Tests that recursive calls to OpenNextEntry from within its callback do not
// starve normal operations. The ExclusiveOperationCoordinator's sequence-based
// scheduling ensures that the older normal operation (CreateEntry) is executed
// before the newer exclusive operation (the second OpenNextEntry).
TEST_F(SqlBackendImplTest, RecursiveOpenNextEntry) {
  auto backend = CreateBackendAndInit();

  // Create two entries to iterate over.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result1 = cb_create.GetResult(
      backend->CreateEntry("key1", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result1.net_error(), IsOk());
  create_result1.ReleaseEntry()->Close();

  disk_cache::EntryResult create_result2 = cb_create.GetResult(
      backend->CreateEntry("key2", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result2.net_error(), IsOk());
  create_result2.ReleaseEntry()->Close();

  auto iter = backend->CreateIterator();

  base::RunLoop run_loop;
  Entry* entry3 = nullptr;

  bool key_1_found = false;
  bool key_2_found = false;

  // The first call to OpenNextEntry. Inside its callback, we'll trigger the
  // second call.
  ASSERT_THAT(
      iter->OpenNextEntry(base::BindLambdaForTesting([&](EntryResult result1) {
            ASSERT_THAT(result1.net_error(), IsOk());
            auto* entry1_itr = result1.ReleaseEntry();
            key_1_found = entry1_itr->GetKey() == "key1";
            key_2_found = entry1_itr->GetKey() == "key2";
            EXPECT_TRUE(key_1_found || key_2_found);
            entry1_itr->Close();
            // Now, make the recursive call to OpenNextEntry.
            ASSERT_THAT(
                iter->OpenNextEntry(
                        base::BindLambdaForTesting([&](EntryResult result2) {
                          ASSERT_THAT(result2.net_error(), IsOk());
                          // By this point, the CreateEntry for "key3" should
                          // have completed, proving that the normal operation
                          // was not starved.
                          CHECK(entry3);

                          auto* entry2_itr = result2.ReleaseEntry();
                          if (entry2_itr->GetKey() == "key3") {
                            EXPECT_EQ(entry2_itr, entry3);
                          } else {
                            if (key_1_found) {
                              EXPECT_EQ(entry2_itr->GetKey(), "key2");
                            } else {
                              EXPECT_EQ(entry2_itr->GetKey(), "key1");
                            }
                          }
                          entry2_itr->Close();
                          entry3->Close();
                          run_loop.Quit();
                        }))
                    .net_error(),
                IsError(net::ERR_IO_PENDING));
          }))
          .net_error(),
      IsError(net::ERR_IO_PENDING));

  // While the first OpenNextEntry is in flight, post a normal operation to
  // create a third entry. This tests that the recursive exclusive operations
  // do not starve the normal one.
  ASSERT_THAT(
      backend
          ->CreateEntry("key3", net::HIGHEST,
                        base::BindLambdaForTesting([&](EntryResult result3) {
                          ASSERT_THAT(result3.net_error(), IsOk());
                          entry3 = result3.ReleaseEntry();
                        }))
          .net_error(),
      IsError(net::ERR_IO_PENDING));
  run_loop.Run();
}

// Tests that recursive calls to OpenNextEntry from within its callback do not
// starve normal operations, even when one of the iterated entries is already
// active.
TEST_F(SqlBackendImplTest, RecursiveOpenNextEntryWithActiveEntry) {
  auto backend = CreateBackendAndInit();

  // Create one entry and close it.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result1 = cb_create.GetResult(
      backend->CreateEntry("key1", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result1.net_error(), IsOk());
  create_result1.ReleaseEntry()->Close();

  // Create a second entry and keep it active.
  disk_cache::EntryResult create_result2 = cb_create.GetResult(
      backend->CreateEntry("key2", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result2.net_error(), IsOk());
  auto* entry2_active = create_result2.ReleaseEntry();

  auto iter = backend->CreateIterator();

  base::RunLoop run_loop;
  Entry* entry3 = nullptr;

  bool key_1_found = false;
  bool key_2_found = false;

  // The first call to OpenNextEntry. Inside its callback, we'll trigger the
  // second call.
  ASSERT_THAT(
      iter->OpenNextEntry(base::BindLambdaForTesting([&](EntryResult result1) {
            ASSERT_THAT(result1.net_error(), IsOk());
            auto* entry1_iter = result1.ReleaseEntry();
            key_1_found = entry1_iter->GetKey() == "key1";
            key_2_found = entry1_iter->GetKey() == "key2";
            EXPECT_TRUE(key_1_found || key_2_found);
            if (key_2_found) {
              EXPECT_EQ(entry1_iter, entry2_active);
            }
            entry1_iter->Close();
            // Now, make the recursive call to OpenNextEntry.
            ASSERT_THAT(
                iter->OpenNextEntry(
                        base::BindLambdaForTesting([&](EntryResult result2) {
                          ASSERT_THAT(result2.net_error(), IsOk());
                          // By this point, the CreateEntry for "key3" should
                          // have completed, proving that the normal operation
                          // was not starved.
                          CHECK(entry3);

                          auto* entry2_itr = result2.ReleaseEntry();
                          if (entry2_itr->GetKey() == "key3") {
                            EXPECT_EQ(entry2_itr, entry3);
                          } else {
                            if (key_1_found) {
                              EXPECT_EQ(entry2_itr->GetKey(), "key2");
                            } else {
                              EXPECT_EQ(entry2_itr->GetKey(), "key1");
                            }
                          }
                          entry2_itr->Close();
                          entry3->Close();
                          run_loop.Quit();
                        }))
                    .net_error(),
                IsError(net::ERR_IO_PENDING));
          }))
          .net_error(),
      IsError(net::ERR_IO_PENDING));

  // While the first OpenNextEntry is in flight, post a normal operation to
  // create a third entry.
  ASSERT_THAT(
      backend
          ->CreateEntry("key3", net::HIGHEST,
                        base::BindLambdaForTesting([&](EntryResult result3) {
                          ASSERT_THAT(result3.net_error(), IsOk());
                          entry3 = result3.ReleaseEntry();
                        }))
          .net_error(),
      IsError(net::ERR_IO_PENDING));
  run_loop.Run();

  // Close the initially active entry.
  entry2_active->Close();
}

// Tests that if a pending ReadData operation is aborted (e.g., due to backend
// destruction), the callback is invoked with net::ERR_ABORTED.
TEST_F(SqlBackendImplTest, AbortPendingReadData) {
  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback create_cb;
  disk_cache::EntryResult create_result = create_cb.GetResult(
      backend->CreateEntry("key", net::HIGHEST, create_cb.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Write some data to stream 1 so that a subsequent read will be pending.
  const std::string kBodyData = "body_data";
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kBodyData);
  net::TestCompletionCallback write_cb;
  int write_rv =
      entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                       write_cb.callback(), false);
  ASSERT_EQ(write_cb.GetResult(write_rv),
            static_cast<int>(write_buffer->size()));

  // Initiate a ReadData operation, which will be pending.
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  base::test::TestFuture<int> read_future;
  int rv = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                           read_future.GetCallback());
  ASSERT_THAT(rv, IsError(net::ERR_IO_PENDING));

  // Destroy the backend while the read is in flight.
  backend.reset();

  // The callback should be aborted.
  EXPECT_EQ(read_future.Get(), net::ERR_ABORTED);

  entry->Close();
}

// Tests that if a pending WriteData operation is aborted (e.g., due to backend
// destruction), the callback is invoked with net::ERR_ABORTED.
TEST_F(SqlBackendImplTest, AbortPendingWriteData) {
  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback create_cb;
  disk_cache::EntryResult create_result = create_cb.GetResult(
      backend->CreateEntry("key", net::HIGHEST, create_cb.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  net::TestCompletionCallback flush_cb;
  backend->FlushQueueForTest(flush_cb.callback());
  EXPECT_THAT(flush_cb.WaitForResult(), IsOk());

  // Initiate a WriteData operation, which will be pending.
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  base::test::TestFuture<int> write_future;
  int rv = entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                            write_future.GetCallback(), false);
  ASSERT_THAT(rv, write_buffer->size());

  auto task_runners = backend->GetBackgroundTaskRunnersForTest();

  // Destroy the backend while the write is in flight.
  backend.reset();

  auto res_id_or_error = static_cast<SqlEntryImpl*>(entry)->res_id_or_error();
  while (!(res_id_or_error->data.has_value() &&
           std::holds_alternative<SqlPersistentStore::Error>(
               *res_id_or_error->data))) {
    FlushQueueInTaskRunners(task_runners);
  }

  // The res_id_or_error should have been set to aborted.
  EXPECT_EQ(std::get<SqlPersistentStore::Error>(*res_id_or_error->data),
            SqlPersistentStore::Error::kAborted);

  entry->Close();
}

// Tests that if a pending GetAvailableRange operation is aborted (e.g., due to
// backend destruction), the callback is invoked with net::ERR_ABORTED.
TEST_F(SqlBackendImplTest, AbortPendingGetAvailableRange) {
  auto backend = CreateBackendAndInit();

  // Create an entry.
  TestEntryResultCompletionCallback create_cb;
  disk_cache::EntryResult create_result = create_cb.GetResult(
      backend->CreateEntry("key", net::HIGHEST, create_cb.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // Initiate a GetAvailableRange operation, which will be pending.
  base::test::TestFuture<const RangeResult&> range_future;
  RangeResult result =
      entry->GetAvailableRange(0, 100, range_future.GetCallback());
  ASSERT_THAT(result.net_error, IsError(net::ERR_IO_PENDING));

  // Destroy the backend while the operation is in flight.
  backend.reset();

  // The callback should be aborted.
  const RangeResult& aborted_result = range_future.Get();
  EXPECT_THAT(aborted_result.net_error, IsError(net::ERR_ABORTED));

  entry->Close();
}

TEST_F(SqlBackendImplTest, DoomedEntriesCleanup) {
  // 1. Create a backend and add three entries with data.
  auto backend = CreateBackendAndInit();
  auto task_runners = backend->GetBackgroundTaskRunnersForTest();

  const std::string kKey1 = "key1";
  const std::string kKey2 = "key2";
  const std::string kKey3 = "key3";
  const std::string kData = "some data";

  auto* entry1 = CreateEntryAndWriteData(backend.get(), kKey1, kData);
  auto* entry2 = CreateEntryAndWriteData(backend.get(), kKey2, kData);
  auto* entry3 = CreateEntryAndWriteData(backend.get(), kKey3, kData);
  WaitUntilInitialized(*backend,
                       static_cast<SqlEntryImpl*>(entry3)->res_id_or_error());
  auto res_id = std::get<SqlPersistentStore::ResId>(
      static_cast<SqlEntryImpl*>(entry3)->res_id_or_error()->data.value());
  entry1->Close();
  entry2->Close();
  entry3->Close();

  backend.reset();

  FlushQueueInTaskRunners(task_runners);

  // 2. Open the database directly via SqlPersistentStore and doom the third
  // entry.
  {
    auto store = std::make_unique<SqlPersistentStore>(
        temp_dir_.GetPath(), kDefaultMaxBytes, net::CacheType::DISK_CACHE,
        task_runners);

    base::test::TestFuture<disk_cache::SqlPersistentStore::Error> future_init;
    store->Initialize(future_init.GetCallback());
    ASSERT_EQ(future_init.Get(), disk_cache::SqlPersistentStore::Error::kOk);

    base::test::TestFuture<SqlPersistentStore::Error> future_doom;
    store->DoomEntry(CacheEntryKey(kKey3), res_id, future_doom.GetCallback());
    EXPECT_EQ(future_doom.Get(), SqlPersistentStore::Error::kOk);

    store.reset();
  }

  FlushQueueInTaskRunners(task_runners);

  // 3. Recreate the backend
  backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  // 4. Open and doom the first and the second entries and let them as active.
  TestEntryResultCompletionCallback cb_open1;
  disk_cache::EntryResult open_result1 = cb_open1.GetResult(
      backend->OpenEntry(kKey1, net::HIGHEST, cb_open1.callback()));
  entry1 = open_result1.ReleaseEntry();
  entry1->Doom();

  TestEntryResultCompletionCallback cb_open2;
  disk_cache::EntryResult open_result2 = cb_open2.GetResult(
      backend->OpenEntry(kKey2, net::HIGHEST, cb_open2.callback()));
  entry2 = open_result2.ReleaseEntry();
  entry2->Doom();

  base::HistogramTester histogram_tester;
  backend->OnBrowserIdle();

  // Flush the queue to ensure that cleanup task is completed.
  net::TestCompletionCallback flush_cb;
  backend->FlushQueueForTest(flush_cb.callback());
  EXPECT_THAT(flush_cb.WaitForResult(), IsOk());

  // Verify that `DeleteDoomedEntriesCount` UMA was recorded in the histogram.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.DeleteDoomedEntriesCount", 1, 1);

  // 5. Verify that the data can still be read from the doomed entry.
  ReadAndVerifyData(entry1, kData);
  ReadAndVerifyData(entry2, kData);

  entry1->Close();
  entry2->Close();
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntry) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  const std::string kKey = "my-key";

  // 1. Create an entry. This should return immediately with a speculatively
  //    created entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  // 2. The res_id should not be available yet.
  auto* sql_entry = static_cast<SqlEntryImpl*>(entry);
  EXPECT_FALSE(sql_entry->res_id_or_error()->data.has_value());

  // 3. Wait for the database operation to complete.
  WaitUntilInitialized(*backend, sql_entry->res_id_or_error());

  // 4. Now the res_id should be available.
  ASSERT_TRUE(sql_entry->res_id_or_error()->data.has_value());
  EXPECT_TRUE(std::holds_alternative<SqlPersistentStore::ResId>(
      sql_entry->res_id_or_error()->data.value()));

  // 5. Doom the entry.
  entry->Doom();
  EXPECT_TRUE(sql_entry->doomed());
  entry->Close();

  // 6. Verify that the entry is gone.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntrySyncClose) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  entry->Close();

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();
  ASSERT_TRUE(entry);
  entry->Close();
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntrySyncDoom) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  entry->Doom();
  entry->Close();

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntrySyncWrite) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";
  const std::string kData = "some data";

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  net::TestCompletionCallback write_cb;
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  EXPECT_EQ(write_cb.GetResult(entry->WriteData(1, 0, write_buffer.get(),
                                                write_buffer->size(),
                                                write_cb.callback(), false)),
            static_cast<int>(write_buffer->size()));

  entry->Close();

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);

  base::test::TestFuture<int> read_future;
  int rv = entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                           read_future.GetCallback());
  ASSERT_THAT(rv, IsError(net::ERR_IO_PENDING));
  ASSERT_EQ(read_future.Get(), write_buffer->size());

  EXPECT_EQ(std::string_view(read_buffer->data(), kData.size()), kData);
  entry->Close();
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryWithDbFailure) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  const std::string kKey = "my-key";

  // 1. Create an entry. This should return immediately with a speculatively
  //    created entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  // 2. The res_id should not be available yet.
  auto* sql_entry = static_cast<SqlEntryImpl*>(entry);
  EXPECT_FALSE(sql_entry->res_id_or_error()->data.has_value());

  // 3. Wait for the database operation to complete.
  WaitUntilInitialized(*backend, sql_entry->res_id_or_error());

  // 4. Now the res_id should be available and hold a kFailedForTesting.
  ASSERT_TRUE(sql_entry->res_id_or_error()->data.has_value());
  ASSERT_TRUE(std::holds_alternative<SqlPersistentStore::Error>(
      sql_entry->res_id_or_error()->data.value()));
  EXPECT_EQ(std::get<SqlPersistentStore::Error>(
                sql_entry->res_id_or_error()->data.value()),
            SqlPersistentStore::Error::kFailedForTesting);

  // 5. Doom the entry.
  entry->Doom();
  EXPECT_TRUE(sql_entry->doomed());
  entry->Close();

  // 6. Verify that the entry is not found.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

TEST_F(SqlBackendImplTest,
       SpeculativeCreateEntryDbFailureOperationsBeforeErrorSet) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult entry_result =
      backend->CreateEntry("key", net::HIGHEST, cb.callback());
  ASSERT_THAT(entry_result.net_error(), IsOk());
  auto* entry = entry_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  EXPECT_EQ(
      entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                       base::BindOnce([](int rv) { NOTREACHED(); }), false),
      write_buffer->size());

  net::TestCompletionCallback read_cb;
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  EXPECT_EQ(entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                            read_cb.callback()),
            net::ERR_IO_PENDING);

  base::test::TestFuture<const RangeResult&> range_future;
  EXPECT_EQ(
      entry->GetAvailableRange(0, 10, range_future.GetCallback()).net_error,
      net::ERR_IO_PENDING);

  EXPECT_THAT(read_cb.WaitForResult(), IsError(net::ERR_FAILED));
  EXPECT_THAT(range_future.Get().net_error, IsError(net::ERR_FAILED));
  entry->Close();
}

TEST_F(SqlBackendImplTest,
       SpeculativeCreateEntryDbFailureOperationsAfterErrorSet) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult entry_result =
      backend->CreateEntry("key", net::HIGHEST, cb.callback());
  ASSERT_THAT(entry_result.net_error(), IsOk());
  auto* entry = entry_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  auto* sql_entry = static_cast<SqlEntryImpl*>(entry);
  WaitUntilInitialized(*backend, sql_entry->res_id_or_error());
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  EXPECT_EQ(entry->WriteData(0, 0, write_buffer.get(), write_buffer->size(),
                             base::DoNothing(), false),
            static_cast<int>(write_buffer->size()));

  net::TestCompletionCallback write_cb;
  EXPECT_EQ(
      entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                       base::BindOnce([](int rv) { NOTREACHED(); }), false),
      net::ERR_FAILED);

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  EXPECT_EQ(entry->ReadData(1, 0, read_buffer.get(), read_buffer->size(),
                            base::BindOnce([](int) { NOTREACHED(); })),
            net::ERR_FAILED);

  EXPECT_EQ(
      entry
          ->GetAvailableRange(
              0, 10, base::BindOnce([](const RangeResult&) { NOTREACHED(); }))
          .net_error,
      net::ERR_FAILED);
  EXPECT_EQ(backend->DoomEntry("key", net::HIGHEST,
                               base::BindOnce([](int) { NOTREACHED(); })),
            net::ERR_FAILED);
  entry->Close();

  EXPECT_EQ(backend->GetSizeOfInFlightEntryModificationsMapForTesting(), 0u);
}

TEST_F(SqlBackendImplTest, SpeculativeCreateEntryDbFailureDoom) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult entry_result =
      backend->CreateEntry("key", net::HIGHEST, cb.callback());
  ASSERT_THAT(entry_result.net_error(), IsOk());
  auto* entry = entry_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  auto* sql_entry = static_cast<SqlEntryImpl*>(entry);
  WaitUntilInitialized(*backend, sql_entry->res_id_or_error());

  entry->Doom();
  EXPECT_TRUE(static_cast<SqlEntryImpl*>(entry)->doomed());
  entry->Close();
  entry = nullptr;

  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry("key", net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

TEST_F(SqlBackendImplTest, OptimisticWriteBufferSize) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "100"}});

  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult entry_result =
      backend->CreateEntry("key", net::HIGHEST, cb.callback());
  ASSERT_THAT(entry_result.net_error(), IsOk());
  auto* entry = entry_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  // First write is smaller than the buffer size, should be optimistic.
  auto write_buffer1 = base::MakeRefCounted<net::StringIOBuffer>("data1");
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer1.get(), write_buffer1->size(),
                             base::DoNothing(), false),
            static_cast<int>(write_buffer1->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size());

  // Second write exceeds the buffer size, should be pending.
  auto write_buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(100, 'a'));
  net::TestCompletionCallback write_cb;
  EXPECT_EQ(entry->WriteData(1, write_buffer1->size(), write_buffer2.get(),
                             write_buffer2->size(), write_cb.callback(), false),
            net::ERR_IO_PENDING);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size());

  EXPECT_EQ(write_cb.WaitForResult(), static_cast<int>(write_buffer2->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  entry->Close();
}

TEST_F(SqlBackendImplTest, OptimisticWriteBufferLifecycle) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "100"}});

  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult entry_result =
      backend->CreateEntry("key", net::HIGHEST, cb.callback());
  ASSERT_THAT(entry_result.net_error(), IsOk());
  auto* entry = entry_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  // This write should be optimistic.
  auto write_buffer1 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(50, 'a'));
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer1.get(), write_buffer1->size(),
                             base::DoNothing(), false),
            static_cast<int>(write_buffer1->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size());

  // This write should also be optimistic, filling the buffer.
  auto write_buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(50, 'b'));
  EXPECT_EQ(entry->WriteData(1, 50, write_buffer2.get(), write_buffer2->size(),
                             base::DoNothing(), false),
            static_cast<int>(write_buffer2->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size() + write_buffer2->size());

  // This write should be pending as the buffer is full.
  auto write_buffer3 = base::MakeRefCounted<net::StringIOBuffer>("c");
  net::TestCompletionCallback write_cb3;
  EXPECT_EQ(entry->WriteData(1, 100, write_buffer3.get(), write_buffer3->size(),
                             write_cb3.callback(), false),
            net::ERR_IO_PENDING);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size() + write_buffer2->size());

  // Flush the queue. This will ensure the first two optimistic writes complete
  // on the background thread, which will free up the buffer and allow the
  // pending write to proceed.
  net::TestCompletionCallback flush_cb1;
  backend->FlushQueueForTest(flush_cb1.callback());
  EXPECT_THAT(flush_cb1.WaitForResult(), IsOk());

  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  // Now that the queue is flushed, the pending write should have completed.
  EXPECT_EQ(write_cb3.WaitForResult(), static_cast<int>(write_buffer3->size()));

  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);
  // The buffer should be free again, so this write should be optimistic.
  auto write_buffer4 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(50, 'd'));
  EXPECT_EQ(entry->WriteData(1, 101, write_buffer4.get(), write_buffer4->size(),
                             base::DoNothing(), false),
            static_cast<int>(write_buffer4->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer4->size());

  entry->Close();

  net::TestCompletionCallback flush_cb2;
  backend->FlushQueueForTest(flush_cb2.callback());
  EXPECT_THAT(flush_cb2.WaitForResult(), IsOk());

  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);
}

TEST_F(SqlBackendImplTest, OptimisticWriteFailure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "100"}});

  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  const std::string kKey = "my-key";
  const std::string kInitialData = "initial data";

  // 1. Create an entry with some data.
  auto* entry = CreateEntryAndWriteData(backend.get(), kKey, kInitialData);
  entry->Close();

  // 2. Re-open the entry.
  TestEntryResultCompletionCallback open_cb;
  disk_cache::EntryResult open_result = open_cb.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, open_cb.callback()));
  ASSERT_THAT(open_result.net_error(), IsOk());
  entry = open_result.ReleaseEntry();
  auto* sql_entry = static_cast<SqlEntryImpl*>(entry);

  // 3. Simulate a database failure for subsequent operations.
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);

  // 4. Perform an optimistic write, which should fail in the background.
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("new data");
  EXPECT_EQ(entry->WriteData(1, kInitialData.size(), write_buffer.get(),
                             write_buffer->size(), base::DoNothing(), false),
            static_cast<int>(write_buffer->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer->size());

  // 5. Disable failure simulation.
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(false);

  // 6. Wait for the background write to fail and update the entry's state.
  net::TestCompletionCallback flush_cb1;
  backend->FlushQueueForTest(flush_cb1.callback());
  EXPECT_THAT(flush_cb1.WaitForResult(), IsOk());

  // 7. Verify that the entry is now in an error state.
  ASSERT_TRUE(sql_entry->res_id_or_error()->data.has_value());
  ASSERT_TRUE(std::holds_alternative<SqlPersistentStore::Error>(
      sql_entry->res_id_or_error()->data.value()));
  EXPECT_EQ(std::get<SqlPersistentStore::Error>(
                sql_entry->res_id_or_error()->data.value()),
            SqlPersistentStore::Error::kFailedForTesting);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  // 8. Subsequent writes should fail immediately.
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                             base::DoNothing(), false),
            net::ERR_FAILED);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  entry->Close();

  // 9. Since the entry should have been deleted from storage, OpenEntry will
  //    fail.
  TestEntryResultCompletionCallback open_cb2;
  open_result = open_cb2.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, open_cb2.callback()));
  ASSERT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

TEST_F(SqlBackendImplTest, OptimisticWriteAfterSpeculativeCreateEntry) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  // 1. Enable failure simulation.
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);

  const std::string kKey = "my-key";

  // 2. Create an entry. This should return immediately with a speculatively
  //    created entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  // 2. Disable failure simulation.
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(false);

  // 3. This write should be optimistic, filling the buffer.
  auto write_buffer1 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(50, 'a'));
  EXPECT_EQ(entry->WriteData(1, 0, write_buffer1.get(), write_buffer1->size(),
                             base::DoNothing(), false),
            static_cast<int>(write_buffer1->size()));
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size());

  // 4. Wait for the background creation and write to fail and update the
  //    entry's state.
  net::TestCompletionCallback flush_cb;
  backend->FlushQueueForTest(flush_cb.callback());
  EXPECT_THAT(flush_cb.WaitForResult(), IsOk());

  auto* sql_entry = static_cast<SqlEntryImpl*>(entry);

  // 6. Verify that the entry is now in an error state.
  ASSERT_TRUE(sql_entry->res_id_or_error()->data.has_value());
  ASSERT_TRUE(std::holds_alternative<SqlPersistentStore::Error>(
      sql_entry->res_id_or_error()->data.value()));
  EXPECT_EQ(std::get<SqlPersistentStore::Error>(
                sql_entry->res_id_or_error()->data.value()),
            SqlPersistentStore::Error::kFailedForTesting);

  // 7. The buffer size should be set to 0.
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(), 0);

  entry->Close();
}

TEST_F(SqlBackendImplTest,
       SpeculativeCreateEntryDbFailureAndNonOptimisticWrite) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheOptimisticWriteBufferSize.name, "100"}});

  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));

  // Create the first entry.
  disk_cache::EntryResult entry_result1 = backend->CreateEntry(
      "key1", net::HIGHEST, base::BindOnce([](EntryResult) { NOTREACHED(); }));
  ASSERT_THAT(entry_result1.net_error(), IsOk());
  auto* entry1 = entry_result1.ReleaseEntry();
  ASSERT_TRUE(entry1);

  // Flush the queue to make sure the first entry is written to the database.
  net::TestCompletionCallback flush_cb;
  backend->FlushQueueForTest(flush_cb.callback());
  EXPECT_THAT(flush_cb.WaitForResult(), IsOk());

  // Check that the first entry has a valid resource ID.
  auto* sql_entry1 = static_cast<SqlEntryImpl*>(entry1);
  ASSERT_TRUE(sql_entry1->res_id_or_error()->data.has_value());
  ASSERT_TRUE(std::holds_alternative<SqlPersistentStore::ResId>(
      sql_entry1->res_id_or_error()->data.value()));

  // Simulate a database failure for subsequent operations.
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);

  // Attempt to create a second entry speculatively, but it will fail due to
  // the database failure.
  disk_cache::EntryResult entry_result2 = backend->CreateEntry(
      "key2", net::HIGHEST, base::BindOnce([](EntryResult) { NOTREACHED(); }));
  ASSERT_THAT(entry_result2.net_error(), IsOk());
  auto* entry2 = entry_result2.ReleaseEntry();
  ASSERT_TRUE(entry2);

  // Disable the database failure simulation.
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(false);

  // Write to the first entry. This should be an optimistic write.
  auto write_buffer1 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(100, 'a'));
  EXPECT_EQ(entry1->WriteData(1, 0, write_buffer1.get(), write_buffer1->size(),
                              base::BindOnce([](int) { NOTREACHED(); }), false),
            static_cast<int>(write_buffer1->size()));

  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size());

  // Write to the second entry. This should return ERR_IO_PENDING since the
  // buffer size exceeds kSqlDiskCacheOptimisticWriteBufferSize.
  net::TestCompletionCallback write_cb;
  auto write_buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(50, 'b'));
  EXPECT_EQ(entry2->WriteData(1, 0, write_buffer2.get(), write_buffer2->size(),
                              write_cb.callback(), false),
            net::ERR_IO_PENDING);
  EXPECT_EQ(backend->GetOptimisticWriteBufferTotalSizeForTesting(),
            write_buffer1->size());

  // The write operation should asynchronously fail.
  EXPECT_EQ(write_cb.GetResult(net::ERR_IO_PENDING), net::ERR_FAILED);

  entry1->Close();
  entry2->Close();
}

TEST_F(SqlBackendImplTest, IdleTimeEviction) {
  const int64_t kMaxBytes = 10000;
  const int64_t kIdleTimeHighWatermark =
      kMaxBytes * kSqlBackendIdleTimeEvictionHighWaterMarkPermille /
      1000;  // 9250
  auto buffer =
      base::MakeRefCounted<net::StringIOBuffer>(std::string(1000, 'x'));

  auto backend = CreateBackendAndInit(kMaxBytes);

  // Add entries to be above idle time watermark.
  int i = 0;
  while (GetSizeOfAllEntries(*backend) <= kIdleTimeHighWatermark) {
    TestEntryResultCompletionCallback cb;
    EntryResult result = cb.GetResult(backend->CreateEntry(
        base::StringPrintf("key%d", i++), net::HIGHEST, cb.callback()));
    ASSERT_THAT(result.net_error(), IsOk());
    auto* entry = result.ReleaseEntry();
    net::TestCompletionCallback write_cb;
    EXPECT_EQ(
        write_cb.GetResult(entry->WriteData(1, 0, buffer.get(), buffer->size(),
                                            write_cb.callback(), false)),
        buffer->size());
    entry->Close();
    FlushQueue(*backend);
  }

  auto test_helper = PerformanceScenarioTestHelper::Create();
  // Set the state to idle.
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kNoPageLoading);
  test_helper->SetInputScenario(ScenarioScope::kGlobal,
                                InputScenario::kNoInput);

  // Trigger idle time eviction.
  backend->OnBrowserIdle();

  // The eviction process involves multiple asynchronous steps across different
  // shards. The first FlushQueue ensures that all shards have processed their
  // eviction candidates and posted their results to the
  // EvictionCandidateAggregator. The second FlushQueue ensures that the
  // EvictionCandidateAggregator has aggregated the results and posted the final
  // eviction tasks back to the individual shards, and that those tasks have
  // been processed.
  FlushQueue(*backend);
  FlushQueue(*backend);

  // Eviction should have run and reduced the size.
  const int64_t kLowWatermark =
      kMaxBytes * kSqlBackendEvictionLowWaterMarkPermille / 1000;  // 9000
  EXPECT_LE(GetSizeOfAllEntries(*backend), kLowWatermark);
}

void SqlBackendImplTest::RunDelayedPostInitializationTasksTest() {
  auto backend = CreateBackendAndInit();
  auto* sql_store = backend->GetSqlStoreForTest();
  auto task_runners = backend->GetBackgroundTaskRunnersForTest();

  const auto kKey1 = CacheEntryKey("key1");
  const auto kKey2 = CacheEntryKey("key2");
  const std::string kData = "some data";
  const auto shard_id1 = sql_store->GetShardIdForHash(kKey1.hash());
  const auto shard_id2 = sql_store->GetShardIdForHash(kKey2.hash());

  // Create two entries and write some data to them.
  auto* entry1 = CreateEntryAndWriteData(backend.get(), kKey1.string(), kData);
  auto* entry2 = CreateEntryAndWriteData(backend.get(), kKey2.string(), kData);
  WaitUntilInitialized(*backend,
                       static_cast<SqlEntryImpl*>(entry1)->res_id_or_error());
  WaitUntilInitialized(*backend,
                       static_cast<SqlEntryImpl*>(entry2)->res_id_or_error());
  auto res_id1 = std::get<SqlPersistentStore::ResId>(
      static_cast<SqlEntryImpl*>(entry1)->res_id_or_error()->data.value());
  auto res_id2 = std::get<SqlPersistentStore::ResId>(
      static_cast<SqlEntryImpl*>(entry2)->res_id_or_error()->data.value());
  entry1->Close();
  entry2->Close();

  // Close the backend to ensure everything is written to disk.
  backend.reset();

  FlushQueueInTaskRunners(task_runners);

  // This block simulates a previous session where an entry was doomed but not
  // fully cleaned up.
  {
    auto store = std::make_unique<SqlPersistentStore>(
        temp_dir_.GetPath(), kDefaultMaxBytes, net::CacheType::DISK_CACHE,
        task_runners);

    base::test::TestFuture<disk_cache::SqlPersistentStore::Error> future_init;
    store->Initialize(future_init.GetCallback());
    ASSERT_EQ(future_init.Get(), disk_cache::SqlPersistentStore::Error::kOk);

    // Doom one of the entries.
    base::test::TestFuture<SqlPersistentStore::Error> future_doom;
    store->DoomEntry(kKey1, res_id1, future_doom.GetCallback());
    EXPECT_EQ(future_doom.Get(), SqlPersistentStore::Error::kOk);

    store.reset();

    FlushQueueInTaskRunners(task_runners);
  }

  // Verify directly in the database that the blobs for the entries still exist.
  EXPECT_EQ(OpenDatabaseAndGetBlobsCount(shard_id1, res_id1), 1);
  EXPECT_EQ(OpenDatabaseAndGetBlobsCount(shard_id2, res_id2), 1);

  // Create and initialize a new backend.
  backend = CreateBackend();
  sql_store = backend->GetSqlStoreForTest();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::OK);

  if (net::features::kSqlDiskCacheLoadIndexOnInit.Get()) {
    // When the SqlDiskCacheLoadIndexOnInit is enabled, the index should have
    // been loaded. The doomed entry should be gone, and the other entry should
    // be present.
    EXPECT_EQ(sql_store->GetIndexStateForHash(kKey1.hash()),
              SqlPersistentStore::IndexState::kHashNotFound);
    EXPECT_EQ(sql_store->GetIndexStateForHash(kKey2.hash()),
              SqlPersistentStore::IndexState::kHashFound);
  } else {
    // At this point, the in-memory index should not be loaded yet.
    EXPECT_EQ(sql_store->GetIndexStateForHash(kKey1.hash()),
              SqlPersistentStore::IndexState::kNotReady);
    EXPECT_EQ(sql_store->GetIndexStateForHash(kKey2.hash()),
              SqlPersistentStore::IndexState::kNotReady);
  }

  // Fast forward time to trigger the delayed post-initialization tasks.
  task_environment_.FastForwardBy(kSqlBackendPostInitializationTasksDelay);

  FlushQueue(*backend);

  // Now, the index should be loaded even if SqlDiskCacheLoadIndexOnInit is
  // disabled. The doomed entry should be gone, and the other entry should be
  // present.
  EXPECT_EQ(sql_store->GetIndexStateForHash(kKey1.hash()),
            SqlPersistentStore::IndexState::kHashNotFound);
  EXPECT_EQ(sql_store->GetIndexStateForHash(kKey2.hash()),
            SqlPersistentStore::IndexState::kHashFound);

  task_runners = backend->GetBackgroundTaskRunnersForTest();
  backend.reset();

  FlushQueueInTaskRunners(task_runners);

  // Verify directly in the database that the blob for the doomed entry has been
  // deleted, while the other one still exists.
  EXPECT_EQ(OpenDatabaseAndGetBlobsCount(shard_id1, res_id1), 0);
  EXPECT_EQ(OpenDatabaseAndGetBlobsCount(shard_id2, res_id2), 1);
}

TEST_F(SqlBackendImplTest, DelayedPostInitializationTasks) {
  RunDelayedPostInitializationTasksTest();
}

TEST_F(SqlBackendImplTest,
       DelayedPostInitializationTasksWithLoadIndexOnInitFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{net::features::kDiskCacheBackendExperiment,
        {{net::features::kDiskCacheBackendParam.name, "sql"},
         {net::features::kSqlDiskCacheLoadIndexOnInit.name, "true"}}}},
      {});
  RunDelayedPostInitializationTasksTest();
}

// Regression test for https://crbug.com/456384561
// Tests that the dangling pointer warning does not occur when the backend is
// destroyed with a pending operation that holds the last reference to an entry.
// This test reproduces the scenario where the destruction order of
// `SqlBackendImpl` members (`exclusive_operation_coordinator_` before
// `active_entries_`) could lead to a dangling `raw_ref` in `active_entries_`.
TEST_F(SqlBackendImplTest, DestructionWithPendingOperationOnEntry) {
  auto backend = CreateBackendAndInit();

  // 1. Create an entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // 2. Post a separate async exclusive operation. This ensures that the
  //    subsequent `Doom()` call will be queued and not run synchronously.
  backend->CalculateSizeOfAllEntries(base::DoNothing());

  // 3. Call `entry->Doom()`. This queues a `HandleDoomActiveEntryOperation`
  //    task in the `ExclusiveOperationCoordinator`. The task's callback
  //    captures a `scoped_refptr` to the `SqlEntryImpl`.
  entry->Doom();

  // 4. Close the entry. The last owning `scoped_refptr` is now held by the
  //    pending `Doom` operation inside the coordinator. The `active_entries_`
  //    map only holds a non-owning `raw_ref`.
  entry->Close();
  entry = nullptr;

  // 5. Destroy the backend. This triggers the destruction sequence that could
  //    cause the bug if member declaration order is incorrect.
  //    a. `weak_factory_` is destroyed, invalidating the entry's `backend_`
  //       WeakPtr.
  //    b. `exclusive_operation_coordinator_` is destroyed, which destroys the
  //       pending `Doom` task. This releases the last `scoped_refptr`.
  //    c. `~SqlEntryImpl()` is called.
  //    d. Inside `~SqlEntryImpl()`, the `if (!backend_)` check now passes,
  //       causing `ReleaseActiveEntry()` to be skipped.
  //    e. `active_entries_` is destroyed, but it still contains a `raw_ref` to
  //       the now-deleted entry, causing a dangling pointer issue.
  // If the bug exists, this test will crash here.
  auto task_runners = backend->GetBackgroundTaskRunnersForTest();
  backend.reset();

  // 6. If the bug is fixed, destruction completes safely. Run any remaining
  //    tasks to ensure clean shutdown and prevent leaks.
  FlushQueueInTaskRunners(task_runners);
}

TEST_F(SqlBackendImplTest, DoomEntryWithInMemoryIndex) {
  auto backend = CreateBackendAndInit();
  const std::string kKey = "my-key";
  const CacheEntryKey kEntryKey(kKey);

  // 1. Create an entry and close it.
  TestEntryResultCompletionCallback create_cb;
  disk_cache::EntryResult create_result = create_cb.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, create_cb.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  create_result.ReleaseEntry()->Close();

  // 2. Load in-memory index.
  ASSERT_TRUE(LoadInMemoryIndex(*backend));

  // 3. Verify that the entry is in the index.
  EXPECT_EQ(
      backend->GetSqlStoreForTest()->GetIndexStateForHash(kEntryKey.hash()),
      SqlPersistentStore::IndexState::kHashFound);

  // 4. Doom the entry.
  net::TestCompletionCallback cb_doom;
  int rv_doom = backend->DoomEntry(kKey, net::HIGHEST, cb_doom.callback());

  // 5. Verify that the entry is removed from the in-memory index synchronously.
  EXPECT_EQ(
      backend->GetSqlStoreForTest()->GetIndexStateForHash(kEntryKey.hash()),
      SqlPersistentStore::IndexState::kHashNotFound);

  EXPECT_THAT(cb_doom.GetResult(rv_doom), IsOk());

  // 6. Verify that the entry is gone.
  TestEntryResultCompletionCallback cb_open;
  disk_cache::EntryResult open_result = cb_open.GetResult(
      backend->OpenEntry(kKey, net::HIGHEST, cb_open.callback()));
  EXPECT_THAT(open_result.net_error(), IsError(net::ERR_FAILED));
}

TEST_F(SqlBackendImplTest, SetDataHintsAndDoomAndWriteOptimistically) {
  auto backend = CreateBackendAndInit();
  const std::string kKey = "my-key";
  const uint8_t kUnusableHint = 1;

  // 1. Create an entry.
  TestEntryResultCompletionCallback cb_create;
  EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback()));
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();

  // 2. Set an in-memory hint.
  entry->SetEntryInMemoryData(kUnusableHint);
  entry->Close();

  // 3. Call OnBrowserIdle() to trigger in-memory index loading.
  backend->OnBrowserIdle();
  FlushQueue(*backend);

  // 4. Verify the hint is set in the backend.
  EXPECT_EQ(backend->GetEntryInMemoryData(kKey), kUnusableHint);

  // 5. Doom the entry.
  base::test::TestFuture<int> doom_future;
  int doom_rv =
      backend->DoomEntry(kKey, net::HIGHEST, doom_future.GetCallback());
  EXPECT_EQ(doom_rv, net::ERR_IO_PENDING);

  // 6. OpenOrCreateEntry should complete synchronously and create a new entry.
  TestEntryResultCompletionCallback cb_open_or_create;
  EntryResult open_or_create_result = backend->OpenOrCreateEntry(
      kKey, net::HIGHEST, cb_open_or_create.callback());
  ASSERT_THAT(open_or_create_result.net_error(), IsOk());
  EXPECT_FALSE(open_or_create_result.opened());

  open_or_create_result.ReleaseEntry()->Close();
  EXPECT_EQ(doom_future.Get(), net::OK);
}

TEST_F(SqlBackendImplTest, SetEntryDataHintsWithSpeculativeCreateEntryFailure) {
  auto backend = CreateBackendAndInit();
  EXPECT_TRUE(LoadInMemoryIndex(*backend));
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(true);
  const std::string kKey = "my-key";

  // 1. Create an entry. This should return immediately with a speculatively
  //    created entry.
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result =
      backend->CreateEntry(kKey, net::HIGHEST, cb_create.callback());
  ASSERT_THAT(create_result.net_error(), IsOk());
  auto* entry = create_result.ReleaseEntry();
  ASSERT_TRUE(entry);

  // 2. Wait for the database operation to complete.
  auto* sql_entry = static_cast<SqlEntryImpl*>(entry);
  WaitUntilInitialized(*backend, sql_entry->res_id_or_error());
  backend->GetSqlStoreForTest()->SetSimulateDbFailureForTesting(false);

  // 3. Set an in-memory hint. This should fail silently because the entry has
  //    an error.
  const uint8_t kUnusableHint = 1;
  entry->SetEntryInMemoryData(kUnusableHint);
  entry->Close();

  // 4. Flush the queue to make sure the SetEntryInMemoryData operation is
  //    processed.
  FlushQueue(*backend);

  // 5. Verify the hint is not set in the backend.
  EXPECT_EQ(backend->GetEntryInMemoryData(kKey), 0);
}

}  // namespace
}  // namespace disk_cache
