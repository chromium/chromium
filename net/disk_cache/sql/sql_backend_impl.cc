// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_backend_impl.h"

#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/sql/sql_entry_impl.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "sql_backend_constants.h"

namespace disk_cache {
namespace {

using FakeIndexFileError = SqlBackendImpl::FakeIndexFileError;

size_t GetShardCount() {
  return std::max(std::min(net::features::kSqlDiskCacheShardCount.Get(), 255),
                  1);
}

// Checks the fake index file, creating it if it doesn't exist. Returns an
// error code if the file is corrupted or cannot be created.
FakeIndexFileError CheckFakeIndexFileInternal(const base::FilePath& path) {
  const std::string expected_contents = base::StrCat(
      {kSqlBackendFakeIndexPrefix, base::NumberToString(GetShardCount())});
  const base::FilePath file_path = path.Append(kSqlBackendFakeIndexFileName);
  const std::optional<int64_t> file_size = base::GetFileSize(file_path);
  if (file_size.has_value()) {
    if (file_size != expected_contents.size()) {
      return FakeIndexFileError::kWrongFileSize;
    }
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file.IsValid()) {
      return FakeIndexFileError::kOpenFileFailed;
    }
    std::vector<uint8_t> contents(expected_contents.size());
    if (!file.ReadAndCheck(0, contents)) {
      return FakeIndexFileError::kReadFileFailed;
    }
    if (base::span(contents) != base::span(expected_contents)) {
      return FakeIndexFileError::kWrongMagicNumber;
    }
    return FakeIndexFileError::kOkExisting;
  }
  if (!base::DirectoryExists(path) && !base::CreateDirectory(path)) {
    return FakeIndexFileError::kFailedToCreateDirectory;
  }
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    return FakeIndexFileError::kCreateFileFailed;
  }
  if (!file.WriteAndCheck(0, base::as_byte_span(expected_contents))) {
    return FakeIndexFileError::kWriteFileFailed;
  }
  return FakeIndexFileError::kOkNew;
}

// Checks the fake index file and records a histogram of the result.
bool CheckFakeIndexFile(const base::FilePath& path) {
  FakeIndexFileError error = CheckFakeIndexFileInternal(path);
  base::UmaHistogramEnumeration("Net.SqlDiskCache.FakeIndexFileError", error);
  return error == FakeIndexFileError::kOkNew ||
         error == FakeIndexFileError::kOkExisting;
}

// Checks if the browser is still idle. This is called within ShouldRunEviction.
// The purpose of this is to prevent operations from running if the browser is
// no longer idle in the time between SqlBackendImpl::OnBrowserIdle() being
// called and the actual processing.
bool IsBrowserIdle() {
  return performance_scenarios::CurrentScenariosMatch(
      performance_scenarios::ScenarioScope::kGlobal,
      performance_scenarios::kDefaultIdleScenarios);
}

// Determines whether cache eviction should run based on the urgency and timing.
// Eviction is triggered under three conditions:
// 1. Not needed: Eviction is skipped.
// 2. Idle time: Eviction runs only if it's an idle-time task and the browser
//    is currently idle.
// 3. Needed: Eviction runs immediately, regardless of browser state.
bool ShouldRunEviction(SqlPersistentStore::EvictionUrgency eviction_urgency,
                       bool is_idle_time_eviction) {
  switch (eviction_urgency) {
    case SqlPersistentStore::EvictionUrgency::kNotNeeded:
      return false;
    case SqlPersistentStore::EvictionUrgency::kIdleTime:
      return is_idle_time_eviction && IsBrowserIdle();
    case SqlPersistentStore::EvictionUrgency::kNeeded:
      return true;
  }
}

// Wraps a OnceCallback. If the returned callback is destroyed without being
// run, the original callback is run with `abort_result`.
// This ensures that the callback is always run, even if the operation is
// cancelled or the owner is destroyed.
template <typename ResultType>
base::OnceCallback<void(ResultType)> WrapCallbackWithAbortError(
    base::OnceCallback<void(ResultType)> callback,
    ResultType abort_result) {
  CHECK(callback);
  auto [success_cb, failure_cb] = base::SplitOnceCallback(std::move(callback));

  // The ScopedClosureRunner will run the `failure_cb` with `abort_result` if
  // it's destroyed before being released.
  auto runner = std::make_unique<base::ScopedClosureRunner>(
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(std::move(failure_cb), abort_result)));

  // The returned callback represents the "success" path.
  return base::BindOnce(
      [](std::unique_ptr<base::ScopedClosureRunner> runner,
         base::OnceCallback<void(ResultType)> cb, ResultType result) {
        // Release the runner to prevent the failure callback from running on
        // destruction.
        std::ignore = runner->Release();
        // Run the success callback with the provided result.
        std::move(cb).Run(std::move(result));
      },
      std::move(runner), std::move(success_cb));
}

// A helper to handle methods that may complete synchronously.
//
// This allows a caller to dispatch an async operation and immediately check if
// it completed synchronously. If so, the result is returned directly. If not,
// a provided callback is invoked later.
template <typename T, typename R = std::decay_t<T>>
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
  std::optional<R> FinishSyncCall() {
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
  std::optional<R> result_;
  // Set to true when FinishSyncCall is called.
  bool sync_call_finished_ = false;
};

// Creates a `base::OnceClosure` that takes ownership of `args`. When the
// closure is run, the `args` are destroyed. This is typically used with
// `base::OnceCallback::Then()` to ensure the handle is released only after the
// primary callback has finished.
template <typename... Args>
base::OnceClosure OnceClosureWithBoundArgs(Args&&... args) {
  return base::OnceClosure(
      base::DoNothingWithBoundArgs(std::forward<Args>(args)...));
}

// Retrieves the `ResId` from `res_id_or_error` if it holds a `ResId` value.
// This function should only be called after the speculative entry creation has
// completed and `res_id_or_error->data` is populated. The call is sequenced
// by the ExclusiveOperationCoordinator, which ensures that this function runs
// only after the handle for the creation operation is released.
std::optional<SqlPersistentStore::ResId> GetResId(
    const scoped_refptr<SqlBackendImpl::ResIdOrErrorHolder>& res_id_or_error) {
  CHECK(res_id_or_error);
  CHECK(res_id_or_error->data.has_value());
  if (std::holds_alternative<SqlPersistentStore::ResId>(
          res_id_or_error->data.value())) {
    return std::get<SqlPersistentStore::ResId>(res_id_or_error->data.value());
  }
  return std::nullopt;
}

// Retrieves the `Error` from `res_id_or_error` if it holds an `Error` value.
// This function should only be called after the speculative entry creation has
// completed and `res_id_or_error->data` is populated. The call is sequenced
// by the ExclusiveOperationCoordinator, which ensures that this function runs
// only after the handle for the creation operation is released.
std::optional<SqlPersistentStore::Error> GetError(
    const scoped_refptr<SqlBackendImpl::ResIdOrErrorHolder>& res_id_or_error) {
  CHECK(res_id_or_error);
  CHECK(res_id_or_error->data.has_value());
  if (std::holds_alternative<SqlPersistentStore::Error>(
          res_id_or_error->data.value())) {
    return std::get<SqlPersistentStore::Error>(res_id_or_error->data.value());
  }
  return std::nullopt;
}

std::vector<scoped_refptr<base::SequencedTaskRunner>> CreateTaskRunners() {
  const size_t shard_count = GetShardCount();
  std::vector<scoped_refptr<base::SequencedTaskRunner>> runners;
  runners.reserve(shard_count);
  for (size_t i = 0; i < shard_count; ++i) {
    runners.push_back(base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  }
  return runners;
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
    // Request the next entry from the persistent store. `entry_iterator_` keeps
    // track of the last entry returned, allowing the store to fetch the next
    // entry.
    // `handle` will be destroyed after executing`OnOpenNextEntryFinished()`,
    // may be triggering queued operations.
    backend_->store_->OpenNextEntry(
        entry_iterator_,
        base::BindOnce(&IteratorImpl::OnOpenNextEntryFinished,
                       weak_factory_.GetWeakPtr())
            .Then(OnceClosureWithBoundArgs(std::move(handle))));
  }

  // Callback for `SqlPersistentStore::OpenNextEntry`.
  void OnOpenNextEntryFinished(
      SqlPersistentStore::OptionalEntryInfoWithKeyAndIterator result) {
    CHECK(callback_);
    if (!backend_) {
      std::move(callback_).Run(EntryResult::MakeError(net::ERR_FAILED));
      return;
    }
    // If no more entries are found or an error occurred in the store.
    if (!result.has_value()) {
      std::move(callback_).Run(EntryResult::MakeError(net::ERR_FAILED));
      return;
    }
    SqlPersistentStore::EntryInfoWithKeyAndIterator& entry_info = *result;

    // Update the `entry_iterator_` to the `iterator` of the result, so the next
    // call to `OpenNextEntry` starts from here.
    entry_iterator_ = entry_info.iterator;

    // Check if the entry is already active in `active_entries_`. If so,
    // reuse the existing `SqlEntryImpl` instance.
    if (SqlEntryImpl* entry = backend_->GetActiveEntry(entry_info.key)) {
      entry->AddRef();
      std::move(callback_).Run(EntryResult::MakeOpened(entry));
      return;
    }

    // This DCHECK ensures that an entry returned by the store for iteration
    // is not already in the `doomed_entries_` set. This invariant is
    // maintained because iterator operations are "exclusive" and dooming
    // operations are "normal", and the `ExclusiveOperationCoordinator`
    // ensures they do not run concurrently. If a doom operation runs first,
    // the entry is marked as doomed in the database and `OpenNextEntry` will
    // not return it. If the iterator operation runs first, any subsequent doom
    // operation will be queued until the iteration step is complete.
    DCHECK(std::none_of(backend_->doomed_entries_.begin(),
                        backend_->doomed_entries_.end(),
                        [&](const raw_ref<const SqlEntryImpl>& doomed_entry) {
                          const auto optional_res_id =
                              GetResId(doomed_entry.get().res_id_or_error());
                          return optional_res_id.has_value() &&
                                 backend_->store_->GetShardIdForHash(
                                     doomed_entry.get().cache_key().hash()) ==
                                     backend_->store_->GetShardIdForHash(
                                         entry_info.key.hash()) &&
                                 *optional_res_id == entry_info.info.res_id;
                        }));

    // Apply any in-flight modifications (e.g., last_used time updates, header
    // changes) that were queued for this entry while it was not active.
    backend_->ApplyInFlightEntryModifications(entry_info.key, entry_info.info);

    // If the entry is not active, create a new `SqlEntryImpl`.
    scoped_refptr<SqlEntryImpl> new_entry = base::MakeRefCounted<SqlEntryImpl>(
        backend_, entry_info.key,
        base::MakeRefCounted<ResIdOrErrorHolder>(entry_info.info.res_id),
        entry_info.info.last_used, entry_info.info.body_end,
        entry_info.info.head);
    new_entry->AddRef();
    CHECK(backend_->active_entries_
              .insert(std::make_pair(entry_info.key,
                                     raw_ref<SqlEntryImpl>(*new_entry.get())))
              .second);

    // Return the newly opened entry.
    std::move(callback_).Run(EntryResult::MakeOpened(new_entry.get()));
  }

  base::WeakPtr<SqlBackendImpl> backend_;
  // The `entry_iterator` of the last entry returned by the iterator. Used to
  // fetch the next entry in subsequent calls.
  SqlPersistentStore::EntryIterator entry_iterator_;
  EntryResultCallback callback_;
  base::WeakPtrFactory<IteratorImpl> weak_factory_{this};
};

SqlBackendImpl::SqlBackendImpl(const base::FilePath& path,
                               int64_t max_bytes,
                               net::CacheType cache_type)
    : Backend(cache_type),
      path_(path),
      background_task_runners_(CreateTaskRunners()),
      store_(std::make_unique<SqlPersistentStore>(path,
                                                  max_bytes > 0 ? max_bytes : 0,
                                                  GetCacheType(),
                                                  background_task_runners_)) {
  DVLOG(1) << "SqlBackendImpl::SqlBackendImpl " << path;
}

SqlBackendImpl::~SqlBackendImpl() = default;

void SqlBackendImpl::Init(CompletionOnceCallback callback) {
  auto barrier_callback = base::BarrierCallback<bool>(
      2, base::BindOnce(&SqlBackendImpl::OnInitialized,
                        weak_factory_.GetWeakPtr(), std::move(callback)));

  store_->Initialize(base::BindOnce([](SqlPersistentStore::Error result) {
                       return result == SqlPersistentStore::Error::kOk;
                     }).Then(barrier_callback));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&CheckFakeIndexFile, path_),
      base::OnceCallback<void(bool)>(barrier_callback));
}

void SqlBackendImpl::OnInitialized(CompletionOnceCallback callback,
                                   const std::vector<bool>& results) {
  const bool success = std::all_of(results.begin(), results.end(),
                                   [](bool result) { return result; });
  if (success) {
    // Schedule a one-time task to load in-memory index and clean up doomed
    // entries from previous sessions. This runs after a delay to avoid
    // impacting startup performance. This is especially important for Android
    // WebView where Performance Scenario Detection doesn't work. See
    // https://crbug.com/456009994 for more details.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SqlBackendImpl::RunDelayedPostInitializationTasks,
                       weak_factory_.GetWeakPtr()),
        kSqlBackendPostInitializationTasksDelay);
  }
  std::move(callback).Run(success ? net::OK : net::ERR_FAILED);
}

void SqlBackendImpl::RunDelayedPostInitializationTasks() {
  if (store_->MaybeLoadInMemoryIndex(base::BindOnce(
          [](base::WeakPtr<SqlBackendImpl> self,
             SqlPersistentStore::Error result) {
            if (self && result == SqlPersistentStore::Error::kOk) {
              self->store_->MaybeRunCleanupDoomedEntries(base::DoNothing());
            }
          },
          weak_factory_.GetWeakPtr()))) {
    return;
  }
  // The in-memory index has been already read. So trigger clean up task.
  store_->MaybeRunCleanupDoomedEntries(base::DoNothing());
}

int64_t SqlBackendImpl::MaxFileSize() const {
  // Delegates to the persistent store to get the max file size.
  return store_->MaxFileSize();
}

int32_t SqlBackendImpl::GetEntryCount(
    net::Int32CompletionOnceCallback callback) const {
  // The entry count must be retrieved asynchronously to ensure that all
  // pending database operations are reflected in the result.
  store_->GetEntryCountAsync(std::move(callback));
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

  if (store_->GetIndexStateForHash(entry_key.hash()) ==
      SqlPersistentStore::IndexState::kHashNotFound) {
    if (type == OpenOrCreateEntryOperationType::kOpenEntry) {
      std::move(callback).Run(EntryResult::MakeError(net::ERR_FAILED));
      return;
    }
    std::move(callback).Run(
        SpeculativeCreateEntry(entry_key, std::move(handle)));
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
          entry_key, base::Time::Now(),
          base::BindOnce(&SqlBackendImpl::OnEntryOperationFinished,
                         base::Unretained(this), entry_key, std::move(callback),
                         std::move(handle)));
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

void SqlBackendImpl::DoomActiveEntry(SqlEntryImpl& entry) {
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      entry.cache_key(),
      base::BindOnce(&SqlBackendImpl::HandleDoomActiveEntryOperation,
                     weak_factory_.GetWeakPtr(),
                     scoped_refptr<SqlEntryImpl>(&entry)));
}

void SqlBackendImpl::HandleDoomActiveEntryOperation(
    scoped_refptr<SqlEntryImpl> entry,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  if (entry->doomed()) {
    return;
  }
  DoomActiveEntryInternal(*entry,
                          base::DoNothingWithBoundArgs(std::move(handle)));
}

void SqlBackendImpl::DoomActiveEntryInternal(SqlEntryImpl& entry,
                                             CompletionOnceCallback callback) {
  // Mark the entry as doomed internally.
  entry.MarkAsDoomed();
  // Move it from the active_entries_ map to the doomed_entries_ set.
  ReleaseActiveEntry(entry);
  doomed_entries_.emplace(entry);

  const auto optional_res_id = GetResId(entry.res_id_or_error());
  if (!optional_res_id) {
    // Fail the operation for entries that previously failed a speculative
    // creation or optimistic write.
    CHECK(GetError(entry.res_id_or_error()).has_value());
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  // Ask the store to mark the entry as doomed in the database.
  store_->DoomEntry(
      entry.cache_key(), *optional_res_id,
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

  auto sync_result_receiver =
      base::MakeRefCounted<SyncResultReceiver<int>>(std::move(callback));
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      entry_key, base::BindOnce(&SqlBackendImpl::HandleDoomEntryOperation,
                                weak_factory_.GetWeakPtr(), entry_key, priority,
                                sync_result_receiver->GetCallback()));
  auto sync_result = sync_result_receiver->FinishSyncCall();
  return sync_result ? static_cast<net::Error>(std::move(*sync_result))
                     : net::ERR_IO_PENDING;
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

  // Convert store error to net error. kNotFound is considered a success for
  // dooming (idempotency).
  auto store_callback = base::BindOnce(
      [](CompletionOnceCallback callback, SqlPersistentStore::Error result) {
        std::move(callback).Run((result == SqlPersistentStore::Error::kOk ||
                                 result == SqlPersistentStore::Error::kNotFound)
                                    ? net::OK
                                    : net::ERR_FAILED);
      },
      std::move(callback));

  // If there is a unique entry in the in-memory index, call DoomEntry using its
  // res_id.
  if (auto res_id = store_->TryGetSingleResIdFromInMemoryIndex(key.hash())) {
    store_->DoomEntry(key, *res_id, std::move(store_callback));
    // The handle for this operation is released upon returning, allowing the
    // next queued operation to run.
    return;
  }

  // If the entry is not active and a single entry could not be found in the
  // in-memory index, we can directly ask the store to delete the "live" (not
  // yet doomed) entry from the database.
  store_->DeleteLiveEntry(
      key, std::move(store_callback)
               .Then(OnceClosureWithBoundArgs(std::move(handle))));
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
            .Then(OnceClosureWithBoundArgs(std::move(handle))));
    return;
  }

  // Collect Ids of active entries to exclude them from the store's
  // DeleteLiveEntriesBetween operation, as they will be handled by dooming them
  // directly within this method.
  std::vector<SqlPersistentStore::ResIdAndShardId> excluded_list;
  excluded_list.reserve(active_entries_.size());
  std::vector<SqlEntryImpl*> active_entries_to_be_doomed;
  for (auto& it : active_entries_) {
    const auto optional_res_id = GetResId(it.second->res_id_or_error());
    if (optional_res_id.has_value()) {
      excluded_list.emplace_back(*optional_res_id,
                                 store_->GetShardIdForHash(it.first.hash()));
    }
    // Check if the active entry falls within the specified time range.
    const base::Time last_used_time = it.second->GetLastUsed();
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
          .Then(OnceClosureWithBoundArgs(std::move(handle))));

  // Doom active entries that fall within the time range.
  for (auto* entry : active_entries_to_be_doomed) {
    DoomActiveEntryInternal(*entry, barrier_callback);
  }


  // Ask the store to delete all other "live" (not doomed, not active, not
  // pending) entries within the specified time range, excluding those already
  // handled.
  store_->DeleteLiveEntriesBetween(
      initial_time, end_time, std::move(excluded_list),
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
  return CalculateSizeOfEntriesBetween(base::Time::Min(), base::Time::Max(),
                                       std::move(callback));
}

int64_t SqlBackendImpl::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    Int64CompletionOnceCallback callback) {
  exclusive_operation_coordinator_.PostOrRunExclusiveOperation(base::BindOnce(
      &SqlBackendImpl::HandleCalculateSizeOfEntriesBetweenOperation,
      weak_factory_.GetWeakPtr(), initial_time, end_time, std::move(callback)));
  return net::ERR_IO_PENDING;
}

void SqlBackendImpl::HandleCalculateSizeOfEntriesBetweenOperation(
    base::Time initial_time,
    base::Time end_time,
    Int64CompletionOnceCallback callback,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  store_->CalculateSizeOfEntriesBetween(
      initial_time, end_time,
      base::BindOnce(
          [](base::WeakPtr<SqlBackendImpl> weak_ptr,
             Int64CompletionOnceCallback callback,
             std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle>
                 handle,
             SqlPersistentStore::Int64OrError result) {
            if (weak_ptr) {
              std::move(callback).Run(result.has_value() ? result.value()
                                                         : net::ERR_FAILED);
            }
          },
          weak_factory_.GetWeakPtr(), std::move(callback), std::move(handle)));
}

std::unique_ptr<Backend::Iterator> SqlBackendImpl::CreateIterator() {
  return std::make_unique<IteratorImpl>(weak_factory_.GetWeakPtr());
}

void SqlBackendImpl::GetStats(base::StringPairs* stats) {
  stats->emplace_back(std::make_pair("Cache type", "SQL Cache"));
  // TODO(crbug.com/422065015): Write more stats.
}

void SqlBackendImpl::OnExternalCacheHit(const std::string& key) {
  const CacheEntryKey entry_key(key);
  if (auto it = active_entries_.find(entry_key); it != active_entries_.end()) {
    it->second->UpdateLastUsed();
    return;
  }
  const base::Time now = base::Time::Now();
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      entry_key,
      base::BindOnce(&SqlBackendImpl::HandleOnExternalCacheHitOperation,
                     weak_factory_.GetWeakPtr(), entry_key, now,
                     PushInFlightEntryModification(
                         entry_key, InFlightEntryModification(nullptr, now))));
}

void SqlBackendImpl::HandleOnExternalCacheHitOperation(
    const CacheEntryKey& key,
    base::Time now,
    PopInFlightEntryModificationRunner pop_in_flight_entry_modification,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  store_->UpdateEntryLastUsedByKey(
      key, now,
      base::BindOnce([](SqlPersistentStore::Error error) {})
          .Then(OnceClosureWithBoundArgs(
              std::move(pop_in_flight_entry_modification)))
          .Then(OnceClosureWithBoundArgs(std::move(handle))));
}

uint8_t SqlBackendImpl::GetEntryInMemoryData(const std::string& key) {
  return store_->GetInMemoryEntryDataHints(CacheEntryKey::HashFromString(key))
      .value_or(MemoryEntryDataHints(0))
      .value();
}

void SqlBackendImpl::OnBrowserIdle() {
  store_->MaybeLoadInMemoryIndex(base::DoNothing());
  store_->MaybeRunCleanupDoomedEntries(base::DoNothing());
  store_->MaybeRunCheckpoint(base::DoNothing());
  MaybeTriggerEviction(/*is_idle_time_eviction=*/true);
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
      weak_factory_.GetWeakPtr(), key,
      base::MakeRefCounted<ResIdOrErrorHolder>(entry_info.res_id),
      entry_info.last_used, entry_info.body_end, entry_info.head);

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

  MaybeTriggerEviction(/*is_idle_time_eviction=*/false);
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

EntryResult SqlBackendImpl::SpeculativeCreateEntry(
    const CacheEntryKey& entry_key,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  auto optional_res_id_or_error =
      base::MakeRefCounted<ResIdOrErrorHolder>(std::nullopt);
  const auto creation_time = base::Time::Now();
  store_->CreateEntry(
      entry_key, creation_time,
      base::BindOnce(&SqlBackendImpl::OnSpeculativeCreateEntryFinished,
                     base::Unretained(this), optional_res_id_or_error,
                     std::move(handle)));

  // Create a new SqlEntryImpl instance.
  scoped_refptr<SqlEntryImpl> new_entry = base::MakeRefCounted<SqlEntryImpl>(
      weak_factory_.GetWeakPtr(), entry_key,
      std::move(optional_res_id_or_error), creation_time, /*body_end=*/0,
      /*head=*/nullptr);

  // Add a reference for passing to the `callback`.
  new_entry->AddRef();
  // Add the new entry to the active_entries_ map.
  auto insert_result = active_entries_.insert(
      std::make_pair(entry_key, raw_ref<SqlEntryImpl>(*new_entry.get())));
  CHECK(insert_result.second);

  return EntryResult::MakeCreated(new_entry.get());
}

void SqlBackendImpl::OnSpeculativeCreateEntryFinished(
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle,
    SqlPersistentStore::EntryInfoOrError result) {
  if (result.has_value()) {
    res_id_or_error->data = result->res_id;
  } else {
    res_id_or_error->data = result.error();
  }
  MaybeTriggerEviction(/*is_idle_time_eviction=*/false);
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
                     entry.res_id_or_error()));
}

void SqlBackendImpl::HandleDeleteDoomedEntry(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  const auto optional_res_id = GetResId(res_id_or_error);
  if (!optional_res_id) {
    // Fail the operation for entries that previously failed a speculative
    // creation or optimistic write.
    return;
  }
  store_->DeleteDoomedEntry(
      key, *optional_res_id,
      base::BindOnce(
          [](std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle>
                 handle,
             SqlPersistentStore::Error error) {},
          std::move(handle)));
}

void SqlBackendImpl::UpdateEntryLastUsed(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    base::Time last_used) {
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      key,
      base::BindOnce(
          &SqlBackendImpl::HandleUpdateEntryLastUsedOperation,
          weak_factory_.GetWeakPtr(), key, res_id_or_error, last_used,
          PushInFlightEntryModification(
              key, InFlightEntryModification(res_id_or_error, last_used))));
}

void SqlBackendImpl::HandleUpdateEntryLastUsedOperation(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    base::Time last_used,
    PopInFlightEntryModificationRunner pop_in_flight_entry_modification,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  const auto optional_res_id = GetResId(res_id_or_error);
  if (!optional_res_id) {
    // Fail the operation for entries that previously failed a speculative
    // creation or optimistic write.
    return;
  }
  store_->UpdateEntryLastUsedByResId(
      key, *optional_res_id, last_used,
      base::BindOnce([](SqlPersistentStore::Error error) {})
          .Then(OnceClosureWithBoundArgs(
              std::move(pop_in_flight_entry_modification)))
          .Then(OnceClosureWithBoundArgs(std::move(handle))));
}

void SqlBackendImpl::UpdateEntryHeaderAndLastUsed(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    base::Time last_used,
    const std::optional<MemoryEntryDataHints>& new_hints,
    scoped_refptr<net::GrowableIOBuffer> buffer,
    int64_t header_size_delta) {
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      key, base::BindOnce(
               &SqlBackendImpl::HandleUpdateEntryHeaderAndLastUsedOperation,
               weak_factory_.GetWeakPtr(), key, res_id_or_error, last_used,
               new_hints, std::move(buffer), header_size_delta,
               PushInFlightEntryModification(
                   key, InFlightEntryModification(res_id_or_error, last_used,
                                                  buffer))));
}

void SqlBackendImpl::HandleUpdateEntryHeaderAndLastUsedOperation(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    base::Time last_used,
    const std::optional<MemoryEntryDataHints>& new_hints,
    scoped_refptr<net::GrowableIOBuffer> buffer,
    int64_t header_size_delta,
    PopInFlightEntryModificationRunner pop_in_flight_entry_modification,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  const auto optional_res_id = GetResId(res_id_or_error);
  if (!optional_res_id) {
    // Fail the operation for entries that previously failed a speculative
    // creation or optimistic write.
    const auto optional_error = GetError(res_id_or_error);
    CHECK(optional_error.has_value());
    return;
  }
  store_->UpdateEntryHeaderAndLastUsed(
      key, *optional_res_id, last_used, new_hints, std::move(buffer),
      header_size_delta,
      base::BindOnce([](SqlPersistentStore::Error error) {})
          .Then(OnceClosureWithBoundArgs(
              std::move(pop_in_flight_entry_modification)))
          .Then(OnceClosureWithBoundArgs(std::move(handle))));
}

int SqlBackendImpl::WriteEntryData(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    int64_t old_body_end,
    int64_t body_end,
    int64_t offset,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    bool truncate,
    CompletionOnceCallback callback) {
  if (res_id_or_error->data.has_value() &&
      std::holds_alternative<SqlPersistentStore::Error>(
          res_id_or_error->data.value())) {
    // Fail the operation for entries that previously failed a speculative
    // creation or optimistic write.
    return net::ERR_FAILED;
  }

  // Perform optimistic writes as long as `optimistic_write_buffer_total_size_`
  // does not exceed `kSqlDiskCacheOptimisticWriteBufferSize`.
  const bool can_execute_optimistic_write =
      optimistic_write_buffer_total_size_ + buf_len <=
      net::features::kSqlDiskCacheOptimisticWriteBufferSize.Get();
  base::UmaHistogramBoolean("Net.SqlDiskCache.Write.IsOptimistic",
                            can_execute_optimistic_write);
  if (can_execute_optimistic_write) {
    optimistic_write_buffer_total_size_ += buf_len;
    if (buffer) {
      // Note: `buffer` can be nullptr.
      buffer = base::MakeRefCounted<net::VectorIOBuffer>(
          buffer->first(static_cast<size_t>(buf_len)));
    }
    // Callback to set an error on `res_id_or_error` when an error occurs or
    // the backend is deleted.
    auto maybe_update_res_id_or_error_callback =
        WrapCallbackWithAbortError<SqlPersistentStore::Error>(
            base::BindOnce(
                [](const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
                   SqlPersistentStore::Error result) {
                  base::UmaHistogramEnumeration(
                      "Net.SqlDiskCache.OptimisticWrite.Result", result);
                  if (result != SqlPersistentStore::Error::kOk) {
                    res_id_or_error->data = result;
                  }
                },
                res_id_or_error),
            SqlPersistentStore::Error::kAborted);
    exclusive_operation_coordinator_.PostOrRunNormalOperation(
        key,
        base::BindOnce(
            &SqlBackendImpl::HandleOptimisticWriteEntryDataOperation,
            weak_factory_.GetWeakPtr(), key, res_id_or_error, old_body_end,
            offset, std::move(buffer), buf_len, truncate,
            std::move(maybe_update_res_id_or_error_callback),
            PushInFlightEntryModification(
                key, InFlightEntryModification(res_id_or_error, body_end))));
    return buf_len;
  }
  auto sync_result_receiver =
      base::MakeRefCounted<SyncResultReceiver<int>>(std::move(callback));
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      key,
      base::BindOnce(
          &SqlBackendImpl::HandleWriteEntryDataOperation,
          weak_factory_.GetWeakPtr(), key, res_id_or_error, old_body_end,
          offset, std::move(buffer), buf_len, truncate,
          base::BindOnce(
              [](CompletionOnceCallback callback, int buf_len,
                 SqlPersistentStore::Error result) {
                std::move(callback).Run(result == SqlPersistentStore::Error::kOk
                                            ? buf_len
                                            : net::ERR_FAILED);
              },
              WrapCallbackWithAbortError<int>(
                  sync_result_receiver->GetCallback(), net::ERR_ABORTED),
              buf_len),
          PushInFlightEntryModification(
              key, InFlightEntryModification(res_id_or_error, body_end))));
  auto sync_result = sync_result_receiver->FinishSyncCall();
  return sync_result ? std::move(*sync_result) : net::ERR_IO_PENDING;
}

void SqlBackendImpl::HandleWriteEntryDataOperation(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    int64_t old_body_end,
    int64_t offset,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    bool truncate,
    SqlPersistentStore::ErrorCallback callback,
    PopInFlightEntryModificationRunner pop_in_flight_entry_modification,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  const auto optional_res_id = GetResId(res_id_or_error);
  if (!optional_res_id) {
    // Fail the operation for entries that previously failed a speculative
    // creation or optimistic write.
    const auto optional_error = GetError(res_id_or_error);
    CHECK(optional_error.has_value());
    std::move(callback).Run(*optional_error);
    return;
  }
  store_->WriteEntryData(
      key, *optional_res_id, old_body_end, offset, std::move(buffer), buf_len,
      truncate,
      std::move(callback)
          .Then(OnceClosureWithBoundArgs(
              std::move(pop_in_flight_entry_modification)))
          .Then(OnceClosureWithBoundArgs(std::move(handle))));
}

void SqlBackendImpl::HandleOptimisticWriteEntryDataOperation(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    int64_t old_body_end,
    int64_t offset,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    bool truncate,
    SqlPersistentStore::ErrorCallback maybe_update_res_id_or_error_callback,
    PopInFlightEntryModificationRunner pop_in_flight_entry_modification,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  const auto optional_res_id = GetResId(res_id_or_error);
  if (!optional_res_id) {
    // Decrement the total size.
    optimistic_write_buffer_total_size_ -= buf_len;
    CHECK_GE(optimistic_write_buffer_total_size_, 0);
    // Fail the operation for entries that previously failed a speculative
    // creation or optimistic write.
    const auto optional_error = GetError(res_id_or_error);
    CHECK(optional_error.has_value());
    // Need to call `maybe_update_res_id_or_error_callback` here, otherwise
    // `res_id_or_error` will be set to SqlPersistentStore::Error::kAborted.
    std::move(maybe_update_res_id_or_error_callback).Run(*optional_error);
    return;
  }
  store_->WriteEntryData(
      key, *optional_res_id, old_body_end, offset, std::move(buffer), buf_len,
      truncate,
      base::BindOnce(&SqlBackendImpl::OnOptimisticWriteFinished,
                     weak_factory_.GetWeakPtr(), key, *optional_res_id, buf_len,
                     std::move(maybe_update_res_id_or_error_callback),
                     std::move(pop_in_flight_entry_modification),
                     std::move(handle)));
}

void SqlBackendImpl::OnOptimisticWriteFinished(
    const CacheEntryKey& key,
    SqlPersistentStore::ResId res_id,
    int buf_len,
    SqlPersistentStore::ErrorCallback maybe_update_res_id_or_error_callback,
    PopInFlightEntryModificationRunner pop_in_flight_entry_modification,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle,
    SqlPersistentStore::Error result) {
  optimistic_write_buffer_total_size_ -= buf_len;
  CHECK_GE(optimistic_write_buffer_total_size_, 0);
  std::move(maybe_update_res_id_or_error_callback).Run(result);

  if (result == SqlPersistentStore::Error::kOk) {
    return;
  }
  // If an optimistic write fails, `maybe_update_res_id_or_error_callback` has
  // set an error value in the entry's `res_id_or_error`. This ensures that all
  // subsequent operations on this entry will also fail.
  // Since the user of the Sql backend can no longer delete the entry from
  // storage, SqlBackendImpl takes responsibility for deleting it.
  store_->DoomEntry(key, res_id, base::DoNothing());
  store_->DeleteDoomedEntry(key, res_id,
                            base::DoNothingWithBoundArgs(std::move(handle)));
}

int SqlBackendImpl::ReadEntryData(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    int64_t offset,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    int64_t body_end,
    bool sparse_reading,
    CompletionOnceCallback callback) {
  auto sync_result_receiver =
      base::MakeRefCounted<SyncResultReceiver<int>>(std::move(callback));
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      key,
      base::BindOnce(
          &SqlBackendImpl::HandleReadEntryDataOperation,
          weak_factory_.GetWeakPtr(), key, res_id_or_error, offset,
          std::move(buffer), buf_len, body_end, sparse_reading,
          base::BindOnce(
              [](CompletionOnceCallback callback,
                 SqlPersistentStore::IntOrError result) {
                std::move(callback).Run(result.value_or(net::ERR_FAILED));
              },
              WrapCallbackWithAbortError<int>(
                  sync_result_receiver->GetCallback(), net::ERR_ABORTED))));

  auto sync_result = sync_result_receiver->FinishSyncCall();
  return sync_result ? std::move(*sync_result) : net::ERR_IO_PENDING;
}

void SqlBackendImpl::HandleReadEntryDataOperation(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    int64_t offset,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    int64_t body_end,
    bool sparse_reading,
    SqlPersistentStore::IntOrErrorCallback callback,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  const auto optional_res_id = GetResId(res_id_or_error);
  if (!optional_res_id) {
    // Fail the operation for entries that previously failed a speculative
    // creation or optimistic write.
    const auto optional_error = GetError(res_id_or_error);
    CHECK(optional_error.has_value());
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  store_->ReadEntryData(
      key, *optional_res_id, offset, buffer, buf_len, body_end, sparse_reading,
      std::move(callback).Then(OnceClosureWithBoundArgs(std::move(handle))));
}

RangeResult SqlBackendImpl::GetEntryAvailableRange(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    int64_t offset,
    int len,
    RangeResultCallback callback) {
  auto sync_result_receiver =
      base::MakeRefCounted<SyncResultReceiver<const RangeResult&>>(
          std::move(callback));
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      key, base::BindOnce(
               &SqlBackendImpl::HandleGetEntryAvailableRangeOperation,
               weak_factory_.GetWeakPtr(), key, res_id_or_error, offset, len,
               WrapCallbackWithAbortError<const RangeResult&>(
                   sync_result_receiver->GetCallback(),
                   RangeResult(net::ERR_ABORTED))));
  auto sync_result = sync_result_receiver->FinishSyncCall();
  return sync_result ? std::move(*sync_result)
                     : RangeResult(net::ERR_IO_PENDING);
}

void SqlBackendImpl::HandleGetEntryAvailableRangeOperation(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    int64_t offset,
    int len,
    RangeResultCallback callback,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  const auto optional_res_id = GetResId(res_id_or_error);
  if (!optional_res_id) {
    // Fail the operation for entries that previously failed a speculative
    // creation or optimistic write.
    const auto optional_error = GetError(res_id_or_error);
    CHECK(optional_error.has_value());
    std::move(callback).Run(RangeResult(net::ERR_FAILED));
    return;
  }
  store_->GetEntryAvailableRange(key, *optional_res_id, offset, len,
                                 std::move(callback));
}

void SqlBackendImpl::SetEntryDataHints(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    MemoryEntryDataHints hints) {
  exclusive_operation_coordinator_.PostOrRunNormalOperation(
      key,
      base::BindOnce(&SqlBackendImpl::HandleSetEntryDataHintsOperation,
                     weak_factory_.GetWeakPtr(), key, res_id_or_error, hints));
}

void SqlBackendImpl::HandleSetEntryDataHintsOperation(
    const CacheEntryKey& key,
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    MemoryEntryDataHints hints,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  const auto optional_res_id = GetResId(res_id_or_error);
  if (!optional_res_id) {
    // Fail the operation for entries that previously failed a speculative
    // creation or optimistic write.
    return;
  }
  store_->SetInMemoryEntryDataHints(key.hash(), *optional_res_id, hints);
}

SqlBackendImpl::PopInFlightEntryModificationRunner
SqlBackendImpl::PushInFlightEntryModification(
    const CacheEntryKey& entry_key,
    InFlightEntryModification in_flight_entry_modification) {
  in_flight_entry_modifications_[entry_key].emplace_back(
      std::move(in_flight_entry_modification));
  return PopInFlightEntryModificationRunner(base::ScopedClosureRunner(
      base::BindOnce(&SqlBackendImpl::PopInFlightEntryModification,
                     weak_factory_.GetWeakPtr(), entry_key)));
}

void SqlBackendImpl::PopInFlightEntryModification(
    const CacheEntryKey& entry_key) {
  // The in-flight modifications for a given key are queued and removed in FIFO
  // order. This is safe because `exclusive_operation_coordinator_` serializes
  // all normal operations for the same key. This guarantees that modifications
  // are enqueued and the corresponding store operations are executed in the
  // same order.
  auto it = in_flight_entry_modifications_.find(entry_key);
  CHECK(it != in_flight_entry_modifications_.end());
  CHECK(!it->second.empty());
  it->second.pop_front();
  if (it->second.empty()) {
    in_flight_entry_modifications_.erase(it);
  }
}

void SqlBackendImpl::ApplyInFlightEntryModifications(
    const CacheEntryKey& key,
    SqlPersistentStore::EntryInfo& entry_info) {
  auto it = in_flight_entry_modifications_.find(key);
  if (it == in_flight_entry_modifications_.end()) {
    return;
  }
  for (const auto& modification : it->second) {
    std::optional<SqlPersistentStore::ResId> optional_res_id =
        modification.res_id_or_error ? GetResId(modification.res_id_or_error)
                                     : std::nullopt;
    if (!modification.res_id_or_error ||
        (optional_res_id.has_value() &&
         *optional_res_id == entry_info.res_id)) {
      if (modification.last_used.has_value()) {
        entry_info.last_used = *modification.last_used;
      }
      if (modification.head.has_value()) {
        entry_info.head = *modification.head;
      }
      if (modification.body_end.has_value()) {
        entry_info.body_end = *modification.body_end;
      }
    }
  }
}

int SqlBackendImpl::FlushQueueForTest(CompletionOnceCallback callback) {
  exclusive_operation_coordinator_.PostOrRunExclusiveOperation(base::BindOnce(
      [](std::vector<scoped_refptr<base::SequencedTaskRunner>>
             background_task_runners,
         CompletionOnceCallback callback,
         std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle>
             handle) {
        auto barrier_closure = base::BarrierClosure(
            background_task_runners.size(),
            base::BindOnce(std::move(callback), net::OK)
                .Then(OnceClosureWithBoundArgs(std::move(handle))));
        for (auto& runner : background_task_runners) {
          runner->PostTaskAndReply(
              // Post a no-op task to the background runner.
              FROM_HERE, base::BindOnce([]() {}), barrier_closure);
        }
      },
      background_task_runners_, std::move(callback)));

  return net::ERR_IO_PENDING;
}

void SqlBackendImpl::MaybeTriggerEviction(bool is_idle_time_eviction) {
  if (eviction_operation_queued_ ||
      !ShouldRunEviction(store_->GetEvictionUrgency(), is_idle_time_eviction)) {
    return;
  }
  eviction_operation_queued_ = true;
  exclusive_operation_coordinator_.PostOrRunExclusiveOperation(base::BindOnce(
      base::BindOnce(&SqlBackendImpl::HandleTriggerEvictionOperation,
                     weak_factory_.GetWeakPtr(), is_idle_time_eviction)));
}

void SqlBackendImpl::HandleTriggerEvictionOperation(
    bool is_idle_time_eviction,
    std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle> handle) {
  eviction_operation_queued_ = false;
  if (!ShouldRunEviction(store_->GetEvictionUrgency(), is_idle_time_eviction)) {
    return;
  }
  std::vector<SqlPersistentStore::ResIdAndShardId> excluded_list;
  excluded_list.reserve(active_entries_.size());
  for (const auto& it : active_entries_) {
    const auto optional_res_id = GetResId(it.second->res_id_or_error());
    if (optional_res_id.has_value()) {
      excluded_list.emplace_back(*optional_res_id,
                                 store_->GetShardIdForHash(it.first.hash()));
    }
  }
  store_->StartEviction(std::move(excluded_list), is_idle_time_eviction,
                        base::BindOnce([](SqlPersistentStore::Error result) {
                        }).Then(OnceClosureWithBoundArgs(std::move(handle))));
}

void SqlBackendImpl::EnableStrictCorruptionCheckForTesting() {
  store_->EnableStrictCorruptionCheckForTesting();  // IN-TEST
}

SqlBackendImpl::InFlightEntryModification::InFlightEntryModification(
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    base::Time last_used)
    : res_id_or_error(res_id_or_error), last_used(last_used) {}
SqlBackendImpl::InFlightEntryModification::InFlightEntryModification(
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    base::Time last_used,
    scoped_refptr<net::GrowableIOBuffer> head)
    : res_id_or_error(res_id_or_error),
      last_used(last_used),
      head(std::move(head)) {}
SqlBackendImpl::InFlightEntryModification::InFlightEntryModification(
    const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error,
    int64_t body_end)
    : res_id_or_error(res_id_or_error), body_end(body_end) {}
SqlBackendImpl::InFlightEntryModification::~InFlightEntryModification() =
    default;
SqlBackendImpl::InFlightEntryModification::InFlightEntryModification(
    InFlightEntryModification&&) = default;

}  // namespace disk_cache
