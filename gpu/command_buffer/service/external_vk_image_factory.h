// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_FACTORY_H_

#include <vulkan/vulkan.h>
#include <memory>

#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class SharedContextState;
class VulkanCommandPool;

// This class is the SharedImageBackingFactory that is used on Linux when
// Vulkan/GL interoperability is required. The created backing is a VkImage that
// can be exported out of Vulkan and be used in GL. Synchronization between
// Vulkan and GL is done using VkSemaphores that are created with special flags
// that allow it to be exported out and shared with GL.
class GPU_GLES2_EXPORT ExternalVkImageFactory
    : public SharedImageBackingFactory {
 public:
  explicit ExternalVkImageFactory(
      scoped_refptr<SharedContextState> context_state);
  ~ExternalVkImageFactory() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      int client_id,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override;
  bool CanImportGpuMemoryBuffer(
      gfx::GpuMemoryBufferType memory_buffer_type) override;

 private:
  VkResult CreateExternalVkImage(VkFormat format,
                                 const gfx::Size& size,
                                 VkImage* image);

  void TransitionToColorAttachment(VkImage image);

  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<VulkanCommandPool> command_pool_;

  const VulkanImageUsageCache image_usage_cache_;

  DISALLOW_COPY_AND_ASSIGN(ExternalVkImageFactory);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_FACTORY_H_
