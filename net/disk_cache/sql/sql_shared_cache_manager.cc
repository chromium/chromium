// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_manager.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "net/disk_cache/sql/sql_persistent_store.h"

namespace disk_cache {

SqlSharedCacheManager::SqlSharedCacheManager(SqlPersistentStore& store,
                                             const base::FilePath& path)
    : store_(store),
      directory_(path),
      index_database_(base::ThreadPool::CreateSequencedTaskRunner(
                          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                      store_->GetAsyncTaskManager(),
                      path) {}

SqlSharedCacheManager::~SqlSharedCacheManager() = default;

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
      db_task_runner);
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

}  // namespace disk_cache
