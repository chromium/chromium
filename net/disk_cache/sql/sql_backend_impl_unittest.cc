// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_backend_impl.h"

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_entry_impl.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

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

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
};

TEST_F(SqlBackendImplTest, InitWithNoFakeIndexFile) {
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
  EXPECT_EQ(*file_size, sizeof(kSqlBackendFakeIndexMagicNumber));
  int64_t magic_number_from_file;
  ASSERT_TRUE(base::ReadFile(file_path,
                             base::byte_span_from_ref(magic_number_from_file)));
  EXPECT_EQ(magic_number_from_file, kSqlBackendFakeIndexMagicNumber);
}

TEST_F(SqlBackendImplTest, InitWithFakeIndexFile) {
  base::HistogramTester histogram_tester;
  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  ASSERT_TRUE(base::WriteFile(
      file_path, base::byte_span_from_ref(kSqlBackendFakeIndexMagicNumber)));

  auto backend = CreateBackend();
  base::test::TestFuture<int> future;
  backend->Init(future.GetCallback());
  ASSERT_EQ(future.Get(), net::OK);
  histogram_tester.ExpectUniqueSample("Net.SqlDiskCache.FakeIndexFileError",
                                      FakeIndexFileError::kOkExisting, 1);
}

TEST_F(SqlBackendImplTest, InitWithCorruptedFakeIndexFile) {
  base::HistogramTester histogram_tester;
  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  const int64_t kWrongMagicNumber = 0xDEADBEEFDEADBEEF;
  ASSERT_TRUE(
      base::WriteFile(file_path, base::byte_span_from_ref(kWrongMagicNumber)));

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
  base::HistogramTester histogram_tester;
  base::FilePath file_path =
      temp_dir_.GetPath().Append(kSqlBackendFakeIndexFileName);
  ASSERT_TRUE(base::WriteFile(
      file_path, base::byte_span_from_ref(kSqlBackendFakeIndexMagicNumber)));
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
  EXPECT_EQ(buffer->span().first(kHeadData.size()),
            base::as_byte_span(kHeadData));
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

  // The first call to OpenNextEntry. Inside its callback, we'll trigger the
  // second call.
  ASSERT_THAT(
      iter->OpenNextEntry(base::BindLambdaForTesting([&](EntryResult result1) {
            ASSERT_THAT(result1.net_error(), IsOk());
            auto* entry1 = result1.ReleaseEntry();
            EXPECT_EQ(entry1->GetKey(), "key2");  // Newest entry first
            entry1->Close();
            // Now, make the recursive call to OpenNextEntry.
            ASSERT_THAT(
                iter->OpenNextEntry(
                        base::BindLambdaForTesting([&](EntryResult result2) {
                          ASSERT_THAT(result2.net_error(), IsOk());
                          auto* entry2 = result2.ReleaseEntry();
                          EXPECT_EQ(entry2->GetKey(), "key1");
                          entry2->Close();
                          // By this point, the CreateEntry for "key3" should
                          // have completed, proving that the normal operation
                          // was not starved.
                          CHECK(entry3);
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

  // The first call to OpenNextEntry. Inside its callback, we'll trigger the
  // second call.
  ASSERT_THAT(
      iter->OpenNextEntry(base::BindLambdaForTesting([&](EntryResult result1) {
            ASSERT_THAT(result1.net_error(), IsOk());
            auto* entry1_iter = result1.ReleaseEntry();
            EXPECT_EQ(entry1_iter->GetKey(), "key2");  // Newest entry first
            // The returned entry should be the same as the active one.
            EXPECT_EQ(entry1_iter, entry2_active);
            entry1_iter->Close();
            // Now, make the recursive call to OpenNextEntry.
            ASSERT_THAT(
                iter->OpenNextEntry(
                        base::BindLambdaForTesting([&](EntryResult result2) {
                          ASSERT_THAT(result2.net_error(), IsOk());
                          auto* entry2_iter = result2.ReleaseEntry();
                          EXPECT_EQ(entry2_iter->GetKey(), "key1");
                          entry2_iter->Close();
                          // By this point, the CreateEntry for "key3" should
                          // have completed.
                          CHECK(entry3);
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

  // Initiate a WriteData operation, which will be pending.
  auto write_buffer = base::MakeRefCounted<net::StringIOBuffer>("data");
  base::test::TestFuture<int> write_future;
  int rv = entry->WriteData(1, 0, write_buffer.get(), write_buffer->size(),
                            write_future.GetCallback(), false);
  ASSERT_THAT(rv, IsError(net::ERR_IO_PENDING));

  // Destroy the backend while the write is in flight.
  backend.reset();

  // The callback should be aborted.
  EXPECT_EQ(write_future.Get(), net::ERR_ABORTED);

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
  auto task_runner = backend->GetBackgroundTaskRunnerForTest();

  const std::string kKey1 = "key1";
  const std::string kKey2 = "key2";
  const std::string kKey3 = "key3";
  const std::string kData = "some data";

  auto* entry1 = CreateEntryAndWriteData(backend.get(), kKey1, kData);
  auto* entry2 = CreateEntryAndWriteData(backend.get(), kKey2, kData);
  auto* entry3 = CreateEntryAndWriteData(backend.get(), kKey3, kData);
  auto token = static_cast<SqlEntryImpl*>(entry3)->token();
  entry1->Close();
  entry2->Close();
  entry3->Close();

  backend.reset();

  {
    base::RunLoop run_loop;
    task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // 2. Open the database directly via SqlPersistentStore and doom the third
  // entry.
  {
    auto store = disk_cache::SqlPersistentStore::Create(
        temp_dir_.GetPath(), kDefaultMaxBytes, net::CacheType::DISK_CACHE,
        task_runner);
    base::test::TestFuture<disk_cache::SqlPersistentStore::Error> future_init;
    store->Initialize(future_init.GetCallback());
    ASSERT_EQ(future_init.Get(), disk_cache::SqlPersistentStore::Error::kOk);

    base::test::TestFuture<SqlPersistentStore::Error> future_doom;
    store->DoomEntry(CacheEntryKey(kKey3), token, future_doom.GetCallback());
    EXPECT_EQ(future_doom.Get(), SqlPersistentStore::Error::kOk);

    store.reset();
  }
  {
    base::RunLoop run_loop;
    task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // 3. Recreate the backend
  backend = CreateBackendAndInit();

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
  task_environment_.FastForwardBy(kSqlBackendDeleteDoomedEntriesDelay +
                                  base::Seconds(1));
  // Verify that `DeleteDoomedEntriesCount` UMA was recorded in the histogram.
  histogram_tester.ExpectUniqueSample(
      "Net.SqlDiskCache.DeleteDoomedEntriesCount", 1, 1);

  // 5. Verify that the data can still be read from the doomed entry.
  ReadAndVerifyData(entry1, kData);
  ReadAndVerifyData(entry2, kData);

  entry1->Close();
  entry2->Close();
}

}  // namespace
}  // namespace disk_cache
