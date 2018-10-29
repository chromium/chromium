// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/storage_area_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {
using leveldb::mojom::BatchedOperation;
using leveldb::mojom::BatchedOperationPtr;
using leveldb::mojom::DatabaseError;
}  // namespace

StorageAreaImpl::Delegate::~Delegate() {}

void StorageAreaImpl::Delegate::MigrateData(
    base::OnceCallback<void(std::unique_ptr<ValueMap>)> callback) {
  std::move(callback).Run(nullptr);
}

std::vector<StorageAreaImpl::Change> StorageAreaImpl::Delegate::FixUpData(
    const ValueMap& data) {
  return std::vector<Change>();
}

void StorageAreaImpl::Delegate::OnMapLoaded(DatabaseError) {}

bool StorageAreaImpl::s_aggressive_flushing_enabled_ = false;

StorageAreaImpl::RateLimiter::RateLimiter(size_t desired_rate,
                                          base::TimeDelta time_quantum)
    : rate_(desired_rate), samples_(0), time_quantum_(time_quantum) {
  DCHECK_GT(desired_rate, 0ul);
}

base::TimeDelta StorageAreaImpl::RateLimiter::ComputeTimeNeeded() const {
  return time_quantum_ * (samples_ / rate_);
}

base::TimeDelta StorageAreaImpl::RateLimiter::ComputeDelayNeeded(
    const base::TimeDelta elapsed_time) const {
  base::TimeDelta time_needed = ComputeTimeNeeded();
  if (time_needed > elapsed_time)
    return time_needed - elapsed_time;
  return base::TimeDelta();
}

StorageAreaImpl::CommitBatch::CommitBatch() : clear_all_first(false) {}
StorageAreaImpl::CommitBatch::~CommitBatch() {}

StorageAreaImpl::StorageAreaImpl(leveldb::mojom::LevelDBDatabase* database,
                                 const std::string& prefix,
                                 Delegate* delegate,
                                 const Options& options)
    : StorageAreaImpl(database,
                      leveldb::StdStringToUint8Vector(prefix),
                      delegate,
                      options) {}

StorageAreaImpl::StorageAreaImpl(leveldb::mojom::LevelDBDatabase* database,
                                 std::vector<uint8_t> prefix,
                                 Delegate* delegate,
                                 const Options& options)
    : prefix_(std::move(prefix)),
      delegate_(delegate),
      database_(database),
      cache_mode_(database ? options.cache_mode : CacheMode::KEYS_AND_VALUES),
      storage_used_(0),
      max_size_(options.max_size),
      memory_used_(0),
      start_time_(base::TimeTicks::Now()),
      default_commit_delay_(options.default_commit_delay),
      data_rate_limiter_(options.max_bytes_per_hour,
                         base::TimeDelta::FromHours(1)),
      commit_rate_limiter_(options.max_commits_per_hour,
                           base::TimeDelta::FromHours(1)),
      weak_ptr_factory_(this) {
  bindings_.set_connection_error_handler(
      base::Bind(&StorageAreaImpl::OnConnectionError, base::Unretained(this)));
}

StorageAreaImpl::~StorageAreaImpl() {
  DCHECK(!has_pending_load_tasks());
  if (commit_batch_)
    CommitChanges();
}

void StorageAreaImpl::Bind(blink::mojom::StorageAreaRequest request) {
  bindings_.AddBinding(this, std::move(request));
  // If the number of bindings is more than 1, then the |client_old_value| sent
  // by the clients need not be valid due to races on updates from multiple
  // clients. So, cache the values in the service. Setting cache mode back to
  // only keys when the number of bindings goes back to 1 could cause
  // inconsistency due to the async notifications of mutations to the client
  // reaching late.
  if (cache_mode_ == CacheMode::KEYS_ONLY_WHEN_POSSIBLE &&
      bindings_.size() > 1) {
    SetCacheMode(CacheMode::KEYS_AND_VALUES);
  }
}

std::unique_ptr<StorageAreaImpl> StorageAreaImpl::ForkToNewPrefix(
    const std::string& new_prefix,
    Delegate* delegate,
    const Options& options) {
  return ForkToNewPrefix(leveldb::StdStringToUint8Vector(new_prefix), delegate,
                         options);
}

std::unique_ptr<StorageAreaImpl> StorageAreaImpl::ForkToNewPrefix(
    std::vector<uint8_t> new_prefix,
    Delegate* delegate,
    const Options& options) {
  auto forked_area = std::make_unique<StorageAreaImpl>(
      database_, std::move(new_prefix), delegate, options);

  forked_area->map_state_ = MapState::LOADING_FROM_FORK;

  if (IsMapLoaded()) {
    DoForkOperation(forked_area->weak_ptr_factory_.GetWeakPtr());
  } else {
    LoadMap(base::BindOnce(&StorageAreaImpl::DoForkOperation,
                           weak_ptr_factory_.GetWeakPtr(),
                           forked_area->weak_ptr_factory_.GetWeakPtr()));
  }
  return forked_area;
}

void StorageAreaImpl::CancelAllPendingRequests() {
  on_load_complete_tasks_.clear();
}

void StorageAreaImpl::EnableAggressiveCommitDelay() {
  s_aggressive_flushing_enabled_ = true;
}

void StorageAreaImpl::ScheduleImmediateCommit() {
  if (!on_load_complete_tasks_.empty()) {
    LoadMap(base::BindOnce(&StorageAreaImpl::ScheduleImmediateCommit,
                           base::Unretained(this)));
    return;
  }

  if (!database_ || !commit_batch_)
    return;
  CommitChanges();
}

void StorageAreaImpl::OnMemoryDump(const std::string& name,
                                   base::trace_event::ProcessMemoryDump* pmd) {
  if (!IsMapLoaded())
    return;

  const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  if (commit_batch_) {
    size_t data_size = 0;
    for (const auto& iter : commit_batch_->changed_values)
      data_size += iter.first.size() + iter.second.size();
    for (const auto& key : commit_batch_->changed_keys)
      data_size += key.size();

    auto* commit_batch_mad = pmd->CreateAllocatorDump(name + "/commit_batch");
    commit_batch_mad->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes, data_size);
    if (system_allocator_name)
      pmd->AddSuballocation(commit_batch_mad->guid(), system_allocator_name);
  }

  // Do not add storage map usage if less than 1KB.
  if (memory_used_ < 1024)
    return;

  auto* map_mad = pmd->CreateAllocatorDump(name + "/storage_map");
  map_mad->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                     base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                     memory_used_);
  map_mad->AddString("load_state", "",
                     map_state_ == MapState::LOADED_KEYS_ONLY
                         ? "keys_only"
                         : "keys_and_values");
  if (system_allocator_name)
    pmd->AddSuballocation(map_mad->guid(), system_allocator_name);
}

void StorageAreaImpl::PurgeMemory() {
  if (!IsMapLoaded() ||  // We're not using any memory.
      commit_batch_ ||   // We leave things alone with changes pending.
      !database_) {  // Don't purge anything if we're not backed by a database.
    return;
  }

  map_state_ = MapState::UNLOADED;
  memory_used_ = 0;
  keys_only_map_.clear();
  keys_values_map_.clear();
}

void StorageAreaImpl::SetCacheModeForTesting(CacheMode cache_mode) {
  SetCacheMode(cache_mode);
}

mojo::InterfacePtrSetElementId StorageAreaImpl::AddObserver(
    blink::mojom::StorageAreaObserverAssociatedPtr observer) {
  if (cache_mode_ == CacheMode::KEYS_AND_VALUES)
    observer->ShouldSendOldValueOnMutations(false);
  return observers_.AddPtr(std::move(observer));
}

bool StorageAreaImpl::HasObserver(mojo::InterfacePtrSetElementId id) {
  return observers_.HasPtr(id);
}

blink::mojom::StorageAreaObserverAssociatedPtr StorageAreaImpl::RemoveObserver(
    mojo::InterfacePtrSetElementId id) {
  return observers_.RemovePtr(id);
}

void StorageAreaImpl::AddObserver(
    blink::mojom::StorageAreaObserverAssociatedPtrInfo observer) {
  AddObserver(
      blink::mojom::StorageAreaObserverAssociatedPtr(std::move(observer)));
}

void StorageAreaImpl::Put(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& value,
    const base::Optional<std::vector<uint8_t>>& client_old_value,
    const std::string& source,
    PutCallback callback) {
  if (!IsMapLoaded() || IsMapUpgradeNeeded()) {
    LoadMap(base::BindOnce(&StorageAreaImpl::Put, base::Unretained(this), key,
                           value, client_old_value, source,
                           std::move(callback)));
    return;
  }

  size_t old_item_size = 0;
  size_t old_item_memory = 0;
  size_t new_item_memory = 0;
  base::Optional<std::vector<uint8_t>> old_value;
  if (map_state_ == MapState::LOADED_KEYS_ONLY) {
    KeysOnlyMap::const_iterator found = keys_only_map_.find(key);
    if (found != keys_only_map_.end()) {
      if (client_old_value &&
          client_old_value.value().size() == found->second) {
        if (client_old_value == value) {
          std::move(callback).Run(true);  // Key already has this value.
          return;
        }
        old_value = client_old_value.value();
      } else {
#if DCHECK_IS_ON()
        // If |client_old_value| was not provided or if it's size does not
        // match, then we still let the change go through. But the notification
        // sent to clients will not contain old value. This is okay since
        // currently the only observer to these notification is the client
        // itself.
        DVLOG(1) << "Storage area with prefix "
                 << leveldb::Uint8VectorToStdString(prefix_)
                 << ": past value has length of " << found->second << ", but:";
        if (client_old_value) {
          DVLOG(1) << "Given past value has incorrect length of "
                   << client_old_value.value().size();
        } else {
          DVLOG(1) << "No given past value was provided.";
        }
#endif
      }
      old_item_size = key.size() + found->second;
      old_item_memory = key.size() + sizeof(size_t);
    }
    new_item_memory = key.size() + sizeof(size_t);
  } else {
    DCHECK_EQ(map_state_, MapState::LOADED_KEYS_AND_VALUES);
    auto found = keys_values_map_.find(key);
    if (found != keys_values_map_.end()) {
      if (found->second == value) {
        std::move(callback).Run(true);  // Key already has this value.
        return;
      }
      old_value = std::move(found->second);
      old_item_size = key.size() + old_value.value().size();
      old_item_memory = old_item_size;
    }
    new_item_memory = key.size() + value.size();
  }

  size_t new_item_size = key.size() + value.size();
  size_t new_storage_used = storage_used_ - old_item_size + new_item_size;

  // Only check quota if the size is increasing, this allows
  // shrinking changes to pre-existing maps that are over budget.
  if (new_item_size > old_item_size && new_storage_used > max_size_) {
    if (map_state_ == MapState::LOADED_KEYS_ONLY) {
      bindings_.ReportBadMessage(
          "The quota in browser cannot exceed when there is only one "
          "renderer.");
    } else {
      std::move(callback).Run(false);
    }
    return;
  }

  if (database_) {
    CreateCommitBatchIfNeeded();
    // No need to store values in |commit_batch_| if values are already
    // available in |keys_values_map_|, since CommitChanges() will take values
    // from there.
    if (map_state_ == MapState::LOADED_KEYS_ONLY)
      commit_batch_->changed_values[key] = value;
    else
      commit_batch_->changed_keys.insert(key);
  }

  if (map_state_ == MapState::LOADED_KEYS_ONLY)
    keys_only_map_[key] = value.size();
  else
    keys_values_map_[key] = value;

  storage_used_ = new_storage_used;
  memory_used_ += new_item_memory - old_item_memory;
  if (!old_value) {
    // We added a new key/value pair.
    observers_.ForAllPtrs(
        [&key, &value, &source](blink::mojom::StorageAreaObserver* observer) {
          observer->KeyAdded(key, value, source);
        });
  } else {
    // We changed the value for an existing key.
    observers_.ForAllPtrs([&key, &value, &source, &old_value](
                              blink::mojom::StorageAreaObserver* observer) {
      observer->KeyChanged(key, value, old_value.value(), source);
    });
  }
  std::move(callback).Run(true);
}

void StorageAreaImpl::Delete(
    const std::vector<uint8_t>& key,
    const base::Optional<std::vector<uint8_t>>& client_old_value,
    const std::string& source,
    DeleteCallback callback) {
  // Map upgrade check is required because the cache state could be changed
  // due to multiple bindings, and when multiple bindings are involved the
  // |client_old_value| can race. Thus any changes require checking for an
  // upgrade.
  if (!IsMapLoaded() || IsMapUpgradeNeeded()) {
    LoadMap(base::BindOnce(&StorageAreaImpl::Delete, base::Unretained(this),
                           key, client_old_value, source, std::move(callback)));
    return;
  }

  if (database_)
    CreateCommitBatchIfNeeded();

  std::vector<uint8_t> old_value;
  if (map_state_ == MapState::LOADED_KEYS_ONLY) {
    KeysOnlyMap::const_iterator found = keys_only_map_.find(key);
    if (found == keys_only_map_.end()) {
      std::move(callback).Run(true);
      return;
    }
    if (client_old_value && client_old_value.value().size() == found->second) {
      old_value = client_old_value.value();
    } else {
#if DCHECK_IS_ON()
      // If |client_old_value| was not provided or if it's size does not match,
      // then we still let the change go through. But the notification sent to
      // clients will not contain old value. This is okay since currently the
      // only observer to these notification is the client itself.
      DVLOG(1) << "Storage area with prefix "
               << leveldb::Uint8VectorToStdString(prefix_)
               << ": past value has length of " << found->second << ", but:";
      if (client_old_value) {
        DVLOG(1) << "Given past value has incorrect length of "
                 << client_old_value.value().size();
      } else {
        DVLOG(1) << "No given past value was provided.";
      }
#endif
    }
    storage_used_ -= key.size() + found->second;
    keys_only_map_.erase(found);
    memory_used_ -= key.size() + sizeof(size_t);
    if (commit_batch_)
      commit_batch_->changed_values[key] = std::vector<uint8_t>();
  } else {
    DCHECK_EQ(map_state_, MapState::LOADED_KEYS_AND_VALUES);
    auto found = keys_values_map_.find(key);
    if (found == keys_values_map_.end()) {
      std::move(callback).Run(true);
      return;
    }
    old_value.swap(found->second);
    keys_values_map_.erase(found);
    memory_used_ -= key.size() + old_value.size();
    storage_used_ -= key.size() + old_value.size();
    if (commit_batch_)
      commit_batch_->changed_keys.insert(key);
  }

  observers_.ForAllPtrs(
      [&key, &source, &old_value](blink::mojom::StorageAreaObserver* observer) {
        observer->KeyDeleted(key, old_value, source);
      });
  std::move(callback).Run(true);
}

void StorageAreaImpl::DeleteAll(const std::string& source,
                                DeleteAllCallback callback) {
  // Don't check if a map upgrade is needed here and instead just create an
  // empty map ourself.
  if (!IsMapLoaded()) {
    LoadMap(base::BindOnce(&StorageAreaImpl::DeleteAll, base::Unretained(this),
                           source, std::move(callback)));
    return;
  }

  bool already_empty = IsMapLoadedAndEmpty();

  // Upgrade map state if needed.
  if (IsMapUpgradeNeeded()) {
    DCHECK(keys_values_map_.empty());
    map_state_ = MapState::LOADED_KEYS_AND_VALUES;
  }

  if (already_empty) {
    std::move(callback).Run(true);
    return;
  }

  if (database_) {
    CreateCommitBatchIfNeeded();
    commit_batch_->clear_all_first = true;
    commit_batch_->changed_values.clear();
    commit_batch_->changed_keys.clear();
  }

  keys_only_map_.clear();
  keys_values_map_.clear();

  storage_used_ = 0;
  memory_used_ = 0;
  observers_.ForAllPtrs([&source](blink::mojom::StorageAreaObserver* observer) {
    observer->AllDeleted(source);
  });
  std::move(callback).Run(true);
}

void StorageAreaImpl::Get(const std::vector<uint8_t>& key,
                          GetCallback callback) {
  // TODO(ssid): Remove this method since it is not supported in only keys mode,
  // crbug.com/764127.
  if (cache_mode_ == CacheMode::KEYS_ONLY_WHEN_POSSIBLE) {
    NOTREACHED();
    return;
  }
  if (!IsMapLoaded() || IsMapUpgradeNeeded()) {
    LoadMap(base::BindOnce(&StorageAreaImpl::Get, base::Unretained(this), key,
                           std::move(callback)));
    return;
  }

  auto found = keys_values_map_.find(key);
  if (found == keys_values_map_.end()) {
    std::move(callback).Run(false, std::vector<uint8_t>());
    return;
  }
  std::move(callback).Run(true, found->second);
}

void StorageAreaImpl::GetAll(
    blink::mojom::StorageAreaGetAllCallbackAssociatedPtrInfo complete_callback,
    GetAllCallback callback) {
  // The map must always be loaded for the KEYS_ONLY_WHEN_POSSIBLE mode.
  if (map_state_ != MapState::LOADED_KEYS_AND_VALUES) {
    LoadMap(base::BindOnce(&StorageAreaImpl::GetAll, base::Unretained(this),
                           std::move(complete_callback), std::move(callback)));
    return;
  }

  std::vector<blink::mojom::KeyValuePtr> all;
  for (const auto& it : keys_values_map_) {
    auto kv = blink::mojom::KeyValue::New();
    kv->key = it.first;
    kv->value = it.second;
    all.push_back(std::move(kv));
  }
  std::move(callback).Run(true, std::move(all));
  if (complete_callback.is_valid()) {
    blink::mojom::StorageAreaGetAllCallbackAssociatedPtr complete_ptr;
    complete_ptr.Bind(std::move(complete_callback));
    complete_ptr->Complete(true);
  }
}

void StorageAreaImpl::SetCacheMode(CacheMode cache_mode) {
  if (cache_mode_ == cache_mode ||
      (!database_ && cache_mode == CacheMode::KEYS_ONLY_WHEN_POSSIBLE)) {
    return;
  }
  cache_mode_ = cache_mode;
  bool should_send_values = cache_mode == CacheMode::KEYS_ONLY_WHEN_POSSIBLE;
  observers_.ForAllPtrs(
      [should_send_values](blink::mojom::StorageAreaObserver* observer) {
        observer->ShouldSendOldValueOnMutations(should_send_values);
      });

  // If the |keys_only_map_| is loaded and desired state needs values, no point
  // keeping around the map since the next change would require reload. On the
  // other hand if only keys are desired, the keys and values map can still be
  // used. Consider not unloading when the map is still useful.
  UnloadMapIfPossible();
}

void StorageAreaImpl::OnConnectionError() {
  if (!bindings_.empty())
    return;
  // If any tasks are waiting for load to complete, delay calling the
  // no_bindings_callback_ until all those tasks have completed.
  if (!on_load_complete_tasks_.empty())
    return;
  delegate_->OnNoBindings();
}

void StorageAreaImpl::LoadMap(base::OnceClosure completion_callback) {
  DCHECK_NE(map_state_, MapState::LOADED_KEYS_AND_VALUES);
  DCHECK(keys_values_map_.empty());

  // Current commit batch needs to be applied before re-loading the map. The
  // re-load of map occurs only when GetAll() is called or CacheMode is set to
  // keys and values, and the |keys_only_map_| is already loaded. In this case
  // commit batch needs to be committed before reading the database.
  if (map_state_ == MapState::LOADED_KEYS_ONLY) {
    DCHECK(on_load_complete_tasks_.empty());
    DCHECK(database_);
    if (commit_batch_)
      CommitChanges();
    // Make sure the keys only map is not used when on load tasks are in queue.
    // The changes to the area will be queued to on load tasks.
    keys_only_map_.clear();
    map_state_ = MapState::UNLOADED;
  }

  on_load_complete_tasks_.push_back(std::move(completion_callback));
  if (map_state_ == MapState::LOADING_FROM_DATABASE ||
      map_state_ == MapState::LOADING_FROM_FORK) {
    return;
  }

  map_state_ = MapState::LOADING_FROM_DATABASE;

  if (!database_) {
    OnMapLoaded(DatabaseError::IO_ERROR,
                std::vector<leveldb::mojom::KeyValuePtr>());
    return;
  }

  database_->GetPrefixed(prefix_,
                         base::BindOnce(&StorageAreaImpl::OnMapLoaded,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void StorageAreaImpl::OnMapLoaded(
    DatabaseError status,
    std::vector<leveldb::mojom::KeyValuePtr> data) {
  DCHECK(keys_values_map_.empty());
  DCHECK_EQ(map_state_, MapState::LOADING_FROM_DATABASE);

  if (data.empty() && status == DatabaseError::OK) {
    delegate_->MigrateData(base::BindOnce(&StorageAreaImpl::OnGotMigrationData,
                                          weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  keys_only_map_.clear();
  map_state_ = MapState::LOADED_KEYS_AND_VALUES;

  keys_values_map_.clear();
  for (auto& it : data) {
    DCHECK_GE(it->key.size(), prefix_.size());
    keys_values_map_[std::vector<uint8_t>(it->key.begin() + prefix_.size(),
                                          it->key.end())] =
        std::move(it->value);
  }
  CalculateStorageAndMemoryUsed();

  std::vector<Change> changes = delegate_->FixUpData(keys_values_map_);
  if (!changes.empty()) {
    DCHECK(database_);
    CreateCommitBatchIfNeeded();
    for (auto& change : changes) {
      auto it = keys_values_map_.find(change.first);
      if (!change.second) {
        DCHECK(it != keys_values_map_.end());
        keys_values_map_.erase(it);
      } else {
        if (it != keys_values_map_.end()) {
          it->second = std::move(*change.second);
        } else {
          keys_values_map_[change.first] = std::move(*change.second);
        }
      }
      // No need to store values in |commit_batch_| if values are already
      // available in |keys_values_map_|, since CommitChanges() will take values
      // from there.
      commit_batch_->changed_keys.insert(std::move(change.first));
    }
    CalculateStorageAndMemoryUsed();
    CommitChanges();
  }

  // We proceed without using a backing store, nothing will be persisted but the
  // class is functional for the lifetime of the object.
  delegate_->OnMapLoaded(status);
  if (status != DatabaseError::OK) {
    database_ = nullptr;
    SetCacheMode(CacheMode::KEYS_AND_VALUES);
  }

  OnLoadComplete();
}

void StorageAreaImpl::OnGotMigrationData(std::unique_ptr<ValueMap> data) {
  keys_only_map_.clear();
  keys_values_map_ = data ? std::move(*data) : ValueMap();
  map_state_ = MapState::LOADED_KEYS_AND_VALUES;
  CalculateStorageAndMemoryUsed();

  if (database_ && !empty()) {
    CreateCommitBatchIfNeeded();
    // CommitChanges() will take values from |keys_values_map_|.
    for (const auto& it : keys_values_map_)
      commit_batch_->changed_keys.insert(it.first);
    CommitChanges();
  }

  OnLoadComplete();
}

void StorageAreaImpl::CalculateStorageAndMemoryUsed() {
  memory_used_ = 0;
  storage_used_ = 0;

  for (auto& it : keys_values_map_)
    memory_used_ += it.first.size() + it.second.size();
  storage_used_ = memory_used_;

  for (auto& it : keys_only_map_) {
    memory_used_ += it.first.size() + sizeof(size_t);
    storage_used_ += it.first.size() + it.second;
  }
}

void StorageAreaImpl::OnLoadComplete() {
  DCHECK(IsMapLoaded());

  std::vector<base::OnceClosure> tasks;
  on_load_complete_tasks_.swap(tasks);
  for (auto it = tasks.begin(); it != tasks.end(); ++it) {
    // Some tasks (like GetAll) can require a reload if they need a different
    // map type. If this happens, stop our task execution. Appending tasks is
    // required (instead of replacing) because the task that required the
    // reload-requesting-task put itself on the task queue and it still needs
    // to be executed before the rest of the tasks.
    if (!IsMapLoaded()) {
      on_load_complete_tasks_.reserve(on_load_complete_tasks_.size() +
                                      (tasks.end() - it));
      std::move(it, tasks.end(), std::back_inserter(on_load_complete_tasks_));
      return;
    }
    std::move(*it).Run();
  }

  // Call before |OnNoBindings| as delegate can destroy this object.
  UnloadMapIfPossible();

  // We might need to call the no_bindings_callback_ here if bindings became
  // empty while waiting for load to complete.
  if (bindings_.empty())
    delegate_->OnNoBindings();
}

void StorageAreaImpl::CreateCommitBatchIfNeeded() {
  if (commit_batch_)
    return;
  DCHECK(database_);

  commit_batch_.reset(new CommitBatch());
  BrowserThread::PostAfterStartupTask(
      FROM_HERE, base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(&StorageAreaImpl::StartCommitTimer,
                     weak_ptr_factory_.GetWeakPtr()));
}

void StorageAreaImpl::StartCommitTimer() {
  if (!commit_batch_)
    return;

  // Start a timer to commit any changes that accrue in the batch, but only if
  // no commits are currently in flight. In that case the timer will be
  // started after the commits have happened.
  if (commit_batches_in_flight_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&StorageAreaImpl::CommitChanges,
                     weak_ptr_factory_.GetWeakPtr()),
      ComputeCommitDelay());
}

base::TimeDelta StorageAreaImpl::ComputeCommitDelay() const {
  if (s_aggressive_flushing_enabled_)
    return base::TimeDelta::FromSeconds(1);

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  base::TimeDelta delay =
      std::max(default_commit_delay_,
               std::max(commit_rate_limiter_.ComputeDelayNeeded(elapsed_time),
                        data_rate_limiter_.ComputeDelayNeeded(elapsed_time)));
  // TODO(mek): Rename histogram to match class name, or eliminate histogram
  // entirely.
  UMA_HISTOGRAM_LONG_TIMES("LevelDBWrapper.CommitDelay", delay);
  return delay;
}

void StorageAreaImpl::CommitChanges() {
  // Note: commit_batch_ may be null if ScheduleImmediateCommit was called
  // after a delayed commit task was scheduled.
  if (!commit_batch_)
    return;

  DCHECK(database_);
  DCHECK(IsMapLoaded()) << static_cast<int>(map_state_);

  commit_rate_limiter_.add_samples(1);

  // Commit all our changes in a single batch.
  std::vector<BatchedOperationPtr> operations = delegate_->PrepareToCommit();
  bool has_changes = !operations.empty() ||
                     !commit_batch_->changed_values.empty() ||
                     !commit_batch_->changed_keys.empty();
  if (commit_batch_->clear_all_first) {
    BatchedOperationPtr item = BatchedOperation::New();
    item->type = leveldb::mojom::BatchOperationType::DELETE_PREFIXED_KEY;
    item->key = prefix_;
    operations.push_back(std::move(item));
  }
  size_t data_size = 0;
  if (map_state_ == MapState::LOADED_KEYS_AND_VALUES) {
    DCHECK(commit_batch_->changed_values.empty())
        << "Map state and commit state out of sync.";
    for (const auto& key : commit_batch_->changed_keys) {
      data_size += key.size();
      BatchedOperationPtr item = BatchedOperation::New();
      item->key.reserve(prefix_.size() + key.size());
      item->key.insert(item->key.end(), prefix_.begin(), prefix_.end());
      item->key.insert(item->key.end(), key.begin(), key.end());
      auto kv_it = keys_values_map_.find(key);
      if (kv_it != keys_values_map_.end()) {
        item->type = leveldb::mojom::BatchOperationType::PUT_KEY;
        data_size += kv_it->second.size();
        item->value = kv_it->second;
      } else {
        item->type = leveldb::mojom::BatchOperationType::DELETE_KEY;
      }
      operations.push_back(std::move(item));
    }
  } else {
    DCHECK(commit_batch_->changed_keys.empty())
        << "Map state and commit state out of sync.";
    DCHECK_EQ(map_state_, MapState::LOADED_KEYS_ONLY);
    for (auto& it : commit_batch_->changed_values) {
      const auto& key = it.first;
      data_size += key.size();
      BatchedOperationPtr item = BatchedOperation::New();
      item->key.reserve(prefix_.size() + key.size());
      item->key.insert(item->key.end(), prefix_.begin(), prefix_.end());
      item->key.insert(item->key.end(), key.begin(), key.end());
      auto kv_it = keys_only_map_.find(key);
      if (kv_it != keys_only_map_.end()) {
        item->type = leveldb::mojom::BatchOperationType::PUT_KEY;
        data_size += it.second.size();
        item->value = std::move(it.second);
      } else {
        item->type = leveldb::mojom::BatchOperationType::DELETE_KEY;
      }
      operations.push_back(std::move(item));
    }
  }
  // Schedule the copy, and ignore if |clear_all_first| is specified and there
  // are no changing keys.
  if (commit_batch_->copy_to_prefix) {
    DCHECK(!has_changes);
    DCHECK(!commit_batch_->clear_all_first);
    BatchedOperationPtr item = BatchedOperation::New();
    item->type = leveldb::mojom::BatchOperationType::COPY_PREFIXED_KEY;
    item->key = prefix_;
    item->value = std::move(commit_batch_->copy_to_prefix.value());
    operations.push_back(std::move(item));
  }
  commit_batch_.reset();

  data_rate_limiter_.add_samples(data_size);

  ++commit_batches_in_flight_;

  // TODO(michaeln): Currently there is no guarantee LevelDBDatabaseImpl::Write
  // will run during a clean shutdown. We need that to avoid dataloss.
  database_->Write(std::move(operations),
                   base::BindOnce(&StorageAreaImpl::OnCommitComplete,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void StorageAreaImpl::OnCommitComplete(DatabaseError error) {
  has_committed_data_ = true;
  --commit_batches_in_flight_;
  StartCommitTimer();

  if (error != DatabaseError::OK) {
    SetCacheMode(CacheMode::KEYS_AND_VALUES);
  }

  // Call before |DidCommit| as delegate can destroy this object.
  UnloadMapIfPossible();

  delegate_->DidCommit(error);
}

void StorageAreaImpl::UnloadMapIfPossible() {
  // Do not unload the map if:
  // * The desired cache mode isn't key-only,
  // * The map isn't a loaded key-value map,
  // * There are pending tasks waiting on the key-value map being loaded, or
  // * There is no database connection.
  // * We have commit batches in-flight.
  // * We haven't committed data yet.
  if (cache_mode_ != CacheMode::KEYS_ONLY_WHEN_POSSIBLE ||
      map_state_ != MapState::LOADED_KEYS_AND_VALUES ||
      has_pending_load_tasks() || !database_ || commit_batches_in_flight_ > 0 ||
      !has_committed_data_) {
    return;
  }

  keys_only_map_.clear();
  memory_used_ = 0;
  for (auto& it : keys_values_map_) {
    keys_only_map_.insert(std::make_pair(it.first, it.second.size()));
  }
  if (commit_batch_) {
    for (const auto& key : commit_batch_->changed_keys) {
      auto value_it = keys_values_map_.find(key);
      commit_batch_->changed_values[key] = value_it == keys_values_map_.end()
                                               ? std::vector<uint8_t>()
                                               : std::move(value_it->second);
    }
    commit_batch_->changed_keys.clear();
  }

  keys_values_map_.clear();
  map_state_ = MapState::LOADED_KEYS_ONLY;

  CalculateStorageAndMemoryUsed();
}

void StorageAreaImpl::DoForkOperation(
    const base::WeakPtr<StorageAreaImpl>& forked_area) {
  if (!forked_area)
    return;

  DCHECK(IsMapLoaded());
  // TODO(dmurph): If these commits fails, then the disk could be in an
  // inconsistant state. Ideally all further operations will fail and the code
  // will correctly delete the database?
  if (database_) {
    // All changes must be stored in the database before the copy operation.
    if (has_changes_to_commit())
      CommitChanges();
    CreateCommitBatchIfNeeded();
    commit_batch_->copy_to_prefix = forked_area->prefix_;
    CommitChanges();
  }

  forked_area->OnForkStateLoaded(database_ != nullptr, keys_values_map_,
                                 keys_only_map_);
}

void StorageAreaImpl::OnForkStateLoaded(bool database_enabled,
                                        const ValueMap& value_map,
                                        const KeysOnlyMap& keys_only_map) {
  // This callback can get either the value map or the key only map depending
  // on parent operations and other things. So handle both.
  if (!value_map.empty() || keys_only_map.empty()) {
    keys_values_map_ = value_map;
    map_state_ = MapState::LOADED_KEYS_AND_VALUES;
  } else {
    keys_only_map_ = keys_only_map;
    map_state_ = MapState::LOADED_KEYS_ONLY;
  }

  if (!database_enabled) {
    database_ = nullptr;
    cache_mode_ = CacheMode::KEYS_AND_VALUES;
  }

  CalculateStorageAndMemoryUsed();
  OnLoadComplete();
}

}  // namespace content
