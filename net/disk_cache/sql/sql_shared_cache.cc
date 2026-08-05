// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/features.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "net/disk_cache/sql/sql_shared_cache_handle.h"
#include "net/disk_cache/sql/sql_shared_cache_isolated_database.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_info.h"

namespace disk_cache {

SqlSharedCache::SqlSharedCache(
    std::string nik_string,
    SqlPersistentStore& store,
    const base::FilePath& directory,
    base::RepeatingCallback<void(SqlSharedCache&)> on_unreferenced_callback,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    scoped_refptr<BackendCleanupTracker> cleanup_tracker)
    : nik_string_(std::move(nik_string)),
      store_(store),
      directory_(directory),
      on_unreferenced_callback_(std::move(on_unreferenced_callback)),
      db_task_runner_(std::move(db_task_runner)),
      cleanup_tracker_(std::move(cleanup_tracker)) {}

SqlSharedCache::~SqlSharedCache() {
  isolated_database_.Reset();
  if (cleanup_tracker_) {
    CHECK(db_task_runner_);
    db_task_runner_->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::DoNothingWithBoundArgs(std::move(cleanup_tracker_)));
  }
}

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
      shared_cache_db_id, db_task_runner_);
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

void SqlSharedCache::CopyEntries(
    base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    base::OnceCallback<void(
        base::queue<SqlPersistentStore::SharedCacheEligibleEntry>)> callback) {
  CHECK(pending_copy_entries_.empty());
  CHECK(!copy_callback_);
  CHECK(!current_copy_row_id_);
  CHECK(!entries.empty());
  CHECK(shared_cache_db_id_);
  CHECK(isolated_database_);
  pending_copy_entries_ = std::move(entries);
  copy_abort_flag_ = std::move(abort_flag);
  copy_callback_ = std::move(callback);
  CopyNextEntry();
}

void SqlSharedCache::CopyNextEntry() {
  if (pending_copy_entries_.empty() ||
      copy_abort_flag_->data.load(std::memory_order_relaxed)) {
    FinishCopy();
    return;
  }
  auto entry = std::move(pending_copy_entries_.front());
  pending_copy_entries_.pop();
  const auto key = entry.key;
  store_->OpenEntry(
      key, base::BindOnce(&SqlSharedCache::OnEntryOpenedForSharedCache,
                          weak_factory_.GetWeakPtr(), std::move(entry)));
}

void SqlSharedCache::OnEntryOpenedForSharedCache(
    SqlPersistentStore::SharedCacheEligibleEntry entry,
    base::expected<std::optional<SqlPersistentStore::EntryInfo>,
                   SqlPersistentStore::Error> result) {
  if (!result.has_value() || !result.value().has_value() ||
      !result.value()->head) {
    OnCopyEntryFailed();
    return;
  }
  auto info = std::move(result.value().value());
  if (info.body_end >
      net::features::kSqlDiskCacheMaxSharedCacheCopyEntrySize.Get()) {
    OnCopyEntryFailed();
    return;
  }
  net::HttpResponseInfo disk_response_info;
  bool response_truncated = false;
  if (!net::HttpCache::ParseResponseInfo(base::as_bytes(info.head->span()),
                                         &disk_response_info,
                                         &response_truncated) ||
      response_truncated ||
      disk_response_info.response_time != entry.response_info->response_time) {
    OnCopyEntryFailed();
    return;
  }
  auto pickled_buffer = base::MakeRefCounted<net::PickledIOBuffer>(
      entry.response_info->MakePickle(/*skip_transient_headers=*/true,
                                      /*response_truncated=*/false));
  if (info.body_end > 0) {
    int64_t chunk_size = std::min(
        static_cast<int64_t>(
            net::features::kSqlDiskCacheSharedCacheReadBufferSize.Get()),
        info.body_end);
    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(chunk_size);
    const auto key = entry.key;
    const auto res_id = info.res_id;
    const auto body_end = info.body_end;
    store_->ReadEntryData(
        key, res_id, 0, buffer, chunk_size, body_end,
        /*sparse_reading=*/false,
        base::BindOnce(&SqlSharedCache::OnEntryDataReadForInsert,
                       weak_factory_.GetWeakPtr(), std::move(entry), res_id,
                       std::move(pickled_buffer), body_end, buffer));
    return;
  }
  isolated_database_.AsyncCall(&SqlSharedCacheIsolatedDatabase::Insert)
      .WithArgs(entry.key, std::move(pickled_buffer), info.body_end, nullptr)
      .Then(base::BindOnce(&SqlSharedCache::OnIsolatedDatabaseInserted,
                           weak_factory_.GetWeakPtr(), entry.key, info.res_id,
                           info.body_end, 0));
}

void SqlSharedCache::OnEntryDataReadForInsert(
    SqlPersistentStore::SharedCacheEligibleEntry entry,
    SqlPersistentStore::ResId res_id,
    scoped_refptr<net::PickledIOBuffer> headers,
    int64_t body_end,
    scoped_refptr<net::IOBuffer> buffer,
    base::expected<SqlPersistentStore::ReadResult, SqlPersistentStore::Error>
        result) {
  // For `sparse_reading = false`, `SqlPersistentStoreBackend::ReadEntryData`
  // zero-fills gaps and returns `read_bytes` equal to the requested size
  // as long as the read is within bounds.
  if (!result.has_value() ||
      result.value().read_bytes != static_cast<int>(buffer->size())) {
    OnCopyEntryFailed();
    return;
  }
  isolated_database_.AsyncCall(&SqlSharedCacheIsolatedDatabase::Insert)
      .WithArgs(entry.key, std::move(headers), body_end, buffer)
      .Then(base::BindOnce(&SqlSharedCache::OnIsolatedDatabaseInserted,
                           weak_factory_.GetWeakPtr(), entry.key, res_id,
                           body_end, buffer->size()));
}

void SqlSharedCache::OnIsolatedDatabaseInserted(
    CacheEntryKey key,
    SqlPersistentStore::ResId res_id,
    int64_t body_end,
    int64_t offset,
    base::expected<SqlSharedCacheRowId, SqlSharedCacheIsolatedDatabase::Error>
        result) {
  if (!result.has_value()) {
    OnCopyEntryFailed();
    return;
  }
  CHECK(!current_copy_row_id_);
  current_copy_row_id_ = result.value();
  ReadNextChunk(std::move(key), res_id, body_end, offset, result.value());
}

void SqlSharedCache::ReadNextChunk(CacheEntryKey key,
                                   SqlPersistentStore::ResId res_id,
                                   int64_t body_end,
                                   int64_t offset,
                                   SqlSharedCacheRowId shared_cache_row_id) {
  CHECK_LE(offset, body_end);
  if (offset == body_end) {
    MoveBlobsToSharedCache(key, res_id, shared_cache_row_id);
    return;
  }
  int64_t chunk_size =
      std::min(static_cast<int64_t>(
                   net::features::kSqlDiskCacheSharedCacheReadBufferSize.Get()),
               body_end - offset);
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(chunk_size);
  store_->ReadEntryData(
      key, res_id, offset, buffer, chunk_size, body_end,
      /*sparse_reading=*/false,
      base::BindOnce(&SqlSharedCache::OnEntryDataRead,
                     weak_factory_.GetWeakPtr(), key, res_id, body_end, offset,
                     shared_cache_row_id, buffer));
}

void SqlSharedCache::OnEntryDataRead(
    CacheEntryKey key,
    SqlPersistentStore::ResId res_id,
    int64_t body_end,
    int64_t offset,
    SqlSharedCacheRowId shared_cache_row_id,
    scoped_refptr<net::IOBuffer> buffer,
    base::expected<SqlPersistentStore::ReadResult, SqlPersistentStore::Error>
        result) {
  // For `sparse_reading = false`, `SqlPersistentStoreBackend::ReadEntryData`
  // zero-fills gaps and returns `read_bytes` equal to the requested size
  // as long as the read is within bounds.
  if (!result.has_value() || result.value().read_bytes != buffer->size()) {
    OnCopyEntryFailed();
    return;
  }
  bool set_ready = (offset + buffer->size() == body_end);
  isolated_database_.AsyncCall(&SqlSharedCacheIsolatedDatabase::WriteBody)
      .WithArgs(key, shared_cache_row_id, offset, buffer, set_ready)
      .Then(base::BindOnce(&SqlSharedCache::OnIsolatedDatabaseWritten,
                           weak_factory_.GetWeakPtr(), key, res_id, body_end,
                           offset + buffer->size(), shared_cache_row_id));
}

void SqlSharedCache::OnIsolatedDatabaseWritten(
    CacheEntryKey key,
    SqlPersistentStore::ResId res_id,
    int64_t body_end,
    int64_t next_offset,
    SqlSharedCacheRowId shared_cache_row_id,
    base::expected<void, SqlSharedCacheIsolatedDatabase::Error> result) {
  if (!result.has_value()) {
    OnCopyEntryFailed();
    return;
  }
  ReadNextChunk(std::move(key), res_id, body_end, next_offset,
                shared_cache_row_id);
}

void SqlSharedCache::MoveBlobsToSharedCache(
    CacheEntryKey key,
    SqlPersistentStore::ResId res_id,
    SqlSharedCacheRowId shared_cache_row_id) {
  store_->MoveBlobsToSharedCache(
      key, res_id, {*shared_cache_db_id_, shared_cache_row_id},
      base::BindOnce(
          [](base::WeakPtr<SqlSharedCache> self,
             SqlPersistentStore::Error error) {
            if (self) {
              if (error == SqlPersistentStore::Error::kOk) {
                self->OnCopyEntryComplete();
              } else {
                self->OnCopyEntryFailed();
              }
            }
          },
          weak_factory_.GetWeakPtr()));
}

void SqlSharedCache::OnCopyEntryComplete() {
  // Resource redirection via SqlPersistentStore::MoveBlobsToSharedCache and
  // Mojo client notifications will be hooked up in a follow-up CL.
  current_copy_row_id_ = std::nullopt;
  CopyNextEntry();
}

void SqlSharedCache::OnCopyEntryFailed() {
  if (current_copy_row_id_.has_value()) {
    isolated_database_.AsyncCall(&SqlSharedCacheIsolatedDatabase::DeleteEntry)
        .WithArgs(*current_copy_row_id_);
    current_copy_row_id_ = std::nullopt;
  }
  CopyNextEntry();
}

void SqlSharedCache::FinishCopy() {
  CHECK(copy_callback_);
  CHECK(!current_copy_row_id_);
  auto unprocessed_entries = std::move(pending_copy_entries_);
  CHECK(pending_copy_entries_.empty());
  copy_abort_flag_ = nullptr;
  auto callback = std::move(copy_callback_);
  CHECK(!copy_callback_);
  std::move(callback).Run(std::move(unprocessed_entries));
}

void SqlSharedCache::DeleteEntries(
    const std::vector<SqlSharedCacheRowId>& shared_cache_row_ids,
    base::OnceCallback<
        void(base::expected<void, SqlSharedCacheIsolatedDatabase::Error>)>
        callback) {
  if (!isolated_database_) {
    std::move(callback).Run(base::unexpected(
        SqlSharedCacheIsolatedDatabase::Error::kIsolatedDatabaseNotAvailable));
    return;
  }
  isolated_database_.AsyncCall(&SqlSharedCacheIsolatedDatabase::DeleteEntries)
      .WithArgs(shared_cache_row_ids)
      .Then(std::move(callback));
}

void SqlSharedCache::GetBlobHandle(
    const CacheEntryKey& entry_key,
    SqlSharedCacheRowId shared_cache_row_id,
    int body_size,
    base::OnceCallback<
        void(base::expected<scoped_refptr<SqlSharedCacheBlobHandle>,
                            SqlSharedCacheIsolatedDatabase::Error>)> callback) {
  if (!isolated_database_) {
    std::move(callback).Run(base::unexpected(
        SqlSharedCacheIsolatedDatabase::Error::kIsolatedDatabaseNotAvailable));
    return;
  }
  isolated_database_.AsyncCall(&SqlSharedCacheIsolatedDatabase::GetBlobHandle)
      .WithArgs(entry_key, shared_cache_row_id, body_size)
      .Then(std::move(callback));
}

}  // namespace disk_cache
