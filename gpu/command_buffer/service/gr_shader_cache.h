// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GR_SHADER_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GR_SHADER_CACHE_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/hash/hash.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/raster_export.h"
#include "third_party/skia/include/gpu/ganesh/GrContextOptions.h"

class GrDirectContext;

namespace gpu {
namespace raster {

class RASTER_EXPORT GrShaderCache
    : public GrContextOptions::PersistentCache,
      public base::trace_event::MemoryDumpProvider {
 public:
  class RASTER_EXPORT Client {
   public:
    virtual ~Client() {}

    virtual void StoreShader(const std::string& key,
                             const std::string& shader) = 0;
  };

  class RASTER_EXPORT ScopedCacheUse {
   public:
    ScopedCacheUse(GrShaderCache* cache, int32_t client_id);
    ~ScopedCacheUse();

   private:
    raw_ptr<GrShaderCache> cache_;
  };

  GrShaderCache(size_t max_cache_size_bytes, Client* client);

  GrShaderCache(const GrShaderCache&) = delete;
  GrShaderCache& operator=(const GrShaderCache&) = delete;

  ~GrShaderCache() override;

  // GrContextOptions::PersistentCache implementation.
  sk_sp<SkData> load(const SkData& key) override;
  void store(const SkData& key, const SkData& data) override;

  void PopulateCache(const std::string& key, const std::string& data);
  void CacheClientIdOnDisk(int32_t client_id);
  void PurgeMemory(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  size_t num_cache_entries() const;
  size_t curr_size_bytes_for_testing() const;

  void StoreVkPipelineCacheIfNeeded(GrDirectContext* gr_context);

 private:
  static constexpr int32_t kInvalidClientId = 0;

  struct CacheKey {
    explicit CacheKey(sk_sp<SkData> data);
    CacheKey(CacheKey&& other);
    CacheKey(const CacheKey& other);
    CacheKey& operator=(const CacheKey& other);
    CacheKey& operator=(CacheKey&& other);
    ~CacheKey();

    bool operator==(const CacheKey& other) const;

    sk_sp<SkData> data;
    size_t hash;
  };

  struct CacheData {
   public:
    explicit CacheData(sk_sp<SkData> data);
    CacheData(CacheData&& other);
    CacheData& operator=(CacheData&& other);
    ~CacheData();

    bool operator==(const CacheData& other) const;

    sk_sp<SkData> data;

    // Indicates that this cache entry needs to be written to the disk.
    bool pending_disk_write = true;

    // Indicates that this cache entry was loaded from the disk and hasn't been
    // read yet.
    bool prefetched_but_not_read = false;
  };

  struct CacheKeyHash {
    size_t operator()(const CacheKey& key) const { return key.hash; }
  };

  using Store = base::HashingLRUCache<CacheKey, CacheData, CacheKeyHash>;

  void EnforceLimits(size_t size_needed);

  Store::iterator AddToCache(CacheKey key, CacheData data);
  template <typename Iterator>
  void EraseFromCache(Iterator it);

  void WriteToDisk(const CacheKey& key, CacheData* data);

  bool IsVkPipelineCacheEntry(const CacheKey& key);

  int32_t current_client_id() const;

  mutable base::Lock lock_;
  size_t cache_size_limit_ GUARDED_BY(lock_) = 0u;
  size_t curr_size_bytes_ GUARDED_BY(lock_) = 0u;
  Store store_ GUARDED_BY(lock_);
  raw_ptr<Client> const client_ GUARDED_BY(lock_);
  base::flat_set<int32_t> client_ids_to_cache_on_disk_ GUARDED_BY(lock_);

  // Multiple threads and hence multiple clients can be accessing the shader
  // cache at the same time. Hence use per thread |current_client_id_|.
  base::flat_map<base::PlatformThreadId, int32_t> current_client_id_
      GUARDED_BY(lock_);
  bool need_store_pipeline_cache_ GUARDED_BY(lock_) = false;
  const bool enable_vk_pipeline_cache_;

  // Bound to the thread on which GrShaderCache is created. Some methods can
  // only be called on this thread. GrShaderCache is created on gpu main thread.
  THREAD_CHECKER(gpu_main_thread_checker_);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GR_SHADER_CACHE_H_
