// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GRAPHITE_VK_ANDROID_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GRAPHITE_VK_ANDROID_IMAGE_REPRESENTATION_H_

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image/android_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gpu {
class SharedContextState;
class VulkanImage;

// A generic Skia Graphite/Vulkan representation which can be used by any
// backing.
class GPU_GLES2_EXPORT SkiaGraphiteVkAndroidImageRepresentation
    : public SkiaGraphiteImageRepresentation {
 public:
  SkiaGraphiteVkAndroidImageRepresentation(
      SharedImageManager* manager,
      AndroidImageBacking* backing,
      scoped_refptr<SharedContextState> context_state,
      MemoryTypeTracker* tracker);
  ~SkiaGraphiteVkAndroidImageRepresentation() override;

  // SkiaGraphiteImageRepresentation implementation:
  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect) override;
  std::vector<scoped_refptr<GraphiteTextureHolder>> BeginWriteAccess() override;
  void EndWriteAccess() override;
  std::vector<scoped_refptr<GraphiteTextureHolder>> BeginReadAccess() override;
  void EndReadAccess() override;

 protected:
  AndroidImageBacking* android_backing() const {
    return static_cast<AndroidImageBacking*>(backing());
  }

  SharedContextState* context_state() const { return context_state_.get(); }

  VulkanImage* vulkan_image() const { return vulkan_image_.get(); }

  gpu::VulkanImplementation* GetVulkanImplementation() const;
  VkDevice GetVkDevice() const;

  void SetVulkanImage(std::unique_ptr<VulkanImage> vulkan_image);
  void SetInitialFence(base::ScopedFD initial_fence);

  bool BeginAccessInternal(bool readonly);
  void EndAccessInternal(bool readonly);

 private:
  scoped_refptr<GraphiteTextureHolder> CreateGraphiteTextureHolder();

  const scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<VulkanImage> vulkan_image_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;

  // Initial fence to wait on before accessing |vulkan_image_|.
  base::ScopedFD initial_fence_;
  VkSemaphore begin_access_semaphore_ = VK_NULL_HANDLE;
  VkSemaphore end_access_semaphore_ = VK_NULL_HANDLE;

  sk_sp<SkSurface> write_surface_;
  scoped_refptr<GraphiteTextureHolder> graphite_texture_holder_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GRAPHITE_VK_ANDROID_IMAGE_REPRESENTATION_H_
