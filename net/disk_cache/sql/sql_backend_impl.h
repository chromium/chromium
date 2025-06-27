// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_BACKEND_IMPL_H_
#define NET_DISK_CACHE_SQL_SQL_BACKEND_IMPL_H_

#include <list>
#include <map>
#include <queue>
#include <set>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/disk_cache/buildflags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/sql/cache_entry_key.h"
#include "net/disk_cache/sql/sql_persistent_store.h"

// This backend is experimental and only available when the build flag is set.
static_assert(BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND));

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace disk_cache {

class SqlEntryImpl;

// Provides a concrete implementation of the disk cache backend that stores
// entries in a SQLite database. This class is responsible for all operations
// related to creating, opening, dooming, and enumerating cache entries.
//
// NOTE: This is currently a skeleton implementation, and some methods are not
// yet implemented, returning `net::ERR_NOT_IMPLEMENTED`.
class NET_EXPORT_PRIVATE SqlBackendImpl final : public Backend {
 public:
  SqlBackendImpl(const base::FilePath& path,
                 int64_t max_bytes,
                 net::CacheType cache_type);

  SqlBackendImpl(const SqlBackendImpl&) = delete;
  SqlBackendImpl& operator=(const SqlBackendImpl&) = delete;

  ~SqlBackendImpl() override;

  // Finishes initialization. Always asynchronous.
  void Init(CompletionOnceCallback callback);

  // Backend interface.
  int64_t MaxFileSize() const override;
  int32_t GetEntryCount(
      net::Int32CompletionOnceCallback callback) const override;
  EntryResult OpenOrCreateEntry(const std::string& key,
                                net::RequestPriority priority,
                                EntryResultCallback callback) override;
  EntryResult OpenEntry(const std::string& key,
                        net::RequestPriority priority,
                        EntryResultCallback callback) override;
  EntryResult CreateEntry(const std::string& key,
                          net::RequestPriority priority,
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
  int64_t CalculateSizeOfEntriesBetween(
      base::Time initial_time,
      base::Time end_time,
      Int64CompletionOnceCallback callback) override;
  std::unique_ptr<Iterator> CreateIterator() override;
  void GetStats(base::StringPairs* stats) override;
  void OnExternalCacheHit(const std::string& key) override;

  // Called by SqlEntryImpl when it's being closed and is not doomed.
  // Removes the entry from `active_entries_`.
  void ReleaseActiveEntry(SqlEntryImpl& entry);
  // Called by SqlEntryImpl when it's being closed and is doomed.
  // Removes the entry from `doomed_entries_`.
  void ReleaseDoomedEntry(SqlEntryImpl& entry);

  // Marks an active entry as doomed and initiates its removal from the store.
  // If `callback` is provided, it will be run upon completion.
  void DoomActiveEntry(
      SqlEntryImpl& entry,
      CompletionOnceCallback callback = CompletionOnceCallback());

  // Updates the `last_used` timestamp for an entry.
  void UpdateEntryLastUsed(const CacheEntryKey& key,
                           const base::UnguessableToken& token,
                           base::Time last_used,
                           SqlPersistentStore::ErrorCallback callback);

  // Updates the header data and `last_used` timestamp for an entry.
  void UpdateEntryHeaderAndLastUsed(const CacheEntryKey& key,
                                    const base::UnguessableToken& token,
                                    base::Time last_used,
                                    scoped_refptr<net::GrowableIOBuffer> buffer,
                                    int64_t header_size_delta,
                                    SqlPersistentStore::ErrorCallback callback);

  // Sends a dummy operation through the operation queue, for unit tests.
  int FlushQueueForTest(CompletionOnceCallback callback);

  scoped_refptr<base::SequencedTaskRunner> GetBackgroundTaskRunnerForTest() {
    return background_task_runner_;
  }

  // Provides direct access to the underlying SqlPersistentStore.
  // This is primarily used by SqlEntryImpl to interact with the database.
  SqlPersistentStore& GetStore();

 private:
  class IteratorImpl;
  class ExclusiveOperationHandle;

  // Represents a pending doom operation. This is used when an entry is doomed
  // while another operation (like `Open()` or `Create()`) for the same key is
  // in progress. The doom operation is queued and executed after the initial
  // operation completes.
  struct PendingDoomOperation {
    // Constructor for dooming a specific entry.
    explicit PendingDoomOperation(CompletionOnceCallback callback);
    // Constructor for dooming entries within a time range.
    PendingDoomOperation(base::Time initial_time,
                         base::Time end_time,
                         CompletionOnceCallback callback);
    ~PendingDoomOperation();
    PendingDoomOperation(PendingDoomOperation&&);

    // The start of the time range for dooming entries. Defaults to Min().
    base::Time initial_time = base::Time::Min();
    // The end of the time range for dooming entries. Defaults to Max().
    base::Time end_time = base::Time::Max();
    // Callback to be invoked when the doom operation completes.
    CompletionOnceCallback callback;
  };

  // Represents an in-flight modification to an entry's metadata (e.g.,
  // last_used, header). These modifications are queued and applied when the
  // entry is re-activated by `Iterator::OpenNextEntry()`.
  struct InFlightEntryModification {
    InFlightEntryModification(const base::UnguessableToken& token,
                              base::Time last_used);
    InFlightEntryModification(const base::UnguessableToken& token,
                              base::Time last_used,
                              scoped_refptr<net::GrowableIOBuffer> head);
    ~InFlightEntryModification();
    InFlightEntryModification(InFlightEntryModification&&);

    base::UnguessableToken token;
    std::optional<base::Time> last_used;
    std::optional<scoped_refptr<net::GrowableIOBuffer>> head;
  };

  // Holds information related to a pending `OpenOrCreateEntry()`,
  // `OpenEntry()`, or `CreateEntry()` operation. This includes the original
  // callback and any subsequent doom operations that were requested for the
  // same key while the initial operation was in flight.
  struct EntryResultCallbackInfo {
    explicit EntryResultCallbackInfo(EntryResultCallback callback);
    ~EntryResultCallbackInfo();
    EntryResultCallbackInfo(EntryResultCallbackInfo&&);

    // The callback provided by the caller of `OpenOrCreateEntry()`,
    // `OpenEntry()`, or `CreateEntry()`.
    EntryResultCallback callback;
    // A list of doom operations that were enqueued for this key while the
    // entry operation was pending.
    std::vector<PendingDoomOperation> pending_doom_operations;
  };

  // Inserts a new EntryResultCallbackInfo into the
  // `entry_result_callback_info_map_` for the given `key`.
  void InsertEntryResultCallback(const CacheEntryKey& key,
                                 EntryResultCallback callback);
  // Callback for store operations that return an EntryInfo (`OpenOrCreate()`,
  // `Create()`).
  void OnEntryOperationFinished(const CacheEntryKey& key,
                                SqlPersistentStore::EntryInfoOrError result);
  // Callback for store operations that return an Optional<EntryInfo>
  // (`Open()`).
  void OnOptionalEntryOperationFinished(
      const CacheEntryKey& key,
      SqlPersistentStore::OptionalEntryInfoOrError result);
  // Callback for store operations related to dooming an entry.
  void OnDoomEntryFinished(const CacheEntryKey& key,
                           CompletionOnceCallback callback,
                           std::unique_ptr<ExclusiveOperationHandle> handle,
                           SqlPersistentStore::Error result);

  SqlEntryImpl* GetActiveEntry(const CacheEntryKey& key);
  EntryResultCallbackInfo* GetEntryResultCallbackInfo(const CacheEntryKey& key);

  // Runs the next pending exclusive operation if one is not already in flight.
  void PostOrRunExclusiveOperation(
      base::OnceCallback<void(std::unique_ptr<ExclusiveOperationHandle>)>
          operation);
  void RunNextExclusiveOperation();

  // Internal implementation of `DoomEntry()`. This is scheduled as an exclusive
  // operation.
  void DoomEntryInternal(const std::string& key,
                         net::RequestPriority priority,
                         CompletionOnceCallback callback,
                         std::unique_ptr<ExclusiveOperationHandle> handle);

  // Internal implementation of `DoomEntriesBetween()`. This is scheduled as an
  // exclusive operation.
  void DoomEntriesBetweenInternal(
      base::Time initial_time,
      base::Time end_time,
      CompletionOnceCallback callback,
      std::unique_ptr<ExclusiveOperationHandle> handle);

  // Applies in-flight modifications to an entry's info.
  void ApplyInFlightEntryModifications(
      SqlPersistentStore::EntryInfo& entry_info);

  // Wraps an `ErrorCallback` to pop the oldest in-flight entry modification
  // from `in_flight_entry_modifications_` once the callback is invoked. This
  // ensures that the queue of in-flight modifications is managed correctly.
  SqlPersistentStore::ErrorCallback
  WrapErrorCallbackToPopInFlightEntryModification(
      SqlPersistentStore::ErrorCallback callback);

  // Task runner for all background SQLite operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The persistent store that manages the SQLite database.
  std::unique_ptr<SqlPersistentStore> store_;

  // Map of cache keys to EntryResultCallbackInfo. This tracks pending
  // `OpenOrCreateEntry()`, `OpenEntry()`, and `CreateEntry()` operations.
  // Entries are added when an operation starts and removed when it completes.
  std::map<CacheEntryKey, EntryResultCallbackInfo>
      entry_result_callback_info_map_;

  // Map of cache keys to currently active (opened) entries.
  // `raw_ref` is used because the SqlEntryImpl objects are ref-counted and
  // their lifetime is managed by their ref_count. This map only holds
  // non-owning references to them.
  std::map<CacheEntryKey, raw_ref<SqlEntryImpl>> active_entries_;

  // Set of entries that have been marked as doomed but are still active
  // (i.e., have outstanding references).
  std::set<raw_ref<const SqlEntryImpl>> doomed_entries_;

  // Stores tokens of entries that have been marked for dooming and are
  // currently being processed by the `SqlPersistentStore`. This prevents
  // these entries from being re-added to `active_entries_` if reopened.
  std::set<base::UnguessableToken> pending_doomed_entry_tokens_;

  // A flag to serialize exclusive operations like mass-delete and iteration to
  // prevent data inconsistencies between in-memory entry states and the
  // persistent storage.
  bool exclusive_operation_inflight_ = false;

  // Queue of operations to be run when `exclusive_operation_inflight_` is
  // false.
  std::queue<
      base::OnceCallback<void(std::unique_ptr<ExclusiveOperationHandle>)>>
      pending_exclusive_operations_;

  // Queue of in-flight entry modifications that need to be applied.
  // These are typically updates to `last_used` or header data that occur
  // while an entry is not actively open.
  std::list<InFlightEntryModification> in_flight_entry_modifications_;

  // Weak pointer factory for this class.
  base::WeakPtrFactory<SqlBackendImpl> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_BACKEND_IMPL_H_
