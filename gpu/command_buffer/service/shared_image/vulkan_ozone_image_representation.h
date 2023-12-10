// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VULKAN_OZONE_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VULKAN_OZONE_IMAGE_REPRESENTATION_H_

#include "gpu/vulkan/buildflags.h"

#if BUILDFLAG(ENABLE_VULKAN)

#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

#include <vulkan/vulkan_core.h>

namespace gpu {

class OzoneImageBacking;
class VulkanDeviceQueue;
class VulkanImage;
class VulkanImplementation;

class GPU_GLES2_EXPORT VulkanOzoneImageRepresentation
    : public VulkanImageRepresentation {
 public:
  VulkanOzoneImageRepresentation(SharedImageManager* manager,
                                 SharedImageBacking* backing,
                                 MemoryTypeTracker* tracker,
                                 std::unique_ptr<gpu::VulkanImage> vulkan_image,
                                 gpu::VulkanDeviceQueue* vulkan_device_queue,
                                 gpu::VulkanImplementation& vulkan_impl);
  ~VulkanOzoneImageRepresentation() override;

  std::unique_ptr<ScopedAccess> BeginScopedAccess(
      AccessMode access_mode,
      std::vector<VkSemaphore>& begin_semaphores,
      std::vector<VkSemaphore>& end_semaphores) override;

 protected:
  void EndScopedAccess(bool is_read_only, VkSemaphore end_semaphore) override;

 private:
  gpu::OzoneImageBacking* ozone_backing() const {
    return reinterpret_cast<gpu::OzoneImageBacking*>(backing());
  }
};

}  // namespace gpu

#endif

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VULKAN_OZONE_IMAGE_REPRESENTATION_H_
