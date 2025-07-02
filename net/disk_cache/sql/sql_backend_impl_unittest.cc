// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_backend_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
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

  // `OpenNextEntry()` is an exclusive operation, while `DoomEntry()` is a
  // normal operation. Since `OpenNextEntry()` is posted first, it will run
  // before the `DoomEntry()` operation, which gets queued. After the iterator
  // returns the entry, the `DoomEntry()` operation runs and marks the entry as
  // doomed.
  result_iter = cb.GetResult(std::move(result_iter));
  ASSERT_THAT(result_iter.net_error(), IsOk());
  entry = result_iter.ReleaseEntry();
  EXPECT_TRUE((static_cast<SqlEntryImpl*>(entry))->doomed());
  // TODO(crbug.com/422065015): Check that the `entry` data can be read from the
  // storage after implementing the entry data read/write operations.
  entry->Close();
}

// Tests a race condition between an iterator opening an entry and a call to
// `Backend::DoomAllEntries`.
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

  // Both `DoomAllEntries()` and `OpenNextEntry()` are exclusive operations and
  // are serialized. Since `OpenNextEntry()` is posted first, it will run
  // first, retrieving the entry. Then, `DoomAllEntries()` will run and doom all
  // entries, including the one just opened.
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
// starve normal operations. This is prevented by releasing the exclusive
// operation handle before invoking the callback.
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

}  // namespace
}  // namespace disk_cache
