// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_backend_impl.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <limits>

#if defined(OS_POSIX)
#include <sys/resource.h>
#endif

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/prioritized_task_runner.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_entry_impl.h"
#include "net/disk_cache/simple/simple_file_tracker.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_util.h"
#include "net/disk_cache/simple/simple_version_upgrade.h"

using base::Callback;
using base::Closure;
using base::FilePath;
using base::Time;
using base::DirectoryExists;
using base::CreateDirectory;

namespace disk_cache {

namespace {

// Maximum fraction of the cache that one entry can consume.
const int kMaxFileRatio = 8;

// Overrides the above.
const int kMinFileSizeLimit = 5 * 1024 * 1024;

bool g_fd_limit_histogram_has_been_populated = false;

void MaybeHistogramFdLimit() {
  if (g_fd_limit_histogram_has_been_populated)
    return;

  // Used in histograms; add new entries at end.
  enum FdLimitStatus {
    FD_LIMIT_STATUS_UNSUPPORTED = 0,
    FD_LIMIT_STATUS_FAILED      = 1,
    FD_LIMIT_STATUS_SUCCEEDED   = 2,
    FD_LIMIT_STATUS_MAX         = 3
  };
  FdLimitStatus fd_limit_status = FD_LIMIT_STATUS_UNSUPPORTED;
  int soft_fd_limit = 0;
  int hard_fd_limit = 0;

#if defined(OS_POSIX)
  struct rlimit nofile;
  if (!getrlimit(RLIMIT_NOFILE, &nofile)) {
    soft_fd_limit = nofile.rlim_cur;
    hard_fd_limit = nofile.rlim_max;
    fd_limit_status = FD_LIMIT_STATUS_SUCCEEDED;
  } else {
    fd_limit_status = FD_LIMIT_STATUS_FAILED;
  }
#endif

  UMA_HISTOGRAM_ENUMERATION("SimpleCache.FileDescriptorLimitStatus",
                            fd_limit_status, FD_LIMIT_STATUS_MAX);
  if (fd_limit_status == FD_LIMIT_STATUS_SUCCEEDED) {
    base::UmaHistogramSparse("SimpleCache.FileDescriptorLimitSoft",
                             soft_fd_limit);
    base::UmaHistogramSparse("SimpleCache.FileDescriptorLimitHard",
                             hard_fd_limit);
  }

  g_fd_limit_histogram_has_been_populated = true;
}

// Global context of all the files we have open --- this permits some to be
// closed on demand if too many FDs are being used, to avoid running out.
base::LazyInstance<SimpleFileTracker>::Leaky g_simple_file_tracker =
    LAZY_INSTANCE_INITIALIZER;

// Detects if the files in the cache directory match the current disk cache
// backend type and version. If the directory contains no cache, occupies it
// with the fresh structure.
SimpleCacheConsistencyResult FileStructureConsistent(
    const base::FilePath& path) {
  if (!base::PathExists(path) && !base::CreateDirectory(path)) {
    LOG(ERROR) << "Failed to create directory: " << path.LossyDisplayName();
    return SimpleCacheConsistencyResult::kCreateDirectoryFailed;
  }
  return disk_cache::UpgradeSimpleCacheOnDisk(path);
}

// A context used by a BarrierCompletionCallback to track state.
struct BarrierContext {
  explicit BarrierContext(net::CompletionOnceCallback final_callback,
                          int expected)
      : final_callback_(std::move(final_callback)),
        expected(expected),
        count(0),
        had_error(false) {}

  net::CompletionOnceCallback final_callback_;
  const int expected;
  int count;
  bool had_error;
};

void BarrierCompletionCallbackImpl(
    BarrierContext* context,
    int result) {
  DCHECK_GT(context->expected, context->count);
  if (context->had_error)
    return;
  if (result != net::OK) {
    context->had_error = true;
    std::move(context->final_callback_).Run(result);
    return;
  }
  ++context->count;
  if (context->count == context->expected)
    std::move(context->final_callback_).Run(net::OK);
}

// A barrier completion callback is a repeatable callback that waits for
// |count| successful results before invoking |final_callback|. In the case of
// an error, the first error is passed to |final_callback| and all others
// are ignored.
base::RepeatingCallback<void(int)> MakeBarrierCompletionCallback(
    int count,
    net::CompletionOnceCallback final_callback) {
  BarrierContext* context =
      new BarrierContext(std::move(final_callback), count);
  return base::BindRepeating(&BarrierCompletionCallbackImpl,
                             base::Owned(context));
}

// A short bindable thunk that ensures a completion callback is always called
// after running an operation asynchronously.
void RunOperationAndCallback(
    base::OnceCallback<net::Error(net::CompletionOnceCallback)> operation,
    net::CompletionOnceCallback operation_callback) {
  base::RepeatingCallback<void(int)> copyable_callback;
  if (operation_callback)
    copyable_callback =
        base::AdaptCallbackForRepeating(std::move(operation_callback));
  const int operation_result = std::move(operation).Run(copyable_callback);
  if (operation_result != net::ERR_IO_PENDING && copyable_callback)
    copyable_callback.Run(operation_result);
}

void RecordIndexLoad(net::CacheType cache_type,
                     base::TimeTicks constructed_since,
                     int result) {
  const base::TimeDelta creation_to_index = base::TimeTicks::Now() -
                                            constructed_since;
  if (result == net::OK) {
    SIMPLE_CACHE_UMA(TIMES, "CreationToIndex", cache_type, creation_to_index);
  } else {
    SIMPLE_CACHE_UMA(TIMES,
                     "CreationToIndexFail", cache_type, creation_to_index);
  }
}

}  // namespace

const base::Feature SimpleBackendImpl::kPrioritizedSimpleCacheTasks{
    "PrioritizedSimpleCacheTasks", base::FEATURE_DISABLED_BY_DEFAULT};

// Static function which is called by base::trace_event::EstimateMemoryUsage()
// to estimate the memory of SimpleEntryImpl* type.
// This needs to be in disk_cache namespace.
size_t EstimateMemoryUsage(const SimpleEntryImpl* const& entry_impl) {
  return sizeof(SimpleEntryImpl) + entry_impl->EstimateMemoryUsage();
}

class SimpleBackendImpl::ActiveEntryProxy
    : public SimpleEntryImpl::ActiveEntryProxy {
 public:
  ~ActiveEntryProxy() override {
    if (backend_) {
      DCHECK_EQ(1U, backend_->active_entries_.count(entry_hash_));
      backend_->active_entries_.erase(entry_hash_);
    }
  }

  static std::unique_ptr<SimpleEntryImpl::ActiveEntryProxy> Create(
      int64_t entry_hash,
      SimpleBackendImpl* backend) {
    std::unique_ptr<SimpleEntryImpl::ActiveEntryProxy> proxy(
        new ActiveEntryProxy(entry_hash, backend));
    return proxy;
  }

 private:
  ActiveEntryProxy(uint64_t entry_hash, SimpleBackendImpl* backend)
      : entry_hash_(entry_hash), backend_(backend->AsWeakPtr()) {}

  uint64_t entry_hash_;
  base::WeakPtr<SimpleBackendImpl> backend_;
};

SimpleBackendImpl::SimpleBackendImpl(
    const FilePath& path,
    scoped_refptr<BackendCleanupTracker> cleanup_tracker,
    SimpleFileTracker* file_tracker,
    int64_t max_bytes,
    net::CacheType cache_type,
    net::NetLog* net_log)
    : cleanup_tracker_(std::move(cleanup_tracker)),
      file_tracker_(file_tracker ? file_tracker
                                 : g_simple_file_tracker.Pointer()),
      path_(path),
      cache_type_(cache_type),
      cache_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      orig_max_size_(max_bytes),
      entry_operations_mode_((cache_type == net::DISK_CACHE ||
                              cache_type == net::GENERATED_CODE_CACHE)
                                 ? SimpleEntryImpl::OPTIMISTIC_OPERATIONS
                                 : SimpleEntryImpl::NON_OPTIMISTIC_OPERATIONS),
      net_log_(net_log) {
  // Treat negative passed-in sizes same as SetMaxSize would here and in other
  // backends, as default (if first call).
  if (orig_max_size_ < 0)
    orig_max_size_ = 0;
  MaybeHistogramFdLimit();
}

SimpleBackendImpl::~SimpleBackendImpl() {
  index_->WriteToDisk(SimpleIndex::INDEX_WRITE_REASON_SHUTDOWN);
}

void SimpleBackendImpl::SetWorkerPoolForTesting(
    scoped_refptr<base::TaskRunner> task_runner) {
  prioritized_task_runner_ =
      base::MakeRefCounted<net::PrioritizedTaskRunner>(std::move(task_runner));
}

net::Error SimpleBackendImpl::Init(CompletionOnceCallback completion_callback) {
  auto worker_pool = base::CreateTaskRunnerWithTraits(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  prioritized_task_runner_ =
      base::MakeRefCounted<net::PrioritizedTaskRunner>(worker_pool);

  index_ = std::make_unique<SimpleIndex>(
      base::ThreadTaskRunnerHandle::Get(), cleanup_tracker_.get(), this,
      cache_type_,
      std::make_unique<SimpleIndexFile>(cache_runner_, worker_pool.get(),
                                        cache_type_, path_));
  index_->ExecuteWhenReady(
      base::BindOnce(&RecordIndexLoad, cache_type_, base::TimeTicks::Now()));

  PostTaskAndReplyWithResult(
      cache_runner_.get(), FROM_HERE,
      base::BindOnce(&SimpleBackendImpl::InitCacheStructureOnDisk, path_,
                     orig_max_size_, cache_type_),
      base::BindOnce(&SimpleBackendImpl::InitializeIndex, AsWeakPtr(),
                     std::move(completion_callback)));
  return net::ERR_IO_PENDING;
}

bool SimpleBackendImpl::SetMaxSize(int64_t max_bytes) {
  if (max_bytes < 0)
    return false;
  orig_max_size_ = max_bytes;
  index_->SetMaxSize(max_bytes);
  return true;
}

int SimpleBackendImpl::GetMaxFileSize() const {
  return std::max(base::saturated_cast<int>(index_->max_size() / kMaxFileRatio),
                  kMinFileSizeLimit);
}

void SimpleBackendImpl::OnDoomStart(uint64_t entry_hash) {
  DCHECK_EQ(0u, entries_pending_doom_.count(entry_hash));
  entries_pending_doom_.insert(
      std::make_pair(entry_hash, std::vector<PostDoomWaiter>()));
}

void SimpleBackendImpl::OnDoomComplete(uint64_t entry_hash) {
  DCHECK_EQ(1u, entries_pending_doom_.count(entry_hash));
  auto it = entries_pending_doom_.find(entry_hash);
  std::vector<PostDoomWaiter> to_handle_waiters;
  to_handle_waiters.swap(it->second);
  entries_pending_doom_.erase(it);

  SIMPLE_CACHE_UMA(COUNTS_1000, "NumOpsBlockedByPendingDoom", cache_type_,
                   to_handle_waiters.size());

  for (PostDoomWaiter& post_doom : to_handle_waiters) {
    SIMPLE_CACHE_UMA(TIMES, "QueueLatency.PendingDoom", cache_type_,
                     (base::TimeTicks::Now() - post_doom.time_queued));
    std::move(post_doom.run_post_doom).Run();
  }
}

void SimpleBackendImpl::DoomEntries(std::vector<uint64_t>* entry_hashes,
                                    net::CompletionOnceCallback callback) {
  std::unique_ptr<std::vector<uint64_t>> mass_doom_entry_hashes(
      new std::vector<uint64_t>());
  mass_doom_entry_hashes->swap(*entry_hashes);

  std::vector<uint64_t> to_doom_individually_hashes;

  // For each of the entry hashes, there are two cases:
  // 1. There are corresponding entries in active set, pending doom, or both
  //    sets, and so the hash should be doomed individually to avoid flakes.
  // 2. The hash is not in active use at all, so we can call
  //    SimpleSynchronousEntry::DeleteEntrySetFiles and delete the files en
  //    masse.
  for (int i = mass_doom_entry_hashes->size() - 1; i >= 0; --i) {
    const uint64_t entry_hash = (*mass_doom_entry_hashes)[i];
    if (!active_entries_.count(entry_hash) &&
        !entries_pending_doom_.count(entry_hash)) {
      continue;
    }

    to_doom_individually_hashes.push_back(entry_hash);

    (*mass_doom_entry_hashes)[i] = mass_doom_entry_hashes->back();
    mass_doom_entry_hashes->resize(mass_doom_entry_hashes->size() - 1);
  }

  base::RepeatingCallback<void(int)> barrier_callback =
      MakeBarrierCompletionCallback(to_doom_individually_hashes.size() + 1,
                                    std::move(callback));
  for (std::vector<uint64_t>::const_iterator
           it = to_doom_individually_hashes.begin(),
           end = to_doom_individually_hashes.end();
       it != end; ++it) {
    const int doom_result = DoomEntryFromHash(*it, barrier_callback);
    DCHECK_EQ(net::ERR_IO_PENDING, doom_result);
    index_->Remove(*it);
  }

  for (std::vector<uint64_t>::const_iterator
           it = mass_doom_entry_hashes->begin(),
           end = mass_doom_entry_hashes->end();
       it != end; ++it) {
    index_->Remove(*it);
    OnDoomStart(*it);
  }

  // Taking this pointer here avoids undefined behaviour from calling
  // base::Passed before mass_doom_entry_hashes.get().
  std::vector<uint64_t>* mass_doom_entry_hashes_ptr =
      mass_doom_entry_hashes.get();

  PostTaskAndReplyWithResult(
      prioritized_task_runner_->task_runner(), FROM_HERE,
      base::BindOnce(&SimpleSynchronousEntry::DeleteEntrySetFiles,
                     mass_doom_entry_hashes_ptr, path_),
      base::BindOnce(&SimpleBackendImpl::DoomEntriesComplete, AsWeakPtr(),
                     base::Passed(&mass_doom_entry_hashes), barrier_callback));
}

net::CacheType SimpleBackendImpl::GetCacheType() const {
  return net::DISK_CACHE;
}

int32_t SimpleBackendImpl::GetEntryCount() const {
  // TODO(pasko): Use directory file count when index is not ready.
  return index_->GetEntryCount();
}

net::Error SimpleBackendImpl::OpenEntry(const std::string& key,
                                        net::RequestPriority request_priority,
                                        Entry** entry,
                                        CompletionOnceCallback callback) {
  const uint64_t entry_hash = simple_util::GetEntryHashKey(key);

  std::vector<PostDoomWaiter>* post_doom = nullptr;
  scoped_refptr<SimpleEntryImpl> simple_entry = CreateOrFindActiveOrDoomedEntry(
      entry_hash, key, request_priority, &post_doom);
  if (!simple_entry) {
    if (post_doom->empty() &&
        entry_operations_mode_ == SimpleEntryImpl::OPTIMISTIC_OPERATIONS) {
      // The entry is doomed, and no other backend operations are queued for the
      // entry, thus the open must fail and it's safe to return synchronously.
      net::NetLogWithSource log_for_entry(net::NetLogWithSource::Make(
          net_log_, net::NetLogSourceType::DISK_CACHE_ENTRY));
      log_for_entry.AddEvent(
          net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_CALL);
      log_for_entry.AddEventWithNetErrorCode(
          net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_END, net::ERR_FAILED);
      return net::ERR_FAILED;
    }

    base::OnceCallback<net::Error(CompletionOnceCallback)> operation =
        base::BindOnce(&SimpleBackendImpl::OpenEntry, base::Unretained(this),
                       key, request_priority, entry);
    post_doom->emplace_back(base::BindOnce(
        &RunOperationAndCallback, std::move(operation), std::move(callback)));
    return net::ERR_IO_PENDING;
  }
  return simple_entry->OpenEntry(entry, std::move(callback));
}

net::Error SimpleBackendImpl::CreateEntry(const std::string& key,
                                          net::RequestPriority request_priority,
                                          Entry** entry,
                                          CompletionOnceCallback callback) {
  DCHECK_LT(0u, key.size());
  const uint64_t entry_hash = simple_util::GetEntryHashKey(key);

  std::vector<PostDoomWaiter>* post_doom = nullptr;
  scoped_refptr<SimpleEntryImpl> simple_entry = CreateOrFindActiveOrDoomedEntry(
      entry_hash, key, request_priority, &post_doom);

  if (!simple_entry) {
    // We would like to optimistically have create go ahead, for benefit of
    // HTTP cache use. This can only be sanely done if we are the only op
    // serialized after doom's completion.
    if (post_doom->empty() &&
        entry_operations_mode_ == SimpleEntryImpl::OPTIMISTIC_OPERATIONS) {
      simple_entry = new SimpleEntryImpl(
          cache_type_, path_, cleanup_tracker_.get(), entry_hash,
          entry_operations_mode_, this, file_tracker_, net_log_,
          GetNewEntryPriority(request_priority));
      simple_entry->SetKey(key);
      simple_entry->SetActiveEntryProxy(
          ActiveEntryProxy::Create(entry_hash, this));
      simple_entry->SetCreatePendingDoom();
      std::pair<EntryMap::iterator, bool> insert_result =
          active_entries_.insert(
              EntryMap::value_type(entry_hash, simple_entry.get()));
      post_doom->emplace_back(base::BindOnce(
          &SimpleEntryImpl::NotifyDoomBeforeCreateComplete, simple_entry));
      DCHECK(insert_result.second);
    } else {
      base::OnceCallback<net::Error(CompletionOnceCallback)> operation =
          base::BindOnce(&SimpleBackendImpl::CreateEntry,
                         base::Unretained(this), key, request_priority, entry);
      post_doom->emplace_back(base::BindOnce(
          &RunOperationAndCallback, std::move(operation), std::move(callback)));
      return net::ERR_IO_PENDING;
    }
  }

  return simple_entry->CreateEntry(entry, std::move(callback));
}

net::Error SimpleBackendImpl::DoomEntry(const std::string& key,
                                        net::RequestPriority priority,
                                        CompletionOnceCallback callback) {
  const uint64_t entry_hash = simple_util::GetEntryHashKey(key);

  std::vector<PostDoomWaiter>* post_doom = nullptr;
  scoped_refptr<SimpleEntryImpl> simple_entry =
      CreateOrFindActiveOrDoomedEntry(entry_hash, key, priority, &post_doom);
  if (!simple_entry) {
    // At first glance, it appears exceedingly silly to queue up a doom
    // when we get here because the files corresponding to our key are being
    // deleted... but it's possible that one of the things in post_doom is a
    // create for our key, in which case we still have work to do.
    base::OnceCallback<net::Error(CompletionOnceCallback)> operation =
        base::BindOnce(&SimpleBackendImpl::DoomEntry, base::Unretained(this),
                       key, priority);
    post_doom->emplace_back(base::BindOnce(
        &RunOperationAndCallback, std::move(operation), std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  return simple_entry->DoomEntry(std::move(callback));
}

net::Error SimpleBackendImpl::DoomAllEntries(CompletionOnceCallback callback) {
  return DoomEntriesBetween(Time(), Time(), std::move(callback));
}

net::Error SimpleBackendImpl::DoomEntriesBetween(
    const Time initial_time,
    const Time end_time,
    CompletionOnceCallback callback) {
  return index_->ExecuteWhenReady(
      base::BindOnce(&SimpleBackendImpl::IndexReadyForDoom, AsWeakPtr(),
                     initial_time, end_time, std::move(callback)));
}

net::Error SimpleBackendImpl::DoomEntriesSince(
    const Time initial_time,
    CompletionOnceCallback callback) {
  return DoomEntriesBetween(initial_time, Time(), std::move(callback));
}

int64_t SimpleBackendImpl::CalculateSizeOfAllEntries(
    Int64CompletionOnceCallback callback) {
  return index_->ExecuteWhenReady(
      base::BindOnce(&SimpleBackendImpl::IndexReadyForSizeCalculation,
                     AsWeakPtr(), std::move(callback)));
}

int64_t SimpleBackendImpl::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    Int64CompletionOnceCallback callback) {
  return index_->ExecuteWhenReady(
      base::BindOnce(&SimpleBackendImpl::IndexReadyForSizeBetweenCalculation,
                     AsWeakPtr(), initial_time, end_time, std::move(callback)));
}

class SimpleBackendImpl::SimpleIterator final : public Iterator {
 public:
  explicit SimpleIterator(base::WeakPtr<SimpleBackendImpl> backend)
      : backend_(backend),
        weak_factory_(this) {
  }

  // From Backend::Iterator:
  net::Error OpenNextEntry(Entry** next_entry,
                           CompletionOnceCallback callback) override {
    CompletionOnceCallback open_next_entry_impl = base::BindOnce(
        &SimpleIterator::OpenNextEntryImpl, weak_factory_.GetWeakPtr(),
        next_entry, std::move(callback));
    return backend_->index_->ExecuteWhenReady(std::move(open_next_entry_impl));
  }

  void OpenNextEntryImpl(Entry** next_entry,
                         CompletionOnceCallback callback,
                         int index_initialization_error_code) {
    if (!backend_) {
      std::move(callback).Run(net::ERR_FAILED);
      return;
    }
    if (index_initialization_error_code != net::OK) {
      std::move(callback).Run(index_initialization_error_code);
      return;
    }
    if (!hashes_to_enumerate_)
      hashes_to_enumerate_ = backend_->index()->GetAllHashes();

    auto copyable_callback =
        base::AdaptCallbackForRepeating(std::move(callback));

    while (!hashes_to_enumerate_->empty()) {
      uint64_t entry_hash = hashes_to_enumerate_->back();
      hashes_to_enumerate_->pop_back();
      if (backend_->index()->Has(entry_hash)) {
        *next_entry = NULL;
        CompletionOnceCallback continue_iteration = base::BindOnce(
            &SimpleIterator::CheckIterationReturnValue,
            weak_factory_.GetWeakPtr(), next_entry, copyable_callback);
        int error_code_open = backend_->OpenEntryFromHash(
            entry_hash, next_entry, std::move(continue_iteration));
        if (error_code_open == net::ERR_IO_PENDING)
          return;
        if (error_code_open != net::ERR_FAILED) {
          copyable_callback.Run(error_code_open);
          return;
        }
      }
    }
    copyable_callback.Run(net::ERR_FAILED);
  }

  void CheckIterationReturnValue(Entry** entry,
                                 CompletionOnceCallback callback,
                                 int error_code) {
    if (error_code == net::ERR_FAILED) {
      OpenNextEntry(entry, std::move(callback));
      return;
    }
    std::move(callback).Run(error_code);
  }

 private:
  base::WeakPtr<SimpleBackendImpl> backend_;
  std::unique_ptr<std::vector<uint64_t>> hashes_to_enumerate_;
  base::WeakPtrFactory<SimpleIterator> weak_factory_;
};

std::unique_ptr<Backend::Iterator> SimpleBackendImpl::CreateIterator() {
  return std::unique_ptr<Iterator>(new SimpleIterator(AsWeakPtr()));
}

void SimpleBackendImpl::GetStats(base::StringPairs* stats) {
  std::pair<std::string, std::string> item;
  item.first = "Cache type";
  item.second = "Simple Cache";
  stats->push_back(item);
}

void SimpleBackendImpl::OnExternalCacheHit(const std::string& key) {
  index_->UseIfExists(simple_util::GetEntryHashKey(key));
}

size_t SimpleBackendImpl::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_absolute_name) const {
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(parent_absolute_name + "/simple_backend");

  size_t size = base::trace_event::EstimateMemoryUsage(index_) +
                base::trace_event::EstimateMemoryUsage(active_entries_);
  // TODO(xunjieli): crbug.com/669108. Track |entries_pending_doom_| once
  // base::Closure is suppported in memory_usage_estimator.h.
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes, size);
  return size;
}

uint8_t SimpleBackendImpl::GetEntryInMemoryData(const std::string& key) {
  const uint64_t entry_hash = simple_util::GetEntryHashKey(key);
  return index_->GetEntryInMemoryData(entry_hash);
}

void SimpleBackendImpl::SetEntryInMemoryData(const std::string& key,
                                             uint8_t data) {
  const uint64_t entry_hash = simple_util::GetEntryHashKey(key);
  index_->SetEntryInMemoryData(entry_hash, data);
}

SimpleBackendImpl::PostDoomWaiter::PostDoomWaiter() {}

SimpleBackendImpl::PostDoomWaiter::PostDoomWaiter(
    base::OnceClosure to_run_post_doom)
    : time_queued(base::TimeTicks::Now()),
      run_post_doom(std::move(to_run_post_doom)) {}

SimpleBackendImpl::PostDoomWaiter::PostDoomWaiter(PostDoomWaiter&& other)
    : time_queued(other.time_queued),
      run_post_doom(std::move(other.run_post_doom)) {}

SimpleBackendImpl::PostDoomWaiter& SimpleBackendImpl::PostDoomWaiter::operator=(
    PostDoomWaiter&& other) {
  time_queued = other.time_queued;
  run_post_doom = std::move(other.run_post_doom);
  return *this;
}

SimpleBackendImpl::PostDoomWaiter::~PostDoomWaiter() {}

void SimpleBackendImpl::InitializeIndex(CompletionOnceCallback callback,
                                        const DiskStatResult& result) {
  if (result.net_error == net::OK) {
    index_->SetMaxSize(result.max_size);
#if defined(OS_ANDROID)
    if (app_status_listener_)
      index_->set_app_status_listener(app_status_listener_);
#endif
    index_->Initialize(result.cache_dir_mtime);
  }
  std::move(callback).Run(result.net_error);
}

void SimpleBackendImpl::IndexReadyForDoom(Time initial_time,
                                          Time end_time,
                                          CompletionOnceCallback callback,
                                          int result) {
  if (result != net::OK) {
    std::move(callback).Run(result);
    return;
  }
  std::unique_ptr<std::vector<uint64_t>> removed_key_hashes(
      index_->GetEntriesBetween(initial_time, end_time).release());
  DoomEntries(removed_key_hashes.get(), std::move(callback));
}

void SimpleBackendImpl::IndexReadyForSizeCalculation(
    Int64CompletionOnceCallback callback,
    int result) {
  int64_t rv = result == net::OK ? index_->GetCacheSize() : result;
  std::move(callback).Run(rv);
}

void SimpleBackendImpl::IndexReadyForSizeBetweenCalculation(
    base::Time initial_time,
    base::Time end_time,
    Int64CompletionOnceCallback callback,
    int result) {
  int64_t rv = result == net::OK
                   ? index_->GetCacheSizeBetween(initial_time, end_time)
                   : result;
  std::move(callback).Run(rv);
}

// static
SimpleBackendImpl::DiskStatResult SimpleBackendImpl::InitCacheStructureOnDisk(
    const base::FilePath& path,
    uint64_t suggested_max_size,
    net::CacheType cache_type) {
  DiskStatResult result;
  result.max_size = suggested_max_size;
  result.net_error = net::OK;
  SimpleCacheConsistencyResult consistency = FileStructureConsistent(path);
  SIMPLE_CACHE_UMA(ENUMERATION, "ConsistencyResult", cache_type, consistency);
  if (consistency != SimpleCacheConsistencyResult::kOK) {
    LOG(ERROR) << "Simple Cache Backend: wrong file structure on disk: "
               << static_cast<int>(consistency)
               << " path: " << path.LossyDisplayName();
    result.net_error = net::ERR_FAILED;
  } else {
    bool mtime_result =
        disk_cache::simple_util::GetMTime(path, &result.cache_dir_mtime);
    DCHECK(mtime_result);
    if (!result.max_size) {
      int64_t available = base::SysInfo::AmountOfFreeDiskSpace(path);
      result.max_size = disk_cache::PreferredCacheSize(available);
    }
    DCHECK(result.max_size);
  }
  return result;
}

scoped_refptr<SimpleEntryImpl>
SimpleBackendImpl::CreateOrFindActiveOrDoomedEntry(
    const uint64_t entry_hash,
    const std::string& key,
    net::RequestPriority request_priority,
    std::vector<PostDoomWaiter>** post_doom) {
  DCHECK_EQ(entry_hash, simple_util::GetEntryHashKey(key));

  // If there is a doom pending, we would want to serialize after it.
  auto doom_it = entries_pending_doom_.find(entry_hash);
  if (doom_it != entries_pending_doom_.end()) {
    *post_doom = &doom_it->second;
    return nullptr;
  }

  std::pair<EntryMap::iterator, bool> insert_result =
      active_entries_.insert(EntryMap::value_type(entry_hash, NULL));
  EntryMap::iterator& it = insert_result.first;
  const bool did_insert = insert_result.second;
  if (did_insert) {
    SimpleEntryImpl* entry = it->second = new SimpleEntryImpl(
        cache_type_, path_, cleanup_tracker_.get(), entry_hash,
        entry_operations_mode_, this, file_tracker_, net_log_,
        GetNewEntryPriority(request_priority));
    entry->SetKey(key);
    entry->SetActiveEntryProxy(ActiveEntryProxy::Create(entry_hash, this));
  }
  // TODO(jkarlin): In case of recycling a half-closed entry, we might want to
  // update its priority.
  DCHECK(it->second);
  // It's possible, but unlikely, that we have an entry hash collision with a
  // currently active entry.
  if (key != it->second->key()) {
    it->second->Doom();
    DCHECK_EQ(0U, active_entries_.count(entry_hash));
    DCHECK_EQ(1U, entries_pending_doom_.count(entry_hash));
    // Re-run ourselves to handle the now-pending doom.
    return CreateOrFindActiveOrDoomedEntry(entry_hash, key, request_priority,
                                           post_doom);
  }
  return base::WrapRefCounted(it->second);
}

net::Error SimpleBackendImpl::OpenEntryFromHash(
    uint64_t entry_hash,
    Entry** entry,
    CompletionOnceCallback callback) {
  auto it = entries_pending_doom_.find(entry_hash);
  if (it != entries_pending_doom_.end()) {
    base::OnceCallback<net::Error(CompletionOnceCallback)> operation =
        base::BindOnce(&SimpleBackendImpl::OpenEntryFromHash,
                       base::Unretained(this), entry_hash, entry);
    it->second.emplace_back(base::BindOnce(
        &RunOperationAndCallback, std::move(operation), std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  auto has_active = active_entries_.find(entry_hash);
  if (has_active != active_entries_.end()) {
    return OpenEntry(has_active->second->key(), net::HIGHEST, entry,
                     std::move(callback));
  }

  scoped_refptr<SimpleEntryImpl> simple_entry = new SimpleEntryImpl(
      cache_type_, path_, cleanup_tracker_.get(), entry_hash,
      entry_operations_mode_, this, file_tracker_, net_log_,
      GetNewEntryPriority(net::HIGHEST));
  CompletionOnceCallback backend_callback =
      base::BindOnce(&SimpleBackendImpl::OnEntryOpenedFromHash, AsWeakPtr(),
                     entry_hash, entry, simple_entry, std::move(callback));
  return simple_entry->OpenEntry(entry, std::move(backend_callback));
}

net::Error SimpleBackendImpl::DoomEntryFromHash(
    uint64_t entry_hash,
    CompletionOnceCallback callback) {
  Entry** entry = new Entry*();
  std::unique_ptr<Entry*> scoped_entry(entry);

  auto pending_it = entries_pending_doom_.find(entry_hash);
  if (pending_it != entries_pending_doom_.end()) {
    base::OnceCallback<net::Error(CompletionOnceCallback)> operation =
        base::BindOnce(&SimpleBackendImpl::DoomEntryFromHash,
                       base::Unretained(this), entry_hash);
    pending_it->second.emplace_back(base::BindOnce(
        &RunOperationAndCallback, std::move(operation), std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  auto active_it = active_entries_.find(entry_hash);
  if (active_it != active_entries_.end())
    return active_it->second->DoomEntry(std::move(callback));

  // There's no pending dooms, nor any open entry. We can make a trivial
  // call to DoomEntries() to delete this entry.
  std::vector<uint64_t> entry_hash_vector;
  entry_hash_vector.push_back(entry_hash);
  DoomEntries(&entry_hash_vector, std::move(callback));
  return net::ERR_IO_PENDING;
}

void SimpleBackendImpl::OnEntryOpenedFromHash(
    uint64_t hash,
    Entry** entry,
    const scoped_refptr<SimpleEntryImpl>& simple_entry,
    CompletionOnceCallback callback,
    int error_code) {
  if (error_code != net::OK) {
    std::move(callback).Run(error_code);
    return;
  }
  DCHECK(*entry);
  std::pair<EntryMap::iterator, bool> insert_result =
      active_entries_.insert(EntryMap::value_type(hash, simple_entry.get()));
  EntryMap::iterator& it = insert_result.first;
  const bool did_insert = insert_result.second;
  if (did_insert) {
    // There was no active entry corresponding to this hash. We've already put
    // the entry opened from hash in the |active_entries_|. We now provide the
    // proxy object to the entry.
    it->second->SetActiveEntryProxy(ActiveEntryProxy::Create(hash, this));
    std::move(callback).Run(net::OK);
  } else {
    // The entry was made active while we waiting for the open from hash to
    // finish. The entry created from hash needs to be closed, and the one
    // in |active_entries_| can be returned to the caller.
    simple_entry->Close();
    it->second->OpenEntry(entry, std::move(callback));
  }
}

void SimpleBackendImpl::DoomEntriesComplete(
    std::unique_ptr<std::vector<uint64_t>> entry_hashes,
    CompletionOnceCallback callback,
    int result) {
  for (const uint64_t& entry_hash : *entry_hashes)
    OnDoomComplete(entry_hash);
  std::move(callback).Run(result);
}

// static
void SimpleBackendImpl::FlushWorkerPoolForTesting() {
  // TODO(morlovich): Remove this, move everything over to disk_cache:: use.
  base::TaskScheduler::GetInstance()->FlushForTesting();
}

uint32_t SimpleBackendImpl::GetNewEntryPriority(
    net::RequestPriority request_priority) {
  if (base::FeatureList::IsEnabled(kPrioritizedSimpleCacheTasks)) {
    // Lower priority is better, so give high network priority the least bump.
    return ((net::RequestPriority::MAXIMUM_PRIORITY - request_priority) *
            10000) +
           entry_count_++;
  }

  return 0;
}

}  // namespace disk_cache
