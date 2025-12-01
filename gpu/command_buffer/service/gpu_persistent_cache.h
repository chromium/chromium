// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_

#include <dawn/platform/DawnPlatform.h>

#include <atomic>
#include <memory>
#include <string_view>

#include "base/memory/memory_pressure_listener.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/persistent_cache/buffer_provider.h"
#include "components/persistent_cache/pending_backend.h"
#include "gpu/command_buffer/common/shm_count.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/gpu/ganesh/GrContextOptions.h"

namespace persistent_cache {
class PersistentCache;
}  // namespace persistent_cache

namespace gpu {

class MemoryCache;

// Wraps a persistent_cache::PersistentCache to be used as a Dawn, Skia or ANGLE
// cache. Entries are always stored in a MemoryCache and PersistentCache as well
// once it is initialized. Entries loaded before the PersistentCache is
// initialized are copied into it on initialization.
class GPU_GLES2_EXPORT GpuPersistentCache
    : public dawn::platform::CachingInterface,
      public GrContextOptions::PersistentCache,
      public base::RefCountedThreadSafe<GpuPersistentCache> {
 public:
  struct GPU_GLES2_EXPORT AsyncDiskWriteOpts {
    AsyncDiskWriteOpts();
    AsyncDiskWriteOpts(const AsyncDiskWriteOpts&);
    AsyncDiskWriteOpts(AsyncDiskWriteOpts&&);
    ~AsyncDiskWriteOpts();
    AsyncDiskWriteOpts& operator=(const AsyncDiskWriteOpts&);
    AsyncDiskWriteOpts& operator=(AsyncDiskWriteOpts&&);

    // The task runner to use for asynchronous writes. If null, writes will be
    // synchronous.
    scoped_refptr<base::SequencedTaskRunner> task_runner;
    // The maximum number of bytes that can be pending for an asynchronous
    // write. If the pending bytes exceed this limit, the write will be
    // performed after the initial delay and will not be rescheduled even if the
    // cache is not idle.
    size_t max_pending_bytes_to_write = std::numeric_limits<size_t>::max();
  };

  // If `async_write_options.task_runner` is null, then writes are synchronous.
  explicit GpuPersistentCache(std::string_view cache_prefix,
                              scoped_refptr<MemoryCache> memory_cache,
                              AsyncDiskWriteOpts async_write_options = {});

  GpuPersistentCache(const GpuPersistentCache&) = delete;
  GpuPersistentCache& operator=(const GpuPersistentCache&) = delete;

  // This can only be called once but is thread safe w.r.t loads and stores.
  void InitializeCache(persistent_cache::PendingBackend pending_backend,
                       scoped_refptr<RefCountedGpuProcessShmCount>
                           use_shader_cache_shm_count = nullptr);

  // dawn::platform::CachingInterface implementation.
  size_t LoadData(const void* key,
                  size_t key_size,
                  void* value,
                  size_t value_size) override;
  void StoreData(const void* key,
                 size_t key_size,
                 const void* value,
                 size_t value_size) override;

  // GrContextOptions::PersistentCache implementation.
  sk_sp<SkData> load(const SkData& key) override;
  void store(const SkData& key, const SkData& data) override;

  // OpenGL ES (GL_ANGLE_blob_cache)
  int64_t GLBlobCacheGet(const void* key,
                         int64_t key_size,
                         void* value_out,
                         int64_t value_size);
  void GLBlobCacheSet(const void* key,
                      int64_t key_size,
                      const void* value,
                      int64_t value_size);

  void PurgeMemory(base::MemoryPressureLevel memory_pressure_level);

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::ProcessMemoryDump* pmd);

  const persistent_cache::PersistentCache& GetPersistentCacheForTesting() const;

 private:
  friend class base::RefCountedThreadSafe<GpuPersistentCache>;

  ~GpuPersistentCache() override;

  struct DiskCache;

  // Values are mirrored in tools/metrics/histograms/metadata/gpu/enums.xml
  enum class CacheLoadResult {
    kMiss = 0,
    kMissNoDiskCache = 1,
    kMaxMissValue = kMissNoDiskCache,
    // Extra enum space for future miss results
    kHitMemory = 10,
    kHitDisk = 11,
    kMaxValue = kHitDisk,
  };

  static bool IsCacheHitResult(CacheLoadResult result);

  CacheLoadResult LoadImpl(std::string_view key,
                           persistent_cache::BufferProvider buffer_provider);
  void StoreImpl(std::string_view key, base::span<const uint8_t> value);

  void RecordCacheLoadResultHistogram(CacheLoadResult result);

  // Prefix to prepend to UMA histogram's name. e.g GraphiteDawn, WebGPU
  const std::string cache_prefix_;

  std::atomic<size_t> load_count_ = 0;
  std::atomic<size_t> store_count_ = 0;

  // A MemoryCache is used for fast access to the most recently used elements of
  // the cache and allows data to be stored before the persistent_cache is
  // initialized
  scoped_refptr<MemoryCache> memory_cache_;

  base::AtomicFlag disk_cache_initialized_;
  scoped_refptr<DiskCache> disk_cache_;
  const AsyncDiskWriteOpts async_write_options_;
};

void BindCacheToCurrentOpenGLContext(GpuPersistentCache* cache);
void UnbindCacheFromCurrentOpenGLContext();

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_
