// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_factory.h"

#include <list>
#include <utility>

#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageBackingFactory

GLTextureImageBackingFactory::GLTextureImageBackingFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const gles2::FeatureInfo* feature_info,
    gl::ProgressReporter* progress_reporter,
    bool for_cpu_upload_usage)
    : GLCommonImageBackingFactory(gpu_preferences,
                                  workarounds,
                                  feature_info,
                                  progress_reporter),
      for_cpu_upload_usage_(for_cpu_upload_usage) {}

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
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  return CreateSharedImageInternal(mailbox, format, surface_handle, size,
                                   color_space, surface_origin, alpha_type,
                                   usage, base::span<const uint8_t>());
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
    base::span<const uint8_t> pixel_data) {
  return CreateSharedImageInternal(mailbox, format, kNullSurfaceHandle, size,
                                   color_space, surface_origin, alpha_type,
                                   usage, pixel_data);
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
    uint32_t usage) {
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
  if (format.is_multi_plane()) {
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

  if (for_cpu_upload_usage_ != has_cpu_upload_usage)
    return false;

  if (has_cpu_upload_usage) {
    if (!GLTextureImageBacking::SupportsPixelUploadWithFormat(format))
      return false;

    // Drop scanout usage for shared memory GMBs to match legacy behaviour
    // from GLImageBackingFactory.
    usage = usage & ~SHARED_IMAGE_USAGE_SCANOUT;
  }

  constexpr uint32_t kInvalidUsages = SHARED_IMAGE_USAGE_VIDEO_DECODE |
                                      SHARED_IMAGE_USAGE_SCANOUT |
                                      SHARED_IMAGE_USAGE_WEBGPU;
  if (usage & kInvalidUsages) {
    return false;
  }

  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    constexpr uint32_t kMetalInvalidUsages =
        SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_SCANOUT |
        SHARED_IMAGE_USAGE_VIDEO_DECODE | SHARED_IMAGE_USAGE_GLES2 |
        SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT | SHARED_IMAGE_USAGE_WEBGPU;
    if (usage & kMetalInvalidUsages) {
      return false;
    }
  }

  // Doesn't support contexts other than GL for OOPR Canvas
  if (gr_context_type != GrContextType::kGL &&
      ((usage & SHARED_IMAGE_USAGE_DISPLAY_READ) ||
       (usage & SHARED_IMAGE_USAGE_DISPLAY_WRITE) ||
       (usage & SHARED_IMAGE_USAGE_RASTER))) {
    return false;
  }

  return CanCreateSharedImage(size, pixel_data, GetFormatInfo(format),
                              GL_TEXTURE_2D);
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
    base::span<const uint8_t> pixel_data) {
  const FormatInfo& format_info = GetFormatInfo(format);
  DCHECK(CanCreateSharedImage(size, pixel_data, format_info, GL_TEXTURE_2D));

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;
  const bool framebuffer_attachment_angle =
      for_framebuffer_attachment && texture_usage_angle_;

  auto result = std::make_unique<GLTextureImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      use_passthrough_);
  result->InitializeGLTexture(format_info, pixel_data, progress_reporter_,
                              framebuffer_attachment_angle);

  return std::move(result);
}

}  // namespace gpu
