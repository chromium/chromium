// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <limits>
#include <memory>
#include <string>

#include "base/barrier_closure.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/block_files.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "testing/platform_test.h"

using base::Time;

namespace {

const size_t kNumEntries = 10000;
const int kHeadersSize = 2000;

const int kBodySize = 72 * 1024 - 1;

// HttpCache likes this chunk size.
const int kChunkSize = 32 * 1024;

// As of 2017-01-12, this is a typical per-tab limit on HTTP connections.
const int kMaxParallelOperations = 10;

static constexpr char kMetricPrefixDiskCache[] = "DiskCache.";
static constexpr char kMetricPrefixSimpleIndex[] = "SimpleIndex.";
static constexpr char kMetricCacheEntriesWriteTimeMs[] =
    "cache_entries_write_time";
static constexpr char kMetricCacheHeadersReadTimeColdMs[] =
    "cache_headers_read_time_cold";
static constexpr char kMetricCacheHeadersReadTimeWarmMs[] =
    "cache_headers_read_time_warm";
static constexpr char kMetricCacheEntriesReadTimeColdMs[] =
    "cache_entries_read_time_cold";
static constexpr char kMetricCacheEntriesReadTimeWarmMs[] =
    "cache_entries_read_time_warm";
static constexpr char kMetricCacheKeysHashTimeMs[] = "cache_keys_hash_time";
static constexpr char kMetricFillBlocksTimeMs[] = "fill_sequential_blocks_time";
static constexpr char kMetricCreateDeleteBlocksTimeMs[] =
    "create_and_delete_random_blocks_time";
static constexpr char kMetricSimpleCacheInitTotalTimeMs[] =
    "simple_cache_initial_read_total_time";
static constexpr char kMetricSimpleCacheInitPerEntryTimeUs[] =
    "simple_cache_initial_read_per_entry_time";
static constexpr char kMetricAverageEvictionTimeMs[] = "average_eviction_time";

perf_test::PerfResultReporter SetUpDiskCacheReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixDiskCache, story);
  reporter.RegisterImportantMetric(kMetricCacheEntriesWriteTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricCacheHeadersReadTimeColdMs, "ms");
  reporter.RegisterImportantMetric(kMetricCacheHeadersReadTimeWarmMs, "ms");
  reporter.RegisterImportantMetric(kMetricCacheEntriesReadTimeColdMs, "ms");
  reporter.RegisterImportantMetric(kMetricCacheEntriesReadTimeWarmMs, "ms");
  reporter.RegisterImportantMetric(kMetricCacheKeysHashTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricFillBlocksTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricCreateDeleteBlocksTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricSimpleCacheInitTotalTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricSimpleCacheInitPerEntryTimeUs, "us");
  return reporter;
}

perf_test::PerfResultReporter SetUpSimpleIndexReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixSimpleIndex, story);
  reporter.RegisterImportantMetric(kMetricAverageEvictionTimeMs, "ms");
  return reporter;
}

void MaybeIncreaseFdLimitTo(unsigned int max_descriptors) {
#if BUILDFLAG(IS_POSIX)
  base::IncreaseFdLimitTo(max_descriptors);
#endif
}

struct TestEntry {
  std::string key;
  int data_len;
};

enum class WhatToRead {
  HEADERS_ONLY,
  HEADERS_AND_BODY,
};

class DiskCachePerfTest : public DiskCacheTestWithCache {
 public:
  DiskCachePerfTest() { MaybeIncreaseFdLimitTo(kFdLimitForCacheTests); }

  const std::vector<TestEntry>& entries() const { return entries_; }

 protected:
  // Helper methods for constructing tests.
  bool TimeWrites(const std::string& story);
  bool TimeReads(WhatToRead what_to_read,
                 const std::string& metric,
                 const std::string& story);
  void ResetAndEvictSystemDiskCache();

  // Callbacks used within tests for intermediate operations.
  void WriteCallback(net::CompletionOnceCallback final_callback,
                     scoped_refptr<net::IOBuffer> headers_buffer,
                     scoped_refptr<net::IOBuffer> body_buffer,
                     disk_cache::Entry* cache_entry,
                     int entry_index,
                     size_t write_offset,
                     int result);

  // Complete perf tests.
  void CacheBackendPerformance(const std::string& story);

  const size_t kFdLimitForCacheTests = 8192;

  std::vector<TestEntry> entries_;
};

class WriteHandler {
 public:
  WriteHandler(const DiskCachePerfTest* test,
               disk_cache::Backend* cache,
               net::CompletionOnceCallback final_callback)
      : test_(test), cache_(cache), final_callback_(std::move(final_callback)) {
    CacheTestFillBuffer(headers_buffer_->data(), kHeadersSize, false);
    CacheTestFillBuffer(body_buffer_->data(), kChunkSize, false);
  }

  void Run();

 protected:
  void CreateNextEntry();

  void CreateCallback(int data_len, disk_cache::EntryResult result);
  void WriteDataCallback(disk_cache::Entry* entry,
                         int next_offset,
                         int data_len,
                         int expected_result,
                         int result);

 private:
  bool CheckForErrorAndCancel(int result);

  raw_ptr<const DiskCachePerfTest> test_;
  raw_ptr<disk_cache::Backend> cache_;
  net::CompletionOnceCallback final_callback_;

  size_t next_entry_index_ = 0;
  size_t pending_operations_count_ = 0;

  int pending_result_ = net::OK;

  scoped_refptr<net::IOBuffer> headers_buffer_ =
      base::MakeRefCounted<net::IOBufferWithSize>(kHeadersSize);
  scoped_refptr<net::IOBuffer> body_buffer_ =
      base::MakeRefCounted<net::IOBufferWithSize>(kChunkSize);
};

void WriteHandler::Run() {
  for (int i = 0; i < kMaxParallelOperations; ++i) {
    ++pending_operations_count_;
    CreateNextEntry();
  }
}

void WriteHandler::CreateNextEntry() {
  ASSERT_GT(kNumEntries, next_entry_index_);
  TestEntry test_entry = test_->entries()[next_entry_index_++];
  auto callback =
      base::BindRepeating(&WriteHandler::CreateCallback, base::Unretained(this),
                          test_entry.data_len);
  disk_cache::EntryResult result =
      cache_->CreateEntry(test_entry.key, net::HIGHEST, callback);
  if (result.net_error() != net::ERR_IO_PENDING)
    callback.Run(std::move(result));
}

void WriteHandler::CreateCallback(int data_len,
                                  disk_cache::EntryResult result) {
  if (CheckForErrorAndCancel(result.net_error()))
    return;

  disk_cache::Entry* entry = result.ReleaseEntry();
  net::CompletionRepeatingCallback callback = base::BindRepeating(
      &WriteHandler::WriteDataCallback, base::Unretained(this), entry, 0,
      data_len, kHeadersSize);
  int new_result = entry->WriteData(0, 0, headers_buffer_.get(), kHeadersSize,
                                    callback, false);
  if (new_result != net::ERR_IO_PENDING)
    callback.Run(new_result);
}

void WriteHandler::WriteDataCallback(disk_cache::Entry* entry,
                                     int next_offset,
                                     int data_len,
                                     int expected_result,
                                     int result) {
  if (CheckForErrorAndCancel(result)) {
    entry->Close();
    return;
  }
  DCHECK_LE(next_offset, data_len);
  if (next_offset == data_len) {
    entry->Close();
    if (next_entry_index_ < kNumEntries) {
      CreateNextEntry();
    } else {
      --pending_operations_count_;
      if (pending_operations_count_ == 0)
        std::move(final_callback_).Run(net::OK);
    }
    return;
  }

  int write_size = std::min(kChunkSize, data_len - next_offset);
  net::CompletionRepeatingCallback callback = base::BindRepeating(
      &WriteHandler::WriteDataCallback, base::Unretained(this), entry,
      next_offset + write_size, data_len, write_size);
  int new_result = entry->WriteData(1, next_offset, body_buffer_.get(),
                                    write_size, callback, true);
  if (new_result != net::ERR_IO_PENDING)
    callback.Run(new_result);
}

bool WriteHandler::CheckForErrorAndCancel(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result != net::OK && !(result > 0))
    pending_result_ = result;
  if (pending_result_ != net::OK) {
    --pending_operations_count_;
    if (pending_operations_count_ == 0)
      std::move(final_callback_).Run(pending_result_);
    return true;
  }
  return false;
}

class ReadHandler {
 public:
  ReadHandler(const DiskCachePerfTest* test,
              WhatToRead what_to_read,
              disk_cache::Backend* cache,
              net::CompletionOnceCallback final_callback)
      : test_(test),
        what_to_read_(what_to_read),
        cache_(cache),
        final_callback_(std::move(final_callback)) {
    for (auto& read_buffer : read_buffers_) {
      read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(
          std::max(kHeadersSize, kChunkSize));
    }
  }

  void Run();

 protected:
  void OpenNextEntry(int parallel_operation_index);

  void OpenCallback(int parallel_operation_index,
                    int data_len,
                    disk_cache::EntryResult result);
  void ReadDataCallback(int parallel_operation_index,
                        disk_cache::Entry* entry,
                        int next_offset,
                        int data_len,
                        int expected_result,
                        int result);

 private:
  bool CheckForErrorAndCancel(int result);

  raw_ptr<const DiskCachePerfTest> test_;
  const WhatToRead what_to_read_;

  raw_ptr<disk_cache::Backend> cache_;
  net::CompletionOnceCallback final_callback_;

  size_t next_entry_index_ = 0;
  size_t pending_operations_count_ = 0;

  int pending_result_ = net::OK;

  scoped_refptr<net::IOBuffer> read_buffers_[kMaxParallelOperations];
};

void ReadHandler::Run() {
  for (int i = 0; i < kMaxParallelOperations; ++i) {
    OpenNextEntry(pending_operations_count_);
    ++pending_operations_count_;
  }
}

void ReadHandler::OpenNextEntry(int parallel_operation_index) {
  ASSERT_GT(kNumEntries, next_entry_index_);
  TestEntry test_entry = test_->entries()[next_entry_index_++];
  auto callback =
      base::BindRepeating(&ReadHandler::OpenCallback, base::Unretained(this),
                          parallel_operation_index, test_entry.data_len);
  disk_cache::EntryResult result =
      cache_->OpenEntry(test_entry.key, net::HIGHEST, callback);
  if (result.net_error() != net::ERR_IO_PENDING)
    callback.Run(std::move(result));
}

void ReadHandler::OpenCallback(int parallel_operation_index,
                               int data_len,
                               disk_cache::EntryResult result) {
  if (CheckForErrorAndCancel(result.net_error()))
    return;

  disk_cache::Entry* entry = result.ReleaseEntry();

  EXPECT_EQ(data_len, entry->GetDataSize(1));

  net::CompletionRepeatingCallback callback = base::BindRepeating(
      &ReadHandler::ReadDataCallback, base::Unretained(this),
      parallel_operation_index, entry, 0, data_len, kHeadersSize);
  int new_result =
      entry->ReadData(0, 0, read_buffers_[parallel_operation_index].get(),
                      kChunkSize, callback);
  if (new_result != net::ERR_IO_PENDING)
    callback.Run(new_result);
}

void ReadHandler::ReadDataCallback(int parallel_operation_index,
                                   disk_cache::Entry* entry,
                                   int next_offset,
                                   int data_len,
                                   int expected_result,
                                   int result) {
  if (CheckForErrorAndCancel(result)) {
    entry->Close();
    return;
  }
  DCHECK_LE(next_offset, data_len);
  if (what_to_read_ == WhatToRead::HEADERS_ONLY || next_offset == data_len) {
    entry->Close();
    if (next_entry_index_ < kNumEntries) {
      OpenNextEntry(parallel_operation_index);
    } else {
      --pending_operations_count_;
      if (pending_operations_count_ == 0)
        std::move(final_callback_).Run(net::OK);
    }
    return;
  }

  int expected_read_size = std::min(kChunkSize, data_len - next_offset);
  net::CompletionRepeatingCallback callback = base::BindRepeating(
      &ReadHandler::ReadDataCallback, base::Unretained(this),
      parallel_operation_index, entry, next_offset + expected_read_size,
      data_len, expected_read_size);
  int new_result = entry->ReadData(
      1, next_offset, read_buffers_[parallel_operation_index].get(), kChunkSize,
      callback);
  if (new_result != net::ERR_IO_PENDING)
    callback.Run(new_result);
}

bool ReadHandler::CheckForErrorAndCancel(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result != net::OK && !(result > 0))
    pending_result_ = result;
  if (pending_result_ != net::OK) {
    --pending_operations_count_;
    if (pending_operations_count_ == 0)
      std::move(final_callback_).Run(pending_result_);
    return true;
  }
  return false;
}

bool DiskCachePerfTest::TimeWrites(const std::string& story) {
  for (size_t i = 0; i < kNumEntries; i++) {
    TestEntry entry;
    entry.key = GenerateKey(true);
    entry.data_len = base::RandInt(0, kBodySize);
    entries_.push_back(entry);
  }

  net::TestCompletionCallback cb;

  auto reporter = SetUpDiskCacheReporter(story);
  base::ElapsedTimer write_timer;

  WriteHandler write_handler(this, cache_.get(), cb.callback());
  write_handler.Run();
  auto result = cb.WaitForResult();
  reporter.AddResult(kMetricCacheEntriesWriteTimeMs,
                     write_timer.Elapsed().InMillisecondsF());
  return result == net::OK;
}

bool DiskCachePerfTest::TimeReads(WhatToRead what_to_read,
                                  const std::string& metric,
                                  const std::string& story) {
  auto reporter = SetUpDiskCacheReporter(story);
  base::ElapsedTimer timer;

  net::TestCompletionCallback cb;
  ReadHandler read_handler(this, what_to_read, cache_.get(), cb.callback());
  read_handler.Run();
  auto result = cb.WaitForResult();
  reporter.AddResult(metric, timer.Elapsed().InMillisecondsF());
  return result == net::OK;
}

TEST_F(DiskCachePerfTest, BlockfileHashes) {
  auto reporter = SetUpDiskCacheReporter("baseline_story");
  base::ElapsedTimer timer;
  for (int i = 0; i < 300000; i++) {
    std::string key = GenerateKey(true);
    // TODO(dcheng): It's unclear if this is sufficient to keep a sufficiently
    // smart optimizer from simply discarding the function call if it realizes
    // there are no side effects.
    base::PersistentHash(key);
  }
  reporter.AddResult(kMetricCacheKeysHashTimeMs,
                     timer.Elapsed().InMillisecondsF());
}

void DiskCachePerfTest::ResetAndEvictSystemDiskCache() {
  base::RunLoop().RunUntilIdle();
  cache_.reset();

  // Flush all files in the cache out of system memory.
  const base::FilePath::StringType file_pattern = FILE_PATH_LITERAL("*");
  base::FileEnumerator enumerator(cache_path_, true /* recursive */,
                                  base::FileEnumerator::FILES, file_pattern);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    ASSERT_TRUE(base::EvictFileFromSystemCache(file_path));
  }
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // And, cache directories, on platforms where the eviction utility supports
  // this (currently Linux and Android only).
  if (simple_cache_mode_) {
    ASSERT_TRUE(
        base::EvictFileFromSystemCache(cache_path_.AppendASCII("index-dir")));
  }
  ASSERT_TRUE(base::EvictFileFromSystemCache(cache_path_));
#endif

  DisableFirstCleanup();
  InitCache();
}

void DiskCachePerfTest::CacheBackendPerformance(const std::string& story) {
  base::test::ScopedRunLoopTimeout default_timeout(
      FROM_HERE, TestTimeouts::action_max_timeout());

  LOG(ERROR) << "Using cache at:" << cache_path_.MaybeAsASCII();
  SetMaxSize(500 * 1024 * 1024);
  InitCache();
  EXPECT_TRUE(TimeWrites(story));

  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  ResetAndEvictSystemDiskCache();
  EXPECT_TRUE(TimeReads(WhatToRead::HEADERS_ONLY,
                        kMetricCacheHeadersReadTimeColdMs, story));
  EXPECT_TRUE(TimeReads(WhatToRead::HEADERS_ONLY,
                        kMetricCacheHeadersReadTimeWarmMs, story));

  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  ResetAndEvictSystemDiskCache();
  EXPECT_TRUE(TimeReads(WhatToRead::HEADERS_AND_BODY,
                        kMetricCacheEntriesReadTimeColdMs, story));
  EXPECT_TRUE(TimeReads(WhatToRead::HEADERS_AND_BODY,
                        kMetricCacheEntriesReadTimeWarmMs, story));

  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/41393579): Fix this test on Fuchsia and re-enable.
#define MAYBE_CacheBackendPerformance DISABLED_CacheBackendPerformance
#else
#define MAYBE_CacheBackendPerformance CacheBackendPerformance
#endif
TEST_F(DiskCachePerfTest, MAYBE_CacheBackendPerformance) {
  CacheBackendPerformance("blockfile_cache");
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/41393579): Fix this test on Fuchsia and re-enable.
#define MAYBE_SimpleCacheBackendPerformance \
  DISABLED_SimpleCacheBackendPerformance
#else
#define MAYBE_SimpleCacheBackendPerformance SimpleCacheBackendPerformance
#endif
TEST_F(DiskCachePerfTest, MAYBE_SimpleCacheBackendPerformance) {
  SetSimpleCacheMode();
  CacheBackendPerformance("simple_cache");
}

// Creating and deleting "entries" on a block-file is something quite frequent
// (after all, almost everything is stored on block files). The operation is
// almost free when the file is empty, but can be expensive if the file gets
// fragmented, or if we have multiple files. This test measures that scenario,
// by using multiple, highly fragmented files.
TEST_F(DiskCachePerfTest, BlockFilesPerformance) {
  ASSERT_TRUE(CleanupCacheDir());

  disk_cache::BlockFiles files(cache_path_);
  ASSERT_TRUE(files.Init(true));

  const int kNumBlocks = 60000;
  disk_cache::Addr address[kNumBlocks];

  auto reporter = SetUpDiskCacheReporter("blockfile_cache");
  base::ElapsedTimer sequential_timer;

  // Fill up the 32-byte block file (use three files).
  for (auto& addr : address) {
    int block_size = base::RandInt(1, 4);
    EXPECT_TRUE(files.CreateBlock(disk_cache::RANKINGS, block_size, &addr));
  }

  reporter.AddResult(kMetricFillBlocksTimeMs,
                     sequential_timer.Elapsed().InMillisecondsF());
  base::ElapsedTimer random_timer;

  for (int i = 0; i < 200000; i++) {
    int block_size = base::RandInt(1, 4);
    int entry = base::RandInt(0, kNumBlocks - 1);

    files.DeleteBlock(address[entry], false);
    EXPECT_TRUE(
        files.CreateBlock(disk_cache::RANKINGS, block_size, &address[entry]));
  }

  reporter.AddResult(kMetricCreateDeleteBlocksTimeMs,
                     random_timer.Elapsed().InMillisecondsF());
  base::RunLoop().RunUntilIdle();
}

void VerifyRvAndCallClosure(base::RepeatingClosure* c, int expect_rv, int rv) {
  EXPECT_EQ(expect_rv, rv);
  c->Run();
}

TEST_F(DiskCachePerfTest, SimpleCacheInitialReadPortion) {
  // A benchmark that aims to measure how much time we take in I/O thread
  // for initial bookkeeping before returning to the caller, and how much
  // after (batched up some). The later portion includes some event loop
  // overhead.
  const int kBatchSize = 100;

  SetSimpleCacheMode();

  InitCache();
  // Write out the entries, and keep their objects around.
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kHeadersSize);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kBodySize);

  CacheTestFillBuffer(buffer1->data(), kHeadersSize, false);
  CacheTestFillBuffer(buffer2->data(), kBodySize, false);

  disk_cache::Entry* cache_entry[kBatchSize];
  for (int i = 0; i < kBatchSize; ++i) {
    TestEntryResultCompletionCallback cb_create;
    disk_cache::EntryResult result = cb_create.GetResult(cache_->CreateEntry(
        base::NumberToString(i), net::HIGHEST, cb_create.callback()));
    ASSERT_EQ(net::OK, result.net_error());
    cache_entry[i] = result.ReleaseEntry();

    net::TestCompletionCallback cb;
    int rv = cache_entry[i]->WriteData(0, 0, buffer1.get(), kHeadersSize,
                                       cb.callback(), false);
    ASSERT_EQ(kHeadersSize, cb.GetResult(rv));
    rv = cache_entry[i]->WriteData(1, 0, buffer2.get(), kBodySize,
                                   cb.callback(), false);
    ASSERT_EQ(kBodySize, cb.GetResult(rv));
  }

  // Now repeatedly read these, batching up the waiting to try to
  // account for the two portions separately. Note that we need separate entries
  // since we are trying to keep interesting work from being on the delayed-done
  // portion.
  const int kIterations = 50000;

  double elapsed_early = 0.0;
  double elapsed_late = 0.0;

  for (int i = 0; i < kIterations; ++i) {
    base::RunLoop event_loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(kBatchSize, event_loop.QuitWhenIdleClosure());
    net::CompletionRepeatingCallback cb_batch(base::BindRepeating(
        VerifyRvAndCallClosure, base::Unretained(&barrier), kHeadersSize));

    base::ElapsedTimer timer_early;
    for (auto* entry : cache_entry) {
      int rv = entry->ReadData(0, 0, buffer1.get(), kHeadersSize, cb_batch);
      if (rv != net::ERR_IO_PENDING) {
        barrier.Run();
        ASSERT_EQ(kHeadersSize, rv);
      }
    }
    elapsed_early += timer_early.Elapsed().InMillisecondsF();

    base::ElapsedTimer timer_late;
    event_loop.Run();
    elapsed_late += timer_late.Elapsed().InMillisecondsF();
  }

  // Cleanup
  for (auto* entry : cache_entry)
    entry->Close();

  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();
  auto reporter = SetUpDiskCacheReporter("early_portion");
  reporter.AddResult(kMetricSimpleCacheInitTotalTimeMs, elapsed_early);
  reporter.AddResult(kMetricSimpleCacheInitPerEntryTimeUs,
                     1000 * (elapsed_early / (kIterations * kBatchSize)));
  reporter = SetUpDiskCacheReporter("event_loop_portion");
  reporter.AddResult(kMetricSimpleCacheInitTotalTimeMs, elapsed_late);
  reporter.AddResult(kMetricSimpleCacheInitPerEntryTimeUs,
                     1000 * (elapsed_late / (kIterations * kBatchSize)));
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40222788): Fix this test on Fuchsia and re-enable.
#define MAYBE_EvictionPerformance DISABLED_EvictionPerformance
#else
#define MAYBE_EvictionPerformance EvictionPerformance
#endif
// Measures how quickly SimpleIndex can compute which entries to evict.
TEST(SimpleIndexPerfTest, MAYBE_EvictionPerformance) {
  const int kEntries = 10000;

  class NoOpDelegate : public disk_cache::SimpleIndexDelegate {
    void DoomEntries(std::vector<uint64_t>* entry_hashes,
                     net::CompletionOnceCallback callback) override {}
  };

  NoOpDelegate delegate;
  base::Time start(base::Time::Now());

  double evict_elapsed_ms = 0;
  int iterations = 0;
  while (iterations < 61000) {
    ++iterations;
    disk_cache::SimpleIndex index(/* io_thread = */ nullptr,
                                  /* cleanup_tracker = */ nullptr, &delegate,
                                  net::DISK_CACHE,
                                  /* simple_index_file = */ nullptr);

    // Make sure large enough to not evict on insertion.
    index.SetMaxSize(kEntries * 2);

    for (int i = 0; i < kEntries; ++i) {
      index.InsertEntryForTesting(
          i, disk_cache::EntryMetadata(start + base::Seconds(i), 1u));
    }

    // Trigger an eviction.
    base::ElapsedTimer timer;
    index.SetMaxSize(kEntries);
    index.UpdateEntrySize(0, 1u);
    evict_elapsed_ms += timer.Elapsed().InMillisecondsF();
  }

  auto reporter = SetUpSimpleIndexReporter("baseline_story");
  reporter.AddResult(kMetricAverageEvictionTimeMs,
                     evict_elapsed_ms / iterations);
}

}  // namespace
