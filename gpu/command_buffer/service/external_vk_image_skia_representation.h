// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_SKIA_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_SKIA_REPRESENTATION_H_

#include <vector>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"

namespace gpu {

class ExternalVkImageSkiaRepresentation : public SharedImageRepresentationSkia {
 public:
  ExternalVkImageSkiaRepresentation(SharedImageManager* manager,
                                    SharedImageBacking* backing,
                                    MemoryTypeTracker* tracker);
  ~ExternalVkImageSkiaRepresentation() override;

  // SharedImageRepresentationSkia implementation.
  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndWriteAccess(sk_sp<SkSurface> surface) override;
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndReadAccess() override;

 private:
  gpu::VulkanImplementation* vk_implementation() {
    return backing_impl()
        ->context_state()
        ->vk_context_provider()
        ->GetVulkanImplementation();
  }

  VkDevice vk_device() {
    return backing_impl()
        ->context_state()
        ->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanDevice();
  }

  VkQueue vk_queue() {
    return backing_impl()
        ->context_state()
        ->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanQueue();
  }

  VulkanFenceHelper* fence_helper() {
    return backing_impl()
        ->context_state()
        ->vk_context_provider()
        ->GetDeviceQueue()
        ->GetFenceHelper();
  }

  ExternalVkImageBacking* backing_impl() {
    return static_cast<ExternalVkImageBacking*>(backing());
  }

  sk_sp<SkPromiseImageTexture> BeginAccess(
      bool readonly,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);

  void EndAccess(bool readonly);

  enum AccessMode {
    kNone = 0,
    kRead = 1,
    kWrite = 2,
  };
  AccessMode access_mode_ = kNone;
  sk_sp<SkSurface> surface_;
  VkSemaphore end_access_semaphore_ = VK_NULL_HANDLE;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_SKIA_REPRESENTATION_H_
