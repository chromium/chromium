// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_MANAGER_H_
#define NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_MANAGER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/disk_cache/sql/sql_read_cache_memory_monitor.h"
#include "net/disk_cache/sql/sql_shared_cache.h"
#include "net/disk_cache/sql/sql_shared_cache_handle.h"
#include "net/disk_cache/sql/sql_shared_cache_index_database.h"
#include "net/disk_cache/sql/sql_tracked_sequence_bound.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace disk_cache {

class BackendCleanupTracker;
class SqlPersistentStore;

// Manages the creation, lookup, and lifecycle of `SqlSharedCache` instances.
//
// Maintains mapping between NetworkIsolationKey string representation / DbId
// and `SqlSharedCache` objects. Handles serialized database operations for
// initializing index databases and retrieving or creating shared caches.
class NET_EXPORT_PRIVATE SqlSharedCacheManager {
 public:
  using InitCallback = base::OnceCallback<void(
      base::expected<void, SqlSharedCacheIndexDatabase::Error>)>;

  SqlSharedCacheManager(
      SqlPersistentStore& store,
      const base::FilePath& path,
      scoped_refptr<SqlReadCacheMemoryMonitor> read_cache_memory_monitor,
      scoped_refptr<BackendCleanupTracker> cleanup_tracker);
  ~SqlSharedCacheManager();

  // Asynchronously initializes the index database.
  void Init(InitCallback callback);

  // Asynchronously retrieves a `SqlSharedCacheHandle` associated with the given
  // `shared_cache_db_id`. Calls `callback` with a handle (or nullptr if lookup
  // fails/error occurs).
  void GetCacheByDbId(
      SqlSharedCacheDbId shared_cache_db_id,
      base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback);

  // Asynchronously retrieves or creates a `SqlSharedCacheHandle` for the given
  // `nik`. If `require_shared_cache_db_id` is true, an entry in the index
  // database will be created/resolved.
  void GetCacheByNik(
      const net::NetworkIsolationKey& nik,
      bool require_shared_cache_db_id,
      base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback);

  // Asynchronously deletes the shared cache resources specified by `resources`.
  // The resources are grouped by their database ID and deleted from their
  // corresponding isolated databases. Invokes `callback` upon completion.
  void DeleteResources(std::vector<SqlSharedCacheResourceId> resources,
                       base::OnceClosure callback);

  // Asynchronously copies eligible entries into their corresponding isolated
  // shared cache databases grouped by NetworkIsolationKey. Unprocessed entries
  // are returned via `callback`.
  void ProcessSharedCacheEligibleEntries(
      std::map<net::NetworkIsolationKey,
               base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
          entries,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
      base::OnceCallback<void(
          std::vector<SqlPersistentStore::SharedCacheEligibleEntry>)> callback,
      base::RepeatingCallback<void(const CacheEntryKey&)>
          on_entry_copied_callback = {});

  // Sets a flag to simulate index database operation failures for testing.
  void SetSimulateDbFailureForTesting(bool fail);

 private:
  friend class SqlSharedCacheManagerTest;

  // Handle used to signal completion of a serialized database operation.
  // When destroyed, `FinishDbOperation()` is invoked to run the next queued
  // operation.
  using DbOperationHandle = base::ScopedClosureRunner;

  void PostDbOperation(base::OnceCallback<void(DbOperationHandle)> operation);
  void MaybeRunNextDbOperation();
  void FinishDbOperation();

  void OnSqlSharedCacheUnreferenced(SqlSharedCache& cache);
  void DoDeleteUnreferencedSqlSharedCache(
      const std::string& nik_string,
      DbOperationHandle db_operation_handle);

  void DoGetCacheByNik(
      net::NetworkIsolationKey nik,
      bool require_shared_cache_db_id,
      base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback,
      DbOperationHandle db_operation_handle);
  void OnGetSharedDbIdForNik(
      net::NetworkIsolationKey nik,
      base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback,
      DbOperationHandle db_operation_handle,
      base::expected<SqlSharedCacheDbId, SqlSharedCacheIndexDatabase::Error>
          result);
  scoped_refptr<SqlSharedCacheHandle> RegisterNewSqlSharedCache(
      const std::string& nik_str,
      std::optional<SqlSharedCacheDbId> shared_cache_db_id,
      DbOperationHandle db_operation_handle);

  void DoGetCacheByDbId(
      SqlSharedCacheDbId shared_cache_db_id,
      base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback,
      DbOperationHandle db_operation_handle);
  void OnGetNikStringForDbId(
      SqlSharedCacheDbId shared_cache_db_id,
      base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback,
      DbOperationHandle db_operation_handle,
      base::expected<std::string, SqlSharedCacheIndexDatabase::Error> result);

  void DeleteNextResourceGroup(
      absl::flat_hash_map<SqlSharedCacheDbId, std::vector<SqlSharedCacheRowId>>
          grouped_resources,
      base::OnceClosure callback,
      DbOperationHandle db_operation_handle);

  void DoProcessSharedCacheEligibleEntries(
      std::map<net::NetworkIsolationKey,
               base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
          entries,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
      base::OnceCallback<void(
          std::vector<SqlPersistentStore::SharedCacheEligibleEntry>)> callback,
      base::RepeatingCallback<void(const CacheEntryKey&)>
          on_entry_copied_callback,
      DbOperationHandle db_operation_handle);

  void ProcessNextNikGroup(
      base::queue<base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
          groups,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
      std::vector<SqlPersistentStore::SharedCacheEligibleEntry> all_unprocessed,
      base::OnceCallback<void(
          std::vector<SqlPersistentStore::SharedCacheEligibleEntry>)> callback,
      base::RepeatingCallback<void(const CacheEntryKey&)>
          on_entry_copied_callback);
  void OnGetSharedCacheForProcess(
      net::NetworkIsolationKey current_nik,
      base::queue<base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
          groups,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
      std::vector<SqlPersistentStore::SharedCacheEligibleEntry> all_unprocessed,
      base::OnceCallback<void(
          std::vector<SqlPersistentStore::SharedCacheEligibleEntry>)> callback,
      base::RepeatingCallback<void(const CacheEntryKey&)>
          on_entry_copied_callback,
      scoped_refptr<SqlSharedCacheHandle> handle);
  void OnProcessEntryCompleted(
      scoped_refptr<SqlSharedCacheHandle> handle,
      base::queue<base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
          groups,
      scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
      std::vector<SqlPersistentStore::SharedCacheEligibleEntry> all_unprocessed,
      base::OnceCallback<void(
          std::vector<SqlPersistentStore::SharedCacheEligibleEntry>)> callback,
      base::RepeatingCallback<void(const CacheEntryKey&)>
          on_entry_copied_callback,
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry> results);

  const raw_ref<SqlPersistentStore> store_;
  const base::FilePath directory_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  SqlTrackedSequenceBound<SqlSharedCacheIndexDatabase> index_database_;
  scoped_refptr<SqlReadCacheMemoryMonitor> read_cache_memory_monitor_;
  scoped_refptr<BackendCleanupTracker> cleanup_tracker_;

  base::queue<base::OnceCallback<void(DbOperationHandle)>>
      pending_db_operations_;
  bool db_operation_in_progress_ = true;

  base::flat_set<std::unique_ptr<SqlSharedCache>, base::UniquePtrComparator>
      shared_caches_;
  absl::flat_hash_map<SqlSharedCacheDbId, raw_ptr<SqlSharedCache>>
      shared_caches_by_shared_cache_db_id_;
  absl::flat_hash_map<std::string, raw_ptr<SqlSharedCache>>
      shared_caches_by_nik_string_;

  base::WeakPtrFactory<SqlSharedCacheManager> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_MANAGER_H_
