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
    SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
    SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
    SHARED_IMAGE_USAGE_CPU_UPLOAD;

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
    const SharedImageInfo& si_info,
    SurfaceHandle surface_handle,
    bool is_thread_safe) {
  auto backing = std::make_unique<AngleVulkanImageBacking>(context_state_,
                                                           mailbox, si_info);

  if (!backing->Initialize({}))
    return nullptr;

  return backing;
}

std::unique_ptr<SharedImageBacking>
AngleVulkanImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    bool is_thread_safe,
    base::span<const uint8_t> data) {
  auto backing = std::make_unique<AngleVulkanImageBacking>(context_state_,
                                                           mailbox, si_info);

  if (!backing->Initialize(data)) {
    return nullptr;
  }

  return backing;
}

std::unique_ptr<SharedImageBacking>
AngleVulkanImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    const SharedImageInfo& si_info,
    bool is_thread_safe,
    gfx::GpuMemoryBufferHandle handle) {
  auto backing = std::make_unique<AngleVulkanImageBacking>(context_state_,
                                                           mailbox, si_info);

  if (!backing->InitializeWihGMB(std::move(handle))) {
    return nullptr;
  }

  return backing;
}

bool AngleVulkanImageBackingFactory::IsGMBSupported(
    gfx::GpuMemoryBufferType gmb_type,
    SharedImageUsageSet usage) const {
  switch (gmb_type) {
    // AngleVulkan backing is used for GL & Vulkan interop, so the usage must
    // contain GLES2, unless it is created from GPU memory buffer.
    // TODO(penghuang): use AngleVulkan backing for non GL & Vulkan interop
    // usage?
    case gfx::EMPTY_BUFFER:
      return HasGLES2ReadOrWriteUsage(usage);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
    case gfx::NATIVE_PIXMAP: {
      auto* vulkan_implementation =
          context_state_->vk_context_provider()->GetVulkanImplementation();
      auto* device_queue =
          context_state_->vk_context_provider()->GetDeviceQueue();
      return vulkan_implementation->CanImportGpuMemoryBuffer(device_queue,
                                                             gmb_type);
    }
#endif
    default:
      return false;
  }
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
  if (thread_safe) {
    return false;
  }

  if (!HasVkFormat(format)) {
    return false;
  }

  if (!IsGMBSupported(gmb_type, usage)) {
    return false;
  }

  return CanCreateTexture(format, size, pixel_data, GL_TEXTURE_2D);
}

bool AngleVulkanImageBackingFactory::IsSupportedForAccessStream(
    SharedImageAccessStream stream,
    viz::SharedImageFormat format,
    const AccessParams* params) const {
  // `AngleVulkanImageBackingFactory` is strictly bound to the
  // `SharedContextState` it was created with. If a request is made from a
  // different thread/context, we must return false early to protect the
  // subsequent `IsSupported` call which accesses `context_state_`.
  // Note that this currently restricts this factory to only be selected and
  // used on the GPU main thread. If it's refactored in the future to remove its
  // dependency on `SharedContextState` in `IsSupported`, this restriction can
  // be relaxed.
  if (params && params->context_state &&
      params->context_state != context_state_) {
    return false;
  }
  return true;
}

SharedImageBackingType AngleVulkanImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kAngleVulkan;
}

}  // namespace gpu
