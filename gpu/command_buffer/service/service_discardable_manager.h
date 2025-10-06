// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SERVICE_DISCARDABLE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SERVICE_DISCARDABLE_MANAGER_H_

#include <vector>

#include "base/containers/lru_cache.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
struct GpuPreferences;
namespace gles2 {
class TextureManager;
class TextureRef;
}

class GPU_GLES2_EXPORT ServiceDiscardableManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  explicit ServiceDiscardableManager(const GpuPreferences& preferences);

  ServiceDiscardableManager(const ServiceDiscardableManager&) = delete;
  ServiceDiscardableManager& operator=(const ServiceDiscardableManager&) =
      delete;

  ~ServiceDiscardableManager() override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
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
    raw_ptr<gles2::TextureManager> texture_manager;
  };
  struct GpuDiscardableEntryKeyCompare {
    bool operator()(const GpuDiscardableEntryKey& lhs,
                    const GpuDiscardableEntryKey& rhs) const {
      return std::tie(lhs.texture_manager, lhs.texture_id) <
             std::tie(rhs.texture_manager, rhs.texture_id);
    }
  };
  using EntryCache = base::LRUCache<GpuDiscardableEntryKey,
                                    GpuDiscardableEntry,
                                    GpuDiscardableEntryKeyCompare>;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SERVICE_DISCARDABLE_MANAGER_H_
