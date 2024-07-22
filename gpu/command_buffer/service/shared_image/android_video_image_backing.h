// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANDROID_VIDEO_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANDROID_VIDEO_IMAGE_BACKING_H_

#include <memory>
#include <optional>
#include <string>

#include "gpu/command_buffer/service/shared_image/android_image_backing.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"

namespace viz {
class VulkanContextProvider;
}  // namespace viz

namespace gpu {
class DawnContextProvider;
struct Mailbox;
struct VulkanYCbCrInfo;
class AbstractTextureAndroid;
class RefCountedLock;
class StreamTextureSharedImageInterface;
class SharedContextState;
class TextureOwner;

// Implementation of SharedImageBacking that renders MediaCodec buffers to a
// TextureOwner or overlay as needed in order to draw them.
class GPU_GLES2_EXPORT AndroidVideoImageBacking : public AndroidImageBacking {
 public:
  static std::unique_ptr<AndroidVideoImageBacking> Create(
      const Mailbox& mailbox,
      const gfx::Size& size,
      const gfx::ColorSpace color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      std::string debug_label,
      scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
      scoped_refptr<SharedContextState> context_state,
      scoped_refptr<RefCountedLock> drdc_lock);

  // Returns ycbcr information. This is only valid in vulkan/dawn contexts and
  // nullopt for other contexts.
  static std::optional<VulkanYCbCrInfo> GetYcbcrInfo(
      TextureOwner* texture_owner,
      viz::VulkanContextProvider* vulkan_context_provider,
      DawnContextProvider* dawn_context_provider);

  ~AndroidVideoImageBacking() override;

  // Disallow copy and assign.
  AndroidVideoImageBacking(const AndroidVideoImageBacking&) = delete;
  AndroidVideoImageBacking& operator=(const AndroidVideoImageBacking&) = delete;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  size_t GetEstimatedSizeForMemoryDump() const override;

 protected:
  AndroidVideoImageBacking(const Mailbox& mailbox,
                           const gfx::Size& size,
                           const gfx::ColorSpace color_space,
                           GrSurfaceOrigin surface_origin,
                           SkAlphaType alpha_type,
                           std::string debug_label,
                           bool is_thread_safe);

  std::unique_ptr<AbstractTextureAndroid> GenAbstractTexture(
      const bool passthrough);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANDROID_VIDEO_IMAGE_BACKING_H_
