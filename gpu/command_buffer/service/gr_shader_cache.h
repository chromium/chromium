// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GR_SHADER_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GR_SHADER_CACHE_H_

#include "base/containers/flat_set.h"
#include "base/containers/mru_cache.h"
#include "base/hash/hash.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/gpu/GrContextOptions.h"

namespace gpu {
namespace raster {

class GPU_GLES2_EXPORT GrShaderCache
    : public GrContextOptions::PersistentCache,
      public base::trace_event::MemoryDumpProvider {
 public:
  class GPU_GLES2_EXPORT Client {
   public:
    virtual ~Client() {}

    virtual void StoreShader(const std::string& key,
                             const std::string& shader) = 0;
  };

  class GPU_GLES2_EXPORT ScopedCacheUse {
   public:
    ScopedCacheUse(GrShaderCache* cache, int32_t client_id);
    ~ScopedCacheUse();

   private:
    GrShaderCache* cache_;
  };

  GrShaderCache(size_t max_cache_size_bytes, Client* client);
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

  size_t num_cache_entries() const { return store_.size(); }
  size_t curr_size_bytes_for_testing() const { return curr_size_bytes_; }

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
    bool pending_disk_write = true;
  };

  struct CacheKeyHash {
    size_t operator()(const CacheKey& key) const { return key.hash; }
  };

  using Store = base::HashingMRUCache<CacheKey, CacheData, CacheKeyHash>;

  void EnforceLimits(size_t size_needed);

  Store::iterator AddToCache(CacheKey key, CacheData data);
  template <typename Iterator>
  void EraseFromCache(Iterator it);

  void WriteToDisk(const CacheKey& key, CacheData* data);

  size_t cache_size_limit_;
  size_t curr_size_bytes_ = 0u;
  Store store_;

  Client* const client_;
  base::flat_set<int32_t> client_ids_to_cache_on_disk_;

  int32_t current_client_id_ = kInvalidClientId;

  DISALLOW_COPY_AND_ASSIGN(GrShaderCache);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GR_SHADER_CACHE_H_
