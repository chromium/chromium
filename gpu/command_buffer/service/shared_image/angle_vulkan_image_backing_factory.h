// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANGLE_VULKAN_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANGLE_VULKAN_IMAGE_BACKING_FACTORY_H_

#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/gl_common_image_backing_factory.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class SharedContextState;

class GPU_GLES2_EXPORT AngleVulkanImageBackingFactory
    : public GLCommonImageBackingFactory {
 public:
  AngleVulkanImageBackingFactory(
      const GpuPreferences& gpu_preferences,
      const GpuDriverBugWorkarounds& workarounds,
      scoped_refptr<SharedContextState> context_state);
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
  bool IsSupported(SharedImageUsageSet usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override;
  SharedImageBackingType GetBackingType() override;

 private:
  bool IsGMBSupported(gfx::GpuMemoryBufferType gmb_type) const;
  bool CanUseAngleVulkanImageBacking(SharedImageUsageSet usage,
                                     gfx::GpuMemoryBufferType gmb_type) const;

  const scoped_refptr<SharedContextState> context_state_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANGLE_VULKAN_IMAGE_BACKING_FACTORY_H_
