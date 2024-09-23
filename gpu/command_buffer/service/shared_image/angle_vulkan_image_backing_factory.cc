// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/angle_vulkan_image_backing_factory.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/angle_vulkan_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {
namespace {

// TODO(penghuang): verify the scanout is the right usage for video playback.
// crbug.com/1280798
constexpr SharedImageUsageSet kSupportedUsage =
#if BUILDFLAG(IS_LINUX)
    SHARED_IMAGE_USAGE_SCANOUT |
#endif
    SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
    SHARED_IMAGE_USAGE_GLES2_FOR_RASTER_ONLY | SHARED_IMAGE_USAGE_RASTER_READ |
    SHARED_IMAGE_USAGE_RASTER_WRITE |
    SHARED_IMAGE_USAGE_RASTER_OVER_GLES2_ONLY |
    SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
    SHARED_IMAGE_USAGE_OOP_RASTERIZATION | SHARED_IMAGE_USAGE_CPU_UPLOAD;

}  // namespace

AngleVulkanImageBackingFactory::AngleVulkanImageBackingFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    scoped_refptr<SharedContextState> context_state)
    : GLCommonImageBackingFactory(kSupportedUsage,
                                  gpu_preferences,
                                  workarounds,
                                  context_state->feature_info(),
                                  context_state->progress_reporter()),
      context_state_(std::move(context_state)) {
  DCHECK(context_state_->GrContextIsVulkan());
  DCHECK(gl::GLSurfaceEGL::GetGLDisplayEGL()->ext->b_EGL_ANGLE_vulkan_image);
}

AngleVulkanImageBackingFactory::~AngleVulkanImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
AngleVulkanImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe) {
  auto backing = std::make_unique<AngleVulkanImageBacking>(
      context_state_, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, std::move(debug_label));

  if (!backing->Initialize({}))
    return nullptr;

  return backing;
}

std::unique_ptr<SharedImageBacking>
AngleVulkanImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    base::span<const uint8_t> data) {
  auto backing = std::make_unique<AngleVulkanImageBacking>(
      context_state_, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, std::move(debug_label));

  if (!backing->Initialize(data)) {
    return nullptr;
  }

  return backing;
}

std::unique_ptr<SharedImageBacking>
AngleVulkanImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    gfx::GpuMemoryBufferHandle handle) {
  auto backing = std::make_unique<AngleVulkanImageBacking>(
      context_state_, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, std::move(debug_label));

  if (!backing->InitializeWihGMB(std::move(handle))) {
    return nullptr;
  }

  return backing;
}

bool AngleVulkanImageBackingFactory::IsGMBSupported(
    gfx::GpuMemoryBufferType gmb_type) const {
  switch (gmb_type) {
    case gfx::EMPTY_BUFFER:
      return true;
    case gfx::NATIVE_PIXMAP: {
      auto* vulkan_implementation =
          context_state_->vk_context_provider()->GetVulkanImplementation();
      auto* device_queue =
          context_state_->vk_context_provider()->GetDeviceQueue();
      return vulkan_implementation->CanImportGpuMemoryBuffer(device_queue,
                                                             gmb_type);
    }
    default:
      return false;
  }
}

bool AngleVulkanImageBackingFactory::CanUseAngleVulkanImageBacking(
    SharedImageUsageSet usage,
    gfx::GpuMemoryBufferType gmb_type) const {
  if (!IsGMBSupported(gmb_type))
    return false;

  // AngleVulkan backing is used for GL & Vulkan interop, so the usage must
  // contain GLES2, unless it is created from GPU memory buffer.
  // TODO(penghuang): use AngleVulkan backing for non GL & Vulkan interop usage?
  if (gmb_type == gfx::EMPTY_BUFFER)
    return HasGLES2ReadOrWriteUsage(usage);

  return true;
}

bool AngleVulkanImageBackingFactory::IsSupported(
    SharedImageUsageSet usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  DCHECK_EQ(gr_context_type, GrContextType::kVulkan);

  if (!HasVkFormat(format)) {
    return false;
  }

  if (!CanUseAngleVulkanImageBacking(usage, gmb_type)) {
    return false;
  }

  if (thread_safe) {
    return false;
  }

  return CanCreateTexture(format, size, pixel_data, GL_TEXTURE_2D);
}

SharedImageBackingType AngleVulkanImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kAngleVulkan;
}

}  // namespace gpu
