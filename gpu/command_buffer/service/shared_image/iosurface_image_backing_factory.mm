// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "ui/gfx/mac/display_icc_profiles.h"
#endif

#import <Metal/Metal.h>

namespace gpu {

namespace {
bool UsageWillResultInGLWrite(gpu::SharedImageUsageSet usage,
                              GrContextType gr_context_type) {
  return usage.Has(SHARED_IMAGE_USAGE_GLES2_WRITE) ||
         ((gr_context_type == GrContextType::kGL) &&
          usage.HasAny(SHARED_IMAGE_USAGE_RASTER_WRITE |
                       SHARED_IMAGE_USAGE_DISPLAY_WRITE));
}

bool IsFormatSupported(viz::SharedImageFormat format) {
  return (format == viz::SinglePlaneFormat::kRGBA_8888) ||
         (format == viz::SinglePlaneFormat::kRGBX_8888) ||
         (format == viz::SinglePlaneFormat::kBGRA_8888) ||
         (format == viz::SinglePlaneFormat::kBGRX_8888) ||
         (format == viz::SinglePlaneFormat::kRGBA_F16) ||
         (format == viz::SinglePlaneFormat::kR_8) ||
         (format == viz::SinglePlaneFormat::kRG_88) ||
         (format == viz::SinglePlaneFormat::kR_16) ||
         (format == viz::SinglePlaneFormat::kR_F16) ||
         (format == viz::SinglePlaneFormat::kRG_1616) ||
         (format == viz::SinglePlaneFormat::kBGRA_1010102) ||
         (format == viz::SinglePlaneFormat::kRGBA_1010102);
}

void SetIOSurfaceColorSpace(IOSurfaceRef io_surface,
                            const gfx::ColorSpace& color_space) {
  if (!color_space.IsValid()) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  base::apple::ScopedCFTypeRef<CFDataRef> cf_data =
      gfx::DisplayICCProfiles::GetInstance()->GetDataForColorSpace(color_space);
  if (cf_data) {
    IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceColorSpace"), cf_data.get());
  } else {
    IOSurfaceSetColorSpace(io_surface, color_space);
  }
#else
  IOSurfaceSetColorSpace(io_surface, color_space);
#endif
}

bool IsValidSize(const gfx::Size& size, int32_t max_texture_size) {
  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_texture_size || size.height() > max_texture_size) {
    LOG(ERROR) << "Invalid size=" << size.ToString()
               << ", max_texture_size=" << max_texture_size;
    return false;
  }
  return true;
}

bool IsPixelDataValid(viz::SharedImageFormat format,
                      const gfx::Size& size,
                      base::span<const uint8_t> pixel_data) {
  if (pixel_data.empty()) {
    return true;
  }
  // If we have initial data to upload, ensure it is sized appropriately

  auto estimated_size = format.MaybeEstimatedSizeInBytes(size);
  if (!estimated_size) {
    LOG(ERROR) << "Failed to calculate SharedImage size";
    return false;
  }
  if (pixel_data.size() != estimated_size.value()) {
    LOG(ERROR) << "Initial data does not have expected size.";
    return false;
  }
  return true;
}

constexpr SharedImageUsageSet kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
    SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_WEBGPU_READ |
    SHARED_IMAGE_USAGE_WEBGPU_WRITE | SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE |
    SHARED_IMAGE_USAGE_VIDEO_DECODE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU |
    SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE | SHARED_IMAGE_USAGE_CPU_UPLOAD |
    SHARED_IMAGE_USAGE_RASTER_COPY_SOURCE | SHARED_IMAGE_USAGE_CPU_READ |
    SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER |
    SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR |
    SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR_WRITE |
    SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR_READ;

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// IOSurfaceImageBackingFactory

IOSurfaceImageBackingFactory::IOSurfaceImageBackingFactory(
    GrContextType gr_context_type,
    int32_t max_texture_size,
    const gles2::FeatureInfo* feature_info,
    gl::ProgressReporter* progress_reporter,
    uint32_t texture_target)
    : SharedImageBackingFactory(kSupportedUsage),
      gr_context_type_(gr_context_type),
      max_texture_size_(max_texture_size),
      angle_texture_usage_(feature_info->feature_flags().angle_texture_usage),
      progress_reporter_(progress_reporter),
      texture_target_(texture_target) {
  for (gfx::BufferFormat buffer_format :
       feature_info->feature_flags().gpu_memory_buffer_formats) {
    viz::SharedImageFormat format = viz::GetSharedImageFormat(buffer_format);
    // Add supported single-plane formats.
    if (format.is_single_plane() && IsFormatSupported(format)) {
      supported_formats_.insert(format);
    }
  }

  // Support R_F16 for SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR.
  supported_formats_.insert(viz::SinglePlaneFormat::kR_F16);

  // Add supported multi-plane formats.
  supported_formats_.insert(viz::MultiPlaneFormat::kNV12);
  supported_formats_.insert(viz::MultiPlaneFormat::kP210);
  supported_formats_.insert(viz::MultiPlaneFormat::kP410);
  supported_formats_.insert(viz::MultiPlaneFormat::kP010);
  supported_formats_.insert(viz::MultiPlaneFormat::kNV12A);
  supported_formats_.insert(viz::MultiPlaneFormat::kNV16);
  supported_formats_.insert(viz::MultiPlaneFormat::kNV24);
}

IOSurfaceImageBackingFactory::~IOSurfaceImageBackingFactory() = default;

// static
gfx::GpuMemoryBufferHandle
IOSurfaceImageBackingFactory::CreateGpuMemoryBufferHandle(
    const gfx::Size& size,
    viz::SharedImageFormat format) {
  base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface =
      gfx::CreateIOSurface(size, format, /*should_clear=*/true);
  if (!io_surface) {
    LOG(ERROR) << "Failed to allocate IOSurface.";
    return {};
  }

  return gfx::GpuMemoryBufferHandle(std::move(io_surface));
}

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImage(
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
  return CreateSharedImageInternal(mailbox, format, surface_handle, size,
                                   color_space, surface_origin, alpha_type,
                                   usage, std::move(debug_label),
                                   is_thread_safe, base::span<const uint8_t>());
}

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImage(
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
  return CreateSharedImageInternal(
      mailbox, format, kNullSurfaceHandle, size, color_space, surface_origin,
      alpha_type, usage, std::move(debug_label), is_thread_safe, pixel_data);
}

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    gfx::GpuMemoryBufferHandle handle) {
  // MacOS does not support external sampler.
  CHECK(!format.PrefersExternalSampler());
  return CreateSharedImageGMBs(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(handle), is_thread_safe);
}

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    gfx::BufferUsage buffer_usage) {
  // |scoped_progress_reporter| will notify |progress_reporter_| upon
  // construction and destruction. We limit the scope so that progress is
  // reported immediately after allocation/upload and before other GL
  // operations.
  gfx::ScopedIOSurface io_surface;
  {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    const bool should_clear = true;
    const bool override_rgba_to_bgra =
#if BUILDFLAG(IS_IOS)
        false;
#else
        gr_context_type_ == GrContextType::kGL;
#endif
    io_surface =
        gfx::CreateIOSurface(size, format, should_clear, override_rgba_to_bgra);
    if (!io_surface) {
      LOG(ERROR) << "CreateSharedImage: Failed to create bindable image";
      return nullptr;
    }
  }
  SetIOSurfaceColorSpace(io_surface.get(), color_space);

  CHECK(!format.PrefersExternalSampler());
  return CreateSharedImageGMBs(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), gfx::GpuMemoryBufferHandle(std::move(io_surface)),
      is_thread_safe, std::move(buffer_usage));
}

bool IOSurfaceImageBackingFactory::IsSupported(
    SharedImageUsageSet usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  // Only allow WebGPU shared buffer for WebNN use case for now.
  if (usage.Has(SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER) &&
      !usage.Has(SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR)) {
    return false;
  }

  // This is the only format that can be used as MLMultiArray for WebNN.
  if (usage.Has(SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR)) {
    if (format != viz::SinglePlaneFormat::kR_F16) {
      return false;
    }
  }

  if (thread_safe &&
      !base::FeatureList::IsEnabled(features::kIOSurfaceMultiThreading)) {
    return false;
  }

  // Never used with shared memory GMBs.
  if (gmb_type != gfx::EMPTY_BUFFER && gmb_type != gfx::IO_SURFACE_BUFFER) {
    return false;
  }

  if (usage.Has(SHARED_IMAGE_USAGE_CPU_WRITE_ONLY) &&
      gmb_type != gfx::IO_SURFACE_BUFFER) {
    // Only CPU writable when the client provides a IOSurface.
    return false;
  }

  // On macOS, there is no separate interop factory. Any GpuMemoryBuffer-backed
  // image can be used with both OpenGL and Metal

  // In certain modes on Mac, Angle needs the image to be released when ending a
  // write. To avoid that release resulting in the GLES2 command decoders
  // needing to perform on-demand binding, we disallow concurrent read/write in
  // these modes. See
  // IOSurfaceImageBacking::GLTextureImageRepresentationEndAccess() for further
  // details.
  // TODO(https://anglebug.com/7626): Adjust the Metal-related conditions here
  // if/as they are adjusted in
  // IOSurfaceImageBacking::GLTextureImageRepresentationEndAccess().
  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader ||
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    if (usage.Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
      return false;
    }
  }

  return true;
}

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImageInternal(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    base::span<const uint8_t> pixel_data) {
  if (!base::Contains(supported_formats_, format)) {
    LOG(ERROR) << "CreateSharedImage: Unable to create SharedImage with format "
               << format.ToString();
    return nullptr;
  }

  if (format.is_multi_plane() && !pixel_data.empty()) {
    LOG(ERROR) << "CreateSharedImage: Creation from pixel data is not "
                  "supported for multiplanar format "
               << format.ToString();
    return nullptr;
  }

  if (!IsValidSize(size, max_texture_size_) ||
      !IsPixelDataValid(format, size, pixel_data)) {
    return nullptr;
  }

  const bool for_framebuffer_attachment =
      UsageWillResultInGLWrite(usage, gr_context_type_);

  // |scoped_progress_reporter| will notify |progress_reporter_| upon
  // construction and destruction. We limit the scope so that progress is
  // reported immediately after allocation/upload and before other GL
  // operations.
  gfx::ScopedIOSurface io_surface;
  const bool should_clear =
      usage.Has(SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR) ? true : false;
  {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);

    const bool override_rgba_to_bgra =
#if BUILDFLAG(IS_IOS)
        false;
#else
        gr_context_type_ == GrContextType::kGL;
#endif
    io_surface =
        gfx::CreateIOSurface(size, format, should_clear, override_rgba_to_bgra);
    if (!io_surface) {
      LOG(ERROR) << "CreateSharedImage: Failed to create bindable image";
      return nullptr;
    }
  }
  SetIOSurfaceColorSpace(io_surface.get(), color_space);

  const bool is_cleared = !pixel_data.empty() || should_clear;
  const bool framebuffer_attachment_angle =
      for_framebuffer_attachment && angle_texture_usage_;

  auto backing = std::make_unique<IOSurfaceImageBacking>(
      io_surface, mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, std::move(debug_label), texture_target_,
      framebuffer_attachment_angle, is_cleared, is_thread_safe,
      gr_context_type_);
  if (!pixel_data.empty()) {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    backing->InitializePixels(pixel_data);
  }
  return std::move(backing);
}

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImageGMBs(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    gfx::GpuMemoryBufferHandle handle,
    bool is_thread_safe,
    std::optional<gfx::BufferUsage> buffer_usage) {
  if (handle.type != gfx::IO_SURFACE_BUFFER || !handle.io_surface()) {
    LOG(ERROR) << "Invalid IOSurface GpuMemoryBufferHandle.";
    return nullptr;
  }

  if (!base::Contains(supported_formats_, format)) {
    LOG(ERROR) << "CreateSharedImage: Unable to create SharedImage with format "
               << format.ToString();
    return nullptr;
  }

  auto io_surface = std::move(handle).io_surface();

  // Ensure that the IOSurface has the same size and pixel format as those
  // specified by `size` and `format`. A malicious client could lie about
  // this, which, if subsequently used to determine parameters for bounds
  // checking, could result in an out-of-bounds memory access.
  {
    uint32_t io_surface_format = IOSurfaceGetPixelFormat(io_surface.get());
    const bool override_rgba_to_bgra =
#if BUILDFLAG(IS_IOS)
        false;
#else
        gr_context_type_ == GrContextType::kGL;
#endif
    if (io_surface_format != SharedImageFormatToIOSurfacePixelFormat(
                                 format, override_rgba_to_bgra)) {
      LOG(ERROR) << "IOSurface pixel format does not match specified shared "
                    "image format.";
      return nullptr;
    }
    gfx::Size io_surface_size(IOSurfaceGetWidth(io_surface.get()),
                              IOSurfaceGetHeight(io_surface.get()));
    if (io_surface_size != size) {
      LOG(ERROR) << "IOSurface size does not match specified size.";
      return nullptr;
    }
  }

  const bool for_framebuffer_attachment =
      UsageWillResultInGLWrite(usage, gr_context_type_);
  const bool framebuffer_attachment_angle =
      for_framebuffer_attachment && angle_texture_usage_;

  return std::make_unique<IOSurfaceImageBacking>(
      std::move(io_surface), mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, std::move(debug_label), texture_target_,
      framebuffer_attachment_angle, /*is_cleared=*/true, is_thread_safe,
      gr_context_type_, std::move(buffer_usage));
}

SharedImageBackingType IOSurfaceImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kIOSurface;
}

}  // namespace gpu
