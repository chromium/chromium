// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/angle_vulkan_image_backing_factory.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/angle_vulkan_image_backing.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {
namespace {

// TODO(penghuang): verify the scanout is the right usage for video playback.
// crbug.com/1280798
constexpr uint32_t kSupportedUsage =
#if BUILDFLAG(IS_LINUX)
    SHARED_IMAGE_USAGE_SCANOUT |
#endif
    SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
    SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
    SHARED_IMAGE_USAGE_CPU_UPLOAD;

}  // namespace

AngleVulkanImageBackingFactory::AngleVulkanImageBackingFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    SharedContextState* context_state)
    : GLCommonImageBackingFactory(kSupportedUsage,
                                  gpu_preferences,
                                  workarounds,
                                  context_state->feature_info(),
                                  context_state->progress_reporter()),
      context_state_(context_state) {
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
    uint32_t usage,
    bool is_thread_safe) {
  auto backing = std::make_unique<AngleVulkanImageBacking>(
      context_state_, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage);

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
    uint32_t usage,
    base::span<const uint8_t> data) {
  auto backing = std::make_unique<AngleVulkanImageBacking>(
      context_state_, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage);

  if (!backing->Initialize(data))
    return nullptr;

  return backing;
}

std::unique_ptr<SharedImageBacking>
AngleVulkanImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  auto resource_format = viz::GetResourceFormat(buffer_format);
  auto si_format = viz::SharedImageFormat::SinglePlane(resource_format);
  auto backing = std::make_unique<AngleVulkanImageBacking>(
      context_state_, mailbox, si_format, size, color_space, surface_origin,
      alpha_type, usage);

  if (!backing->InitializeWihGMB(std::move(handle)))
    return nullptr;

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
    uint32_t usage,
    gfx::GpuMemoryBufferType gmb_type) const {
  if (usage & ~kSupportedUsage) {
    return false;
  }

  if (!IsGMBSupported(gmb_type))
    return false;

  // AngleVulkan backing is used for GL & Vulkan interop, so the usage must
  // contain GLES2, unless it is created from GPU memory buffer.
  // TODO(penghuang): use AngleVulkan backing for non GL & Vulkan interop usage?
  if (gmb_type == gfx::EMPTY_BUFFER)
    return usage & SHARED_IMAGE_USAGE_GLES2;

  return true;
}

bool AngleVulkanImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  DCHECK_EQ(gr_context_type, GrContextType::kVulkan);

  if (format.is_multi_plane()) {
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

}  // namespace gpu
