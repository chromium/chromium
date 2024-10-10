// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <memory>
#include <optional>
#include <string_view>

#include "base/containers/queue.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/base/tracing.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/entry_impl.h"
#include "net/disk_cache/blockfile/experiments.h"
#include "net/disk_cache/blockfile/mapped_file.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_histogram_enums.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_test_util.h"
#include "net/disk_cache/simple/simple_util.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/dynamic_annotations.h"

using disk_cache::EntryResult;
using net::test::IsError;
using net::test::IsOk;
using testing::ByRef;
using testing::Contains;
using testing::Eq;
using testing::Field;

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#endif

// TODO(crbug.com/41451310): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_BlockFileOpenOrCreateEntry DISABLED_BlockFileOpenOrCreateEntry
#define MAYBE_NonEmptyCorruptSimpleCacheDoesNotRecover \
  DISABLED_NonEmptyCorruptSimpleCacheDoesNotRecover
#define MAYBE_SimpleOpenOrCreateEntry DISABLED_SimpleOpenOrCreateEntry
#else
#define MAYBE_BlockFileOpenOrCreateEntry BlockFileOpenOrCreateEntry
#define MAYBE_NonEmptyCorruptSimpleCacheDoesNotRecover \
  NonEmptyCorruptSimpleCacheDoesNotRecover
#define MAYBE_SimpleOpenOrCreateEntry SimpleOpenOrCreateEntry
#endif

using base::Time;

namespace {

#if BUILDFLAG(IS_FUCHSIA)
// Load tests with large numbers of file descriptors perform poorly on
// virtualized test execution environments.
// TODO(crbug.com/40560856): Remove this workaround when virtualized test
// performance improves.
const int kLargeNumEntries = 100;
#else
const int kLargeNumEntries = 512;
#endif

}  // namespace

// Tests that can run with different types of caches.
class DiskCacheBackendTest : public DiskCacheTestWithCache {
 protected:
  // Some utility methods:

  // Perform IO operations on the cache until there is pending IO.
  int GeneratePendingIO(net::TestCompletionCallback* cb);

  // Adds 5 sparse entries. |doomed_start| and |doomed_end| if not NULL,
  // will be filled with times, used by DoomEntriesSince and DoomEntriesBetween.
  // There are 4 entries after doomed_start and 2 after doomed_end.
  void InitSparseCache(base::Time* doomed_start, base::Time* doomed_end);

  bool CreateSetOfRandomEntries(std::set<std::string>* key_pool);
  bool EnumerateAndMatchKeys(int max_to_open,
                             TestIterator* iter,
                             std::set<std::string>* keys_to_match,
                             size_t* count);

  // Computes the expected size of entry metadata, i.e. the total size without
  // the actual data stored. This depends only on the entry's |key| size.
  int GetEntryMetadataSize(std::string key);

  // The Simple Backend only tracks the approximate sizes of entries. This
  // rounds the exact size appropriately.
  int GetRoundedSize(int exact_size);

  // Create a default key with the name provided, populate it with
  // CacheTestFillBuffer, and ensure this was done correctly.
  void CreateKeyAndCheck(disk_cache::Backend* cache, std::string key);

  // For the simple cache, wait until indexing has occurred and make sure
  // completes successfully.
  void WaitForSimpleCacheIndexAndCheck(disk_cache::Backend* cache);

  // Run all of the task runners untile idle, covers cache worker pools.
  void RunUntilIdle();

  // Actual tests:
  void BackendBasics();
  void BackendKeying();
  void BackendShutdownWithPendingFileIO(bool fast);
  void BackendShutdownWithPendingIO(bool fast);
  void BackendShutdownWithPendingCreate(bool fast);
  void BackendShutdownWithPendingDoom();
  void BackendSetSize();
  void BackendLoad();
  void BackendChain();
  void BackendValidEntry();
  void BackendInvalidEntry();
  void BackendInvalidEntryRead();
  void BackendInvalidEntryWithLoad();
  void BackendTrimInvalidEntry();
  void BackendTrimInvalidEntry2();
  void BackendEnumerations();
  void BackendEnumerations2();
  void BackendDoomMidEnumeration();
  void BackendInvalidEntryEnumeration();
  void BackendFixEnumerators();
  void BackendDoomRecent();
  void BackendDoomBetween();
  void BackendCalculateSizeOfAllEntries();
  void BackendCalculateSizeOfEntriesBetween(
      bool expect_access_time_range_comparisons);
  void BackendTransaction(const std::string& name, int num_entries, bool load);
  void BackendRecoverInsert();
  void BackendRecoverRemove();
  void BackendRecoverWithEviction();
  void BackendInvalidEntry2();
  void BackendInvalidEntry3();
  void BackendInvalidEntry7();
  void BackendInvalidEntry8();
  void BackendInvalidEntry9(bool eviction);
  void BackendInvalidEntry10(bool eviction);
  void BackendInvalidEntry11(bool eviction);
  void BackendTrimInvalidEntry12();
  void BackendDoomAll();
  void BackendDoomAll2();
  void BackendInvalidRankings();
  void BackendInvalidRankings2();
  void BackendDisable();
  void BackendDisable2();
  void BackendDisable3();
  void BackendDisable4();
  void BackendDisabledAPI();
  void BackendEviction();
  void BackendOpenOrCreateEntry();
  void BackendDeadOpenNextEntry();
  void BackendIteratorConcurrentDoom();
  void BackendValidateMigrated();
};

void DiskCacheBackendTest::CreateKeyAndCheck(disk_cache::Backend* cache,
                                             std::string key) {
  const int kBufSize = 4 * 1024;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  CacheTestFillBuffer(buffer->data(), kBufSize, true);
  TestEntryResultCompletionCallback cb_entry;
  disk_cache::EntryResult result =
      cache->CreateEntry(key, net::HIGHEST, cb_entry.callback());
  result = cb_entry.GetResult(std::move(result));
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  EXPECT_EQ(kBufSize, WriteData(entry, 0, 0, buffer.get(), kBufSize, false));
  entry->Close();
  RunUntilIdle();
}

void DiskCacheBackendTest::WaitForSimpleCacheIndexAndCheck(
    disk_cache::Backend* cache) {
  net::TestCompletionCallback wait_for_index_cb;
  static_cast<disk_cache::SimpleBackendImpl*>(cache)->index()->ExecuteWhenReady(
      wait_for_index_cb.callback());
  int rv = wait_for_index_cb.WaitForResult();
  ASSERT_THAT(rv, IsOk());
  RunUntilIdle();
}

void DiskCacheBackendTest::RunUntilIdle() {
  DiskCacheTestWithCache::RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  disk_cache::FlushCacheThreadForTesting();
}

int DiskCacheBackendTest::GeneratePendingIO(net::TestCompletionCallback* cb) {
  if (!use_current_thread_ && !simple_cache_mode_) {
    ADD_FAILURE();
    return net::ERR_FAILED;
  }

  TestEntryResultCompletionCallback create_cb;
  EntryResult entry_result;
  entry_result =
      cache_->CreateEntry("some key", net::HIGHEST, create_cb.callback());
  entry_result = create_cb.GetResult(std::move(entry_result));
  if (entry_result.net_error() != net::OK)
    return net::ERR_CACHE_CREATE_FAILURE;
  disk_cache::Entry* entry = entry_result.ReleaseEntry();

  const int kSize = 25000;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  int rv = net::OK;
  for (int i = 0; i < 10 * 1024 * 1024; i += 64 * 1024) {
    // We are using the current thread as the cache thread because we want to
    // be able to call directly this method to make sure that the OS (instead
    // of us switching thread) is returning IO pending.
    if (!simple_cache_mode_) {
      rv = static_cast<disk_cache::EntryImpl*>(entry)->WriteDataImpl(
          0, i, buffer.get(), kSize, cb->callback(), false);
    } else {
      rv = entry->WriteData(0, i, buffer.get(), kSize, cb->callback(), false);
    }

    if (rv == net::ERR_IO_PENDING)
      break;
    if (rv != kSize)
      rv = net::ERR_FAILED;
  }

  // Don't call Close() to avoid going through the queue or we'll deadlock
  // waiting for the operation to finish.
  if (!simple_cache_mode_)
    static_cast<disk_cache::EntryImpl*>(entry)->Release();
  else
    entry->Close();

  return rv;
}

void DiskCacheBackendTest::InitSparseCache(base::Time* doomed_start,
                                           base::Time* doomed_end) {
  InitCache();

  const int kSize = 50;
  // This must be greater than MemEntryImpl::kMaxSparseEntrySize.
  const int kOffset = 10 + 1024 * 1024;

  disk_cache::Entry* entry0 = nullptr;
  disk_cache::Entry* entry1 = nullptr;
  disk_cache::Entry* entry2 = nullptr;

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  ASSERT_THAT(CreateEntry("zeroth", &entry0), IsOk());
  ASSERT_EQ(kSize, WriteSparseData(entry0, 0, buffer.get(), kSize));
  ASSERT_EQ(kSize,
            WriteSparseData(entry0, kOffset + kSize, buffer.get(), kSize));
  entry0->Close();

  FlushQueueForTest();
  AddDelay();
  if (doomed_start)
    *doomed_start = base::Time::Now();

  // Order in rankings list:
  // first_part1, first_part2, second_part1, second_part2
  ASSERT_THAT(CreateEntry("first", &entry1), IsOk());
  ASSERT_EQ(kSize, WriteSparseData(entry1, 0, buffer.get(), kSize));
  ASSERT_EQ(kSize,
            WriteSparseData(entry1, kOffset + kSize, buffer.get(), kSize));
  entry1->Close();

  ASSERT_THAT(CreateEntry("second", &entry2), IsOk());
  ASSERT_EQ(kSize, WriteSparseData(entry2, 0, buffer.get(), kSize));
  ASSERT_EQ(kSize,
            WriteSparseData(entry2, kOffset + kSize, buffer.get(), kSize));
  entry2->Close();

  FlushQueueForTest();
  AddDelay();
  if (doomed_end)
    *doomed_end = base::Time::Now();

  // Order in rankings list:
  // third_part1, fourth_part1, third_part2, fourth_part2
  disk_cache::Entry* entry3 = nullptr;
  disk_cache::Entry* entry4 = nullptr;
  ASSERT_THAT(CreateEntry("third", &entry3), IsOk());
  ASSERT_EQ(kSize, WriteSparseData(entry3, 0, buffer.get(), kSize));
  ASSERT_THAT(CreateEntry("fourth", &entry4), IsOk());
  ASSERT_EQ(kSize, WriteSparseData(entry4, 0, buffer.get(), kSize));
  ASSERT_EQ(kSize,
            WriteSparseData(entry3, kOffset + kSize, buffer.get(), kSize));
  ASSERT_EQ(kSize,
            WriteSparseData(entry4, kOffset + kSize, buffer.get(), kSize));
  entry3->Close();
  entry4->Close();

  FlushQueueForTest();
  AddDelay();
}

// Creates entries based on random keys. Stores these keys in |key_pool|.
bool DiskCacheBackendTest::CreateSetOfRandomEntries(
    std::set<std::string>* key_pool) {
  const int kNumEntries = 10;
  const int initial_entry_count = cache_->GetEntryCount();

  for (int i = 0; i < kNumEntries; ++i) {
    std::string key = GenerateKey(true);
    disk_cache::Entry* entry;
    if (CreateEntry(key, &entry) != net::OK) {
      return false;
    }
    key_pool->insert(key);
    entry->Close();
  }
  return key_pool->size() ==
         static_cast<size_t>(cache_->GetEntryCount() - initial_entry_count);
}

// Performs iteration over the backend and checks that the keys of entries
// opened are in |keys_to_match|, then erases them. Up to |max_to_open| entries
// will be opened, if it is positive. Otherwise, iteration will continue until
// OpenNextEntry stops returning net::OK.
bool DiskCacheBackendTest::EnumerateAndMatchKeys(
    int max_to_open,
    TestIterator* iter,
    std::set<std::string>* keys_to_match,
    size_t* count) {
  disk_cache::Entry* entry;

  if (!iter)
    return false;
  while (iter->OpenNextEntry(&entry) == net::OK) {
    if (!entry)
      return false;
    EXPECT_EQ(1U, keys_to_match->erase(entry->GetKey()));
    entry->Close();
    ++(*count);
    if (max_to_open >= 0 && static_cast<int>(*count) >= max_to_open)
      break;
  };

  return true;
}

int DiskCacheBackendTest::GetEntryMetadataSize(std::string key) {
  // For blockfile and memory backends, it is just the key size.
  if (!simple_cache_mode_)
    return key.size();

  // For the simple cache, we must add the file header and EOF, and that for
  // every stream.
  return disk_cache::kSimpleEntryStreamCount *
         (sizeof(disk_cache::SimpleFileHeader) +
          sizeof(disk_cache::SimpleFileEOF) + key.size());
}

int DiskCacheBackendTest::GetRoundedSize(int exact_size) {
  if (!simple_cache_mode_)
    return exact_size;

  return (exact_size + 255) & 0xFFFFFF00;
}

void DiskCacheBackendTest::BackendBasics() {
  InitCache();
  disk_cache::Entry *entry1 = nullptr, *entry2 = nullptr;
  EXPECT_NE(net::OK, OpenEntry("the first key", &entry1));
  ASSERT_THAT(CreateEntry("the first key", &entry1), IsOk());
  ASSERT_TRUE(nullptr != entry1);
  entry1->Close();
  entry1 = nullptr;

  ASSERT_THAT(OpenEntry("the first key", &entry1), IsOk());
  ASSERT_TRUE(nullptr != entry1);
  entry1->Close();
  entry1 = nullptr;

  EXPECT_NE(net::OK, CreateEntry("the first key", &entry1));
  ASSERT_THAT(OpenEntry("the first key", &entry1), IsOk());
  EXPECT_NE(net::OK, OpenEntry("some other key", &entry2));
  ASSERT_THAT(CreateEntry("some other key", &entry2), IsOk());
  ASSERT_TRUE(nullptr != entry1);
  ASSERT_TRUE(nullptr != entry2);
  EXPECT_EQ(2, cache_->GetEntryCount());

  disk_cache::Entry* entry3 = nullptr;
  ASSERT_THAT(OpenEntry("some other key", &entry3), IsOk());
  ASSERT_TRUE(nullptr != entry3);
  EXPECT_TRUE(entry2 == entry3);

  EXPECT_THAT(DoomEntry("some other key"), IsOk());
  EXPECT_EQ(1, cache_->GetEntryCount());
  entry1->Close();
  entry2->Close();
  entry3->Close();

  EXPECT_THAT(DoomEntry("the first key"), IsOk());
  EXPECT_EQ(0, cache_->GetEntryCount());

  ASSERT_THAT(CreateEntry("the first key", &entry1), IsOk());
  ASSERT_THAT(CreateEntry("some other key", &entry2), IsOk());
  entry1->Doom();
  entry1->Close();
  EXPECT_THAT(DoomEntry("some other key"), IsOk());
  EXPECT_EQ(0, cache_->GetEntryCount());
  entry2->Close();
}

TEST_F(DiskCacheBackendTest, Basics) {
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, NewEvictionBasics) {
  SetNewEviction();
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyBasics) {
  SetMemoryOnlyMode();
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, AppCacheBasics) {
  SetCacheType(net::APP_CACHE);
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, ShaderCacheBasics) {
  SetCacheType(net::SHADER_CACHE);
  BackendBasics();
}

void DiskCacheBackendTest::BackendKeying() {
  InitCache();
  const char kName1[] = "the first key";
  const char kName2[] = "the first Key";
  disk_cache::Entry *entry1, *entry2;
  ASSERT_THAT(CreateEntry(kName1, &entry1), IsOk());

  ASSERT_THAT(CreateEntry(kName2, &entry2), IsOk());
  EXPECT_TRUE(entry1 != entry2) << "Case sensitive";
  entry2->Close();

  char buffer[30];
  base::strlcpy(buffer, kName1, std::size(buffer));
  ASSERT_THAT(OpenEntry(buffer, &entry2), IsOk());
  EXPECT_TRUE(entry1 == entry2);
  entry2->Close();

  base::strlcpy(buffer + 1, kName1, std::size(buffer) - 1);
  ASSERT_THAT(OpenEntry(buffer + 1, &entry2), IsOk());
  EXPECT_TRUE(entry1 == entry2);
  entry2->Close();

  base::strlcpy(buffer + 3, kName1, std::size(buffer) - 3);
  ASSERT_THAT(OpenEntry(buffer + 3, &entry2), IsOk());
  EXPECT_TRUE(entry1 == entry2);
  entry2->Close();

  // Now verify long keys.
  char buffer2[20000];
  memset(buffer2, 's', sizeof(buffer2));
  buffer2[1023] = '\0';
  ASSERT_EQ(net::OK, CreateEntry(buffer2, &entry2)) << "key on block file";
  entry2->Close();

  buffer2[1023] = 'g';
  buffer2[19999] = '\0';
  ASSERT_EQ(net::OK, CreateEntry(buffer2, &entry2)) << "key on external file";
  entry2->Close();
  entry1->Close();

  // Create entries with null terminator(s), and check equality. Note we create
  // the strings via the ctor instead of using literals because literals are
  // implicitly C strings which will stop at the first null terminator.
  std::string key1(4, '\0');
  key1[1] = 's';
  std::string key2(3, '\0');
  key2[1] = 's';
  ASSERT_THAT(CreateEntry(key1, &entry1), IsOk());
  ASSERT_THAT(CreateEntry(key2, &entry2), IsOk());
  EXPECT_TRUE(entry1 != entry2) << "Different lengths";
  EXPECT_EQ(entry1->GetKey(), key1);
  EXPECT_EQ(entry2->GetKey(), key2);
  entry1->Close();
  entry2->Close();
}

TEST_F(DiskCacheBackendTest, Keying) {
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, NewEvictionKeying) {
  SetNewEviction();
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyKeying) {
  SetMemoryOnlyMode();
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, AppCacheKeying) {
  SetCacheType(net::APP_CACHE);
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, ShaderCacheKeying) {
  SetCacheType(net::SHADER_CACHE);
  BackendKeying();
}

TEST_F(DiskCacheTest, CreateBackend) {
  TestBackendResultCompletionCallback cb;

  {
    ASSERT_TRUE(CleanupCacheDir());

    // Test the private factory method(s).
    std::unique_ptr<disk_cache::Backend> cache;
    cache = disk_cache::MemBackendImpl::CreateBackend(0, nullptr);
    ASSERT_TRUE(cache.get());
    cache.reset();

    // Now test the public API.

    disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
        net::DISK_CACHE, net::CACHE_BACKEND_DEFAULT,
        /*file_operations=*/nullptr, cache_path_, 0,
        disk_cache::ResetHandling::kNeverReset, nullptr, cb.callback());
    rv = cb.GetResult(std::move(rv));
    ASSERT_THAT(rv.net_error, IsOk());
    ASSERT_TRUE(rv.backend);
    rv.backend.reset();

    rv = disk_cache::CreateCacheBackend(
        net::MEMORY_CACHE, net::CACHE_BACKEND_DEFAULT,
        /*file_operations=*/nullptr, base::FilePath(), 0,
        disk_cache::ResetHandling::kNeverReset, nullptr, cb.callback());
    rv = cb.GetResult(std::move(rv));
    ASSERT_THAT(rv.net_error, IsOk());
    ASSERT_TRUE(rv.backend);
    rv.backend.reset();
  }

  base::RunLoop().RunUntilIdle();
}

TEST_F(DiskCacheTest, MemBackendPostCleanupCallback) {
  TestBackendResultCompletionCallback cb;

  net::TestClosure on_cleanup;

  disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
      net::MEMORY_CACHE, net::CACHE_BACKEND_DEFAULT,
      /*file_operations=*/nullptr, base::FilePath(), 0,
      disk_cache::ResetHandling::kNeverReset, nullptr, on_cleanup.closure(),
      cb.callback());
  rv = cb.GetResult(std::move(rv));
  ASSERT_THAT(rv.net_error, IsOk());
  ASSERT_TRUE(rv.backend);
  // The callback should be posted after backend is destroyed.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(on_cleanup.have_result());

  rv.backend.reset();

  EXPECT_FALSE(on_cleanup.have_result());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(on_cleanup.have_result());
}

TEST_F(DiskCacheTest, CreateBackendDouble) {
  // Make sure that creation for the second backend for same path happens
  // after the first one completes.
  TestBackendResultCompletionCallback cb, cb2;

  disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
      net::APP_CACHE, net::CACHE_BACKEND_DEFAULT, /*file_operations=*/nullptr,
      cache_path_, 0, disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr, cb.callback());

  disk_cache::BackendResult rv2 = disk_cache::CreateCacheBackend(
      net::APP_CACHE, net::CACHE_BACKEND_DEFAULT, /*file_operations=*/nullptr,
      cache_path_, 0, disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr, cb2.callback());

  rv = cb.GetResult(std::move(rv));
  EXPECT_THAT(rv.net_error, IsOk());
  EXPECT_TRUE(rv.backend);
  disk_cache::FlushCacheThreadForTesting();

  // No rv2.backend yet.
  EXPECT_EQ(net::ERR_IO_PENDING, rv2.net_error);
  EXPECT_FALSE(rv2.backend);
  EXPECT_FALSE(cb2.have_result());

  rv.backend.reset();

  // Now rv2.backend should exist.
  rv2 = cb2.GetResult(std::move(rv2));
  EXPECT_THAT(rv2.net_error, IsOk());
  EXPECT_TRUE(rv2.backend);
}

TEST_F(DiskCacheBackendTest, CreateBackendDoubleOpenEntry) {
  // Demonstrate the creation sequencing with an open entry. This is done
  // with SimpleCache since the block-file cache cancels most of I/O on
  // destruction and blocks for what it can't cancel.

  // Don't try to sanity-check things as a blockfile cache
  SetSimpleCacheMode();

  // Make sure that creation for the second backend for same path happens
  // after the first one completes, and all of its ops complete.
  TestBackendResultCompletionCallback cb, cb2;

  disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
      net::APP_CACHE, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr,
      cache_path_, 0, disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr, cb.callback());

  disk_cache::BackendResult rv2 = disk_cache::CreateCacheBackend(
      net::APP_CACHE, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr,
      cache_path_, 0, disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr, cb2.callback());

  rv = cb.GetResult(std::move(rv));
  EXPECT_THAT(rv.net_error, IsOk());
  ASSERT_TRUE(rv.backend);
  disk_cache::FlushCacheThreadForTesting();

  // No cache 2 yet.
  EXPECT_EQ(net::ERR_IO_PENDING, rv2.net_error);
  EXPECT_FALSE(rv2.backend);
  EXPECT_FALSE(cb2.have_result());

  TestEntryResultCompletionCallback cb3;
  EntryResult entry_result =
      rv.backend->CreateEntry("key", net::HIGHEST, cb3.callback());
  entry_result = cb3.GetResult(std::move(entry_result));
  ASSERT_EQ(net::OK, entry_result.net_error());

  rv.backend.reset();

  // Still doesn't exist.
  EXPECT_FALSE(cb2.have_result());

  entry_result.ReleaseEntry()->Close();

  // Now should exist.
  rv2 = cb2.GetResult(std::move(rv2));
  EXPECT_THAT(rv2.net_error, IsOk());
  EXPECT_TRUE(rv2.backend);
}

TEST_F(DiskCacheBackendTest, CreateBackendPostCleanup) {
  // Test for the explicit PostCleanupCallback parameter to CreateCacheBackend.

  // Extravagant size payload to make reproducing races easier.
  const int kBufSize = 256 * 1024;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  CacheTestFillBuffer(buffer->data(), kBufSize, true);

  SetSimpleCacheMode();
  CleanupCacheDir();

  base::RunLoop run_loop;
  TestBackendResultCompletionCallback cb;

  disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
      net::APP_CACHE, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr,
      cache_path_, 0, disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr, run_loop.QuitClosure(), cb.callback());
  rv = cb.GetResult(std::move(rv));
  EXPECT_THAT(rv.net_error, IsOk());
  ASSERT_TRUE(rv.backend);

  TestEntryResultCompletionCallback cb2;
  EntryResult result =
      rv.backend->CreateEntry("key", net::HIGHEST, cb2.callback());
  result = cb2.GetResult(std::move(result));
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  EXPECT_EQ(kBufSize, WriteData(entry, 0, 0, buffer.get(), kBufSize, false));
  entry->Close();

  rv.backend.reset();

  // Wait till the post-cleanup callback.
  run_loop.Run();

  // All of the payload should be on disk, despite stream 0 being written
  // back in the async Close()
  base::FilePath entry_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex("key", 0));
  std::optional<int64_t> size = base::GetFileSize(entry_path);
  ASSERT_TRUE(size.has_value());
  EXPECT_GT(size.value(), kBufSize);
}

TEST_F(DiskCacheBackendTest, SimpleCreateBackendRecoveryAppCache) {
  // Tests index recovery in APP_CACHE mode. (This is harder to test for
  // DISK_CACHE since post-cleanup callbacks aren't permitted there).
  const int kBufSize = 4 * 1024;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  CacheTestFillBuffer(buffer->data(), kBufSize, true);

  SetSimpleCacheMode();
  SetCacheType(net::APP_CACHE);
  DisableFirstCleanup();
  CleanupCacheDir();

  base::RunLoop run_loop;
  TestBackendResultCompletionCallback cb;

  // Create a backend with post-cleanup callback specified, in order to know
  // when the index has been written back (so it can be deleted race-free).
  disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
      net::APP_CACHE, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr,
      cache_path_, 0, disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr, run_loop.QuitClosure(), cb.callback());
  rv = cb.GetResult(std::move(rv));
  EXPECT_THAT(rv.net_error, IsOk());
  ASSERT_TRUE(rv.backend);

  // Create an entry.
  TestEntryResultCompletionCallback cb2;
  disk_cache::EntryResult result =
      rv.backend->CreateEntry("key", net::HIGHEST, cb2.callback());
  result = cb2.GetResult(std::move(result));
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  EXPECT_EQ(kBufSize, WriteData(entry, 0, 0, buffer.get(), kBufSize, false));
  entry->Close();

  rv.backend.reset();

  // Wait till the post-cleanup callback.
  run_loop.Run();

  // Delete the index.
  base::DeleteFile(
      cache_path_.AppendASCII("index-dir").AppendASCII("the-real-index"));

  // Open the cache again. The fixture will also waits for index init.
  InitCache();

  // Entry should not have a trailer size, since can't tell what it should be
  // when doing recovery (and definitely shouldn't interpret last use time as
  // such).
  EXPECT_EQ(0, simple_cache_impl_->index()->GetTrailerPrefetchSize(
                   disk_cache::simple_util::GetEntryHashKey("key")));
}

// Tests that |BackendImpl| fails to initialize with a missing file.
TEST_F(DiskCacheBackendTest, CreateBackend_MissingFile) {
  ASSERT_TRUE(CopyTestCache("bad_entry"));
  base::FilePath filename = cache_path_.AppendASCII("data_1");
  base::DeleteFile(filename);
  net::TestCompletionCallback cb;

  // Blocking shouldn't be needed to create the cache.
  std::optional<base::ScopedDisallowBlocking> disallow_blocking(std::in_place);
  std::unique_ptr<disk_cache::BackendImpl> cache(
      std::make_unique<disk_cache::BackendImpl>(cache_path_, nullptr, nullptr,
                                                net::DISK_CACHE, nullptr));
  cache->Init(cb.callback());
  EXPECT_THAT(cb.WaitForResult(), IsError(net::ERR_FAILED));
  disallow_blocking.reset();

  cache.reset();
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, MemoryListensToMemoryPressure) {
  const int kLimit = 16 * 1024;
  const int kEntrySize = 256;
  SetMaxSize(kLimit);
  SetMemoryOnlyMode();
  InitCache();

  // Fill in to about 80-90% full.
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kEntrySize);
  CacheTestFillBuffer(buffer->data(), kEntrySize, false);

  for (int i = 0; i < 0.9 * (kLimit / kEntrySize); ++i) {
    disk_cache::Entry* entry = nullptr;
    ASSERT_EQ(net::OK, CreateEntry(base::NumberToString(i), &entry));
    EXPECT_EQ(kEntrySize,
              WriteData(entry, 0, 0, buffer.get(), kEntrySize, true));
    entry->Close();
  }

  EXPECT_GT(CalculateSizeOfAllEntries(), 0.8 * kLimit);

  // Signal low-memory of various sorts, and see how small it gets.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_LT(CalculateSizeOfAllEntries(), 0.5 * kLimit);

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  EXPECT_LT(CalculateSizeOfAllEntries(), 0.1 * kLimit);
}

TEST_F(DiskCacheBackendTest, ExternalFiles) {
  InitCache();
  // First, let's create a file on the folder.
  base::FilePath filename = cache_path_.AppendASCII("f_000001");

  const int kSize = 50;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer1->data(), kSize, false);
  ASSERT_TRUE(base::WriteFile(
      filename, std::string_view(buffer1->data(), static_cast<size_t>(kSize))));

  // Now let's create a file with the cache.
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("key", &entry), IsOk());
  ASSERT_EQ(0, WriteData(entry, 0, 20000, buffer1.get(), 0, false));
  entry->Close();

  // And verify that the first file is still there.
  auto buffer2(base::MakeRefCounted<net::IOBufferWithSize>(kSize));
  ASSERT_EQ(kSize, base::ReadFile(filename, buffer2->data(), kSize));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer2->data(), kSize));
}

// Tests that we deal with file-level pending operations at destruction time.
void DiskCacheBackendTest::BackendShutdownWithPendingFileIO(bool fast) {
  ASSERT_TRUE(CleanupCacheDir());
  uint32_t flags = disk_cache::kNoBuffering;
  if (!fast)
    flags |= disk_cache::kNoRandom;

  if (!simple_cache_mode_)
    UseCurrentThread();
  CreateBackend(flags);

  net::TestCompletionCallback cb;
  int rv = GeneratePendingIO(&cb);

  // The cache destructor will see one pending operation here.
  ResetCaches();

  if (rv == net::ERR_IO_PENDING) {
    if (fast || simple_cache_mode_)
      EXPECT_FALSE(cb.have_result());
    else
      EXPECT_TRUE(cb.have_result());
  }

  base::RunLoop().RunUntilIdle();

#if !BUILDFLAG(IS_IOS)
  // Wait for the actual operation to complete, or we'll keep a file handle that
  // may cause issues later. Note that on iOS systems even though this test
  // uses a single thread, the actual IO is posted to a worker thread and the
  // cache destructor breaks the link to reach cb when the operation completes.
  rv = cb.GetResult(rv);
#endif
}

TEST_F(DiskCacheBackendTest, ShutdownWithPendingFileIO) {
  BackendShutdownWithPendingFileIO(false);
}

// Here and below, tests that simulate crashes are not compiled in LeakSanitizer
// builds because they contain a lot of intentional memory leaks.
#if !defined(LEAK_SANITIZER)
// We'll be leaking from this test.
TEST_F(DiskCacheBackendTest, ShutdownWithPendingFileIO_Fast) {
  // The integrity test sets kNoRandom so there's a version mismatch if we don't
  // force new eviction.
  SetNewEviction();
  BackendShutdownWithPendingFileIO(true);
}
#endif

// See crbug.com/330074
#if !BUILDFLAG(IS_IOS)
// Tests that one cache instance is not affected by another one going away.
TEST_F(DiskCacheBackendTest, MultipleInstancesWithPendingFileIO) {
  base::ScopedTempDir store;
  ASSERT_TRUE(store.CreateUniqueTempDir());

  net::TestCompletionCallback cb;
  TestBackendResultCompletionCallback create_cb;
  disk_cache::BackendResult backend_rv = disk_cache::CreateCacheBackend(
      net::DISK_CACHE, net::CACHE_BACKEND_DEFAULT, /*file_operations=*/nullptr,
      store.GetPath(), 0, disk_cache::ResetHandling::kNeverReset,
      /* net_log = */ nullptr, create_cb.callback());
  backend_rv = create_cb.GetResult(std::move(backend_rv));
  ASSERT_THAT(backend_rv.net_error, IsOk());
  ASSERT_TRUE(backend_rv.backend);

  ASSERT_TRUE(CleanupCacheDir());
  SetNewEviction();  // Match the expected behavior for integrity verification.
  UseCurrentThread();

  CreateBackend(disk_cache::kNoBuffering);
  int rv = GeneratePendingIO(&cb);

  // cache_ has a pending operation, and backend_rv.backend will go away.
  backend_rv.backend.reset();

  if (rv == net::ERR_IO_PENDING)
    EXPECT_FALSE(cb.have_result());

  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  // Wait for the actual operation to complete, or we'll keep a file handle that
  // may cause issues later.
  rv = cb.GetResult(rv);
}
#endif

// Tests that we deal with background-thread pending operations.
void DiskCacheBackendTest::BackendShutdownWithPendingIO(bool fast) {
  TestEntryResultCompletionCallback cb;

  {
    ASSERT_TRUE(CleanupCacheDir());

    uint32_t flags = disk_cache::kNoBuffering;
    if (!fast)
      flags |= disk_cache::kNoRandom;

    CreateBackend(flags);

    EntryResult result =
        cache_->CreateEntry("some key", net::HIGHEST, cb.callback());
    result = cb.GetResult(std::move(result));
    ASSERT_THAT(result.net_error(), IsOk());

    result.ReleaseEntry()->Close();

    // The cache destructor will see one pending operation here.
    ResetCaches();
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(cb.have_result());
}

TEST_F(DiskCacheBackendTest, ShutdownWithPendingIO) {
  BackendShutdownWithPendingIO(false);
}

#if !defined(LEAK_SANITIZER)
// We'll be leaking from this test.
TEST_F(DiskCacheBackendTest, ShutdownWithPendingIO_Fast) {
  // The integrity test sets kNoRandom so there's a version mismatch if we don't
  // force new eviction.
  SetNewEviction();
  BackendShutdownWithPendingIO(true);
}
#endif

// Tests that we deal with create-type pending operations.
void DiskCacheBackendTest::BackendShutdownWithPendingCreate(bool fast) {
  TestEntryResultCompletionCallback cb;

  {
    ASSERT_TRUE(CleanupCacheDir());

    disk_cache::BackendFlags flags =
        fast ? disk_cache::kNone : disk_cache::kNoRandom;
    CreateBackend(flags);

    EntryResult result =
        cache_->CreateEntry("some key", net::HIGHEST, cb.callback());
    ASSERT_THAT(result.net_error(), IsError(net::ERR_IO_PENDING));

    ResetCaches();
    EXPECT_FALSE(cb.have_result());
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(cb.have_result());
}

TEST_F(DiskCacheBackendTest, ShutdownWithPendingCreate) {
  BackendShutdownWithPendingCreate(false);
}

#if !defined(LEAK_SANITIZER)
// We'll be leaking an entry from this test.
TEST_F(DiskCacheBackendTest, ShutdownWithPendingCreate_Fast) {
  // The integrity test sets kNoRandom so there's a version mismatch if we don't
  // force new eviction.
  SetNewEviction();
  BackendShutdownWithPendingCreate(true);
}
#endif

void DiskCacheBackendTest::BackendShutdownWithPendingDoom() {
  net::TestCompletionCallback cb;
  {
    ASSERT_TRUE(CleanupCacheDir());

    disk_cache::BackendFlags flags = disk_cache::kNoRandom;
    CreateBackend(flags);

    TestEntryResultCompletionCallback cb2;
    EntryResult result =
        cache_->CreateEntry("some key", net::HIGHEST, cb2.callback());
    result = cb2.GetResult(std::move(result));
    ASSERT_THAT(result.net_error(), IsOk());
    result.ReleaseEntry()->Close();

    int rv = cache_->DoomEntry("some key", net::HIGHEST, cb.callback());
    ASSERT_THAT(rv, IsError(net::ERR_IO_PENDING));

    ResetCaches();
    EXPECT_FALSE(cb.have_result());
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(cb.have_result());
}

TEST_F(DiskCacheBackendTest, ShutdownWithPendingDoom) {
  BackendShutdownWithPendingDoom();
}

// Disabled on android since this test requires cache creator to create
// blockfile caches.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DiskCacheTest, TruncatedIndex) {
  ASSERT_TRUE(CleanupCacheDir());
  base::FilePath index = cache_path_.AppendASCII("index");
  ASSERT_TRUE(base::WriteFile(index, "hello"));

  TestBackendResultCompletionCallback cb;

  disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
      net::DISK_CACHE, net::CACHE_BACKEND_BLOCKFILE,
      /*file_operations=*/nullptr, cache_path_, 0,
      disk_cache::ResetHandling::kNeverReset, /*net_log=*/nullptr,
      cb.callback());
  rv = cb.GetResult(std::move(rv));
  ASSERT_NE(net::OK, rv.net_error);
  ASSERT_FALSE(rv.backend);
}
#endif

void DiskCacheBackendTest::BackendSetSize() {
  const int cache_size = 0x10000;  // 64 kB
  SetMaxSize(cache_size);
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(first, &entry), IsOk());

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(cache_size);
  memset(buffer->data(), 0, cache_size);
  EXPECT_EQ(cache_size / 10,
            WriteData(entry, 0, 0, buffer.get(), cache_size / 10, false))
      << "normal file";

  EXPECT_EQ(net::ERR_FAILED,
            WriteData(entry, 1, 0, buffer.get(), cache_size / 5, false))
      << "file size above the limit";

  // By doubling the total size, we make this file cacheable.
  SetMaxSize(cache_size * 2);
  EXPECT_EQ(cache_size / 5,
            WriteData(entry, 1, 0, buffer.get(), cache_size / 5, false));

  // Let's fill up the cache!.
  SetMaxSize(cache_size * 10);
  EXPECT_EQ(cache_size * 3 / 4,
            WriteData(entry, 0, 0, buffer.get(), cache_size * 3 / 4, false));
  entry->Close();
  FlushQueueForTest();

  SetMaxSize(cache_size);

  // The cache is 95% full.

  ASSERT_THAT(CreateEntry(second, &entry), IsOk());
  EXPECT_EQ(cache_size / 10,
            WriteData(entry, 0, 0, buffer.get(), cache_size / 10, false));

  disk_cache::Entry* entry2;
  ASSERT_THAT(CreateEntry("an extra key", &entry2), IsOk());
  EXPECT_EQ(cache_size / 10,
            WriteData(entry2, 0, 0, buffer.get(), cache_size / 10, false));
  entry2->Close();  // This will trigger the cache trim.

  EXPECT_NE(net::OK, OpenEntry(first, &entry2));

  FlushQueueForTest();  // Make sure that we are done trimming the cache.
  FlushQueueForTest();  // We may have posted two tasks to evict stuff.

  entry->Close();
  ASSERT_THAT(OpenEntry(second, &entry), IsOk());
  EXPECT_EQ(cache_size / 10, entry->GetDataSize(0));
  entry->Close();
}

TEST_F(DiskCacheBackendTest, SetSize) {
  BackendSetSize();
}

TEST_F(DiskCacheBackendTest, NewEvictionSetSize) {
  SetNewEviction();
  BackendSetSize();
}

TEST_F(DiskCacheBackendTest, MemoryOnlySetSize) {
  SetMemoryOnlyMode();
  BackendSetSize();
}

void DiskCacheBackendTest::BackendLoad() {
  InitCache();
  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  disk_cache::Entry* entries[kLargeNumEntries];
  for (auto*& entry : entries) {
    std::string key = GenerateKey(true);
    ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  }
  EXPECT_EQ(kLargeNumEntries, cache_->GetEntryCount());

  for (int i = 0; i < kLargeNumEntries; i++) {
    int source1 = rand() % kLargeNumEntries;
    int source2 = rand() % kLargeNumEntries;
    disk_cache::Entry* temp = entries[source1];
    entries[source1] = entries[source2];
    entries[source2] = temp;
  }

  for (auto* entry : entries) {
    disk_cache::Entry* new_entry;
    ASSERT_THAT(OpenEntry(entry->GetKey(), &new_entry), IsOk());
    EXPECT_TRUE(new_entry == entry);
    new_entry->Close();
    entry->Doom();
    entry->Close();
  }
  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, Load) {
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, NewEvictionLoad) {
  SetNewEviction();
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyLoad) {
  SetMaxSize(0x100000);
  SetMemoryOnlyMode();
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, AppCacheLoad) {
  SetCacheType(net::APP_CACHE);
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, ShaderCacheLoad) {
  SetCacheType(net::SHADER_CACHE);
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  BackendLoad();
}

// Tests the chaining of an entry to the current head.
void DiskCacheBackendTest::BackendChain() {
  SetMask(0x1);        // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  InitCache();

  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("The first key", &entry), IsOk());
  entry->Close();
  ASSERT_THAT(CreateEntry("The Second key", &entry), IsOk());
  entry->Close();
}

TEST_F(DiskCacheBackendTest, Chain) {
  BackendChain();
}

TEST_F(DiskCacheBackendTest, NewEvictionChain) {
  SetNewEviction();
  BackendChain();
}

TEST_F(DiskCacheBackendTest, AppCacheChain) {
  SetCacheType(net::APP_CACHE);
  BackendChain();
}

TEST_F(DiskCacheBackendTest, ShaderCacheChain) {
  SetCacheType(net::SHADER_CACHE);
  BackendChain();
}

TEST_F(DiskCacheBackendTest, NewEvictionTrim) {
  SetNewEviction();
  InitCache();

  disk_cache::Entry* entry;
  for (int i = 0; i < 100; i++) {
    std::string name(base::StringPrintf("Key %d", i));
    ASSERT_THAT(CreateEntry(name, &entry), IsOk());
    entry->Close();
    if (i < 90) {
      // Entries 0 to 89 are in list 1; 90 to 99 are in list 0.
      ASSERT_THAT(OpenEntry(name, &entry), IsOk());
      entry->Close();
    }
  }

  // The first eviction must come from list 1 (10% limit), the second must come
  // from list 0.
  TrimForTest(false);
  EXPECT_NE(net::OK, OpenEntry("Key 0", &entry));
  TrimForTest(false);
  EXPECT_NE(net::OK, OpenEntry("Key 90", &entry));

  // Double check that we still have the list tails.
  ASSERT_THAT(OpenEntry("Key 1", &entry), IsOk());
  entry->Close();
  ASSERT_THAT(OpenEntry("Key 91", &entry), IsOk());
  entry->Close();
}

// Before looking for invalid entries, let's check a valid entry.
void DiskCacheBackendTest::BackendValidEntry() {
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 50;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer1->data(), 0, kSize);
  base::strlcpy(buffer1->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer1.get(), kSize, false));
  entry->Close();
  SimulateCrash();

  ASSERT_THAT(OpenEntry(key, &entry), IsOk());

  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer2->data(), 0, kSize);
  EXPECT_EQ(kSize, ReadData(entry, 0, 0, buffer2.get(), kSize));
  entry->Close();
  EXPECT_STREQ(buffer1->data(), buffer2->data());
}

TEST_F(DiskCacheBackendTest, ValidEntry) {
  BackendValidEntry();
}

TEST_F(DiskCacheBackendTest, NewEvictionValidEntry) {
  SetNewEviction();
  BackendValidEntry();
}

// The same logic of the previous test (ValidEntry), but this time force the
// entry to be invalid, simulating a crash in the middle.
// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntry() {
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 50;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer->data(), 0, kSize);
  base::strlcpy(buffer->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer.get(), kSize, false));
  SimulateCrash();

  EXPECT_NE(net::OK, OpenEntry(key, &entry));
  EXPECT_EQ(0, cache_->GetEntryCount());
}

#if !defined(LEAK_SANITIZER)
// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntry) {
  BackendInvalidEntry();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry) {
  SetNewEviction();
  BackendInvalidEntry();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, AppCacheInvalidEntry) {
  SetCacheType(net::APP_CACHE);
  BackendInvalidEntry();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, ShaderCacheInvalidEntry) {
  SetCacheType(net::SHADER_CACHE);
  BackendInvalidEntry();
}

// Almost the same test, but this time crash the cache after reading an entry.
// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntryRead() {
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 50;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer->data(), 0, kSize);
  base::strlcpy(buffer->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer.get(), kSize, false));
  entry->Close();
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_EQ(kSize, ReadData(entry, 0, 0, buffer.get(), kSize));

  SimulateCrash();

  if (type_ == net::APP_CACHE) {
    // Reading an entry and crashing should not make it dirty.
    ASSERT_THAT(OpenEntry(key, &entry), IsOk());
    EXPECT_EQ(1, cache_->GetEntryCount());
    entry->Close();
  } else {
    EXPECT_NE(net::OK, OpenEntry(key, &entry));
    EXPECT_EQ(0, cache_->GetEntryCount());
  }
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryRead) {
  BackendInvalidEntryRead();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntryRead) {
  SetNewEviction();
  BackendInvalidEntryRead();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, AppCacheInvalidEntryRead) {
  SetCacheType(net::APP_CACHE);
  BackendInvalidEntryRead();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, ShaderCacheInvalidEntryRead) {
  SetCacheType(net::SHADER_CACHE);
  BackendInvalidEntryRead();
}

// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntryWithLoad() {
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  InitCache();

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  const int kNumEntries = 100;
  disk_cache::Entry* entries[kNumEntries];
  for (auto*& entry : entries) {
    std::string key = GenerateKey(true);
    ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  }
  EXPECT_EQ(kNumEntries, cache_->GetEntryCount());

  for (int i = 0; i < kNumEntries; i++) {
    int source1 = rand() % kNumEntries;
    int source2 = rand() % kNumEntries;
    disk_cache::Entry* temp = entries[source1];
    entries[source1] = entries[source2];
    entries[source2] = temp;
  }

  std::string keys[kNumEntries];
  for (int i = 0; i < kNumEntries; i++) {
    keys[i] = entries[i]->GetKey();
    if (i < kNumEntries / 2)
      entries[i]->Close();
  }

  SimulateCrash();

  for (int i = kNumEntries / 2; i < kNumEntries; i++) {
    disk_cache::Entry* entry;
    EXPECT_NE(net::OK, OpenEntry(keys[i], &entry));
  }

  for (int i = 0; i < kNumEntries / 2; i++) {
    disk_cache::Entry* entry;
    ASSERT_THAT(OpenEntry(keys[i], &entry), IsOk());
    entry->Close();
  }

  EXPECT_EQ(kNumEntries / 2, cache_->GetEntryCount());
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryWithLoad) {
  BackendInvalidEntryWithLoad();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntryWithLoad) {
  SetNewEviction();
  BackendInvalidEntryWithLoad();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, AppCacheInvalidEntryWithLoad) {
  SetCacheType(net::APP_CACHE);
  BackendInvalidEntryWithLoad();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, ShaderCacheInvalidEntryWithLoad) {
  SetCacheType(net::SHADER_CACHE);
  BackendInvalidEntryWithLoad();
}

// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendTrimInvalidEntry() {
  const int kSize = 0x3000;  // 12 kB
  SetMaxSize(kSize * 10);
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(first, &entry), IsOk());

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer->data(), 0, kSize);
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer.get(), kSize, false));

  // Simulate a crash.
  SimulateCrash();

  ASSERT_THAT(CreateEntry(second, &entry), IsOk());
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer.get(), kSize, false));

  EXPECT_EQ(2, cache_->GetEntryCount());
  SetMaxSize(kSize);
  entry->Close();  // Trim the cache.
  FlushQueueForTest();

  // If we evicted the entry in less than 20mS, we have one entry in the cache;
  // if it took more than that, we posted a task and we'll delete the second
  // entry too.
  base::RunLoop().RunUntilIdle();

  // This may be not thread-safe in general, but for now it's OK so add some
  // ThreadSanitizer annotations to ignore data races on cache_.
  // See http://crbug.com/55970
  ABSL_ANNOTATE_IGNORE_READS_BEGIN();
  EXPECT_GE(1, cache_->GetEntryCount());
  ABSL_ANNOTATE_IGNORE_READS_END();

  EXPECT_NE(net::OK, OpenEntry(first, &entry));
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, TrimInvalidEntry) {
  BackendTrimInvalidEntry();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionTrimInvalidEntry) {
  SetNewEviction();
  BackendTrimInvalidEntry();
}

// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendTrimInvalidEntry2() {
  SetMask(0xf);  // 16-entry table.

  const int kSize = 0x3000;  // 12 kB
  SetMaxSize(kSize * 40);
  InitCache();

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer->data(), 0, kSize);
  disk_cache::Entry* entry;

  // Writing 32 entries to this cache chains most of them.
  for (int i = 0; i < 32; i++) {
    std::string key(base::StringPrintf("some key %d", i));
    ASSERT_THAT(CreateEntry(key, &entry), IsOk());
    EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer.get(), kSize, false));
    entry->Close();
    ASSERT_THAT(OpenEntry(key, &entry), IsOk());
    // Note that we are not closing the entries.
  }

  // Simulate a crash.
  SimulateCrash();

  ASSERT_THAT(CreateEntry("Something else", &entry), IsOk());
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer.get(), kSize, false));

  FlushQueueForTest();
  EXPECT_EQ(33, cache_->GetEntryCount());
  SetMaxSize(kSize);

  // For the new eviction code, all corrupt entries are on the second list so
  // they are not going away that easy.
  if (new_eviction_) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
  }

  entry->Close();  // Trim the cache.
  FlushQueueForTest();

  // We may abort the eviction before cleaning up everything.
  base::RunLoop().RunUntilIdle();
  FlushQueueForTest();
  // If it's not clear enough: we may still have eviction tasks running at this
  // time, so the number of entries is changing while we read it.
  ABSL_ANNOTATE_IGNORE_READS_AND_WRITES_BEGIN();
  EXPECT_GE(30, cache_->GetEntryCount());
  ABSL_ANNOTATE_IGNORE_READS_AND_WRITES_END();

  // For extra messiness, the integrity check for the cache can actually cause
  // evictions if it's over-capacity, which would race with above. So change the
  // size we pass to CheckCacheIntegrity (but don't mess with existing backend's
  // state.
  size_ = 0;
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, TrimInvalidEntry2) {
  BackendTrimInvalidEntry2();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionTrimInvalidEntry2) {
  SetNewEviction();
  BackendTrimInvalidEntry2();
}
#endif  // !defined(LEAK_SANITIZER)

void DiskCacheBackendTest::BackendEnumerations() {
  InitCache();
  Time initial = Time::Now();

  const int kNumEntries = 100;
  for (int i = 0; i < kNumEntries; i++) {
    std::string key = GenerateKey(true);
    disk_cache::Entry* entry;
    ASSERT_THAT(CreateEntry(key, &entry), IsOk());
    entry->Close();
  }
  EXPECT_EQ(kNumEntries, cache_->GetEntryCount());
  Time final = Time::Now();

  disk_cache::Entry* entry;
  std::unique_ptr<TestIterator> iter = CreateIterator();
  int count = 0;
  Time last_modified[kNumEntries];
  Time last_used[kNumEntries];
  while (iter->OpenNextEntry(&entry) == net::OK) {
    ASSERT_TRUE(nullptr != entry);
    if (count < kNumEntries) {
      last_modified[count] = entry->GetLastModified();
      last_used[count] = entry->GetLastUsed();
      EXPECT_TRUE(initial <= last_modified[count]);
      EXPECT_TRUE(final >= last_modified[count]);
    }

    entry->Close();
    count++;
  };
  EXPECT_EQ(kNumEntries, count);

  iter = CreateIterator();
  count = 0;
  // The previous enumeration should not have changed the timestamps.
  while (iter->OpenNextEntry(&entry) == net::OK) {
    ASSERT_TRUE(nullptr != entry);
    if (count < kNumEntries) {
      EXPECT_TRUE(last_modified[count] == entry->GetLastModified());
      EXPECT_TRUE(last_used[count] == entry->GetLastUsed());
    }
    entry->Close();
    count++;
  };
  EXPECT_EQ(kNumEntries, count);
}

TEST_F(DiskCacheBackendTest, Enumerations) {
  BackendEnumerations();
}

TEST_F(DiskCacheBackendTest, NewEvictionEnumerations) {
  SetNewEviction();
  BackendEnumerations();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyEnumerations) {
  SetMemoryOnlyMode();
  BackendEnumerations();
}

TEST_F(DiskCacheBackendTest, ShaderCacheEnumerations) {
  SetCacheType(net::SHADER_CACHE);
  BackendEnumerations();
}

TEST_F(DiskCacheBackendTest, AppCacheEnumerations) {
  SetCacheType(net::APP_CACHE);
  BackendEnumerations();
}

// Verifies enumerations while entries are open.
void DiskCacheBackendTest::BackendEnumerations2() {
  InitCache();
  const std::string first("first");
  const std::string second("second");
  disk_cache::Entry *entry1, *entry2;
  ASSERT_THAT(CreateEntry(first, &entry1), IsOk());
  entry1->Close();
  ASSERT_THAT(CreateEntry(second, &entry2), IsOk());
  entry2->Close();
  FlushQueueForTest();

  // Make sure that the timestamp is not the same.
  AddDelay();
  ASSERT_THAT(OpenEntry(second, &entry1), IsOk());
  std::unique_ptr<TestIterator> iter = CreateIterator();
  ASSERT_THAT(iter->OpenNextEntry(&entry2), IsOk());
  EXPECT_EQ(entry2->GetKey(), second);

  // Two entries and the iterator pointing at "first".
  entry1->Close();
  entry2->Close();

  // The iterator should still be valid, so we should not crash.
  ASSERT_THAT(iter->OpenNextEntry(&entry2), IsOk());
  EXPECT_EQ(entry2->GetKey(), first);
  entry2->Close();
  iter = CreateIterator();

  // Modify the oldest entry and get the newest element.
  ASSERT_THAT(OpenEntry(first, &entry1), IsOk());
  EXPECT_EQ(0, WriteData(entry1, 0, 200, nullptr, 0, false));
  ASSERT_THAT(iter->OpenNextEntry(&entry2), IsOk());
  if (type_ == net::APP_CACHE) {
    // The list is not updated.
    EXPECT_EQ(entry2->GetKey(), second);
  } else {
    EXPECT_EQ(entry2->GetKey(), first);
  }

  entry1->Close();
  entry2->Close();
}

TEST_F(DiskCacheBackendTest, Enumerations2) {
  BackendEnumerations2();
}

TEST_F(DiskCacheBackendTest, NewEvictionEnumerations2) {
  SetNewEviction();
  BackendEnumerations2();
}

TEST_F(DiskCacheBackendTest, AppCacheEnumerations2) {
  SetCacheType(net::APP_CACHE);
  BackendEnumerations2();
}

TEST_F(DiskCacheBackendTest, ShaderCacheEnumerations2) {
  SetCacheType(net::SHADER_CACHE);
  BackendEnumerations2();
}

void DiskCacheBackendTest::BackendDoomMidEnumeration() {
  InitCache();

  const int kNumEntries = 100;
  std::set<std::string> keys;
  for (int i = 0; i < kNumEntries; i++) {
    std::string key = GenerateKey(true);
    keys.insert(key);
    disk_cache::Entry* entry;
    ASSERT_THAT(CreateEntry(key, &entry), IsOk());
    entry->Close();
  }

  disk_cache::Entry* entry;
  std::unique_ptr<TestIterator> iter = CreateIterator();
  int count = 0;
  while (iter->OpenNextEntry(&entry) == net::OK) {
    if (count == 0) {
      // Delete a random entry from the cache while in the midst of iteration.
      auto key_to_doom = keys.begin();
      while (*key_to_doom == entry->GetKey())
        key_to_doom++;
      ASSERT_THAT(DoomEntry(*key_to_doom), IsOk());
      ASSERT_EQ(1u, keys.erase(*key_to_doom));
    }
    ASSERT_NE(nullptr, entry);
    EXPECT_EQ(1u, keys.erase(entry->GetKey()));
    entry->Close();
    count++;
  };

  EXPECT_EQ(kNumEntries - 1, cache_->GetEntryCount());
  EXPECT_EQ(0u, keys.size());
}

TEST_F(DiskCacheBackendTest, DoomEnumerations) {
  BackendDoomMidEnumeration();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomEnumerations) {
  SetNewEviction();
  BackendDoomMidEnumeration();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomEnumerations) {
  SetMemoryOnlyMode();
  BackendDoomMidEnumeration();
}

TEST_F(DiskCacheBackendTest, ShaderCacheDoomEnumerations) {
  SetCacheType(net::SHADER_CACHE);
  BackendDoomMidEnumeration();
}

TEST_F(DiskCacheBackendTest, AppCacheDoomEnumerations) {
  SetCacheType(net::APP_CACHE);
  BackendDoomMidEnumeration();
}

TEST_F(DiskCacheBackendTest, SimpleDoomEnumerations) {
  SetSimpleCacheMode();
  BackendDoomMidEnumeration();
}

// Verify that ReadData calls do not update the LRU cache
// when using the SHADER_CACHE type.
TEST_F(DiskCacheBackendTest, ShaderCacheEnumerationReadData) {
  SetCacheType(net::SHADER_CACHE);
  InitCache();
  const std::string first("first");
  const std::string second("second");
  disk_cache::Entry *entry1, *entry2;
  const int kSize = 50;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);

  ASSERT_THAT(CreateEntry(first, &entry1), IsOk());
  memset(buffer1->data(), 0, kSize);
  base::strlcpy(buffer1->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, WriteData(entry1, 0, 0, buffer1.get(), kSize, false));

  ASSERT_THAT(CreateEntry(second, &entry2), IsOk());
  entry2->Close();

  FlushQueueForTest();

  // Make sure that the timestamp is not the same.
  AddDelay();

  // Read from the last item in the LRU.
  EXPECT_EQ(kSize, ReadData(entry1, 0, 0, buffer1.get(), kSize));
  entry1->Close();

  std::unique_ptr<TestIterator> iter = CreateIterator();
  ASSERT_THAT(iter->OpenNextEntry(&entry2), IsOk());
  EXPECT_EQ(entry2->GetKey(), second);
  entry2->Close();
}

#if !defined(LEAK_SANITIZER)
// Verify handling of invalid entries while doing enumerations.
// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntryEnumeration() {
  InitCache();

  std::string key("Some key");
  disk_cache::Entry *entry, *entry1, *entry2;
  ASSERT_THAT(CreateEntry(key, &entry1), IsOk());

  const int kSize = 50;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer1->data(), 0, kSize);
  base::strlcpy(buffer1->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, WriteData(entry1, 0, 0, buffer1.get(), kSize, false));
  entry1->Close();
  ASSERT_THAT(OpenEntry(key, &entry1), IsOk());
  EXPECT_EQ(kSize, ReadData(entry1, 0, 0, buffer1.get(), kSize));

  std::string key2("Another key");
  ASSERT_THAT(CreateEntry(key2, &entry2), IsOk());
  entry2->Close();
  ASSERT_EQ(2, cache_->GetEntryCount());

  SimulateCrash();

  std::unique_ptr<TestIterator> iter = CreateIterator();
  int count = 0;
  while (iter->OpenNextEntry(&entry) == net::OK) {
    ASSERT_TRUE(nullptr != entry);
    EXPECT_EQ(key2, entry->GetKey());
    entry->Close();
    count++;
  };
  EXPECT_EQ(1, count);
  EXPECT_EQ(1, cache_->GetEntryCount());
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryEnumeration) {
  BackendInvalidEntryEnumeration();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntryEnumeration) {
  SetNewEviction();
  BackendInvalidEntryEnumeration();
}
#endif  // !defined(LEAK_SANITIZER)

// Tests that if for some reason entries are modified close to existing cache
// iterators, we don't generate fatal errors or reset the cache.
void DiskCacheBackendTest::BackendFixEnumerators() {
  InitCache();

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  const int kNumEntries = 10;
  for (int i = 0; i < kNumEntries; i++) {
    std::string key = GenerateKey(true);
    disk_cache::Entry* entry;
    ASSERT_THAT(CreateEntry(key, &entry), IsOk());
    entry->Close();
  }
  EXPECT_EQ(kNumEntries, cache_->GetEntryCount());

  disk_cache::Entry *entry1, *entry2;
  std::unique_ptr<TestIterator> iter1 = CreateIterator(),
                                iter2 = CreateIterator();
  ASSERT_THAT(iter1->OpenNextEntry(&entry1), IsOk());
  ASSERT_TRUE(nullptr != entry1);
  entry1->Close();
  entry1 = nullptr;

  // Let's go to the middle of the list.
  for (int i = 0; i < kNumEntries / 2; i++) {
    if (entry1)
      entry1->Close();
    ASSERT_THAT(iter1->OpenNextEntry(&entry1), IsOk());
    ASSERT_TRUE(nullptr != entry1);

    ASSERT_THAT(iter2->OpenNextEntry(&entry2), IsOk());
    ASSERT_TRUE(nullptr != entry2);
    entry2->Close();
  }

  // Messing up with entry1 will modify entry2->next.
  entry1->Doom();
  ASSERT_THAT(iter2->OpenNextEntry(&entry2), IsOk());
  ASSERT_TRUE(nullptr != entry2);

  // The link entry2->entry1 should be broken.
  EXPECT_NE(entry2->GetKey(), entry1->GetKey());
  entry1->Close();
  entry2->Close();

  // And the second iterator should keep working.
  ASSERT_THAT(iter2->OpenNextEntry(&entry2), IsOk());
  ASSERT_TRUE(nullptr != entry2);
  entry2->Close();
}

TEST_F(DiskCacheBackendTest, FixEnumerators) {
  BackendFixEnumerators();
}

TEST_F(DiskCacheBackendTest, NewEvictionFixEnumerators) {
  SetNewEviction();
  BackendFixEnumerators();
}

void DiskCacheBackendTest::BackendDoomRecent() {
  InitCache();

  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("first", &entry), IsOk());
  entry->Close();
  ASSERT_THAT(CreateEntry("second", &entry), IsOk());
  entry->Close();
  FlushQueueForTest();

  AddDelay();
  Time middle = Time::Now();

  ASSERT_THAT(CreateEntry("third", &entry), IsOk());
  entry->Close();
  ASSERT_THAT(CreateEntry("fourth", &entry), IsOk());
  entry->Close();
  FlushQueueForTest();

  AddDelay();
  Time final = Time::Now();

  ASSERT_EQ(4, cache_->GetEntryCount());
  EXPECT_THAT(DoomEntriesSince(final), IsOk());
  ASSERT_EQ(4, cache_->GetEntryCount());

  EXPECT_THAT(DoomEntriesSince(middle), IsOk());
  ASSERT_EQ(2, cache_->GetEntryCount());

  ASSERT_THAT(OpenEntry("second", &entry), IsOk());
  entry->Close();
}

TEST_F(DiskCacheBackendTest, DoomRecent) {
  BackendDoomRecent();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomRecent) {
  SetNewEviction();
  BackendDoomRecent();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomRecent) {
  SetMemoryOnlyMode();
  BackendDoomRecent();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomEntriesSinceSparse) {
  SetMemoryOnlyMode();
  base::Time start;
  InitSparseCache(&start, nullptr);
  DoomEntriesSince(start);
  EXPECT_EQ(1, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DoomEntriesSinceSparse) {
  base::Time start;
  InitSparseCache(&start, nullptr);
  DoomEntriesSince(start);
  // NOTE: BackendImpl counts child entries in its GetEntryCount(), while
  // MemBackendImpl does not. Thats why expected value differs here from
  // MemoryOnlyDoomEntriesSinceSparse.
  EXPECT_EQ(3, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomAllSparse) {
  SetMemoryOnlyMode();
  InitSparseCache(nullptr, nullptr);
  EXPECT_THAT(DoomAllEntries(), IsOk());
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DoomAllSparse) {
  InitSparseCache(nullptr, nullptr);
  EXPECT_THAT(DoomAllEntries(), IsOk());
  EXPECT_EQ(0, cache_->GetEntryCount());
}

// This test is for https://crbug.com/827492.
TEST_F(DiskCacheBackendTest, InMemorySparseEvict) {
  const int kMaxSize = 512;

  SetMaxSize(kMaxSize);
  SetMemoryOnlyMode();
  InitCache();

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(64);
  CacheTestFillBuffer(buffer->data(), 64, false /* no_nulls */);

  std::vector<disk_cache::ScopedEntryPtr> entries;

  disk_cache::Entry* entry = nullptr;
  // Create a bunch of entries
  for (size_t i = 0; i < 14; i++) {
    std::string name = "http://www." + base::NumberToString(i) + ".com/";
    ASSERT_THAT(CreateEntry(name, &entry), IsOk());
    entries.push_back(disk_cache::ScopedEntryPtr(entry));
  }

  // Create several sparse entries and fill with enough data to
  // pass eviction threshold
  ASSERT_EQ(64, WriteSparseData(entries[0].get(), 0, buffer.get(), 64));
  ASSERT_EQ(net::ERR_FAILED,
            WriteSparseData(entries[0].get(), 10000, buffer.get(), 4));
  ASSERT_EQ(63, WriteSparseData(entries[1].get(), 0, buffer.get(), 63));
  ASSERT_EQ(64, WriteSparseData(entries[2].get(), 0, buffer.get(), 64));
  ASSERT_EQ(64, WriteSparseData(entries[3].get(), 0, buffer.get(), 64));

  // Close all the entries, leaving a populated LRU list
  // with all entries having refcount 0 (doom implies deletion)
  entries.clear();

  // Create a new entry, triggering buggy eviction
  ASSERT_THAT(CreateEntry("http://www.14.com/", &entry), IsOk());
  entry->Close();
}

void DiskCacheBackendTest::BackendDoomBetween() {
  InitCache();

  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("first", &entry), IsOk());
  entry->Close();
  FlushQueueForTest();

  AddDelay();
  Time middle_start = Time::Now();

  ASSERT_THAT(CreateEntry("second", &entry), IsOk());
  entry->Close();
  ASSERT_THAT(CreateEntry("third", &entry), IsOk());
  entry->Close();
  FlushQueueForTest();

  AddDelay();
  Time middle_end = Time::Now();

  ASSERT_THAT(CreateEntry("fourth", &entry), IsOk());
  entry->Close();
  ASSERT_THAT(OpenEntry("fourth", &entry), IsOk());
  entry->Close();
  FlushQueueForTest();

  AddDelay();
  Time final = Time::Now();

  ASSERT_EQ(4, cache_->GetEntryCount());
  EXPECT_THAT(DoomEntriesBetween(middle_start, middle_end), IsOk());
  ASSERT_EQ(2, cache_->GetEntryCount());

  ASSERT_THAT(OpenEntry("fourth", &entry), IsOk());
  entry->Close();

  EXPECT_THAT(DoomEntriesBetween(middle_start, final), IsOk());
  ASSERT_EQ(1, cache_->GetEntryCount());

  ASSERT_THAT(OpenEntry("first", &entry), IsOk());
  entry->Close();
}

TEST_F(DiskCacheBackendTest, DoomBetween) {
  BackendDoomBetween();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomBetween) {
  SetNewEviction();
  BackendDoomBetween();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomBetween) {
  SetMemoryOnlyMode();
  BackendDoomBetween();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomEntriesBetweenSparse) {
  SetMemoryOnlyMode();
  base::Time start, end;
  InitSparseCache(&start, &end);
  DoomEntriesBetween(start, end);
  EXPECT_EQ(3, cache_->GetEntryCount());

  start = end;
  end = base::Time::Now();
  DoomEntriesBetween(start, end);
  EXPECT_EQ(1, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DoomEntriesBetweenSparse) {
  base::Time start, end;
  InitSparseCache(&start, &end);
  DoomEntriesBetween(start, end);
  EXPECT_EQ(9, cache_->GetEntryCount());

  start = end;
  end = base::Time::Now();
  DoomEntriesBetween(start, end);
  EXPECT_EQ(3, cache_->GetEntryCount());
}

void DiskCacheBackendTest::BackendCalculateSizeOfAllEntries() {
  InitCache();

  // The cache is initially empty.
  EXPECT_EQ(0, CalculateSizeOfAllEntries());

  // Generate random entries and populate them with data of respective
  // sizes 0, 1, ..., count - 1 bytes.
  std::set<std::string> key_pool;
  CreateSetOfRandomEntries(&key_pool);

  int count = 0;
  int total_size = 0;
  for (std::string key : key_pool) {
    std::string data(count, ' ');
    scoped_refptr<net::StringIOBuffer> buffer =
        base::MakeRefCounted<net::StringIOBuffer>(data);

    // Alternate between writing to first two streams to test that we do not
    // take only one stream into account.
    disk_cache::Entry* entry;
    ASSERT_THAT(OpenEntry(key, &entry), IsOk());
    ASSERT_EQ(count, WriteData(entry, count % 2, 0, buffer.get(), count, true));
    entry->Close();

    total_size += GetRoundedSize(count + GetEntryMetadataSize(key));
    ++count;
  }

  int result = CalculateSizeOfAllEntries();
  EXPECT_EQ(total_size, result);

  // Add another entry and test if the size is updated. Then remove it and test
  // if the size is back to original value.
  {
    const int last_entry_size = 47;
    std::string data(last_entry_size, ' ');
    scoped_refptr<net::StringIOBuffer> buffer =
        base::MakeRefCounted<net::StringIOBuffer>(data);

    disk_cache::Entry* entry;
    std::string key = GenerateKey(true);
    ASSERT_THAT(CreateEntry(key, &entry), IsOk());
    ASSERT_EQ(last_entry_size,
              WriteData(entry, 0, 0, buffer.get(), last_entry_size, true));
    entry->Close();

    int new_result = CalculateSizeOfAllEntries();
    EXPECT_EQ(
        result + GetRoundedSize(last_entry_size + GetEntryMetadataSize(key)),
        new_result);

    DoomEntry(key);
    new_result = CalculateSizeOfAllEntries();
    EXPECT_EQ(result, new_result);
  }

  // After dooming the entries, the size should be back to zero.
  ASSERT_THAT(DoomAllEntries(), IsOk());
  EXPECT_EQ(0, CalculateSizeOfAllEntries());
}

TEST_F(DiskCacheBackendTest, CalculateSizeOfAllEntries) {
  BackendCalculateSizeOfAllEntries();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyCalculateSizeOfAllEntries) {
  SetMemoryOnlyMode();
  BackendCalculateSizeOfAllEntries();
}

TEST_F(DiskCacheBackendTest, SimpleCacheCalculateSizeOfAllEntries) {
  // Use net::APP_CACHE to make size estimations deterministic via
  // non-optimistic writes.
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  BackendCalculateSizeOfAllEntries();
}

void DiskCacheBackendTest::BackendCalculateSizeOfEntriesBetween(
    bool expect_access_time_comparisons) {
  InitCache();

  EXPECT_EQ(0, CalculateSizeOfEntriesBetween(base::Time(), base::Time::Max()));

  Time start = Time::Now();

  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("first", &entry), IsOk());
  entry->Close();
  FlushQueueForTest();
  base::RunLoop().RunUntilIdle();

  AddDelay();
  Time middle = Time::Now();
  AddDelay();

  ASSERT_THAT(CreateEntry("second", &entry), IsOk());
  entry->Close();
  ASSERT_THAT(CreateEntry("third_entry", &entry), IsOk());
  entry->Close();
  FlushQueueForTest();
  base::RunLoop().RunUntilIdle();

  AddDelay();
  Time end = Time::Now();

  int size_1 = GetRoundedSize(GetEntryMetadataSize("first"));
  int size_2 = GetRoundedSize(GetEntryMetadataSize("second"));
  int size_3 = GetRoundedSize(GetEntryMetadataSize("third_entry"));

  ASSERT_EQ(3, cache_->GetEntryCount());
  ASSERT_EQ(CalculateSizeOfAllEntries(),
            CalculateSizeOfEntriesBetween(base::Time(), base::Time::Max()));

  if (expect_access_time_comparisons) {
    int start_end = CalculateSizeOfEntriesBetween(start, end);
    ASSERT_EQ(CalculateSizeOfAllEntries(), start_end);
    ASSERT_EQ(size_1 + size_2 + size_3, start_end);

    ASSERT_EQ(size_1, CalculateSizeOfEntriesBetween(start, middle));
    ASSERT_EQ(size_2 + size_3, CalculateSizeOfEntriesBetween(middle, end));
  }

  // After dooming the entries, the size should be back to zero.
  ASSERT_THAT(DoomAllEntries(), IsOk());
  EXPECT_EQ(0, CalculateSizeOfEntriesBetween(base::Time(), base::Time::Max()));
}

TEST_F(DiskCacheBackendTest, CalculateSizeOfEntriesBetween) {
  InitCache();
  ASSERT_EQ(net::ERR_NOT_IMPLEMENTED,
            CalculateSizeOfEntriesBetween(base::Time(), base::Time::Max()));
}

TEST_F(DiskCacheBackendTest, MemoryOnlyCalculateSizeOfEntriesBetween) {
  SetMemoryOnlyMode();
  BackendCalculateSizeOfEntriesBetween(true);
}

TEST_F(DiskCacheBackendTest, SimpleCacheCalculateSizeOfEntriesBetween) {
  // Test normal mode in where access time range comparisons are supported.
  SetSimpleCacheMode();
  BackendCalculateSizeOfEntriesBetween(true);
}

TEST_F(DiskCacheBackendTest, SimpleCacheAppCacheCalculateSizeOfEntriesBetween) {
  // Test SimpleCache in APP_CACHE mode separately since it does not support
  // access time range comparisons.
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  BackendCalculateSizeOfEntriesBetween(false);
}

void DiskCacheBackendTest::BackendTransaction(const std::string& name,
                                              int num_entries,
                                              bool load) {
  success_ = false;
  ASSERT_TRUE(CopyTestCache(name));
  DisableFirstCleanup();

  uint32_t mask;
  if (load) {
    mask = 0xf;
    SetMaxSize(0x100000);
  } else {
    // Clear the settings from the previous run.
    mask = 0;
    SetMaxSize(0);
  }
  SetMask(mask);

  InitCache();
  ASSERT_EQ(num_entries + 1, cache_->GetEntryCount());

  std::string key("the first key");
  disk_cache::Entry* entry1;
  ASSERT_NE(net::OK, OpenEntry(key, &entry1));

  int actual = cache_->GetEntryCount();
  if (num_entries != actual) {
    ASSERT_TRUE(load);
    // If there is a heavy load, inserting an entry will make another entry
    // dirty (on the hash bucket) so two entries are removed.
    ASSERT_EQ(num_entries - 1, actual);
  }

  ResetCaches();

  ASSERT_TRUE(CheckCacheIntegrity(cache_path_, new_eviction_, MaxSize(), mask));
  success_ = true;
}

void DiskCacheBackendTest::BackendRecoverInsert() {
  // Tests with an empty cache.
  BackendTransaction("insert_empty1", 0, false);
  ASSERT_TRUE(success_) << "insert_empty1";
  BackendTransaction("insert_empty2", 0, false);
  ASSERT_TRUE(success_) << "insert_empty2";
  BackendTransaction("insert_empty3", 0, false);
  ASSERT_TRUE(success_) << "insert_empty3";

  // Tests with one entry on the cache.
  BackendTransaction("insert_one1", 1, false);
  ASSERT_TRUE(success_) << "insert_one1";
  BackendTransaction("insert_one2", 1, false);
  ASSERT_TRUE(success_) << "insert_one2";
  BackendTransaction("insert_one3", 1, false);
  ASSERT_TRUE(success_) << "insert_one3";

  // Tests with one hundred entries on the cache, tiny index.
  BackendTransaction("insert_load1", 100, true);
  ASSERT_TRUE(success_) << "insert_load1";
  BackendTransaction("insert_load2", 100, true);
  ASSERT_TRUE(success_) << "insert_load2";
}

TEST_F(DiskCacheBackendTest, RecoverInsert) {
  BackendRecoverInsert();
}

TEST_F(DiskCacheBackendTest, NewEvictionRecoverInsert) {
  SetNewEviction();
  BackendRecoverInsert();
}

void DiskCacheBackendTest::BackendRecoverRemove() {
  // Removing the only element.
  BackendTransaction("remove_one1", 0, false);
  ASSERT_TRUE(success_) << "remove_one1";
  BackendTransaction("remove_one2", 0, false);
  ASSERT_TRUE(success_) << "remove_one2";
  BackendTransaction("remove_one3", 0, false);
  ASSERT_TRUE(success_) << "remove_one3";

  // Removing the head.
  BackendTransaction("remove_head1", 1, false);
  ASSERT_TRUE(success_) << "remove_head1";
  BackendTransaction("remove_head2", 1, false);
  ASSERT_TRUE(success_) << "remove_head2";
  BackendTransaction("remove_head3", 1, false);
  ASSERT_TRUE(success_) << "remove_head3";

  // Removing the tail.
  BackendTransaction("remove_tail1", 1, false);
  ASSERT_TRUE(success_) << "remove_tail1";
  BackendTransaction("remove_tail2", 1, false);
  ASSERT_TRUE(success_) << "remove_tail2";
  BackendTransaction("remove_tail3", 1, false);
  ASSERT_TRUE(success_) << "remove_tail3";

  // Removing with one hundred entries on the cache, tiny index.
  BackendTransaction("remove_load1", 100, true);
  ASSERT_TRUE(success_) << "remove_load1";
  BackendTransaction("remove_load2", 100, true);
  ASSERT_TRUE(success_) << "remove_load2";
  BackendTransaction("remove_load3", 100, true);
  ASSERT_TRUE(success_) << "remove_load3";

  // This case cannot be reverted.
  BackendTransaction("remove_one4", 0, false);
  ASSERT_TRUE(success_) << "remove_one4";
  BackendTransaction("remove_head4", 1, false);
  ASSERT_TRUE(success_) << "remove_head4";
}

#if BUILDFLAG(IS_WIN)
// http://crbug.com/396392
#define MAYBE_RecoverRemove DISABLED_RecoverRemove
#else
#define MAYBE_RecoverRemove RecoverRemove
#endif
TEST_F(DiskCacheBackendTest, MAYBE_RecoverRemove) {
  BackendRecoverRemove();
}

#if BUILDFLAG(IS_WIN)
// http://crbug.com/396392
#define MAYBE_NewEvictionRecoverRemove DISABLED_NewEvictionRecoverRemove
#else
#define MAYBE_NewEvictionRecoverRemove NewEvictionRecoverRemove
#endif
TEST_F(DiskCacheBackendTest, MAYBE_NewEvictionRecoverRemove) {
  SetNewEviction();
  BackendRecoverRemove();
}

void DiskCacheBackendTest::BackendRecoverWithEviction() {
  success_ = false;
  ASSERT_TRUE(CopyTestCache("insert_load1"));
  DisableFirstCleanup();

  SetMask(0xf);
  SetMaxSize(0x1000);

  // We should not crash here.
  InitCache();
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, RecoverWithEviction) {
  BackendRecoverWithEviction();
}

TEST_F(DiskCacheBackendTest, NewEvictionRecoverWithEviction) {
  SetNewEviction();
  BackendRecoverWithEviction();
}

// Tests that the |BackendImpl| fails to start with the wrong cache version.
TEST_F(DiskCacheTest, WrongVersion) {
  ASSERT_TRUE(CopyTestCache("wrong_version"));
  net::TestCompletionCallback cb;

  std::unique_ptr<disk_cache::BackendImpl> cache(
      std::make_unique<disk_cache::BackendImpl>(cache_path_, nullptr, nullptr,
                                                net::DISK_CACHE, nullptr));
  cache->Init(cb.callback());
  ASSERT_THAT(cb.WaitForResult(), IsError(net::ERR_FAILED));
}

// Tests that the cache is properly restarted on recovery error.
// Disabled on android since this test requires cache creator to create
// blockfile caches.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DiskCacheBackendTest, DeleteOld) {
  ASSERT_TRUE(CopyTestCache("wrong_version"));
  SetNewEviction();

  TestBackendResultCompletionCallback cb;
  {
    base::ScopedDisallowBlocking disallow_blocking;
    base::FilePath path(cache_path_);
    disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
        net::DISK_CACHE, net::CACHE_BACKEND_BLOCKFILE,
        /*file_operations=*/nullptr, path, 0,
        disk_cache::ResetHandling::kResetOnError, /*net_log=*/nullptr,
        cb.callback());
    path.clear();  // Make sure path was captured by the previous call.
    rv = cb.GetResult(std::move(rv));
    ASSERT_THAT(rv.net_error, IsOk());
  }
  EXPECT_TRUE(CheckCacheIntegrity(cache_path_, new_eviction_, /*max_size = */ 0,
                                  mask_));
}
#endif

// We want to be able to deal with messed up entries on disk.
void DiskCacheBackendTest::BackendInvalidEntry2() {
  ASSERT_TRUE(CopyTestCache("bad_entry"));
  DisableFirstCleanup();
  InitCache();

  disk_cache::Entry *entry1, *entry2;
  ASSERT_THAT(OpenEntry("the first key", &entry1), IsOk());
  EXPECT_NE(net::OK, OpenEntry("some other key", &entry2));
  entry1->Close();

  // CheckCacheIntegrity will fail at this point.
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, InvalidEntry2) {
  BackendInvalidEntry2();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry2) {
  SetNewEviction();
  BackendInvalidEntry2();
}

// Tests that we don't crash or hang when enumerating this cache.
void DiskCacheBackendTest::BackendInvalidEntry3() {
  SetMask(0x1);        // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  disk_cache::Entry* entry;
  std::unique_ptr<TestIterator> iter = CreateIterator();
  while (iter->OpenNextEntry(&entry) == net::OK) {
    entry->Close();
  }
}

TEST_F(DiskCacheBackendTest, InvalidEntry3) {
  ASSERT_TRUE(CopyTestCache("dirty_entry3"));
  BackendInvalidEntry3();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry3) {
  ASSERT_TRUE(CopyTestCache("dirty_entry4"));
  SetNewEviction();
  BackendInvalidEntry3();
  DisableIntegrityCheck();
}

// Test that we handle a dirty entry on the LRU list, already replaced with
// the same key, and with hash collisions.
TEST_F(DiskCacheBackendTest, InvalidEntry4) {
  ASSERT_TRUE(CopyTestCache("dirty_entry3"));
  SetMask(0x1);        // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  TrimForTest(false);
}

// Test that we handle a dirty entry on the deleted list, already replaced with
// the same key, and with hash collisions.
TEST_F(DiskCacheBackendTest, InvalidEntry5) {
  ASSERT_TRUE(CopyTestCache("dirty_entry4"));
  SetNewEviction();
  SetMask(0x1);        // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  TrimDeletedListForTest(false);
}

TEST_F(DiskCacheBackendTest, InvalidEntry6) {
  ASSERT_TRUE(CopyTestCache("dirty_entry5"));
  SetMask(0x1);        // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  // There is a dirty entry (but marked as clean) at the end, pointing to a
  // deleted entry through the hash collision list. We should not re-insert the
  // deleted entry into the index table.

  TrimForTest(false);
  // The cache should be clean (as detected by CheckCacheIntegrity).
}

// Tests that we don't hang when there is a loop on the hash collision list.
// The test cache could be a result of bug 69135.
TEST_F(DiskCacheBackendTest, BadNextEntry1) {
  ASSERT_TRUE(CopyTestCache("list_loop2"));
  SetMask(0x1);        // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  // The second entry points at itselft, and the first entry is not accessible
  // though the index, but it is at the head of the LRU.

  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("The first key", &entry), IsOk());
  entry->Close();

  TrimForTest(false);
  TrimForTest(false);
  ASSERT_THAT(OpenEntry("The first key", &entry), IsOk());
  entry->Close();
  EXPECT_EQ(1, cache_->GetEntryCount());
}

// Tests that we don't hang when there is a loop on the hash collision list.
// The test cache could be a result of bug 69135.
TEST_F(DiskCacheBackendTest, BadNextEntry2) {
  ASSERT_TRUE(CopyTestCache("list_loop3"));
  SetMask(0x1);        // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  // There is a wide loop of 5 entries.

  disk_cache::Entry* entry;
  ASSERT_NE(net::OK, OpenEntry("Not present key", &entry));
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry6) {
  ASSERT_TRUE(CopyTestCache("bad_rankings3"));
  DisableFirstCleanup();
  SetNewEviction();
  InitCache();

  // The second entry is dirty, but removing it should not corrupt the list.
  disk_cache::Entry* entry;
  ASSERT_NE(net::OK, OpenEntry("the second key", &entry));
  ASSERT_THAT(OpenEntry("the first key", &entry), IsOk());

  // This should not delete the cache.
  entry->Doom();
  FlushQueueForTest();
  entry->Close();

  ASSERT_THAT(OpenEntry("some other key", &entry), IsOk());
  entry->Close();
}

// Tests handling of corrupt entries by keeping the rankings node around, with
// a fatal failure.
void DiskCacheBackendTest::BackendInvalidEntry7() {
  const int kSize = 0x3000;  // 12 kB.
  SetMaxSize(kSize * 10);
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(first, &entry), IsOk());
  entry->Close();
  ASSERT_THAT(CreateEntry(second, &entry), IsOk());

  // Corrupt this entry.
  disk_cache::EntryImpl* entry_impl =
      static_cast<disk_cache::EntryImpl*>(entry);

  entry_impl->rankings()->Data()->next = 0;
  entry_impl->rankings()->Store();
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(2, cache_->GetEntryCount());

  // This should detect the bad entry.
  EXPECT_NE(net::OK, OpenEntry(second, &entry));
  EXPECT_EQ(1, cache_->GetEntryCount());

  // We should delete the cache. The list still has a corrupt node.
  std::unique_ptr<TestIterator> iter = CreateIterator();
  EXPECT_NE(net::OK, iter->OpenNextEntry(&entry));
  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, InvalidEntry7) {
  BackendInvalidEntry7();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry7) {
  SetNewEviction();
  BackendInvalidEntry7();
}

// Tests handling of corrupt entries by keeping the rankings node around, with
// a non fatal failure.
void DiskCacheBackendTest::BackendInvalidEntry8() {
  const int kSize = 0x3000;  // 12 kB
  SetMaxSize(kSize * 10);
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(first, &entry), IsOk());
  entry->Close();
  ASSERT_THAT(CreateEntry(second, &entry), IsOk());

  // Corrupt this entry.
  disk_cache::EntryImpl* entry_impl =
      static_cast<disk_cache::EntryImpl*>(entry);

  entry_impl->rankings()->Data()->contents = 0;
  entry_impl->rankings()->Store();
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(2, cache_->GetEntryCount());

  // This should detect the bad entry.
  EXPECT_NE(net::OK, OpenEntry(second, &entry));
  EXPECT_EQ(1, cache_->GetEntryCount());

  // We should not delete the cache.
  std::unique_ptr<TestIterator> iter = CreateIterator();
  ASSERT_THAT(iter->OpenNextEntry(&entry), IsOk());
  entry->Close();
  EXPECT_NE(net::OK, iter->OpenNextEntry(&entry));
  EXPECT_EQ(1, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, InvalidEntry8) {
  BackendInvalidEntry8();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry8) {
  SetNewEviction();
  BackendInvalidEntry8();
}

// Tests handling of corrupt entries detected by enumerations. Note that these
// tests (xx9 to xx11) are basically just going though slightly different
// codepaths so they are tighlty coupled with the code, but that is better than
// not testing error handling code.
void DiskCacheBackendTest::BackendInvalidEntry9(bool eviction) {
  const int kSize = 0x3000;  // 12 kB.
  SetMaxSize(kSize * 10);
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(first, &entry), IsOk());
  entry->Close();
  ASSERT_THAT(CreateEntry(second, &entry), IsOk());

  // Corrupt this entry.
  disk_cache::EntryImpl* entry_impl =
      static_cast<disk_cache::EntryImpl*>(entry);

  entry_impl->entry()->Data()->state = 0xbad;
  entry_impl->entry()->Store();
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(2, cache_->GetEntryCount());

  if (eviction) {
    TrimForTest(false);
    EXPECT_EQ(1, cache_->GetEntryCount());
    TrimForTest(false);
    EXPECT_EQ(1, cache_->GetEntryCount());
  } else {
    // We should detect the problem through the list, but we should not delete
    // the entry, just fail the iteration.
    std::unique_ptr<TestIterator> iter = CreateIterator();
    EXPECT_NE(net::OK, iter->OpenNextEntry(&entry));

    // Now a full iteration will work, and return one entry.
    ASSERT_THAT(iter->OpenNextEntry(&entry), IsOk());
    entry->Close();
    EXPECT_NE(net::OK, iter->OpenNextEntry(&entry));

    // This should detect what's left of the bad entry.
    EXPECT_NE(net::OK, OpenEntry(second, &entry));
    EXPECT_EQ(2, cache_->GetEntryCount());
  }
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, InvalidEntry9) {
  BackendInvalidEntry9(false);
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry9) {
  SetNewEviction();
  BackendInvalidEntry9(false);
}

TEST_F(DiskCacheBackendTest, TrimInvalidEntry9) {
  BackendInvalidEntry9(true);
}

TEST_F(DiskCacheBackendTest, NewEvictionTrimInvalidEntry9) {
  SetNewEviction();
  BackendInvalidEntry9(true);
}

// Tests handling of corrupt entries detected by enumerations.
void DiskCacheBackendTest::BackendInvalidEntry10(bool eviction) {
  const int kSize = 0x3000;  // 12 kB.
  SetMaxSize(kSize * 10);
  SetNewEviction();
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(first, &entry), IsOk());
  entry->Close();
  ASSERT_THAT(OpenEntry(first, &entry), IsOk());
  EXPECT_EQ(0, WriteData(entry, 0, 200, nullptr, 0, false));
  entry->Close();
  ASSERT_THAT(CreateEntry(second, &entry), IsOk());

  // Corrupt this entry.
  disk_cache::EntryImpl* entry_impl =
      static_cast<disk_cache::EntryImpl*>(entry);

  entry_impl->entry()->Data()->state = 0xbad;
  entry_impl->entry()->Store();
  entry->Close();
  ASSERT_THAT(CreateEntry("third", &entry), IsOk());
  entry->Close();
  EXPECT_EQ(3, cache_->GetEntryCount());

  // We have:
  // List 0: third -> second (bad).
  // List 1: first.

  if (eviction) {
    // Detection order: second -> first -> third.
    TrimForTest(false);
    EXPECT_EQ(3, cache_->GetEntryCount());
    TrimForTest(false);
    EXPECT_EQ(2, cache_->GetEntryCount());
    TrimForTest(false);
    EXPECT_EQ(1, cache_->GetEntryCount());
  } else {
    // Detection order: third -> second -> first.
    // We should detect the problem through the list, but we should not delete
    // the entry.
    std::unique_ptr<TestIterator> iter = CreateIterator();
    ASSERT_THAT(iter->OpenNextEntry(&entry), IsOk());
    entry->Close();
    ASSERT_THAT(iter->OpenNextEntry(&entry), IsOk());
    EXPECT_EQ(first, entry->GetKey());
    entry->Close();
    EXPECT_NE(net::OK, iter->OpenNextEntry(&entry));
  }
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, InvalidEntry10) {
  BackendInvalidEntry10(false);
}

TEST_F(DiskCacheBackendTest, TrimInvalidEntry10) {
  BackendInvalidEntry10(true);
}

// Tests handling of corrupt entries detected by enumerations.
void DiskCacheBackendTest::BackendInvalidEntry11(bool eviction) {
  const int kSize = 0x3000;  // 12 kB.
  SetMaxSize(kSize * 10);
  SetNewEviction();
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(first, &entry), IsOk());
  entry->Close();
  ASSERT_THAT(OpenEntry(first, &entry), IsOk());
  EXPECT_EQ(0, WriteData(entry, 0, 200, nullptr, 0, false));
  entry->Close();
  ASSERT_THAT(CreateEntry(second, &entry), IsOk());
  entry->Close();
  ASSERT_THAT(OpenEntry(second, &entry), IsOk());
  EXPECT_EQ(0, WriteData(entry, 0, 200, nullptr, 0, false));

  // Corrupt this entry.
  disk_cache::EntryImpl* entry_impl =
      static_cast<disk_cache::EntryImpl*>(entry);

  entry_impl->entry()->Data()->state = 0xbad;
  entry_impl->entry()->Store();
  entry->Close();
  ASSERT_THAT(CreateEntry("third", &entry), IsOk());
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(3, cache_->GetEntryCount());

  // We have:
  // List 0: third.
  // List 1: second (bad) -> first.

  if (eviction) {
    // Detection order: third -> first -> second.
    TrimForTest(false);
    EXPECT_EQ(2, cache_->GetEntryCount());
    TrimForTest(false);
    EXPECT_EQ(1, cache_->GetEntryCount());
    TrimForTest(false);
    EXPECT_EQ(1, cache_->GetEntryCount());
  } else {
    // Detection order: third -> second.
    // We should detect the problem through the list, but we should not delete
    // the entry, just fail the iteration.
    std::unique_ptr<TestIterator> iter = CreateIterator();
    ASSERT_THAT(iter->OpenNextEntry(&entry), IsOk());
    entry->Close();
    EXPECT_NE(net::OK, iter->OpenNextEntry(&entry));

    // Now a full iteration will work, and return two entries.
    ASSERT_THAT(iter->OpenNextEntry(&entry), IsOk());
    entry->Close();
    ASSERT_THAT(iter->OpenNextEntry(&entry), IsOk());
    entry->Close();
    EXPECT_NE(net::OK, iter->OpenNextEntry(&entry));
  }
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, InvalidEntry11) {
  BackendInvalidEntry11(false);
}

TEST_F(DiskCacheBackendTest, TrimInvalidEntry11) {
  BackendInvalidEntry11(true);
}

// Tests handling of corrupt entries in the middle of a long eviction run.
void DiskCacheBackendTest::BackendTrimInvalidEntry12() {
  const int kSize = 0x3000;  // 12 kB
  SetMaxSize(kSize * 10);
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(first, &entry), IsOk());
  entry->Close();
  ASSERT_THAT(CreateEntry(second, &entry), IsOk());

  // Corrupt this entry.
  disk_cache::EntryImpl* entry_impl =
      static_cast<disk_cache::EntryImpl*>(entry);

  entry_impl->entry()->Data()->state = 0xbad;
  entry_impl->entry()->Store();
  entry->Close();
  ASSERT_THAT(CreateEntry("third", &entry), IsOk());
  entry->Close();
  ASSERT_THAT(CreateEntry("fourth", &entry), IsOk());
  TrimForTest(true);
  EXPECT_EQ(1, cache_->GetEntryCount());
  entry->Close();
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, TrimInvalidEntry12) {
  BackendTrimInvalidEntry12();
}

TEST_F(DiskCacheBackendTest, NewEvictionTrimInvalidEntry12) {
  SetNewEviction();
  BackendTrimInvalidEntry12();
}

// We want to be able to deal with messed up entries on disk.
void DiskCacheBackendTest::BackendInvalidRankings2() {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  InitCache();

  disk_cache::Entry *entry1, *entry2;
  EXPECT_NE(net::OK, OpenEntry("the first key", &entry1));
  ASSERT_THAT(OpenEntry("some other key", &entry2), IsOk());
  entry2->Close();

  // CheckCacheIntegrity will fail at this point.
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, InvalidRankings2) {
  BackendInvalidRankings2();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidRankings2) {
  SetNewEviction();
  BackendInvalidRankings2();
}

// If the LRU is corrupt, we delete the cache.
void DiskCacheBackendTest::BackendInvalidRankings() {
  disk_cache::Entry* entry;
  std::unique_ptr<TestIterator> iter = CreateIterator();
  ASSERT_THAT(iter->OpenNextEntry(&entry), IsOk());
  entry->Close();
  EXPECT_EQ(2, cache_->GetEntryCount());

  EXPECT_NE(net::OK, iter->OpenNextEntry(&entry));
  FlushQueueForTest();  // Allow the restart to finish.
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, InvalidRankingsSuccess) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  InitCache();
  BackendInvalidRankings();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidRankingsSuccess) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetNewEviction();
  InitCache();
  BackendInvalidRankings();
}

TEST_F(DiskCacheBackendTest, InvalidRankingsFailure) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendInvalidRankings();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidRankingsFailure) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetNewEviction();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendInvalidRankings();
}

// If the LRU is corrupt and we have open entries, we disable the cache.
void DiskCacheBackendTest::BackendDisable() {
  disk_cache::Entry *entry1, *entry2;
  std::unique_ptr<TestIterator> iter = CreateIterator();
  ASSERT_THAT(iter->OpenNextEntry(&entry1), IsOk());

  EXPECT_NE(net::OK, iter->OpenNextEntry(&entry2));
  EXPECT_EQ(0, cache_->GetEntryCount());
  EXPECT_NE(net::OK, CreateEntry("Something new", &entry2));

  entry1->Close();
  FlushQueueForTest();  // Flushing the Close posts a task to restart the cache.
  FlushQueueForTest();  // This one actually allows that task to complete.

  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DisableSuccess) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  InitCache();
  BackendDisable();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableSuccess) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetNewEviction();
  InitCache();
  BackendDisable();
}

TEST_F(DiskCacheBackendTest, DisableFailure) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableFailure) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetNewEviction();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable();
}

// This is another type of corruption on the LRU; disable the cache.
void DiskCacheBackendTest::BackendDisable2() {
  EXPECT_EQ(8, cache_->GetEntryCount());

  disk_cache::Entry* entry;
  std::unique_ptr<TestIterator> iter = CreateIterator();
  int count = 0;
  while (iter->OpenNextEntry(&entry) == net::OK) {
    ASSERT_TRUE(nullptr != entry);
    entry->Close();
    count++;
    ASSERT_LT(count, 9);
  };

  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DisableSuccess2) {
  ASSERT_TRUE(CopyTestCache("list_loop"));
  DisableFirstCleanup();
  InitCache();
  BackendDisable2();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableSuccess2) {
  ASSERT_TRUE(CopyTestCache("list_loop"));
  DisableFirstCleanup();
  SetNewEviction();
  InitCache();
  BackendDisable2();
}

TEST_F(DiskCacheBackendTest, DisableFailure2) {
  ASSERT_TRUE(CopyTestCache("list_loop"));
  DisableFirstCleanup();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable2();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableFailure2) {
  ASSERT_TRUE(CopyTestCache("list_loop"));
  DisableFirstCleanup();
  SetNewEviction();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable2();
}

// If the index size changes when we disable the cache, we should not crash.
void DiskCacheBackendTest::BackendDisable3() {
  disk_cache::Entry *entry1, *entry2;
  std::unique_ptr<TestIterator> iter = CreateIterator();
  EXPECT_EQ(2, cache_->GetEntryCount());
  ASSERT_THAT(iter->OpenNextEntry(&entry1), IsOk());
  entry1->Close();

  EXPECT_NE(net::OK, iter->OpenNextEntry(&entry2));
  FlushQueueForTest();

  ASSERT_THAT(CreateEntry("Something new", &entry2), IsOk());
  entry2->Close();

  EXPECT_EQ(1, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DisableSuccess3) {
  ASSERT_TRUE(CopyTestCache("bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  InitCache();
  BackendDisable3();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableSuccess3) {
  ASSERT_TRUE(CopyTestCache("bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  SetNewEviction();
  InitCache();
  BackendDisable3();
}

// If we disable the cache, already open entries should work as far as possible.
void DiskCacheBackendTest::BackendDisable4() {
  disk_cache::Entry *entry1, *entry2, *entry3, *entry4;
  std::unique_ptr<TestIterator> iter = CreateIterator();
  ASSERT_THAT(iter->OpenNextEntry(&entry1), IsOk());

  char key2[2000];
  char key3[20000];
  CacheTestFillBuffer(key2, sizeof(key2), true);
  CacheTestFillBuffer(key3, sizeof(key3), true);
  key2[sizeof(key2) - 1] = '\0';
  key3[sizeof(key3) - 1] = '\0';
  ASSERT_THAT(CreateEntry(key2, &entry2), IsOk());
  ASSERT_THAT(CreateEntry(key3, &entry3), IsOk());

  const int kBufSize = 20000;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  memset(buf->data(), 0, kBufSize);
  EXPECT_EQ(100, WriteData(entry2, 0, 0, buf.get(), 100, false));
  EXPECT_EQ(kBufSize, WriteData(entry3, 0, 0, buf.get(), kBufSize, false));

  // This line should disable the cache but not delete it.
  EXPECT_NE(net::OK, iter->OpenNextEntry(&entry4));
  EXPECT_EQ(0, cache_->GetEntryCount());

  EXPECT_NE(net::OK, CreateEntry("cache is disabled", &entry4));

  EXPECT_EQ(100, ReadData(entry2, 0, 0, buf.get(), 100));
  EXPECT_EQ(100, WriteData(entry2, 0, 0, buf.get(), 100, false));
  EXPECT_EQ(100, WriteData(entry2, 1, 0, buf.get(), 100, false));

  EXPECT_EQ(kBufSize, ReadData(entry3, 0, 0, buf.get(), kBufSize));
  EXPECT_EQ(kBufSize, WriteData(entry3, 0, 0, buf.get(), kBufSize, false));
  EXPECT_EQ(kBufSize, WriteData(entry3, 1, 0, buf.get(), kBufSize, false));

  std::string key = entry2->GetKey();
  EXPECT_EQ(sizeof(key2) - 1, key.size());
  key = entry3->GetKey();
  EXPECT_EQ(sizeof(key3) - 1, key.size());

  entry1->Close();
  entry2->Close();
  entry3->Close();
  FlushQueueForTest();  // Flushing the Close posts a task to restart the cache.
  FlushQueueForTest();  // This one actually allows that task to complete.

  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DisableSuccess4) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  InitCache();
  BackendDisable4();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableSuccess4) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetNewEviction();
  InitCache();
  BackendDisable4();
}

// Tests the exposed API with a disabled cache.
void DiskCacheBackendTest::BackendDisabledAPI() {
  cache_impl_->SetUnitTestMode();  // Simulate failure restarting the cache.

  disk_cache::Entry *entry1, *entry2;
  std::unique_ptr<TestIterator> iter = CreateIterator();
  EXPECT_EQ(2, cache_->GetEntryCount());
  ASSERT_THAT(iter->OpenNextEntry(&entry1), IsOk());
  entry1->Close();
  EXPECT_NE(net::OK, iter->OpenNextEntry(&entry2));
  FlushQueueForTest();
  // The cache should be disabled.

  EXPECT_EQ(net::DISK_CACHE, cache_->GetCacheType());
  EXPECT_EQ(0, cache_->GetEntryCount());
  EXPECT_NE(net::OK, OpenEntry("First", &entry2));
  EXPECT_NE(net::OK, CreateEntry("Something new", &entry2));
  EXPECT_NE(net::OK, DoomEntry("First"));
  EXPECT_NE(net::OK, DoomAllEntries());
  EXPECT_NE(net::OK, DoomEntriesBetween(Time(), Time::Now()));
  EXPECT_NE(net::OK, DoomEntriesSince(Time()));
  iter = CreateIterator();
  EXPECT_NE(net::OK, iter->OpenNextEntry(&entry2));

  base::StringPairs stats;
  cache_->GetStats(&stats);
  EXPECT_TRUE(stats.empty());
  OnExternalCacheHit("First");
}

TEST_F(DiskCacheBackendTest, DisabledAPI) {
  ASSERT_TRUE(CopyTestCache("bad_rankings2"));
  DisableFirstCleanup();
  InitCache();
  BackendDisabledAPI();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisabledAPI) {
  ASSERT_TRUE(CopyTestCache("bad_rankings2"));
  DisableFirstCleanup();
  SetNewEviction();
  InitCache();
  BackendDisabledAPI();
}

// Test that some eviction of some kind happens.
void DiskCacheBackendTest::BackendEviction() {
  const int kMaxSize = 200 * 1024;
  const int kMaxEntryCount = 20;
  const int kWriteSize = kMaxSize / kMaxEntryCount;

  const int kWriteEntryCount = kMaxEntryCount * 2;

  static_assert(kWriteEntryCount * kWriteSize > kMaxSize,
                "must write more than MaxSize");

  SetMaxSize(kMaxSize);
  InitSparseCache(nullptr, nullptr);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kWriteSize);
  CacheTestFillBuffer(buffer->data(), kWriteSize, false);

  std::string key_prefix("prefix");
  for (int i = 0; i < kWriteEntryCount; ++i) {
    AddDelay();
    disk_cache::Entry* entry = nullptr;
    ASSERT_THAT(CreateEntry(key_prefix + base::NumberToString(i), &entry),
                IsOk());
    disk_cache::ScopedEntryPtr entry_closer(entry);
    EXPECT_EQ(kWriteSize,
              WriteData(entry, 1, 0, buffer.get(), kWriteSize, false));
  }

  int size = CalculateSizeOfAllEntries();
  EXPECT_GT(kMaxSize, size);
}

TEST_F(DiskCacheBackendTest, BackendEviction) {
  BackendEviction();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyBackendEviction) {
  SetMemoryOnlyMode();
  BackendEviction();
}

// TODO(morlovich): Enable BackendEviction test for simple cache after
// performance problems are addressed. See crbug.com/588184 for more
// information.

// This overly specific looking test is a regression test aimed at
// crbug.com/589186.
TEST_F(DiskCacheBackendTest, MemoryOnlyUseAfterFree) {
  SetMemoryOnlyMode();

  const int kMaxSize = 200 * 1024;
  const int kMaxEntryCount = 20;
  const int kWriteSize = kMaxSize / kMaxEntryCount;

  SetMaxSize(kMaxSize);
  InitCache();

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kWriteSize);
  CacheTestFillBuffer(buffer->data(), kWriteSize, false);

  // Create an entry to be our sparse entry that gets written later.
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("first parent", &entry), IsOk());
  disk_cache::ScopedEntryPtr first_parent(entry);

  // Create a ton of entries, and keep them open, to put the cache well above
  // its eviction threshhold.
  const int kTooManyEntriesCount = kMaxEntryCount * 2;
  std::list<disk_cache::ScopedEntryPtr> open_entries;
  std::string key_prefix("prefix");
  for (int i = 0; i < kTooManyEntriesCount; ++i) {
    ASSERT_THAT(CreateEntry(key_prefix + base::NumberToString(i), &entry),
                IsOk());
    // Not checking the result because it will start to fail once the max size
    // is reached.
    WriteData(entry, 1, 0, buffer.get(), kWriteSize, false);
    open_entries.push_back(disk_cache::ScopedEntryPtr(entry));
  }

  // Writing this sparse data should not crash. Ignoring the result because
  // we're only concerned with not crashing in this particular test.
  first_parent->WriteSparseData(32768, buffer.get(), 1024,
                                net::CompletionOnceCallback());
}

TEST_F(DiskCacheBackendTest, MemoryCapsWritesToMaxSize) {
  // Verify that the memory backend won't grow beyond its max size if lots of
  // open entries (each smaller than the max entry size) are trying to write
  // beyond the max size.
  SetMemoryOnlyMode();

  const int kMaxSize = 100 * 1024;       // 100KB cache
  const int kNumEntries = 20;            // 20 entries to write
  const int kWriteSize = kMaxSize / 10;  // Each entry writes 1/10th the max

  SetMaxSize(kMaxSize);
  InitCache();

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kWriteSize);
  CacheTestFillBuffer(buffer->data(), kWriteSize, false);

  // Create an entry to be the final entry that gets written later.
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("final", &entry), IsOk());
  disk_cache::ScopedEntryPtr final_entry(entry);

  // Create a ton of entries, write to the cache, and keep the entries open.
  // They should start failing writes once the cache fills.
  std::list<disk_cache::ScopedEntryPtr> open_entries;
  std::string key_prefix("prefix");
  for (int i = 0; i < kNumEntries; ++i) {
    ASSERT_THAT(CreateEntry(key_prefix + base::NumberToString(i), &entry),
                IsOk());
    WriteData(entry, 1, 0, buffer.get(), kWriteSize, false);
    open_entries.push_back(disk_cache::ScopedEntryPtr(entry));
  }
  EXPECT_GE(kMaxSize, CalculateSizeOfAllEntries());

  // Any more writing at this point should cause an error.
  EXPECT_THAT(
      WriteData(final_entry.get(), 1, 0, buffer.get(), kWriteSize, false),
      IsError(net::ERR_INSUFFICIENT_RESOURCES));
}

TEST_F(DiskCacheTest, Backend_UsageStatsTimer) {
  MessageLoopHelper helper;

  ASSERT_TRUE(CleanupCacheDir());
  // Want to use our thread since we call SyncInit ourselves.
  std::unique_ptr<disk_cache::BackendImpl> cache(
      std::make_unique<disk_cache::BackendImpl>(
          cache_path_, nullptr,
          base::SingleThreadTaskRunner::GetCurrentDefault(), net::DISK_CACHE,
          nullptr));
  ASSERT_TRUE(nullptr != cache.get());
  cache->SetUnitTestMode();
  ASSERT_THAT(cache->SyncInit(), IsOk());

  // Wait for a callback that never comes... about 2 secs :). The message loop
  // has to run to allow invocation of the usage timer.
  helper.WaitUntilCacheIoFinished(1);
}

TEST_F(DiskCacheBackendTest, TimerNotCreated) {
  ASSERT_TRUE(CopyTestCache("wrong_version"));

  // Want to use our thread since we call SyncInit ourselves.
  std::unique_ptr<disk_cache::BackendImpl> cache(
      std::make_unique<disk_cache::BackendImpl>(
          cache_path_, nullptr,
          base::SingleThreadTaskRunner::GetCurrentDefault(), net::DISK_CACHE,
          nullptr));
  ASSERT_TRUE(nullptr != cache.get());
  cache->SetUnitTestMode();
  ASSERT_NE(net::OK, cache->SyncInit());

  ASSERT_TRUE(nullptr == cache->GetTimerForTest());

  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, Backend_UsageStats) {
  InitCache();
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("key", &entry), IsOk());
  entry->Close();
  FlushQueueForTest();

  disk_cache::StatsItems stats;
  cache_->GetStats(&stats);
  EXPECT_FALSE(stats.empty());

  disk_cache::StatsItems::value_type hits("Create hit", "0x1");
  EXPECT_EQ(1, base::ranges::count(stats, hits));

  ResetCaches();

  // Now open the cache and verify that the stats are still there.
  DisableFirstCleanup();
  InitCache();
  EXPECT_EQ(1, cache_->GetEntryCount());

  stats.clear();
  cache_->GetStats(&stats);
  EXPECT_FALSE(stats.empty());

  EXPECT_EQ(1, base::ranges::count(stats, hits));
}

void DiskCacheBackendTest::BackendDoomAll() {
  InitCache();

  disk_cache::Entry *entry1, *entry2;
  ASSERT_THAT(CreateEntry("first", &entry1), IsOk());
  ASSERT_THAT(CreateEntry("second", &entry2), IsOk());
  entry1->Close();
  entry2->Close();

  ASSERT_THAT(CreateEntry("third", &entry1), IsOk());
  ASSERT_THAT(CreateEntry("fourth", &entry2), IsOk());

  ASSERT_EQ(4, cache_->GetEntryCount());
  EXPECT_THAT(DoomAllEntries(), IsOk());
  ASSERT_EQ(0, cache_->GetEntryCount());

  // We should stop posting tasks at some point (if we post any).
  base::RunLoop().RunUntilIdle();

  disk_cache::Entry *entry3, *entry4;
  EXPECT_NE(net::OK, OpenEntry("third", &entry3));
  ASSERT_THAT(CreateEntry("third", &entry3), IsOk());
  ASSERT_THAT(CreateEntry("fourth", &entry4), IsOk());

  EXPECT_THAT(DoomAllEntries(), IsOk());
  ASSERT_EQ(0, cache_->GetEntryCount());

  entry1->Close();
  entry2->Close();
  entry3->Doom();  // The entry should be already doomed, but this must work.
  entry3->Close();
  entry4->Close();

  // Now try with all references released.
  ASSERT_THAT(CreateEntry("third", &entry1), IsOk());
  ASSERT_THAT(CreateEntry("fourth", &entry2), IsOk());
  entry1->Close();
  entry2->Close();

  ASSERT_EQ(2, cache_->GetEntryCount());
  EXPECT_THAT(DoomAllEntries(), IsOk());
  ASSERT_EQ(0, cache_->GetEntryCount());

  EXPECT_THAT(DoomAllEntries(), IsOk());
}

TEST_F(DiskCacheBackendTest, DoomAll) {
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomAll) {
  SetNewEviction();
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomAll) {
  SetMemoryOnlyMode();
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, AppCacheOnlyDoomAll) {
  SetCacheType(net::APP_CACHE);
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, ShaderCacheOnlyDoomAll) {
  SetCacheType(net::SHADER_CACHE);
  BackendDoomAll();
}

// If the index size changes when we doom the cache, we should not crash.
void DiskCacheBackendTest::BackendDoomAll2() {
  EXPECT_EQ(2, cache_->GetEntryCount());
  EXPECT_THAT(DoomAllEntries(), IsOk());

  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("Something new", &entry), IsOk());
  entry->Close();

  EXPECT_EQ(1, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DoomAll2) {
  ASSERT_TRUE(CopyTestCache("bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  InitCache();
  BackendDoomAll2();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomAll2) {
  ASSERT_TRUE(CopyTestCache("bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  SetNewEviction();
  InitCache();
  BackendDoomAll2();
}

// We should be able to create the same entry on multiple simultaneous instances
// of the cache.
TEST_F(DiskCacheTest, MultipleInstances) {
  base::ScopedTempDir store1, store2;
  ASSERT_TRUE(store1.CreateUniqueTempDir());
  ASSERT_TRUE(store2.CreateUniqueTempDir());

  TestBackendResultCompletionCallback cb;

  const int kNumberOfCaches = 2;
  std::unique_ptr<disk_cache::Backend> caches[kNumberOfCaches];

  disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
      net::DISK_CACHE, net::CACHE_BACKEND_DEFAULT, /*file_operations=*/nullptr,
      store1.GetPath(), 0, disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr, cb.callback());
  rv = cb.GetResult(std::move(rv));
  ASSERT_THAT(rv.net_error, IsOk());
  caches[0] = std::move(rv.backend);
  rv = disk_cache::CreateCacheBackend(
      net::GENERATED_BYTE_CODE_CACHE, net::CACHE_BACKEND_DEFAULT,
      /*file_operations=*/nullptr, store2.GetPath(), 0,
      disk_cache::ResetHandling::kNeverReset, /*net_log=*/nullptr,
      cb.callback());
  rv = cb.GetResult(std::move(rv));
  ASSERT_THAT(rv.net_error, IsOk());
  caches[1] = std::move(rv.backend);

  ASSERT_TRUE(caches[0].get() != nullptr && caches[1].get() != nullptr);

  std::string key("the first key");
  for (auto& cache : caches) {
    TestEntryResultCompletionCallback cb2;
    EntryResult result = cache->CreateEntry(key, net::HIGHEST, cb2.callback());
    result = cb2.GetResult(std::move(result));
    ASSERT_THAT(result.net_error(), IsOk());
    result.ReleaseEntry()->Close();
  }
}

// Test the six regions of the curve that determines the max cache size.
TEST_F(DiskCacheTest, AutomaticMaxSize) {
  using disk_cache::kDefaultCacheSize;
  int64_t large_size = kDefaultCacheSize;

  // Region 1: expected = available * 0.8
  EXPECT_EQ((kDefaultCacheSize - 1) * 8 / 10,
            disk_cache::PreferredCacheSize(large_size - 1));
  EXPECT_EQ(kDefaultCacheSize * 8 / 10,
            disk_cache::PreferredCacheSize(large_size));
  EXPECT_EQ(kDefaultCacheSize - 1,
            disk_cache::PreferredCacheSize(large_size * 10 / 8 - 1));

  // Region 2: expected = default_size
  EXPECT_EQ(kDefaultCacheSize,
            disk_cache::PreferredCacheSize(large_size * 10 / 8));
  EXPECT_EQ(kDefaultCacheSize,
            disk_cache::PreferredCacheSize(large_size * 10 - 1));

  // Region 3: expected = available * 0.1
  EXPECT_EQ(kDefaultCacheSize, disk_cache::PreferredCacheSize(large_size * 10));
  EXPECT_EQ((kDefaultCacheSize * 25 - 1) / 10,
            disk_cache::PreferredCacheSize(large_size * 25 - 1));

  // Region 4: expected = default_size * 2.5
  EXPECT_EQ(kDefaultCacheSize * 25 / 10,
            disk_cache::PreferredCacheSize(large_size * 25));
  EXPECT_EQ(kDefaultCacheSize * 25 / 10,
            disk_cache::PreferredCacheSize(large_size * 100 - 1));
  EXPECT_EQ(kDefaultCacheSize * 25 / 10,
            disk_cache::PreferredCacheSize(large_size * 100));
  EXPECT_EQ(kDefaultCacheSize * 25 / 10,
            disk_cache::PreferredCacheSize(large_size * 250 - 1));

  // Region 5: expected = available * 0.1
  int64_t largest_size = kDefaultCacheSize * 4;
  EXPECT_EQ(kDefaultCacheSize * 25 / 10,
            disk_cache::PreferredCacheSize(large_size * 250));
  EXPECT_EQ(largest_size - 1,
            disk_cache::PreferredCacheSize(largest_size * 100 - 1));

  // Region 6: expected = largest possible size
  EXPECT_EQ(largest_size, disk_cache::PreferredCacheSize(largest_size * 100));
  EXPECT_EQ(largest_size, disk_cache::PreferredCacheSize(largest_size * 10000));
}

// Make sure that we keep the total memory used by the internal buffers under
// control.
TEST_F(DiskCacheBackendTest, TotalBuffersSize1) {
  InitCache();
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 200;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, true);

  for (int i = 0; i < 10; i++) {
    SCOPED_TRACE(i);
    // Allocate 2MB for this entry.
    EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer.get(), kSize, true));
    EXPECT_EQ(kSize, WriteData(entry, 1, 0, buffer.get(), kSize, true));
    EXPECT_EQ(kSize,
              WriteData(entry, 0, 1024 * 1024, buffer.get(), kSize, false));
    EXPECT_EQ(kSize,
              WriteData(entry, 1, 1024 * 1024, buffer.get(), kSize, false));

    // Delete one of the buffers and truncate the other.
    EXPECT_EQ(0, WriteData(entry, 0, 0, buffer.get(), 0, true));
    EXPECT_EQ(0, WriteData(entry, 1, 10, buffer.get(), 0, true));

    // Delete the second buffer, writing 10 bytes to disk.
    entry->Close();
    ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  }

  entry->Close();
  EXPECT_EQ(0, cache_impl_->GetTotalBuffersSize());
}

// This test assumes at least 150MB of system memory.
TEST_F(DiskCacheBackendTest, TotalBuffersSize2) {
  InitCache();

  const int kOneMB = 1024 * 1024;
  EXPECT_TRUE(cache_impl_->IsAllocAllowed(0, kOneMB));
  EXPECT_EQ(kOneMB, cache_impl_->GetTotalBuffersSize());

  EXPECT_TRUE(cache_impl_->IsAllocAllowed(0, kOneMB));
  EXPECT_EQ(kOneMB * 2, cache_impl_->GetTotalBuffersSize());

  EXPECT_TRUE(cache_impl_->IsAllocAllowed(0, kOneMB));
  EXPECT_EQ(kOneMB * 3, cache_impl_->GetTotalBuffersSize());

  cache_impl_->BufferDeleted(kOneMB);
  EXPECT_EQ(kOneMB * 2, cache_impl_->GetTotalBuffersSize());

  // Check the upper limit.
  EXPECT_FALSE(cache_impl_->IsAllocAllowed(0, 30 * kOneMB));

  for (int i = 0; i < 30; i++)
    cache_impl_->IsAllocAllowed(0, kOneMB);  // Ignore the result.

  EXPECT_FALSE(cache_impl_->IsAllocAllowed(0, kOneMB));
}

// Tests that sharing of external files works and we are able to delete the
// files when we need to.
TEST_F(DiskCacheBackendTest, FileSharing) {
  InitCache();

  disk_cache::Addr address(0x80000001);
  ASSERT_TRUE(cache_impl_->CreateExternalFile(&address));
  base::FilePath name = cache_impl_->GetFileName(address);

  {
    auto file = base::MakeRefCounted<disk_cache::File>(false);
    file->Init(name);

#if BUILDFLAG(IS_WIN)
    DWORD sharing = FILE_SHARE_READ | FILE_SHARE_WRITE;
    DWORD access = GENERIC_READ | GENERIC_WRITE;
    base::win::ScopedHandle file2(CreateFile(name.value().c_str(), access,
                                             sharing, nullptr, OPEN_EXISTING, 0,
                                             nullptr));
    EXPECT_FALSE(file2.IsValid());

    sharing |= FILE_SHARE_DELETE;
    file2.Set(CreateFile(name.value().c_str(), access, sharing, nullptr,
                         OPEN_EXISTING, 0, nullptr));
    EXPECT_TRUE(file2.IsValid());
#endif

    EXPECT_TRUE(base::DeleteFile(name));

    // We should be able to use the file.
    const int kSize = 200;
    char buffer1[kSize];
    char buffer2[kSize];
    memset(buffer1, 't', kSize);
    memset(buffer2, 0, kSize);
    EXPECT_TRUE(file->Write(buffer1, kSize, 0));
    EXPECT_TRUE(file->Read(buffer2, kSize, 0));
    EXPECT_EQ(0, memcmp(buffer1, buffer2, kSize));
  }

  base::File file(name, base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_FALSE(file.IsValid());
  EXPECT_EQ(file.error_details(), base::File::FILE_ERROR_NOT_FOUND);
}

TEST_F(DiskCacheBackendTest, UpdateRankForExternalCacheHit) {
  InitCache();

  disk_cache::Entry* entry;

  for (int i = 0; i < 2; ++i) {
    std::string key = base::StringPrintf("key%d", i);
    ASSERT_THAT(CreateEntry(key, &entry), IsOk());
    entry->Close();
  }

  // Ping the oldest entry.
  OnExternalCacheHit("key0");

  TrimForTest(false);

  // Make sure the older key remains.
  EXPECT_EQ(1, cache_->GetEntryCount());
  ASSERT_THAT(OpenEntry("key0", &entry), IsOk());
  entry->Close();
}

TEST_F(DiskCacheBackendTest, ShaderCacheUpdateRankForExternalCacheHit) {
  SetCacheType(net::SHADER_CACHE);
  InitCache();

  disk_cache::Entry* entry;

  for (int i = 0; i < 2; ++i) {
    std::string key = base::StringPrintf("key%d", i);
    ASSERT_THAT(CreateEntry(key, &entry), IsOk());
    entry->Close();
  }

  // Ping the oldest entry.
  OnExternalCacheHit("key0");

  TrimForTest(false);

  // Make sure the older key remains.
  EXPECT_EQ(1, cache_->GetEntryCount());
  ASSERT_THAT(OpenEntry("key0", &entry), IsOk());
  entry->Close();
}

TEST_F(DiskCacheBackendTest, SimpleCacheShutdownWithPendingCreate) {
  // Use net::APP_CACHE to make size estimations deterministic via
  // non-optimistic writes.
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  BackendShutdownWithPendingCreate(false);
}

TEST_F(DiskCacheBackendTest, SimpleCacheShutdownWithPendingDoom) {
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  BackendShutdownWithPendingDoom();
}

TEST_F(DiskCacheBackendTest, SimpleCacheShutdownWithPendingFileIO) {
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  BackendShutdownWithPendingFileIO(false);
}

TEST_F(DiskCacheBackendTest, SimpleCacheBasics) {
  SetSimpleCacheMode();
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, SimpleCacheAppCacheBasics) {
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, SimpleCacheKeying) {
  SetSimpleCacheMode();
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, SimpleCacheAppCacheKeying) {
  SetSimpleCacheMode();
  SetCacheType(net::APP_CACHE);
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, SimpleCacheLoad) {
  SetMaxSize(0x100000);
  SetSimpleCacheMode();
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, SimpleCacheAppCacheLoad) {
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  SetMaxSize(0x100000);
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, SimpleDoomRecent) {
  SetSimpleCacheMode();
  BackendDoomRecent();
}

// crbug.com/330926, crbug.com/370677
TEST_F(DiskCacheBackendTest, DISABLED_SimpleDoomBetween) {
  SetSimpleCacheMode();
  BackendDoomBetween();
}

TEST_F(DiskCacheBackendTest, SimpleCacheDoomAll) {
  SetSimpleCacheMode();
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, SimpleCacheAppCacheOnlyDoomAll) {
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, SimpleCacheOpenMissingFile) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";
  disk_cache::Entry* entry = nullptr;

  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  ASSERT_TRUE(entry != nullptr);
  entry->Close();
  entry = nullptr;

  // To make sure the file creation completed we need to call open again so that
  // we block until it actually created the files.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  ASSERT_TRUE(entry != nullptr);
  entry->Close();
  entry = nullptr;

  // Delete one of the files in the entry.
  base::FilePath to_delete_file = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 0));
  EXPECT_TRUE(base::PathExists(to_delete_file));
  EXPECT_TRUE(base::DeleteFile(to_delete_file));

  // Failing to open the entry should delete the rest of these files.
  ASSERT_THAT(OpenEntry(key, &entry), IsError(net::ERR_FAILED));

  // Confirm the rest of the files are gone.
  for (int i = 1; i < disk_cache::kSimpleEntryNormalFileCount; ++i) {
    base::FilePath should_be_gone_file(cache_path_.AppendASCII(
        disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, i)));
    EXPECT_FALSE(base::PathExists(should_be_gone_file));
  }
}

TEST_F(DiskCacheBackendTest, SimpleCacheOpenBadFile) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";
  disk_cache::Entry* entry = nullptr;

  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  disk_cache::Entry* null = nullptr;
  ASSERT_NE(null, entry);
  entry->Close();
  entry = nullptr;

  // To make sure the file creation completed we need to call open again so that
  // we block until it actually created the files.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  ASSERT_NE(null, entry);
  entry->Close();
  entry = nullptr;

  // The entry is being closed on the Simple Cache worker pool
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  // Write an invalid header for stream 0 and stream 1.
  base::FilePath entry_file1_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 0));

  disk_cache::SimpleFileHeader header;
  header.initial_magic_number = UINT64_C(0xbadf00d);
  EXPECT_TRUE(
      base::WriteFile(entry_file1_path, base::byte_span_from_ref(header)));
  ASSERT_THAT(OpenEntry(key, &entry), IsError(net::ERR_FAILED));
}

// Tests that the Simple Cache Backend fails to initialize with non-matching
// file structure on disk.
TEST_F(DiskCacheBackendTest, SimpleCacheOverBlockfileCache) {
  // Create a cache structure with the |BackendImpl|.
  InitCache();
  disk_cache::Entry* entry;
  const int kSize = 50;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);
  ASSERT_THAT(CreateEntry("key", &entry), IsOk());
  ASSERT_EQ(0, WriteData(entry, 0, 0, buffer.get(), 0, false));
  entry->Close();
  ResetCaches();

  // Check that the |SimpleBackendImpl| does not favor this structure.
  auto simple_cache = std::make_unique<disk_cache::SimpleBackendImpl>(
      /*file_operations_factory=*/nullptr, cache_path_, nullptr, nullptr, 0,
      net::DISK_CACHE, nullptr);
  net::TestCompletionCallback cb;
  simple_cache->Init(cb.callback());
  EXPECT_NE(net::OK, cb.WaitForResult());
  simple_cache.reset();
  DisableIntegrityCheck();
}

// Tests that the |BackendImpl| refuses to initialize on top of the files
// generated by the Simple Cache Backend.
TEST_F(DiskCacheBackendTest, BlockfileCacheOverSimpleCache) {
  // Create a cache structure with the |SimpleBackendImpl|.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* entry;
  const int kSize = 50;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);
  ASSERT_THAT(CreateEntry("key", &entry), IsOk());
  ASSERT_EQ(0, WriteData(entry, 0, 0, buffer.get(), 0, false));
  entry->Close();
  ResetCaches();

  // Check that the |BackendImpl| does not favor this structure.
  auto cache = std::make_unique<disk_cache::BackendImpl>(
      cache_path_, nullptr, nullptr, net::DISK_CACHE, nullptr);
  cache->SetUnitTestMode();
  net::TestCompletionCallback cb;
  cache->Init(cb.callback());
  EXPECT_NE(net::OK, cb.WaitForResult());
  cache.reset();
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, SimpleCacheFixEnumerators) {
  SetSimpleCacheMode();
  BackendFixEnumerators();
}

// Tests basic functionality of the SimpleBackend implementation of the
// enumeration API.
TEST_F(DiskCacheBackendTest, SimpleCacheEnumerationBasics) {
  SetSimpleCacheMode();
  InitCache();
  std::set<std::string> key_pool;
  ASSERT_TRUE(CreateSetOfRandomEntries(&key_pool));

  // Check that enumeration returns all entries.
  std::set<std::string> keys_to_match(key_pool);
  std::unique_ptr<TestIterator> iter = CreateIterator();
  size_t count = 0;
  ASSERT_TRUE(EnumerateAndMatchKeys(-1, iter.get(), &keys_to_match, &count));
  iter.reset();
  EXPECT_EQ(key_pool.size(), count);
  EXPECT_TRUE(keys_to_match.empty());

  // Check that opening entries does not affect enumeration.
  keys_to_match = key_pool;
  iter = CreateIterator();
  count = 0;
  disk_cache::Entry* entry_opened_before;
  ASSERT_THAT(OpenEntry(*(key_pool.begin()), &entry_opened_before), IsOk());
  ASSERT_TRUE(EnumerateAndMatchKeys(key_pool.size() / 2, iter.get(),
                                    &keys_to_match, &count));

  disk_cache::Entry* entry_opened_middle;
  ASSERT_EQ(net::OK, OpenEntry(*(keys_to_match.begin()), &entry_opened_middle));
  ASSERT_TRUE(EnumerateAndMatchKeys(-1, iter.get(), &keys_to_match, &count));
  iter.reset();
  entry_opened_before->Close();
  entry_opened_middle->Close();

  EXPECT_EQ(key_pool.size(), count);
  EXPECT_TRUE(keys_to_match.empty());
}

// Tests that the enumerations are not affected by dooming an entry in the
// middle.
TEST_F(DiskCacheBackendTest, SimpleCacheEnumerationWhileDoomed) {
  SetSimpleCacheMode();
  InitCache();
  std::set<std::string> key_pool;
  ASSERT_TRUE(CreateSetOfRandomEntries(&key_pool));

  // Check that enumeration returns all entries but the doomed one.
  std::set<std::string> keys_to_match(key_pool);
  std::unique_ptr<TestIterator> iter = CreateIterator();
  size_t count = 0;
  ASSERT_TRUE(EnumerateAndMatchKeys(key_pool.size() / 2, iter.get(),
                                    &keys_to_match, &count));

  std::string key_to_delete = *(keys_to_match.begin());
  DoomEntry(key_to_delete);
  keys_to_match.erase(key_to_delete);
  key_pool.erase(key_to_delete);
  ASSERT_TRUE(EnumerateAndMatchKeys(-1, iter.get(), &keys_to_match, &count));
  iter.reset();

  EXPECT_EQ(key_pool.size(), count);
  EXPECT_TRUE(keys_to_match.empty());
}

// Tests that enumerations are not affected by corrupt files.
TEST_F(DiskCacheBackendTest, SimpleCacheEnumerationCorruption) {
  SetSimpleCacheMode();
  InitCache();
  // Create a corrupt entry.
  const std::string key = "the key";
  disk_cache::Entry* corrupted_entry;

  ASSERT_THAT(CreateEntry(key, &corrupted_entry), IsOk());
  ASSERT_TRUE(corrupted_entry);
  const int kSize = 50;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);
  ASSERT_EQ(kSize,
            WriteData(corrupted_entry, 0, 0, buffer.get(), kSize, false));
  ASSERT_EQ(kSize, ReadData(corrupted_entry, 0, 0, buffer.get(), kSize));
  corrupted_entry->Close();
  // Let all I/O finish so it doesn't race with corrupting the file below.
  RunUntilIdle();

  std::set<std::string> key_pool;
  ASSERT_TRUE(CreateSetOfRandomEntries(&key_pool));

  EXPECT_TRUE(
      disk_cache::simple_util::CreateCorruptFileForTests(key, cache_path_));
  EXPECT_EQ(key_pool.size() + 1, static_cast<size_t>(cache_->GetEntryCount()));

  // Check that enumeration returns all entries but the corrupt one.
  std::set<std::string> keys_to_match(key_pool);
  std::unique_ptr<TestIterator> iter = CreateIterator();
  size_t count = 0;
  ASSERT_TRUE(EnumerateAndMatchKeys(-1, iter.get(), &keys_to_match, &count));
  iter.reset();

  EXPECT_EQ(key_pool.size(), count);
  EXPECT_TRUE(keys_to_match.empty());
}

// Tests that enumerations don't leak memory when the backend is destructed
// mid-enumeration.
TEST_F(DiskCacheBackendTest, SimpleCacheEnumerationDestruction) {
  SetSimpleCacheMode();
  InitCache();
  std::set<std::string> key_pool;
  ASSERT_TRUE(CreateSetOfRandomEntries(&key_pool));

  std::unique_ptr<TestIterator> iter = CreateIterator();
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(iter->OpenNextEntry(&entry), IsOk());
  EXPECT_TRUE(entry);
  disk_cache::ScopedEntryPtr entry_closer(entry);

  ResetCaches();
  // This test passes if we don't leak memory.
}

// Verify that tasks run in priority order when the experiment is enabled.
// Test has races, disabling until fixed: https://crbug.com/853283
TEST_F(DiskCacheBackendTest, DISABLED_SimpleCachePrioritizedEntryOrder) {
  base::test::ScopedFeatureList scoped_feature_list;
  SetSimpleCacheMode();
  InitCache();

  // Set the SimpleCache's worker pool to a sequenced type for testing
  // priority order.
  disk_cache::SimpleBackendImpl* simple_cache =
      static_cast<disk_cache::SimpleBackendImpl*>(cache_.get());
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE, base::MayBlock()});
  simple_cache->SetTaskRunnerForTesting(task_runner);

  // Create three entries. Priority order is 3, 1, 2 because 3 has the highest
  // request priority and 1 is created before 2.
  disk_cache::Entry* entry1 = nullptr;
  disk_cache::Entry* entry2 = nullptr;
  disk_cache::Entry* entry3 = nullptr;
  ASSERT_THAT(CreateEntryWithPriority("first", net::LOWEST, &entry1), IsOk());
  ASSERT_THAT(CreateEntryWithPriority("second", net::LOWEST, &entry2), IsOk());
  ASSERT_THAT(CreateEntryWithPriority("third", net::HIGHEST, &entry3), IsOk());

  // Write some data to the entries.
  const int kSize = 10;
  auto buf1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buf2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buf3 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf1->data(), kSize, false);
  CacheTestFillBuffer(buf2->data(), kSize, false);
  CacheTestFillBuffer(buf3->data(), kSize, false);

  // Write to stream 2 because it's the only stream that can't be read from
  // synchronously.
  EXPECT_EQ(kSize, WriteData(entry1, 2, 0, buf1.get(), kSize, true));
  EXPECT_EQ(kSize, WriteData(entry2, 2, 0, buf1.get(), kSize, true));
  EXPECT_EQ(kSize, WriteData(entry3, 2, 0, buf1.get(), kSize, true));

  // Wait until the task_runner's queue is empty (WriteData might have
  // optimistically returned synchronously but still had some tasks to run in
  // the worker pool.
  base::RunLoop run_loop;
  task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                run_loop.QuitClosure());
  run_loop.Run();

  std::vector<int> finished_read_order;
  auto finished_callback = [](std::vector<int>* finished_read_order,
                              int entry_number, base::OnceClosure quit_closure,
                              int rv) {
    finished_read_order->push_back(entry_number);
    if (quit_closure)
      std::move(quit_closure).Run();
  };

  auto read_buf1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto read_buf2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto read_buf3 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);

  // Read from the entries in order 2, 3, 1. They should be reprioritized to
  // 3, 1, 2.
  base::RunLoop read_run_loop;

  entry2->ReadData(2, 0, read_buf2.get(), kSize,
                   base::BindOnce(finished_callback, &finished_read_order, 2,
                                  read_run_loop.QuitClosure()));
  entry3->ReadData(2, 0, read_buf3.get(), kSize,
                   base::BindOnce(finished_callback, &finished_read_order, 3,
                                  base::OnceClosure()));
  entry1->ReadData(2, 0, read_buf1.get(), kSize,
                   base::BindOnce(finished_callback, &finished_read_order, 1,
                                  base::OnceClosure()));
  EXPECT_EQ(0u, finished_read_order.size());

  read_run_loop.Run();
  EXPECT_EQ((std::vector<int>{3, 1, 2}), finished_read_order);
  entry1->Close();
  entry2->Close();
  entry3->Close();
}

// Tests that enumerations include entries with long keys.
TEST_F(DiskCacheBackendTest, SimpleCacheEnumerationLongKeys) {
  SetSimpleCacheMode();
  InitCache();
  std::set<std::string> key_pool;
  ASSERT_TRUE(CreateSetOfRandomEntries(&key_pool));

  const size_t long_key_length =
      disk_cache::SimpleSynchronousEntry::kInitialHeaderRead + 10;
  std::string long_key(long_key_length, 'X');
  key_pool.insert(long_key);
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(long_key.c_str(), &entry), IsOk());
  entry->Close();

  std::unique_ptr<TestIterator> iter = CreateIterator();
  size_t count = 0;
  EXPECT_TRUE(EnumerateAndMatchKeys(-1, iter.get(), &key_pool, &count));
  EXPECT_TRUE(key_pool.empty());
}

// Tests that a SimpleCache doesn't crash when files are deleted very quickly
// after closing.
// NOTE: IF THIS TEST IS FLAKY THEN IT IS FAILING. See https://crbug.com/416940
TEST_F(DiskCacheBackendTest, SimpleCacheDeleteQuickly) {
  SetSimpleCacheMode();
  for (int i = 0; i < 100; ++i) {
    InitCache();
    ResetCaches();
    EXPECT_TRUE(CleanupCacheDir());
  }
}

TEST_F(DiskCacheBackendTest, SimpleCacheLateDoom) {
  SetSimpleCacheMode();
  InitCache();

  disk_cache::Entry *entry1, *entry2;
  ASSERT_THAT(CreateEntry("first", &entry1), IsOk());
  ASSERT_THAT(CreateEntry("second", &entry2), IsOk());
  entry1->Close();

  // Ensure that the directory mtime is flushed to disk before serializing the
  // index.
  disk_cache::FlushCacheThreadForTesting();
#if BUILDFLAG(IS_POSIX)
  base::File cache_dir(cache_path_,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_TRUE(cache_dir.Flush());
#endif  // BUILDFLAG(IS_POSIX)
  ResetCaches();
  disk_cache::FlushCacheThreadForTesting();

  // The index is now written. Dooming the last entry can't delete a file,
  // because that would advance the cache directory mtime and invalidate the
  // index.
  entry2->Doom();
  entry2->Close();

  DisableFirstCleanup();
  InitCache();
  EXPECT_EQ(disk_cache::SimpleIndex::INITIALIZE_METHOD_LOADED,
            simple_cache_impl_->index()->init_method());
}

TEST_F(DiskCacheBackendTest, SimpleCacheNegMaxSize) {
  SetMaxSize(-1);
  SetSimpleCacheMode();
  InitCache();
  // We don't know what it will pick, but it's limited to what
  // disk_cache::PreferredCacheSize would return, scaled by the size experiment,
  // which only goes as much as 4x. It definitely should not be MAX_UINT64.
  EXPECT_NE(simple_cache_impl_->index()->max_size(),
            std::numeric_limits<uint64_t>::max());

  int max_default_size =
      2 * disk_cache::PreferredCacheSize(std::numeric_limits<int32_t>::max());

  ASSERT_GE(max_default_size, 0);
  EXPECT_LT(simple_cache_impl_->index()->max_size(),
            static_cast<unsigned>(max_default_size));

  uint64_t max_size_without_scaling = simple_cache_impl_->index()->max_size();

  // Scale to 200%. The size should be twice of |max_size_without_scaling| but
  // since that's capped on 20% of available size, checking for the size to be
  // between max_size_without_scaling and max_size_without_scaling*2.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> field_trial_params;
    field_trial_params["percent_relative_size"] = "200";
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        disk_cache::kChangeDiskCacheSizeExperiment, field_trial_params);

    InitCache();

    uint64_t max_size_scaled = simple_cache_impl_->index()->max_size();

    EXPECT_GE(max_size_scaled, max_size_without_scaling);
    EXPECT_LE(max_size_scaled, 2 * max_size_without_scaling);
  }
}

TEST_F(DiskCacheBackendTest, SimpleLastModified) {
  // Simple cache used to incorrectly set LastModified on entries based on
  // timestamp of the cache directory, and not the entries' file
  // (https://crbug.com/714143). So this test arranges for a situation
  // where this would occur by doing:
  // 1) Write entry 1
  // 2) Delay
  // 3) Write entry 2. This sets directory time stamp to be different from
  //    timestamp of entry 1 (due to the delay)
  // It then checks whether the entry 1 got the proper timestamp or not.

  SetSimpleCacheMode();
  InitCache();
  std::string key1 = GenerateKey(true);
  std::string key2 = GenerateKey(true);

  disk_cache::Entry* entry1;
  ASSERT_THAT(CreateEntry(key1, &entry1), IsOk());

  // Make the Create complete --- SimpleCache can handle it optimistically,
  // and if we let it go fully async then trying to flush the Close might just
  // flush the Create.
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  entry1->Close();

  // Make the ::Close actually complete, since it is asynchronous.
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  Time entry1_timestamp = Time::NowFromSystemTime();

  // Don't want AddDelay since it sleep 1s(!) for SimpleCache, and we don't
  // care about reduced precision in index here.
  while (base::Time::NowFromSystemTime() <=
         (entry1_timestamp + base::Milliseconds(10))) {
    base::PlatformThread::Sleep(base::Milliseconds(1));
  }

  disk_cache::Entry* entry2;
  ASSERT_THAT(CreateEntry(key2, &entry2), IsOk());
  entry2->Close();
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  disk_cache::Entry* reopen_entry1;
  ASSERT_THAT(OpenEntry(key1, &reopen_entry1), IsOk());

  // This shouldn't pick up entry2's write time incorrectly.
  EXPECT_LE(reopen_entry1->GetLastModified(), entry1_timestamp);
  reopen_entry1->Close();
}

TEST_F(DiskCacheBackendTest, SimpleFdLimit) {
  base::HistogramTester histogram_tester;
  SetSimpleCacheMode();
  // Make things blocking so CreateEntry actually waits for file to be
  // created.
  SetCacheType(net::APP_CACHE);
  InitCache();

  disk_cache::Entry* entries[kLargeNumEntries];
  std::string keys[kLargeNumEntries];
  for (int i = 0; i < kLargeNumEntries; ++i) {
    keys[i] = GenerateKey(true);
    ASSERT_THAT(CreateEntry(keys[i], &entries[i]), IsOk());
  }

  // Note the fixture sets the file limit to 64.
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_CLOSE_FILE,
                                     kLargeNumEntries - 64);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_REOPEN_FILE, 0);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_FAIL_REOPEN_FILE, 0);

  const int kSize = 25000;
  auto buf1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf1->data(), kSize, false);

  auto buf2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf2->data(), kSize, false);

  // Doom an entry and create a new one with same name, to test that both
  // re-open properly.
  EXPECT_EQ(net::OK, DoomEntry(keys[0]));
  disk_cache::Entry* alt_entry;
  ASSERT_THAT(CreateEntry(keys[0], &alt_entry), IsOk());

  // One more file closure here to accommodate for alt_entry.
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_CLOSE_FILE,
                                     kLargeNumEntries - 64 + 1);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_REOPEN_FILE, 0);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_FAIL_REOPEN_FILE, 0);

  // Do some writes in [1...kLargeNumEntries) range, both testing bring those in
  // and kicking out [0] and [alt_entry]. These have to be to stream != 0 to
  // actually need files.
  for (int i = 1; i < kLargeNumEntries; ++i) {
    EXPECT_EQ(kSize, WriteData(entries[i], 1, 0, buf1.get(), kSize, true));
    auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
    ASSERT_EQ(kSize, ReadData(entries[i], 1, 0, read_buf.get(), kSize));
    EXPECT_EQ(0, memcmp(read_buf->data(), buf1->data(), kSize));
  }

  histogram_tester.ExpectBucketCount(
      "SimpleCache.FileDescriptorLimiterAction",
      disk_cache::FD_LIMIT_CLOSE_FILE,
      kLargeNumEntries - 64 + 1 + kLargeNumEntries - 1);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_REOPEN_FILE,
                                     kLargeNumEntries - 1);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_FAIL_REOPEN_FILE, 0);
  EXPECT_EQ(kSize, WriteData(entries[0], 1, 0, buf1.get(), kSize, true));
  EXPECT_EQ(kSize, WriteData(alt_entry, 1, 0, buf2.get(), kSize, true));

  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  ASSERT_EQ(kSize, ReadData(entries[0], 1, 0, read_buf.get(), kSize));
  EXPECT_EQ(0, memcmp(read_buf->data(), buf1->data(), kSize));

  auto read_buf2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  ASSERT_EQ(kSize, ReadData(alt_entry, 1, 0, read_buf2.get(), kSize));
  EXPECT_EQ(0, memcmp(read_buf2->data(), buf2->data(), kSize));

  // Two more things than last time --- entries[0] and |alt_entry|
  histogram_tester.ExpectBucketCount(
      "SimpleCache.FileDescriptorLimiterAction",
      disk_cache::FD_LIMIT_CLOSE_FILE,
      kLargeNumEntries - 64 + 1 + kLargeNumEntries - 1 + 2);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_REOPEN_FILE,
                                     kLargeNumEntries + 1);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_FAIL_REOPEN_FILE, 0);

  for (auto* entry : entries) {
    entry->Close();
    RunUntilIdle();
  }
  alt_entry->Close();
  RunUntilIdle();

  // Closes have to pull things in to write out the footer, but they also
  // free up FDs.
  histogram_tester.ExpectBucketCount(
      "SimpleCache.FileDescriptorLimiterAction",
      disk_cache::FD_LIMIT_CLOSE_FILE,
      kLargeNumEntries - 64 + 1 + kLargeNumEntries - 1 + 2);
  histogram_tester.ExpectBucketCount(
      "SimpleCache.FileDescriptorLimiterAction",
      disk_cache::FD_LIMIT_REOPEN_FILE,
      kLargeNumEntries - 64 + 1 + kLargeNumEntries - 1 + 2);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_FAIL_REOPEN_FILE, 0);
}

TEST_F(DiskCacheBackendTest, SparseEvict) {
  const int kMaxSize = 512;

  SetMaxSize(kMaxSize);
  InitCache();

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(64);
  CacheTestFillBuffer(buffer->data(), 64, false);

  disk_cache::Entry* entry0 = nullptr;
  ASSERT_THAT(CreateEntry("http://www.0.com/", &entry0), IsOk());

  disk_cache::Entry* entry1 = nullptr;
  ASSERT_THAT(CreateEntry("http://www.1.com/", &entry1), IsOk());

  disk_cache::Entry* entry2 = nullptr;
  // This strange looking domain name affects cache trim order
  // due to hashing
  ASSERT_THAT(CreateEntry("http://www.15360.com/", &entry2), IsOk());

  // Write sparse data to put us over the eviction threshold
  ASSERT_EQ(64, WriteSparseData(entry0, 0, buffer.get(), 64));
  ASSERT_EQ(1, WriteSparseData(entry0, 67108923, buffer.get(), 1));
  ASSERT_EQ(1, WriteSparseData(entry1, 53, buffer.get(), 1));
  ASSERT_EQ(1, WriteSparseData(entry2, 0, buffer.get(), 1));

  // Closing these in a special order should not lead to buggy reentrant
  // eviction.
  entry1->Close();
  entry2->Close();
  entry0->Close();
}

TEST_F(DiskCacheBackendTest, InMemorySparseDoom) {
  const int kMaxSize = 512;

  SetMaxSize(kMaxSize);
  SetMemoryOnlyMode();
  InitCache();

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(64);
  CacheTestFillBuffer(buffer->data(), 64, false);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry("http://www.0.com/", &entry), IsOk());

  ASSERT_EQ(net::ERR_FAILED, WriteSparseData(entry, 4337, buffer.get(), 64));
  entry->Close();

  // Dooming all entries at this point should properly iterate over
  // the parent and its children
  DoomAllEntries();
}

TEST_F(DiskCacheBackendTest, BlockFileMaxSizeLimit) {
  InitCache();

  int64_t size = std::numeric_limits<int32_t>::max();
  SetMaxSize(size, true /* should_succeed */);

  size += 1;
  SetMaxSize(size, false /* should_succeed */);
}

TEST_F(DiskCacheBackendTest, InMemoryMaxSizeLimit) {
  SetMemoryOnlyMode();
  InitCache();

  int64_t size = std::numeric_limits<int32_t>::max();
  SetMaxSize(size, true /* should_succeed */);

  size += 1;
  SetMaxSize(size, false /* should_succeed */);
}

TEST_F(DiskCacheBackendTest, SimpleMaxSizeLimit) {
  SetSimpleCacheMode();
  InitCache();

  int64_t size = std::numeric_limits<int32_t>::max();
  SetMaxSize(size, true /* should_succeed */);

  size += 1;
  SetMaxSize(size, true /* should_succeed */);
}

void DiskCacheBackendTest::BackendOpenOrCreateEntry() {
  // Avoid the weird kNoRandom flag on blockfile, since this needs to
  // test cleanup behavior actually used in production.
  if (memory_only_) {
    InitCache();
  } else {
    CleanupCacheDir();
    // Since we're not forcing a clean shutdown, integrity check may fail.
    DisableIntegrityCheck();
    CreateBackend(disk_cache::kNone);
  }

  // Test that new key is created.
  disk_cache::EntryResult es1 = OpenOrCreateEntry("first");
  ASSERT_THAT(es1.net_error(), IsOk());
  ASSERT_FALSE(es1.opened());
  disk_cache::Entry* e1 = es1.ReleaseEntry();
  ASSERT_TRUE(nullptr != e1);

  // Test that existing key is opened and its entry matches.
  disk_cache::EntryResult es2 = OpenOrCreateEntry("first");
  ASSERT_THAT(es2.net_error(), IsOk());
  ASSERT_TRUE(es2.opened());
  disk_cache::Entry* e2 = es2.ReleaseEntry();
  ASSERT_TRUE(nullptr != e2);
  ASSERT_EQ(e1, e2);

  // Test that different keys' entries are not the same.
  disk_cache::EntryResult es3 = OpenOrCreateEntry("second");
  ASSERT_THAT(es3.net_error(), IsOk());
  ASSERT_FALSE(es3.opened());
  disk_cache::Entry* e3 = es3.ReleaseEntry();
  ASSERT_TRUE(nullptr != e3);
  ASSERT_NE(e3, e1);

  // Test that a new entry can be created with the same key as a doomed entry.
  e3->Doom();
  disk_cache::EntryResult es4 = OpenOrCreateEntry("second");
  ASSERT_THAT(es4.net_error(), IsOk());
  ASSERT_FALSE(es4.opened());
  disk_cache::Entry* e4 = es4.ReleaseEntry();
  ASSERT_TRUE(nullptr != e4);
  ASSERT_NE(e4, e3);

  // Verify the expected number of entries
  ASSERT_EQ(2, cache_->GetEntryCount());

  e1->Close();
  e2->Close();
  e3->Close();
  e4->Close();

  // Test proper cancellation of callback. In-memory cache
  // is always synchronous, so this isn't' meaningful for it.
  if (!memory_only_) {
    TestEntryResultCompletionCallback callback;

    // Using "first" here:
    // 1) It's an existing entry, so SimpleCache can't cheat with an optimistic
    //    create.
    // 2) "second"'s creation is a cheated post-doom create one, which also
    //    makes testing trickier.
    EntryResult result =
        cache_->OpenOrCreateEntry("first", net::HIGHEST, callback.callback());
    ASSERT_EQ(net::ERR_IO_PENDING, result.net_error());
    ResetCaches();

    // Callback is supposed to be cancelled, so have to flush everything
    // to check for any trouble.
    disk_cache::FlushCacheThreadForTesting();
    RunUntilIdle();
    EXPECT_FALSE(callback.have_result());
  }
}

TEST_F(DiskCacheBackendTest, InMemoryOnlyOpenOrCreateEntry) {
  SetMemoryOnlyMode();
  BackendOpenOrCreateEntry();
}

TEST_F(DiskCacheBackendTest, MAYBE_BlockFileOpenOrCreateEntry) {
  BackendOpenOrCreateEntry();
}

TEST_F(DiskCacheBackendTest, MAYBE_SimpleOpenOrCreateEntry) {
  SetSimpleCacheMode();
  BackendOpenOrCreateEntry();
}

void DiskCacheBackendTest::BackendDeadOpenNextEntry() {
  InitCache();
  std::unique_ptr<disk_cache::Backend::Iterator> iter =
      cache_->CreateIterator();
  ResetCaches();
  EntryResult result = iter->OpenNextEntry(base::DoNothing());
  ASSERT_EQ(net::ERR_FAILED, result.net_error());
}

TEST_F(DiskCacheBackendTest, BlockFileBackendDeadOpenNextEntry) {
  BackendDeadOpenNextEntry();
}

TEST_F(DiskCacheBackendTest, SimpleBackendDeadOpenNextEntry) {
  SetSimpleCacheMode();
  BackendDeadOpenNextEntry();
}

TEST_F(DiskCacheBackendTest, InMemorySimpleBackendDeadOpenNextEntry) {
  SetMemoryOnlyMode();
  BackendDeadOpenNextEntry();
}

void DiskCacheBackendTest::BackendIteratorConcurrentDoom() {
  disk_cache::Entry* entry1 = nullptr;
  disk_cache::Entry* entry2 = nullptr;
  EXPECT_EQ(net::OK, CreateEntry("Key0", &entry1));
  EXPECT_EQ(net::OK, CreateEntry("Key1", &entry2));

  std::unique_ptr<disk_cache::Backend::Iterator> iter =
      cache_->CreateIterator();

  disk_cache::Entry* entry3 = nullptr;
  EXPECT_EQ(net::OK, OpenEntry("Key0", &entry3));

  TestEntryResultCompletionCallback cb;
  EntryResult result_iter = iter->OpenNextEntry(cb.callback());
  result_iter = cb.GetResult(std::move(result_iter));
  EXPECT_EQ(net::OK, result_iter.net_error());

  net::TestCompletionCallback cb_doom;
  int rv_doom = cache_->DoomAllEntries(cb_doom.callback());
  EXPECT_EQ(net::OK, cb_doom.GetResult(rv_doom));

  TestEntryResultCompletionCallback cb2;
  EntryResult result_iter2 = iter->OpenNextEntry(cb2.callback());
  result_iter2 = cb2.GetResult(std::move(result_iter2));

  EXPECT_TRUE(result_iter2.net_error() == net::ERR_FAILED ||
              result_iter2.net_error() == net::OK);

  entry1->Close();
  entry2->Close();
  entry3->Close();
}

TEST_F(DiskCacheBackendTest, BlockFileIteratorConcurrentDoom) {
  // Init in normal mode, bug not reproducible with kNoRandom. Still need to
  // let the test fixture know the new eviction algorithm will be on.
  CleanupCacheDir();
  SetNewEviction();
  CreateBackend(disk_cache::kNone);
  BackendIteratorConcurrentDoom();
}

TEST_F(DiskCacheBackendTest, SimpleIteratorConcurrentDoom) {
  SetSimpleCacheMode();
  InitCache();
  BackendIteratorConcurrentDoom();
}

TEST_F(DiskCacheBackendTest, InMemoryConcurrentDoom) {
  SetMemoryOnlyMode();
  InitCache();
  BackendIteratorConcurrentDoom();
}

TEST_F(DiskCacheBackendTest, EmptyCorruptSimpleCacheRecovery) {
  SetSimpleCacheMode();

  const std::string kCorruptData("corrupted");

  // Create a corrupt fake index in an otherwise empty simple cache.
  ASSERT_TRUE(base::PathExists(cache_path_));
  const base::FilePath index = cache_path_.AppendASCII("index");
  ASSERT_TRUE(base::WriteFile(index, kCorruptData));

  TestBackendResultCompletionCallback cb;

  // Simple cache should be able to recover.
  disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
      net::APP_CACHE, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr,
      cache_path_, 0, disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr, cb.callback());
  rv = cb.GetResult(std::move(rv));
  EXPECT_THAT(rv.net_error, IsOk());
}

TEST_F(DiskCacheBackendTest, MAYBE_NonEmptyCorruptSimpleCacheDoesNotRecover) {
  SetSimpleCacheMode();
  BackendOpenOrCreateEntry();

  const std::string kCorruptData("corrupted");

  // Corrupt the fake index file for the populated simple cache.
  ASSERT_TRUE(base::PathExists(cache_path_));
  const base::FilePath index = cache_path_.AppendASCII("index");
  ASSERT_TRUE(base::WriteFile(index, kCorruptData));

  TestBackendResultCompletionCallback cb;

  // Simple cache should not be able to recover when there are entry files.
  disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
      net::APP_CACHE, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr,
      cache_path_, 0, disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr, cb.callback());
  rv = cb.GetResult(std::move(rv));
  EXPECT_THAT(rv.net_error, IsError(net::ERR_FAILED));
}

TEST_F(DiskCacheBackendTest, SimpleOwnershipTransferBackendDestroyRace) {
  struct CleanupContext {
    explicit CleanupContext(bool* ran_ptr) : ran_ptr(ran_ptr) {}
    ~CleanupContext() {
      *ran_ptr = true;
    }

    raw_ptr<bool> ran_ptr;
  };

  const char kKey[] = "skeleton";

  // This test was for a fix for see https://crbug.com/946349, but the mechanics
  // of that failure became impossible after a follow up API refactor. Still,
  // the timing is strange, and warrant coverage; in particular this tests what
  // happen if the SimpleBackendImpl is destroyed after SimpleEntryImpl
  // decides to return an entry to the caller, but before the callback is run.
  SetSimpleCacheMode();
  InitCache();

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());
  // Make sure create actually succeeds, not just optimistically.
  RunUntilIdle();

  bool cleanup_context_ran = false;
  auto cleanup_context = std::make_unique<CleanupContext>(&cleanup_context_ran);

  // The OpenEntry code below will find a pre-existing entry in a READY state,
  // so it will immediately post a task to return a result. Destroying the
  // backend before running the event loop again will run that callback in the
  // dead-backend state, while OpenEntry completion was still with it alive.

  EntryResult result = cache_->OpenEntry(
      kKey, net::HIGHEST,
      base::BindOnce(
          [](std::unique_ptr<CleanupContext>, EntryResult result) {
            // The callback is here for ownership of CleanupContext,
            // and it shouldn't get invoked in this test. Normal
            // one would transfer result.entry to CleanupContext.
            ADD_FAILURE() << "This should not actually run";

            // ... but if it ran, it also shouldn't see the pointer.
            EXPECT_EQ(nullptr, result.ReleaseEntry());
          },
          std::move(cleanup_context)));
  EXPECT_EQ(net::ERR_IO_PENDING, result.net_error());
  ResetCaches();

  // Give CleanupContext a chance to do its thing.
  RunUntilIdle();
  EXPECT_TRUE(cleanup_context_ran);

  entry->Close();
}

// Verify that reloading the cache will preserve indices in kNeverReset mode.
TEST_F(DiskCacheBackendTest, SimpleCacheSoftResetKeepsValues) {
  SetSimpleCacheMode();
  SetCacheType(net::APP_CACHE);
  DisableFirstCleanup();
  CleanupCacheDir();

  {  // Do the initial cache creation then delete the values.
    TestBackendResultCompletionCallback cb;

    // Create an initial back-end and wait for indexing
    disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
        net::APP_CACHE, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr,
        cache_path_, 0, disk_cache::ResetHandling::kNeverReset,
        /*net_log=*/nullptr, cb.callback());
    rv = cb.GetResult(std::move(rv));
    EXPECT_THAT(rv.net_error, IsOk());
    std::unique_ptr<disk_cache::Backend> cache = std::move(rv.backend);
    ASSERT_TRUE(cache.get());
    WaitForSimpleCacheIndexAndCheck(cache.get());

    // Create an entry in the cache
    CreateKeyAndCheck(cache.get(), "key");
  }

  RunUntilIdle();

  {  // Do the second cache creation with no reset flag, preserving entries.
    TestBackendResultCompletionCallback cb;

    disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
        net::APP_CACHE, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr,
        cache_path_, 0, disk_cache::ResetHandling::kNeverReset,
        /*net_log=*/nullptr, cb.callback());
    rv = cb.GetResult(std::move(rv));
    EXPECT_THAT(rv.net_error, IsOk());
    std::unique_ptr<disk_cache::Backend> cache = std::move(rv.backend);
    ASSERT_TRUE(cache.get());
    WaitForSimpleCacheIndexAndCheck(cache.get());

    // The entry should be present, as a forced reset was not called for.
    EXPECT_TRUE(static_cast<disk_cache::SimpleBackendImpl*>(cache.get())
                    ->index()
                    ->Has(disk_cache::simple_util::GetEntryHashKey("key")));
  }
}

// Verify that reloading the cache will not preserve indices in Reset mode.
TEST_F(DiskCacheBackendTest, SimpleCacheHardResetDropsValues) {
  SetSimpleCacheMode();
  SetCacheType(net::APP_CACHE);
  DisableFirstCleanup();
  CleanupCacheDir();

  {  // Create the initial back-end.
    TestBackendResultCompletionCallback cb;

    disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
        net::APP_CACHE, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr,
        cache_path_, 0, disk_cache::ResetHandling::kNeverReset,
        /*net_log=*/nullptr, cb.callback());
    rv = cb.GetResult(std::move(rv));
    EXPECT_THAT(rv.net_error, IsOk());
    std::unique_ptr<disk_cache::Backend> cache = std::move(rv.backend);
    ASSERT_TRUE(cache.get());
    WaitForSimpleCacheIndexAndCheck(cache.get());

    // Create an entry in the cache.
    CreateKeyAndCheck(cache.get(), "key");
  }

  RunUntilIdle();

  {  // Re-load cache with a reset flag, which should ignore existing entries.
    TestBackendResultCompletionCallback cb;

    disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
        net::APP_CACHE, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr,
        cache_path_, 0, disk_cache::ResetHandling::kReset, /*net_log=*/nullptr,
        cb.callback());
    rv = cb.GetResult(std::move(rv));
    EXPECT_THAT(rv.net_error, IsOk());
    std::unique_ptr<disk_cache::Backend> cache = std::move(rv.backend);
    ASSERT_TRUE(cache.get());
    WaitForSimpleCacheIndexAndCheck(cache.get());

    // The entry shouldn't be present, as a forced reset was called for.
    EXPECT_FALSE(static_cast<disk_cache::SimpleBackendImpl*>(cache.get())
                     ->index()
                     ->Has(disk_cache::simple_util::GetEntryHashKey("key")));

    // Add the entry back in the cache, then make sure it's present.
    CreateKeyAndCheck(cache.get(), "key");

    EXPECT_TRUE(static_cast<disk_cache::SimpleBackendImpl*>(cache.get())
                    ->index()
                    ->Has(disk_cache::simple_util::GetEntryHashKey("key")));
  }
}

// Test to make sure cancelation of backend operation that got queued after
// a pending doom on backend destruction happens properly.
TEST_F(DiskCacheBackendTest, SimpleCancelOpPendingDoom) {
  struct CleanupContext {
    explicit CleanupContext(bool* ran_ptr) : ran_ptr(ran_ptr) {}
    ~CleanupContext() { *ran_ptr = true; }

    raw_ptr<bool> ran_ptr;
  };

  const char kKey[] = "skeleton";

  // Disable optimistic ops.
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  InitCache();

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());
  entry->Close();

  // Queue doom.
  cache_->DoomEntry(kKey, net::LOWEST, base::DoNothing());

  // Queue create after it.
  bool cleanup_context_ran = false;
  auto cleanup_context = std::make_unique<CleanupContext>(&cleanup_context_ran);

  EntryResult entry_result = cache_->CreateEntry(
      kKey, net::HIGHEST,
      base::BindOnce(
          [](std::unique_ptr<CleanupContext>, EntryResult result) {
            ADD_FAILURE() << "This should not actually run";
          },
          std::move(cleanup_context)));

  EXPECT_EQ(net::ERR_IO_PENDING, entry_result.net_error());
  ResetCaches();

  RunUntilIdle();
  EXPECT_TRUE(cleanup_context_ran);
}

TEST_F(DiskCacheBackendTest, SimpleDontLeakPostDoomCreate) {
  // If an entry has been optimistically created after a pending doom, and the
  // backend destroyed before the doom completed, the entry would get wedged,
  // with no operations on it workable and entry leaked.
  // (See https://crbug.com/1015774).
  const char kKey[] = "for_lock";
  const int kBufSize = 2 * 1024;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  CacheTestFillBuffer(buffer->data(), kBufSize, true);

  SetSimpleCacheMode();
  InitCache();

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());
  entry->Close();

  // Make sure create actually succeeds, not just optimistically.
  RunUntilIdle();

  // Queue doom.
  int rv = cache_->DoomEntry(kKey, net::LOWEST, base::DoNothing());
  ASSERT_EQ(net::ERR_IO_PENDING, rv);

  // And then do a create. This actually succeeds optimistically.
  EntryResult result =
      cache_->CreateEntry(kKey, net::LOWEST, base::DoNothing());
  ASSERT_EQ(net::OK, result.net_error());
  entry = result.ReleaseEntry();

  ResetCaches();

  // Entry is still supposed to be operable. This part is needed to see the bug
  // without a leak checker.
  EXPECT_EQ(kBufSize, WriteData(entry, 1, 0, buffer.get(), kBufSize, false));

  entry->Close();

  // Should not have leaked files here.
}

TEST_F(DiskCacheBackendTest, BlockFileDelayedWriteFailureRecovery) {
  // Test that blockfile recovers appropriately when some entries are
  // in a screwed up state due to an error in delayed writeback.
  //
  // https://crbug.com/1086727
  InitCache();

  const char kKey[] = "Key2";
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());

  const int kBufSize = 24320;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  CacheTestFillBuffer(buffer->data(), kBufSize, true);

  ASSERT_EQ(kBufSize, WriteSparseData(entry, 0, buffer.get(), kBufSize));

  // Setting the size limit artificially low injects a failure on writing back
  // data buffered above.
  SetMaxSize(4096);

  // This causes SparseControl to close the child entry corresponding to
  // low portion of offset space, triggering the writeback --- which fails
  // due to the space cap, and in particular fails to allocate data for
  // a stream, so it gets address 0.
  ASSERT_EQ(net::ERR_FAILED, WriteSparseData(entry, 16773118, buffer.get(), 4));

  // Now try reading the broken child. This should report an error, not
  // DCHECK.
  ASSERT_EQ(net::ERR_FAILED, ReadSparseData(entry, 4, buffer.get(), 4));

  entry->Close();
}

TEST_F(DiskCacheBackendTest, BlockFileInsertAliasing) {
  // Test for not having rankings corruption due to aliasing between iterator
  // and other ranking list copies during insertion operations.
  //
  // https://crbug.com/1156288

  // Need to disable weird extra sync behavior to hit the bug.
  CreateBackend(disk_cache::kNone);
  SetNewEviction();  // default, but integrity check doesn't realize that.

  const char kKey[] = "Key0";
  const char kKeyA[] = "KeyAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA41";
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());

  const int kBufSize = 61188;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  CacheTestFillBuffer(buffer->data(), kBufSize, true);

  net::TestCompletionCallback cb_write64;
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->WriteSparseData(8, buffer.get(), 64, cb_write64.callback()));

  net::TestCompletionCallback cb_write61k;
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->WriteSparseData(16773118, buffer.get(), 61188,
                                   cb_write61k.callback()));

  EXPECT_EQ(64, cb_write64.WaitForResult());
  EXPECT_EQ(61188, cb_write61k.WaitForResult());

  EXPECT_EQ(4128, WriteSparseData(entry, 2147479550, buffer.get(), 4128));

  std::unique_ptr<TestIterator> iter = CreateIterator();
  EXPECT_EQ(4128, WriteSparseData(entry, 2147479550, buffer.get(), 4128));
  EXPECT_EQ(64, WriteSparseData(entry, 8, buffer.get(), 64));

  disk_cache::Entry* itEntry1 = nullptr;
  ASSERT_EQ(net::OK, iter->OpenNextEntry(&itEntry1));
  // These are actually child nodes for range.

  entry->Close();

  disk_cache::Entry* itEntry2 = nullptr;
  ASSERT_EQ(net::OK, iter->OpenNextEntry(&itEntry2));

  net::TestCompletionCallback doom_cb;
  EXPECT_EQ(net::ERR_IO_PENDING, cache_->DoomAllEntries(doom_cb.callback()));

  TestEntryResultCompletionCallback cb_create1;
  disk_cache::EntryResult result =
      cache_->CreateEntry(kKey, net::HIGHEST, cb_create1.callback());
  EXPECT_EQ(net::OK, doom_cb.WaitForResult());
  result = cb_create1.WaitForResult();
  EXPECT_EQ(net::OK, result.net_error());
  entry = result.ReleaseEntry();

  disk_cache::Entry* entryA = nullptr;
  ASSERT_THAT(CreateEntry(kKeyA, &entryA), IsOk());
  entryA->Close();

  disk_cache::Entry* itEntry3 = nullptr;
  EXPECT_EQ(net::OK, iter->OpenNextEntry(&itEntry3));

  EXPECT_EQ(net::OK, DoomEntry(kKeyA));
  itEntry1->Close();
  entry->Close();
  itEntry2->Close();
  if (itEntry3)
    itEntry3->Close();
}

TEST_F(DiskCacheBackendTest, MemCacheBackwardsClock) {
  // Test to make sure that wall clock going backwards is tolerated.

  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());

  SetMemoryOnlyMode();
  InitCache();
  mem_cache_->SetClockForTesting(&clock);

  const int kBufSize = 4 * 1024;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  CacheTestFillBuffer(buffer->data(), kBufSize, true);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry("key1", &entry), IsOk());
  EXPECT_EQ(kBufSize, WriteData(entry, 0, 0, buffer.get(), kBufSize, false));
  entry->Close();

  clock.Advance(-base::Hours(1));

  ASSERT_THAT(CreateEntry("key2", &entry), IsOk());
  EXPECT_EQ(kBufSize, WriteData(entry, 0, 0, buffer.get(), kBufSize, false));
  entry->Close();

  EXPECT_LE(2 * kBufSize,
            CalculateSizeOfEntriesBetween(base::Time(), base::Time::Max()));
  EXPECT_EQ(net::OK, DoomEntriesBetween(base::Time(), base::Time::Max()));
  EXPECT_EQ(0, CalculateSizeOfEntriesBetween(base::Time(), base::Time::Max()));
  EXPECT_EQ(0, CalculateSizeOfAllEntries());

  mem_cache_->SetClockForTesting(nullptr);
}

TEST_F(DiskCacheBackendTest, SimpleOpenOrCreateIndexError) {
  // Exercise behavior of OpenOrCreateEntry in SimpleCache where the index
  // incorrectly claims the entry is missing. Regression test for
  // https://crbug.com/1316034
  const char kKey[] = "http://example.org";

  const int kBufSize = 256;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  CacheTestFillBuffer(buffer->data(), kBufSize, /*no_nulls=*/false);

  SetSimpleCacheMode();
  InitCache();

  // Create an entry.
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());

  EXPECT_EQ(kBufSize, WriteData(entry, /*index=*/1, /*offset=*/0, buffer.get(),
                                /*len=*/kBufSize, /*truncate=*/false));
  entry->Close();

  // Mess up the index to say it's not there.
  simple_cache_impl_->index()->Remove(
      disk_cache::simple_util::GetEntryHashKey(kKey));

  // Reopening with OpenOrCreateEntry should still work.
  disk_cache::EntryResult result = OpenOrCreateEntry(kKey);
  ASSERT_THAT(result.net_error(), IsOk());
  ASSERT_TRUE(result.opened());
  entry = result.ReleaseEntry();
  EXPECT_EQ(kBufSize, entry->GetDataSize(/*index=*/1));
  entry->Close();
}

TEST_F(DiskCacheBackendTest, SimpleOpenOrCreateIndexErrorOptimistic) {
  // Exercise behavior of OpenOrCreateEntry in SimpleCache where the index
  // incorrectly claims the entry is missing and we do an optimistic create.
  // Covers a codepath adjacent to the one that caused https://crbug.com/1316034
  const char kKey[] = "http://example.org";

  SetSimpleCacheMode();
  InitCache();

  const int kBufSize = 256;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  CacheTestFillBuffer(buffer->data(), kBufSize, /*no_nulls=*/false);

  // Create an entry.
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());
  EXPECT_EQ(kBufSize, WriteData(entry, /*index=*/1, /*offset=*/0, buffer.get(),
                                /*len=*/kBufSize, /*truncate=*/false));
  entry->Close();

  // Let all the I/O finish, so that OpenOrCreateEntry can try optimistic path.
  RunUntilIdle();

  // Mess up the index to say it's not there.
  simple_cache_impl_->index()->Remove(
      disk_cache::simple_util::GetEntryHashKey(kKey));

  // Reopening with OpenOrCreateEntry should still work, but since the backend
  // chose to be optimistic based on index, the result should be a fresh empty
  // entry.
  disk_cache::EntryResult result = OpenOrCreateEntry(kKey);
  ASSERT_THAT(result.net_error(), IsOk());
  ASSERT_FALSE(result.opened());
  entry = result.ReleaseEntry();
  EXPECT_EQ(0, entry->GetDataSize(/*index=*/1));
  entry->Close();
}

TEST_F(DiskCacheBackendTest, SimpleDoomAfterBackendDestruction) {
  // Test for when validating file headers/footers during close on simple
  // backend fails. To get the header to be checked on close, there needs to be
  // a stream 2, since 0/1 are validated on open, and no other operation must
  // have happened to stream 2, since those will force it, too. A way of getting
  // the validation to fail is to perform a doom on the file after the backend
  // is destroyed, since that will truncated the files to mark them invalid. See
  // https://crbug.com/1317884
  const char kKey[] = "Key0";

  const int kBufSize = 256;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  CacheTestFillBuffer(buffer->data(), kBufSize, /*no_nulls=*/false);

  SetCacheType(net::SHADER_CACHE);
  SetSimpleCacheMode();

  InitCache();
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());

  EXPECT_EQ(0, WriteData(entry, /*index=*/2, /*offset=*/1, buffer.get(),
                         /*len=*/0, /*truncate=*/false));
  entry->Close();

  ASSERT_THAT(OpenEntry(kKey, &entry), IsOk());
  ResetCaches();

  entry->Doom();
  entry->Close();
}

void DiskCacheBackendTest::BackendValidateMigrated() {
  // Blockfile 3.0 migration test.
  DisableFirstCleanup();  // started from copied dir, not cleaned dir.
  InitCache();

  // The total size comes straight from the headers, and is expected to be 1258
  // for either set of testdata.
  EXPECT_EQ(1258, CalculateSizeOfAllEntries());
  EXPECT_EQ(1, cache_->GetEntryCount());

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(OpenEntry("https://example.org/data", &entry), IsOk());

  // Size of the actual payload.
  EXPECT_EQ(1234, entry->GetDataSize(1));

  entry->Close();
}

TEST_F(DiskCacheBackendTest, BlockfileMigrate20) {
  ASSERT_TRUE(CopyTestCache("good_2_0"));
  BackendValidateMigrated();
}

TEST_F(DiskCacheBackendTest, BlockfileMigrate21) {
  ASSERT_TRUE(CopyTestCache("good_2_1"));
  BackendValidateMigrated();
}

TEST_F(DiskCacheBackendTest, BlockfileMigrateNewEviction20) {
  ASSERT_TRUE(CopyTestCache("good_2_0"));
  SetNewEviction();
  BackendValidateMigrated();
}

TEST_F(DiskCacheBackendTest, BlockfileMigrateNewEviction21) {
  ASSERT_TRUE(CopyTestCache("good_2_1"));
  SetNewEviction();
  BackendValidateMigrated();
}

// Disabled on android since this test requires cache creator to create
// blockfile caches, and we don't use them on Android anyway.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DiskCacheBackendTest, BlockfileEmptyIndex) {
  // Regression case for https://crbug.com/1441330 --- blockfile DCHECKing
  // on mmap error for files it uses.

  // Create a cache.
  TestBackendResultCompletionCallback cb;
  disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
      net::DISK_CACHE, net::CACHE_BACKEND_BLOCKFILE,
      /*file_operations=*/nullptr, cache_path_, 0,
      disk_cache::ResetHandling::kNeverReset, nullptr, cb.callback());
  rv = cb.GetResult(std::move(rv));
  ASSERT_THAT(rv.net_error, IsOk());
  ASSERT_TRUE(rv.backend);
  rv.backend.reset();

  // Make sure it's done doing I/O stuff.
  disk_cache::BackendImpl::FlushForTesting();

  // Truncate the index to zero bytes.
  base::File index(cache_path_.AppendASCII("index"),
                   base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  ASSERT_TRUE(index.IsValid());
  ASSERT_TRUE(index.SetLength(0));
  index.Close();

  // Open the backend again. Fails w/o error-recovery.
  rv = disk_cache::CreateCacheBackend(
      net::DISK_CACHE, net::CACHE_BACKEND_BLOCKFILE,
      /*file_operations=*/nullptr, cache_path_, 0,
      disk_cache::ResetHandling::kNeverReset, nullptr, cb.callback());
  rv = cb.GetResult(std::move(rv));
  EXPECT_EQ(rv.net_error, net::ERR_FAILED);
  EXPECT_FALSE(rv.backend);

  // Now try again with the "delete and start over on error" flag people
  // normally use.
  rv = disk_cache::CreateCacheBackend(
      net::DISK_CACHE, net::CACHE_BACKEND_BLOCKFILE,
      /*file_operations=*/nullptr, cache_path_, 0,
      disk_cache::ResetHandling::kResetOnError, nullptr, cb.callback());
  rv = cb.GetResult(std::move(rv));
  ASSERT_THAT(rv.net_error, IsOk());
  ASSERT_TRUE(rv.backend);
}
#endif

// See https://crbug.com/1486958
TEST_F(DiskCacheBackendTest, SimpleDoomIter) {
  const int kEntries = 1000;

  SetSimpleCacheMode();
  // Note: this test relies on InitCache() making sure the index is ready.
  InitCache();

  // We create a whole bunch of entries so that deleting them will hopefully
  // finish after the iteration, in order to reproduce timing for the bug.
  for (int i = 0; i < kEntries; ++i) {
    disk_cache::Entry* entry = nullptr;
    ASSERT_THAT(CreateEntry(base::NumberToString(i), &entry), IsOk());
    entry->Close();
  }
  RunUntilIdle();  // Make sure close completes.

  auto iterator = cache_->CreateIterator();
  base::RunLoop run_loop;

  disk_cache::EntryResult result = iterator->OpenNextEntry(
      base::BindLambdaForTesting([&](disk_cache::EntryResult result) {
        ASSERT_EQ(result.net_error(), net::OK);
        disk_cache::Entry* entry = result.ReleaseEntry();
        entry->Doom();
        entry->Close();
        run_loop.Quit();
      }));
  ASSERT_EQ(result.net_error(), net::ERR_IO_PENDING);
  cache_->DoomAllEntries(base::DoNothing());
  run_loop.Run();
}

// See https://crbug.com/1486958 for non-corrupting version,
// https://crbug.com/1510452 for corrupting one.
TEST_F(DiskCacheBackendTest, SimpleOpenIter) {
  constexpr int kEntries = 50;

  SetSimpleCacheMode();

  for (bool do_corrupt : {false, true}) {
    SCOPED_TRACE(do_corrupt);

    // Note: this test relies on InitCache() making sure the index is ready.
    InitCache();

    // We create a whole bunch of entries so that deleting them will hopefully
    // finish after the iteration, in order to reproduce timing for the bug.
    for (int i = 0; i < kEntries; ++i) {
      disk_cache::Entry* entry = nullptr;
      ASSERT_THAT(CreateEntry(base::NumberToString(i), &entry), IsOk());
      entry->Close();
    }
    RunUntilIdle();  // Make sure close completes.
    EXPECT_EQ(kEntries, cache_->GetEntryCount());

    // Iterate once to get the order.
    std::list<std::string> keys;
    auto iterator = cache_->CreateIterator();
    base::RunLoop run_loop;
    base::RepeatingCallback<void(EntryResult)> collect_entry_key =
        base::BindLambdaForTesting([&](disk_cache::EntryResult result) {
          if (result.net_error() == net::ERR_FAILED) {
            run_loop.Quit();
            return;  // iteration complete.
          }
          ASSERT_EQ(result.net_error(), net::OK);
          disk_cache::Entry* entry = result.ReleaseEntry();
          keys.push_back(entry->GetKey());
          entry->Close();
          result = iterator->OpenNextEntry(collect_entry_key);
          EXPECT_EQ(result.net_error(), net::ERR_IO_PENDING);
        });

    disk_cache::EntryResult result = iterator->OpenNextEntry(collect_entry_key);
    ASSERT_EQ(result.net_error(), net::ERR_IO_PENDING);
    run_loop.Run();

    // Corrupt all the files, if we're exercising that.
    if (do_corrupt) {
      for (const auto& key : keys) {
        EXPECT_TRUE(disk_cache::simple_util::CreateCorruptFileForTests(
            key, cache_path_));
      }
    }

    // Open all entries with iterator...
    int opened = 0;
    int iter_opened = 0;
    bool iter_done = false;
    auto all_done = [&]() { return opened == kEntries && iter_done; };

    iterator = cache_->CreateIterator();
    base::RunLoop run_loop2;
    base::RepeatingCallback<void(EntryResult)> handle_entry =
        base::BindLambdaForTesting([&](disk_cache::EntryResult result) {
          ++iter_opened;
          if (result.net_error() == net::ERR_FAILED) {
            EXPECT_EQ(iter_opened - 1, do_corrupt ? 0 : kEntries);
            iter_done = true;
            if (all_done()) {
              run_loop2.Quit();
            }
            return;  // iteration complete.
          }
          EXPECT_EQ(result.net_error(), net::OK);
          result = iterator->OpenNextEntry(handle_entry);
          EXPECT_EQ(result.net_error(), net::ERR_IO_PENDING);
        });

    result = iterator->OpenNextEntry(handle_entry);
    ASSERT_EQ(result.net_error(), net::ERR_IO_PENDING);

    // ... while simultaneously opening them via name.
    auto handle_open_result =
        base::BindLambdaForTesting([&](disk_cache::EntryResult result) {
          int expected_status = do_corrupt ? net::ERR_FAILED : net::OK;
          if (result.net_error() == expected_status) {
            ++opened;
          }
          if (all_done()) {
            run_loop2.Quit();
          }
        });

    base::RepeatingClosure open_one_entry = base::BindLambdaForTesting([&]() {
      std::string key = keys.front();
      keys.pop_front();
      disk_cache::EntryResult result =
          cache_->OpenEntry(key, net::DEFAULT_PRIORITY, handle_open_result);
      if (result.net_error() != net::ERR_IO_PENDING) {
        handle_open_result.Run(std::move(result));
      }

      if (!keys.empty()) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, open_one_entry);
      }
    });
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                             open_one_entry);

    run_loop2.Run();

    // Should not have eaten any entries, if not corrupting them.
    EXPECT_EQ(do_corrupt ? 0 : kEntries, cache_->GetEntryCount());
  }
}

// Make sure that if we close an entry in callback from open/create we do not
// trigger dangling pointer warnings.
TEST_F(DiskCacheBackendTest, BlockFileImmediateCloseNoDangle) {
  InitCache();
  base::RunLoop run_loop;
  EntryResult result =
      cache_->CreateEntry("some key", net::HIGHEST,
                          base::BindLambdaForTesting([&](EntryResult result) {
                            ASSERT_EQ(result.net_error(), net::OK);
                            result.ReleaseEntry()->Close();
                            // Make sure the close actually happens now.
                            disk_cache::BackendImpl::FlushForTesting();
                            run_loop.Quit();
                          }));
  EXPECT_EQ(result.net_error(), net::ERR_IO_PENDING);
  run_loop.Run();
}

// Test that when a write causes a doom, it doesn't result in wrong delivery
// order of callbacks due to re-entrant operation execution.
TEST_F(DiskCacheBackendTest, SimpleWriteOrderEviction) {
  SetSimpleCacheMode();
  SetMaxSize(4096);
  InitCache();

  // Writes of [1, 2, ..., kMaxSize] are more than enough to trigger eviction,
  // as (1 + 80)*80/2 * 2 = 6480 (last * 2 since two streams are written).
  constexpr int kMaxSize = 80;

  scoped_refptr<net::IOBufferWithSize> buffer =
      CacheTestCreateAndFillBuffer(kMaxSize, /*no_nulls=*/false);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry("key", &entry), IsOk());
  ASSERT_TRUE(entry);

  bool expected_next_write_stream_1 = true;
  int expected_next_write_size = 1;
  int next_offset = 0;
  base::RunLoop run_loop;
  for (int size = 1; size <= kMaxSize; ++size) {
    entry->WriteData(/*index=*/1, /*offset = */ next_offset, buffer.get(),
                     /*buf_len=*/size,
                     base::BindLambdaForTesting([&](int result) {
                       EXPECT_TRUE(expected_next_write_stream_1);
                       EXPECT_EQ(result, expected_next_write_size);
                       expected_next_write_stream_1 = false;
                     }),
                     /*truncate=*/true);
    // Stream 0 writes are used here because unlike with stream 1 ones,
    // WriteDataInternal can succeed and queue response callback immediately.
    entry->WriteData(/*index=*/0, /*offset = */ next_offset, buffer.get(),
                     /*buf_len=*/size,
                     base::BindLambdaForTesting([&](int result) {
                       EXPECT_FALSE(expected_next_write_stream_1);
                       EXPECT_EQ(result, expected_next_write_size);
                       expected_next_write_stream_1 = true;
                       ++expected_next_write_size;
                       if (expected_next_write_size == (kMaxSize + 1)) {
                         run_loop.Quit();
                       }
                     }),
                     /*truncate=*/true);
    next_offset += size;
  }

  entry->Close();
  run_loop.Run();
}

// Test that when a write causes a doom, it doesn't result in wrong delivery
// order of callbacks due to re-entrant operation execution. Variant that
// uses stream 0 ops only.
TEST_F(DiskCacheBackendTest, SimpleWriteOrderEvictionStream0) {
  SetSimpleCacheMode();
  SetMaxSize(4096);
  InitCache();

  // Writes of [1, 2, ..., kMaxSize] are more than enough to trigger eviction,
  // as (1 + 120)*120/2 = 7260.
  constexpr int kMaxSize = 120;

  scoped_refptr<net::IOBufferWithSize> buffer =
      CacheTestCreateAndFillBuffer(kMaxSize, /*no_nulls=*/false);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry("key", &entry), IsOk());
  ASSERT_TRUE(entry);

  int expected_next_write_size = 1;
  int next_offset = 0;
  base::RunLoop run_loop;
  for (int size = 1; size <= kMaxSize; ++size) {
    // Stream 0 writes are used here because unlike with stream 1 ones,
    // WriteDataInternal can succeed and queue response callback immediately.
    entry->WriteData(/*index=*/0, /*offset = */ next_offset, buffer.get(),
                     /*buf_len=*/size,
                     base::BindLambdaForTesting([&](int result) {
                       EXPECT_EQ(result, expected_next_write_size);
                       ++expected_next_write_size;
                       if (expected_next_write_size == (kMaxSize + 1)) {
                         run_loop.Quit();
                       }
                     }),
                     /*truncate=*/true);
    next_offset += size;
  }

  entry->Close();
  run_loop.Run();
}

// Test to make sure that if entry creation triggers eviction, a queued up
// close (possible with optimistic ops) doesn't run from within creation
// completion handler (which is indirectly detected as a dangling pointer).
TEST_F(DiskCacheBackendTest, SimpleNoCloseFromWithinCreate) {
  SetSimpleCacheMode();
  SetMaxSize(4096);
  InitCache();

  // Make entries big enough to force their eviction.
  constexpr int kDataSize = 4097;

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kDataSize);
  CacheTestFillBuffer(buffer->data(), kDataSize, false);

  for (int i = 0; i < 100; ++i) {
    std::string key = base::NumberToString(i);
    EntryResult entry_result =
        cache_->CreateEntry(key, net::HIGHEST, base::DoNothing());
    ASSERT_EQ(entry_result.net_error(), net::OK);
    disk_cache::Entry* entry = entry_result.ReleaseEntry();
    // Doing stream 0 write to avoid need for thread round-trips for it to take
    // effect if SimpleEntryImpl runs it.
    entry->WriteData(/*index=*/0, /*offset = */ 0, buffer.get(),
                     /*buf_len=*/kDataSize,
                     base::BindLambdaForTesting(
                         [&](int result) { EXPECT_EQ(kDataSize, result); }),
                     /*truncate=*/true);
    entry->Close();
  }
  RunUntilIdle();
}
