// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_BLOCKFILE_BACKEND_IMPL_H_
#define NET_DISK_CACHE_BLOCKFILE_BACKEND_IMPL_H_

#include <stdint.h>

#include <unordered_map>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "net/base/net_export.h"
#include "net/disk_cache/blockfile/block_files.h"
#include "net/disk_cache/blockfile/eviction.h"
#include "net/disk_cache/blockfile/in_flight_backend_io.h"
#include "net/disk_cache/blockfile/rankings.h"
#include "net/disk_cache/blockfile/stats.h"
#include "net/disk_cache/blockfile/stress_support.h"
#include "net/disk_cache/disk_cache.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class NetLog;
}  // namespace net

namespace disk_cache {

class BackendCleanupTracker;
struct Index;

enum BackendFlags {
  kNone = 0,
  kMask = 1,                    // A mask (for the index table) was specified.
  kMaxSize = 1 << 1,            // A maximum size was provided.
  kUnitTestMode = 1 << 2,       // We are modifying the behavior for testing.
  kUpgradeMode = 1 << 3,        // This is the upgrade tool (dump).
  kNewEviction = 1 << 4,        // Use of new eviction was specified.
  kNoRandom = 1 << 5,           // Don't add randomness to the behavior.
  kNoLoadProtection = 1 << 6,   // Don't act conservatively under load.
  kNoBuffering = 1 << 7         // Disable extended IO buffering.
};

// This class implements the Backend interface. An object of this
// class handles the operations of the cache for a particular profile.
class NET_EXPORT_PRIVATE BackendImpl : public Backend {
  friend class Eviction;
 public:
  BackendImpl(const base::FilePath& path,
              scoped_refptr<BackendCleanupTracker> cleanup_tracker,
              const scoped_refptr<base::SingleThreadTaskRunner>& cache_thread,
              net::CacheType cache_type,
              net::NetLog* net_log);

  // mask can be used to limit the usable size of the hash table, for testing.
  BackendImpl(const base::FilePath& path,
              uint32_t mask,
              const scoped_refptr<base::SingleThreadTaskRunner>& cache_thread,
              net::CacheType cache_type,
              net::NetLog* net_log);

  ~BackendImpl() override;

  // Performs general initialization for this current instance of the cache.
  net::Error Init(CompletionOnceCallback callback);

  // Performs the actual initialization and final cleanup on destruction.
  int SyncInit();
  void CleanupCache();

  // Synchronous implementation of the asynchronous interface.
  int SyncOpenEntry(const std::string& key, scoped_refptr<EntryImpl>* entry);
  int SyncCreateEntry(const std::string& key, scoped_refptr<EntryImpl>* entry);
  int SyncDoomEntry(const std::string& key);
  int SyncDoomAllEntries();
  int SyncDoomEntriesBetween(base::Time initial_time,
                             base::Time end_time);
  int SyncCalculateSizeOfAllEntries();
  int SyncDoomEntriesSince(base::Time initial_time);
  int SyncOpenNextEntry(Rankings::Iterator* iterator,
                        scoped_refptr<EntryImpl>* next_entry);
  void SyncEndEnumeration(std::unique_ptr<Rankings::Iterator> iterator);
  void SyncOnExternalCacheHit(const std::string& key);

  // Called at end of any backend operation on the background thread.
  void OnSyncBackendOpComplete();

  // Open or create an entry for the given |key| or |iter|.
  scoped_refptr<EntryImpl> OpenEntryImpl(const std::string& key);
  scoped_refptr<EntryImpl> CreateEntryImpl(const std::string& key);
  scoped_refptr<EntryImpl> OpenNextEntryImpl(Rankings::Iterator* iter);

  // Sets the maximum size for the total amount of data stored by this instance.
  bool SetMaxSize(int64_t max_bytes);

  // Returns the full name for an external storage file.
  base::FilePath GetFileName(Addr address) const;

  // Returns the actual file used to store a given (non-external) address.
  MappedFile* File(Addr address);

  // Returns a weak pointer to the background queue.
  base::WeakPtr<InFlightBackendIO> GetBackgroundQueue();

  // Creates an external storage file.
  bool CreateExternalFile(Addr* address);

  // Creates a new storage block of size block_count.
  bool CreateBlock(FileType block_type, int block_count,
                   Addr* block_address);

  // Deletes a given storage block. deep set to true can be used to zero-fill
  // the related storage in addition of releasing the related block.
  void DeleteBlock(Addr block_address, bool deep);

  // Retrieves a pointer to the LRU-related data.
  LruData* GetLruData();

  // Updates the ranking information for an entry.
  void UpdateRank(EntryImpl* entry, bool modified);

  // A node was recovered from a crash, it may not be on the index, so this
  // method checks it and takes the appropriate action.
  void RecoveredEntry(CacheRankingsBlock* rankings);

  // Permanently deletes an entry, but still keeps track of it.
  void InternalDoomEntry(EntryImpl* entry);

#if defined(NET_BUILD_STRESS_CACHE)
  // Returns the address of the entry linked to the entry at a given |address|.
  CacheAddr GetNextAddr(Addr address);

  // Verifies that |entry| is not currently reachable through the index.
  void NotLinked(EntryImpl* entry);
#endif

  // Removes all references to this entry.
  void RemoveEntry(EntryImpl* entry);

  // This method must be called when an entry is released for the last time, so
  // the entry should not be used anymore. |address| is the cache address of the
  // entry.
  void OnEntryDestroyBegin(Addr address);

  // This method must be called after all resources for an entry have been
  // released.
  void OnEntryDestroyEnd();

  // If the data stored by the provided |rankings| points to an open entry,
  // returns a pointer to that entry, otherwise returns NULL. Note that this
  // method does NOT increase the ref counter for the entry.
  EntryImpl* GetOpenEntry(CacheRankingsBlock* rankings) const;

  // Returns the id being used on this run of the cache.
  int32_t GetCurrentEntryId() const;

  // Returns the maximum size for a file to reside on the cache.
  int64_t MaxFileSize() const override;

  // A user data block is being created, extended or truncated.
  void ModifyStorageSize(int32_t old_size, int32_t new_size);

  // Logs requests that are denied due to being too big.
  void TooMuchStorageRequested(int32_t size);

  // Returns true if a temporary buffer is allowed to be extended.
  bool IsAllocAllowed(int current_size, int new_size);

  // Tracks the release of |size| bytes by an entry buffer.
  void BufferDeleted(int size);

  // Only intended for testing the two previous methods.
  int GetTotalBuffersSize() const {
    return buffer_bytes_;
  }

  // Returns true if this instance seems to be under heavy load.
  bool IsLoaded() const;

  // Returns the full histogram name, for the given base |name| and experiment,
  // and the current cache type. The name will be "DiskCache.t.name_e" where n
  // is the cache type and e the provided |experiment|.
  std::string HistogramName(const char* name, int experiment) const;

  bool read_only() const {
    return read_only_;
  }

  // Returns a weak pointer to this object.
  base::WeakPtr<BackendImpl> GetWeakPtr();

  // Returns true if we should send histograms for this user again. The caller
  // must call this function only once per run (because it returns always the
  // same thing on a given run).
  bool ShouldReportAgain();

  // Reports some data when we filled up the cache.
  void FirstEviction();

  // Reports a critical error (and disables the cache).
  void CriticalError(int error);

  // Reports an uncommon, recoverable error.
  void ReportError(int error);

  // Called when an interesting event should be logged (counted).
  void OnEvent(Stats::Counters an_event);

  // Keeps track of payload access (doesn't include metadata).
  void OnRead(int bytes);
  void OnWrite(int bytes);

  // Timer callback to calculate usage statistics.
  void OnStatsTimer();

  // Handles the pending asynchronous IO count.
  void IncrementIoCount();
  void DecrementIoCount();

  // Sets internal parameters to enable unit testing mode.
  void SetUnitTestMode();

  // Sets internal parameters to enable upgrade mode (for internal tools).
  void SetUpgradeMode();

  // Sets the eviction algorithm to version 2.
  void SetNewEviction();

  // Sets an explicit set of BackendFlags.
  void SetFlags(uint32_t flags);

  // Clears the counter of references to test handling of corruptions.
  void ClearRefCountForTest();

  // Sends a dummy operation through the operation queue, for unit tests.
  int FlushQueueForTest(CompletionOnceCallback callback);

  // Runs the provided task on the cache thread. The task will be automatically
  // deleted after it runs.
  int RunTaskForTest(base::OnceClosure task, CompletionOnceCallback callback);

  // Trims an entry (all if |empty| is true) from the list of deleted
  // entries. This method should be called directly on the cache thread.
  void TrimForTest(bool empty);

  // Trims an entry (all if |empty| is true) from the list of deleted
  // entries. This method should be called directly on the cache thread.
  void TrimDeletedListForTest(bool empty);

  // Only intended for testing
  base::RepeatingTimer* GetTimerForTest();

  // Performs a simple self-check, and returns the number of dirty items
  // or an error code (negative value).
  int SelfCheck();

  // Ensures the index is flushed to disk (a no-op on platforms with mmap).
  void FlushIndex();

  // Ensures that the private cache thread completes work.
  static void FlushForTesting();

  // Backend implementation.
  int32_t GetEntryCount() const override;
  EntryResult OpenOrCreateEntry(const std::string& key,
                                net::RequestPriority request_priority,
                                EntryResultCallback callback) override;
  EntryResult OpenEntry(const std::string& key,
                        net::RequestPriority request_priority,
                        EntryResultCallback callback) override;
  EntryResult CreateEntry(const std::string& key,
                          net::RequestPriority request_priority,
                          EntryResultCallback callback) override;
  net::Error DoomEntry(const std::string& key,
                       net::RequestPriority priority,
                       CompletionOnceCallback callback) override;
  net::Error DoomAllEntries(CompletionOnceCallback callback) override;
  net::Error DoomEntriesBetween(base::Time initial_time,
                                base::Time end_time,
                                CompletionOnceCallback callback) override;
  net::Error DoomEntriesSince(base::Time initial_time,
                              CompletionOnceCallback callback) override;
  int64_t CalculateSizeOfAllEntries(
      Int64CompletionOnceCallback callback) override;
  // NOTE: The blockfile Backend::Iterator::OpenNextEntry method does not modify
  // the last_used field of the entry, and therefore it does not impact the
  // eviction ranking of the entry. However, an enumeration will go through all
  // entries on the cache only if the cache is not modified while the
  // enumeration is taking place. Significantly altering the entry pointed by
  // the iterator (for example, deleting the entry) will invalidate the
  // iterator. Performing operations on an entry that modify the entry may
  // result in loops in the iteration, skipped entries or similar.
  std::unique_ptr<Iterator> CreateIterator() override;
  void GetStats(StatsItems* stats) override;
  void OnExternalCacheHit(const std::string& key) override;
  size_t DumpMemoryStats(
      base::trace_event::ProcessMemoryDump* pmd,
      const std::string& parent_absolute_name) const override;

 private:
  using EntriesMap = std::unordered_map<CacheAddr, EntryImpl*>;
  class IteratorImpl;

  // Creates a new backing file for the cache index.
  bool CreateBackingStore(disk_cache::File* file);
  bool InitBackingStore(bool* file_created);
  void AdjustMaxCacheSize(int table_len);

  bool InitStats();
  void StoreStats();

  // Deletes the cache and starts again.
  void RestartCache(bool failure);
  void PrepareForRestart();

  // Creates a new entry object. Returns zero on success, or a disk_cache error
  // on failure.
  int NewEntry(Addr address, scoped_refptr<EntryImpl>* entry);

  // Returns a given entry from the cache. The entry to match is determined by
  // key and hash, and the returned entry may be the matched one or it's parent
  // on the list of entries with the same hash (or bucket). To look for a parent
  // of a given entry, |entry_addr| should be grabbed from that entry, so that
  // if it doesn't match the entry on the index, we know that it was replaced
  // with a new entry; in this case |*match_error| will be set to true and the
  // return value will be NULL.
  scoped_refptr<EntryImpl> MatchEntry(const std::string& key,
                                      uint32_t hash,
                                      bool find_parent,
                                      Addr entry_addr,
                                      bool* match_error);

  // Opens the next or previous entry on a single list. If successful,
  // |from_entry| will be updated to point to the new entry, otherwise it will
  // be set to NULL; in other words, it is used as an explicit iterator.
  bool OpenFollowingEntryFromList(Rankings::List list,
                                  CacheRankingsBlock** from_entry,
                                  scoped_refptr<EntryImpl>* next_entry);

  // Returns the entry that is pointed by |next|, from the given |list|.
  scoped_refptr<EntryImpl> GetEnumeratedEntry(CacheRankingsBlock* next,
                                              Rankings::List list);

  // Re-opens an entry that was previously deleted.
  scoped_refptr<EntryImpl> ResurrectEntry(
      scoped_refptr<EntryImpl> deleted_entry);

  void DestroyInvalidEntry(EntryImpl* entry);

  // Handles the used storage count.
  void AddStorageSize(int32_t bytes);
  void SubstractStorageSize(int32_t bytes);

  // Update the number of referenced cache entries.
  void IncreaseNumRefs();
  void DecreaseNumRefs();
  void IncreaseNumEntries();
  void DecreaseNumEntries();

  // Dumps current cache statistics to the log.
  void LogStats();

  // Send UMA stats.
  void ReportStats();

  // Upgrades the index file to version 2.1.
  void UpgradeTo2_1();

  // Performs basic checks on the index file. Returns false on failure.
  bool CheckIndex();

  // Part of the self test. Returns the number or dirty entries, or an error.
  int CheckAllEntries();

  // Part of the self test. Returns false if the entry is corrupt.
  bool CheckEntry(EntryImpl* cache_entry);

  // Returns the maximum total memory for the memory buffers.
  int MaxBuffersSize();

  // We want this destroyed after every other field.
  scoped_refptr<BackendCleanupTracker> cleanup_tracker_;

  InFlightBackendIO background_queue_;  // The controller of pending operations.
  scoped_refptr<MappedFile> index_;  // The main cache index.
  base::FilePath path_;  // Path to the folder used as backing storage.
  Index* data_;  // Pointer to the index data.
  BlockFiles block_files_;  // Set of files used to store all data.
  Rankings rankings_;  // Rankings to be able to trim the cache.
  uint32_t mask_ = 0;  // Binary mask to map a hash to the hash table.
  int32_t max_size_ = 0;  // Maximum data size for this instance.
  Eviction eviction_;  // Handler of the eviction algorithm.
  EntriesMap open_entries_;  // Map of open entries.
  int num_refs_;  // Number of referenced cache entries.
  int max_refs_;  // Max number of referenced cache entries.
  int num_pending_io_;  // Number of pending IO operations.
  int entry_count_;  // Number of entries accessed lately.
  int byte_count_;  // Number of bytes read/written lately.
  int buffer_bytes_;  // Total size of the temporary entries' buffers.
  int up_ticks_ = 0;  // The number of timer ticks received (OnStatsTimer).
  int uma_report_ = 0;   // Controls transmission of UMA data.
  uint32_t user_flags_;  // Flags set by the user.
  bool init_ = false;    // controls the initialization of the system.
  bool restarted_ = false;
  bool unit_test_ = false;
  bool read_only_ =
      false;  // Prevents updates of the rankings data (used by tools).
  bool disabled_ = false;
  bool new_eviction_ = false;  // What eviction algorithm should be used.
  bool first_timer_ = true;    // True if the timer has not been called.
  bool user_load_ =
      false;  // True if we see a high load coming from the caller.

  // True if we should consider doing eviction at end of current operation.
  bool consider_evicting_at_op_end_ = false;

  net::NetLog* net_log_;

  Stats stats_;  // Usage statistics.
  std::unique_ptr<base::RepeatingTimer> timer_;  // Usage timer.
  base::WeakPtrFactory<BackendImpl> ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackendImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_BACKEND_IMPL_H_
