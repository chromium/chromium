// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/egl_image_backing_factory.h"

#include <algorithm>

#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image/egl_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/shared_gl_fence_egl.h"

namespace gpu {
namespace {

constexpr SharedImageUsageSet kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
    SHARED_IMAGE_USAGE_GLES2_FOR_RASTER_ONLY |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
    SHARED_IMAGE_USAGE_RASTER_OVER_GLES2_ONLY |
    SHARED_IMAGE_USAGE_OOP_RASTERIZATION | SHARED_IMAGE_USAGE_WEBGPU_READ |
    SHARED_IMAGE_USAGE_WEBGPU_WRITE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// EGLImageBackingFactory

EGLImageBackingFactory::EGLImageBackingFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const gles2::FeatureInfo* feature_info)
    : GLCommonImageBackingFactory(kSupportedUsage,
                                  gpu_preferences,
                                  workarounds,
                                  feature_info,
                                  /*progress_reporter=*/nullptr) {}

EGLImageBackingFactory::~EGLImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking> EGLImageBackingFactory::CreateSharedImage(
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
  return MakeEglImageBacking(mailbox, format, size, color_space, surface_origin,
                             alpha_type, usage, std::move(debug_label),
                             base::span<const uint8_t>());
}

std::unique_ptr<SharedImageBacking> EGLImageBackingFactory::CreateSharedImage(
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
  return MakeEglImageBacking(mailbox, format, size, color_space, surface_origin,
                             alpha_type, usage, std::move(debug_label),
                             pixel_data);
}

std::unique_ptr<SharedImageBacking> EGLImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    gfx::GpuMemoryBufferHandle handle) {
  NOTREACHED();
}

bool EGLImageBackingFactory::IsSupported(SharedImageUsageSet usage,
                                         viz::SharedImageFormat format,
                                         const gfx::Size& size,
                                         bool thread_safe,
                                         gfx::GpuMemoryBufferType gmb_type,
                                         GrContextType gr_context_type,
                                         base::span<const uint8_t> pixel_data) {
  if (!pixel_data.empty() && gr_context_type != GrContextType::kGL) {
    return false;
  }

  // Doesn't support gmb for now
  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  // Doesn't support contexts other than GL for OOPR Canvas
  if (gr_context_type != GrContextType::kGL &&
      usage.HasAny(
          SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
          SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE)) {
    return false;
  }
  constexpr SharedImageUsageSet kInvalidUsage =
      SHARED_IMAGE_USAGE_VIDEO_DECODE | SHARED_IMAGE_USAGE_SCANOUT |
      SHARED_IMAGE_USAGE_CPU_UPLOAD;
  if (usage.HasAny(kInvalidUsage)) {
    return false;
  }

  if ((usage.HasAny(SHARED_IMAGE_USAGE_WEBGPU_READ |
                    SHARED_IMAGE_USAGE_WEBGPU_WRITE)) &&
      (use_webgpu_adapter_ != WebGPUAdapterName::kOpenGLES)) {
    return false;
  }

  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    constexpr SharedImageUsageSet kMetalInvalidUsages =
        SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_SCANOUT |
        SHARED_IMAGE_USAGE_VIDEO_DECODE | SHARED_IMAGE_USAGE_GLES2_READ |
        SHARED_IMAGE_USAGE_GLES2_WRITE | SHARED_IMAGE_USAGE_WEBGPU_READ |
        SHARED_IMAGE_USAGE_WEBGPU_WRITE;
    if (usage.HasAny(kMetalInvalidUsages)) {
      return false;
    }
  }

  return CanCreateTexture(format, size, pixel_data, GL_TEXTURE_2D);
}

std::unique_ptr<SharedImageBacking> EGLImageBackingFactory::MakeEglImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    base::span<const uint8_t> pixel_data) {
  DCHECK(!usage.Has(SHARED_IMAGE_USAGE_SCANOUT));

  // Calculate SharedImage size in bytes.
  auto estimated_size = format.MaybeEstimatedSizeInBytes(size);
  if (!estimated_size) {
    DLOG(ERROR) << "MakeEglImageBacking: Failed to calculate SharedImage size";
    return nullptr;
  }

  // EGLImageBacking can support single-planar and multi-planar textures.
  auto format_info = GetFormatInfo(format);
  CHECK_EQ(static_cast<int>(format_info.size()), format.NumberOfPlanes());

  return std::make_unique<EGLImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), estimated_size.value(), format_info, workarounds_,
      use_passthrough_, pixel_data);
}

SharedImageBackingType EGLImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kEGLImage;
}

}  // namespace gpu
