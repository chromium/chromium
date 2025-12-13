// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store_backend_shard.h"

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "net/disk_cache/sql/eviction_candidate_aggregator.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_persistent_store_backend.h"

namespace disk_cache {

SqlPersistentStore::BackendShard::BackendShard(
    ShardId shard_id,
    const base::FilePath& path,
    net::CacheType type,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : backend_(background_task_runner, shard_id, path, type) {}

SqlPersistentStore::BackendShard::~BackendShard() = default;

// Kicks off the asynchronous initialization of the backend.
void SqlPersistentStore::BackendShard::Initialize(
    int64_t user_max_bytes,
    InitResultOrErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::Initialize)
      .WithArgs(user_max_bytes, base::TimeTicks::Now())
      .Then(base::BindOnce(
          [](base::WeakPtr<BackendShard> weak_ptr,
             InitResultOrErrorCallback callback, InitResultOrError result) {
            if (weak_ptr) {
              if (result.has_value()) {
                weak_ptr->store_status_ = result->store_status;
                if (result->in_memory_data) {
                  weak_ptr->index_ = std::move(result->in_memory_data->index);
                  weak_ptr->to_be_deleted_res_ids_ =
                      std::move(result->in_memory_data->doomed_entry_res_ids);
                }
              }
              std::move(callback).Run(std::move(result));
            }
          },
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SqlPersistentStore::BackendShard::OpenOrCreateEntry(
    const CacheEntryKey& key,
    EntryInfoOrErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::OpenOrCreateEntry)
      .WithArgs(key, base::TimeTicks::Now())
      .Then(WrapEntryInfoOrErrorCallback(
          std::move(callback), key, IndexMismatchLocation::kOpenOrCreateEntry));
}

void SqlPersistentStore::BackendShard::OpenEntry(
    const CacheEntryKey& key,
    OptionalEntryInfoOrErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::OpenEntry)
      .WithArgs(key, base::TimeTicks::Now())
      .Then(WrapCallback(std::move(callback)));
}

void SqlPersistentStore::BackendShard::CreateEntry(
    const CacheEntryKey& key,
    base::Time creation_time,
    EntryInfoOrErrorCallback callback) {
  bool run_existance_check = !index_ || index_->Contains(key.hash());
  backend_.AsyncCall(&SqlPersistentStore::Backend::CreateEntry)
      .WithArgs(key, creation_time, run_existance_check, base::TimeTicks::Now())
      .Then(WrapEntryInfoOrErrorCallback(std::move(callback), key,
                                         IndexMismatchLocation::kCreateEntry));
}

void SqlPersistentStore::BackendShard::DoomEntry(const CacheEntryKey& key,
                                                 ResId res_id,
                                                 ErrorCallback callback) {
  bool need_recovery_on_failure = false;
  if (index_.has_value()) {
    // If the in-memory index is available, synchronously remove the entry from
    // the index.
    if (index_->Remove(key.hash(), res_id)) {
      need_recovery_on_failure = true;
    } else {
      RecordIndexMismatch(IndexMismatchLocation::kDoomEntry);
    }
  } else if (loading_index_) {
    // If the in-memory index is being loaded, add to `pending_doomed_res_ids_`
    // to be removed from the index upon completion of the index loading.
    pending_doomed_res_ids_.emplace_back(res_id);
  }
  backend_.AsyncCall(&SqlPersistentStore::Backend::DoomEntry)
      .WithArgs(key, res_id, base::TimeTicks::Now())
      .Then(base::BindOnce(
          [](base::WeakPtr<BackendShard> weak_ptr,
             bool need_recovery_on_failure, CacheEntryKey::Hash hash,
             ResId res_id, ErrorCallback callback, ErrorAndStoreStatus result) {
            if (weak_ptr) {
              // If the DoomEntry operation fails in the database, the entry
              // needs to be re-inserted into the in-memory index to maintain
              // consistency.
              if (need_recovery_on_failure && result.result != Error::kOk &&
                  result.result != Error::kNotFound) {
                weak_ptr->index_->Insert(hash, res_id);
              }
              weak_ptr->store_status_ = result.store_status;
              // We should not run the callback when `this` was deleted.
              std::move(callback).Run(std::move(result.result));
            }
          },
          weak_factory_.GetWeakPtr(), need_recovery_on_failure, key.hash(),
          res_id, std::move(callback)));
}

void SqlPersistentStore::BackendShard::DeleteDoomedEntry(
    const CacheEntryKey& key,
    ResId res_id,
    ErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::DeleteDoomedEntry)
      .WithArgs(key, res_id, base::TimeTicks::Now())
      .Then(WrapCallbackWithStoreStatus(std::move(callback)));
}

void SqlPersistentStore::BackendShard::DeleteLiveEntry(const CacheEntryKey& key,
                                                       ErrorCallback callback) {
  // If the entry is not in the in-memory index, we can skip the DB lookup.
  if (GetIndexStateForHash(key.hash()) == IndexState::kHashNotFound) {
    std::move(callback).Run(Error::kNotFound);
    return;
  }
  backend_.AsyncCall(&SqlPersistentStore::Backend::DeleteLiveEntry)
      .WithArgs(key, base::TimeTicks::Now())
      .Then(WrapErrorCallbackToRemoveFromIndex(
          std::move(callback), IndexMismatchLocation::kDeleteLiveEntry));
}

void SqlPersistentStore::BackendShard::DeleteAllEntries(
    ErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::DeleteAllEntries)
      .WithArgs(base::TimeTicks::Now())
      .Then(base::BindOnce(
          [](base::WeakPtr<BackendShard> weak_ptr, ErrorCallback callback,
             ErrorAndStoreStatus result) {
            if (weak_ptr) {
              if (result.result == Error::kOk && weak_ptr->index_.has_value()) {
                weak_ptr->index_->Clear();
              }
              weak_ptr->store_status_ = result.store_status;
              // We should not run the callback when `this` was deleted.
              std::move(callback).Run(std::move(result.result));
            }
          },
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SqlPersistentStore::BackendShard::DeleteLiveEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    base::flat_set<ResId> excluded_res_ids,
    ErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::DeleteLiveEntriesBetween)
      .WithArgs(initial_time, end_time, std::move(excluded_res_ids),
                base::TimeTicks::Now())
      .Then(WrapErrorCallbackToRemoveFromIndex(
          std::move(callback),
          IndexMismatchLocation::kDeleteLiveEntriesBetween));
}

void SqlPersistentStore::BackendShard::UpdateEntryLastUsedByKey(
    const CacheEntryKey& key,
    base::Time last_used,
    ErrorCallback callback) {
  // If the entry is not in the in-memory index, we can skip the DB lookup.
  if (GetIndexStateForHash(key.hash()) == IndexState::kHashNotFound) {
    std::move(callback).Run(Error::kNotFound);
    return;
  }
  backend_.AsyncCall(&SqlPersistentStore::Backend::UpdateEntryLastUsedByKey)
      .WithArgs(key, last_used, base::TimeTicks::Now())
      .Then(WrapCallback(std::move(callback)));
}

void SqlPersistentStore::BackendShard::UpdateEntryLastUsedByResId(
    ResId res_id,
    base::Time last_used,
    ErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::UpdateEntryLastUsedByResId)
      .WithArgs(res_id, last_used, base::TimeTicks::Now())
      .Then(WrapCallback(std::move(callback)));
}

void SqlPersistentStore::BackendShard::UpdateEntryHeaderAndLastUsed(
    const CacheEntryKey& key,
    ResId res_id,
    base::Time last_used,
    const std::optional<MemoryEntryDataHints>& new_hints,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t header_size_delta,
    ErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::UpdateEntryHeaderAndLastUsed)
      .WithArgs(key, res_id, last_used, new_hints, std::move(buffer),
                header_size_delta, base::TimeTicks::Now())
      .Then(WrapCallbackWithStoreStatus(std::move(callback)));
}

void SqlPersistentStore::BackendShard::WriteEntryData(
    const CacheEntryKey& key,
    ResId res_id,
    int64_t old_body_end,
    int64_t offset,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    bool truncate,
    ErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::WriteEntryData)
      .WithArgs(key, res_id, old_body_end, offset, std::move(buffer), buf_len,
                truncate, base::TimeTicks::Now())
      .Then(WrapCallbackWithStoreStatus(std::move(callback)));
}

void SqlPersistentStore::BackendShard::ReadEntryData(
    const CacheEntryKey& key,
    ResId res_id,
    int64_t offset,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    int64_t body_end,
    bool sparse_reading,
    SqlPersistentStore::IntOrErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::ReadEntryData)
      .WithArgs(key, res_id, offset, std::move(buffer), buf_len, body_end,
                sparse_reading, base::TimeTicks::Now())
      .Then(WrapCallback(std::move(callback)));
}

void SqlPersistentStore::BackendShard::GetEntryAvailableRange(
    const CacheEntryKey& key,
    ResId res_id,
    int64_t offset,
    int len,
    RangeResultCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::GetEntryAvailableRange)
      .WithArgs(res_id, offset, len, base::TimeTicks::Now())
      .Then(WrapCallback(std::move(callback)));
}

void SqlPersistentStore::BackendShard::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    Int64OrErrorCallback callback) {
  backend_
      .AsyncCall(&SqlPersistentStore::Backend::CalculateSizeOfEntriesBetween)
      .WithArgs(initial_time, end_time, base::TimeTicks::Now())
      .Then(WrapCallback(std::move(callback)));
}

void SqlPersistentStore::BackendShard::OpenNextEntry(
    const EntryIterator& iterator,
    OptionalEntryInfoWithKeyAndIteratorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::OpenNextEntry)
      .WithArgs(iterator, base::TimeTicks::Now())
      .Then(WrapCallback(std::move(callback)));
}

void SqlPersistentStore::BackendShard::StartEviction(
    int64_t size_to_be_removed,
    base::flat_set<ResId> excluded_res_ids,
    bool is_idle_time_eviction,
    scoped_refptr<EvictionCandidateAggregator> aggregator,
    ResIdListOrErrorCallback callback) {
  ResIdListOrErrorAndStoreStatusCallback result_callback =
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&BackendShard::OnEvictionFinished,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
  backend_.AsyncCall(&SqlPersistentStore::Backend::StartEviction)
      .WithArgs(size_to_be_removed, std::move(excluded_res_ids),
                is_idle_time_eviction, std::move(aggregator),
                std::move(result_callback));
}

int32_t SqlPersistentStore::BackendShard::GetEntryCount() const {
  return store_status_.entry_count;
}

void SqlPersistentStore::BackendShard::GetEntryCountAsync(
    Int32Callback callback) const {
  backend_.AsyncCall(&SqlPersistentStore::Backend::GetEntryCount)
      .Then(std::move(callback));
}

int64_t SqlPersistentStore::BackendShard::GetSizeOfAllEntries() const {
  return store_status_.GetEstimatedDiskUsage();
}

void SqlPersistentStore::BackendShard::LoadInMemoryIndex(
    ErrorCallback callback) {
  CHECK(!loading_index_);
  CHECK(!index_.has_value());
  loading_index_ = true;
  backend_.AsyncCall(&SqlPersistentStore::Backend::LoadInMemoryIndex)
      .Then(base::BindOnce(
          [](base::WeakPtr<BackendShard> weak_ptr, ErrorCallback callback,
             SqlPersistentStore::InMemoryIndexAndDoomedResIdsOrError result) {
            if (weak_ptr) {
              if (result.has_value()) {
                weak_ptr->index_ = std::move(result->index);
                weak_ptr->to_be_deleted_res_ids_ =
                    std::move(result->doomed_entry_res_ids);
                weak_ptr->loading_index_ = false;
                for (auto doomed_res_id : weak_ptr->pending_doomed_res_ids_) {
                  weak_ptr->index_->Remove(doomed_res_id);
                }
                weak_ptr->pending_doomed_res_ids_.clear();
              }
              std::move(callback).Run(result.has_value() ? Error::kOk
                                                         : result.error());
            }
          },
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

bool SqlPersistentStore::BackendShard::MaybeRunCleanupDoomedEntries(
    ErrorCallback callback) {
  if (to_be_deleted_res_ids_.empty()) {
    return false;
  }
  backend_.AsyncCall(&SqlPersistentStore::Backend::DeleteDoomedEntries)
      .WithArgs(std::move(to_be_deleted_res_ids_), base::TimeTicks::Now())
      .Then(WrapCallback(std::move(callback)));
  return true;
}

void SqlPersistentStore::BackendShard::MaybeRunCheckpoint(
    base::OnceCallback<void(bool)> callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::MaybeRunCheckpoint)
      .Then(std::move(callback));
}

void SqlPersistentStore::BackendShard::EnableStrictCorruptionCheckForTesting() {
  strict_corruption_check_enabled_ = true;
  backend_.AsyncCall(
      &SqlPersistentStore::Backend::EnableStrictCorruptionCheckForTesting);
}

void SqlPersistentStore::BackendShard::SetSimulateDbFailureForTesting(
    bool fail) {
  backend_
      .AsyncCall(&SqlPersistentStore::Backend::SetSimulateDbFailureForTesting)
      .WithArgs(fail);
}

void SqlPersistentStore::BackendShard::RazeAndPoisonForTesting() {
  backend_.AsyncCall(&SqlPersistentStore::Backend::RazeAndPoisonForTesting);
}

SqlPersistentStore::IndexState
SqlPersistentStore::BackendShard::GetIndexStateForHash(
    CacheEntryKey::Hash key_hash) const {
  if (!index_.has_value()) {
    return IndexState::kNotReady;
  }

  if (index_->Contains(key_hash)) {
    return IndexState::kHashFound;
  }
  return IndexState::kHashNotFound;
}

void SqlPersistentStore::BackendShard::SetInMemoryEntryDataHints(
    ResId res_id,
    MemoryEntryDataHints hints) {
  if (index_.has_value()) {
    index_->SetEntryDataHints(res_id, hints);
  }
}

std::optional<MemoryEntryDataHints>
SqlPersistentStore::BackendShard::GetInMemoryEntryDataHints(
    CacheEntryKey::Hash key_hash) const {
  return index_.has_value() ? index_->GetEntryDataHints(key_hash)
                            : std::nullopt;
}

std::optional<SqlPersistentStore::ResId>
SqlPersistentStore::BackendShard::TryGetSingleResIdFromInMemoryIndex(
    CacheEntryKey::Hash key_hash) const {
  return index_.has_value() ? index_->TryGetSingleResId(key_hash)
                            : std::nullopt;
}

// Like `WrapCallback`, but also updates the `store_status_`.
base::OnceCallback<void(SqlPersistentStore::ErrorAndStoreStatus)>
SqlPersistentStore::BackendShard::WrapCallbackWithStoreStatus(
    ErrorCallback callback) {
  return base::BindOnce(
      [](base::WeakPtr<BackendShard> weak_ptr, ErrorCallback callback,
         ErrorAndStoreStatus result) {
        if (weak_ptr) {
          weak_ptr->store_status_ = result.store_status;
          // We should not run the callback when `this` was deleted.
          std::move(callback).Run(std::move(result.result));
        }
      },
      weak_factory_.GetWeakPtr(), std::move(callback));
}

base::OnceCallback<void(SqlPersistentStore::EntryInfoOrErrorAndStoreStatus)>
SqlPersistentStore::BackendShard::WrapEntryInfoOrErrorCallback(
    EntryInfoOrErrorCallback callback,
    const CacheEntryKey& key,
    IndexMismatchLocation location) {
  return base::BindOnce(
      [](base::WeakPtr<BackendShard> weak_ptr,
         EntryInfoOrErrorCallback callback, CacheEntryKey::Hash key_hash,
         IndexMismatchLocation location,
         EntryInfoOrErrorAndStoreStatus result) {
        if (weak_ptr) {
          if (result.result.has_value() && weak_ptr->index_.has_value()) {
            if (!result.result->opened) {
              if (!weak_ptr->index_->Insert(key_hash, result.result->res_id)) {
                weak_ptr->RecordIndexMismatch(location);
              }
            }
          }
          weak_ptr->store_status_ = result.store_status;
          // We should not run the callback when `this` was deleted.
          std::move(callback).Run(std::move(result.result));
        }
      },
      weak_factory_.GetWeakPtr(), std::move(callback), key.hash(), location);
}

base::OnceCallback<void(SqlPersistentStore::ResIdListOrErrorAndStoreStatus)>
SqlPersistentStore::BackendShard::WrapErrorCallbackToRemoveFromIndex(
    ErrorCallback callback,
    IndexMismatchLocation location) {
  return base::BindOnce(
      [](base::WeakPtr<BackendShard> weak_ptr, ErrorCallback callback,
         IndexMismatchLocation location,
         ResIdListOrErrorAndStoreStatus result) {
        if (weak_ptr) {
          if (result.result.has_value() && weak_ptr->index_.has_value()) {
            for (ResId res_id : result.result.value()) {
              if (!weak_ptr->index_->Remove(res_id)) {
                weak_ptr->RecordIndexMismatch(location);
              }
            }
          }
          weak_ptr->store_status_ = result.store_status;
          // We should not run the callback when `this` was deleted.
          std::move(callback).Run(
              std::move(result.result.error_or(Error::kOk)));
        }
      },
      weak_factory_.GetWeakPtr(), std::move(callback), location);
}

void SqlPersistentStore::BackendShard::OnEvictionFinished(
    ResIdListOrErrorCallback callback,
    ResIdListOrErrorAndStoreStatus result) {
  if (result.result.has_value() && index_.has_value()) {
    for (ResId res_id : *result.result) {
      if (!index_->Remove(res_id)) {
        RecordIndexMismatch(IndexMismatchLocation::kStartEviction);
      }
    }
  }
  store_status_ = result.store_status;
  std::move(callback).Run(std::move(result.result));
}

void SqlPersistentStore::BackendShard::RecordIndexMismatch(
    IndexMismatchLocation location) {
  base::UmaHistogramEnumeration(
      base::StrCat({kSqlDiskCacheBackendHistogramPrefix, "IndexMismatch"}),
      location);
  CHECK(!strict_corruption_check_enabled_);
}

}  // namespace disk_cache
