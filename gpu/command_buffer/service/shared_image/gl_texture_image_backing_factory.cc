// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_factory.h"

#include <list>
#include <utility>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {
namespace {

BASE_FEATURE(kCorrectFramebufferAttachmentComputationInGLTexture,
             "CorrectFramebufferAttachmentComputationInGLTexture",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr uint32_t kWebGPUUsages =
    SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE;

constexpr uint32_t kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
    SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
    SHARED_IMAGE_USAGE_OOP_RASTERIZATION | SHARED_IMAGE_USAGE_SCANOUT |
    SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU | SHARED_IMAGE_USAGE_CPU_UPLOAD |
    kWebGPUUsages;
}

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageBackingFactory

GLTextureImageBackingFactory::GLTextureImageBackingFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const gles2::FeatureInfo* feature_info,
    gl::ProgressReporter* progress_reporter,
    bool for_cpu_upload_usage)
    : GLCommonImageBackingFactory(kSupportedUsage,
                                  gpu_preferences,
                                  workarounds,
                                  feature_info,
                                  progress_reporter),
      for_cpu_upload_usage_(for_cpu_upload_usage),
      support_all_metal_usages_(false) {}

GLTextureImageBackingFactory::~GLTextureImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
GLTextureImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  return CreateSharedImageInternal(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, std::move(debug_label), base::span<const uint8_t>());
}

std::unique_ptr<SharedImageBacking>
GLTextureImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    base::span<const uint8_t> pixel_data) {
  return CreateSharedImageInternal(mailbox, format, kNullSurfaceHandle, size,
                                   color_space, surface_origin, alpha_type,
                                   usage, std::move(debug_label), pixel_data);
}

std::unique_ptr<SharedImageBacking>
GLTextureImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    gfx::GpuMemoryBufferHandle handle) {
  NOTREACHED_NORETURN();
}

std::unique_ptr<SharedImageBacking>
GLTextureImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

bool GLTextureImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (format.is_multi_plane() && !use_passthrough_) {
    // With validating command decoder the clear rect tracking doesn't work with
    // multi-planar textures.
    return false;
  }
  if (!pixel_data.empty() && gr_context_type != GrContextType::kGL) {
    return false;
  }
  if (thread_safe) {
    return false;
  }
  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  bool has_cpu_upload_usage = usage & SHARED_IMAGE_USAGE_CPU_UPLOAD;

  if (for_cpu_upload_usage_ != has_cpu_upload_usage) {
    return false;
  }

  if (has_cpu_upload_usage) {
    if (!GLTextureImageBacking::SupportsPixelUploadWithFormat(format)) {
      return false;
    }

    // Don't reject scanout usage for shared memory GMBs to match legacy
    // behaviour from GLImageBackingFactory.
  } else {
    if (usage & SHARED_IMAGE_USAGE_SCANOUT) {
      return false;
    }
  }

  // This is not beneficial on iOS. The main purpose of this is a multi-gpu
  // support.
  if (!support_all_metal_usages_) {
    if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
        gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
      constexpr uint32_t kMetalInvalidUsages =
          SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_SCANOUT |
          SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
          SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;
      if (usage & kMetalInvalidUsages) {
        return false;
      }
    }
  }

  // Using GLTextureImageBacking for raster/display is only appropriate when
  // running on top of GL. For the case WebGL fallback (GrContextType::kNone)
  // this usages aren't actually relevant but WebGL still adds them so ignore.
  if (gr_context_type != GrContextType::kGL &&
      gr_context_type != GrContextType::kNone) {
    constexpr uint32_t kUnsupportedUsages =
        SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
        SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE;
    if (usage & kUnsupportedUsages) {
      return false;
    }
  }

  // Only supports WebGPU usages on Dawn's OpenGLES backend.
  if (usage & kWebGPUUsages) {
    if (use_webgpu_adapter_ != WebGPUAdapterName::kOpenGLES ||
        gl::GetGLImplementation() != gl::kGLImplementationEGLANGLE ||
        gl::GetANGLEImplementation() != gl::ANGLEImplementation::kOpenGL) {
      return false;
    }
  }

  return CanCreateTexture(format, size, pixel_data, GL_TEXTURE_2D);
}

void GLTextureImageBackingFactory::EnableSupportForAllMetalUsagesForTesting() {
  support_all_metal_usages_ = true;
}

std::unique_ptr<SharedImageBacking>
GLTextureImageBackingFactory::CreateSharedImageInternal(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    base::span<const uint8_t> pixel_data) {
  DCHECK(CanCreateTexture(format, size, pixel_data, GL_TEXTURE_2D));

  bool for_framebuffer_attachment = false;
  // NOTE: We are in the process of computing writes to GL without using
  // GLES2_FRAMEBUFFER_HINT as part of eliminating the latter. Here we make the
  // change guarded by a killswitch.
  // TODO(b/41491709): Remove this killswitch post safe rollout.
  if (base::FeatureList::IsEnabled(
          kCorrectFramebufferAttachmentComputationInGLTexture)) {
    // GLTextureImageBackingFactory supports raster and display usage only for
    // Ganesh-GL, meaning that raster/display write usage implies GL writes
    // within Skia.
    for_framebuffer_attachment = usage & (SHARED_IMAGE_USAGE_GLES2_WRITE |
                                          SHARED_IMAGE_USAGE_RASTER_WRITE |
                                          SHARED_IMAGE_USAGE_DISPLAY_WRITE);
  } else {
    for_framebuffer_attachment =
        (usage &
         (SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
          SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;
  }

  const bool framebuffer_attachment_angle =
      for_framebuffer_attachment && texture_usage_angle_;

  auto result = std::make_unique<GLTextureImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), use_passthrough_);
  result->InitializeGLTexture(GetFormatInfo(format), pixel_data,
                              progress_reporter_, framebuffer_attachment_angle);

  return std::move(result);
}

}  // namespace gpu
