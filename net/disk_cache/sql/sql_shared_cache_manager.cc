// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_manager.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/sql/sql_persistent_store.h"

namespace disk_cache {

SqlSharedCacheManager::SqlSharedCacheManager(
    SqlPersistentStore& store,
    const base::FilePath& path,
    scoped_refptr<SqlReadCacheMemoryMonitor> read_cache_memory_monitor,
    scoped_refptr<BackendCleanupTracker> cleanup_tracker)
    : store_(store),
      directory_(path),
      db_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      index_database_(db_task_runner_, store_->GetAsyncTaskManager(), path),
      read_cache_memory_monitor_(std::move(read_cache_memory_monitor)),
      cleanup_tracker_(cleanup_tracker) {}

SqlSharedCacheManager::~SqlSharedCacheManager() {
  index_database_.Reset();
  if (cleanup_tracker_) {
    CHECK(db_task_runner_);
    db_task_runner_->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::DoNothingWithBoundArgs(std::move(cleanup_tracker_)));
  }
}

void SqlSharedCacheManager::Init(InitCallback callback) {
  index_database_.AsyncCall(&SqlSharedCacheIndexDatabase::Initialize)
      .Then(std::move(callback).Then(
          base::BindOnce(&SqlSharedCacheManager::FinishDbOperation,
                         weak_factory_.GetWeakPtr())));
}

void SqlSharedCacheManager::PostDbOperation(
    base::OnceCallback<void(DbOperationHandle)> operation) {
  pending_db_operations_.push(std::move(operation));
  MaybeRunNextDbOperation();
}

void SqlSharedCacheManager::MaybeRunNextDbOperation() {
  if (db_operation_in_progress_ || pending_db_operations_.empty()) {
    return;
  }
  db_operation_in_progress_ = true;
  auto op = std::move(pending_db_operations_.front());
  pending_db_operations_.pop();
  DbOperationHandle db_operation_handle(base::BindOnce(
      &SqlSharedCacheManager::FinishDbOperation, weak_factory_.GetWeakPtr()));
  std::move(op).Run(std::move(db_operation_handle));
}

void SqlSharedCacheManager::FinishDbOperation() {
  CHECK(db_operation_in_progress_);
  db_operation_in_progress_ = false;
  MaybeRunNextDbOperation();
}

void SqlSharedCacheManager::GetCacheByNik(
    const net::NetworkIsolationKey& nik,
    bool require_shared_cache_db_id,
    base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback) {
  CHECK(!nik.IsTransient());
  DoGetCacheByNik(nik, require_shared_cache_db_id, std::move(callback),
                  DbOperationHandle());
}

void SqlSharedCacheManager::DoGetCacheByNik(
    net::NetworkIsolationKey nik,
    bool require_shared_cache_db_id,
    base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback,
    DbOperationHandle db_operation_handle) {
  if (auto it = shared_caches_by_nik_string_.find(*nik.ToCacheKeyString());
      it != shared_caches_by_nik_string_.end()) {
    if (!require_shared_cache_db_id || it->second->shared_cache_db_id()) {
      std::move(callback).Run(it->second->CreateHandle());
      return;
    }
  }
  if (!db_operation_handle) {
    PostDbOperation(base::BindOnce(
        &SqlSharedCacheManager::DoGetCacheByNik, weak_factory_.GetWeakPtr(),
        nik, require_shared_cache_db_id, std::move(callback)));
    return;
  }
  index_database_
      .AsyncCall(&SqlSharedCacheIndexDatabase::GetDbIdByNetworkIsolationKey)
      .WithArgs(nik, require_shared_cache_db_id)
      .Then(base::BindOnce(&SqlSharedCacheManager::OnGetSharedDbIdForNik,
                           weak_factory_.GetWeakPtr(), nik, std::move(callback),
                           std::move(db_operation_handle)));
}

void SqlSharedCacheManager::OnGetSharedDbIdForNik(
    net::NetworkIsolationKey nik,
    base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback,
    DbOperationHandle db_operation_handle,
    base::expected<SqlSharedCacheDbId, SqlSharedCacheIndexDatabase::Error>
        result) {
  std::optional<SqlSharedCacheDbId> shared_cache_db_id;
  if (result.has_value()) {
    shared_cache_db_id = result.value();
  } else if (result.error() != SqlSharedCacheIndexDatabase::Error::kNotFound) {
    std::move(callback).Run(scoped_refptr<SqlSharedCacheHandle>());
    return;
  }

  const auto nik_str = *nik.ToCacheKeyString();
  if (auto it = shared_caches_by_nik_string_.find(nik_str);
      it != shared_caches_by_nik_string_.end()) {
    auto& cache_ptr = it->second;
    if (shared_cache_db_id && !cache_ptr->shared_cache_db_id()) {
      CHECK(shared_caches_by_shared_cache_db_id_
                .emplace(*shared_cache_db_id, cache_ptr)
                .second);
      cache_ptr->InitIsolatedDatabase(
          *shared_cache_db_id,
          base::BindOnce(
              [](DbOperationHandle db_operation_handle, bool result) {},
              std::move(db_operation_handle)));
    }
    std::move(callback).Run(cache_ptr->CreateHandle());
    return;
  }
  std::move(callback).Run(RegisterNewSqlSharedCache(
      nik_str, shared_cache_db_id, std::move(db_operation_handle)));
}

scoped_refptr<SqlSharedCacheHandle>
SqlSharedCacheManager::RegisterNewSqlSharedCache(
    const std::string& nik_str,
    std::optional<SqlSharedCacheDbId> shared_cache_db_id,
    DbOperationHandle db_operation_handle) {
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  auto cache = std::make_unique<SqlSharedCache>(
      nik_str, *store_, directory_,
      base::BindRepeating(&SqlSharedCacheManager::OnSqlSharedCacheUnreferenced,
                          weak_factory_.GetWeakPtr()),
      db_task_runner, read_cache_memory_monitor_, cleanup_tracker_);
  SqlSharedCache* cache_ptr = cache.get();
  shared_caches_.insert(std::move(cache));
  CHECK(shared_caches_by_nik_string_.emplace(nik_str, cache_ptr).second);
  if (shared_cache_db_id) {
    CHECK(shared_caches_by_shared_cache_db_id_
              .emplace(*shared_cache_db_id, cache_ptr)
              .second);
    cache_ptr->InitIsolatedDatabase(
        *shared_cache_db_id,
        base::BindOnce(
            [](DbOperationHandle db_operation_handle, bool result) {},
            std::move(db_operation_handle)));
  }
  return cache_ptr->CreateHandle();
}

void SqlSharedCacheManager::GetCacheByDbId(
    SqlSharedCacheDbId shared_cache_db_id,
    base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback) {
  DoGetCacheByDbId(shared_cache_db_id, std::move(callback),
                   DbOperationHandle());
}

void SqlSharedCacheManager::DoGetCacheByDbId(
    SqlSharedCacheDbId shared_cache_db_id,
    base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback,
    DbOperationHandle db_operation_handle) {
  if (auto it = shared_caches_by_shared_cache_db_id_.find(shared_cache_db_id);
      it != shared_caches_by_shared_cache_db_id_.end()) {
    std::move(callback).Run(it->second->CreateHandle());
    return;
  }
  if (!db_operation_handle) {
    PostDbOperation(base::BindOnce(&SqlSharedCacheManager::DoGetCacheByDbId,
                                   weak_factory_.GetWeakPtr(),
                                   shared_cache_db_id, std::move(callback)));
    return;
  }

  index_database_
      .AsyncCall(&SqlSharedCacheIndexDatabase::GetIsolationKeyStringByDbId)
      .WithArgs(shared_cache_db_id)
      .Then(base::BindOnce(&SqlSharedCacheManager::OnGetNikStringForDbId,
                           weak_factory_.GetWeakPtr(), shared_cache_db_id,
                           std::move(callback),
                           std::move(db_operation_handle)));
}

void SqlSharedCacheManager::OnGetNikStringForDbId(
    SqlSharedCacheDbId shared_cache_db_id,
    base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback,
    DbOperationHandle db_operation_handle,
    base::expected<std::string, SqlSharedCacheIndexDatabase::Error> result) {
  if (!result.has_value()) {
    std::move(callback).Run(scoped_refptr<SqlSharedCacheHandle>());
    return;
  }
  std::move(callback).Run(RegisterNewSqlSharedCache(
      result.value(), shared_cache_db_id, std::move(db_operation_handle)));
}

void SqlSharedCacheManager::DeleteResources(
    std::vector<SqlSharedCacheResourceId> resources,
    base::OnceClosure callback) {
  absl::flat_hash_map<SqlSharedCacheDbId, std::vector<SqlSharedCacheRowId>>
      grouped_resources;
  for (const auto& resource : resources) {
    grouped_resources[resource.db_id].push_back(resource.row_id);
  }
  PostDbOperation(
      base::BindOnce(&SqlSharedCacheManager::DeleteNextResourceGroup,
                     weak_factory_.GetWeakPtr(), std::move(grouped_resources),
                     std::move(callback)));
}

void SqlSharedCacheManager::DeleteNextResourceGroup(
    absl::flat_hash_map<SqlSharedCacheDbId, std::vector<SqlSharedCacheRowId>>
        grouped_resources,
    base::OnceClosure callback,
    DbOperationHandle db_operation_handle) {
  if (grouped_resources.empty()) {
    if (callback) {
      std::move(callback).Run();
    }
    return;
  }

  auto it = grouped_resources.begin();
  const SqlSharedCacheDbId db_id = it->first;
  std::vector<SqlSharedCacheRowId> row_ids = std::move(it->second);
  grouped_resources.erase(it);

  auto next_task =
      base::BindOnce(&SqlSharedCacheManager::DeleteNextResourceGroup,
                     weak_factory_.GetWeakPtr(), std::move(grouped_resources),
                     std::move(callback), std::move(db_operation_handle));

  DoGetCacheByDbId(
      db_id,
      base::BindOnce(
          [](std::vector<SqlSharedCacheRowId> row_ids,
             base::OnceClosure next_task,
             scoped_refptr<SqlSharedCacheHandle> handle) {
            if (handle && *handle) {
              auto* cache = handle->get();
              cache->DeleteEntries(
                  row_ids,
                  base::BindOnce(
                      [](base::OnceClosure next_task,
                         base::expected<void,
                                        SqlSharedCacheIsolatedDatabase::Error>
                             result) { std::move(next_task).Run(); },
                      std::move(next_task)));
            } else {
              std::move(next_task).Run();
            }
          },
          std::move(row_ids), std::move(next_task)),
      DbOperationHandle(base::DoNothing()));
}

void SqlSharedCacheManager::OnSqlSharedCacheUnreferenced(
    SqlSharedCache& cache) {
  PostDbOperation(
      base::BindOnce(&SqlSharedCacheManager::DoDeleteUnreferencedSqlSharedCache,
                     weak_factory_.GetWeakPtr(), cache.nik_string()));
}

void SqlSharedCacheManager::DoDeleteUnreferencedSqlSharedCache(
    const std::string& nik_string,
    DbOperationHandle db_operation_handle) {
  auto it = shared_caches_by_nik_string_.find(nik_string);
  if (it == shared_caches_by_nik_string_.end()) {
    return;
  }

  SqlSharedCache* cache = it->second;
  if (cache->IsReferenced()) {
    return;
  }

  shared_caches_by_nik_string_.erase(it);
  if (cache->shared_cache_db_id()) {
    shared_caches_by_shared_cache_db_id_.erase(*cache->shared_cache_db_id());
  }
  auto cache_it = shared_caches_.find(cache);
  CHECK(cache_it != shared_caches_.end());
  auto cache_to_delete =
      std::move(const_cast<std::unique_ptr<SqlSharedCache>&>(*cache_it));
  shared_caches_.erase(cache_it);
  cache_to_delete->Cleanup(
      base::DoNothingWithBoundArgs(std::move(db_operation_handle)));
}

void SqlSharedCacheManager::ProcessSharedCacheEligibleEntries(
    std::map<net::NetworkIsolationKey,
             base::queue<SqlPersistentStore::SharedCacheEligibleEntry>> entries,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    base::OnceCallback<void(
        std::vector<SqlPersistentStore::SharedCacheEligibleEntry>)> callback,
    base::RepeatingCallback<void(const CacheEntryKey&)>
        on_entry_copied_callback) {
  CHECK(abort_flag);
  PostDbOperation(base::BindOnce(
      &SqlSharedCacheManager::DoProcessSharedCacheEligibleEntries,
      weak_factory_.GetWeakPtr(), std::move(entries), abort_flag,
      std::move(callback), std::move(on_entry_copied_callback)));
}

void SqlSharedCacheManager::DoProcessSharedCacheEligibleEntries(
    std::map<net::NetworkIsolationKey,
             base::queue<SqlPersistentStore::SharedCacheEligibleEntry>> entries,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    base::OnceCallback<void(
        std::vector<SqlPersistentStore::SharedCacheEligibleEntry>)> callback,
    base::RepeatingCallback<void(const CacheEntryKey&)>
        on_entry_copied_callback,
    DbOperationHandle db_operation_handle) {
  base::queue<base::queue<SqlPersistentStore::SharedCacheEligibleEntry>> groups;
  for (auto& [nik, entry_queue] : entries) {
    groups.push(std::move(entry_queue));
  }

  ProcessNextNikGroup(
      std::move(groups), abort_flag, {},
      std::move(callback).Then(base::OnceClosure(
          base::DoNothingWithBoundArgs(std::move(db_operation_handle)))),
      std::move(on_entry_copied_callback));
}

void SqlSharedCacheManager::ProcessNextNikGroup(
    base::queue<base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
        groups,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    std::vector<SqlPersistentStore::SharedCacheEligibleEntry> all_unprocessed,
    base::OnceCallback<void(
        std::vector<SqlPersistentStore::SharedCacheEligibleEntry>)> callback,
    base::RepeatingCallback<void(const CacheEntryKey&)>
        on_entry_copied_callback) {
  CHECK(abort_flag);
  if (groups.empty() || abort_flag->data.load(std::memory_order_relaxed)) {
    while (!groups.empty()) {
      auto& group = groups.front();
      while (!group.empty()) {
        all_unprocessed.push_back(std::move(group.front()));
        group.pop();
      }
      groups.pop();
    }
    std::move(callback).Run(std::move(all_unprocessed));
    return;
  }
  CHECK(!groups.front().empty());
  const net::NetworkIsolationKey next_nik = groups.front().front().nik;
  DoGetCacheByNik(
      next_nik, /*require_shared_cache_db_id=*/true,
      base::BindOnce(&SqlSharedCacheManager::OnGetSharedCacheForProcess,
                     weak_factory_.GetWeakPtr(), next_nik, std::move(groups),
                     std::move(abort_flag), std::move(all_unprocessed),
                     std::move(callback), std::move(on_entry_copied_callback)),
      DbOperationHandle(base::DoNothing()));
}

void SqlSharedCacheManager::OnGetSharedCacheForProcess(
    net::NetworkIsolationKey current_nik,
    base::queue<base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
        groups,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    std::vector<SqlPersistentStore::SharedCacheEligibleEntry> all_unprocessed,
    base::OnceCallback<void(
        std::vector<SqlPersistentStore::SharedCacheEligibleEntry>)> callback,
    base::RepeatingCallback<void(const CacheEntryKey&)>
        on_entry_copied_callback,
    scoped_refptr<SqlSharedCacheHandle> handle) {
  // `groups` is a `base::queue` of entry queues (each inner queue contains
  // entries sharing the same NetworkIsolationKey).
  CHECK(!groups.empty());
  CHECK(!groups.front().empty());
  CHECK(groups.front().front().nik == current_nik);
  auto entry_queue = std::move(groups.front());
  groups.pop();
  if (!handle) {
    ProcessNextNikGroup(std::move(groups), abort_flag,
                        std::move(all_unprocessed), std::move(callback),
                        std::move(on_entry_copied_callback));
    return;
  }
  (*handle)->CopyEntries(
      std::move(entry_queue), abort_flag,
      base::BindOnce(&SqlSharedCacheManager::OnProcessEntryCompleted,
                     weak_factory_.GetWeakPtr(), handle, std::move(groups),
                     abort_flag, std::move(all_unprocessed),
                     std::move(callback), on_entry_copied_callback),
      std::move(on_entry_copied_callback));
}

void SqlSharedCacheManager::OnProcessEntryCompleted(
    scoped_refptr<SqlSharedCacheHandle> handle,
    base::queue<base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
        groups,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    std::vector<SqlPersistentStore::SharedCacheEligibleEntry> all_unprocessed,
    base::OnceCallback<void(
        std::vector<SqlPersistentStore::SharedCacheEligibleEntry>)> callback,
    base::RepeatingCallback<void(const CacheEntryKey&)>
        on_entry_copied_callback,
    base::queue<SqlPersistentStore::SharedCacheEligibleEntry> results) {
  while (!results.empty()) {
    all_unprocessed.push_back(std::move(results.front()));
    results.pop();
  }
  ProcessNextNikGroup(std::move(groups), abort_flag, std::move(all_unprocessed),
                      std::move(callback), std::move(on_entry_copied_callback));
}

void SqlSharedCacheManager::SetSimulateDbFailureForTesting(bool fail) {
  if (index_database_) {
    index_database_
        .AsyncCall(&SqlSharedCacheIndexDatabase::SetSimulateDbFailureForTesting)
        .WithArgs(fail);
  }
}

}  // namespace disk_cache
