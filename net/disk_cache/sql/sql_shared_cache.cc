// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "net/disk_cache/sql/sql_shared_cache_handle.h"
#include "net/disk_cache/sql/sql_shared_cache_isolated_database.h"

namespace disk_cache {

SqlSharedCache::SqlSharedCache(
    std::string nik_string,
    SqlPersistentStore& store,
    const base::FilePath& directory,
    base::RepeatingCallback<void(SqlSharedCache&)> on_unreferenced_callback,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : nik_string_(std::move(nik_string)),
      store_(store),
      directory_(directory),
      on_unreferenced_callback_(std::move(on_unreferenced_callback)),
      db_task_runner_(std::move(db_task_runner)) {}

SqlSharedCache::~SqlSharedCache() = default;

void SqlSharedCache::Cleanup(base::OnceClosure callback) {
  if (!isolated_database_) {
    std::move(callback).Run();
    return;
  }
  isolated_database_.AsyncCall(&SqlSharedCacheIsolatedDatabase::Cleanup)
      .Then(std::move(callback));
  isolated_database_.Reset();
}

void SqlSharedCache::InitIsolatedDatabase(
    SqlSharedCacheDbId shared_cache_db_id,
    base::OnceCallback<void(bool)> callback) {
  CHECK(!shared_cache_db_id_);
  shared_cache_db_id_ = shared_cache_db_id;
  isolated_database_ = SqlTrackedSequenceBound<SqlSharedCacheIsolatedDatabase>(
      db_task_runner_, store_->GetAsyncTaskManager(), nik_string_, directory_,
      shared_cache_db_id);
  isolated_database_.AsyncCall(&SqlSharedCacheIsolatedDatabase::Init)
      .Then(base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             base::expected<void, SqlSharedCacheIsolatedDatabase::Error>
                 result) { std::move(callback).Run(result.has_value()); },
          std::move(callback)));
}

scoped_refptr<SqlSharedCacheHandle> SqlSharedCache::CreateHandle() {
  return base::MakeRefCounted<SqlSharedCacheHandle>(
      weak_factory_.GetWeakPtr(), base::PassKey<SqlSharedCache>());
}

void SqlSharedCache::IncrementHandleCount(base::PassKey<SqlSharedCacheHandle>) {
  handle_count_++;
}

void SqlSharedCache::DecrementHandleCount(base::PassKey<SqlSharedCacheHandle>) {
  handle_count_--;
  if (!IsReferenced()) {
    on_unreferenced_callback_.Run(*this);
  }
}

}  // namespace disk_cache
