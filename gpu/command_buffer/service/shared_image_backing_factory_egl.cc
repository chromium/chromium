// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_egl.h"

#include <algorithm>

#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_backing_egl_image.h"
#include "gpu/command_buffer/service/shared_image_batch_access_manager.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/shared_gl_fence_egl.h"

namespace gpu {

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingFactoryEGL

SharedImageBackingFactoryEGL::SharedImageBackingFactoryEGL(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    SharedImageBatchAccessManager* batch_access_manager)
    : use_passthrough_(gpu_preferences.use_passthrough_cmd_decoder &&
                       gles2::PassthroughCommandDecoderSupported()),
      workarounds_(workarounds) {
  batch_access_manager_ = batch_access_manager;
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_texture_size_);
  // When the passthrough command decoder is used, the max_texture_size
  // workaround is implemented by ANGLE. Trying to adjust the max size here
  // would cause discrepancy between what we think the max size is and what
  // ANGLE tells the clients.
  if (!use_passthrough_ && workarounds.max_texture_size) {
    max_texture_size_ =
        std::min(max_texture_size_, workarounds.max_texture_size);
  }
  // Ensure max_texture_size_ is less than INT_MAX so that gfx::Rect and friends
  // can be used to accurately represent all valid sub-rects, with overflow
  // cases, clamped to INT_MAX, always invalid.
  max_texture_size_ = std::min(max_texture_size_, INT_MAX - 1);

  // TODO(piman): Can we extract the logic out of FeatureInfo?
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(workarounds, gpu_feature_info);
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                           use_passthrough_, gles2::DisallowedFeatures());
  const gles2::Validators* validators = feature_info->validators();
  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    auto format = static_cast<viz::ResourceFormat>(i);
    FormatInfo& info = format_info_[i];
    if (!viz::GLSupportsFormat(format))
      continue;
    const GLuint image_internal_format = viz::GLInternalFormat(format);
    const GLenum gl_format = viz::GLDataFormat(format);
    const GLenum gl_type = viz::GLDataType(format);
    const bool uncompressed_format_valid =
        validators->texture_internal_format.IsValid(image_internal_format) &&
        validators->texture_format.IsValid(gl_format);
    const bool compressed_format_valid =
        validators->compressed_texture_format.IsValid(image_internal_format);
    if ((uncompressed_format_valid || compressed_format_valid) &&
        validators->pixel_type.IsValid(gl_type)) {
      info.enabled = true;
      info.is_compressed = compressed_format_valid;
      info.gl_format = gl_format;
      info.gl_type = gl_type;
    }
  }
}

SharedImageBackingFactoryEGL::~SharedImageBackingFactoryEGL() = default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryEGL::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  return MakeEglImageBacking(mailbox, format, size, color_space, surface_origin,
                             alpha_type, usage);
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryEGL::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryEGL::CreateSharedImage(
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

bool SharedImageBackingFactoryEGL::IsSupported(
    uint32_t usage,
    viz::ResourceFormat format,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    bool* allow_legacy_mailbox,
    bool is_pixel_used) {
  if (is_pixel_used) {
    return false;
  }
  // Doesn't support gmb for now
  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }
  bool needs_interop_factory = (gr_context_type == GrContextType::kVulkan &&
                                (usage & SHARED_IMAGE_USAGE_DISPLAY)) ||
                               (usage & SHARED_IMAGE_USAGE_WEBGPU) ||
                               (usage & SHARED_IMAGE_USAGE_VIDEO_DECODE) ||
                               (usage & SHARED_IMAGE_USAGE_SCANOUT);
  if (needs_interop_factory) {
    // return false if it needs interop factory
    return false;
  }
  *allow_legacy_mailbox = false;
  return true;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryEGL::MakeEglImageBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  const FormatInfo& format_info = format_info_[format];
  if (!format_info.enabled) {
    DLOG(ERROR) << "MakeEglImageBacking: invalid format";
    return nullptr;
  }

  DCHECK(!(usage & SHARED_IMAGE_USAGE_SCANOUT));

  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_texture_size_ || size.height() > max_texture_size_) {
    DLOG(ERROR) << "MakeEglImageBacking: Invalid size";
    return nullptr;
  }

  // Calculate SharedImage size in bytes.
  size_t estimated_size;
  if (!viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size)) {
    DLOG(ERROR) << "MakeEglImageBacking: Failed to calculate SharedImage size";
    return nullptr;
  }

  return std::make_unique<SharedImageBackingEglImage>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      estimated_size, format_info.gl_format, format_info.gl_type,
      batch_access_manager_, workarounds_, use_passthrough_);
}

}  // namespace gpu
