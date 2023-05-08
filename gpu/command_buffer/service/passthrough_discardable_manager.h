// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_DISCARDABLE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_DISCARDABLE_MANAGER_H_

#include "base/containers/lru_cache.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
struct GpuPreferences;
namespace gles2 {
class TexturePassthrough;
class ContextGroup;
}  // namespace gles2

class GPU_GLES2_EXPORT PassthroughDiscardableManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  explicit PassthroughDiscardableManager(const GpuPreferences& preferences);

  PassthroughDiscardableManager(const PassthroughDiscardableManager&) = delete;
  PassthroughDiscardableManager& operator=(
      const PassthroughDiscardableManager&) = delete;

  ~PassthroughDiscardableManager() override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  void InitializeTexture(uint32_t client_id,
                         const gles2::ContextGroup* context_group,
                         size_t texture_size,
                         ServiceDiscardableHandle handle);
  bool UnlockTexture(uint32_t client_id,
                     const gles2::ContextGroup* context_group,
                     gles2::TexturePassthrough** texture_to_unbind);
  bool LockTexture(uint32_t client_id,
                   const gles2::ContextGroup* context_group);

  // Called when a context group is deleted, clean up all textures from this
  // group.
  void DeleteContextGroup(const gles2::ContextGroup* context_group,
                          bool has_context);

  // Called when all contexts with cached textures in this manager are lost.
  void OnContextLost();

  // Called when a texture is deleted, to clean up state.
  void DeleteTexture(uint32_t client_id,
                     const gles2::ContextGroup* context_group);

  // Called when a texture's size may have changed
  void UpdateTextureSize(uint32_t client_id,
                         const gles2::ContextGroup* context_group,
                         size_t new_size);

  void HandleMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Test only functions
  size_t NumCacheEntriesForTesting() const { return cache_.size(); }
  bool IsEntryLockedForTesting(uint32_t client_id,
                               const gles2::ContextGroup* context_group) const;
  size_t TotalSizeForTesting() const { return total_size_; }
  bool IsEntryTrackedForTesting(uint32_t client_id,
                                const gles2::ContextGroup* context_group) const;
  scoped_refptr<gles2::TexturePassthrough> UnlockedTextureForTesting(
      uint32_t client_id,
      const gles2::ContextGroup* context_group) const;
  void SetCacheSizeLimitForTesting(size_t cache_size_limit) {
    cache_size_limit_ = cache_size_limit;
  }

 private:
  void EnforceCacheSizeLimit(size_t limit);

  using DiscardableCacheKey = std::pair<uint32_t, const gles2::ContextGroup*>;
  struct DiscardableCacheValue {
    DiscardableCacheValue();
    DiscardableCacheValue(const DiscardableCacheValue& other);
    DiscardableCacheValue(DiscardableCacheValue&& other);
    DiscardableCacheValue& operator=(const DiscardableCacheValue& other);
    DiscardableCacheValue& operator=(DiscardableCacheValue&& other);
    ~DiscardableCacheValue();

    ServiceDiscardableHandle handle;
    uint32_t lock_count = 1;
    scoped_refptr<gles2::TexturePassthrough> unlocked_texture;
    size_t size = 0;
  };

  // Delete textures belonging to |context_group|. If |context_group| is null
  // then all textures are deleted.
  void DeleteTextures(const gles2::ContextGroup* context_group,
                      bool has_context);

  using DiscardableCache =
      base::LRUCache<DiscardableCacheKey, DiscardableCacheValue>;
  DiscardableCache cache_;

  // Total size of all entries in the cache. The same as summing
  // DiscardableCacheValue::size for each entry.
  size_t total_size_ = 0;

  // The limit above which the cache will start evicting resources.
  size_t cache_size_limit_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_DISCARDABLE_MANAGER_H_
