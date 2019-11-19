// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SERVICE_DISCARDABLE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SERVICE_DISCARDABLE_MANAGER_H_

#include <vector>

#include "base/containers/mru_cache.h"
#include "base/memory/memory_pressure_listener.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
namespace gles2 {
class TextureManager;
class TextureRef;
}

GPU_GLES2_EXPORT size_t DiscardableCacheSizeLimit();
GPU_GLES2_EXPORT size_t DiscardableCacheSizeLimitForPressure(
    size_t base_cache_limit,
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

class GPU_GLES2_EXPORT ServiceDiscardableManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  ServiceDiscardableManager();
  ~ServiceDiscardableManager() override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  void InsertLockedTexture(uint32_t texture_id,
                           size_t texture_size,
                           gles2::TextureManager* texture_manager,
                           ServiceDiscardableHandle handle);

  // Unlocks the indicated texture. If *|texture_to_unbind| is not nullptr,
  // ServiceDiscardableManager has taken ownership of the given texture, and
  // it is the callers responsibility to unbind it from any other objects.
  // Returns false if the given texture_id has not been initialized for use
  // with discardable.
  bool UnlockTexture(uint32_t texture_id,
                     gles2::TextureManager* texture_manager,
                     gles2::TextureRef** texture_to_unbind);
  // Locks the indicated texture, allowing it to be used in future GL commands.
  // Returns false if the given texture_id has not been initialized for use
  // with discardable.
  bool LockTexture(uint32_t texture_id, gles2::TextureManager* texture_manager);

  // Returns all unlocked texture refs to the texture_manager for deletion.
  // After this point, this class will have no references to the given
  // |texture_manager|.
  void OnTextureManagerDestruction(gles2::TextureManager* texture_manager);

  // Called when a texture is deleted, to clean up state.
  void OnTextureDeleted(uint32_t texture_id,
                        gles2::TextureManager* texture_manager);

  // Called by the TextureManager when a texture's size changes.
  void OnTextureSizeChanged(uint32_t texture_id,
                            gles2::TextureManager* texture_manager,
                            size_t new_size);

  // Test only functions:
  size_t NumCacheEntriesForTesting() const { return entries_.size(); }
  bool IsEntryLockedForTesting(uint32_t texture_id,
                               gles2::TextureManager* texture_manager) const;
  size_t TotalSizeForTesting() const { return total_size_; }
  gles2::TextureRef* UnlockedTextureRefForTesting(
      uint32_t texture_id,
      gles2::TextureManager* texture_manager) const;

  void SetCacheSizeLimitForTesting(size_t cache_size_limit) {
    cache_size_limit_ = cache_size_limit;
  }

  void HandleMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

 private:
  void EnforceCacheSizeLimit(size_t limit);

  struct GpuDiscardableEntry {
   public:
    GpuDiscardableEntry(ServiceDiscardableHandle handle, size_t size);
    GpuDiscardableEntry(const GpuDiscardableEntry& other);
    GpuDiscardableEntry(GpuDiscardableEntry&& other);
    ~GpuDiscardableEntry();

    ServiceDiscardableHandle handle;
    scoped_refptr<gles2::TextureRef> unlocked_texture_ref;
    // The current ref count of this object with regards to command buffer
    // execution. May be out of sync with the handle's refcount, as the handle
    // can be locked out of band with the command buffer.
    uint32_t service_ref_count_ = 1;
    size_t size;
  };
  struct GpuDiscardableEntryKey {
    uint32_t texture_id;
    gles2::TextureManager* texture_manager;
  };
  struct GpuDiscardableEntryKeyCompare {
    bool operator()(const GpuDiscardableEntryKey& lhs,
                    const GpuDiscardableEntryKey& rhs) const {
      return std::tie(lhs.texture_manager, lhs.texture_id) <
             std::tie(rhs.texture_manager, rhs.texture_id);
    }
  };
  using EntryCache = base::MRUCache<GpuDiscardableEntryKey,
                                    GpuDiscardableEntry,
                                    GpuDiscardableEntryKeyCompare>;
  EntryCache entries_;

  // Total size of all |entries_|. The same as summing
  // GpuDiscardableEntry::size for each entry.
  size_t total_size_ = 0;

  // The limit above which the cache will start evicting resources.
  size_t cache_size_limit_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ServiceDiscardableManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SERVICE_DISCARDABLE_MANAGER_H_
