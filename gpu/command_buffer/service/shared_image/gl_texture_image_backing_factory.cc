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
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {
namespace {

constexpr SharedImageUsageSet kWebGPUUsages =
    SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE;

constexpr SharedImageUsageSet kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
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
    bool supports_cpu_upload)
    : GLCommonImageBackingFactory(kSupportedUsage,
                                  gpu_preferences,
                                  workarounds,
                                  feature_info,
                                  progress_reporter),
      supports_cpu_upload_(supports_cpu_upload),
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
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe) {
  CHECK(!is_thread_safe);
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
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    base::span<const uint8_t> pixel_data) {
  CHECK(!is_thread_safe);
  return CreateSharedImageInternal(mailbox, format, kNullSurfaceHandle, size,
                                   color_space, surface_origin, alpha_type,
                                   usage, std::move(debug_label), pixel_data);
}

bool GLTextureImageBackingFactory::IsSupported(
    SharedImageUsageSet usage,
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

  if (usage.Has(SHARED_IMAGE_USAGE_CPU_UPLOAD)) {
    if (!supports_cpu_upload_ ||
        !GLTextureImageBacking::SupportsPixelUploadWithFormat(format)) {
      return false;
    }
  }

  // This is not beneficial on iOS. The main purpose of this is a multi-gpu
  // support.
  if (!support_all_metal_usages_) {
    if ((gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
         gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) ||
        emulate_using_angle_metal_for_testing_) {
      // GLES2 usage is not allowed, as WebGL might be on a different
      // GPU than raster/composite.
      SharedImageUsageSet metal_invalid_usages =
          SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_GLES2_READ |
          SHARED_IMAGE_USAGE_GLES2_WRITE;

      if (usage.HasAny(metal_invalid_usages)) {
        return false;
      }
    }
  }

  // Using GLTextureImageBacking for raster/display is only appropriate when
  // running on top of GL. For the case WebGL fallback (GrContextType::kNone)
  // this usages aren't actually relevant but WebGL still adds them so ignore.
  if (gr_context_type != GrContextType::kGL &&
      gr_context_type != GrContextType::kNone) {
    SharedImageUsageSet unsupported_usages =
        SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
        SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE;

    if (usage.HasAny(unsupported_usages)) {
      return false;
    }
  }

  // Only supports WebGPU usages on ANGLE/GL on a Skia/GL context
  if (usage.HasAny(kWebGPUUsages)) {
#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#if BUILDFLAG(USE_WEBGPU_ON_VULKAN_VIA_GL_INTEROP)
    if (enable_webgpu_on_vk_via_gl_interop_) {
      // WebGPU usages will be handled by ExternalVkImageBackingFactory when
      // running in webgpu vk on chromium gl.
      return false;
    }
#endif
    if (gr_context_type != GrContextType::kGL ||
        gl::GetGLImplementation() != gl::kGLImplementationEGLANGLE ||
        gl::GetANGLEImplementation() != gl::ANGLEImplementation::kOpenGL) {
      return false;
    }
#else
    return false;
#endif
  }

  return CanCreateTexture(format, size, pixel_data, GL_TEXTURE_2D);
}

void GLTextureImageBackingFactory::EnableSupportForAllMetalUsagesForTesting(
    bool enable) {
  support_all_metal_usages_ = enable;
}

void GLTextureImageBackingFactory::ForceSetUsingANGLEMetalForTesting(
    bool value) {
  emulate_using_angle_metal_for_testing_ = value;
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
    SharedImageUsageSet usage,
    std::string debug_label,
    base::span<const uint8_t> pixel_data) {
  DCHECK(CanCreateTexture(format, size, pixel_data, GL_TEXTURE_2D));

  // GLTextureImageBackingFactory supports raster and display usage only for
  // Ganesh-GL, meaning that raster/display write usage implies GL writes
  // within Skia.
  const bool for_framebuffer_attachment = usage.HasAny(
      SHARED_IMAGE_USAGE_GLES2_WRITE | SHARED_IMAGE_USAGE_RASTER_WRITE |
      SHARED_IMAGE_USAGE_DISPLAY_WRITE);
  const bool framebuffer_attachment_angle =
      for_framebuffer_attachment && texture_usage_angle_;

  auto result = std::make_unique<GLTextureImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), use_passthrough_);
  result->InitializeGLTexture(GetFormatInfo(format), pixel_data,
                              progress_reporter_, framebuffer_attachment_angle);

  return std::move(result);
}

SharedImageBackingType GLTextureImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kGLTexture;
}

}  // namespace gpu
