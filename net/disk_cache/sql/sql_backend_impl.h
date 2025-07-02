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
#include "net/disk_cache/sql/exclusive_operation_coordinator.h"
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
  void DoomActiveEntry(SqlEntryImpl& entry, CompletionOnceCallback callback);

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

  // Sends a dummy operation through the background task runner via the
  // operation coordinator, for unit tests.
  int FlushQueueForTest(CompletionOnceCallback callback);

  scoped_refptr<base::SequencedTaskRunner> GetBackgroundTaskRunnerForTest() {
    return background_task_runner_;
  }

 private:
  class IteratorImpl;

  // Identifies the type of a entry operation.
  enum class OpenOrCreateEntryOperationType {
    kCreateEntry,
    kOpenEntry,
    kOpenOrCreateEntry,
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

  SqlEntryImpl* GetActiveEntry(const CacheEntryKey& key);

  // Internal helper for Open/Create/OpenOrCreate operations. It uses
  // `ExclusiveOperationCoordinator` to serialize operations on the same key and
  // to correctly handle synchronous vs. asynchronous returns.
  EntryResult OpenOrCreateEntryInternal(OpenOrCreateEntryOperationType type,
                                        const std::string& key,
                                        EntryResultCallback callback);

  // Handles the backend logic for Open/Create/OpenOrCreate operations. This
  // method is scheduled as a normal operation via the
  // `ExclusiveOperationCoordinator` to ensure proper serialization against
  // other operations on the same key.
  void HandleOpenOrCreateEntryOperation(
      OpenOrCreateEntryOperationType type,
      const CacheEntryKey& entry_key,
      EntryResultCallback callback,
      std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle);

  // Callback for store operations that return an EntryInfo (`OpenOrCreate()`,
  // `Create()`).
  void OnEntryOperationFinished(
      const CacheEntryKey& key,
      EntryResultCallback callback,
      std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle,
      SqlPersistentStore::EntryInfoOrError result);
  // Callback for store operations that return an optional<EntryInfo>
  // (`Open()`).
  void OnOptionalEntryOperationFinished(
      const CacheEntryKey& key,
      EntryResultCallback callback,
      std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle,
      SqlPersistentStore::OptionalEntryInfoOrError result);

  // Handles the backend logic for `DoomActiveEntry()`. This method is scheduled
  // as a normal operation via the `ExclusiveOperationCoordinator`.
  void HandleDoomActiveEntryOperation(
      scoped_refptr<SqlEntryImpl> entry,
      CompletionOnceCallback callback,
      std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle);

  // Dooms an active entry. This method must be called while holding an
  // `ExclusiveOperationCoordinator::OperationHandle` to ensure proper
  // serialization of operations on the entry.
  void DoomActiveEntryInternal(SqlEntryImpl& entry,
                               CompletionOnceCallback callback);

  // Handles the backend logic for `ReleaseDoomedEntry()`. This method is
  // scheduled as a normal operation via the `ExclusiveOperationCoordinator`.
  void HandleDeleteDoomedEntry(
      const CacheEntryKey& key,
      const base::UnguessableToken& token,
      std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle);

  // Handles the backend logic for `DoomEntry()`. This method is scheduled as a
  // normal operation via the `ExclusiveOperationCoordinator`.
  void HandleDoomEntryOperation(
      const CacheEntryKey& key,
      net::RequestPriority priority,
      CompletionOnceCallback callback,
      std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle);

  // Handles the backend logic for `DoomEntriesBetween()`. This method is
  // scheduled as an exclusive operation via the
  // `ExclusiveOperationCoordinator`.
  void HandleDoomEntriesBetweenOperation(
      base::Time initial_time,
      base::Time end_time,
      CompletionOnceCallback callback,
      std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle);

  // Handles the backend logic for `UpdateEntryLastUsed()`. This method is
  // scheduled as a normal operation via the `ExclusiveOperationCoordinator`.
  void HandleUpdateEntryLastUsedOperation(
      const CacheEntryKey& key,
      const base::UnguessableToken& token,
      base::Time last_used,
      SqlPersistentStore::ErrorCallback callback,
      std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle);

  // Handles the backend logic for `UpdateEntryHeaderAndLastUsed()`. This method
  // is scheduled as a normal operation via the `ExclusiveOperationCoordinator`.
  void HandleUpdateEntryHeaderAndLastUsedOperation(
      const CacheEntryKey& key,
      const base::UnguessableToken& token,
      base::Time last_used,
      scoped_refptr<net::GrowableIOBuffer> buffer,
      int64_t header_size_delta,
      SqlPersistentStore::ErrorCallback callback,
      std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle);

  // Applies in-flight modifications to an entry's info.
  void ApplyInFlightEntryModifications(
      const CacheEntryKey& key,
      SqlPersistentStore::EntryInfo& entry_info);

  // Wraps an `ErrorCallback` to pop the oldest in-flight entry modification
  // from `in_flight_entry_modifications_` once the callback is invoked. This
  // ensures that the queue of in-flight modifications is managed correctly.
  SqlPersistentStore::ErrorCallback
  WrapErrorCallbackToPopInFlightEntryModification(
      const CacheEntryKey& key,
      SqlPersistentStore::ErrorCallback callback);

  // Task runner for all background SQLite operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The persistent store that manages the SQLite database.
  std::unique_ptr<SqlPersistentStore> store_;

  // Map of cache keys to currently active (opened) entries.
  // `raw_ref` is used because the SqlEntryImpl objects are ref-counted and
  // their lifetime is managed by their ref_count. This map only holds
  // non-owning references to them.
  std::map<CacheEntryKey, raw_ref<SqlEntryImpl>> active_entries_;

  // Set of entries that have been marked as doomed but are still active
  // (i.e., have outstanding references).
  std::set<raw_ref<const SqlEntryImpl>> doomed_entries_;

  // Coordinates exclusive and normal operations to ensure that exclusive
  // operations have exclusive access.
  ExclusiveOperationCoordinator exclusive_operation_coordinator_;

  // Queue of in-flight entry modifications that need to be applied.
  // These are typically updates to `last_used` or header data that occur
  // while an entry is not actively open.
  std::map<CacheEntryKey, std::list<InFlightEntryModification>>
      in_flight_entry_modifications_;

  // Weak pointer factory for this class.
  base::WeakPtrFactory<SqlBackendImpl> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_BACKEND_IMPL_H_
