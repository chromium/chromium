// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_factory.h"

#include <list>
#include <utility>

#include "build/build_config.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {

namespace {

using ScopedRestoreTexture = GLTextureImageBackingHelper::ScopedRestoreTexture;

}  // anonymous namespace

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
    int client_id,
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

  constexpr uint32_t kInvalidUsages =
      SHARED_IMAGE_USAGE_VIDEO_DECODE | SHARED_IMAGE_USAGE_SCANOUT;
  if (usage & kInvalidUsages) {
    return false;
  }

  // Doesn't support contexts other than GL for OOPR Canvas
  if (gr_context_type != GrContextType::kGL &&
      ((usage & SHARED_IMAGE_USAGE_DISPLAY_READ) ||
       (usage & SHARED_IMAGE_USAGE_DISPLAY_WRITE) ||
       (usage & SHARED_IMAGE_USAGE_RASTER))) {
    return false;
  }

  // Linux and ChromeOS support WebGPU/Compat on GL. All other platforms
  // do not support WebGPU on GL.
  if (usage & SHARED_IMAGE_USAGE_WEBGPU) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
    if (use_webgpu_adapter_ != WebGPUAdapterName::kCompat) {
      return false;
    }
#else
    return false;
#endif
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
  GLenum target = GL_TEXTURE_2D;

  const bool is_cleared = !pixel_data.empty();
  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;
  const bool framebuffer_attachment_angle =
      for_framebuffer_attachment && texture_usage_angle_;

  auto result = std::make_unique<GLTextureImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      use_passthrough_);
  result->InitializeGLTexture(format_info, is_cleared,
                              framebuffer_attachment_angle);

  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);
  api->glBindTextureFn(target, result->GetGLServiceId());

  if (format_info.supports_storage) {
    {
      gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
      api->glTexStorage2DEXTFn(target, 1, format_info.storage_internal_format,
                               size.width(), size.height());
    }

    if (!pixel_data.empty()) {
      ScopedUnpackState scoped_unpack_state(
          /*uploading_data=*/true);
      gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
      api->glTexSubImage2DFn(target, 0, 0, 0, size.width(), size.height(),
                             format_info.adjusted_format, format_info.gl_type,
                             pixel_data.data());
    }
  } else if (format_info.is_compressed) {
    ScopedUnpackState scoped_unpack_state(!pixel_data.empty());
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    api->glCompressedTexImage2DFn(target, 0, format_info.image_internal_format,
                                  size.width(), size.height(), 0,
                                  pixel_data.size(), pixel_data.data());
  } else {
    ScopedUnpackState scoped_unpack_state(!pixel_data.empty());
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    api->glTexImage2DFn(target, 0, format_info.image_internal_format,
                        size.width(), size.height(), 0,
                        format_info.adjusted_format, format_info.gl_type,
                        pixel_data.data());
  }

  if (gl::g_current_gl_driver->ext.b_GL_KHR_debug) {
    const std::string label =
        "SharedImage_GLTexture" + CreateLabelForSharedImageUsage(usage);
    api->glObjectLabelFn(GL_TEXTURE, result->GetGLServiceId(), -1,
                         label.c_str());
  }

  result->SetCompatibilitySwizzle(format_info.swizzle);
  return std::move(result);
}

}  // namespace gpu
