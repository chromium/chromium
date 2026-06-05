// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_

#include <atomic>
#include <map>
#include <memory>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/containers/heap_array.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/persistent_cache/buffer_provider.h"
#include "components/persistent_cache/pending_backend.h"
#include "gpu/command_buffer/common/shm_count.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/gpu/ganesh/GrContextOptions.h"
#include "ui/gl/buildflags.h"

#if BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
#include <dawn/platform/DawnPlatform.h>
#endif

namespace persistent_cache {
class PersistentCache;
}  // namespace persistent_cache

namespace gpu {

class MemoryCache;

// Wraps a persistent_cache::PersistentCache to be used as a Dawn, Skia or ANGLE
// cache. Entries are always stored in a MemoryCache and PersistentCache as well
// once it is initialized. Entries loaded before the PersistentCache is
// initialized are copied into it on initialization.
class GPU_GLES2_EXPORT GpuPersistentCache :
#if BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
    public dawn::platform::CachingInterface,
#endif
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

  struct GPU_GLES2_EXPORT MetadataOpts {
    MetadataOpts();

    // Enable reading and writing of metadata about previous runs from the disk
    // cache.
    bool enabled = false;

    // Number of keys to write as the "first accessed" items in the cache. These
    // will be loaded first in subsequent runs.
    size_t preload_count = 50;
  };

  // Metadata structure which stores information about cache usage. It is
  // written to the cache periodically and can be used to improve the
  // performance of future runs.
  // Public for unit testing only.
  struct GPU_GLES2_EXPORT CacheMetadata {
    CacheMetadata();
    CacheMetadata(const CacheMetadata&);
    CacheMetadata(CacheMetadata&&);
    ~CacheMetadata();
    CacheMetadata& operator=(const CacheMetadata&);
    CacheMetadata& operator=(CacheMetadata&&);

    base::HeapArray<uint8_t> Serialize() const;
    static CacheMetadata Deserialize(const base::HeapArray<uint8_t>& data);

    // The first N keys queried in the cache.
    // TODO(geofflang): Determine if it's faster to order the keys. If the
    // preloading of entries races with the queries, we want to load in the
    // order that the entries were queried.
    base::flat_set<std::string> first_loaded_keys;
  };

  // If `async_write_options.task_runner` is null, then writes are synchronous.
  explicit GpuPersistentCache(std::string_view cache_prefix,
                              scoped_refptr<MemoryCache> memory_cache,
                              MetadataOpts metadata_options = {},
                              AsyncDiskWriteOpts async_write_options = {});

  GpuPersistentCache(const GpuPersistentCache&) = delete;
  GpuPersistentCache& operator=(const GpuPersistentCache&) = delete;

  // This can only be called once but is thread safe w.r.t loads and stores.
  void InitializeCache(persistent_cache::PendingBackend pending_backend,
                       scoped_refptr<RefCountedGpuProcessShmCount>
                           use_shader_cache_shm_count = nullptr);

#if BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
  // dawn::platform::CachingInterface implementation.
  size_t FindKey(std::span<const std::byte> key) override;
  size_t LoadData(std::span<const std::byte> key,
                  std::span<std::byte> dest) override;
  void StoreData(std::span<const std::byte> key,
                 std::span<const std::byte> src) override;
  // TODO(503801946): Remove these outdated non-spanified implementations once
  // we have migrated to use the one's above.
  size_t LoadData(const void* key,
                  size_t key_size,
                  void* value,
                  size_t value_size) override;
  void StoreData(const void* key,
                 size_t key_size,
                 const void* value,
                 size_t value_size) override;
#endif

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
  void FlushMetadataForTesting();

 private:
  friend class base::RefCountedThreadSafe<GpuPersistentCache>;

  ~GpuPersistentCache() override;

  struct DiskCache;

  // LINT.IfChange(CacheLoadResult)
  enum class CacheLoadResult {
    kMiss = 0,
    kMissNoDiskCache = 1,
    kMissTimeout = 2,
    kMissTransactionError = 3,
    kMaxMissValue = kMissTransactionError,
    // Extra enum space for future miss results
    kHitMemory = 10,
    kHitDisk = 11,
    kMaxValue = kHitDisk,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/gpu/enums.xml:GpuPersistentCacheLoadResult)

  // LINT.IfChange(MetadataEvent)
  enum class MetadataEvent {
    kCacheInitialized = 0,
    kMetadataLoaded = 1,
    kMetadataFlushed = 2,
    kMaxValue = kMetadataFlushed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/gpu/enums.xml:GpuPersistentCacheMetadataEvent)

  static bool IsCacheHitResult(CacheLoadResult result);

  CacheLoadResult LoadImpl(std::string_view key,
                           persistent_cache::BufferProvider buffer_provider);
  void StoreImpl(std::string_view key,
                 base::span<const uint8_t> value,
                 bool skip_memory_cache);

  void RecordCacheLoadResultHistogram(CacheLoadResult result);

  // Call after querying for a cache key to update the metadata of which entries
  // are loaded first.
  void UpdateCacheMetadataForLoad(std::string_view key);

  // Serialize and write the metadata to the disk cache.
  void FlushCacheMetadata(CacheMetadata metadata);

  // Attempt to load metadata from disk and process it, if it exists.
  void LoadAndProcessMetadataFromDiskCache();

  // Process the metadata:
  // - Load first_loaded_keys from disk into the memory cache
  void ProcessMetadata(const CacheMetadata& metadata);

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

  const MetadataOpts metadata_options_;
  base::Lock metadata_mutex_;
  std::optional<CacheMetadata> cache_metadata_ GUARDED_BY(metadata_mutex_);
};

void BindCacheToCurrentOpenGLContext(GpuPersistentCache* cache);
void UnbindCacheFromCurrentOpenGLContext();

class GPU_GLES2_EXPORT GpuPersistentCacheCollection
    : public base::trace_event::MemoryDumpProvider {
 public:
  explicit GpuPersistentCacheCollection(
      size_t max_in_memory_cache_size,
      GpuPersistentCache::MetadataOpts metadata_options,
      GpuPersistentCache::AsyncDiskWriteOpts async_write_options);
  ~GpuPersistentCacheCollection() override;

  scoped_refptr<GpuPersistentCache> GetCache(const GpuDiskCacheHandle& handle);

  void PurgeMemory(base::MemoryPressureLevel memory_pressure_level);

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  const size_t max_in_memory_cache_size_;
  const GpuPersistentCache::MetadataOpts metadata_options_;
  const GpuPersistentCache::AsyncDiskWriteOpts async_write_options_;

  base::Lock mutex_;
  std::map<GpuDiskCacheHandle, scoped_refptr<GpuPersistentCache>> caches_
      GUARDED_BY(mutex_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_
