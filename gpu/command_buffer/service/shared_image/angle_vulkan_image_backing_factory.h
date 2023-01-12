// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANGLE_VULKAN_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANGLE_VULKAN_IMAGE_BACKING_FACTORY_H_

#include "gpu/command_buffer/service/shared_image/gl_common_image_backing_factory.h"

namespace gpu {

class SharedContextState;

class AngleVulkanImageBackingFactory : public GLCommonImageBackingFactory {
 public:
  AngleVulkanImageBackingFactory(const GpuPreferences& gpu_preferences,
                                 const GpuDriverBugWorkarounds& workarounds,
                                 SharedContextState* context_state);
  ~AngleVulkanImageBackingFactory() override;

  // SharedImageBackingFactory implementation:
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override;
  bool IsSupported(uint32_t usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override;

 private:
  bool IsGMBSupported(gfx::GpuMemoryBufferType gmb_type) const;
  bool CanUseAngleVulkanImageBacking(uint32_t usage,
                                     gfx::GpuMemoryBufferType gmb_type) const;

  raw_ptr<SharedContextState> context_state_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANGLE_VULKAN_IMAGE_BACKING_FACTORY_H_
