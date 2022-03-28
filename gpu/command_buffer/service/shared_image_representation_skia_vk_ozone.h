// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_SKIA_VK_OZONE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_SKIA_VK_OZONE_H_

#include <vulkan/vulkan.h>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image_backing_ozone.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {
class SharedContextState;
class SharedImageBackingOzone;
class VulkanImage;
class VulkanImplementation;

// A generic Skia vulkan representation which can be used by Ozone backing.
class SharedImageRepresentationSkiaVkOzone
    : public SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationSkiaVkOzone(
      SharedImageManager* manager,
      SharedImageBackingOzone* backing,
      scoped_refptr<SharedContextState> context_state,
      std::unique_ptr<VulkanImage> vulkan_image,
      MemoryTypeTracker* tracker);

  ~SharedImageRepresentationSkiaVkOzone() override;

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  void EndWriteAccess(sk_sp<SkSurface> surface) override;
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  void EndReadAccess() override;

 protected:
  SharedImageBackingOzone* ozone_backing() const {
    return static_cast<SharedImageBackingOzone*>(backing());
  }

  SharedContextState* context_state() const { return context_state_.get(); }

  std::unique_ptr<VulkanImage> vulkan_image_;
  sk_sp<SkPromiseImageTexture> promise_texture_;

 private:
  bool BeginAccess(bool readonly,
                   std::vector<GrBackendSemaphore>* begin_semaphores,
                   std::vector<GrBackendSemaphore>* end_semaphores);
  void EndAccess(bool readonly);
  std::unique_ptr<GrBackendSurfaceMutableState> GetEndAccessState();

  VkDevice vk_device();
  VulkanImplementation* vk_implementation();

  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
  int surface_msaa_count_ = 0;
  sk_sp<SkSurface> surface_;
  scoped_refptr<SharedContextState> context_state_;
  std::vector<VkSemaphore> begin_access_semaphores_;
  VkSemaphore end_access_semaphore_ = VK_NULL_HANDLE;
  bool need_end_fence_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_SKIA_VK_OZONE_H_
