// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"

#if BUILDFLAG(IS_MAC)
#include "ui/gfx/mac/display_icc_profiles.h"
#endif

#import <Metal/Metal.h>

namespace gpu {

namespace {
bool IsFormatSupported(viz::ResourceFormat resource_format) {
  switch (resource_format) {
    case viz::ResourceFormat::RGBA_8888:
    case viz::ResourceFormat::RGBX_8888:
    case viz::ResourceFormat::BGRA_8888:
    case viz::ResourceFormat::BGRX_8888:
    case viz::ResourceFormat::RGBA_F16:
    case viz::ResourceFormat::RED_8:
    case viz::ResourceFormat::RG_88:
    case viz::ResourceFormat::BGRA_1010102:
    case viz::ResourceFormat::RGBA_1010102:
      return true;
    default:
      return false;
  }
}

void SetIOSurfaceColorSpace(IOSurfaceRef io_surface,
                            const gfx::ColorSpace& color_space) {
  if (!color_space.IsValid()) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  base::ScopedCFTypeRef<CFDataRef> cf_data =
      gfx::DisplayICCProfiles::GetInstance()->GetDataForColorSpace(color_space);
  if (cf_data) {
    IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceColorSpace"), cf_data);
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
  size_t estimated_size;
  if (!viz::ResourceSizes::MaybeSizeInBytes(size, format.resource_format(),
                                            &estimated_size)) {
    DLOG(ERROR) << "Failed to calculate SharedImage size";
    return false;
  }
  if (pixel_data.size() != estimated_size) {
    LOG(ERROR) << "Initial data does not have expected size.";
    return false;
  }
  return true;
}

constexpr uint32_t kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
    SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_WEBGPU |
    SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE | SHARED_IMAGE_USAGE_VIDEO_DECODE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX |
    SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU | SHARED_IMAGE_USAGE_CPU_WRITE |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE;

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// IOSurfaceImageBackingFactory

IOSurfaceImageBackingFactory::IOSurfaceImageBackingFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const gles2::FeatureInfo* feature_info,
    gl::ProgressReporter* progress_reporter)
    : SharedImageBackingFactory(kSupportedUsage),
      progress_reporter_(progress_reporter),
      gpu_memory_buffer_formats_(
          feature_info->feature_flags().gpu_memory_buffer_formats),
      angle_texture_usage_(feature_info->feature_flags().angle_texture_usage) {
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_texture_size_);
  // Ensure max_texture_size_ is less than INT_MAX so that gfx::Rect and friends
  // can be used to accurately represent all valid sub-rects, with overflow
  // cases, clamped to INT_MAX, always invalid.
  max_texture_size_ = std::min(max_texture_size_, INT_MAX - 1);

  for (gfx::BufferFormat buffer_format : gpu_memory_buffer_formats_) {
    viz::ResourceFormat resource_format = viz::GetResourceFormat(buffer_format);
    if (IsFormatSupported(resource_format)) {
      supported_formats_.insert(resource_format);
    }
  }
}

IOSurfaceImageBackingFactory::~IOSurfaceImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImage(
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
IOSurfaceImageBackingFactory::CreateSharedImage(
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
IOSurfaceImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    gfx::GpuMemoryBufferHandle handle) {
  return CreateSharedImageGMBs(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(handle), /*io_surface_plane=*/0, gfx::BufferPlane::DEFAULT,
      /*is_plane_format=*/false);
}

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  if (!gpu::IsPlaneValidForGpuMemoryBufferFormat(plane, buffer_format)) {
    LOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane) << " for "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  const uint32_t io_surface_plane = GetPlaneIndex(plane, buffer_format);
  auto format = viz::SharedImageFormat::SinglePlane(
      viz::GetResourceFormat(buffer_format));
  return CreateSharedImageGMBs(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(handle), io_surface_plane, plane, /*is_plane_format=*/true);
}

bool IOSurfaceImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (!pixel_data.empty() && gr_context_type != GrContextType::kGL) {
    return false;
  }
  if (thread_safe) {
    return false;
  }

  // Never used with shared memory GMBs.
  if (gmb_type != gfx::EMPTY_BUFFER && gmb_type != gfx::IO_SURFACE_BUFFER) {
    return false;
  }

  if (usage & SHARED_IMAGE_USAGE_CPU_WRITE &&
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
    if (usage & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE) {
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
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  if (!base::Contains(supported_formats_, format.resource_format())) {
    LOG(ERROR) << "CreateSharedImage: SCANOUT shared images unavailable. "
                  "Format= "
               << format.ToString();
    return nullptr;
  }
  if (!IsValidSize(size, max_texture_size_) ||
      !IsPixelDataValid(format, size, pixel_data)) {
    return nullptr;
  }

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;

  // |scoped_progress_reporter| will notify |progress_reporter_| upon
  // construction and destruction. We limit the scope so that progress is
  // reported immediately after allocation/upload and before other GL
  // operations.
  gfx::ScopedIOSurface io_surface;
  const uint32_t io_surface_plane = 0;
  const gfx::GenericSharedMemoryId io_surface_id;
  const gfx::BufferFormat buffer_format = ToBufferFormat(format);
  {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    const bool should_clear = false;
    io_surface.reset(gfx::CreateIOSurface(size, buffer_format, should_clear));
    if (!io_surface) {
      LOG(ERROR) << "CreateSharedImage: Failed to create bindable image";
      return nullptr;
    }
  }
  SetIOSurfaceColorSpace(io_surface.get(), color_space);

  const bool is_cleared = !pixel_data.empty();
  const bool framebuffer_attachment_angle =
      for_framebuffer_attachment && angle_texture_usage_;
  GLenum texture_target = gpu::GetPlatformSpecificTextureTarget();

  auto backing = std::make_unique<IOSurfaceImageBacking>(
      io_surface, io_surface_plane, io_surface_id, mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, texture_target,
      framebuffer_attachment_angle, is_cleared);
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
    uint32_t usage,
    gfx::GpuMemoryBufferHandle handle,
    uint32_t io_surface_plane,
    gfx::BufferPlane buffer_plane,
    bool is_plane_format) {
  if (handle.type != gfx::IO_SURFACE_BUFFER || !handle.io_surface) {
    LOG(ERROR) << "Invalid IOSurface GpuMemoryBufferHandle.";
    return nullptr;
  }

  auto buffer_format = ToBufferFormat(format);
  if (!gpu_memory_buffer_formats_.Has(buffer_format)) {
    LOG(ERROR) << "CreateSharedImage: unsupported buffer format "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  // Note that `size` refers to the size of the IOSurface.
  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    LOG(ERROR) << "Invalid size " << size.ToString() << " for "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  const GLenum target = gpu::GetPlatformSpecificTextureTarget();
  auto io_surface = handle.io_surface;
  const auto io_surface_id = handle.id;

  // Ensure that the IOSurface has the same size and pixel format as those
  // specified by `size` and `buffer_format`. A malicious client could lie about
  // this, which, if subsequently used to determine parameters for bounds
  // checking, could result in an out-of-bounds memory access.
  {
    uint32_t io_surface_format = IOSurfaceGetPixelFormat(io_surface);
    if (io_surface_format !=
        BufferFormatToIOSurfacePixelFormat(buffer_format)) {
      DLOG(ERROR)
          << "IOSurface pixel format does not match specified buffer format.";
      return nullptr;
    }
    gfx::Size io_surface_size(IOSurfaceGetWidth(io_surface),
                              IOSurfaceGetHeight(io_surface));
    if (io_surface_size != size) {
      DLOG(ERROR) << "IOSurface size does not match specified size.";
      return nullptr;
    }
  }

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;
  const bool framebuffer_attachment_angle =
      for_framebuffer_attachment && angle_texture_usage_;

  if (is_plane_format) {
    const gfx::Size plane_size = gpu::GetPlaneSize(buffer_plane, size);
    auto plane_format =
        viz::SharedImageFormat::SinglePlane(viz::GetResourceFormat(
            GetPlaneBufferFormat(buffer_plane, buffer_format)));
    return std::make_unique<IOSurfaceImageBacking>(
        io_surface, io_surface_plane, io_surface_id, mailbox, plane_format,
        plane_size, color_space, surface_origin, alpha_type, usage, target,
        framebuffer_attachment_angle, /*is_cleared=*/true);
  }

  return std::make_unique<IOSurfaceImageBacking>(
      io_surface, /*io_surface_plane=*/0, io_surface_id, mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, target,
      framebuffer_attachment_angle, /*is_cleared=*/true);
}

}  // namespace gpu
