// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_VK_OZONE_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_VK_OZONE_IMAGE_REPRESENTATION_H_

#include <vulkan/vulkan.h>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gpu {
class SharedContextState;
class OzoneImageBacking;
class VulkanImage;
class VulkanImplementation;

// A generic Skia vulkan representation which can be used by Ozone backing.
class SkiaVkOzoneImageRepresentation : public SkiaGaneshImageRepresentation {
 public:
  SkiaVkOzoneImageRepresentation(
      SharedImageManager* manager,
      OzoneImageBacking* backing,
      scoped_refptr<SharedContextState> context_state,
      std::vector<std::unique_ptr<VulkanImage>> vulkan_images,
      MemoryTypeTracker* tracker);

  ~SkiaVkOzoneImageRepresentation() override;

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndWriteAccess() override;
  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndReadAccess() override;

 protected:
  OzoneImageBacking* ozone_backing() const {
    return static_cast<OzoneImageBacking*>(backing());
  }

  SharedContextState* context_state() const { return context_state_.get(); }

  std::vector<std::unique_ptr<VulkanImage>> vulkan_images_;
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures_;

 private:
  bool BeginAccess(bool readonly,
                   std::vector<GrBackendSemaphore>* begin_semaphores,
                   std::vector<GrBackendSemaphore>* end_semaphores);
  void EndAccess(bool readonly);
  std::unique_ptr<skgpu::MutableTextureState> GetEndAccessState();

  VkDevice vk_device();
  VulkanImplementation* vk_implementation();

  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
  int surface_msaa_count_ = 0;
  std::vector<sk_sp<SkSurface>> surfaces_;
  scoped_refptr<SharedContextState> context_state_;
  std::vector<VkSemaphore> begin_access_semaphores_;
  VkSemaphore end_access_semaphore_ = VK_NULL_HANDLE;
  bool need_end_fence_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_VK_OZONE_IMAGE_REPRESENTATION_H_
