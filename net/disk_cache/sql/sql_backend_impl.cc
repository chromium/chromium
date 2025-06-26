// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_backend_impl.h"

#include <algorithm>
#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/sql/sql_entry_impl.h"
#include "net/disk_cache/sql/sql_persistent_store.h"

namespace disk_cache {

// An RAII-style object to manage the lifecycle of an exclusive operation.
// When an exclusive operation starts, an instance of this class is created.
// When the operation (and all its asynchronous parts) completes, the handle is
// destroyed, and its destructor calls `RunNextExclusiveOperation()` to start
// the next queued operation.
class SqlBackendImpl::ExclusiveOperationHandle {
 public:
  explicit ExclusiveOperationHandle(base::WeakPtr<SqlBackendImpl> backend)
      : backend_(std::move(backend)) {}

  ~ExclusiveOperationHandle() {
    if (backend_) {
      CHECK(backend_->exclusive_operation_inflight_);
      backend_->exclusive_operation_inflight_ = false;
      backend_->RunNextExclusiveOperation();
    }
  }

 private:
  base::WeakPtr<SqlBackendImpl> backend_;
};

// IteratorImpl provides an implementation of Backend::Iterator for the
// SqlBackendImpl. It allows iterating through cache entries stored in the
// SQLite database. Iteration is performed in reverse `res_id` order (from
// newest to oldest entry in the database).
class SqlBackendImpl::IteratorImpl : public Backend::Iterator {
 public:
  explicit IteratorImpl(base::WeakPtr<SqlBackendImpl> backend)
      : backend_(backend) {}
  ~IteratorImpl() override = default;
  EntryResult OpenNextEntry(EntryResultCallback callback) override {
    CHECK(!callback_);
    if (!backend_) {
      return EntryResult::MakeError(net::ERR_FAILED);
    }
    callback_ = std::move(callback);
    // Schedule `DoOpenNextEntry` as an exclusive operation to ensure that
    // iteration does not conflict with other backend-wide operations (e.g.,
    // mass deletion).
    backend_->PostOrRunExclusiveOperation(base::BindOnce(
        &IteratorImpl::DoOpenNextEntry, weak_factory_.GetWeakPtr()));
    return EntryResult::MakeError(net::ERR_IO_PENDING);
  }

 private:
  // Performs the actual logic for opening the next entry. This method is
  // executed when it's its turn in the exclusive operation queue.
  void DoOpenNextEntry(std::unique_ptr<ExclusiveOperationHandle> handle) {
    if (!backend_) {
      std::move(callback_).Run(EntryResult::MakeError(net::ERR_FAILED));
      // `handle` is destroyed here, but `backend_` is null, so it's a no-op.
      return;
    }
    // Request the next entry from the persistent store. `res_id_iterator_`
    // keeps track of the last `res_id` returned, allowing the store to fetch
    // entries older than that.
    backend_->GetStore().OpenLatestEntryBeforeResId(
        res_id_iterator_,
        base::BindOnce(&IteratorImpl::OnOpenLatestEntryBeforeResIdFinished,
                       weak_factory_.GetWeakPtr(), std::move(handle)));
  }

  // Callback for `SqlPersistentStore::OpenLatestEntryBeforeResId`.
  void OnOpenLatestEntryBeforeResIdFinished(
      std::unique_ptr<ExclusiveOperationHandle> handle,
      SqlPersistentStore::OptionalEntryInfoWithIdAndKey result) {
    CHECK(callback_);
    if (!backend_) {
      std::move(callback_).Run(EntryResult::MakeError(net::ERR_FAILED));
      // `handle` is destroyed here, but `backend_` is null, so it's a no-op.
      return;
    }
    // If no more entries are found or an error occurred in the store.
    if (!result.has_value()) {
      std::move(callback_).Run(EntryResult::MakeError(net::ERR_FAILED));
      // `handle` is destroyed here, calling `RunNextExclusiveOperation()`.
      return;
    }
    const SqlPersistentStore::EntryInfoWithIdAndKey& entry_info = *result;

    // Update the iterator's cursor to the `res_id` of the current entry,
    // so the next call to `OpenLatestEntryBeforeResId` starts from here.
    res_id_iterator_ = entry_info.res_id;

    // Check if the entry is already active in `active_entries_`. If so,
    // reuse the existing `SqlEntryImpl` instance.
    if (SqlEntryImpl* entry = backend_->GetActiveEntry(entry_info.key)) {
      entry->AddRef();
      std::move(callback_).Run(EntryResult::MakeOpened(entry));
      // `handle` is destroyed here, calling `RunNextExclusiveOperation()`.
      return;
    }

    // An entry might be doomed via `DoomActiveEntry()` while this operation was
    // in-flight. If so, its token will be in `pending_doomed_entry_tokens_`.
    // Skip it and try to open the next available entry.
    if (backend_->pending_doomed_entry_tokens_.find(entry_info.info.token) !=
        backend_->pending_doomed_entry_tokens_.end()) {
      backend_->GetStore().OpenLatestEntryBeforeResId(
          res_id_iterator_,
          base::BindOnce(&IteratorImpl::OnOpenLatestEntryBeforeResIdFinished,
                         weak_factory_.GetWeakPtr(), std::move(handle)));
      return;
    }

    // This DCHECK ensures that an entry returned by the store for iteration
    // is not already in the `doomed_entries_` set. This invariant is
    // maintained by the following synchronization mechanisms:
    //
    // 1. Exclusive Operations:
    //    Mass-doom operations (`DoomEntry()`, `DoomAllEntries()`,
    //    `DoomEntriesBetween()`) are mutually exclusive with iterator
    //    operations (`OpenNextEntry()`). The `ExclusiveOperationHandle`
    //    ensures they are serialized, preventing races where an entry being
    //    iterated over is doomed by one of these operations.
    //
    // 2. Non-Exclusive `DoomActiveEntry()`:
    //    `DoomActiveEntry()` can race with `OpenNextEntry()`. This race is
    //    handled by checking for dooming at two levels:
    //    - At the store level: If `SqlPersistentStore::DoomEntry()` executes
    //      before `SqlPersistentStore::OpenLatestEntryBeforeResId()`, the entry
    //      is marked as `doomed` in the database and will not be returned.
    //    - At the backend level: If `OpenLatestEntryBeforeResId()` reads the
    //      entry first, a concurrent `DoomActiveEntry()` call adds the entry's
    //      token to `pending_doomed_entry_tokens_`. This method checks that set
    //      and will discard the entry if the token is found, preventing an
    //      entry that is being doomed from being returned by the iterator.
    DCHECK(std::none_of(
        backend_->doomed_entries_.begin(), backend_->doomed_entries_.end(),
        [&](const raw_ref<const SqlEntryImpl>& doomed_entry) {
          return doomed_entry.get().token() == entry_info.info.token;
        }));

    // If the entry is not active, create a new `SqlEntryImpl`.
    scoped_refptr<SqlEntryImpl> new_entry = base::MakeRefCounted<SqlEntryImpl>(
        backend_, entry_info.key, entry_info.info.token,
        entry_info.info.last_used, entry_info.info.body_end,
        entry_info.info.head);
    new_entry->AddRef();
    CHECK(backend_->active_entries_
              .insert(std::make_pair(entry_info.key,
                                     raw_ref<SqlEntryImpl>(*new_entry.get())))
              .second);
    // Return the newly opened entry.
    std::move(callback_).Run(EntryResult::MakeOpened(new_entry.get()));
    // `handle` is destroyed here, calling `RunNextExclusiveOperation()`.
  }

  base::WeakPtr<SqlBackendImpl> backend_;
  // The `res_id` of the last entry returned by the iterator. Used to fetch
  // entries with smaller `res_id`s in subsequent calls.
  int64_t res_id_iterator_ = std::numeric_limits<int64_t>::max();
  EntryResultCallback callback_;
  base::WeakPtrFactory<IteratorImpl> weak_factory_{this};
};

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

  // Add the entry's token to a set of pending doomed entries. This prevents
  // the entry from being re-added to `active_entries_` if it's reopened by
  // IteratorImpl before the doom operation completes in the persistent store.
  pending_doomed_entry_tokens_.insert(entry.token());

  // Ask the store to mark the entry as doomed in the database.
  store_->DoomEntry(
      entry.cache_key(), entry.token(),
      base::BindOnce(
          [](base::WeakPtr<SqlBackendImpl> weak_ptr,
             CompletionOnceCallback callback,
             const base::UnguessableToken& token,
             SqlPersistentStore::Error error) {
            if (weak_ptr) {
              weak_ptr->pending_doomed_entry_tokens_.erase(token);
            }
            if (callback) {
              // Return net::OK even if the entry is not found. This matches
              // the behavior of SimpleCache. This is tested by
              // BackendFailedOpenThenMultipleDoomsNonExistentEntry in
              // DiskCacheGenericBackendTest.
              std::move(callback).Run(net::OK);
            }
          },
          weak_factory_.GetWeakPtr(), std::move(callback), entry.token()));
}

net::Error SqlBackendImpl::DoomEntry(const std::string& key,
                                     net::RequestPriority priority,
                                     CompletionOnceCallback callback) {
  PostOrRunExclusiveOperation(base::BindOnce(&SqlBackendImpl::DoomEntryInternal,
                                             weak_factory_.GetWeakPtr(), key,
                                             priority, std::move(callback)));
  return net::ERR_IO_PENDING;
}

void SqlBackendImpl::DoomEntryInternal(
    const std::string& key,
    net::RequestPriority priority,
    CompletionOnceCallback callback,
    std::unique_ptr<ExclusiveOperationHandle> handle) {
  CacheEntryKey entry_key(key);
  // If the entry is currently active, doom it directly.
  if (auto* active_entry = GetActiveEntry(entry_key)) {
    DoomActiveEntry(*active_entry, std::move(callback));
    // `handle` is destroyed here, calling `RunNextExclusiveOperation()`.
    return;
  }

  // If there's a pending Open/Create operation for this key, queue the doom
  // operation to be executed after the initial operation completes.
  if (auto* callback_info = GetEntryResultCallbackInfo(entry_key)) {
    callback_info->pending_doom_operations.emplace_back(std::move(callback));
    // `handle` is destroyed here, calling `RunNextExclusiveOperation()`.
    return;
  }

  // If the entry is not active and no operation is pending, it means the entry
  // is not currently open. In this case, we can directly ask the store to
  // delete the "live" (not yet doomed) entry from the database.
  store_->DeleteLiveEntry(
      entry_key, base::BindOnce(&SqlBackendImpl::OnDoomEntryFinished,
                                weak_factory_.GetWeakPtr(), entry_key,
                                std::move(callback), std::move(handle)));
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
  PostOrRunExclusiveOperation(base::BindOnce(
      &SqlBackendImpl::DoomEntriesBetweenInternal, weak_factory_.GetWeakPtr(),
      initial_time, end_time, std::move(callback)));
  return net::ERR_IO_PENDING;
}

void SqlBackendImpl::DoomEntriesBetweenInternal(
    base::Time initial_time,
    base::Time end_time,
    CompletionOnceCallback callback,
    std::unique_ptr<ExclusiveOperationHandle> handle) {
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
        [](CompletionOnceCallback callback,
           std::unique_ptr<ExclusiveOperationHandle> handle,
           SqlPersistentStore::Error result) {
          std::move(callback).Run(result == SqlPersistentStore::Error::kOk
                                      ? net::OK
                                      : net::ERR_FAILED);
          // `handle` is destroyed here, calling `RunNextExclusiveOperation()`.
        },
        std::move(callback), std::move(handle)));
    return;
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
             CompletionOnceCallback callback,
             std::unique_ptr<ExclusiveOperationHandle> handle,
             const std::vector<int>& result) {
            if (weak_ptr) {
              std::move(callback).Run(net::OK);
            }
            // `handle` is destroyed here, calling
            // `RunNextExclusiveOperation()`.
          },
          weak_factory_.GetWeakPtr(), std::move(callback), std::move(handle)));

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
  return std::make_unique<IteratorImpl>(weak_factory_.GetWeakPtr());
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

void SqlBackendImpl::OnDoomEntryFinished(
    const CacheEntryKey& key,
    CompletionOnceCallback callback,
    std::unique_ptr<ExclusiveOperationHandle> handle,
    SqlPersistentStore::Error result) {
  // Convert store error to net error. kNotFound is considered a success for
  // dooming (idempotency).
  std::move(callback).Run((result == SqlPersistentStore::Error::kOk ||
                           result == SqlPersistentStore::Error::kNotFound)
                              ? net::OK
                              : net::ERR_FAILED);
  // `handle` is destroyed here, calling `RunNextExclusiveOperation()`.
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
  store_->DeleteDoomedEntry(entry.cache_key(), entry.token(),
                            base::DoNothing());
}

int SqlBackendImpl::FlushQueueForTest(CompletionOnceCallback callback) {
  background_task_runner_->PostTaskAndReply(
      // Post a no-op task to the background runner.
      FROM_HERE, base::BindOnce([]() {}),
      base::BindOnce(std::move(callback), net::OK));
  return net::ERR_IO_PENDING;
}

void SqlBackendImpl::PostOrRunExclusiveOperation(
    base::OnceCallback<void(std::unique_ptr<ExclusiveOperationHandle>)>
        operation) {
  pending_exclusive_operations_.push(std::move(operation));
  // If no exclusive operation is currently running, start this one.
  if (!exclusive_operation_inflight_) {
    RunNextExclusiveOperation();
  }
}

void SqlBackendImpl::RunNextExclusiveOperation() {
  CHECK(!exclusive_operation_inflight_);
  if (pending_exclusive_operations_.empty()) {
    return;
  }

  exclusive_operation_inflight_ = true;
  auto operation = std::move(pending_exclusive_operations_.front());
  pending_exclusive_operations_.pop();
  std::move(operation).Run(
      std::make_unique<ExclusiveOperationHandle>(weak_factory_.GetWeakPtr()));
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
