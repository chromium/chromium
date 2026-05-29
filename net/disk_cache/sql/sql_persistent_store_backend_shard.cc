// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store_backend_shard.h"

#include <set>
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
#include "base/types/expected.h"
#include "net/base/features.h"
#include "net/disk_cache/memory_entry_data_hints.h"
#include "net/disk_cache/sql/eviction_candidate_aggregator.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_persistent_store_backend.h"

namespace disk_cache {

SqlPersistentStore::BackendShard::BackendShard(
    ShardId shard_id,
    const base::FilePath& path,
    net::CacheType type,
    scoped_refptr<SqlReadCacheMemoryMonitor> read_cache_memory_monitor,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    SqlAsyncTaskManager& async_task_manager)
    : async_task_manager_(async_task_manager),
      backend_(background_task_runner,
               async_task_manager,
               shard_id,
               path,
               type,
               std::move(read_cache_memory_monitor)) {}

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
                                                 bool accept_index_mismatch,
                                                 ErrorCallback callback) {
  bool need_recovery_on_failure = false;
  if (index_.has_value()) {
    // If the in-memory index is available, synchronously remove the entry from
    // the index.
    if (index_->Remove(key.hash(), res_id)) {
      need_recovery_on_failure = true;
    } else if (!accept_index_mismatch) {
      RecordIndexMismatch(IndexMismatchLocation::kDoomEntry);
    }
  } else {
    // If the in-memory index is not available (e.g. it is being loaded or
    // moved to the backend during eviction), add to
    // `pending_doomed_hash_and_res_ids_` to be removed from the index once it
    // becomes available.
    pending_doomed_hash_and_res_ids_.push_back({key.hash(), res_id});
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
              // Note: Optimistic write failure may trigger a call to DoomEntry,
              // which occurs without exclusive control. In this case, if
              // eviction runs immediately after Backend::DoomEntry, the index
              // might be missing.
              if (need_recovery_on_failure && result.result != Error::kOk &&
                  result.result != Error::kNotFound &&
                  weak_ptr->index_.has_value()) {
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
      .Then(WrapUpdateLastUsedByKeyCallback(std::move(callback), key));
}

void SqlPersistentStore::BackendShard::WriteEntryDataAndMetadata(
    const CacheEntryKey& key,
    std::optional<ResId> res_id,
    std::optional<int64_t> old_body_end,
    EntryWriteBuffer buffer,
    base::Time last_used,
    const std::optional<MemoryEntryDataHints>& new_hints,
    scoped_refptr<net::IOBuffer> head_buffer,
    int64_t header_size_delta,
    bool doomed_new_entry,
    ResIdOrErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::WriteEntryDataAndMetadata)
      .WithArgs(key, res_id, old_body_end, std::move(buffer), last_used,
                new_hints, std::move(head_buffer), header_size_delta,
                doomed_new_entry, base::TimeTicks::Now())
      .Then(WrapCallbackWithStoreStatusAndIndexUpdate(
          std::move(callback), key,
          /*is_new_entry=*/!res_id.has_value(), new_hints,
          IndexMismatchLocation::kWriteEntryDataAndMetadata));
}

void SqlPersistentStore::BackendShard::WriteEntryData(
    const CacheEntryKey& key,
    const ResIdOrTime& res_id_or_last_used_time,
    int64_t old_body_end,
    EntryWriteBuffer buffer,
    bool truncate,
    bool doomed_new_entry,
    bool sparse_write,
    int64_t header_size,
    int64_t max_sparse_data_size,
    ResIdOrErrorCallback callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::WriteEntryData)
      .WithArgs(key, res_id_or_last_used_time, old_body_end, std::move(buffer),
                truncate, doomed_new_entry, sparse_write, header_size,
                max_sparse_data_size, base::TimeTicks::Now())
      .Then(WrapCallbackWithStoreStatusAndIndexUpdate(
          std::move(callback), key,
          /*is_new_entry=*/
          std::holds_alternative<base::Time>(res_id_or_last_used_time),
          /*new_hints=*/std::nullopt, IndexMismatchLocation::kWriteEntryData));
}

void SqlPersistentStore::BackendShard::ReadEntryData(
    const CacheEntryKey& key,
    ResId res_id,
    int64_t offset,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    int64_t body_end,
    bool sparse_reading,
    SqlPersistentStore::ReadResultOrErrorCallback callback) {
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
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
        remaining_mandatory_size,
    EvictionResultCallback callback) {
  EvictionResultWithMetadataCallback result_callback =
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&BackendShard::OnEvictionFinished,
                         weak_factory_.GetWeakPtr(), std::move(callback))
              .Then(base::OnceClosure(base::DoNothingWithBoundArgs(
                  async_task_manager_->StartTask()))));

  backend_.AsyncCall(&SqlPersistentStore::Backend::StartEviction)
      .WithArgs(size_to_be_removed, std::move(excluded_res_ids),
                is_idle_time_eviction, std::move(aggregator),
                std::move(abort_flag), std::move(remaining_mandatory_size),
                std::exchange(index_, std::nullopt),
                std::move(result_callback));
}

void SqlPersistentStore::BackendShard::ResumePendingEviction(
    base::flat_set<ResId> excluded_res_ids,
    bool is_idle_time_eviction,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
        remaining_mandatory_size,
    EvictionResultCallback callback) {
  if (pending_eviction_targets_.empty()) {
    std::move(callback).Run(
        EvictionResult(Error::kOk, /*evicted_entry_count=*/0));
    return;
  }
  backend_.AsyncCall(&SqlPersistentStore::Backend::ResumePendingEviction)
      .WithArgs(std::move(pending_eviction_targets_),
                std::move(excluded_res_ids), is_idle_time_eviction,
                std::move(abort_flag), std::move(remaining_mandatory_size),
                std::move(index_), base::TimeTicks::Now())
      .Then(base::BindOnce(&BackendShard::OnEvictionFinished,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
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
  CHECK(!index_.has_value());
  backend_.AsyncCall(&SqlPersistentStore::Backend::LoadInMemoryIndex)
      .Then(base::BindOnce(
          [](base::WeakPtr<BackendShard> weak_ptr, ErrorCallback callback,
             SqlPersistentStore::InMemoryIndexAndDoomedResIdsOrError result) {
            if (weak_ptr) {
              if (result.has_value()) {
                weak_ptr->index_ = std::move(result->index);
                std::set<ResId> doomed_res_id_set(
                    result->doomed_entry_res_ids.begin(),
                    result->doomed_entry_res_ids.end());
                for (const auto& hash_and_res_id :
                     weak_ptr->pending_doomed_hash_and_res_ids_) {
                  weak_ptr->index_->Remove(hash_and_res_id.hash,
                                           hash_and_res_id.res_id);
                  // If an entry is doomed while the index is being loaded, it
                  // should also be removed from the list of entries to be
                  // deleted if it was previously marked for deletion.
                  doomed_res_id_set.erase(hash_and_res_id.res_id);
                }
                weak_ptr->to_be_deleted_res_ids_.assign(
                    doomed_res_id_set.begin(), doomed_res_id_set.end());
                weak_ptr->pending_doomed_hash_and_res_ids_.clear();
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

void SqlPersistentStore::BackendShard::MaybeRunIncrementalVacuum(
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    base::OnceCallback<void(bool)> callback) {
  backend_.AsyncCall(&SqlPersistentStore::Backend::MaybeRunIncrementalVacuum)
      .WithArgs(std::move(abort_flag))
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

void SqlPersistentStore::BackendShard::SetEvictionHookForTesting(  // IN-TEST
    base::RepeatingClosure hook) {
  backend_.AsyncCall(&Backend::SetEvictionHookForTesting)
      .WithArgs(std::move(hook));
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
    CacheEntryKey::Hash hash,
    ResId res_id,
    MemoryEntryDataHints hints) {
  if (index_.has_value()) {
    index_->SetEntryDataHints(hash, res_id, hints);
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

base::OnceCallback<void(SqlPersistentStore::EntryMetadataOrErrorAndStoreStatus)>
SqlPersistentStore::BackendShard::WrapCallbackWithStoreStatusAndIndexUpdate(
    ResIdOrErrorCallback callback,
    const CacheEntryKey& key,
    bool is_new_entry,
    const std::optional<MemoryEntryDataHints>& new_hints,
    IndexMismatchLocation location) {
  return base::BindOnce(
      [](base::WeakPtr<BackendShard> weak_ptr, ResIdOrErrorCallback callback,
         CacheEntryKey::Hash key_hash, bool is_new_entry,
         const std::optional<MemoryEntryDataHints>& new_hints,
         IndexMismatchLocation location,
         EntryMetadataOrErrorAndStoreStatus result) {
        if (weak_ptr) {
          weak_ptr->store_status_ = result.store_status;
          if (result.result.has_value() && weak_ptr->index_) {
            if (is_new_entry) {
              if (!weak_ptr->index_->Insert(key_hash, result.result->res_id)) {
                weak_ptr->RecordIndexMismatch(location);
              }
            }
            if (new_hints) {
              weak_ptr->index_->SetEntryDataHints(
                  key_hash, result.result->res_id, *new_hints);
            }
            if (weak_ptr->index_->is_entry_metadata_ready()) {
              weak_ptr->index_->SetEntryLastUsedAndUsage(
                  key_hash, result.result->res_id, result.result->last_used,
                  result.result->bytes_usage);
            }
          }
          // We should not run the callback when `this` was deleted.
          std::move(callback).Run(
              result.result.has_value()
                  ? ResIdOrError(result.result->res_id)
                  : ResIdOrError(base::unexpected(result.result.error())));
        }
      },
      weak_factory_.GetWeakPtr(), std::move(callback), key.hash(), is_new_entry,
      new_hints, location);
}

base::OnceCallback<void(SqlPersistentStore::EntryMetadataOrError)>
SqlPersistentStore::BackendShard::WrapUpdateLastUsedByKeyCallback(
    ErrorCallback callback,
    const CacheEntryKey& key) {
  return base::BindOnce(
      [](base::WeakPtr<BackendShard> weak_ptr, ErrorCallback callback,
         CacheEntryKey::Hash key_hash, EntryMetadataOrError result) {
        if (weak_ptr) {
          if (result.has_value() && weak_ptr->index_.has_value() &&
              weak_ptr->index_->is_entry_metadata_ready()) {
            weak_ptr->index_->SetEntryLastUsed(key_hash, result->res_id,
                                               result->last_used);
          }
          // We should not run the callback when `this` was deleted.
          std::move(callback).Run(result.error_or(Error::kOk));
        }
      },
      weak_factory_.GetWeakPtr(), std::move(callback), key.hash());
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
              } else if (weak_ptr->index_->is_entry_metadata_ready()) {
                weak_ptr->index_->SetEntryLastUsed(
                    key_hash, result.result->res_id, result.result->last_used);
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

base::OnceCallback<
    void(SqlPersistentStore::HashAndResIdListOrErrorAndStoreStatus)>
SqlPersistentStore::BackendShard::WrapErrorCallbackToRemoveFromIndex(
    ErrorCallback callback,
    IndexMismatchLocation location) {
  return base::BindOnce(
      [](base::WeakPtr<BackendShard> weak_ptr, ErrorCallback callback,
         IndexMismatchLocation location,
         HashAndResIdListOrErrorAndStoreStatus result) {
        if (weak_ptr) {
          if (result.result.has_value() && weak_ptr->index_.has_value()) {
            for (const auto& hash_and_res_id : result.result.value()) {
              if (!weak_ptr->index_->Remove(hash_and_res_id.hash,
                                            hash_and_res_id.res_id)) {
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
    EvictionResultCallback callback,
    EvictionResultWithMetadata result) {
  if (result.index.has_value()) {
    index_ = std::move(result.index);
    for (const auto& hash_and_res_id : pending_doomed_hash_and_res_ids_) {
      index_->Remove(hash_and_res_id.hash, hash_and_res_id.res_id);
    }
    pending_doomed_hash_and_res_ids_.clear();
  }
  if (result.index_mismatch_detected) {
    RecordIndexMismatch(IndexMismatchLocation::kStartEviction);
  }
  pending_eviction_targets_ = std::move(result.pending_eviction_targets);
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
