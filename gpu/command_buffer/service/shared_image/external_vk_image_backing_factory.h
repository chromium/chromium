// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_BACKING_FACTORY_H_

#include <vulkan/vulkan_core.h>

#include <memory>

#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class SharedContextState;
class VulkanCommandPool;

// This class is the SharedImageBackingFactory that is used on Linux when
// Vulkan/GL interoperability is required. The created backing is a VkImage that
// can be exported out of Vulkan and be used in GL. Synchronization between
// Vulkan and GL is done using VkSemaphores that are created with special flags
// that allow it to be exported out and shared with GL.
class GPU_GLES2_EXPORT ExternalVkImageBackingFactory
    : public SharedImageBackingFactory {
 public:
  explicit ExternalVkImageBackingFactory(
      scoped_refptr<SharedContextState> context_state);

  ExternalVkImageBackingFactory(const ExternalVkImageBackingFactory&) = delete;
  ExternalVkImageBackingFactory& operator=(
      const ExternalVkImageBackingFactory&) = delete;

  ~ExternalVkImageBackingFactory() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe,
      base::span<const uint8_t> pixel_data) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      gfx::GpuMemoryBufferHandle handle) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe,
      gfx::BufferUsage buffer_usage) override;

  bool IsSupported(SharedImageUsageSet usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override;
  SharedImageBackingType GetBackingType() override;

 private:
  VkResult CreateExternalVkImage(VkFormat format,
                                 const gfx::Size& size,
                                 VkImage* image);

  void TransitionToColorAttachment(VkImage image);

  bool CanImportGpuMemoryBuffer(gfx::GpuMemoryBufferType memory_buffer_type);

  const scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<VulkanCommandPool> command_pool_;

  // Map VkImageUsageFlags flags based on VkFormat for VK_IMAGE_TILING_OPTIMAL.
  base::flat_map<VkFormat, VkImageUsageFlags> image_usage_cache_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_BACKING_FACTORY_H_
