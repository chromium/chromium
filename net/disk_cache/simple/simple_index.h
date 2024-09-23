// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/disk_cache/disk_cache.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace base {
class Pickle;
class PickleIterator;
}

namespace disk_cache {

class BackendCleanupTracker;
class SimpleIndexDelegate;
class SimpleIndexFile;
struct SimpleIndexLoadResult;

class NET_EXPORT_PRIVATE EntryMetadata {
 public:
  EntryMetadata();
  EntryMetadata(base::Time last_used_time,
                base::StrictNumeric<uint32_t> entry_size);
  EntryMetadata(int32_t trailer_prefetch_size,
                base::StrictNumeric<uint32_t> entry_size);

  base::Time GetLastUsedTime() const;
  void SetLastUsedTime(const base::Time& last_used_time);

  int32_t GetTrailerPrefetchSize() const;
  void SetTrailerPrefetchSize(int32_t size);

  uint32_t RawTimeForSorting() const {
    return last_used_time_seconds_since_epoch_;
  }

  uint32_t GetEntrySize() const;
  void SetEntrySize(base::StrictNumeric<uint32_t> entry_size);

  uint8_t GetInMemoryData() const { return in_memory_data_; }
  void SetInMemoryData(uint8_t val) { in_memory_data_ = val; }

  // Serialize the data into the provided pickle.
  void Serialize(net::CacheType cache_type, base::Pickle* pickle) const;
  bool Deserialize(net::CacheType cache_type,
                   base::PickleIterator* it,
                   bool has_entry_in_memory_data,
                   bool app_cache_has_trailer_prefetch_size);

  static base::TimeDelta GetLowerEpsilonForTimeComparisons() {
    return base::Seconds(1);
  }
  static base::TimeDelta GetUpperEpsilonForTimeComparisons() {
    return base::TimeDelta();
  }

  static const int kOnDiskSizeBytes = 16;

 private:
  friend class SimpleIndexFileTest;
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexFileTest, ReadV8Format);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexFileTest, ReadV8FormatAppCache);

  // There are tens of thousands of instances of EntryMetadata in memory, so the
  // size of each entry matters.  Even when the values used to set these members
  // are originally calculated as >32-bit types, the actual necessary size for
  // each shouldn't exceed 32 bits, so we use 32-bit types here.

  // In most modes we track the last access time in order to support automatic
  // eviction. In APP_CACHE mode, however, eviction is disabled. Instead of
  // storing the access time in APP_CACHE mode we instead store a hint about
  // how much entry file trailer should be prefetched when its opened.
  union {
    uint32_t last_used_time_seconds_since_epoch_;
    int32_t trailer_prefetch_size_;  // in bytes
  };

  uint32_t entry_size_256b_chunks_ : 24;  // in 256-byte blocks, rounded up.
  uint32_t in_memory_data_ : 8;
};
static_assert(sizeof(EntryMetadata) == 8, "incorrect metadata size");

// This class is not Thread-safe.
class NET_EXPORT_PRIVATE SimpleIndex final {
 public:
  // Used in histograms. Please only add entries at the end.
  enum IndexInitMethod {
    INITIALIZE_METHOD_RECOVERED = 0,
    INITIALIZE_METHOD_LOADED = 1,
    INITIALIZE_METHOD_NEWCACHE = 2,
    INITIALIZE_METHOD_MAX = 3,
  };
  // Used in histograms. Please only add entries at the end.
  enum IndexWriteToDiskReason {
    INDEX_WRITE_REASON_SHUTDOWN = 0,
    INDEX_WRITE_REASON_STARTUP_MERGE = 1,
    INDEX_WRITE_REASON_IDLE = 2,
    INDEX_WRITE_REASON_ANDROID_STOPPED = 3,
    INDEX_WRITE_REASON_MAX = 4,
  };

  typedef std::vector<uint64_t> HashList;

  SimpleIndex(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
              scoped_refptr<BackendCleanupTracker> cleanup_tracker,
              SimpleIndexDelegate* delegate,
              net::CacheType cache_type,
              std::unique_ptr<SimpleIndexFile> simple_index_file);

  virtual ~SimpleIndex();

  void Initialize(base::Time cache_mtime);

  void SetMaxSize(uint64_t max_bytes);
  uint64_t max_size() const { return max_size_; }

  void Insert(uint64_t entry_hash);
  void Remove(uint64_t entry_hash);

  // Check whether the index has the entry given the hash of its key.
  bool Has(uint64_t entry_hash) const;

  // Update the last used time of the entry with the given key and return true
  // iff the entry exist in the index.
  bool UseIfExists(uint64_t entry_hash);

  uint8_t GetEntryInMemoryData(uint64_t entry_hash) const;
  void SetEntryInMemoryData(uint64_t entry_hash, uint8_t value);

  void WriteToDisk(IndexWriteToDiskReason reason);

  int32_t GetTrailerPrefetchSize(uint64_t entry_hash) const;
  void SetTrailerPrefetchSize(uint64_t entry_hash, int32_t size);

  // Update the size (in bytes) of an entry, in the metadata stored in the
  // index. This should be the total disk-file size including all streams of the
  // entry.
  bool UpdateEntrySize(uint64_t entry_hash,
                       base::StrictNumeric<uint32_t> entry_size);

  using EntrySet = std::unordered_map<uint64_t, EntryMetadata>;

  // Insert an entry in the given set if there is not already entry present.
  // Returns true if the set was modified.
  static bool InsertInEntrySet(uint64_t entry_hash,
                               const EntryMetadata& entry_metadata,
                               EntrySet* entry_set);

  // For use in tests only. Updates cache_size_, but will not start evictions
  // or adjust index writing time. Requires entry to not already be in the set.
  void InsertEntryForTesting(uint64_t entry_hash,
                             const EntryMetadata& entry_metadata);

  // Executes the |callback| when the index is ready. Allows multiple callbacks.
  // Never synchronous.
  void ExecuteWhenReady(net::CompletionOnceCallback callback);

  // Returns entries from the index that have last accessed time matching the
  // range between |initial_time| and |end_time| where open intervals are
  // possible according to the definition given in |DoomEntriesBetween()| in the
  // disk cache backend interface.
  //
  // Access times are not updated in net::APP_CACHE mode.  GetEntriesBetween()
  // should only be called with null times indicating the full range when in
  // this mode.
  std::unique_ptr<HashList> GetEntriesBetween(const base::Time initial_time,
                                              const base::Time end_time);

  // Returns the list of all entries key hash.
  std::unique_ptr<HashList> GetAllHashes();

  // Returns number of indexed entries.
  int32_t GetEntryCount() const;

  // Returns the size of the entire cache in bytes. Can only be called after the
  // index has been initialized.
  uint64_t GetCacheSize() const;

  // Returns the size of the cache entries accessed between |initial_time| and
  // |end_time| in bytes. Can only be called after the index has been
  // initialized.
  uint64_t GetCacheSizeBetween(const base::Time initial_time,
                               const base::Time end_time) const;

  // Returns whether the index has been initialized yet.
  bool initialized() const { return initialized_; }

  IndexInitMethod init_method() const { return init_method_; }

  // Returns base::Time() if hash not known.
  base::Time GetLastUsedTime(uint64_t entry_hash);
  void SetLastUsedTimeForTest(uint64_t entry_hash, const base::Time last_used);

#if BUILDFLAG(IS_ANDROID)
  void set_app_status_listener_getter(
      ApplicationStatusListenerGetter app_status_listener_getter) {
    app_status_listener_getter_ = app_status_listener_getter;
  }
#endif

  // Return true if a pending disk write has been scheduled from
  // PostponeWritingToDisk().
  bool HasPendingWrite() const;

 private:
  friend class SimpleIndexTest;
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexTest, IndexSizeCorrectOnMerge);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexTest, DiskWriteQueued);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexTest, DiskWriteExecuted);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexTest, DiskWritePostponed);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexAppCacheTest, DiskWriteQueued);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexCodeCacheTest, DisableEvictBySize);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexCodeCacheTest, EnableEvictBySize);

  void StartEvictionIfNeeded();
  void EvictionDone(int result);

  void PostponeWritingToDisk();

  // Update the size of the entry pointed to by the given iterator.  Return
  // true if the new size actually results in a change.
  bool UpdateEntryIteratorSize(EntrySet::iterator* it,
                               base::StrictNumeric<uint32_t> entry_size);

  // Must run on IO Thread.
  void MergeInitializingSet(std::unique_ptr<SimpleIndexLoadResult> load_result);

#if BUILDFLAG(IS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState state);

  std::unique_ptr<base::android::ApplicationStatusListener>
      owned_app_status_listener_;
  ApplicationStatusListenerGetter app_status_listener_getter_;
#endif

  scoped_refptr<BackendCleanupTracker> cleanup_tracker_;

  // The owner of |this| must ensure the |delegate_| outlives |this|.
  raw_ptr<SimpleIndexDelegate> delegate_;

  EntrySet entries_set_;

  const net::CacheType cache_type_;
  uint64_t cache_size_ = 0;  // Total cache storage size in bytes.
  uint64_t max_size_ = 0;
  uint64_t high_watermark_ = 0;
  uint64_t low_watermark_ = 0;
  bool eviction_in_progress_ = false;
  base::TimeTicks eviction_start_time_;

  // This stores all the entry_hash of entries that are removed during
  // initialization.
  std::unordered_set<uint64_t> removed_entries_;
  bool initialized_ = false;
  IndexInitMethod init_method_ = INITIALIZE_METHOD_MAX;

  std::unique_ptr<SimpleIndexFile> index_file_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // All nonstatic SimpleEntryImpl methods should always be called on its
  // creation sequance, in all cases. |sequence_checker_| documents and
  // enforces this.
  SEQUENCE_CHECKER(sequence_checker_);

  base::OneShotTimer write_to_disk_timer_;
  base::RepeatingClosure write_to_disk_cb_;

  typedef std::list<net::CompletionOnceCallback> CallbackList;
  CallbackList to_run_when_initialized_;

  // Set to true when the app is on the background. When the app is in the
  // background we can write the index much more frequently, to insure fresh
  // index on next startup.
  bool app_on_background_ = false;

  base::WeakPtrFactory<SimpleIndex> weak_ptr_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
