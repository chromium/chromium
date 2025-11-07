// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_persistent_cache.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected_macros.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/persistent_cache.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

namespace {

constexpr size_t kMaxLoadStoreForTrackingCacheAvailable = 100;
constexpr base::TimeDelta kDiskWriteDelaySeconds = base::Seconds(1);

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

std::string GetHistogramName(std::string_view prefix, std::string_view metric) {
  return "GPU.PersistentCache." + std::string(prefix) + "." +
         std::string(metric);
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
      const GpuPersistentCache::AsyncDiskWriteOpts& async_write_options);

  std::unique_ptr<persistent_cache::Entry> Load(std::string_view key);
  void Store(std::string_view key, base::span<const uint8_t> value);

 private:
  friend class base::RefCountedThreadSafe<DiskCache>;
  ~DiskCache();

  void DoStoreToDisk(std::string_view key, base::span<const uint8_t> value);
  void DoDelayedStoreToDisk(std::string key,
                            std::vector<uint8_t> value,
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
};

GpuPersistentCache::DiskCache::DiskCache(
    std::string_view cache_prefix,
    std::unique_ptr<persistent_cache::PersistentCache> cache,
    const GpuPersistentCache::AsyncDiskWriteOpts& async_write_options)
    : cache_prefix_(cache_prefix),
      cache_(std::move(cache)),
      disk_write_task_runner_(async_write_options.task_runner),
      max_pending_bytes_to_write_(
          async_write_options.max_pending_bytes_to_write) {}
GpuPersistentCache::DiskCache::~DiskCache() = default;

std::unique_ptr<persistent_cache::Entry> GpuPersistentCache::DiskCache::Load(
    std::string_view key) {
  ScopedHistogramTimer timer(GetHistogramName(cache_prefix_, "Load"));
  DiskCacheTraceScope trace_scope("GpuPersistentCache::DiskCache::Load");

  const uint64_t idle_id =
      current_idle_id_.fetch_add(1, std::memory_order_relaxed) + 1;
  trace_scope.SetIdleId(idle_id);

  ASSIGN_OR_RETURN(auto entry, cache_->Find(key),
                   [](persistent_cache::TransactionError error)
                       -> std::unique_ptr<persistent_cache::Entry> {
                     // TODO(crbug.com/377475540): Handle or at least address
                     // permanent errors.
                     return nullptr;
                   });

  return entry;
}

void GpuPersistentCache::DiskCache::Store(std::string_view key,
                                          base::span<const uint8_t> value) {
  DiskCacheTraceScope trace_scope("GpuPersistentCache::DiskCache::Store");
  const uint64_t idle_id =
      current_idle_id_.fetch_add(1, std::memory_order_relaxed) + 1;
  trace_scope.SetIdleId(idle_id);

  if (!disk_write_task_runner_) {
    // No async task runner, write to disk immediately.
    DoStoreToDisk(key, value);
    return;
  }

  // Increment the pending bytes in write queue.
  const size_t bytes_to_write = key.size() + value.size();
  const auto pending_bytes = pending_bytes_to_write_.fetch_add(
      bytes_to_write, std::memory_order_relaxed);

  trace_scope.SetPendingBytes(pending_bytes + bytes_to_write);

  disk_write_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuPersistentCache::DiskCache::DoDelayedStoreToDisk,
                     base::WrapRefCounted(this), std::string(key),
                     std::vector<uint8_t>(value.begin(), value.end()), idle_id),
      kDiskWriteDelaySeconds);
}

void GpuPersistentCache::DiskCache::DoStoreToDisk(
    std::string_view key,
    base::span<const uint8_t> value) {
  ScopedHistogramTimer timer(GetHistogramName(cache_prefix_, "Store"));
  TRACE_EVENT0("gpu", "GpuPersistentCache::DiskCache::DoStoreToDisk");
  RETURN_IF_ERROR(cache_->Insert(key, value),
                  [](persistent_cache::TransactionError error) {
                    // TODO(crbug.com/377475540): Handle or at least address
                    // permanent errors.
                    return;
                  });
}

void GpuPersistentCache::DiskCache::DoDelayedStoreToDisk(
    std::string key,
    std::vector<uint8_t> value,
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
    DoStoreToDisk(key, value);
    pending_bytes_to_write_.fetch_sub(key.size() + value.size(),
                                      std::memory_order_relaxed);
    return;
  }

  // Re-schedule the write since the cache is not idle.
  disk_write_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuPersistentCache::DiskCache::DoDelayedStoreToDisk,
                     base::WrapRefCounted(this), std::move(key),
                     std::move(value), current_idle_id),
      kDiskWriteDelaySeconds);
}

// GpuPersistentCache
GpuPersistentCache::GpuPersistentCache(std::string_view cache_prefix,
                                       AsyncDiskWriteOpts async_write_options)
    : cache_prefix_(cache_prefix),
      async_write_options_(std::move(async_write_options)) {}

GpuPersistentCache::~GpuPersistentCache() = default;

void GpuPersistentCache::InitializeCache(
    persistent_cache::BackendParams backend_params) {
  CHECK(!initialized_.IsSet());
  auto cache =
      persistent_cache::PersistentCache::Open(std::move(backend_params));
  if (cache) {
    disk_cache_ = base::MakeRefCounted<DiskCache>(
        cache_prefix_, std::move(cache), async_write_options_);
    initialized_.Set();
  }
}

size_t GpuPersistentCache::LoadData(const void* key,
                                    size_t key_size,
                                    void* value,
                                    size_t value_size) {
  std::string_view key_str(static_cast<const char*>(key), key_size);
  std::unique_ptr<persistent_cache::Entry> entry = LoadImpl(key_str);
  if (!entry) {
    return 0;
  }

  if (value_size > 0) {
    return entry->CopyContentTo(
        UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(value), value_size)));
  }

  return entry->GetContentSize();
}

sk_sp<SkData> GpuPersistentCache::load(const SkData& key) {
  std::string_view key_str(static_cast<const char*>(key.data()), key.size());
  std::unique_ptr<persistent_cache::Entry> entry = LoadImpl(key_str);
  if (!entry) {
    return nullptr;
  }

  sk_sp<SkData> output_data =
      SkData::MakeUninitialized(entry->GetContentSize());
  entry->CopyContentTo(UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(output_data->writable_data()),
                 output_data->size())));
  return output_data;
}

int64_t GpuPersistentCache::GLBlobCacheGet(const void* key,
                                           int64_t key_size,
                                           void* value_out,
                                           int64_t value_size) {
  CHECK_GE(key_size, 0);
  std::string_view key_str(static_cast<const char*>(key),
                           static_cast<size_t>(key_size));
  std::unique_ptr<persistent_cache::Entry> entry = LoadImpl(key_str);
  if (!entry) {
    return 0;
  }

  if (value_size > 0) {
    return entry->CopyContentTo(UNSAFE_BUFFERS(base::span(
        static_cast<uint8_t*>(value_out), static_cast<size_t>(value_size))));
  }

  return static_cast<GLsizeiptr>(entry->GetContentSize());
}

std::unique_ptr<persistent_cache::Entry> GpuPersistentCache::LoadEntry(
    std::string_view key) {
  return LoadImpl(key);
}

std::unique_ptr<persistent_cache::Entry> GpuPersistentCache::LoadImpl(
    std::string_view key) {
  const bool initialized = initialized_.IsSet();
  TRACE_EVENT1("gpu", "GpuPersistentCache::LoadImpl", "persistent_cache",
               initialized);

  // Track cache available for the 1st kMaxLoadStoreForTrackingCacheAvailable
  // loads.
  if (load_count_.fetch_add(1, std::memory_order_relaxed) <
      kMaxLoadStoreForTrackingCacheAvailable) {
    base::UmaHistogramBoolean(
        GetHistogramName(cache_prefix_, "Load.CacheAvailable"), initialized);
  }

  if (!initialized) {
    return nullptr;
  }

  return disk_cache_->Load(key);
}

void GpuPersistentCache::StoreData(const void* key,
                                   size_t key_size,
                                   const void* value,
                                   size_t value_size) {
  std::string_view key_str(static_cast<const char*>(key), key_size);
  base::span<const uint8_t> value_span = UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(value), value_size));
  StoreImpl(key_str, value_span);
}

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
  const bool initialized = initialized_.IsSet();
  TRACE_EVENT1("gpu", "GpuPersistentCache::StoreImpl", "persistent_cache",
               initialized);

  // Track cache available for the 1st kMaxLoadStoreForTrackingCacheAvailable
  // stores.
  if (store_count_.fetch_add(1, std::memory_order_relaxed) <
      kMaxLoadStoreForTrackingCacheAvailable) {
    base::UmaHistogramBoolean(
        GetHistogramName(cache_prefix_, "Store.CacheAvailable"), initialized);
  }

  if (!initialized) {
    return;
  }

  disk_cache_->Store(key, value);
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

}  // namespace gpu
