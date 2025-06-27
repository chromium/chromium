// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_backend_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
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

// Default max cache size for tests, 10 MB.
inline constexpr int64_t kDefaultMaxBytes = 10 * 1024 * 1024;

class SqlBackendImplTest : public testing::Test {
 public:
  SqlBackendImplTest() = default;
  ~SqlBackendImplTest() override = default;

  // Sets up a temporary directory and a background task runner for each test.
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
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

 private:
  base::ScopedTempDir temp_dir_;
};

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

TEST_F(SqlBackendImplTest, CalculateSizeOfAllEntries) {
  auto backend = CreateBackendAndInit();
  net::TestInt64CompletionCallback callback;
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->CalculateSizeOfAllEntries(callback.callback()),
            net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, CalculateSizeOfEntriesBetween) {
  auto backend = CreateBackendAndInit();
  net::TestInt64CompletionCallback callback;
  // TODO(crbug.com/422065015): Implement this method.
  EXPECT_EQ(backend->CalculateSizeOfEntriesBetween(
                base::Time(), base::Time::Max(), callback.callback()),
            net::ERR_NOT_IMPLEMENTED);
}

TEST_F(SqlBackendImplTest, GetStats) {
  auto backend = CreateBackendAndInit();
  base::StringPairs stats;
  backend->GetStats(&stats);
  EXPECT_THAT(stats, ElementsAre(Pair("Cache type", "SQL Cache")));
}

TEST_F(SqlBackendImplTest, OnExternalCacheHit) {
  auto backend = CreateBackendAndInit();
  // Should not crash.
  backend->OnExternalCacheHit("test_key");
}

TEST_F(SqlBackendImplTest, IteratorParallelEntryDoom) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry = create_result.ReleaseEntry();

  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  entry->Doom();

  result_iter = cb.GetResult(std::move(result_iter));
  // `entry->Doom()` is called right after `OpenNextEntry()` is initiated but
  // before its asynchronous work completes. The iterator should detect that the
  // entry is doomed and skip it. With no other entries, the iteration correctly
  // fails.
  ASSERT_THAT(result_iter.net_error(), IsError(net::ERR_FAILED));

  entry->Close();
}

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
  entry->Close();

  result_iter = cb.GetResult(std::move(result_iter));
  // `entry->Doom()` is called right after `OpenNextEntry()` is initiated but
  // before its asynchronous work completes. The iterator should detect that the
  // entry is doomed and skip it. With no other entries, the iteration correctly
  // fails.
  ASSERT_THAT(result_iter.net_error(), IsError(net::ERR_FAILED));
}

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

  // `entry->Doom()` is called after `OpenNextEntry()` is initiated but before
  // its asynchronous work completes.
  // The iterator starts from the newest entry, which is `key2`. However, `key2`
  // is doomed before the iterator's `OpenNextEntry` operation completes. The
  // iterator should detect that `key2` is doomed and skip it, returning `key1`
  // instead.
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

TEST_F(SqlBackendImplTest, IteratorParallelDoom) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry = create_result.ReleaseEntry();
  entry->Close();

  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  net::TestCompletionCallback cb_doom;
  int rv_doom = backend->DoomEntry("key", net::HIGHEST, cb_doom.callback());
  EXPECT_EQ(net::OK, cb_doom.GetResult(rv_doom));

  // Unlike `SqlEntryImpl::Doom()`, `DoomEntry()` and `OpenNextEntry()` are
  // exclusive operations, so the entry will be doomed after the result of
  // `OpenNextEntry()` is retrieved.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  entry = result_iter.ReleaseEntry();
  EXPECT_TRUE((static_cast<SqlEntryImpl*>(entry))->doomed());
  // TODO(crbug.com/422065015): Check that the `entry` data can be read from the
  // storage after implementing the entry data read/write operations.
  entry->Close();
}

TEST_F(SqlBackendImplTest, IteratorParallelDoomAll) {
  auto backend = CreateBackendAndInit();
  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult create_result = cb_create.GetResult(
      backend->CreateEntry("key", net::HIGHEST, cb_create.callback()));
  auto* entry = create_result.ReleaseEntry();
  entry->Close();

  auto iter = backend->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());

  net::TestCompletionCallback cb_doom;
  int rv_doom = backend->DoomAllEntries(cb_doom.callback());
  EXPECT_EQ(net::OK, cb_doom.GetResult(rv_doom));

  // Unlike `SqlEntryImpl::Doom()`, `DoomAllEntries()` and `OpenNextEntry()` are
  // exclusive operations, so the entry will be doomed after the result of
  // `OpenNextEntry()` is retrieved.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  entry = result_iter.ReleaseEntry();
  EXPECT_TRUE((static_cast<SqlEntryImpl*>(entry))->doomed());
  // TODO(crbug.com/422065015): Check that the `entry` data can be read from the
  // storage after implementing the entry data read/write operations.
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

}  // namespace
}  // namespace disk_cache
