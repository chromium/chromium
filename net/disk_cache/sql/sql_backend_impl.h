// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_BACKEND_IMPL_H_
#define NET_DISK_CACHE_SQL_SQL_BACKEND_IMPL_H_

#include <map>
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

  // Sends a dummy operation through the operation queue, for unit tests.
  int FlushQueueForTest(CompletionOnceCallback callback);

  scoped_refptr<base::SequencedTaskRunner> GetBackgroundTaskRunnerForTest() {
    return background_task_runner_;
  }

  // Provides direct access to the underlying SqlPersistentStore.
  // This is primarily used by SqlEntryImpl to interact with the database.
  SqlPersistentStore& GetStore();

 private:
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
                           SqlPersistentStore::Error result);

  SqlEntryImpl* GetActiveEntry(const CacheEntryKey& key);
  EntryResultCallbackInfo* GetEntryResultCallbackInfo(const CacheEntryKey& key);

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

  // Weak pointer factory for this class.
  base::WeakPtrFactory<SqlBackendImpl> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_BACKEND_IMPL_H_
