// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_TEST_BASE_H_
#define NET_DISK_CACHE_DISK_CACHE_TEST_BASE_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/buildflag.h"
#include "net/base/cache_type.h"
#include "net/disk_cache/buildflags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

class IOBuffer;

}  // namespace net

namespace disk_cache {

class Backend;
class BackendImpl;
class Entry;
class MemBackendImpl;
class SimpleBackendImpl;
class SimpleFileTracker;

#if BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND)
class SqlBackendImpl;
#endif  // ENABLE_DISK_CACHE_SQL_BACKEND

}  // namespace disk_cache

// These tests can use the path service, which uses autoreleased objects on the
// Mac, so this needs to be a PlatformTest.  Even tests that do not require a
// cache (and that do not need to be a DiskCacheTestWithCache) are susceptible
// to this problem; all such tests should use TEST_F(DiskCacheTest, ...).
class DiskCacheTest : public PlatformTest, public net::WithTaskEnvironment {
 protected:
  explicit DiskCacheTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  ~DiskCacheTest() override;

  // Copies a set of cache files from the data folder to the test folder.
  bool CopyTestCache(const std::string& name);

  // Deletes the contents of |cache_path_|.
  bool CleanupCacheDir();

  void TearDown() override;

  base::FilePath cache_path_;

 private:
  base::ScopedTempDir temp_dir_;
};

// Provides basic support for cache related tests.
class DiskCacheTestWithCache : public DiskCacheTest {
 public:
  enum class BackendToTest {
    kBlockfile,
    kSimple,
    kMemory,
#if BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND)
    kSql
#endif  // ENABLE_DISK_CACHE_SQL_BACKEND
  };
  static std::string BackendToTestName(BackendToTest backend_to_test);

 protected:
  class TestIterator {
   public:
    explicit TestIterator(
        std::unique_ptr<disk_cache::Backend::Iterator> iterator);
    ~TestIterator();

    int OpenNextEntry(disk_cache::Entry** next_entry);

   private:
    std::unique_ptr<disk_cache::Backend::Iterator> iterator_;
  };

  explicit DiskCacheTestWithCache(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  DiskCacheTestWithCache(const DiskCacheTestWithCache&) = delete;
  DiskCacheTestWithCache& operator=(const DiskCacheTestWithCache&) = delete;

  ~DiskCacheTestWithCache() override;

  void CreateBackend(uint32_t flags);

  void ResetCaches();
  void InitCache();
  void SimulateCrash();
  void SetTestMode();
#if BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND)
  void LoadInMemoryIndex();
#endif  // ENABLE_DISK_CACHE_SQL_BACKEND

  void SetBackendToTest(BackendToTest backend_to_test) {
    backend_to_test_ = backend_to_test;
    CHECK(!(backend_to_test_ == BackendToTest::kSimple && use_current_thread_));
  }
  BackendToTest backend_to_test() const { return backend_to_test_; }

  void SetMask(uint32_t mask) { mask_ = mask; }

  void SetMaxSize(int64_t size);

  // Returns value last given to SetMaxSize (or 0).
  int MaxSize() const { return size_; }

  // Deletes and re-creates the files on initialization errors.
  void SetForceCreation() {
    force_creation_ = true;
  }

  void SetNewEviction() {
    new_eviction_ = true;
  }

  void DisableSimpleCacheWaitForIndex() {
    simple_cache_wait_for_index_ = false;
  }

  void DisableFirstCleanup() {
    first_cleanup_ = false;
  }

  void DisableIntegrityCheck() {
    integrity_ = false;
  }

  // Forces all execution to happen on the current thread. This affects the
  // blockfile cache; and is a no-op for the memory backend which is
  // single-threaded to start with. It can't be used with the simple backend.
  void UseCurrentThread() {
    DCHECK_NE(backend_to_test_, BackendToTest::kSimple);
    use_current_thread_ = true;
  }

  void SetCacheType(net::CacheType type) {
    type_ = type;
  }

  // Utility methods to access the cache and wait for each operation to finish.
  // Also closer to legacy API.
  // TODO(morlovich): Port all the tests to EntryResult.
  int32_t GetEntryCount();
  disk_cache::EntryResult OpenOrCreateEntry(const std::string& key);
  disk_cache::EntryResult OpenOrCreateEntryWithPriority(
      const std::string& key,
      net::RequestPriority request_priority);
  int OpenEntry(const std::string& key, disk_cache::Entry** entry);
  int OpenEntryWithPriority(const std::string& key,
                            net::RequestPriority request_priority,
                            disk_cache::Entry** entry);
  int CreateEntry(const std::string& key, disk_cache::Entry** entry);
  int CreateEntryWithPriority(const std::string& key,
                              net::RequestPriority request_priority,
                              disk_cache::Entry** entry);
  int DoomEntry(const std::string& key);
  int DoomAllEntries();
  int DoomEntriesBetween(const base::Time initial_time,
                         const base::Time end_time);
  int64_t CalculateSizeOfAllEntries();
  int64_t CalculateSizeOfEntriesBetween(const base::Time initial_time,
                                        const base::Time end_time);
  int DoomEntriesSince(const base::Time initial_time);
  std::unique_ptr<TestIterator> CreateIterator();
  void FlushQueueForTest();
  void RunTaskForTest(base::OnceClosure closure);
  int ReadData(disk_cache::Entry* entry, int index, int offset,
               net::IOBuffer* buf, int len);
  int WriteData(disk_cache::Entry* entry, int index, int offset,
                net::IOBuffer* buf, int len, bool truncate);
  int ReadSparseData(disk_cache::Entry* entry,
                     int64_t offset,
                     net::IOBuffer* buf,
                     int len);
  int WriteSparseData(disk_cache::Entry* entry,
                      int64_t offset,
                      net::IOBuffer* buf,
                      int len);
  // TODO(morlovich): Port all the tests using this to RangeResult.
  int GetAvailableRange(disk_cache::Entry* entry,
                        int64_t offset,
                        int len,
                        int64_t* start);

  // Asks the cache to trim an entry. If |empty| is true, the whole cache is
  // deleted.
  void TrimForTest(bool empty);

  // Asks the cache to trim an entry from the deleted list. If |empty| is
  // true, the whole list is deleted.
  void TrimDeletedListForTest(bool empty);

  // Makes sure that some time passes before continuing the test. Time::Now()
  // before and after this method will not be the same.
  void AddDelay();

  void OnExternalCacheHit(const std::string& key);

  std::unique_ptr<disk_cache::Backend> TakeCache();

  void TearDown() override;

  // cache_ will always have a valid object, regardless of how the cache was
  // initialized. The implementation pointers can be NULL.
  std::unique_ptr<disk_cache::Backend> cache_;
  raw_ptr<disk_cache::BackendImpl> cache_impl_ = nullptr;
  std::unique_ptr<disk_cache::SimpleFileTracker> simple_file_tracker_;
  raw_ptr<disk_cache::SimpleBackendImpl> simple_cache_impl_ = nullptr;
#if BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND)
  raw_ptr<disk_cache::SqlBackendImpl> sql_cache_impl_ = nullptr;
#endif  // ENABLE_DISK_CACHE_SQL_BACKEND
  raw_ptr<disk_cache::MemBackendImpl> mem_cache_ = nullptr;

  uint32_t mask_ = 0;
  int64_t size_ = 0;
  net::CacheType type_ = net::DISK_CACHE;
  BackendToTest backend_to_test_ = BackendToTest::kBlockfile;

  bool simple_cache_wait_for_index_ = true;
  bool force_creation_ = false;
  bool new_eviction_ = false;
  bool first_cleanup_ = true;
  bool integrity_ = true;
  bool use_current_thread_ = false;
  // This is intentionally left uninitialized, to be used by any test.
  bool success_;

 private:
  void InitMemoryCache();
  void InitDiskCache();
};

#endif  // NET_DISK_CACHE_DISK_CACHE_TEST_BASE_H_
