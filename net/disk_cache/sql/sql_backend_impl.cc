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
namespace {

// A helper to handle methods that may complete synchronously.
//
// This allows a caller to dispatch an async operation and immediately check if
// it completed synchronously. If so, the result is returned directly. If not,
// a provided callback is invoked later.
template <typename T>
class SyncResultReceiver : public base::RefCounted<SyncResultReceiver<T>> {
 public:
  using ResultCallback = base::OnceCallback<void(T)>;

  explicit SyncResultReceiver(ResultCallback callback)
      : callback_(std::move(callback)) {}

  ~SyncResultReceiver() {
    // As a contract, FinishSyncCall() must be called. If it's not, it's a
    // bug in the calling code.
    CHECK(sync_call_finished_);
  }

  SyncResultReceiver(const SyncResultReceiver&) = delete;
  SyncResultReceiver& operator=(const SyncResultReceiver&) = delete;

  // Returns a callback to pass to the async operation.
  ResultCallback GetCallback() {
    return base::BindOnce(&SyncResultReceiver::OnResult, this);
  }

  // Checks for a synchronous result. If the operation already completed,
  // returns the result. Otherwise, returns nullopt and the original callback
  // will be run asynchronously.
  std::optional<T> FinishSyncCall() {
    sync_call_finished_ = true;
    if (result_) {
      callback_.Reset();
      return std::move(result_);
    }
    return std::nullopt;
  }

 private:
  // Receives the result from the async operation.
  void OnResult(T result) {
    if (sync_call_finished_) {
      // The caller is already waiting for the async result.
      if (callback_) {
        std::move(callback_).Run(std::move(result));
      }
    } else {
      // The result arrived synchronously. Store it for FinishSyncCall.
      result_ = std::move(result);
    }
  }

  // The original callback, to be run on async completion.
  ResultCallback callback_;
  // Holds the result if it arrives synchronously.
  std::optional<T> result_;
  // Set to true when FinishSyncCall is called.
  bool sync_call_finished_ = false;
};

// Creates a `base::OnceClosure` that takes ownership of an `OperationHandle`.
// When the closure is run, the handle is destroyed, signaling the completion
// of the operation to the `ExclusiveOperationCoordinator`. This is typically
// used with `base::OnceCallback::Then()` to ensure the handle is released only
// after the primary callback has finished.
base::OnceClosure DoNothingWithBoundHandle(
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  return base::OnceClosure(base::DoNothingWithBoundArgs(std::move(handle)));
}

}  // namespace

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
    backend_->exclusive_operation_coordinator_.PostOrRunExclusiveOperation(
        base::BindOnce(&IteratorImpl::DoOpenNextEntry,
                       weak_factory_.GetWeakPtr()));
    return EntryResult::MakeError(net::ERR_IO_PENDING);
  }

 private:
  // Performs the actual logic for opening the next entry. This method is
  // executed when it's its turn in the exclusive operation queue.
  void DoOpenNextEntry(
      std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
    if (!backend_) {
      std::move(callback_).Run(EntryResult::MakeError(net::ERR_FAILED));
      // `handle` is destroyed here, but `backend_` is null, so it's a no-op.
      return;
    }
    // Request the next entry from the persistent store. `res_id_iterator_`
    // keeps track of the last `res_id` returned, allowing the store to fetch
    // entries older than that.
    backend_->store_->OpenLatestEntryBeforeResId(
        res_id_iterator_,
        base::BindOnce(&IteratorImpl::OnOpenLatestEntryBeforeResIdFinished,
                       weak_factory_.GetWeakPtr(), std::move(handle)));
  }

  // Callback for `SqlPersistentStore::OpenLatestEntryBeforeResId`.
  void OnOpenLatestEntryBeforeResIdFinished(
      std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle,
      SqlPersistentStore::OptionalEntryInfoWithIdAndKey result) {
    CHECK(callback_);
    if (!backend_) {
      std::move(callback_).Run(EntryResult::MakeError(net::ERR_FAILED));
      // `handle` is destroyed here, may be triggering queued operations.
      return;
    }
    // If no more entries are found or an error occurred in the store.
    if (!result.has_value()) {
      std::move(callback_).Run(EntryResult::MakeError(net::ERR_FAILED));
      // `handle` is destroyed here, may be triggering queued operations.
      return;
    }
    SqlPersistentStore::EntryInfoWithIdAndKey& entry_info = *result;

    // Update the iterator's cursor to the `res_id` of the current entry,
    // so the next call to `OpenLatestEntryBeforeResId` starts from here.
    res_id_iterator_ = entry_info.res_id;

    // Check if the entry is already active in `active_entries_`. If so,
    // reuse the existing `SqlEntryImpl` instance.
    if (SqlEntryImpl* entry = backend_->GetActiveEntry(entry_info.key)) {
      entry->AddRef();
      // Reset `handle` here to trigger queued operations. This is intended not
      // to starve normal operations.
      // TODO(crbug.com/422065015): Resetting the handle here introduces
      // complexities, such as the possibility of passing a doomed entry to the
      // callback, which makes the behavior harder to reason about. We should
      // remove this handle reset once a more sophisticated scheduling mechanism
      // is implemented in ExclusiveOperationCoordinator.
      handle.reset();
      std::move(callback_).Run(EntryResult::MakeOpened(entry));
      return;
    }

    // This DCHECK ensures that an entry returned by the store for iteration
    // is not already in the `doomed_entries_` set. This invariant is
    // maintained because iterator operations are "exclusive" and dooming
    // operations are "normal", and the `ExclusiveOperationCoordinator`
    // ensures they do not run concurrently. If a doom operation runs first,
    // the entry is marked as doomed in the database and
    // `OpenLatestEntryBeforeResId` will not return it. If the iterator
    // operation runs first, any subsequent doom operation will be queued until
    // the iteration step is complete.
    DCHECK(std::none_of(
        backend_->doomed_entries_.begin(), backend_->doomed_entries_.end(),
        [&](const raw_ref<const SqlEntryImpl>& doomed_entry) {
          return doomed_entry.get().token() == entry_info.info.token;
        }));

    // Apply any in-flight modifications (e.g., last_used time updates, header
    // changes) that were queued for this entry while it was not active.
    backend_->ApplyInFlightEntryModifications(entry_info.key, entry_info.info);

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
    // Reset `handle` here to trigger queued operations. This is intended not to
    // starve normal operations.
    // TODO(crbug.com/422065015): Resetting the handle here introduces
    // complexities, such as the possibility of passing a doomed entry to the
    // callback, which makes the behavior harder to reason about. We should
    // remove this handle reset once a more sophisticated scheduling mechanism
    // is implemented in ExclusiveOperationCoordinator.
    handle.reset();
    // Return the newly opened entry.
    std::move(callback_).Run(EntryResult::MakeOpened(new_entry.get()));
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

EntryResult SqlBackendImpl::OpenOrCreateEntry(const std::string& key,
                                              net::RequestPriority priority,
                                              EntryResultCallback callback) {
  return OpenOrCreateEntryInternal(
      OpenOrCreateEntryOperationType::kOpenOrCreateEntry, key,
      std::move(callback));
}

EntryResult SqlBackendImpl::OpenEntry(const std::string& key,
                                      net::RequestPriority priority,
                                      EntryResultCallback callback) {
  return OpenOrCreateEntryInternal(OpenOrCreateEntryOperationType::kOpenEntry,
                                   key, std::move(callback));
}

EntryResult SqlBackendImpl::CreateEntry(const std::string& key,
                                        net::RequestPriority priority,
                                        EntryResultCallback callback) {
  return OpenOrCreateEntryInternal(OpenOrCreateEntryOperationType::kCreateEntry,
                                   key, std::move(callback));
}

EntryResult SqlBackendImpl::OpenOrCreateEntryInternal(
    OpenOrCreateEntryOperationType type,
    const std::string& key,
    EntryResultCallback callback) {
  const CacheEntryKey entry_key(key);
  auto sync_result_receiver =
      base::MakeRefCounted<SyncResultReceiver<EntryResult>>(
          std::move(callback));
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      entry_key,
      base::BindOnce(&SqlBackendImpl::HandleOpenOrCreateEntryOperation,
                     weak_factory_.GetWeakPtr(), type, entry_key,
                     sync_result_receiver->GetCallback()));
  auto sync_result = sync_result_receiver->FinishSyncCall();
  return sync_result ? std::move(*sync_result)
                     : EntryResult::MakeError(net::ERR_IO_PENDING);
}

void SqlBackendImpl::HandleOpenOrCreateEntryOperation(
    OpenOrCreateEntryOperationType type,
    const CacheEntryKey& entry_key,
    EntryResultCallback callback,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  if (SqlEntryImpl* entry = GetActiveEntry(entry_key)) {
    if (type == OpenOrCreateEntryOperationType::kCreateEntry) {
      std::move(callback).Run(EntryResult::MakeError(net::ERR_FAILED));
    } else {
      entry->AddRef();
      std::move(callback).Run(EntryResult::MakeOpened(entry));
    }
    return;
  }

  switch (type) {
    case OpenOrCreateEntryOperationType::kOpenOrCreateEntry:
      store_->OpenOrCreateEntry(
          entry_key, base::BindOnce(&SqlBackendImpl::OnEntryOperationFinished,
                                    base::Unretained(this), entry_key,
                                    std::move(callback), std::move(handle)));
      break;
    case OpenOrCreateEntryOperationType::kOpenEntry:
      store_->OpenEntry(
          entry_key,
          base::BindOnce(&SqlBackendImpl::OnOptionalEntryOperationFinished,
                         base::Unretained(this), entry_key, std::move(callback),
                         std::move(handle)));
      break;
    case OpenOrCreateEntryOperationType::kCreateEntry:
      store_->CreateEntry(
          entry_key, base::BindOnce(&SqlBackendImpl::OnEntryOperationFinished,
                                    base::Unretained(this), entry_key,
                                    std::move(callback), std::move(handle)));
      break;
  }
}

SqlEntryImpl* SqlBackendImpl::GetActiveEntry(const CacheEntryKey& key) {
  if (auto it = active_entries_.find(key); it != active_entries_.end()) {
    // Return a pointer to the SqlEntryImpl if found.
    return &it->second.get();
  }
  return nullptr;
}

void SqlBackendImpl::DoomActiveEntry(SqlEntryImpl& entry,
                                     CompletionOnceCallback callback) {
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      entry.cache_key(),
      base::BindOnce(&SqlBackendImpl::HandleDoomActiveEntryOperation,
                     weak_factory_.GetWeakPtr(),
                     scoped_refptr<SqlEntryImpl>(&entry), std::move(callback)));
}

void SqlBackendImpl::HandleDoomActiveEntryOperation(
    scoped_refptr<SqlEntryImpl> entry,
    CompletionOnceCallback callback,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  if (entry->doomed()) {
    return;
  }
  DoomActiveEntryInternal(
      *entry,
      std::move(callback).Then(DoNothingWithBoundHandle(std::move(handle))));
}

void SqlBackendImpl::DoomActiveEntryInternal(SqlEntryImpl& entry,
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
          [](base::WeakPtr<SqlBackendImpl> weak_ptr,
             CompletionOnceCallback callback, SqlPersistentStore::Error error) {
            // Do not call the `callback` if the backend has been destroyed.
            // This is safe for both backend-initiated and entry-initiated
            // dooms:
            // - For backend operations (e.g., `DoomEntry`), the provided
            //   callback should be cancelled if the backend is gone.
            // - For entry operations (`SqlEntryImpl::Doom`), the callback is a
            //   `base::DoNothing()`, so cancelling it has no effect.
            if (weak_ptr) {
              // Return net::OK even if the entry is not found. This matches
              // the behavior of SimpleCache. This is tested by
              // BackendFailedOpenThenMultipleDoomsNonExistentEntry in
              // DiskCacheGenericBackendTest.
              std::move(callback).Run(net::OK);
            }
          },
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

net::Error SqlBackendImpl::DoomEntry(const std::string& key,
                                     net::RequestPriority priority,
                                     CompletionOnceCallback callback) {
  const CacheEntryKey entry_key(key);
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      entry_key, base::BindOnce(&SqlBackendImpl::HandleDoomEntryOperation,
                                weak_factory_.GetWeakPtr(), entry_key, priority,
                                std::move(callback)));
  return net::ERR_IO_PENDING;
}

void SqlBackendImpl::HandleDoomEntryOperation(
    const CacheEntryKey& key,
    net::RequestPriority priority,
    CompletionOnceCallback callback,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  // If the entry is currently active, doom it directly.
  if (auto* active_entry = GetActiveEntry(key)) {
    DoomActiveEntryInternal(*active_entry, std::move(callback));
    // The handle for this operation is released upon returning, allowing the
    // next queued operation to run.
    return;
  }

  // If the entry is not active and no operation is pending, it means the entry
  // is not currently open. In this case, we can directly ask the store to
  // delete the "live" (not yet doomed) entry from the database.
  store_->DeleteLiveEntry(
      key, base::BindOnce(
               [](base::WeakPtr<SqlBackendImpl> weak_ptr,
                  CompletionOnceCallback callback,
                  SqlPersistentStore::Error result) {
                 // Convert store error to net error. kNotFound is
                 // considered a success for dooming (idempotency).
                 std::move(callback).Run(
                     (result == SqlPersistentStore::Error::kOk ||
                      result == SqlPersistentStore::Error::kNotFound)
                         ? net::OK
                         : net::ERR_FAILED);
               },
               weak_factory_.GetWeakPtr(), std::move(callback))
               .Then(DoNothingWithBoundHandle(std::move(handle))));
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
  exclusive_operation_coordinator_.PostOrRunExclusiveOperation(base::BindOnce(
      &SqlBackendImpl::HandleDoomEntriesBetweenOperation,
      weak_factory_.GetWeakPtr(), initial_time, end_time, std::move(callback)));
  return net::ERR_IO_PENDING;
}

void SqlBackendImpl::HandleDoomEntriesBetweenOperation(
    base::Time initial_time,
    base::Time end_time,
    CompletionOnceCallback callback,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
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
      doomed_entries_.empty()) {
    // Ask the store to delete all entries from the database.
    store_->DeleteAllEntries(
        base::BindOnce(
            [](CompletionOnceCallback callback,
               SqlPersistentStore::Error result) {
              std::move(callback).Run(result == SqlPersistentStore::Error::kOk
                                          ? net::OK
                                          : net::ERR_FAILED);
            },
            std::move(callback))
            .Then(DoNothingWithBoundHandle(std::move(handle))));
    return;
  }

  // Collect keys of active entries to exclude them from the store's
  // DeleteLiveEntriesBetween operation, as they will be handled by dooming them
  // directly within this method.
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
          1,  // For store's DeleteLiveEntriesBetween
      base::BindOnce(
          // This final callback is run after all individual doom operations
          // complete.
          [](base::WeakPtr<SqlBackendImpl> weak_ptr,
             CompletionOnceCallback callback, const std::vector<int>& result) {
            if (weak_ptr) {
              std::move(callback).Run(net::OK);
            }
          },
          weak_factory_.GetWeakPtr(), std::move(callback))
          .Then(DoNothingWithBoundHandle(std::move(handle))));

  // Doom active entries that fall within the time range.
  for (auto* entry : active_entries_to_be_doomed) {
    DoomActiveEntryInternal(*entry, barrier_callback);
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
    EntryResultCallback callback,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle,
    SqlPersistentStore::OptionalEntryInfoOrError result) {
  // If the store operation failed or the entry was not found (for OpenEntry).
  if (!result.has_value() || !result->has_value()) {
    std::move(callback).Run(EntryResult::MakeError(net::ERR_FAILED));
    return;
  }

  SqlPersistentStore::EntryInfo& entry_info = *(*result);
  ApplyInFlightEntryModifications(key, entry_info);

  // Create a new SqlEntryImpl instance.
  scoped_refptr<SqlEntryImpl> new_entry = base::MakeRefCounted<SqlEntryImpl>(
      weak_factory_.GetWeakPtr(), key, entry_info.token, entry_info.last_used,
      entry_info.body_end, entry_info.head);

  // Add a reference for passing to the `callback`.
  new_entry->AddRef();
  // Add the new entry to the active_entries_ map.
  auto insert_result = active_entries_.insert(
      std::make_pair(key, raw_ref<SqlEntryImpl>(*new_entry.get())));
  CHECK(insert_result.second);

  // Run the original callback with the newly created/opened entry.
  std::move(callback).Run((*result)->opened
                              ? EntryResult::MakeOpened(new_entry.get())
                              : EntryResult::MakeCreated(new_entry.get()));

  // TODO(crbug.com/422065015): Consider triggering eviction.
}

void SqlBackendImpl::OnEntryOperationFinished(
    const CacheEntryKey& key,
    EntryResultCallback callback,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle,
    SqlPersistentStore::EntryInfoOrError result) {
  // This is a helper to adapt EntryInfoOrError to
  // OnOptionalEntryOperationFinished which expects OptionalEntryInfoOrError.
  if (result.has_value()) {
    OnOptionalEntryOperationFinished(key, std::move(callback),
                                     std::move(handle), std::move(*result));
  } else {
    OnOptionalEntryOperationFinished(key, std::move(callback),
                                     std::move(handle),
                                     base::unexpected(result.error()));
  }
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
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      entry.cache_key(),
      base::BindOnce(&SqlBackendImpl::HandleDeleteDoomedEntry,
                     weak_factory_.GetWeakPtr(), entry.cache_key(),
                     entry.token()));
}

void SqlBackendImpl::HandleDeleteDoomedEntry(
    const CacheEntryKey& key,
    const base::UnguessableToken& token,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  store_->DeleteDoomedEntry(
      key, token,
      base::BindOnce(
          [](std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle>
                 handle,
             SqlPersistentStore::Error error) {},
          std::move(handle)));
}

void SqlBackendImpl::UpdateEntryLastUsed(
    const CacheEntryKey& key,
    const base::UnguessableToken& token,
    base::Time last_used,
    SqlPersistentStore::ErrorCallback callback) {
  in_flight_entry_modifications_[key].emplace_back(token, last_used);
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      key, base::BindOnce(&SqlBackendImpl::HandleUpdateEntryLastUsedOperation,
                          weak_factory_.GetWeakPtr(), key, token, last_used,
                          std::move(callback)));
}

void SqlBackendImpl::HandleUpdateEntryLastUsedOperation(
    const CacheEntryKey& key,
    const base::UnguessableToken& token,
    base::Time last_used,
    SqlPersistentStore::ErrorCallback callback,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  store_->UpdateEntryLastUsed(
      key, last_used,
      WrapErrorCallbackToPopInFlightEntryModification(key, std::move(callback))
          .Then(DoNothingWithBoundHandle(std::move(handle))));
}

void SqlBackendImpl::UpdateEntryHeaderAndLastUsed(
    const CacheEntryKey& key,
    const base::UnguessableToken& token,
    base::Time last_used,
    scoped_refptr<net::GrowableIOBuffer> buffer,
    int64_t header_size_delta,
    SqlPersistentStore::ErrorCallback callback) {
  in_flight_entry_modifications_[key].emplace_back(token, last_used, buffer);
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      key, base::BindOnce(
               &SqlBackendImpl::HandleUpdateEntryHeaderAndLastUsedOperation,
               weak_factory_.GetWeakPtr(), key, token, last_used,
               std::move(buffer), header_size_delta, std::move(callback)));
}

void SqlBackendImpl::HandleUpdateEntryHeaderAndLastUsedOperation(
    const CacheEntryKey& key,
    const base::UnguessableToken& token,
    base::Time last_used,
    scoped_refptr<net::GrowableIOBuffer> buffer,
    int64_t header_size_delta,
    SqlPersistentStore::ErrorCallback callback,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  store_->UpdateEntryHeaderAndLastUsed(
      key, token, last_used, std::move(buffer), header_size_delta,
      WrapErrorCallbackToPopInFlightEntryModification(key, std::move(callback))
          .Then(DoNothingWithBoundHandle(std::move(handle))));
}

void SqlBackendImpl::ApplyInFlightEntryModifications(
    const CacheEntryKey& key,
    SqlPersistentStore::EntryInfo& entry_info) {
  auto it = in_flight_entry_modifications_.find(key);
  if (it == in_flight_entry_modifications_.end()) {
    return;
  }
  for (const auto& modification : it->second) {
    if (modification.token == entry_info.token) {
      if (modification.last_used.has_value()) {
        entry_info.last_used = *modification.last_used;
      }
      if (modification.head.has_value()) {
        entry_info.head = *modification.head;
      }
    }
  }
}

SqlPersistentStore::ErrorCallback
SqlBackendImpl::WrapErrorCallbackToPopInFlightEntryModification(
    const CacheEntryKey& key,
    SqlPersistentStore::ErrorCallback callback) {
  return base::BindOnce(
      [](base::WeakPtr<SqlBackendImpl> weak_ptr, const CacheEntryKey& key,
         SqlPersistentStore::ErrorCallback callback,
         SqlPersistentStore::Error result) {
        if (weak_ptr) {
          // The in-flight modifications for a given key are queued and removed
          // in FIFO order. This is safe because
          // `exclusive_operation_coordinator_` serializes all normal operations
          // for the same key. This guarantees that modifications are enqueued
          // and the corresponding store operations are executed in the same
          // order.
          auto it = weak_ptr->in_flight_entry_modifications_.find(key);
          CHECK(it != weak_ptr->in_flight_entry_modifications_.end());
          CHECK(!it->second.empty());
          it->second.pop_front();
          if (it->second.empty()) {
            weak_ptr->in_flight_entry_modifications_.erase(it);
          }
        }
        std::move(callback).Run(result);
      },
      weak_factory_.GetWeakPtr(), key, std::move(callback));
}

int SqlBackendImpl::FlushQueueForTest(CompletionOnceCallback callback) {
  exclusive_operation_coordinator_.PostOrRunExclusiveOperation(base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> background_task_runner,
         CompletionOnceCallback callback,
         std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle>
             handle) {
        background_task_runner->PostTaskAndReply(
            // Post a no-op task to the background runner.
            FROM_HERE, base::BindOnce([]() {}),
            base::BindOnce(std::move(callback), net::OK)
                .Then(DoNothingWithBoundHandle(std::move(handle))));
      },
      background_task_runner_, std::move(callback)));

  return net::ERR_IO_PENDING;
}

SqlBackendImpl::InFlightEntryModification::InFlightEntryModification(
    const base::UnguessableToken& token,
    base::Time last_used)
    : token(token), last_used(last_used) {}
SqlBackendImpl::InFlightEntryModification::InFlightEntryModification(
    const base::UnguessableToken& token,
    base::Time last_used,
    scoped_refptr<net::GrowableIOBuffer> head)
    : token(token), last_used(last_used), head(std::move(head)) {}
SqlBackendImpl::InFlightEntryModification::~InFlightEntryModification() =
    default;
SqlBackendImpl::InFlightEntryModification::InFlightEntryModification(
    InFlightEntryModification&&) = default;

}  // namespace disk_cache
