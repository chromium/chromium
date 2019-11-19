// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/disk_cache_test_base.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_file_tracker.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

DiskCacheTest::DiskCacheTest() {
  CHECK(temp_dir_.CreateUniqueTempDir());
  // Put the cache into a subdir of |temp_dir_|, to permit tests to safely
  // remove the cache directory without risking collisions with other tests.
  cache_path_ = temp_dir_.GetPath().AppendASCII("cache");
  CHECK(base::CreateDirectory(cache_path_));
}

DiskCacheTest::~DiskCacheTest() = default;

bool DiskCacheTest::CopyTestCache(const std::string& name) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("cache_tests");
  path = path.AppendASCII(name);

  if (!CleanupCacheDir())
    return false;
  return base::CopyDirectory(path, cache_path_, false);
}

bool DiskCacheTest::CleanupCacheDir() {
  return DeleteCache(cache_path_);
}

void DiskCacheTest::TearDown() {
  base::RunLoop().RunUntilIdle();
}

DiskCacheTestWithCache::TestIterator::TestIterator(
    std::unique_ptr<disk_cache::Backend::Iterator> iterator)
    : iterator_(std::move(iterator)) {}

DiskCacheTestWithCache::TestIterator::~TestIterator() = default;

int DiskCacheTestWithCache::TestIterator::OpenNextEntry(
    disk_cache::Entry** next_entry) {
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult result =
      cb.GetResult(iterator_->OpenNextEntry(cb.callback()));
  int rv = result.net_error();
  *next_entry = result.ReleaseEntry();
  return rv;
}

DiskCacheTestWithCache::DiskCacheTestWithCache()
    : cache_impl_(nullptr),
      simple_cache_impl_(nullptr),
      mem_cache_(nullptr),
      mask_(0),
      size_(0),
      type_(net::DISK_CACHE),
      memory_only_(false),
      simple_cache_mode_(false),
      simple_cache_wait_for_index_(true),
      force_creation_(false),
      new_eviction_(false),
      first_cleanup_(true),
      integrity_(true),
      use_current_thread_(false) {}

DiskCacheTestWithCache::~DiskCacheTestWithCache() = default;

void DiskCacheTestWithCache::InitCache() {
  if (memory_only_)
    InitMemoryCache();
  else
    InitDiskCache();

  ASSERT_TRUE(nullptr != cache_);
  if (first_cleanup_)
    ASSERT_EQ(0, cache_->GetEntryCount());
}

// We are expected to leak memory when simulating crashes.
void DiskCacheTestWithCache::SimulateCrash() {
  ASSERT_TRUE(!memory_only_);
  net::TestCompletionCallback cb;
  int rv = cache_impl_->FlushQueueForTest(cb.callback());
  ASSERT_THAT(cb.GetResult(rv), IsOk());
  cache_impl_->ClearRefCountForTest();

  cache_.reset();
  EXPECT_TRUE(CheckCacheIntegrity(cache_path_, new_eviction_, size_, mask_));

  CreateBackend(disk_cache::kNoRandom);
}

void DiskCacheTestWithCache::SetTestMode() {
  ASSERT_TRUE(!memory_only_);
  cache_impl_->SetUnitTestMode();
}

void DiskCacheTestWithCache::SetMaxSize(int64_t size, bool should_succeed) {
  size_ = size;
  if (simple_cache_impl_)
    EXPECT_EQ(should_succeed, simple_cache_impl_->SetMaxSize(size));

  if (cache_impl_)
    EXPECT_EQ(should_succeed, cache_impl_->SetMaxSize(size));

  if (mem_cache_)
    EXPECT_EQ(should_succeed, mem_cache_->SetMaxSize(size));
}

disk_cache::EntryResult DiskCacheTestWithCache::OpenOrCreateEntry(
    const std::string& key) {
  return OpenOrCreateEntryWithPriority(key, net::HIGHEST);
}

disk_cache::EntryResult DiskCacheTestWithCache::OpenOrCreateEntryWithPriority(
    const std::string& key,
    net::RequestPriority request_priority) {
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult result =
      cache_->OpenOrCreateEntry(key, request_priority, cb.callback());
  return cb.GetResult(std::move(result));
}

int DiskCacheTestWithCache::OpenEntry(const std::string& key,
                                      disk_cache::Entry** entry) {
  return OpenEntryWithPriority(key, net::HIGHEST, entry);
}

int DiskCacheTestWithCache::OpenEntryWithPriority(
    const std::string& key,
    net::RequestPriority request_priority,
    disk_cache::Entry** entry) {
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult result =
      cb.GetResult(cache_->OpenEntry(key, request_priority, cb.callback()));
  int rv = result.net_error();
  *entry = result.ReleaseEntry();
  return rv;
}

int DiskCacheTestWithCache::CreateEntry(const std::string& key,
                                        disk_cache::Entry** entry) {
  return CreateEntryWithPriority(key, net::HIGHEST, entry);
}

int DiskCacheTestWithCache::CreateEntryWithPriority(
    const std::string& key,
    net::RequestPriority request_priority,
    disk_cache::Entry** entry) {
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult result =
      cb.GetResult(cache_->CreateEntry(key, request_priority, cb.callback()));
  int rv = result.net_error();
  *entry = result.ReleaseEntry();
  return rv;
}

int DiskCacheTestWithCache::DoomEntry(const std::string& key) {
  net::TestCompletionCallback cb;
  int rv = cache_->DoomEntry(key, net::HIGHEST, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::DoomAllEntries() {
  net::TestCompletionCallback cb;
  int rv = cache_->DoomAllEntries(cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::DoomEntriesBetween(const base::Time initial_time,
                                               const base::Time end_time) {
  net::TestCompletionCallback cb;
  int rv = cache_->DoomEntriesBetween(initial_time, end_time, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::DoomEntriesSince(const base::Time initial_time) {
  net::TestCompletionCallback cb;
  int rv = cache_->DoomEntriesSince(initial_time, cb.callback());
  return cb.GetResult(rv);
}

int64_t DiskCacheTestWithCache::CalculateSizeOfAllEntries() {
  net::TestInt64CompletionCallback cb;
  int64_t rv = cache_->CalculateSizeOfAllEntries(cb.callback());
  return cb.GetResult(rv);
}

int64_t DiskCacheTestWithCache::CalculateSizeOfEntriesBetween(
    const base::Time initial_time,
    const base::Time end_time) {
  net::TestInt64CompletionCallback cb;
  int64_t rv = cache_->CalculateSizeOfEntriesBetween(initial_time, end_time,
                                                     cb.callback());
  return cb.GetResult(rv);
}

std::unique_ptr<DiskCacheTestWithCache::TestIterator>
DiskCacheTestWithCache::CreateIterator() {
  return std::unique_ptr<TestIterator>(
      new TestIterator(cache_->CreateIterator()));
}

void DiskCacheTestWithCache::FlushQueueForTest() {
  if (memory_only_ || !cache_impl_)
    return;

  net::TestCompletionCallback cb;
  int rv = cache_impl_->FlushQueueForTest(cb.callback());
  EXPECT_THAT(cb.GetResult(rv), IsOk());
}

void DiskCacheTestWithCache::RunTaskForTest(const base::Closure& closure) {
  if (memory_only_ || !cache_impl_) {
    closure.Run();
    return;
  }

  net::TestCompletionCallback cb;
  int rv = cache_impl_->RunTaskForTest(closure, cb.callback());
  EXPECT_THAT(cb.GetResult(rv), IsOk());
}

int DiskCacheTestWithCache::ReadData(disk_cache::Entry* entry,
                                     int index,
                                     int offset,
                                     net::IOBuffer* buf,
                                     int len) {
  net::TestCompletionCallback cb;
  int rv = entry->ReadData(index, offset, buf, len, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::WriteData(disk_cache::Entry* entry,
                                      int index,
                                      int offset,
                                      net::IOBuffer* buf,
                                      int len,
                                      bool truncate) {
  net::TestCompletionCallback cb;
  int rv = entry->WriteData(index, offset, buf, len, cb.callback(), truncate);
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::ReadSparseData(disk_cache::Entry* entry,
                                           int64_t offset,
                                           net::IOBuffer* buf,
                                           int len) {
  net::TestCompletionCallback cb;
  int rv = entry->ReadSparseData(offset, buf, len, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::WriteSparseData(disk_cache::Entry* entry,
                                            int64_t offset,
                                            net::IOBuffer* buf,
                                            int len) {
  net::TestCompletionCallback cb;
  int rv = entry->WriteSparseData(offset, buf, len, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::GetAvailableRange(disk_cache::Entry* entry,
                                              int64_t offset,
                                              int len,
                                              int64_t* start) {
  net::TestCompletionCallback cb;
  int rv = entry->GetAvailableRange(offset, len, start, cb.callback());
  return cb.GetResult(rv);
}

void DiskCacheTestWithCache::TrimForTest(bool empty) {
  if (memory_only_ || !cache_impl_)
    return;

  RunTaskForTest(base::Bind(&disk_cache::BackendImpl::TrimForTest,
                            base::Unretained(cache_impl_), empty));
}

void DiskCacheTestWithCache::TrimDeletedListForTest(bool empty) {
  if (memory_only_ || !cache_impl_)
    return;

  RunTaskForTest(base::Bind(&disk_cache::BackendImpl::TrimDeletedListForTest,
                            base::Unretained(cache_impl_), empty));
}

void DiskCacheTestWithCache::AddDelay() {
  if (simple_cache_mode_) {
    // The simple cache uses second resolution for many timeouts, so it's safest
    // to advance by at least whole seconds before falling back into the normal
    // disk cache epsilon advance.
    const base::Time initial_time = base::Time::Now();
    do {
      base::PlatformThread::YieldCurrentThread();
    } while (base::Time::Now() - initial_time <
             base::TimeDelta::FromSeconds(1));
  }

  base::Time initial = base::Time::Now();
  while (base::Time::Now() <= initial) {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
  };
}

void DiskCacheTestWithCache::OnExternalCacheHit(const std::string& key) {
  cache_->OnExternalCacheHit(key);
}

void DiskCacheTestWithCache::TearDown() {
  RunUntilIdle();
  cache_.reset();

  if (!memory_only_ && !simple_cache_mode_ && integrity_) {
    EXPECT_TRUE(CheckCacheIntegrity(cache_path_, new_eviction_, size_, mask_));
  }
  RunUntilIdle();
  if (simple_cache_mode_ && simple_file_tracker_)
    EXPECT_TRUE(simple_file_tracker_->IsEmptyForTesting());

  DiskCacheTest::TearDown();
}

void DiskCacheTestWithCache::InitMemoryCache() {
  mem_cache_ = new disk_cache::MemBackendImpl(nullptr);
  cache_.reset(mem_cache_);
  ASSERT_TRUE(cache_);

  if (size_)
    EXPECT_TRUE(mem_cache_->SetMaxSize(size_));

  ASSERT_TRUE(mem_cache_->Init());
}

void DiskCacheTestWithCache::InitDiskCache() {
  if (first_cleanup_)
    ASSERT_TRUE(CleanupCacheDir());

  CreateBackend(disk_cache::kNoRandom);
}

void DiskCacheTestWithCache::CreateBackend(uint32_t flags) {
  scoped_refptr<base::SingleThreadTaskRunner> runner;
  if (use_current_thread_)
    runner = base::ThreadTaskRunnerHandle::Get();
  else
    runner = nullptr;  // let the backend sort it out.

  if (simple_cache_mode_) {
    DCHECK(!use_current_thread_)
        << "Using current thread unsupported by SimpleCache";
    net::TestCompletionCallback cb;
    // We limit ourselves to 64 fds since OS X by default gives us 256.
    // (Chrome raises the number on startup, but the test fixture doesn't).
    if (!simple_file_tracker_)
      simple_file_tracker_ =
          std::make_unique<disk_cache::SimpleFileTracker>(64);
    std::unique_ptr<disk_cache::SimpleBackendImpl> simple_backend =
        std::make_unique<disk_cache::SimpleBackendImpl>(
            cache_path_, /* cleanup_tracker = */ nullptr,
            simple_file_tracker_.get(), size_, type_, /*net_log = */ nullptr);
    int rv = simple_backend->Init(cb.callback());
    ASSERT_THAT(cb.GetResult(rv), IsOk());
    simple_cache_impl_ = simple_backend.get();
    cache_ = std::move(simple_backend);
    if (simple_cache_wait_for_index_) {
      net::TestCompletionCallback wait_for_index_cb;
      simple_cache_impl_->index()->ExecuteWhenReady(
          wait_for_index_cb.callback());
      rv = wait_for_index_cb.WaitForResult();
      ASSERT_THAT(rv, IsOk());
    }
    return;
  }

  if (mask_)
    cache_impl_ = new disk_cache::BackendImpl(cache_path_, mask_, runner, type_,
                                              /* net_log = */ nullptr);
  else
    cache_impl_ = new disk_cache::BackendImpl(
        cache_path_, /* cleanup_tracker = */ nullptr, runner, type_,
        /* net_log = */ nullptr);
  cache_.reset(cache_impl_);
  ASSERT_TRUE(cache_);
  if (size_)
    EXPECT_TRUE(cache_impl_->SetMaxSize(size_));
  if (new_eviction_)
    cache_impl_->SetNewEviction();
  cache_impl_->SetFlags(flags);
  net::TestCompletionCallback cb;
  int rv = cache_impl_->Init(cb.callback());
  ASSERT_THAT(cb.GetResult(rv), IsOk());
}
