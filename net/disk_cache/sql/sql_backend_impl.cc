// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_backend_impl.h"

#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/sql/sql_entry_impl.h"
#include "net/disk_cache/sql/sql_persistent_store.h"

namespace disk_cache {

SqlBackendImpl::SqlBackendImpl(const base::FilePath& path,
                               int64_t max_bytes,
                               net::CacheType cache_type)
    : Backend(cache_type),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      store_(SqlPersistentStore::Create(path,
                                        max_bytes > 0 ? max_bytes : 0,
                                        GetCacheType(),
                                        background_task_runner_)) {
  DVLOG(1) << "SqlBackendImpl::SqlBackendImpl " << path;
}

SqlBackendImpl::~SqlBackendImpl() = default;

void SqlBackendImpl::Init(CompletionOnceCallback callback) {
  // Initialize the underlying persistent store. The callback will be run with
  // net::OK on success, or net::ERR_FAILED on failure.
  store_->Initialize(base::BindOnce(
      [](CompletionOnceCallback callback, SqlPersistentStore::Error result) {
        return std::move(callback).Run(result == SqlPersistentStore::Error::kOk
                                           ? net::OK
                                           : net::ERR_FAILED);
      },
      std::move(callback)));
}

int64_t SqlBackendImpl::MaxFileSize() const {
  // Delegates to the persistent store to get the max file size.
  return store_->MaxFileSize();
}

int32_t SqlBackendImpl::GetEntryCount(
    net::Int32CompletionOnceCallback callback) const {
  // Asynchronously retrieves the entry count from the persistent store.
  store_->GetEntryCount(std::move(callback));
  return net::ERR_IO_PENDING;
}

void SqlBackendImpl::InsertEntryResultCallback(const CacheEntryKey& key,
                                               EntryResultCallback callback) {
  // Open/Create/OpenOrCreate operations for the same cache key are serialized
  // at the HttpCache layer (see http://crrev.com/36211). Therefore,
  // an entry for `key` should not already exist in `entry_result_callbacks_`,
  // meaning this insertion should always be successful.
  // TODO(crbug.com/422065015): If SqlBackend is ever used by a client other
  // than HttpCache, we will need to queue operations to prevent concurrent
  // Open/Create/OpenOrCreate operations for the same key.
  auto insert_result = entry_result_callback_info_map_.insert(
      std::make_pair(key, EntryResultCallbackInfo(std::move(callback))));
  CHECK(insert_result.second);
}

EntryResult SqlBackendImpl::OpenOrCreateEntry(const std::string& key,
                                              net::RequestPriority priority,
                                              EntryResultCallback callback) {
  CacheEntryKey entry_key(key);
  // If the entry is already active (open), add a reference and return it.
  if (SqlEntryImpl* entry = GetActiveEntry(entry_key)) {
    entry->AddRef();
    return EntryResult::MakeOpened(entry);
  }
  // Otherwise, insert a callback to be invoked when the store operation
  // finishes.
  InsertEntryResultCallback(entry_key, std::move(callback));
  // Ask the store to open or create the entry.
  store_->OpenOrCreateEntry(
      entry_key, base::BindOnce(&SqlBackendImpl::OnEntryOperationFinished,
                                base::Unretained(this), entry_key));
  return EntryResult::MakeError(net::ERR_IO_PENDING);
}

EntryResult SqlBackendImpl::OpenEntry(const std::string& key,
                                      net::RequestPriority priority,
                                      EntryResultCallback callback) {
  CacheEntryKey entry_key(key);
  // If the entry is already active (open), add a reference and return it.
  if (SqlEntryImpl* entry = GetActiveEntry(entry_key)) {
    entry->AddRef();
    return EntryResult::MakeOpened(entry);
  }
  // Otherwise, insert a callback to be invoked when the store operation
  // finishes.
  InsertEntryResultCallback(entry_key, std::move(callback));
  // Ask the store to open the entry.
  store_->OpenEntry(
      entry_key,
      base::BindOnce(&SqlBackendImpl::OnOptionalEntryOperationFinished,
                     base::Unretained(this), entry_key));
  return EntryResult::MakeError(net::ERR_IO_PENDING);
}

SqlEntryImpl* SqlBackendImpl::GetActiveEntry(const CacheEntryKey& key) {
  if (auto it = active_entries_.find(key); it != active_entries_.end()) {
    // Return a pointer to the SqlEntryImpl if found.
    return &it->second.get();
  }
  return nullptr;
}

SqlBackendImpl::EntryResultCallbackInfo*
SqlBackendImpl::GetEntryResultCallbackInfo(const CacheEntryKey& key) {
  if (auto it = entry_result_callback_info_map_.find(key);
      it != entry_result_callback_info_map_.end()) {
    // Return a pointer to the EntryResultCallbackInfo if found.
    return &it->second;
  }
  return nullptr;
}

EntryResult SqlBackendImpl::CreateEntry(const std::string& key,
                                        net::RequestPriority priority,
                                        EntryResultCallback callback) {
  CacheEntryKey entry_key(key);
  // If an entry with this key is already active, creation fails.
  if (GetActiveEntry(entry_key)) {
    return EntryResult::MakeError(net::ERR_FAILED);
  }
  // Insert a callback to be invoked when the store operation finishes.
  InsertEntryResultCallback(entry_key, std::move(callback));
  // Ask the store to create the entry.
  store_->CreateEntry(entry_key,
                      base::BindOnce(&SqlBackendImpl::OnEntryOperationFinished,
                                     base::Unretained(this), entry_key));
  return EntryResult::MakeError(net::ERR_IO_PENDING);
}

void SqlBackendImpl::DoomActiveEntry(SqlEntryImpl& entry,
                                     CompletionOnceCallback callback) {
  // Mark the entry as doomed internally.
  entry.MarkAsDoomed();
  // Move it from the active_entries_ map to the doomed_entries_ set.
  ReleaseActiveEntry(entry);
  doomed_entries_.emplace(entry);
  // Ask the store to mark the entry as doomed in the database.
  store_->DoomEntry(
      entry.cache_key(), entry.token(),
      base::BindOnce(
          [](CompletionOnceCallback callback, SqlPersistentStore::Error error) {
            if (callback) {
              // Return net::OK even if the entry is not found. This matches
              // the behavior of SimpleCache. This is tested by
              // BackendFailedOpenThenMultipleDoomsNonExistentEntry in
              // DiskCacheGenericBackendTest.
              std::move(callback).Run(net::OK);
            }
          },
          std::move(callback)));
}

net::Error SqlBackendImpl::DoomEntry(const std::string& key,
                                     net::RequestPriority priority,
                                     CompletionOnceCallback callback) {
  CacheEntryKey entry_key(key);
  // If the entry is currently active, doom it directly.
  if (auto* active_entry = GetActiveEntry(entry_key)) {
    DoomActiveEntry(*active_entry, std::move(callback));
    return net::ERR_IO_PENDING;
  }

  // If there's a pending Open/Create operation for this key, queue the doom
  // operation to be executed after the initial operation completes.
  if (auto* callback_info = GetEntryResultCallbackInfo(entry_key)) {
    callback_info->pending_doom_operations.emplace_back(std::move(callback));
    return net::ERR_IO_PENDING;
  }

  // If the entry is not active and no operation is pending, it means the entry
  // is not currently open. In this case, we can directly ask the store to
  // delete the "live" (not yet doomed) entry from the database.
  store_->DeleteLiveEntry(
      entry_key,
      base::BindOnce(&SqlBackendImpl::OnDoomEntryFinished,
                     base::Unretained(this), entry_key, std::move(callback)));
  return net::ERR_IO_PENDING;
}

net::Error SqlBackendImpl::DoomAllEntries(CompletionOnceCallback callback) {
  // DoomAllEntries is a special case of DoomEntriesBetween with an unbounded
  // time range.
  return DoomEntriesBetween(base::Time::Min(), base::Time::Max(),
                            std::move(callback));
}

net::Error SqlBackendImpl::DoomEntriesBetween(base::Time initial_time,
                                              base::Time end_time,
                                              CompletionOnceCallback callback) {
  if (initial_time.is_null()) {
    // If initial_time is null, use the minimum possible time.
    initial_time = base::Time::Min();
  }
  if (end_time.is_null()) {
    // If end_time is null, use the maximum possible time.
    end_time = base::Time::Max();
  }

  // Optimization: If dooming all entries (min to max time) and there are no
  // active, doomed, or pending entries, we can directly ask the store to
  // delete all entries, which is more efficient.
  if (initial_time.is_min() && end_time.is_max() && active_entries_.empty() &&
      doomed_entries_.empty() && entry_result_callback_info_map_.empty()) {
    // Ask the store to delete all entries from the database.
    store_->DeleteAllEntries(base::BindOnce(
        [](CompletionOnceCallback callback, SqlPersistentStore::Error result) {
          std::move(callback).Run(result == SqlPersistentStore::Error::kOk
                                      ? net::OK
                                      : net::ERR_FAILED);
        },
        std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  // Collect keys of active entries to exclude them from the store's
  // DeleteLiveEntriesBetween operation, as they will be handled by
  // DoomActiveEntry.
  std::set<CacheEntryKey> excluded_keys;
  std::vector<SqlEntryImpl*> active_entries_to_be_doomed;
  for (auto& it : active_entries_) {
    excluded_keys.insert(it.first);
    // Check if the active entry falls within the specified time range.
    const base::Time last_used_time = it.second->LastUsedTime();
    if (last_used_time >= initial_time && last_used_time < end_time) {
      active_entries_to_be_doomed.push_back(&it.second.get());
    }
  }

  auto barrier_callback = base::BarrierCallback<int>(
      active_entries_to_be_doomed.size() +  // For active entries being doomed
          entry_result_callback_info_map_.size() +  // For pending operations
          1,  // For store's DeleteLiveEntriesBetween
      base::BindOnce(
          // This final callback is run after all individual doom operations
          // complete.
          [](base::WeakPtr<SqlBackendImpl> weak_ptr,
             CompletionOnceCallback callback, std::vector<int> result) {
            if (weak_ptr) {
              std::move(callback).Run(net::OK);
            }
          },
          weak_factory_.GetWeakPtr(), std::move(callback)));

  // Doom active entries that fall within the time range.
  for (auto* entry : active_entries_to_be_doomed) {
    DoomActiveEntry(*entry, barrier_callback);
  }

  // For entries with pending Open/Create operations, queue a doom operation
  // to be executed after the initial operation completes.
  for (auto& it : entry_result_callback_info_map_) {
    excluded_keys.insert(it.first);
    it.second.pending_doom_operations.emplace_back(initial_time, end_time,
                                                   barrier_callback);
  }

  // Ask the store to delete all other "live" (not doomed, not active, not
  // pending) entries within the specified time range, excluding those already
  // handled.
  store_->DeleteLiveEntriesBetween(
      initial_time, end_time, excluded_keys,
      base::BindOnce(
          [](CompletionOnceCallback callback,
             SqlPersistentStore::Error result) {
            std::move(callback).Run(result == SqlPersistentStore::Error::kOk
                                        ? net::OK
                                        : net::ERR_FAILED);
          },
          std::move(barrier_callback)));
  return net::ERR_IO_PENDING;
}

net::Error SqlBackendImpl::DoomEntriesSince(base::Time initial_time,
                                            CompletionOnceCallback callback) {
  // DoomEntriesSince is a special case of DoomEntriesBetween with end_time set
  // to the maximum possible time.
  return DoomEntriesBetween(initial_time, base::Time::Max(),
                            std::move(callback));
}

int64_t SqlBackendImpl::CalculateSizeOfAllEntries(
    Int64CompletionOnceCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int64_t SqlBackendImpl::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    Int64CompletionOnceCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

std::unique_ptr<Backend::Iterator> SqlBackendImpl::CreateIterator() {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return nullptr;
}

void SqlBackendImpl::GetStats(base::StringPairs* stats) {
  stats->emplace_back(std::make_pair("Cache type", "SQL Cache"));
  // TODO(crbug.com/422065015): Write more stats.
}

void SqlBackendImpl::OnExternalCacheHit(const std::string& key) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
}

void SqlBackendImpl::OnOptionalEntryOperationFinished(
    const CacheEntryKey& key,
    SqlPersistentStore::OptionalEntryInfoOrError result) {
  // Retrieve the callback info for this key. It must exist.
  auto it = entry_result_callback_info_map_.find(key);
  CHECK(it != entry_result_callback_info_map_.end());
  // Move out the callback and pending doom operations.
  EntryResultCallback callback = std::move(it->second.callback);
  auto pending_doom_operations = std::move(it->second.pending_doom_operations);
  // Remove the entry from the map as the operation is now finished.
  entry_result_callback_info_map_.erase(it);

  // If the store operation failed or the entry was not found (for OpenEntry).
  if (!result.has_value() || !result->has_value()) {
    // Run any pending doom operations with net::OK, as there's no entry to
    // doom.
    for (auto& doom_operation : pending_doom_operations) {
      std::move(doom_operation.callback).Run(net::OK);
    }
    std::move(callback).Run(EntryResult::MakeError(net::ERR_FAILED));
    return;
  }

  // Create a new SqlEntryImpl instance.
  scoped_refptr<SqlEntryImpl> new_entry = base::MakeRefCounted<SqlEntryImpl>(
      weak_factory_.GetWeakPtr(), key, (*result)->token, (*result)->last_used,
      (*result)->body_end, (*result)->head);
  // Add a reference for passing to the `callback`.
  new_entry->AddRef();
  // Add the new entry to the active_entries_ map.
  auto insert_result = active_entries_.insert(
      std::make_pair(key, raw_ref<SqlEntryImpl>(*new_entry.get())));
  CHECK(insert_result.second);

  // Handle any doom operations that were queued while this entry was being
  // opened/created.
  if (!pending_doom_operations.empty()) {
    // Check if the newly opened/created entry needs to be doomed based on its
    // last_used time and the time ranges of the pending doom operations.
    bool need_to_doom = false;
    const base::Time last_used_time = new_entry->LastUsedTime();
    for (auto& doom_operation : pending_doom_operations) {
      if (last_used_time >= doom_operation.initial_time &&
          last_used_time < doom_operation.end_time) {
        need_to_doom = true;
        break;
      }
    }
    if (need_to_doom) {
      // If the entry needs to be doomed, call DoomActiveEntry. The callback
      // will run all pending doom operation callbacks.
      DoomActiveEntry(
          *new_entry,
          base::BindOnce(
              [](std::vector<PendingDoomOperation> pending_doom_operations,
                 int result) {
                for (auto& doom_operation : pending_doom_operations) {
                  std::move(doom_operation.callback).Run(result);
                }
              },
              std::move(pending_doom_operations)));
    } else {
      // If the entry doesn't need to be doomed, run the pending doom
      // operation callbacks with net::OK.
      for (auto& doom_operation : pending_doom_operations) {
        std::move(doom_operation.callback).Run(net::OK);
      }
    }
  }
  // Run the original callback with the newly created/opened entry.
  std::move(callback).Run((*result)->opened
                              ? EntryResult::MakeOpened(new_entry.get())
                              : EntryResult::MakeCreated(new_entry.get()));

  // TODO(crbug.com/422065015): Consider triggering eviction.
}
void SqlBackendImpl::OnEntryOperationFinished(
    const CacheEntryKey& key,
    SqlPersistentStore::EntryInfoOrError result) {
  // This is a helper to adapt EntryInfoOrError to
  // OnOptionalEntryOperationFinished which expects OptionalEntryInfoOrError.
  if (result.has_value()) {
    OnOptionalEntryOperationFinished(key, std::move(*result));
  } else {
    OnOptionalEntryOperationFinished(key, base::unexpected(result.error()));
  }
}

void SqlBackendImpl::OnDoomEntryFinished(const CacheEntryKey& key,
                                         CompletionOnceCallback callback,
                                         SqlPersistentStore::Error result) {
  // Convert store error to net error. kNotFound is considered a success for
  // dooming (idempotency).
  std::move(callback).Run((result == SqlPersistentStore::Error::kOk ||
                           result == SqlPersistentStore::Error::kNotFound)
                              ? net::OK
                              : net::ERR_FAILED);
}

void SqlBackendImpl::ReleaseActiveEntry(SqlEntryImpl& entry) {
  auto it = active_entries_.find(entry.cache_key());
  // The entry must exist in the active_entries_ map.
  CHECK(it != active_entries_.end());
  CHECK_EQ(&it->second.get(), &entry);
  active_entries_.erase(it);
}

void SqlBackendImpl::ReleaseDoomedEntry(SqlEntryImpl& entry) {
  auto it = doomed_entries_.find(entry);
  // The entry must exist in the doomed_entries_ set.
  CHECK(it != doomed_entries_.end());
  doomed_entries_.erase(it);
}

int SqlBackendImpl::FlushQueueForTest(CompletionOnceCallback callback) {
  background_task_runner_->PostTaskAndReply(
      // Post a no-op task to the background runner.
      FROM_HERE, base::BindOnce([]() {}),
      base::BindOnce(std::move(callback), net::OK));
  return net::ERR_IO_PENDING;
}

SqlPersistentStore& SqlBackendImpl::GetStore() {
  CHECK(store_);
  return *store_;
}

SqlBackendImpl::PendingDoomOperation::PendingDoomOperation(
    CompletionOnceCallback callback)
    : callback(std::move(callback)) {}
SqlBackendImpl::PendingDoomOperation::PendingDoomOperation(
    base::Time initial_time,
    base::Time end_time,
    CompletionOnceCallback callback)
    : initial_time(initial_time),
      end_time(end_time),
      callback(std::move(callback)) {}
SqlBackendImpl::PendingDoomOperation::~PendingDoomOperation() = default;
SqlBackendImpl::PendingDoomOperation::PendingDoomOperation(
    PendingDoomOperation&&) = default;

SqlBackendImpl::EntryResultCallbackInfo::EntryResultCallbackInfo(
    EntryResultCallback callback)
    : callback(std::move(callback)) {}
SqlBackendImpl::EntryResultCallbackInfo::~EntryResultCallbackInfo() = default;
SqlBackendImpl::EntryResultCallbackInfo::EntryResultCallbackInfo(
    EntryResultCallbackInfo&&) = default;

}  // namespace disk_cache
