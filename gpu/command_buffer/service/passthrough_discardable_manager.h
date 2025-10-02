// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_DISCARDABLE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_DISCARDABLE_MANAGER_H_

#include "base/containers/lru_cache.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
struct GpuPreferences;
namespace gles2 {
class TexturePassthrough;
class ContextGroup;
}  // namespace gles2

class GPU_GLES2_EXPORT PassthroughDiscardableManager {
 public:
  explicit PassthroughDiscardableManager(const GpuPreferences& preferences);

  PassthroughDiscardableManager(const PassthroughDiscardableManager&) = delete;
  PassthroughDiscardableManager& operator=(
      const PassthroughDiscardableManager&) = delete;

  ~PassthroughDiscardableManager();

  // Test only functions
  size_t TotalSizeForTesting() const { return 0; }

 private:
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
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_DISCARDABLE_MANAGER_H_
