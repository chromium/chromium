// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_SKIA_VK_ANDROID_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_SKIA_VK_ANDROID_H_

#include <vulkan/vulkan.h>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image_backing_android.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {
class SharedContextState;
class SharedImageBackingAndroid;
class VulkanImage;
class VulkanImplementation;

// A generic Skia vulkan representation which can be used by any backing on
// Android.
class SharedImageRepresentationSkiaVkAndroid
    : public SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationSkiaVkAndroid(
      SharedImageManager* manager,
      SharedImageBackingAndroid* backing,
      scoped_refptr<SharedContextState> context_state,
      MemoryTypeTracker* tracker);

  ~SharedImageRepresentationSkiaVkAndroid() override;

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
  SharedImageBackingAndroid* android_backing() const {
    return static_cast<SharedImageBackingAndroid*>(backing());
  }

  SharedContextState* context_state() const { return context_state_.get(); }

  std::unique_ptr<VulkanImage> vulkan_image_;

  // Initial read fence to wait on before reading |vulkan_image_|.
  base::ScopedFD init_read_fence_;
  sk_sp<SkPromiseImageTexture> promise_texture_;

 private:
  bool BeginAccess(bool readonly,
                   std::vector<GrBackendSemaphore>* begin_semaphores,
                   std::vector<GrBackendSemaphore>* end_semaphores,
                   base::ScopedFD init_read_fence);
  void EndAccess(bool readonly);
  VkDevice vk_device();
  VulkanImplementation* vk_implementation();
  VkPhysicalDevice vk_phy_device();
  VkQueue vk_queue();

  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
  int surface_msaa_count_ = 0;
  sk_sp<SkSurface> surface_;
  scoped_refptr<SharedContextState> context_state_;
  VkSemaphore begin_access_semaphore_ = VK_NULL_HANDLE;
  VkSemaphore end_access_semaphore_ = VK_NULL_HANDLE;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_SKIA_VK_ANDROID_H_
