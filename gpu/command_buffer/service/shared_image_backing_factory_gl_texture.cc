// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"

#include <list>
#include <utility>

#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_texture.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {

namespace {

using ScopedResetAndRestoreUnpackState =
    SharedImageBackingGLCommon::ScopedResetAndRestoreUnpackState;

using ScopedRestoreTexture = SharedImageBackingGLCommon::ScopedRestoreTexture;

using InitializeGLTextureParams =
    SharedImageBackingGLCommon::InitializeGLTextureParams;

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingFactoryGLTexture

SharedImageBackingFactoryGLTexture::SharedImageBackingFactoryGLTexture(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const gles2::FeatureInfo* feature_info,
    gl::ProgressReporter* progress_reporter)
    : SharedImageBackingFactoryGLCommon(gpu_preferences,
                                        workarounds,
                                        feature_info,
                                        progress_reporter) {}

SharedImageBackingFactoryGLTexture::~SharedImageBackingFactoryGLTexture() =
    default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
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
SharedImageBackingFactoryGLTexture::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
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
SharedImageBackingFactoryGLTexture::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImageForTest(
    const Mailbox& mailbox,
    GLenum target,
    GLuint service_id,
    bool is_cleared,
    viz::ResourceFormat format,
    const gfx::Size& size,
    uint32_t usage) {
  auto result = std::make_unique<SharedImageBackingGLTexture>(
      mailbox, format, size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, usage, false /* is_passthrough */);
  InitializeGLTextureParams params;
  params.target = target;
  params.internal_format = viz::GLInternalFormat(format);
  params.format = viz::GLDataFormat(format);
  params.type = viz::GLDataType(format);
  params.is_cleared = is_cleared;
  result->InitializeGLTexture(service_id, params);
  return std::move(result);
}

bool SharedImageBackingFactoryGLTexture::IsSupported(
    uint32_t usage,
    viz::ResourceFormat format,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    bool* allow_legacy_mailbox,
    bool is_pixel_used) {
  if (is_pixel_used && gr_context_type != GrContextType::kGL) {
    return false;
  }
  if (thread_safe) {
    return false;
  }
  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  // Doesn't support contexts other than GL for OOPR Canvas
  if (gr_context_type != GrContextType::kGL &&
      ((usage & SHARED_IMAGE_USAGE_DISPLAY) ||
       (usage & SHARED_IMAGE_USAGE_RASTER))) {
    return false;
  }

  // Linux and ChromeOS support WebGPU/Compat on GL. All other platforms
  // do not support WebGPU on GL.
  if (usage & SHARED_IMAGE_USAGE_WEBGPU) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || defined(USE_OZONE)
    if (use_webgpu_adapter_ != WebGPUAdapterName::kCompat) {
      return false;
    }
#else
    return false;
#endif
  }

  // Needs interop factory
  if ((usage & SHARED_IMAGE_USAGE_VIDEO_DECODE) ||
      (usage & SHARED_IMAGE_USAGE_SCANOUT)) {
    return false;
  }

  *allow_legacy_mailbox = gr_context_type == GrContextType::kGL;
  return true;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImageInternal(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  const FormatInfo& format_info = format_info_[format];
  GLenum target = GL_TEXTURE_2D;
  if (!CanCreateSharedImage(size, pixel_data, format_info, target)) {
    return nullptr;
  }

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;

  InitializeGLTextureParams params;
  params.target = target;
  // TODO(piman): We pretend the texture was created in an ES2 context, so that
  // it can be used in other ES2 contexts, and so we have to pass gl_format as
  // the internal format in the LevelInfo. https://crbug.com/628064
  params.internal_format = format_info.gl_format;
  params.format = format_info.gl_format;
  params.type = format_info.gl_type;
  params.is_cleared = !pixel_data.empty();
  params.has_immutable_storage = format_info.supports_storage;
  params.framebuffer_attachment_angle =
      for_framebuffer_attachment && texture_usage_angle_;

  auto result = std::make_unique<SharedImageBackingGLTexture>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      use_passthrough_);
  result->InitializeGLTexture(0, params);

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
      ScopedResetAndRestoreUnpackState scoped_unpack_state(
          api, attribs_, true /* uploading_data */);
      gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
      api->glTexSubImage2DFn(target, 0, 0, 0, size.width(), size.height(),
                             format_info.adjusted_format, format_info.gl_type,
                             pixel_data.data());
    }
  } else if (format_info.is_compressed) {
    ScopedResetAndRestoreUnpackState scoped_unpack_state(api, attribs_,
                                                         !pixel_data.empty());
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    api->glCompressedTexImage2DFn(target, 0, format_info.image_internal_format,
                                  size.width(), size.height(), 0,
                                  pixel_data.size(), pixel_data.data());
  } else {
    ScopedResetAndRestoreUnpackState scoped_unpack_state(api, attribs_,
                                                         !pixel_data.empty());
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
