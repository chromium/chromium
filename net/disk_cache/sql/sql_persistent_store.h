// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_H_
#define NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_H_

#include <atomic>
#include <optional>
#include <set>
#include <variant>

#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/strong_alias.h"
#include "net/base/cache_type.h"
#include "net/base/net_export.h"
#include "net/disk_cache/buildflags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/sql/cache_entry_key.h"
#include "net/disk_cache/sql/entry_write_buffer.h"
#include "net/disk_cache/sql/sql_backend_aliases.h"
#include "net/disk_cache/sql/sql_persistent_store_in_memory_index.h"

// This backend is experimental and only available when the build flag is set.
static_assert(BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND));

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {
class GrowableIOBuffer;
class IOBuffer;
}  // namespace net

namespace disk_cache {

// This class serves as the main entry point for the SQL-based disk cache's
// persistence layer. It manages multiple database shards to improve
// concurrency, distributing operations across them based on cache entry keys.
// All actual database I/O is performed asynchronously on background task
// runners, with each shard handling its own database file and operations.
class NET_EXPORT_PRIVATE SqlPersistentStore {
 public:
  class BackendShard;
  class Backend;

  // The primary key for resources managed in the SqlPersistentStore's resources
  // table.
  using ResId = SqlPersistentStoreResId;

  // A unique identifier for a database shard.
  using ShardId = SqlPersistentStoreShardId;

  // Represents the error of SqlPersistentStore operation.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(SqlDiskCacheStoreError)
  enum class Error {
    kOk = 0,
    kFailedToCreateDirectory = 1,
    kFailedToOpenDatabase = 2,
    kFailedToRazeIncompatibleDatabase = 3,
    kFailedToStartTransaction = 4,
    kFailedToCommitTransaction = 5,
    kFailedToInitializeMetaTable = 6,
    kFailedToInitializeSchema = 7,
    kFailedToSetEntryCountMetadata = 8,
    kFailedToSetTotalSizeMetadata = 9,
    kFailedToExecute = 10,
    kInvalidData = 11,
    kAlreadyExists = 12,
    kNotFound = 13,
    kInvalidArgument = 14,
    kBodyEndMismatch = 15,
    kFailedForTesting = 16,
    kAborted = 17,
    kNotInitialized = 18,
    kCheckSumError = 19,
    kDatabaseClosed = 20,
    kAbortedDueToBrowserActivity = 21,
    kMaxValue = kAbortedDueToBrowserActivity
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:SqlDiskCacheStoreError)

  // Represents the urgency of cache eviction.
  enum class EvictionUrgency {
    kNotNeeded,
    kIdleTime,
    kNeeded,
  };

  // Holds information about a specific cache entry.
  struct NET_EXPORT_PRIVATE EntryInfo {
    EntryInfo();
    ~EntryInfo();
    EntryInfo(EntryInfo&&);
    EntryInfo& operator=(EntryInfo&&);

    // A unique identifier for this entry instance, used for safe data access.
    ResId res_id;
    // The last time this entry was used.
    base::Time last_used;
    // The total size of the entry's body (all data streams).
    int64_t body_end = 0;
    // The entry's header data (stream 0).
    scoped_refptr<net::GrowableIOBuffer> head;
    // True if the entry was opened, false if it was newly created.
    bool opened = false;
  };

  // Represents the result of a read operation.
  struct NET_EXPORT_PRIVATE ReadResult {
    ReadResult();
    ~ReadResult();
    ReadResult(const ReadResult&);
    ReadResult& operator=(const ReadResult&);
    ReadResult(ReadResult&&);
    ReadResult& operator=(ReadResult&&);

    // The number of bytes successfully read.
    int read_bytes = 0;
    // Optionally, a buffer containing data read beyond the requested range.
    scoped_refptr<net::IOBuffer> cache_buffer;
    // The offset within the entry's body where `cache_buffer` starts.
    int64_t cache_buffer_offset = 0;
  };

  // Holds a resource ID and the ID of the shard it belongs to.
  struct NET_EXPORT_PRIVATE ResIdAndShardId {
    ResIdAndShardId(ResId res_id, ShardId shard_id);
    ResIdAndShardId();
    ~ResIdAndShardId();
    ResIdAndShardId(const ResIdAndShardId&);
    ResIdAndShardId& operator=(const ResIdAndShardId&);
    ResIdAndShardId(ResIdAndShardId&&);
    ResIdAndShardId& operator=(ResIdAndShardId&&);

    // Initialized to the maximum value for convenience when iterating through
    // entries.
    ResId res_id = ResId(std::numeric_limits<int64_t>::max());
    ShardId shard_id = ShardId(0);
  };

  // Holds the position of an entry, used for iterating through entries.
  using EntryIterator =
      base::StrongAlias<class EntryIteratorTag, ResIdAndShardId>;

  // Holds information about a specific cache entry, including its `key` and
  // an `iterator` which is used when iterating through entries.
  struct NET_EXPORT_PRIVATE EntryInfoWithKeyAndIterator {
    EntryInfoWithKeyAndIterator();
    ~EntryInfoWithKeyAndIterator();
    EntryInfoWithKeyAndIterator(EntryInfoWithKeyAndIterator&&);
    EntryInfoWithKeyAndIterator& operator=(EntryInfoWithKeyAndIterator&&);

    EntryInfo info;
    CacheEntryKey key;
    EntryIterator iterator;
  };

  // Holds summary statistics about the cache store.
  class StoreStatus {
   public:
    StoreStatus() = default;
    ~StoreStatus() = default;
    StoreStatus(const StoreStatus& other) = default;
    StoreStatus& operator=(const StoreStatus& other) = default;
    StoreStatus(StoreStatus&& other) = default;
    StoreStatus& operator=(StoreStatus&& other) = default;

    int64_t GetEstimatedDiskUsage() const;

    int32_t entry_count = 0;
    int64_t total_size = 0;
  };

  // A struct to hold the in-memory index and the list of doomed resource IDs.
  // This is used to return both from the backend task that loads them.
  struct InMemoryIndexAndDoomedResIds {
    InMemoryIndexAndDoomedResIds(
        SqlPersistentStoreInMemoryIndex&& index,
        std::vector<SqlPersistentStore::ResId> doomed_entry_res_ids);
    ~InMemoryIndexAndDoomedResIds();
    InMemoryIndexAndDoomedResIds(InMemoryIndexAndDoomedResIds&& other);
    InMemoryIndexAndDoomedResIds& operator=(
        InMemoryIndexAndDoomedResIds&& other);

    SqlPersistentStoreInMemoryIndex index;
    std::vector<SqlPersistentStore::ResId> doomed_entry_res_ids;
  };

  struct NET_EXPORT_PRIVATE EvictionTarget {
    EvictionTarget(SqlPersistentStore::ResId res_id,
                   int64_t entry_size_with_overhead);
    ~EvictionTarget();
    EvictionTarget(EvictionTarget&&);
    EvictionTarget& operator=(EvictionTarget&&);
    EvictionTarget(const EvictionTarget&);
    EvictionTarget& operator=(const EvictionTarget&);

    bool operator==(const EvictionTarget& other) const;

    SqlPersistentStore::ResId res_id;
    int64_t entry_size_with_overhead;
  };

  using EvictionTargetQueue = base::queue<EvictionTarget>;

  // The result of an eviction operation.
  struct EvictionResult {
    EvictionResult(std::vector<ResId> deleted_res_ids,
                   EvictionTargetQueue pending_eviction_targets);
    ~EvictionResult();
    EvictionResult(EvictionResult&& other);
    EvictionResult& operator=(EvictionResult&& other);

    std::vector<ResId> deleted_res_ids;
    EvictionTargetQueue pending_eviction_targets;
  };

  // A helper struct to bundle an operation's result with a flag indicating
  // whether an eviction check is needed. This allows the background sequence,
  // which has direct access to cache size information, to notify the main
  // sequence that an eviction might be necessary without requiring an extra
  // cross-sequence call to check the cache size.
  template <typename ResultType>
  struct ResultAndStoreStatus {
    ResultAndStoreStatus(ResultType result, const StoreStatus& store_status)
        : result(std::move(result)), store_status(store_status) {}
    ~ResultAndStoreStatus() = default;
    ResultAndStoreStatus(ResultAndStoreStatus&&) = default;

    // The actual result of the operation.
    ResultType result;

    // The store status.
    StoreStatus store_status;
  };

  using ErrorCallback = base::OnceCallback<void(Error)>;
  using Int32Callback = base::OnceCallback<void(int32_t)>;
  using Int64Callback = base::OnceCallback<void(int64_t)>;
  using EntryInfoOrError = base::expected<EntryInfo, Error>;
  using EntryInfoOrErrorCallback = base::OnceCallback<void(EntryInfoOrError)>;
  using OptionalEntryInfoOrError =
      base::expected<std::optional<EntryInfo>, Error>;
  using OptionalEntryInfoOrErrorCallback =
      base::OnceCallback<void(OptionalEntryInfoOrError)>;
  using OptionalEntryInfoWithKeyAndIterator =
      std::optional<EntryInfoWithKeyAndIterator>;
  using OptionalEntryInfoWithKeyAndIteratorCallback =
      base::OnceCallback<void(OptionalEntryInfoWithKeyAndIterator)>;
  using ReadResultOrError = base::expected<ReadResult, Error>;
  using ReadResultOrErrorCallback = base::OnceCallback<void(ReadResultOrError)>;
  using Int64OrError = base::expected<int64_t, Error>;
  using Int64OrErrorCallback = base::OnceCallback<void(Int64OrError)>;
  using ResIdOrTime = std::variant<ResId, base::Time>;

  using ResIdList = std::vector<ResId>;
  using ResIdListOrError = base::expected<ResIdList, Error>;
  using ResIdListOrErrorCallback = base::OnceCallback<void(ResIdListOrError)>;

  using ErrorAndStoreStatus = ResultAndStoreStatus<Error>;
  using EntryInfoOrErrorAndStoreStatus = ResultAndStoreStatus<EntryInfoOrError>;
  using ReadResultOrErrorAndStoreStatus =
      ResultAndStoreStatus<ReadResultOrError>;
  using ResIdOrError = base::expected<ResId, Error>;
  using ResIdOrErrorCallback = base::OnceCallback<void(ResIdOrError)>;
  using ResIdOrErrorAndStoreStatus = ResultAndStoreStatus<ResIdOrError>;
  using ResIdListOrErrorAndStoreStatus = ResultAndStoreStatus<ResIdListOrError>;
  using EvictionResultOrError = base::expected<EvictionResult, Error>;
  using EvictionResultOrErrorAndStoreStatus =
      ResultAndStoreStatus<EvictionResultOrError>;
  using EvictionResultOrErrorAndStoreStatusCallback =
      base::OnceCallback<void(EvictionResultOrErrorAndStoreStatus)>;
  using InMemoryIndexAndDoomedResIdsOrError =
      base::expected<InMemoryIndexAndDoomedResIds, Error>;

  // Creates a new instance of the persistent store. The returned object must be
  // initialized by calling `Initialize()`.
  SqlPersistentStore(const base::FilePath& path,
                     int64_t max_bytes,
                     net::CacheType type,
                     std::vector<scoped_refptr<base::SequencedTaskRunner>>
                         background_task_runners);
  ~SqlPersistentStore();

  SqlPersistentStore(const SqlPersistentStore&) = delete;
  SqlPersistentStore& operator=(const SqlPersistentStore&) = delete;

  // Initializes the store. `callback` will be invoked upon completion.
  void Initialize(ErrorCallback callback);

  // Opens an entry with the given `key`. If the entry does not exist, it is
  // created. `callback` is invoked with the entry information on success or
  // an error code on failure.
  void OpenOrCreateEntry(const CacheEntryKey& key,
                         EntryInfoOrErrorCallback callback);

  // Opens an existing entry with the given `key`.
  // The `callback` is invoked with the entry's information on success. If the
  // entry does not exist, the `callback` is invoked with `std::nullopt`.
  void OpenEntry(const CacheEntryKey& key,
                 OptionalEntryInfoOrErrorCallback callback);

  // Creates a new entry with the given `key`. `creation_time` is the time the
  // entry is created and will be used as the initial `last_used` time.
  // The `callback` is invoked with the new entry's information on success. If
  // an entry with this key already exists, the callback is invoked with a
  // `kAlreadyExists` error.
  void CreateEntry(const CacheEntryKey& key,
                   base::Time creation_time,
                   EntryInfoOrErrorCallback callback);

  // Marks an entry for future deletion. When an entry is "doomed", it is
  // immediately removed from the cache's entry count and total size, but its
  // data remains on disk until `DeleteDoomedEntry()` is called. The `res_id`
  // ensures that only the correct instance of an entry is doomed.
  // `accept_index_mismatch` should be set to true if the entry might have
  // already been removed from the in-memory index by a concurrent operation
  // (e.g., a previous Doom call).
  void DoomEntry(const CacheEntryKey& key,
                 ResId res_id,
                 bool accept_index_mismatch,
                 ErrorCallback callback);

  // Physically deletes an entry that has been previously marked as doomed. This
  // operation completes the deletion process by removing the entry's data from
  // the database. The `res_id` ensures that only a specific, doomed instance of
  // the entry is deleted.
  void DeleteDoomedEntry(const CacheEntryKey& key,
                         ResId res_id,
                         ErrorCallback callback);

  // Deletes a "live" entry, i.e., an entry whose `doomed` flag is not set.
  // This is for use for entries which are not open; open entries should have
  // `DoomEntry()` called, and then `DeleteDoomedEntry()` once they're no longer
  // in used.
  void DeleteLiveEntry(const CacheEntryKey& key, ErrorCallback callback);

  // Deletes all entries from the cache. `callback` is invoked on completion.
  void DeleteAllEntries(ErrorCallback callback);

  // Deletes all "live" (not doomed) entries whose `last_used` time falls
  // within the range [`initial_time`, `end_time`), excluding any entries whose
  // IDs are present in `excluded_list`. `callback` is invoked on completion.
  void DeleteLiveEntriesBetween(base::Time initial_time,
                                base::Time end_time,
                                std::vector<ResIdAndShardId> excluded_list,
                                ErrorCallback callback);

  // Updates the `last_used` timestamp for the entry with the specified `key`.
  // `callback` is invoked with `kOk` on success, or `kNotFound` if the entry
  // does not exist or is already doomed.
  void UpdateEntryLastUsedByKey(const CacheEntryKey& key,
                                base::Time last_used,
                                ErrorCallback callback);

  // Writes data and updates metadata (header and last_used) for an entry in a
  // single operation.
  // `key` and `res_id` identify the target entry. If `res_id` is std::nullopt,
  // a new entry is created.
  // `old_body_end`: If provided, indicates that body data should be updated.
  //                 It represents the expected current size of the body.
  // `buffer`: contains the body data and offset to write.
  // `last_used`: The new last used time. If a new entry is created, this is
  //              used as the creation time.
  // `new_hints`: Optional new hints to set.
  // `head_buffer`: Optional new header data.
  // `header_size_delta`: The change in header size.
  // `doomed_new_entry`: If true, the entry is marked as doomed (deleted) upon
  //                     creation. This parameter is only used when creating a
  //                     new entry.
  // `callback`: Invoked with the result of the operation. Returns the resource
  //             ID on success, or an error code on failure.
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

  // Writes data to an entry's body. This can be used to write new data,
  // overwrite existing data, or append to the entry.
  // `key`: Identifies the target entry.
  // `res_id_or_last_used_time`: Identifies the target entry. If it holds
  //                             `base::Time`, a new entry is created with that
  //                             time as the creation time. Otherwise, it holds
  //                             the `ResId` of the existing entry.
  // `old_body_end`: The expected current size of the body. It is used to
  //                 determine whether to trim or truncate existing data, and
  //                 for consistency checks.
  // `buffer`: Contains the data and offset to be written.
  // `truncate`: If true, the entry's body will be truncated to the end of this
  //             write. Otherwise, the body size will grow if the write extends
  //             past the current end.
  // `doomed_new_entry`: If true, the entry is marked as doomed (deleted) upon
  //                     creation. This parameter is only used when creating a
  //                     new entry.
  // `callback`: Invoked with the result of the operation. Returns the resource
  //             ID on success, or an error code on failure.
  void WriteEntryData(const CacheEntryKey& key,
                      const ResIdOrTime& res_id_or_last_used_time,
                      int64_t old_body_end,
                      EntryWriteBuffer buffer,
                      bool truncate,
                      bool doomed_new_entry,
                      ResIdOrErrorCallback callback);

  // Reads data from an entry's body.
  // `res_id` identifies the entry to read from.
  // `offset` is the position within the entry's body to start reading.
  // `buffer` is the destination for the read data.
  // `buf_len` is the size of `buffer`.
  // `body_end` is the logical size of the entry's body.
  // If `sparse_reading` is true, the read will stop at the first gap in the
  // stored data. If false, gaps will be filled with zeros.
  // `callback` is invoked with the number of bytes read on success, or an error
  // code on failure.
  void ReadEntryData(const CacheEntryKey& key,
                     ResId res_id,
                     int64_t offset,
                     scoped_refptr<net::IOBuffer> buffer,
                     int buf_len,
                     int64_t body_end,
                     bool sparse_reading,
                     ReadResultOrErrorCallback callback);

  // Finds the available contiguous range of data for a given entry.
  // `res_id` identifies the entry.
  // `offset` is the starting position of the range to check.
  // `len` is the length of the range to check.
  // `callback` is invoked with the result. The `RangeResult` will contain the
  // starting offset and length of the first contiguous block of data found
  // within the requested range `[offset, offset + len)`. If no data is found
  // in the requested range, the `available_len` in the result will be 0.
  void GetEntryAvailableRange(const CacheEntryKey& key,
                              ResId res_id,
                              int64_t offset,
                              int len,
                              RangeResultCallback callback);

  // Calculates the total size of all entries whose `last_used` time falls
  // within the range [`initial_time`, `end_time`). The size includes the key,
  // header, body data, and a static overhead per entry. `callback` is invoked
  // with the total size on success, or an error code on failure.
  void CalculateSizeOfEntriesBetween(base::Time initial_time,
                                     base::Time end_time,
                                     Int64OrErrorCallback callback);

  // Opens the next cache entry in reverse `res_id` order. This method is used
  // for iterating through entries. To fetch all entries, start with a
  // default-constructed `iterator`. `callback` receives the entry (or
  // `std::nullopt` if no more entries exist).
  void OpenNextEntry(const EntryIterator& iterator,
                     OptionalEntryInfoWithKeyAndIteratorCallback callback);

  // Checks if cache eviction should be initiated. This is typically called by
  // the backend after an operation that increases the cache size.
  EvictionUrgency GetEvictionUrgency();

  // Starts or resumes the eviction process to reduce the cache size. This
  // method removes the least recently used entries until the total cache size
  // is below the low watermark. Entries with ResId in `excluded_res_ids`
  // (typically active entries) will not be evicted. `callback` is invoked upon
  // completion.
  //
  // `excluded_list`: A list of ResIds (typically active entries) to exclude
  //                  from eviction.
  // `is_idle_time_eviction`: True if this eviction is triggered by idle time.
  //                          If true, eviction may be aborted if the browser
  //                          becomes active.
  // `eviction_abort_flag`: A shared atomic flag that can be set to true to
  //                        signal an abort request. Note that even if this flag
  //                        is set, eviction continues until the cache size
  //                        drops below the high watermark.
  // `callback`: Invoked with the result of the eviction.
  void StartEviction(
      std::vector<ResIdAndShardId> excluded_list,
      bool is_idle_time_eviction,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> eviction_abort_flag,
      ErrorCallback callback);

  // Returns true if there is a pending eviction that was paused and needs to be
  // resumed.
  bool HasPendingEviction() const;

  // The maximum size of an individual cache entry's data stream.
  int64_t MaxFileSize() const;

  // The maximum total size of the cache.
  int64_t MaxSize() const;

  // Retrieves the count of entries.
  // Note that this value may be stale, as it doesn't account for ongoing
  // database operations.
  int32_t GetEntryCount() const;

  // Asynchronously retrieves the count of entries.
  // Retrieves the entry count asynchronously, ensuring all pending database
  // operations are complete.
  void GetEntryCountAsync(Int32Callback callback) const;

  // Retrieves the total size of all entries.
  // Note that this value may be stale, as it doesn't account for ongoing
  // database operations.
  int64_t GetSizeOfAllEntries() const;

  // Loads the in-memory index if it hasn't been loaded yet. `callback` is
  // invoked with the result of the load. If the index is already loaded, the
  // callback is run immediately with the previous result. Multiple concurrent
  // calls will all receive the same result when the load completes.
  void MaybeLoadInMemoryIndex(ErrorCallback callback);

  // If there are entries that were doomed in a previous session, this method
  // triggers a task to delete them from the database. The cleanup is performed
  // in the background. Returns true if a cleanup task was scheduled, and false
  // otherwise. `callback` is invoked upon completion of the cleanup task.
  bool MaybeRunCleanupDoomedEntries(ErrorCallback callback);

  // If the browser is idle and the number of pages recorded in the WAL exceeds
  // kSqlDiskCacheIdleCheckpointThreshold, a checkpoint is executed.
  void MaybeRunCheckpoint(base::OnceCallback<void(bool)> callback);

  enum class IndexState {
    // The in-memory index is not available (e.g., not yet loaded or
    // invalidated).
    kNotReady,
    // The index is ready and the hash was found. This may be a false positive.
    kHashFound,
    // The index is ready, but the hash was not found.
    kHashNotFound,
  };

  // Synchronously checks the state of a key hash against the in-memory index.
  IndexState GetIndexStateForHash(CacheEntryKey::Hash key_hash) const;

  // Updates the in-memory index with the given hints for the specified entry.
  void SetInMemoryEntryDataHints(CacheEntryKey::Hash key_hash,
                                 ResId res_id,
                                 MemoryEntryDataHints hints);

  // Retrieves the hints for the specified entry from the in-memory index, if
  // available.
  std::optional<MemoryEntryDataHints> GetInMemoryEntryDataHints(
      CacheEntryKey::Hash key_hash) const;

  // Attempts to retrieve a single resource ID associated with the given key
  // hash from the in-memory index. Returns the resource ID if a unique entry
  // exists for the hash; otherwise, returns std::nullopt.
  std::optional<ResId> TryGetSingleResIdFromInMemoryIndex(
      CacheEntryKey::Hash key_hash) const;

  // Returns the shard ID for a given cache key hash.
  ShardId GetShardIdForHash(CacheEntryKey::Hash key_hash) const;

  // Enables a strict corruption checking mode for testing purposes.
  void EnableStrictCorruptionCheckForTesting();

  // Sets a flag to simulate database operation failures for testing.
  void SetSimulateDbFailureForTesting(bool fail);

  // Raze the Database and the poison the database handle for testing. This is
  // useful for testing the behavior after a catastrophic error.
  void RazeAndPoisonForTesting();

  // Sets a hook to be called during eviction, allowing tests to control timing.
  void SetEvictionHookForTesting(base::RepeatingClosure hook);

 private:
  // The result of a successful initialization.
  struct InitResult {
    InitResult(std::optional<int64_t> max_bytes,
               const StoreStatus& store_status,
               int64_t database_size,
               std::optional<InMemoryIndexAndDoomedResIds> in_memory_data);
    ~InitResult();
    InitResult(InitResult&& other);
    InitResult& operator=(InitResult&& other);

    // max_bytes is set only on the first shard.
    std::optional<int64_t> max_bytes;
    StoreStatus store_status;
    int64_t database_size;
    // Used only when features::kSqlDiskCacheLoadIndexOnInit is true.
    std::optional<InMemoryIndexAndDoomedResIds> in_memory_data;
  };

  using InitResultOrError = base::expected<InitResult, Error>;
  using InitResultOrErrorCallback = base::OnceCallback<void(InitResultOrError)>;

  void SetMaxSize(int64_t max_bytes);
  base::RepeatingCallback<void(Error)> CreateBarrierErrorCallback(
      ErrorCallback callback);
  size_t GetSizeOfShards() const;
  BackendShard& GetShard(CacheEntryKey::Hash hash) const;
  BackendShard& GetShard(const CacheEntryKey& key) const;

  static std::vector<std::unique_ptr<BackendShard>> CreateBackendShards(
      const base::FilePath& path,
      net::CacheType type,
      std::vector<scoped_refptr<base::SequencedTaskRunner>>
          background_task_runners);

  void OnInitializeFinished(ErrorCallback callback,
                            std::vector<InitResultOrError> results);

  void OnLoadInMemoryIndexFinished(Error result);
  void StartEvictionInternal(
      std::vector<ResIdAndShardId> excluded_list,
      bool is_idle_time_eviction,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> eviction_abort_flag,
      ErrorCallback callback,
      Error index_load_result);
  void ResumePendingEviction(
      std::vector<base::flat_set<SqlPersistentStore::ResId>>
          excluded_res_id_sets,
      bool is_idle_time_eviction,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> eviction_abort_flag,
      ErrorCallback callback);
  void OnPendingEvictionFinished(
      std::vector<base::flat_set<SqlPersistentStore::ResId>>
          excluded_res_id_sets,
      bool is_idle_time_eviction,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> eviction_abort_flag,
      base::TimeTicks start_time,
      ErrorCallback callback,
      std::vector<ResIdListOrError> results);
  void StartNewEviction(
      std::vector<base::flat_set<SqlPersistentStore::ResId>>
          excluded_res_id_sets,
      bool is_idle_time_eviction,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> eviction_abort_flag,
      ErrorCallback callback);
  void OnEvictionFinished(bool is_idle_time_eviction,
                          base::TimeTicks start_time,
                          std::vector<ResIdListOrError> results);

  void RunNextCheckpoint(base::OnceCallback<void(bool)> callback,
                         std::vector<bool> results);

  const std::vector<scoped_refptr<base::SequencedTaskRunner>>
      background_task_runners_;
  const std::vector<std::unique_ptr<BackendShard>> backend_shards_;
  const int64_t user_max_bytes_;

  int64_t max_bytes_ = 0;

  int64_t high_watermark_ = 0;
  int64_t idle_time_high_watermark_ = 0;
  int64_t low_watermark_ = 0;
  int64_t max_file_size_ = 0;

  // Whether a cache eviction operation is currently in progress.
  bool eviction_in_progress_ = false;
  // A callback to be called when the eviction is finished.
  ErrorCallback eviction_result_callback_;

  // Whether loading of the in-memory index has been triggered.
  bool in_memory_load_triggered_ = false;
  // The result of the in-memory index load, if it has finished.
  std::optional<Error> in_memory_load_result_;
  // Callbacks waiting for the in-memory index load to complete.
  std::vector<ErrorCallback> pending_in_memory_load_result_callbacks_;

  base::WeakPtrFactory<SqlPersistentStore> weak_factory_{this};
};
}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_H_
