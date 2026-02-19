// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "net/base/cache_type.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/sql/cache_entry_key.h"
#include "net/disk_cache/sql/eviction_candidate_aggregator.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_persistent_store_backend.h"
#include "net/disk_cache/sql/sql_persistent_store_backend_shard.h"
#include "net/disk_cache/sql/sql_read_cache_memory_monitor.h"

namespace disk_cache {
namespace {

std::vector<base::flat_set<SqlPersistentStore::ResId>> GroupResIdPerShardId(
    std::vector<SqlPersistentStore::ResIdAndShardId> excluded_list,
    size_t size_of_shards) {
  std::vector<std::vector<SqlPersistentStore::ResId>> res_id_lists(
      size_of_shards);
  for (const auto& res_id_and_shard_id : excluded_list) {
    res_id_lists[res_id_and_shard_id.shard_id.value()].emplace_back(
        res_id_and_shard_id.res_id);
  }
  std::vector<base::flat_set<SqlPersistentStore::ResId>> res_id_sets;
  for (size_t i = 0; i < size_of_shards; ++i) {
    std::sort(res_id_lists[i].begin(), res_id_lists[i].end());
    res_id_sets.emplace_back(base::sorted_unique, std::move(res_id_lists[i]));
  }
  return res_id_sets;
}

// Calculates the maximum size for a single cache entry's data.
int64_t CalculateMaxFileSize(int64_t max_bytes) {
  return std::max(base::saturated_cast<int64_t>(
                      max_bytes / kSqlBackendMaxFileRatioDenominator),
                  kSqlBackendMinFileSizeLimit);
}

// Appends `result` to `results` and returns `results`.
// This is used as a helper for base::BindOnce to chain callbacks.
std::vector<bool> AppendResult(std::vector<bool> results, bool result) {
  results.push_back(result);
  return results;
}

void RecordEvictionHistograms(std::string_view method_name,
                              SqlPersistentStore::Error error,
                              base::TimeTicks start_time,
                              size_t entry_count) {
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({kSqlDiskCacheBackendHistogramPrefix, method_name,
                    error == SqlPersistentStore::Error::kOk ? ".SuccessTime"
                                                            : ".FailureTime"}),
      base::TimeTicks::Now() - start_time);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {kSqlDiskCacheBackendHistogramPrefix, method_name, ".Result"}),
      error);
  if (error == SqlPersistentStore::Error::kOk) {
    base::UmaHistogramCounts1000(
        base::StrCat(
            {kSqlDiskCacheBackendHistogramPrefix, method_name, ".EntryCount"}),
        entry_count);
  }
}

}  // namespace

// static
std::vector<std::unique_ptr<SqlPersistentStore::BackendShard>>
SqlPersistentStore::CreateBackendShards(
    const base::FilePath& path,
    net::CacheType type,
    std::vector<scoped_refptr<base::SequencedTaskRunner>>
        background_task_runners) {
  const size_t num_shards = background_task_runners.size();
  CHECK(num_shards < std::numeric_limits<ShardId::underlying_type>::max());
  std::vector<std::unique_ptr<BackendShard>> backend_shards;
  backend_shards.reserve(num_shards);
  auto read_cache_memory_monitor =
      base::MakeRefCounted<SqlReadCacheMemoryMonitor>(
          net::features::kSqlDiskCacheMaxReadBufferTotalSize.Get());
  for (size_t i = 0; i < num_shards; ++i) {
    backend_shards.emplace_back(std::make_unique<BackendShard>(
        ShardId(i), path, type, read_cache_memory_monitor,
        background_task_runners[i]));
  }
  return backend_shards;
}

SqlPersistentStore::SqlPersistentStore(
    const base::FilePath& path,
    int64_t max_bytes,
    net::CacheType type,
    std::vector<scoped_refptr<base::SequencedTaskRunner>>
        background_task_runners)
    : background_task_runners_(std::move(background_task_runners)),
      backend_shards_(
          CreateBackendShards(path, type, background_task_runners_)),
      user_max_bytes_(max_bytes) {}
SqlPersistentStore::~SqlPersistentStore() = default;

void SqlPersistentStore::Initialize(ErrorCallback callback) {
  auto barrier_callback = base::BarrierCallback<InitResultOrError>(
      GetSizeOfShards(),
      base::BindOnce(&SqlPersistentStore::OnInitializeFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  for (const auto& backend_shard : backend_shards_) {
    backend_shard->Initialize(user_max_bytes_, barrier_callback);
  }
}

void SqlPersistentStore::OpenOrCreateEntry(const CacheEntryKey& key,
                                           EntryInfoOrErrorCallback callback) {
  GetShard(key).OpenOrCreateEntry(key, std::move(callback));
}

void SqlPersistentStore::OpenEntry(const CacheEntryKey& key,
                                   OptionalEntryInfoOrErrorCallback callback) {
  GetShard(key).OpenEntry(key, std::move(callback));
}

void SqlPersistentStore::CreateEntry(const CacheEntryKey& key,
                                     base::Time creation_time,
                                     EntryInfoOrErrorCallback callback) {
  GetShard(key).CreateEntry(key, creation_time, std::move(callback));
}

void SqlPersistentStore::DoomEntry(const CacheEntryKey& key,
                                   ResId res_id,
                                   bool accept_index_mismatch,
                                   ErrorCallback callback) {
  GetShard(key).DoomEntry(key, res_id, accept_index_mismatch,
                          std::move(callback));
}

void SqlPersistentStore::DeleteDoomedEntry(const CacheEntryKey& key,
                                           ResId res_id,
                                           ErrorCallback callback) {
  GetShard(key).DeleteDoomedEntry(key, res_id, std::move(callback));
}

void SqlPersistentStore::DeleteLiveEntry(const CacheEntryKey& key,
                                         ErrorCallback callback) {
  GetShard(key).DeleteLiveEntry(key, std::move(callback));
}

void SqlPersistentStore::DeleteAllEntries(ErrorCallback callback) {
  auto barrier_callback = CreateBarrierErrorCallback(std::move(callback));
  for (const auto& backend_shard : backend_shards_) {
    backend_shard->DeleteAllEntries(barrier_callback);
  }
}

void SqlPersistentStore::DeleteLiveEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    std::vector<ResIdAndShardId> excluded_list,
    ErrorCallback callback) {
  auto barrier_callback = CreateBarrierErrorCallback(std::move(callback));
  auto res_id_sets =
      GroupResIdPerShardId(std::move(excluded_list), GetSizeOfShards());
  for (size_t i = 0; i < GetSizeOfShards(); ++i) {
    backend_shards_[i]->DeleteLiveEntriesBetween(
        initial_time, end_time, std::move(res_id_sets[i]), barrier_callback);
  }
}

void SqlPersistentStore::UpdateEntryLastUsedByKey(const CacheEntryKey& key,
                                                  base::Time last_used,
                                                  ErrorCallback callback) {
  GetShard(key).UpdateEntryLastUsedByKey(key, last_used, std::move(callback));
}

void SqlPersistentStore::WriteEntryDataAndMetadata(
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
  GetShard(key).WriteEntryDataAndMetadata(
      key, res_id, old_body_end, std::move(buffer), last_used, new_hints,
      std::move(head_buffer), header_size_delta, doomed_new_entry,
      std::move(callback));
}

void SqlPersistentStore::WriteEntryData(
    const CacheEntryKey& key,
    const ResIdOrTime& res_id_or_last_used_time,
    int64_t old_body_end,
    EntryWriteBuffer buffer,
    bool truncate,
    bool doomed_new_entry,
    ResIdOrErrorCallback callback) {
  GetShard(key).WriteEntryData(key, res_id_or_last_used_time, old_body_end,
                               std::move(buffer), truncate, doomed_new_entry,
                               std::move(callback));
}

void SqlPersistentStore::ReadEntryData(const CacheEntryKey& key,
                                       ResId res_id,
                                       int64_t offset,
                                       scoped_refptr<net::IOBuffer> buffer,
                                       int buf_len,
                                       int64_t body_end,
                                       bool sparse_reading,
                                       ReadResultOrErrorCallback callback) {
  GetShard(key).ReadEntryData(key, res_id, offset, std::move(buffer), buf_len,
                              body_end, sparse_reading, std::move(callback));
}

void SqlPersistentStore::GetEntryAvailableRange(const CacheEntryKey& key,
                                                ResId res_id,
                                                int64_t offset,
                                                int len,
                                                RangeResultCallback callback) {
  GetShard(key).GetEntryAvailableRange(key, res_id, offset, len,
                                       std::move(callback));
}

void SqlPersistentStore::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    Int64OrErrorCallback callback) {
  auto barrier_callback = base::BarrierCallback<Int64OrError>(
      GetSizeOfShards(),
      base::BindOnce(
          [](Int64OrErrorCallback callback, std::vector<Int64OrError> results) {
            int64_t total_size = 0;
            for (const auto& result : results) {
              if (!result.has_value()) {
                std::move(callback).Run(base::unexpected(result.error()));
                return;
              }
              total_size += result.value();
            }
            std::move(callback).Run(total_size);
          },
          std::move(callback)));
  for (const auto& backend_shard : backend_shards_) {
    backend_shard->CalculateSizeOfEntriesBetween(initial_time, end_time,
                                                 barrier_callback);
  }
}

void SqlPersistentStore::OpenNextEntry(
    const EntryIterator& iterator,
    OptionalEntryInfoWithKeyAndIteratorCallback callback) {
  if (iterator.value().shard_id.value() >= GetSizeOfShards()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  backend_shards_[iterator.value().shard_id.value()]->OpenNextEntry(
      iterator,
      base::BindOnce(
          [](base::WeakPtr<SqlPersistentStore> weak_ptr, ShardId shard_id,
             OptionalEntryInfoWithKeyAndIteratorCallback callback,
             OptionalEntryInfoWithKeyAndIterator result) {
            if (!weak_ptr) {
              return;
            }
            if (result.has_value()) {
              std::move(callback).Run(std::move(result));
              return;
            }
            EntryIterator new_iterator;
            new_iterator.value().shard_id = ShardId(shard_id.value() + 1);
            weak_ptr->OpenNextEntry(new_iterator, std::move(callback));
          },
          weak_factory_.GetWeakPtr(), iterator.value().shard_id,
          std::move(callback)));
}

SqlPersistentStore::EvictionUrgency SqlPersistentStore::GetEvictionUrgency() {
  if (eviction_in_progress_) {
    return EvictionUrgency::kNotNeeded;
  }
  if (HasPendingEviction()) {
    return EvictionUrgency::kNeeded;
  }
  // Checks if the total size of entries exceeds the high watermark and the
  // database is open, to determine if eviction should be initiated.
  const int64_t current_size = GetSizeOfAllEntries();
  if (current_size > high_watermark_) {
    return EvictionUrgency::kNeeded;
  }
  if (current_size > idle_time_high_watermark_) {
    return EvictionUrgency::kIdleTime;
  }
  return EvictionUrgency::kNotNeeded;
}

void SqlPersistentStore::StartEviction(
    std::vector<ResIdAndShardId> excluded_list,
    bool is_idle_time_eviction,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> eviction_abort_flag,
    ErrorCallback callback) {
  CHECK(!eviction_in_progress_);
  eviction_in_progress_ = true;
  auto new_callback = base::BindOnce(
      [](base::WeakPtr<SqlPersistentStore> weak_ptr, ErrorCallback callback,
         Error result) {
        if (weak_ptr) {
          weak_ptr->eviction_in_progress_ = false;
        }
        std::move(callback).Run(result);
      },
      weak_factory_.GetWeakPtr(), std::move(callback));
  MaybeLoadInMemoryIndex(base::BindOnce(
      &SqlPersistentStore::StartEvictionInternal, weak_factory_.GetWeakPtr(),
      std::move(excluded_list), is_idle_time_eviction,
      std::move(eviction_abort_flag), std::move(new_callback)));
}

void SqlPersistentStore::StartEvictionInternal(
    std::vector<ResIdAndShardId> excluded_list,
    bool is_idle_time_eviction,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> eviction_abort_flag,
    ErrorCallback callback,
    Error load_index_result) {
  if (load_index_result != Error::kOk) {
    std::move(callback).Run(load_index_result);
    return;
  }
  auto excluded_res_id_sets =
      GroupResIdPerShardId(std::move(excluded_list), GetSizeOfShards());
  if (HasPendingEviction()) {
    ResumePendingEviction(std::move(excluded_res_id_sets),
                          is_idle_time_eviction, std::move(eviction_abort_flag),
                          std::move(callback));
  } else {
    StartNewEviction(std::move(excluded_res_id_sets), is_idle_time_eviction,
                     std::move(eviction_abort_flag), std::move(callback));
  }
}

void SqlPersistentStore::ResumePendingEviction(
    std::vector<base::flat_set<SqlPersistentStore::ResId>> excluded_res_id_sets,
    bool is_idle_time_eviction,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> eviction_abort_flag,
    ErrorCallback callback) {
  const auto size_of_all_entries = GetSizeOfAllEntries();
  // If the size is less than the high watermark and the abort flag is already
  // true, return OK before posting tasks to each shard.
  if (size_of_all_entries <= high_watermark_ &&
      eviction_abort_flag->data.load(std::memory_order_relaxed)) {
    std::move(callback).Run(Error::kOk);
    return;
  }
  auto remaining_mandatory_size =
      base::MakeRefCounted<base::RefCountedData<std::atomic_int64_t>>(
          std::in_place,
          std::max<int64_t>(size_of_all_entries - high_watermark_, 0));
  auto barrier_callback = base::BarrierCallback<ResIdListOrError>(
      GetSizeOfShards(),
      base::BindOnce(&SqlPersistentStore::OnPendingEvictionFinished,
                     weak_factory_.GetWeakPtr(), excluded_res_id_sets,
                     is_idle_time_eviction, eviction_abort_flag,
                     base::TimeTicks::Now(), std::move(callback)));
  for (size_t i = 0; i < GetSizeOfShards(); ++i) {
    backend_shards_[i]->ResumePendingEviction(
        std::move(excluded_res_id_sets[i]), is_idle_time_eviction,
        eviction_abort_flag, remaining_mandatory_size, barrier_callback);
  }
}

void SqlPersistentStore::OnPendingEvictionFinished(
    std::vector<base::flat_set<SqlPersistentStore::ResId>> excluded_res_id_sets,
    bool is_idle_time_eviction,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> eviction_abort_flag,
    base::TimeTicks start_time,
    ErrorCallback callback,
    std::vector<ResIdListOrError> results) {
  Error error = Error::kOk;
  size_t count = 0;
  for (const auto& result : results) {
    if (!result.has_value()) {
      error = result.error();
      break;
    }
    count += result.value().size();
  }
  RecordEvictionHistograms(
      is_idle_time_eviction ? "ResumeEvictionOnIdleTime" : "ResumeEviction",
      error, start_time, count);

  if (error != Error::kOk || HasPendingEviction()) {
    std::move(callback).Run(error);
    return;
  }
  StartNewEviction(std::move(excluded_res_id_sets), is_idle_time_eviction,
                   std::move(eviction_abort_flag), std::move(callback));
}

void SqlPersistentStore::StartNewEviction(
    std::vector<base::flat_set<SqlPersistentStore::ResId>> excluded_res_id_sets,
    bool is_idle_time_eviction,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> eviction_abort_flag,
    ErrorCallback callback) {
  CHECK(!eviction_result_callback_);
  CHECK(eviction_abort_flag);
  CHECK(callback);
  const auto size_of_all_entries = GetSizeOfAllEntries();
  if (size_of_all_entries <=
      (is_idle_time_eviction ? idle_time_high_watermark_ : high_watermark_)) {
    std::move(callback).Run(Error::kOk);
    return;
  }
  const int64_t size_to_be_removed = size_of_all_entries - low_watermark_;
  CHECK(size_to_be_removed > 0);
  auto remaining_mandatory_size =
      base::MakeRefCounted<base::RefCountedData<std::atomic_int64_t>>(
          std::in_place,
          std::max<int64_t>(size_of_all_entries - high_watermark_, 0));
  eviction_result_callback_ = std::move(callback);
  auto barrier_callback = base::BarrierCallback<ResIdListOrError>(
      GetSizeOfShards(),
      base::BindOnce(&SqlPersistentStore::OnEvictionFinished,
                     weak_factory_.GetWeakPtr(), is_idle_time_eviction,
                     base::TimeTicks::Now()));
  auto aggregator = base::MakeRefCounted<EvictionCandidateAggregator>(
      size_to_be_removed, background_task_runners_);
  for (size_t i = 0; i < GetSizeOfShards(); ++i) {
    backend_shards_[i]->StartEviction(
        size_to_be_removed, std::move(excluded_res_id_sets[i]),
        is_idle_time_eviction, aggregator, eviction_abort_flag,
        remaining_mandatory_size, barrier_callback);
  }
}

void SqlPersistentStore::OnEvictionFinished(
    bool is_idle_time_eviction,
    base::TimeTicks start_time,
    std::vector<ResIdListOrError> results) {
  Error error = Error::kOk;
  size_t count = 0;
  for (const auto& result : results) {
    if (!result.has_value()) {
      error = result.error();
      break;
    }
    count += result.value().size();
  }

  RecordEvictionHistograms(
      is_idle_time_eviction ? "RunNewEvictionOnIdleTime" : "RunNewEviction",
      error, start_time, count);

  CHECK(eviction_result_callback_);
  auto callback = std::move(eviction_result_callback_);
  eviction_result_callback_.Reset();
  std::move(callback).Run(error);
}

bool SqlPersistentStore::HasPendingEviction() const {
  return std::ranges::any_of(backend_shards_, [](const auto& backend_shard) {
    return backend_shard->HasPendingEviction();
  });
}

int64_t SqlPersistentStore::MaxFileSize() const {
  return max_file_size_;
}

int64_t SqlPersistentStore::MaxSize() const {
  return max_bytes_;
}

int32_t SqlPersistentStore::GetEntryCount() const {
  base::ClampedNumeric<int32_t> result;
  for (const auto& backend_shard : backend_shards_) {
    result += backend_shard->GetEntryCount();
  }
  return result;
}

void SqlPersistentStore::GetEntryCountAsync(Int32Callback callback) const {
  auto barrier_callback = base::BarrierCallback<int32_t>(
      GetSizeOfShards(),
      base::BindOnce(
          [](Int32Callback callback, const std::vector<int32_t>& results) {
            int32_t total_count = 0;
            for (const auto& result : results) {
              total_count += result;
            }
            std::move(callback).Run(total_count);
          },
          std::move(callback)));
  for (const auto& backend_shard : backend_shards_) {
    backend_shard->GetEntryCountAsync(barrier_callback);
  }
}

int64_t SqlPersistentStore::GetSizeOfAllEntries() const {
  base::ClampedNumeric<int64_t> result;
  for (const auto& backend_shard : backend_shards_) {
    result += backend_shard->GetSizeOfAllEntries();
  }
  return result;
}

void SqlPersistentStore::MaybeLoadInMemoryIndex(ErrorCallback callback) {
  if (in_memory_load_result_.has_value()) {
    std::move(callback).Run(*in_memory_load_result_);
    return;
  }
  pending_in_memory_load_result_callbacks_.push_back(std::move(callback));
  if (!in_memory_load_triggered_) {
    in_memory_load_triggered_ = true;
    auto barrier_callback = CreateBarrierErrorCallback(
        base::BindOnce(&SqlPersistentStore::OnLoadInMemoryIndexFinished,
                       weak_factory_.GetWeakPtr()));
    for (const auto& backend_shard : backend_shards_) {
      backend_shard->LoadInMemoryIndex(barrier_callback);
    }
  }
}

void SqlPersistentStore::OnLoadInMemoryIndexFinished(Error result) {
  CHECK(!in_memory_load_result_.has_value());
  in_memory_load_result_ = result;
  auto callbacks = std::move(pending_in_memory_load_result_callbacks_);
  pending_in_memory_load_result_callbacks_.clear();
  for (auto& callback : callbacks) {
    std::move(callback).Run(result);
  }
}

bool SqlPersistentStore::MaybeRunCleanupDoomedEntries(ErrorCallback callback) {
  auto barrier_callback = CreateBarrierErrorCallback(std::move(callback));
  size_t sync_return_count = 0;
  for (const auto& backend_shard : backend_shards_) {
    if (!backend_shard->MaybeRunCleanupDoomedEntries(barrier_callback)) {
      // If a shard completes synchronously, it returns false. Count how many do
      // so.
      ++sync_return_count;
    }
  }
  if (sync_return_count == GetSizeOfShards()) {
    // If all shards completed synchronously, no cleanup task was scheduled.
    return false;
  }
  for (size_t i = 0; i < sync_return_count; ++i) {
    barrier_callback.Run(Error::kOk);
  }
  // Reaching this point means at least one shard scheduled a task.
  return true;
}

void SqlPersistentStore::MaybeRunCheckpoint(
    base::OnceCallback<void(bool)> callback) {
  if (net::features::kSqlDiskCacheSerialCheckpoint.Get()) {
    std::vector<bool> results;
    results.reserve(backend_shards_.size());
    RunNextCheckpoint(std::move(callback), std::move(results));
    return;
  }
  auto barrier_callback = base::BarrierCallback<bool>(
      GetSizeOfShards(), base::BindOnce([](std::vector<bool> results) {
                           return std::ranges::any_of(results, std::identity{});
                         }).Then(std::move(callback)));
  for (const auto& backend_shard : backend_shards_) {
    backend_shard->MaybeRunCheckpoint(barrier_callback);
  }
}

void SqlPersistentStore::RunNextCheckpoint(
    base::OnceCallback<void(bool)> callback,
    std::vector<bool> results) {
  if (results.size() == backend_shards_.size()) {
    std::move(callback).Run(std::ranges::any_of(results, std::identity{}));
    return;
  }
  const auto index = results.size();
  backend_shards_[index]->MaybeRunCheckpoint(
      base::BindOnce(&AppendResult, std::move(results))
          .Then(base::BindOnce(&SqlPersistentStore::RunNextCheckpoint,
                               weak_factory_.GetWeakPtr(),
                               std::move(callback))));
}

void SqlPersistentStore::EnableStrictCorruptionCheckForTesting() {
  for (const auto& backend_shard : backend_shards_) {
    backend_shard->EnableStrictCorruptionCheckForTesting();  // IN-TEST
  }
}

void SqlPersistentStore::SetSimulateDbFailureForTesting(bool fail) {
  for (const auto& backend_shard : backend_shards_) {
    backend_shard->SetSimulateDbFailureForTesting(fail);  // IN-TEST
  }
}

void SqlPersistentStore::RazeAndPoisonForTesting() {
  for (const auto& backend_shard : backend_shards_) {
    backend_shard->RazeAndPoisonForTesting();  // IN-TEST
  }
}

void SqlPersistentStore::SetEvictionHookForTesting(  // IN-TEST
    base::RepeatingClosure hook) {
  for (auto& shard : backend_shards_) {
    shard->SetEvictionHookForTesting(hook);  // IN-TEST
  }
}

SqlPersistentStore::IndexState SqlPersistentStore::GetIndexStateForHash(
    CacheEntryKey::Hash key_hash) const {
  return GetShard(key_hash).GetIndexStateForHash(key_hash);
}

void SqlPersistentStore::SetInMemoryEntryDataHints(CacheEntryKey::Hash key_hash,
                                                   ResId res_id,
                                                   MemoryEntryDataHints hints) {
  return GetShard(key_hash).SetInMemoryEntryDataHints(res_id, hints);
}

std::optional<MemoryEntryDataHints>
SqlPersistentStore::GetInMemoryEntryDataHints(
    CacheEntryKey::Hash key_hash) const {
  return GetShard(key_hash).GetInMemoryEntryDataHints(key_hash);
}

std::optional<SqlPersistentStore::ResId>
SqlPersistentStore::TryGetSingleResIdFromInMemoryIndex(
    CacheEntryKey::Hash key_hash) const {
  return GetShard(key_hash).TryGetSingleResIdFromInMemoryIndex(key_hash);
}

SqlPersistentStore::ShardId SqlPersistentStore::GetShardIdForHash(
    CacheEntryKey::Hash key_hash) const {
  return ShardId(key_hash.value() % GetSizeOfShards());
}

void SqlPersistentStore::SetMaxSize(int64_t max_bytes) {
  max_bytes_ = max_bytes;
  high_watermark_ = max_bytes * kSqlBackendEvictionHighWaterMarkPermille / 1000;
  idle_time_high_watermark_ =
      max_bytes * kSqlBackendIdleTimeEvictionHighWaterMarkPermille / 1000;
  low_watermark_ = max_bytes * kSqlBackendEvictionLowWaterMarkPermille / 1000;
  max_file_size_ = CalculateMaxFileSize(max_bytes);
}

base::RepeatingCallback<void(SqlPersistentStore::Error)>
SqlPersistentStore::CreateBarrierErrorCallback(ErrorCallback callback) {
  return base::BarrierCallback<Error>(
      GetSizeOfShards(),
      base::BindOnce(
          [](ErrorCallback callback, const std::vector<Error>& errors) {
            // Return the first error, or kOk if all succeeded.
            for (const auto& error : errors) {
              if (error != Error::kOk) {
                std::move(callback).Run(error);
                return;
              }
            }
            std::move(callback).Run(Error::kOk);
          },
          std::move(callback)));
}

size_t SqlPersistentStore::GetSizeOfShards() const {
  return background_task_runners_.size();
}

SqlPersistentStore::BackendShard& SqlPersistentStore::GetShard(
    CacheEntryKey::Hash hash) const {
  return *backend_shards_[GetShardIdForHash(hash).value()];
}

SqlPersistentStore::BackendShard& SqlPersistentStore::GetShard(
    const CacheEntryKey& key) const {
  return GetShard(key.hash());
}

void SqlPersistentStore::OnInitializeFinished(
    ErrorCallback callback,
    std::vector<InitResultOrError> results) {
  CHECK_EQ(results.size(), GetSizeOfShards());
  int64_t total_database_size = 0;
  for (const auto& result : results) {
    if (!result.has_value()) {
      std::move(callback).Run(result.error());
      return;
    }
  }
  if (net::features::kSqlDiskCacheLoadIndexOnInit.Get()) {
    in_memory_load_result_ = Error::kOk;
  }
  for (const auto& result : results) {
    // Only the result from the shard 0 has max_bytes.
    if (result->max_bytes.has_value()) {
      SetMaxSize(*result->max_bytes);
      base::UmaHistogramMemoryLargeMB(
          base::StrCat({kSqlDiskCacheBackendHistogramPrefix, "MaxSize"}),
          *result->max_bytes / 1024 / 1024);
    }
    total_database_size += result->database_size;
  }
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({kSqlDiskCacheBackendHistogramPrefix, "DatabaseSize"}),
      total_database_size / 1024 / 1024);
  base::UmaHistogramCounts1M(
      base::StrCat({kSqlDiskCacheBackendHistogramPrefix, "EntryCount"}),
      GetEntryCount());
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({kSqlDiskCacheBackendHistogramPrefix, "TotalSize"}),
      GetSizeOfAllEntries() / 1024 / 1024);
  std::move(callback).Run(Error::kOk);
}

// Default constructor and move operations for EntryInfo.
SqlPersistentStore::EntryInfo::EntryInfo() = default;
SqlPersistentStore::EntryInfo::~EntryInfo() = default;
SqlPersistentStore::EntryInfo::EntryInfo(EntryInfo&&) = default;
SqlPersistentStore::EntryInfo& SqlPersistentStore::EntryInfo::operator=(
    EntryInfo&&) = default;

SqlPersistentStore::ReadResult::ReadResult() = default;
SqlPersistentStore::ReadResult::~ReadResult() = default;
SqlPersistentStore::ReadResult::ReadResult(const ReadResult&) = default;
SqlPersistentStore::ReadResult& SqlPersistentStore::ReadResult::operator=(
    const ReadResult&) = default;
SqlPersistentStore::ReadResult::ReadResult(ReadResult&&) = default;
SqlPersistentStore::ReadResult& SqlPersistentStore::ReadResult::operator=(
    ReadResult&&) = default;

SqlPersistentStore::ResIdAndShardId::ResIdAndShardId(ResId res_id,
                                                     ShardId shard_id)
    : res_id(res_id), shard_id(shard_id) {}
SqlPersistentStore::ResIdAndShardId::ResIdAndShardId() = default;
SqlPersistentStore::ResIdAndShardId::~ResIdAndShardId() = default;
SqlPersistentStore::ResIdAndShardId::ResIdAndShardId(const ResIdAndShardId&) =
    default;
SqlPersistentStore::ResIdAndShardId&
SqlPersistentStore::ResIdAndShardId::operator=(const ResIdAndShardId&) =
    default;
SqlPersistentStore::ResIdAndShardId::ResIdAndShardId(ResIdAndShardId&&) =
    default;
SqlPersistentStore::ResIdAndShardId&
SqlPersistentStore::ResIdAndShardId::operator=(ResIdAndShardId&&) = default;

SqlPersistentStore::EntryInfoWithKeyAndIterator::EntryInfoWithKeyAndIterator() =
    default;
SqlPersistentStore::EntryInfoWithKeyAndIterator::
    ~EntryInfoWithKeyAndIterator() = default;
SqlPersistentStore::EntryInfoWithKeyAndIterator::EntryInfoWithKeyAndIterator(
    EntryInfoWithKeyAndIterator&&) = default;
SqlPersistentStore::EntryInfoWithKeyAndIterator&
SqlPersistentStore::EntryInfoWithKeyAndIterator::operator=(
    EntryInfoWithKeyAndIterator&&) = default;

int64_t SqlPersistentStore::StoreStatus::GetEstimatedDiskUsage() const {
  base::ClampedNumeric<int64_t> result;
  result = entry_count;
  result *= kSqlBackendStaticResourceSize;
  result += total_size;
  return result;
}

SqlPersistentStore::InMemoryIndexAndDoomedResIds::InMemoryIndexAndDoomedResIds(
    SqlPersistentStoreInMemoryIndex&& index,
    std::vector<SqlPersistentStore::ResId> doomed_entry_res_ids)
    : index(std::move(index)),
      doomed_entry_res_ids(std::move(doomed_entry_res_ids)) {}
SqlPersistentStore::InMemoryIndexAndDoomedResIds::
    ~InMemoryIndexAndDoomedResIds() = default;
SqlPersistentStore::InMemoryIndexAndDoomedResIds::InMemoryIndexAndDoomedResIds(
    InMemoryIndexAndDoomedResIds&& other) = default;
SqlPersistentStore::InMemoryIndexAndDoomedResIds&
SqlPersistentStore::InMemoryIndexAndDoomedResIds::operator=(
    InMemoryIndexAndDoomedResIds&& other) = default;

SqlPersistentStore::EvictionTarget::EvictionTarget(
    SqlPersistentStore::ResId res_id,
    int64_t entry_size_with_overhead)
    : res_id(res_id), entry_size_with_overhead(entry_size_with_overhead) {}
SqlPersistentStore::EvictionTarget::~EvictionTarget() = default;
SqlPersistentStore::EvictionTarget::EvictionTarget(EvictionTarget&&) = default;
SqlPersistentStore::EvictionTarget&
SqlPersistentStore::EvictionTarget::operator=(EvictionTarget&&) = default;
SqlPersistentStore::EvictionTarget::EvictionTarget(const EvictionTarget&) =
    default;
SqlPersistentStore::EvictionTarget&
SqlPersistentStore::EvictionTarget::operator=(const EvictionTarget&) = default;
bool SqlPersistentStore::EvictionTarget::operator==(
    const EvictionTarget& other) const = default;

SqlPersistentStore::EvictionResult::EvictionResult(
    std::vector<ResId> deleted_res_ids,
    EvictionTargetQueue pending_eviction_targets)
    : deleted_res_ids(std::move(deleted_res_ids)),
      pending_eviction_targets(std::move(pending_eviction_targets)) {}
SqlPersistentStore::EvictionResult::~EvictionResult() = default;
SqlPersistentStore::EvictionResult::EvictionResult(EvictionResult&&) = default;
SqlPersistentStore::EvictionResult&
SqlPersistentStore::EvictionResult::operator=(EvictionResult&&) = default;

SqlPersistentStore::InitResult::InitResult(
    std::optional<int64_t> max_bytes,
    const StoreStatus& store_status,
    int64_t database_size,
    std::optional<InMemoryIndexAndDoomedResIds> in_memory_data)
    : max_bytes(max_bytes),
      store_status(store_status),
      database_size(database_size),
      in_memory_data(std::move(in_memory_data)) {}
SqlPersistentStore::InitResult::~InitResult() = default;
SqlPersistentStore::InitResult::InitResult(InitResult&& other) = default;
SqlPersistentStore::InitResult& SqlPersistentStore::InitResult::operator=(
    InitResult&& other) = default;

}  // namespace disk_cache
