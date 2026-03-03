// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_BACKEND_H_
#define NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_BACKEND_H_

#include <atomic>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/disk_cache/sql/entry_write_buffer.h"
#include "net/disk_cache/sql/eviction_candidate_aggregator.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "sql/database.h"
#include "sql/meta_table.h"

namespace sql {
class Statement;
class Transaction;
}  // namespace sql

namespace disk_cache {

class SqlReadCacheMemoryMonitor;

// The `Backend` class encapsulates all direct interaction with the SQLite
// database. It is designed to be owned by a `base::SequenceBound` and run on a
// dedicated background sequence to avoid blocking the network IO thread.
class SqlPersistentStore::Backend {
 public:
  Backend(ShardId shard_id,
          const base::FilePath& path,
          net::CacheType type,
          scoped_refptr<SqlReadCacheMemoryMonitor> read_cache_memory_monitor);

  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;
  ~Backend();

  // Initializes the database, including setting up the schema and reading
  // metadata. Returns the initialization result on success.
  InitResultOrError Initialize(int64_t user_max_bytes,
                               base::TimeTicks start_time);

  int32_t GetEntryCount() const;

  EntryInfoOrErrorAndStoreStatus OpenOrCreateEntry(const CacheEntryKey& key,
                                                   base::TimeTicks start_time);
  OptionalEntryInfoOrError OpenEntry(const CacheEntryKey& key,
                                     base::TimeTicks start_time);
  EntryInfoOrErrorAndStoreStatus CreateEntry(const CacheEntryKey& key,
                                             base::Time creation_time,
                                             bool run_existance_check,
                                             base::TimeTicks start_time);

  ErrorAndStoreStatus DoomEntry(const CacheEntryKey& key,
                                ResId res_id,
                                base::TimeTicks start_time);
  ErrorAndStoreStatus DeleteDoomedEntry(const CacheEntryKey& key,
                                        ResId res_id,
                                        base::TimeTicks start_time);
  Error DeleteDoomedEntries(ResIdList res_ids_to_delete,
                            base::TimeTicks start_time);
  ResIdListOrErrorAndStoreStatus DeleteLiveEntry(const CacheEntryKey& key,
                                                 base::TimeTicks start_time);

  ErrorAndStoreStatus DeleteAllEntries(base::TimeTicks start_time);
  ResIdListOrErrorAndStoreStatus DeleteLiveEntriesBetween(
      base::Time initial_time,
      base::Time end_time,
      base::flat_set<ResId> excluded_res_ids,
      base::TimeTicks start_time);
  Error UpdateEntryLastUsedByKey(const CacheEntryKey& key,
                                 base::Time last_used,
                                 base::TimeTicks start_time);
  ResIdOrErrorAndStoreStatus WriteEntryDataAndMetadata(
      const CacheEntryKey& key,
      std::optional<ResId> res_id,
      std::optional<int64_t> old_body_end,
      EntryWriteBuffer buffer,
      base::Time last_used,
      const std::optional<MemoryEntryDataHints>& new_hints,
      scoped_refptr<net::IOBuffer> head_buffer,
      int64_t header_size_delta,
      bool doomed_new_entry,
      base::TimeTicks start_time);
  ResIdOrErrorAndStoreStatus WriteEntryData(
      const CacheEntryKey& key,
      const ResIdOrTime& res_id_or_last_used_time,
      int64_t old_body_end,
      EntryWriteBuffer buffer,
      bool truncate,
      bool doomed_new_entry,
      base::TimeTicks start_time);
  ReadResultOrError ReadEntryData(const CacheEntryKey& key,
                                  ResId res_id,
                                  int64_t offset,
                                  scoped_refptr<net::IOBuffer> buffer,
                                  int buf_len,
                                  int64_t body_end,
                                  bool sparse_reading,
                                  base::TimeTicks start_time);
  RangeResult GetEntryAvailableRange(ResId res_id,
                                     int64_t offset,
                                     int len,
                                     base::TimeTicks start_time);
  Int64OrError CalculateSizeOfEntriesBetween(base::Time initial_time,
                                             base::Time end_time,
                                             base::TimeTicks start_time);
  OptionalEntryInfoWithKeyAndIterator OpenNextEntry(
      const EntryIterator& iterator,
      base::TimeTicks start_time);

  // Starts the eviction process.
  //
  // The process begins by selecting eviction candidates from the database.
  // Then, `aggregator->OnCandidate()` is called to aggregate candidates from
  // all shards. Finally, `EvictEntries()` is called to delete the selected
  // entries.
  //
  // `size_to_be_removed`: The target size to be removed from this shard. This
  //                       is used to select candidates.
  // `excluded_res_ids`: A set of resource IDs to exclude from eviction (e.g.,
  //                     currently active entries).
  // `high_priority_res_ids`: A list of resource IDs that should be prioritized
  //                          for staying in the cache.
  // `is_idle_time_eviction`: True if this is an eviction triggered by idle
  //                          time. If true, the eviction may be aborted if the
  //                          browser becomes active.
  // `aggregator`: The aggregator used to collect and select candidates across
  //               all shards.
  // `abort_flag`: A flag used to signal an abort request. If set to true, the
  //               eviction process will stop after the mandatory size has been
  //               removed.
  // `remaining_mandatory_size`: The remaining size that *must* be evicted even
  //                             if `abort_flag` is set. This is typically the
  //                             amount needed to bring the cache size below the
  //                             high watermark. Once this value becomes <= 0,
  //                             and `abort_flag` is set, the eviction will
  //                             stop.
  // `callback`: Called when the eviction finishes or is aborted.
  void StartEviction(
      int64_t size_to_be_removed,
      base::flat_set<ResId> excluded_res_ids,
      std::vector<ResId> high_priority_res_ids,
      bool is_idle_time_eviction,
      scoped_refptr<EvictionCandidateAggregator> aggregator,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
      scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
          remaining_mandatory_size,
      EvictionResultOrErrorAndStoreStatusCallback callback);

  // Resumes a previously paused eviction.
  //
  // This method continues evicting entries from `eviction_targets` that were
  // left over from a previous `StartEviction` or `ResumePendingEviction` call
  // that was aborted.
  //
  // `eviction_targets`: The queue of eviction targets remaining from the
  //                     previous attempt.
  // `excluded_res_ids`: A set of resource IDs to exclude from eviction.
  // `is_idle_time_eviction`: See `StartEviction`.
  // `abort_flag`: See `StartEviction`.
  // `remaining_mandatory_size`: See `StartEviction`.
  // `start_time`: The time when the resume operation was posted.
  EvictionResultOrErrorAndStoreStatus ResumePendingEviction(
      EvictionTargetQueue eviction_targets,
      base::flat_set<ResId> excluded_res_ids,
      bool is_idle_time_eviction,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
      scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
          remaining_mandatory_size,
      base::TimeTicks start_time);

  InMemoryIndexAndDoomedResIdsOrError LoadInMemoryIndex();
  bool MaybeRunCheckpoint();

  void EnableStrictCorruptionCheckForTesting() {
    strict_corruption_check_enabled_ = true;
  }

  void SetSimulateDbFailureForTesting(bool fail) {
    simulate_db_failure_for_testing_ = fail;
  }

  void RazeAndPoisonForTesting() {
    db_.RazeAndPoison();
    store_status_ = StoreStatus();
  }

  void SetEvictionHookForTesting(base::RepeatingClosure hook) {
    eviction_hook_ = std::move(hook);
  }

 private:
  using RangeResultOrError = base::expected<RangeResult, Error>;
  using OptionalEntryInfoWithKeyAndIteratorOrError =
      base::expected<OptionalEntryInfoWithKeyAndIterator, Error>;

  using EvictionCandidateList =
      EvictionCandidateAggregator::EvictionCandidateList;
  using EvictionTargetQueue = SqlPersistentStore::EvictionTargetQueue;

  // A helper struct to associate an IOBuffer with a starting offset.
  struct BufferWithStart {
    BufferWithStart(scoped_refptr<net::IOBuffer> buffer, int64_t start);
    ~BufferWithStart();
    BufferWithStart(BufferWithStart&& other);
    BufferWithStart& operator=(BufferWithStart&& other);

    scoped_refptr<net::IOBuffer> buffer;
    int64_t start;
  };

  void DatabaseErrorCallback(int error, sql::Statement* statement);

  Error InitializeInternal(bool& corruption_detected);
  EntryInfoOrError OpenOrCreateEntryInternal(const CacheEntryKey& key,
                                             bool& corruption_detected);
  OptionalEntryInfoOrError OpenEntryInternal(const CacheEntryKey& key);
  EntryInfoOrError CreateEntryInternal(const CacheEntryKey& key,
                                       base::Time creation_time,
                                       bool run_existance_check,
                                       bool& corruption_detected);
  Error DoomEntryInternal(ResId res_id, bool& corruption_detected);
  Error DeleteDoomedEntryInternal(ResId res_id);
  Error DeleteDoomedEntriesInternal(const ResIdList& res_ids_to_delete,
                                    bool& corruption_detected);
  ResIdListOrError DeleteLiveEntryInternal(const CacheEntryKey& key,
                                           bool& corruption_detected);
  Error DeleteAllEntriesInternal(bool& corruption_detected);
  ResIdListOrError DeleteLiveEntriesBetweenInternal(
      base::Time initial_time,
      base::Time end_time,
      const base::flat_set<ResId>& excluded_res_ids,
      bool& corruption_detected);
  Error UpdateEntryLastUsedByKeyInternal(const CacheEntryKey& key,
                                         base::Time last_used);
  Error WriteEntryBodyDataHelper(
      const CacheEntryKey& key,
      ResId res_id,
      int64_t old_body_end,
      EntryWriteBuffer buffer,
      bool truncate,
      int64_t& body_end_delta,
      base::CheckedNumeric<int64_t>& checked_total_size_delta,
      int64_t& new_body_end,
      bool& corruption_detected);
  ResIdOrError WriteEntryDataAndMetadataInternal(
      const CacheEntryKey& key,
      std::optional<ResId> res_id,
      std::optional<int64_t> old_body_end,
      EntryWriteBuffer buffer,
      base::Time last_used,
      const std::optional<MemoryEntryDataHints>& new_hints,
      scoped_refptr<net::IOBuffer> head_buffer,
      int64_t header_size_delta,
      bool doomed_new_entry,
      bool& corruption_detected);
  ResIdOrError WriteEntryDataInternal(
      const CacheEntryKey& key,
      const ResIdOrTime& res_id_or_last_used_time,
      int64_t old_body_end,
      EntryWriteBuffer buffer,
      bool truncate,
      bool doomed_new_entry,
      bool& corruption_detected);
  ReadResultOrError ReadEntryDataInternal(const CacheEntryKey& key,
                                          ResId res_id,
                                          int64_t offset,
                                          scoped_refptr<net::IOBuffer> buffer,
                                          int buf_len,
                                          int64_t body_end,
                                          bool sparse_reading,
                                          bool& corruption_detected);
  RangeResultOrError GetEntryAvailableRangeInternal(ResId res_id,
                                                    int64_t offset,
                                                    int len);
  Int64OrError CalculateSizeOfEntriesBetweenInternal(base::Time initial_time,
                                                     base::Time end_time);
  OptionalEntryInfoWithKeyAndIteratorOrError OpenNextEntryInternal(
      const EntryIterator& iterator,
      bool& corruption_detected);
  InMemoryIndexAndDoomedResIdsOrError LoadInMemoryIndexInternal();

  // Trims blobs that overlap with the new write range [offset, end), and
  // updates the total size delta.
  Error TrimOverlappingBlobs(
      const CacheEntryKey& key,
      ResId res_id,
      int64_t offset,
      int64_t end,
      bool truncate,
      base::CheckedNumeric<int64_t>& checked_total_size_delta,
      bool& corruption_detected);
  // Truncates data by deleting all blobs that start at or after the given
  // offset.
  Error TruncateBlobsAfter(
      ResId res_id,
      int64_t truncate_offset,
      base::CheckedNumeric<int64_t>& checked_total_size_delta);
  // Inserts a vector of new blobs into the database, and updates the total size
  // delta.
  Error InsertNewBlobs(const CacheEntryKey& key,
                       ResId res_id,
                       const std::vector<BufferWithStart>& new_blobs,
                       base::CheckedNumeric<int64_t>& checked_total_size_delta);
  // Inserts a single new blob into the database, and updates the total size
  // delta.
  Error InsertNewBlob(const CacheEntryKey& key,
                      ResId res_id,
                      int64_t start,
                      const scoped_refptr<net::IOBuffer>& buffer,
                      int buf_len,
                      base::CheckedNumeric<int64_t>& checked_total_size_delta);
  // Deletes blobs by their IDs, and updates the total size delta.
  Error DeleteBlobsById(const std::vector<int64_t>& blob_ids_to_be_removed,
                        base::CheckedNumeric<int64_t>& checked_total_size_delta,
                        bool& corruption_detected);
  // Deletes a single blob by its ID, and updates the total size delta.
  Error DeleteBlobById(int64_t blob_id,
                       base::CheckedNumeric<int64_t>& checked_total_size_delta,
                       bool& corruption_detected);
  // Deletes all blobs associated with a given res_id.
  Error DeleteBlobsByResId(ResId res_id);
  // Deletes all blobs associated with a list of entry res_ids.
  Error DeleteBlobsByResIds(const std::vector<ResId>& res_ids);
  // Deletes a single resource entry from the `resources` table by its `res_id`.
  Error DeleteResourceByResId(ResId res_id);
  // Deletes a single live resource entry from the `resources` table by its
  // `res_id` and returns the `bytes_usage` of the deleted entry.
  Int64OrError DeleteLiveResourceByResIdReturnUsage(ResId res_id);
  // Deletes multiple resource entries from the `resources` table by their
  // `res_id`s.
  Error DeleteResourcesByResIds(const std::vector<ResId>& res_ids);

  // Selects a list of eviction candidates from the `resources` table.
  // Entries in `high_priority_res_ids` are less likely to be selected as
  // candidates if prioritized caching is enabled.
  EvictionCandidateList SelectEvictionCandidates(
      int64_t size_to_be_removed,
      base::flat_set<ResId> excluded_res_ids,
      std::vector<ResId> high_priority_res_ids,
      bool is_idle_time_eviction);
  // Called by the `EvictionCandidateAggregator` to evict a list of selected
  // entries.
  void EvictEntries(
      EvictionResultOrErrorAndStoreStatusCallback callback,
      bool is_idle_time_eviction,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
      scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
          remaining_mandatory_size,
      EvictionTargetQueue eviction_targets,
      base::TimeTicks post_task_time);

  // A helper function to evict entries.
  // `trust_target_size`: If true, it assumes the entry exists and uses the size
  // from `eviction_targets` (used for new eviction). If false, entries that are
  // not found in the DB are ignored, and the size is retrieved from the DB
  // (used for resuming eviction).
  EvictionResultOrError EvictEntriesHelper(
      EvictionTargetQueue eviction_targets,
      const base::flat_set<ResId>& excluded_res_ids,
      bool is_idle_time_eviction,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
      scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
          remaining_mandatory_size,
      bool trust_target_size,
      bool& corruption_detected);

  // Updates the in-memory `store_status_` by `entry_count_delta` and
  // `total_size_delta`. If the update results in an overflow or a negative
  // value, it recalculates the correct value from the database to recover from
  // potential metadata corruption.
  // It then updates the meta table values and attempts to commit the
  // `transaction`.
  // Returns Error::kOk on success, or an error code on failure.
  Error UpdateStoreStatusAndCommitTransaction(sql::Transaction& transaction,
                                              int64_t entry_count_delta,
                                              int64_t total_size_delta,
                                              bool& corruption_detected);

  // Recalculates the store's status (entry count and total size) directly from
  // the database. This is a recovery mechanism used when metadata might be
  // inconsistent, e.g., after a numerical overflow.
  // Returns Error::kOk on success, or an error code on failure.
  Error RecalculateStoreStatusAndCommitTransaction(
      sql::Transaction& transaction);

  int64_t CalculateResourceEntryCount();
  int64_t CalculateTotalSize();

  // Checks the database status. Returns Error::kOk on success, or an error
  // code if something is wrong.
  Error CheckDatabaseStatus();

  void MaybeCrashIfCorrupted(bool corruption_detected);
  void OnCommitCallback(int pages);

  base::FilePath GetDatabaseFilePath() const;

  const ShardId shard_id_;
  const base::FilePath path_;
  const net::CacheType type_;
  const scoped_refptr<SqlReadCacheMemoryMonitor> read_cache_memory_monitor_;
  sql::Database db_;
  sql::MetaTable meta_table_;
  std::optional<Error> db_init_status_;
  StoreStatus store_status_;
  bool strict_corruption_check_enabled_ = false;
  bool simulate_db_failure_for_testing_ = false;
  // The number of pages in the write-ahead log file. This is updated by
  // `OnCommitCallback` and reset to 0 after a checkpoint.
  int wal_pages_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  // A hook called during eviction for testing purposes.
  base::RepeatingClosure eviction_hook_;

  base::WeakPtrFactory<Backend> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_BACKEND_H_
