// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_BACKEND_SHARD_H_
#define NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_BACKEND_SHARD_H_

#include <atomic>
#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "net/base/cache_type.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "net/disk_cache/sql/sql_persistent_store_in_memory_index.h"
#include "net/disk_cache/sql/sql_read_cache_memory_monitor.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {
class IOBuffer;
}  // namespace net

namespace disk_cache {

class EvictionCandidateAggregator;

// SqlPersistentStoreBackendShard` manages a single shard of the cache,
// including its own `Backend` instance and in-memory index. It forwards
// operations to the `Backend` on a dedicated background task runner.
class SqlPersistentStore::BackendShard {
 public:
  BackendShard(
      ShardId shard_id,
      const base::FilePath& path,
      net::CacheType type,
      scoped_refptr<SqlReadCacheMemoryMonitor> read_cache_memory_monitor,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~BackendShard();

  // Kicks off the asynchronous initialization of the backend.
  void Initialize(int64_t user_max_bytes, InitResultOrErrorCallback callback);
  void OpenOrCreateEntry(const CacheEntryKey& key,
                         EntryInfoOrErrorCallback callback);
  void OpenEntry(const CacheEntryKey& key,
                 OptionalEntryInfoOrErrorCallback callback);
  void CreateEntry(const CacheEntryKey& key,
                   base::Time creation_time,
                   EntryInfoOrErrorCallback callback);
  void DoomEntry(const CacheEntryKey& key,
                 ResId res_id,
                 bool accept_index_mismatch,
                 ErrorCallback callback);
  void DeleteDoomedEntry(const CacheEntryKey& key,
                         ResId res_id,
                         ErrorCallback callback);
  void DeleteLiveEntry(const CacheEntryKey& key, ErrorCallback callback);
  void DeleteAllEntries(ErrorCallback callback);
  void DeleteLiveEntriesBetween(base::Time initial_time,
                                base::Time end_time,
                                base::flat_set<ResId> excluded_res_ids,
                                ErrorCallback callback);
  void UpdateEntryLastUsedByKey(const CacheEntryKey& key,
                                base::Time last_used,
                                ErrorCallback callback);
  void WriteEntryDataAndMetadata(
      const CacheEntryKey& key,
      std::optional<ResId> res_id,
      std::optional<int64_t> old_body_end,
      EntryWriteBuffer buffer,
      base::Time last_used,
      const std::optional<MemoryEntryDataHints>& new_hints,
      scoped_refptr<net::IOBuffer> head_buffer,
      int64_t header_size_delta,
      bool doomed_new_entry,
      ResIdOrErrorCallback callback);
  void WriteEntryData(const CacheEntryKey& key,
                      const ResIdOrTime& res_id_or_last_used_time,
                      int64_t old_body_end,
                      EntryWriteBuffer buffer,
                      bool truncate,
                      bool doomed_new_entry,
                      ResIdOrErrorCallback callback);
  void ReadEntryData(const CacheEntryKey& key,
                     ResId res_id,
                     int64_t offset,
                     scoped_refptr<net::IOBuffer> buffer,
                     int buf_len,
                     int64_t body_end,
                     bool sparse_reading,
                     SqlPersistentStore::ReadResultOrErrorCallback callback);

  void GetEntryAvailableRange(const CacheEntryKey& key,
                              ResId res_id,
                              int64_t offset,
                              int len,
                              RangeResultCallback callback);
  void CalculateSizeOfEntriesBetween(base::Time initial_time,
                                     base::Time end_time,
                                     Int64OrErrorCallback callback);
  void OpenNextEntry(const EntryIterator& iterator,
                     OptionalEntryInfoWithKeyAndIteratorCallback callback);
  void StartEviction(
      int64_t size_to_be_removed,
      base::flat_set<ResId> excluded_res_ids,
      bool is_idle_time_eviction,
      scoped_refptr<EvictionCandidateAggregator> aggregator,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
      scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
          remaining_mandatory_size,
      ResIdListOrErrorCallback callback);
  void ResumePendingEviction(
      base::flat_set<ResId> excluded_res_ids,
      bool is_idle_time_eviction,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
      scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
          remaining_mandatory_size,
      ResIdListOrErrorCallback callback);
  bool HasPendingEviction() const { return !pending_eviction_targets_.empty(); }

  int32_t GetEntryCount() const;
  void GetEntryCountAsync(Int32Callback callback) const;
  int64_t GetSizeOfAllEntries() const;

  IndexState GetIndexStateForHash(CacheEntryKey::Hash key_hash) const;

  // Updates the in-memory index with the given hints for the specified entry.
  void SetInMemoryEntryDataHints(ResId res_id, MemoryEntryDataHints hints);

  // Retrieves the hints for the specified entry from the in-memory index, if
  // available.
  std::optional<MemoryEntryDataHints> GetInMemoryEntryDataHints(
      CacheEntryKey::Hash key_hash) const;

  // Tries to find a single resource ID for the given key hash in the in-memory
  // index of this shard. Returns the resource ID if the index is available and
  // contains a unique entry for the hash.
  std::optional<ResId> TryGetSingleResIdFromInMemoryIndex(
      CacheEntryKey::Hash key_hash) const;

  void LoadInMemoryIndex(ErrorCallback callback);

  // If there are entries that were doomed in a previous session, this method
  // triggers a task to delete them from the database. The cleanup is performed
  // in the background. Returns true if a cleanup task was scheduled, and false
  // otherwise. `callback` is invoked upon completion of the cleanup task.
  bool MaybeRunCleanupDoomedEntries(ErrorCallback callback);

  void MaybeRunCheckpoint(base::OnceCallback<void(bool)> callback);

  void EnableStrictCorruptionCheckForTesting();
  void SetSimulateDbFailureForTesting(bool fail);
  void RazeAndPoisonForTesting();
  void SetEvictionHookForTesting(base::RepeatingClosure hook);

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(IndexMismatchLocation)
  enum class IndexMismatchLocation {
    kOpenOrCreateEntry = 0,
    kCreateEntry = 1,
    kDoomEntry = 2,
    kStartEviction = 3,
    kDeleteLiveEntry = 4,
    kDeleteLiveEntriesBetween = 5,
    kWriteEntryDataAndMetadata = 6,
    kWriteEntryData = 7,
    kMaxValue = kWriteEntryData,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:SqlDiskCacheIndexMismatchLocation)

  // Wraps a callback to ensure it is only run if the `BackendShard` is still
  // alive.
  template <typename ResultType>
  base::OnceCallback<void(ResultType)> WrapCallback(
      base::OnceCallback<void(ResultType)> callback) {
    return base::BindOnce(
        [](base::WeakPtr<BackendShard> weak_ptr,
           base::OnceCallback<void(ResultType)> callback, ResultType result) {
          if (weak_ptr) {
            // We should not run the callback when `this` was deleted.
            std::move(callback).Run(std::move(result));
          }
        },
        weak_factory_.GetWeakPtr(), std::move(callback));
  }

  // Like `WrapCallback`, but also updates the `store_status_`.
  base::OnceCallback<void(ErrorAndStoreStatus)> WrapCallbackWithStoreStatus(
      ErrorCallback callback);

  base::OnceCallback<void(ResIdOrErrorAndStoreStatus)>
  WrapCallbackWithStoreStatusAndIndexUpdate(
      ResIdOrErrorCallback callback,
      const CacheEntryKey& key,
      bool is_new_entry,
      const std::optional<MemoryEntryDataHints>& new_hints,
      IndexMismatchLocation location);

  base::OnceCallback<void(EntryInfoOrErrorAndStoreStatus)>
  WrapEntryInfoOrErrorCallback(EntryInfoOrErrorCallback callback,
                               const CacheEntryKey& key,
                               IndexMismatchLocation location);

  base::OnceCallback<void(ResIdListOrErrorAndStoreStatus)>
  WrapErrorCallbackToRemoveFromIndex(ErrorCallback callback,
                                     IndexMismatchLocation location);
  void OnEvictionFinished(ResIdListOrErrorCallback callback,
                          EvictionResultOrErrorAndStoreStatus result);
  void RecordIndexMismatch(IndexMismatchLocation location);

  base::SequenceBound<Backend> backend_;

  // The in-memory summary of the store's status.
  StoreStatus store_status_;

  // The in-memory index of cache entries. This is loaded asynchronously after
  // MaybeLoadInMemoryIndex() is called.
  std::optional<SqlPersistentStoreInMemoryIndex> index_;

  // A list of resource IDs for entries that were doomed in a previous session
  // and are scheduled for deletion.
  ResIdList to_be_deleted_res_ids_;

  // True while the in-memory index is being loaded from the database.
  bool loading_index_ = false;

  // A list of resource IDs of entries that are doomed during the in-memory
  // index is being loaded. Once loading is complete, these entries are removed
  // from the newly loaded index to ensure consistency.
  ResIdList pending_doomed_res_ids_;

  bool strict_corruption_check_enabled_ = false;

  // The list of eviction candidates that were selected but not yet evicted
  // because the eviction was paused.
  SqlPersistentStore::EvictionTargetQueue pending_eviction_targets_;

  base::WeakPtrFactory<BackendShard> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_BACKEND_SHARD_H_
