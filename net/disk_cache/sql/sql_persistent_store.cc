// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store.h"

#include <algorithm>
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
#include "base/types/expected.h"
#include "net/base/cache_type.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/sql/cache_entry_key.h"
#include "net/disk_cache/sql/eviction_candidate_aggregator.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_persistent_store_backend_shard.h"

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
  for (size_t i = 0; i < num_shards; ++i) {
    backend_shards.emplace_back(std::make_unique<BackendShard>(
        ShardId(i), path, type, background_task_runners[i]));
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
                                   ErrorCallback callback) {
  GetShard(key).DoomEntry(key, res_id, std::move(callback));
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

void SqlPersistentStore::UpdateEntryLastUsedByResId(const CacheEntryKey& key,
                                                    ResId res_id,
                                                    base::Time last_used,
                                                    ErrorCallback callback) {
  GetShard(key).UpdateEntryLastUsedByResId(res_id, last_used,
                                           std::move(callback));
}

void SqlPersistentStore::UpdateEntryHeaderAndLastUsed(
    const CacheEntryKey& key,
    ResId res_id,
    base::Time last_used,
    const std::optional<MemoryEntryDataHints>& new_hints,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t header_size_delta,
    ErrorCallback callback) {
  GetShard(key).UpdateEntryHeaderAndLastUsed(
      key, res_id, last_used, new_hints, std::move(buffer), header_size_delta,
      std::move(callback));
}

void SqlPersistentStore::WriteEntryData(const CacheEntryKey& key,
                                        ResId res_id,
                                        int64_t old_body_end,
                                        int64_t offset,
                                        scoped_refptr<net::IOBuffer> buffer,
                                        int buf_len,
                                        bool truncate,
                                        ErrorCallback callback) {
  GetShard(key).WriteEntryData(key, res_id, old_body_end, offset,
                               std::move(buffer), buf_len, truncate,
                               std::move(callback));
}

void SqlPersistentStore::ReadEntryData(const CacheEntryKey& key,
                                       ResId res_id,
                                       int64_t offset,
                                       scoped_refptr<net::IOBuffer> buffer,
                                       int buf_len,
                                       int64_t body_end,
                                       bool sparse_reading,
                                       IntOrErrorCallback callback) {
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
  if (eviction_result_callback_) {
    return EvictionUrgency::kNotNeeded;
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
    ErrorCallback callback) {
  CHECK(!eviction_result_callback_);
  CHECK(callback);
  const int64_t size_to_be_removed = GetSizeOfAllEntries() - low_watermark_;
  if (size_to_be_removed <= 0) {
    std::move(callback).Run(Error::kOk);
    return;
  }
  eviction_result_callback_ = std::move(callback);
  auto barrier_callback = base::BarrierCallback<ResIdListOrError>(
      GetSizeOfShards(),
      base::BindOnce(&SqlPersistentStore::OnEvictionFinished,
                     weak_factory_.GetWeakPtr(), is_idle_time_eviction,
                     base::TimeTicks::Now()));
  auto aggregator = base::MakeRefCounted<EvictionCandidateAggregator>(
      size_to_be_removed, background_task_runners_);
  auto res_id_sets =
      GroupResIdPerShardId(std::move(excluded_list), GetSizeOfShards());
  for (size_t i = 0; i < GetSizeOfShards(); ++i) {
    backend_shards_[i]->StartEviction(
        size_to_be_removed, std::move(res_id_sets[i]), is_idle_time_eviction,
        aggregator, barrier_callback);
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
  const std::string_view kMethodName =
      is_idle_time_eviction ? "RunEviction" : "RunEvictionOnIdleTime";
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({kSqlDiskCacheBackendHistogramPrefix, kMethodName,
                    error == Error::kOk ? ".SuccessTime" : ".FailureTime"}),
      base::TimeTicks::Now() - start_time);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {kSqlDiskCacheBackendHistogramPrefix, kMethodName, ".Result"}),
      error);
  if (error == Error::kOk) {
    base::UmaHistogramCounts1000(
        base::StrCat(
            {kSqlDiskCacheBackendHistogramPrefix, kMethodName, ".EntryCount"}),
        count);
  }

  CHECK(eviction_result_callback_);
  auto callback = std::move(eviction_result_callback_);
  eviction_result_callback_.Reset();
  std::move(callback).Run(error);
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

bool SqlPersistentStore::MaybeLoadInMemoryIndex(ErrorCallback callback) {
  if (in_memory_load_triggered_) {
    return false;
  }
  if (net::features::kSqlDiskCacheLoadIndexOnInit.Get()) {
    return false;
  }
  in_memory_load_triggered_ = true;
  auto barrier_callback = CreateBarrierErrorCallback(std::move(callback));
  for (const auto& backend_shard : backend_shards_) {
    backend_shard->LoadInMemoryIndex(barrier_callback);
  }
  return true;
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
  auto barrier_callback = base::BarrierCallback<bool>(
      GetSizeOfShards(), base::BindOnce(
                             [](base::OnceCallback<void(bool)> callback,
                                std::vector<bool> results) {
                               for (auto result : results) {
                                 if (result) {
                                   std::move(callback).Run(true);
                                   return;
                                 }
                               }
                               std::move(callback).Run(false);
                             },
                             std::move(callback)));
  for (const auto& backend_shard : backend_shards_) {
    backend_shard->MaybeRunCheckpoint(barrier_callback);
  }
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
