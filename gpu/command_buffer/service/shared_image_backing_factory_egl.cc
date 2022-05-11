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
    const gles2::FeatureInfo* feature_info,
    SharedImageBatchAccessManager* batch_access_manager)
    : SharedImageBackingFactoryGLCommon(gpu_preferences,
                                        workarounds,
                                        feature_info,
                                        /*progress_reporter=*/nullptr),
      batch_access_manager_(batch_access_manager) {}

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
                             alpha_type, usage, base::span<const uint8_t>());
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
  return MakeEglImageBacking(mailbox, format, size, color_space, surface_origin,
                             alpha_type, usage, pixel_data);
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
  if (is_pixel_used && gr_context_type != GrContextType::kGL) {
    return false;
  }

  // Doesn't support gmb for now
  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  // Doesn't support contexts other than GL for OOPR Canvas
  if (gr_context_type != GrContextType::kGL &&
      ((usage & SHARED_IMAGE_USAGE_DISPLAY) ||
       (usage & SHARED_IMAGE_USAGE_RASTER))) {
    return false;
  }
  if ((usage & SHARED_IMAGE_USAGE_WEBGPU) ||
      (usage & SHARED_IMAGE_USAGE_VIDEO_DECODE) ||
      (usage & SHARED_IMAGE_USAGE_SCANOUT)) {
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
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  DCHECK(!(usage & SHARED_IMAGE_USAGE_SCANOUT));

  const FormatInfo& format_info = format_info_[format];
  GLenum target = GL_TEXTURE_2D;
  if (!CanCreateSharedImage(size, pixel_data, format_info, target)) {
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
      estimated_size, format_info, batch_access_manager_, workarounds_,
      attribs_, use_passthrough_, pixel_data);
}

}  // namespace gpu
