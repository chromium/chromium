// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_persistent_cache.h"

#include <optional>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected_macros.h"
#include "components/persistent_cache/persistent_cache.h"
#include "components/persistent_cache/transaction_error.h"
#include "gpu/command_buffer/service/memory_cache.h"
#include "ipc/common/gpu_client_ids.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

namespace {

constexpr size_t kMaxLoadStoreForTrackingCacheAvailable = 100;
constexpr base::TimeDelta kDiskWriteDelaySeconds = base::Seconds(1);
constexpr base::TimeDelta kDiskOpWaitTimeoutMs = base::Milliseconds(20);

class ScopedHistogramTimer {
 public:
  explicit ScopedHistogramTimer(const std::string& name) : name_(name) {}
  ~ScopedHistogramTimer() {
    if (enabled_) {
      base::UmaHistogramCustomMicrosecondsTimes(name_, timer_.Elapsed(),
                                                base::Microseconds(1),
                                                base::Seconds(30), 100);
    }
  }

  void SetEnabled(bool enabled) { enabled_ = enabled; }

 private:
  const std::string name_;
  base::ElapsedTimer timer_;
  bool enabled_ = true;
};

class DiskCacheTraceScope {
 public:
  explicit DiskCacheTraceScope(const char* name) : name_(name) {
    TRACE_EVENT_BEGIN0("gpu", name_);
  }
  ~DiskCacheTraceScope() {
    if (pending_bytes_) {
      TRACE_EVENT_END2("gpu", name_, "idle_id", idle_id_, "pending_bytes",
                       pending_bytes_);
    } else {
      TRACE_EVENT_END1("gpu", name_, "idle_id", idle_id_);
    }
  }

  void SetIdleId(uint64_t idle_id) { idle_id_ = idle_id; }
  void SetPendingBytes(size_t pending_bytes) { pending_bytes_ = pending_bytes; }

 private:
  const char* name_;
  uint64_t idle_id_ = 0;
  std::optional<size_t> pending_bytes_;
};

GLsizeiptr GL_APIENTRY GLBlobCacheGetCallback(const void* key,
                                              GLsizeiptr key_size,
                                              void* value_out,
                                              GLsizeiptr value_size,
                                              const void* user_param) {
  DCHECK(user_param != nullptr);
  GpuPersistentCache* cache =
      static_cast<GpuPersistentCache*>(const_cast<void*>(user_param));

  return cache->GLBlobCacheGet(key, key_size, value_out, value_size);
}

void GL_APIENTRY GLBlobCacheSetCallback(const void* key,
                                        GLsizeiptr key_size,
                                        const void* value,
                                        GLsizeiptr value_size,
                                        const void* user_param) {
  DCHECK(user_param != nullptr);
  GpuPersistentCache* cache =
      static_cast<GpuPersistentCache*>(const_cast<void*>(user_param));

  cache->GLBlobCacheSet(key, key_size, value, value_size);
}

// Cache prefix name used in all histograms, eg:
// GPU.PersistentCache.{CachePrefix}.MetricName
// Do not modify without changing
// tools/metrics/histograms/metadata/gpu/histograms.xml
const char* GetCacheHistogramPrefix(GpuDiskCacheHandle handle) {
  switch (GetHandleType(handle)) {
    case GpuDiskCacheType::kGlShaders:
      return IsReservedGpuDiskCacheHandle(handle) ? "Ganesh" : "WebGL";
    case GpuDiskCacheType::kDawnWebGPU:
      return "WebGPU";
    case GpuDiskCacheType::kDawnGraphite:
      return "GraphiteDawn";
    default:
      NOTREACHED();
  }
}

std::string GetHistogramName(std::string_view prefix, std::string_view metric) {
  return "GPU.PersistentCache." + std::string(prefix) + "." +
         std::string(metric);
}

NOINLINE NOOPT void HandlePersistentCacheError(
    GpuProcessShmCount* use_shader_cache_shm_count,
    persistent_cache::TransactionError error) {
  switch (error) {
    case persistent_cache::TransactionError::kPermanent:
      if (use_shader_cache_shm_count) {
        GpuProcessShmCount::ScopedIncrement scoped_increment(
            use_shader_cache_shm_count);
        base::ImmediateCrash();
      }
      break;
    default:
      break;
  }
}

bool TimedWait(base::ConditionVariable& cond_var,
               base::TimeDelta timeout,
               base::FunctionRef<bool()> wait_condition) {
  base::TimeTicks deadline = base::TimeTicks::Now() + timeout;
  while (wait_condition()) {
    base::TimeDelta remaining = deadline - base::TimeTicks::Now();
    if (!remaining.is_positive()) {
      return false;  // Timeout
    }
    cond_var.TimedWait(remaining);
  }
  return true;
}

}  // namespace

// AsyncDiskWriteOpts
GpuPersistentCache::AsyncDiskWriteOpts::AsyncDiskWriteOpts() = default;
GpuPersistentCache::AsyncDiskWriteOpts::AsyncDiskWriteOpts(
    const AsyncDiskWriteOpts&) = default;
GpuPersistentCache::AsyncDiskWriteOpts::AsyncDiskWriteOpts(
    AsyncDiskWriteOpts&&) = default;
GpuPersistentCache::AsyncDiskWriteOpts::~AsyncDiskWriteOpts() = default;
GpuPersistentCache::AsyncDiskWriteOpts&
GpuPersistentCache::AsyncDiskWriteOpts::operator=(const AsyncDiskWriteOpts&) =
    default;
GpuPersistentCache::AsyncDiskWriteOpts&
GpuPersistentCache::AsyncDiskWriteOpts::operator=(AsyncDiskWriteOpts&&) =
    default;

// Ref-counted wrapper for the persistent cache data, so it can be used safely
// with asynchronous operations.
struct GpuPersistentCache::DiskCache
    : public base::RefCountedThreadSafe<DiskCache> {
  explicit DiskCache(
      std::string_view cache_prefix,
      std::unique_ptr<persistent_cache::PersistentCache> cache,
      const GpuPersistentCache::AsyncDiskWriteOpts& async_write_options,
      scoped_refptr<RefCountedGpuProcessShmCount> use_shader_cache_shm_count);

  bool Load(std::string_view key,
            persistent_cache::BufferProvider buffer_provider);
  void Store(scoped_refptr<MemoryCacheEntry> entry);

  const persistent_cache::PersistentCache& persistent_cache() const {
    return *cache_;
  }

 private:
  friend class base::RefCountedThreadSafe<DiskCache>;
  ~DiskCache();

  void SignalUsingCacheComplete();
  void DoStoreToDisk(scoped_refptr<MemoryCacheEntry> entry);
  void DoDelayedStoreToDisk(scoped_refptr<MemoryCacheEntry> entry,
                            uint64_t idle_id);

  const std::string cache_prefix_;
  const std::unique_ptr<persistent_cache::PersistentCache> cache_;
  // Used to track cache activity to delay writes until idle. Relaxed memory
  // order is sufficient because we only need to detect if any change has
  // occurred, and the variable doesn't need to synchronize with other
  // variables.
  std::atomic<uint64_t> current_idle_id_{0};
  // Used to track the total bytes of pending writes. Relaxed memory order is
  // sufficient as this is used as a heuristic and strict synchronization is not
  // required.
  std::atomic<size_t> pending_bytes_to_write_{0};
  const scoped_refptr<base::SequencedTaskRunner> disk_write_task_runner_;
  const size_t max_pending_bytes_to_write_;

  const scoped_refptr<RefCountedGpuProcessShmCount> use_shader_cache_shm_count_;

  // Synchronization primitives to enforce timed waits between reads & writes.
  base::Lock cache_in_use_mutex_;
  base::ConditionVariable cache_in_use_cond_var_{&cache_in_use_mutex_};
  bool cache_in_use_ GUARDED_BY(cache_in_use_mutex_) = false;
};

GpuPersistentCache::DiskCache::DiskCache(
    std::string_view cache_prefix,
    std::unique_ptr<persistent_cache::PersistentCache> cache,
    const GpuPersistentCache::AsyncDiskWriteOpts& async_write_options,
    scoped_refptr<RefCountedGpuProcessShmCount> use_shader_cache_shm_count)
    : cache_prefix_(cache_prefix),
      cache_(std::move(cache)),
      disk_write_task_runner_(async_write_options.task_runner),
      max_pending_bytes_to_write_(
          async_write_options.max_pending_bytes_to_write),
      use_shader_cache_shm_count_(std::move(use_shader_cache_shm_count)) {}

GpuPersistentCache::DiskCache::~DiskCache() = default;

void GpuPersistentCache::DiskCache::SignalUsingCacheComplete() {
  {
    base::AutoLock lock(cache_in_use_mutex_);
    DCHECK(cache_in_use_);
    cache_in_use_ = false;
  }

  cache_in_use_cond_var_.Signal();
}

bool GpuPersistentCache::DiskCache::Load(
    std::string_view key,
    persistent_cache::BufferProvider buffer_provider) {
  ScopedHistogramTimer timer(GetHistogramName(cache_prefix_, "Load"));
  DiskCacheTraceScope trace_scope("GpuPersistentCache::DiskCache::Load");

  const uint64_t idle_id =
      current_idle_id_.fetch_add(1, std::memory_order_relaxed) + 1;
  trace_scope.SetIdleId(idle_id);

  // The persistent cache backend can't read and write in parallel. Wait for
  // any pending writes/reads to complete before loading from the cache, to
  // avoid long waits in the backend. We wait for a maximum of 10ms.
  {
    base::ScopedAllowBaseSyncPrimitives allow_base_sync_primitives;
    base::AutoLock lock(cache_in_use_mutex_);
    if (!TimedWait(cache_in_use_cond_var_, kDiskOpWaitTimeoutMs,
                   [this]() { return cache_in_use_; })) {
      // Treat as cache miss
      return false;
    }

    cache_in_use_ = true;
  }

  // The work
  base::expected<std::optional<persistent_cache::EntryMetadata>,
                 persistent_cache::TransactionError>
      result;
  {
    TRACE_EVENT0("gpu", "GpuPersistentCache::DiskCache::Cache::Find");
    result = cache_->Find(key, buffer_provider);
  }

  // Notify other threads
  SignalUsingCacheComplete();

  ASSIGN_OR_RETURN(auto metadata, result,
                   [&](persistent_cache::TransactionError error) {
                     HandlePersistentCacheError(
                         &use_shader_cache_shm_count_->data, error);
                     return false;
                   });

  return metadata.has_value();  // Hit if present; miss otherwise.
}

void GpuPersistentCache::DiskCache::Store(
    scoped_refptr<MemoryCacheEntry> entry) {
  DiskCacheTraceScope trace_scope("GpuPersistentCache::DiskCache::Store");
  const uint64_t idle_id =
      current_idle_id_.fetch_add(1, std::memory_order_relaxed) + 1;
  trace_scope.SetIdleId(idle_id);

  if (!disk_write_task_runner_) {
    // No async task runner, write to disk immediately.
    DoStoreToDisk(entry);
    return;
  }

  // Increment the pending bytes in write queue.
  const size_t bytes_to_write = entry->TotalSize();
  const auto pending_bytes = pending_bytes_to_write_.fetch_add(
      bytes_to_write, std::memory_order_relaxed);

  trace_scope.SetPendingBytes(pending_bytes + bytes_to_write);

  disk_write_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuPersistentCache::DiskCache::DoDelayedStoreToDisk,
                     base::WrapRefCounted(this), std::move(entry), idle_id),
      kDiskWriteDelaySeconds);
}

void GpuPersistentCache::DiskCache::DoStoreToDisk(
    scoped_refptr<MemoryCacheEntry> entry) {
  ScopedHistogramTimer timer(GetHistogramName(cache_prefix_, "Store"));
  TRACE_EVENT0("gpu", "GpuPersistentCache::DiskCache::DoStoreToDisk");

  {
    base::ScopedAllowBaseSyncPrimitives allow_base_sync_primitives;
    base::AutoLock lock(cache_in_use_mutex_);
    // Wait until the cache is not in use.
    while (cache_in_use_) {
      cache_in_use_cond_var_.Wait();
    }
    cache_in_use_ = true;
  }

  // The work.
  base::expected<void, persistent_cache::TransactionError> result;
  {
    TRACE_EVENT0("gpu", "GpuPersistentCache::DiskCache::Cache::Insert");
    result = cache_->Insert(entry->Key(), entry->Data());
  }

  // Unblock other threads.
  SignalUsingCacheComplete();

  RETURN_IF_ERROR(result, [&](persistent_cache::TransactionError error) {
    HandlePersistentCacheError(&use_shader_cache_shm_count_->data, error);
  });
}

void GpuPersistentCache::DiskCache::DoDelayedStoreToDisk(
    scoped_refptr<MemoryCacheEntry> entry,
    uint64_t idle_id) {
  DiskCacheTraceScope trace_scope(
      "GpuPersistentCache::DiskCache::DoDelayedStoreToDisk");
  trace_scope.SetIdleId(idle_id);

  // The idle ID is used to check if there has been any cache activity since
  // the delayed store task was posted. If the IDs don't match, it means
  // another cache operation has occurred, so we reschedule the task to wait
  // for the next idle period. This ensures that we only perform the write
  // when the cache is truly idle.
  const uint64_t current_idle_id =
      current_idle_id_.load(std::memory_order_relaxed);
  const bool idle_id_match = current_idle_id == idle_id;

  // We also force writing if the pending bytes exceed limit.
  const size_t pending_bytes =
      pending_bytes_to_write_.load(std::memory_order_relaxed);
  const bool exceed_max_pending_bytes =
      pending_bytes > max_pending_bytes_to_write_;

  trace_scope.SetPendingBytes(pending_bytes);

  if (idle_id_match || exceed_max_pending_bytes) {
    DoStoreToDisk(entry);
    pending_bytes_to_write_.fetch_sub(entry->TotalSize(),
                                      std::memory_order_relaxed);
    return;
  }

  // Re-schedule the write since the cache is not idle.
  disk_write_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuPersistentCache::DiskCache::DoDelayedStoreToDisk,
                     base::WrapRefCounted(this), std::move(entry),
                     current_idle_id),
      kDiskWriteDelaySeconds);
}

// GpuPersistentCache
GpuPersistentCache::GpuPersistentCache(std::string_view cache_prefix,
                                       scoped_refptr<MemoryCache> memory_cache,
                                       AsyncDiskWriteOpts async_write_options)
    : cache_prefix_(cache_prefix),
      memory_cache_(std::move(memory_cache)),
      async_write_options_(std::move(async_write_options)) {}

GpuPersistentCache::~GpuPersistentCache() = default;

void GpuPersistentCache::InitializeCache(
    persistent_cache::PendingBackend pending_backend,
    scoped_refptr<RefCountedGpuProcessShmCount> use_shader_cache_shm_count) {
  CHECK(!disk_cache_initialized_.IsSet());
  auto cache =
      persistent_cache::PersistentCache::Bind(std::move(pending_backend));
  if (!cache) {
    HandlePersistentCacheError(&use_shader_cache_shm_count->data,
                               persistent_cache::TransactionError::kPermanent);
    return;
  }

  disk_cache_ = base::MakeRefCounted<DiskCache>(
      cache_prefix_, std::move(cache), async_write_options_,
      std::move(use_shader_cache_shm_count));
  disk_cache_initialized_.Set();

  if (memory_cache_) {
    // If opening the persistent cache succeeded, copy all entries from the
    // memory cache into it.
    memory_cache_->ForEach([this](MemoryCacheEntry* memory_entry) {
      // Query the existence of the disk cache entry by providing an empty
      // buffer so no data is copied.
      bool exists = disk_cache_->Load(
          memory_entry->Key(), [](size_t) { return base::span<uint8_t>(); });
      if (!exists) {
        disk_cache_->Store(memory_entry);
      }
    });
  }
}

#if BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
size_t GpuPersistentCache::LoadData(const void* key,
                                    size_t key_size,
                                    void* value,
                                    size_t value_size) {
  std::string_view key_str(static_cast<const char*>(key), key_size);
  size_t discovered_size = 0;

  // A BufferProvider for PersistentCache that puts the size of the content, in
  // bytes, into `discovered_size` and returns a view into the buffer at
  // `value_out` if it is big enough or an empty span otherwise.
  // SAFETY: Caller provides either null `value` or `value` plus `value_size`.
  auto buffer_provider = [value = UNSAFE_BUFFERS(base::span(
                              static_cast<uint8_t*>(value), value_size)),
                          &discovered_size](size_t content_size) {
    // Cache hit: retain the size.
    discovered_size = content_size;

    if (value.size() >= content_size) {
      return value.first(content_size);
    }
    return base::span<uint8_t>();
  };

  CacheLoadResult result = LoadImpl(key_str, std::move(buffer_provider));
  if (!IsCacheHitResult(result) || value_size == 0) {
    // This function is called twice in the cache hit case, once to query the
    // size of the buffer and again with a buffer to write into. To avoid
    // skewing the metrics by generating two cache hit data points, only record
    // a cache hit when there is no buffer provided.
    RecordCacheLoadResultHistogram(result);
  }

  return static_cast<GLsizeiptr>(discovered_size);
}
#endif

sk_sp<SkData> GpuPersistentCache::load(const SkData& key) {
  std::string_view key_str(static_cast<const char*>(key.data()), key.size());
  sk_sp<SkData> output_data;

  // A BufferProvider for PersistentCache that allocates a new SkData to hold an
  // entry's content and returns a view into it.
  auto buffer_provider = [&output_data](size_t content_size) {
    output_data = SkData::MakeUninitialized(content_size);
    // SAFETY: SkData doesn't provide an API to get its buffer as a
    // writeable span.
    return UNSAFE_BUFFERS(
        base::span(static_cast<uint8_t*>(output_data->writable_data()),
                   output_data->size()));
  };

  CacheLoadResult result = LoadImpl(key_str, std::move(buffer_provider));
  RecordCacheLoadResultHistogram(result);

  return output_data;
}

int64_t GpuPersistentCache::GLBlobCacheGet(const void* key,
                                           int64_t key_size,
                                           void* value_out,
                                           int64_t value_size) {
  CHECK_GE(key_size, 0);
  std::string_view key_str(static_cast<const char*>(key),
                           static_cast<size_t>(key_size));
  size_t discovered_size = 0;

  // A BufferProvider for PersistentCache that puts the size of the content, in
  // bytes, into `discovered_size` and returns a view into the buffer at
  // `value_out` if it is big enough or an empty span otherwise.
  // SAFETY: Caller provides either null `value_out` or `value_out` plus
  // `value_size`.
  auto buffer_provider =
      [value = UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(value_out),
                                         static_cast<size_t>(value_size))),
       &discovered_size](size_t content_size) {
        // Cache hit: retain the size to return to the caller.
        discovered_size = content_size;
        if (value.size() >= content_size) {
          return value.first(content_size);
        }
        return base::span<uint8_t>();
      };

  CacheLoadResult result = LoadImpl(key_str, std::move(buffer_provider));
  if (!IsCacheHitResult(result) || value_size == 0) {
    // This function is called twice in the cache hit case, once to query the
    // size of the buffer and again with a buffer to write into. To avoid
    // skewing the metrics by generating two cache hit data points, only record
    // a cache hit when there is no buffer provided.
    RecordCacheLoadResultHistogram(result);
  }

  return discovered_size;
}

void GpuPersistentCache::PurgeMemory(
    base::MemoryPressureLevel memory_pressure_level) {
  if (memory_cache_) {
    memory_cache_->PurgeMemory(memory_pressure_level);
  }
}

void GpuPersistentCache::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (memory_cache_) {
    memory_cache_->OnMemoryDump(dump_name, pmd);
  }
}

const persistent_cache::PersistentCache&
GpuPersistentCache::GetPersistentCacheForTesting() const {
  return disk_cache_->persistent_cache();
}

bool GpuPersistentCache::IsCacheHitResult(CacheLoadResult result) {
  return result > CacheLoadResult::kMaxMissValue;
}

GpuPersistentCache::CacheLoadResult GpuPersistentCache::LoadImpl(
    std::string_view key,
    persistent_cache::BufferProvider buffer_provider) {
  const bool disk_cache_initialized = disk_cache_initialized_.IsSet();
  TRACE_EVENT1("gpu", "GpuPersistentCache::LoadImpl", "persistent_cache",
               disk_cache_initialized);

  // Track cache available for the 1st kMaxLoadStoreForTrackingCacheAvailable
  // loads.
  if (load_count_.fetch_add(1, std::memory_order_relaxed) <
      kMaxLoadStoreForTrackingCacheAvailable) {
    base::UmaHistogramBoolean(
        GetHistogramName(cache_prefix_, "Load.CacheAvailable"),
        disk_cache_initialized);
  }

  if (memory_cache_) {
    if (auto memory_entry = memory_cache_->Find(key)) {
      base::span<uint8_t> output_buffer =
          buffer_provider(memory_entry->DataSize());
      memory_entry->ReadData(output_buffer.data(), output_buffer.size());
      return CacheLoadResult::kHitMemory;
    }
  }

  if (!disk_cache_initialized) {
    return CacheLoadResult::kMissNoDiskCache;
  }

  base::span<uint8_t> provided_buffer;
  base::HeapArray<uint8_t> local_allocated_buffer;

  // A BufferProvider for PersistentCache that returns one of:
  // 1.  a view into the buffer at `provided_buffer` if it is big enough
  // 2.  an empty span if no memory_cache_ exists, or
  // 3.  a view into a new base::HeapArray (`local_allocated_buffer`)
  auto wrapped_buffer_provider =
      [buffer_provider, memory_cache_exists = memory_cache_ != nullptr,
       &provided_buffer, &local_allocated_buffer](size_t content_size) {
        // First attempt to use the buffer_provider to allocate a buffer for the
        // result.
        provided_buffer = buffer_provider(content_size);

        // If the `provided_buffer` is large enough, simply return it and let
        // the disk cache write into it
        if (provided_buffer.size() >= content_size) {
          return provided_buffer.first(content_size);  // Case 1.
        }

        if (!memory_cache_exists) {
          return base::span<uint8_t>();  // Case 2.
        }

        // Allocate our own buffer into `local_allocated_buffer` so the result
        // can be put in the memory cache
        DCHECK(content_size != 0);
        local_allocated_buffer = base::HeapArray<uint8_t>::Uninit(content_size);
        return base::span<uint8_t>(local_allocated_buffer);  // Case 3.
      };

  if (!disk_cache_->Load(key, wrapped_buffer_provider)) {
    return CacheLoadResult::kMiss;
  }

  if (memory_cache_) {
    // Verify the assumptions above. There should always be data in one of the
    // two buffers if the load was successful and a memory cache exists.
    DCHECK(!local_allocated_buffer.empty() || !provided_buffer.empty());

    // After loading from the disk cache, copy the entry into the memory cache
    // for faster access on future loads.
    if (!local_allocated_buffer.empty()) {
      // Prefer the `local_allocated_buffer` because it can be moved directly
      // into the memory cache.
      memory_cache_->Store(key, std::move(local_allocated_buffer));
    } else {
      // Otherwise we need to copy the result from the user provided buffer
      memory_cache_->Store(key, provided_buffer);
    }
  }

  return CacheLoadResult::kHitDisk;
}

#if BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
void GpuPersistentCache::StoreData(const void* key,
                                   size_t key_size,
                                   const void* value,
                                   size_t value_size) {
  std::string_view key_str(static_cast<const char*>(key), key_size);
  base::span<const uint8_t> value_span = UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(value), value_size));
  StoreImpl(key_str, value_span);
}
#endif

void GpuPersistentCache::store(const SkData& key, const SkData& data) {
  std::string_view key_str(static_cast<const char*>(key.data()), key.size());
  base::span<const uint8_t> value_span = UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(data.bytes()), data.size()));
  StoreImpl(key_str, value_span);
}

void GpuPersistentCache::GLBlobCacheSet(const void* key,
                                        int64_t key_size,
                                        const void* value,
                                        int64_t value_size) {
  CHECK_GE(key_size, 0);
  CHECK_GE(value_size, 0);
  std::string_view key_str(static_cast<const char*>(key),
                           static_cast<size_t>(key_size));
  base::span<const uint8_t> value_span = UNSAFE_BUFFERS(base::span(
      static_cast<const uint8_t*>(value), static_cast<size_t>(value_size)));
  StoreImpl(key_str, value_span);
}

void GpuPersistentCache::StoreImpl(std::string_view key,
                                   base::span<const uint8_t> value) {
  const bool disk_cache_initialized = disk_cache_initialized_.IsSet();
  TRACE_EVENT1("gpu", "GpuPersistentCache::StoreImpl", "persistent_cache",
               disk_cache_initialized);

  // Track cache available for the 1st kMaxLoadStoreForTrackingCacheAvailable
  // stores.
  if (store_count_.fetch_add(1, std::memory_order_relaxed) <
      kMaxLoadStoreForTrackingCacheAvailable) {
    base::UmaHistogramBoolean(
        GetHistogramName(cache_prefix_, "Store.CacheAvailable"),
        disk_cache_initialized);
  }

  scoped_refptr<MemoryCacheEntry> memory_cache_entry;
  if (memory_cache_) {
    memory_cache_entry = memory_cache_->Store(key, value);
  }

  if (!disk_cache_initialized) {
    return;
  }

  // If there was no memory cache, wrap the data in a new MemoryCacheEntry for
  // insertion.
  if (!memory_cache_entry) {
    memory_cache_entry = base::MakeRefCounted<MemoryCacheEntry>(key, value);
  }

  disk_cache_->Store(memory_cache_entry);
}

void GpuPersistentCache::RecordCacheLoadResultHistogram(
    CacheLoadResult result) {
  base::UmaHistogramEnumeration(GetHistogramName(cache_prefix_, "LoadResult"),
                                result);
}

void BindCacheToCurrentOpenGLContext(GpuPersistentCache* cache) {
  if (!cache || !gl::g_current_gl_driver->ext.b_GL_ANGLE_blob_cache) {
    return;
  }

  glBlobCacheCallbacksANGLE(GLBlobCacheSetCallback, GLBlobCacheGetCallback,
                            cache);
}

void UnbindCacheFromCurrentOpenGLContext() {
  if (!gl::g_current_gl_driver->ext.b_GL_ANGLE_blob_cache) {
    return;
  }

  glBlobCacheCallbacksANGLE(nullptr, nullptr, nullptr);
}

GpuPersistentCacheCollection::GpuPersistentCacheCollection(
    size_t max_in_memory_cache_size,
    GpuPersistentCache::AsyncDiskWriteOpts async_write_options)
    : max_in_memory_cache_size_(max_in_memory_cache_size),
      async_write_options_(async_write_options) {
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "GpuPersistentCache",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

GpuPersistentCacheCollection::~GpuPersistentCacheCollection() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

scoped_refptr<GpuPersistentCache> GpuPersistentCacheCollection::GetCache(
    const GpuDiskCacheHandle& handle) {
  base::AutoLock lock(mutex_);
  if (auto iter = caches_.find(handle); iter != caches_.end()) {
    return iter->second.get();
  }

  auto memory_cache =
      base::MakeRefCounted<MemoryCache>(max_in_memory_cache_size_);

  auto [iter, inserted] = caches_.emplace(
      handle, base::MakeRefCounted<GpuPersistentCache>(
                  GetCacheHistogramPrefix(handle), std::move(memory_cache),
                  async_write_options_));
  DCHECK(inserted);
  return iter->second;
}

void GpuPersistentCacheCollection::PurgeMemory(
    base::MemoryPressureLevel memory_pressure_level) {
  base::AutoLock lock(mutex_);
  for (auto& [_, cache] : caches_) {
    cache->PurgeMemory(memory_pressure_level);
  }
}

bool GpuPersistentCacheCollection::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(mutex_);
  for (auto& [handle, cache] : caches_) {
    std::ostringstream dump_name;
    dump_name << "gpu/shader_cache/" << GetCacheHistogramPrefix(handle);
    if (!IsReservedGpuDiskCacheHandle(handle)) {
      int32_t value = GetHandleValue(handle);
      DCHECK_GE(value, 0);
      dump_name << "_" << value;
    }
    cache->OnMemoryDump(dump_name.str(), pmd);
  }
  return true;
}

}  // namespace gpu
