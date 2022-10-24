// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"

#include <list>
#include <utility>

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {

namespace {

using InitializeGLTextureParams =
    GLTextureImageBackingHelper::InitializeGLTextureParams;

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// IOSurfaceImageBackingFactory

IOSurfaceImageBackingFactory::IOSurfaceImageBackingFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const gles2::FeatureInfo* feature_info,
    ImageFactory* image_factory,
    gl::ProgressReporter* progress_reporter)
    : GLCommonImageBackingFactory(gpu_preferences,
                                  workarounds,
                                  feature_info,
                                  progress_reporter),
      image_factory_(image_factory) {
  gpu_memory_buffer_formats_ =
      feature_info->feature_flags().gpu_memory_buffer_formats;
  // Return if scanout images are not supported
  if (!(image_factory_ && image_factory_->SupportsCreateAnonymousImage())) {
    return;
  }
  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    auto format = static_cast<viz::ResourceFormat>(i);
    FormatInfo& info = format_info_[i];
    BufferFormatInfo& buffer_format_info = buffer_format_info_[i];
    if (!info.enabled || !IsGpuMemoryBufferFormatSupported(format)) {
      continue;
    }
    const gfx::BufferFormat buffer_format = viz::BufferFormat(format);
    switch (buffer_format) {
      case gfx::BufferFormat::RGBA_8888:
      case gfx::BufferFormat::RGBX_8888:
      case gfx::BufferFormat::BGRA_8888:
      case gfx::BufferFormat::BGRX_8888:
      case gfx::BufferFormat::RGBA_F16:
      case gfx::BufferFormat::R_8:
      case gfx::BufferFormat::BGRA_1010102:
      case gfx::BufferFormat::RGBA_1010102:
        break;
      default:
        continue;
    }
    if (!gpu_memory_buffer_formats_.Has(buffer_format))
      continue;
    buffer_format_info.allow_scanout = true;
    buffer_format_info.buffer_format = buffer_format;
    DCHECK_EQ(info.image_internal_format,
              gl::BufferFormatToGLInternalFormat(buffer_format));
    if (base::Contains(gpu_preferences.texture_target_exception_list,
                       gfx::BufferUsageAndFormat(gfx::BufferUsage::SCANOUT,
                                                 buffer_format))) {
      buffer_format_info.target_for_scanout =
          gpu::GetPlatformSpecificTextureTarget();
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
  if (!gpu_memory_buffer_formats_.Has(buffer_format)) {
    LOG(ERROR) << "CreateSharedImage: unsupported buffer format "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  if (!gpu::IsPlaneValidForGpuMemoryBufferFormat(plane, buffer_format)) {
    LOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane) << " for "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    LOG(ERROR) << "Invalid image size " << size.ToString() << " for "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  GLenum target =
      !NativeBufferNeedsPlatformSpecificTextureTarget(buffer_format, plane)
          ? GL_TEXTURE_2D
          : gpu::GetPlatformSpecificTextureTarget();
  scoped_refptr<gl::GLImage> image =
      MakeGLImage(client_id, std::move(handle), buffer_format, color_space,
                  plane, surface_handle, size);
  if (!image) {
    LOG(ERROR) << "Failed to create image.";
    return nullptr;
  }
  // If we decide to use GL_TEXTURE_2D at the target for a native buffer, we
  // would like to verify that it will actually work. If the image expects to be
  // copied, there is no way to do this verification here, because copying is
  // done lazily after the SharedImage is created, so require that the image is
  // bindable. Currently NativeBufferNeedsPlatformSpecificTextureTarget can
  // only return false on Chrome OS where GLImageNativePixmap is used which is
  // always bindable.
#if DCHECK_IS_ON()
  bool texture_2d_support = false;
  // If the PlatformSpecificTextureTarget on Mac is GL_TEXTURE_2D, this is
  // supported.
  texture_2d_support =
      (gpu::GetPlatformSpecificTextureTarget() == GL_TEXTURE_2D);
  DCHECK(target != GL_TEXTURE_2D || texture_2d_support ||
         image->ShouldBindOrCopy() == gl::GLImage::BIND);
#endif  // DCHECK_IS_ON()

  const viz::ResourceFormat plane_format =
      viz::GetResourceFormat(GetPlaneBufferFormat(plane, buffer_format));

  const gfx::Size plane_size = gpu::GetPlaneSize(plane, size);
  DCHECK_EQ(image->GetSize(), plane_size);

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;

  InitializeGLTextureParams params;
  params.target = target;
  params.internal_format = image->GetInternalFormat();
  params.format = image->GetDataFormat();
  params.type = image->GetDataType();
  params.is_cleared = true;
  params.framebuffer_attachment_angle =
      for_framebuffer_attachment && texture_usage_angle_;

  auto si_format = viz::SharedImageFormat::SinglePlane(plane_format);
  DCHECK(use_passthrough_);
  return std::make_unique<IOSurfaceImageBacking>(
      image, mailbox, si_format, plane_size, color_space, surface_origin,
      alpha_type, usage, params);
}

scoped_refptr<gl::GLImage> IOSurfaceImageBackingFactory::MakeGLImage(
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    const gfx::ColorSpace& color_space,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size) {
  if (!image_factory_)
    return nullptr;

  return image_factory_->CreateImageForGpuMemoryBuffer(
      std::move(handle), size, format, color_space, plane, client_id,
      surface_handle);
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
  if (gmb_type == gfx::SHARED_MEMORY_BUFFER) {
    return false;
  }
  if (usage & SHARED_IMAGE_USAGE_CPU_UPLOAD) {
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
  const FormatInfo& format_info = GetFormatInfo(format);
  const BufferFormatInfo& buffer_format_info = GetBufferFormatInfo(format);
  GLenum target = buffer_format_info.target_for_scanout;

  if (!buffer_format_info.allow_scanout) {
    LOG(ERROR) << "CreateSharedImage: SCANOUT shared images unavailable. "
                  "Buffer format= "
               << gfx::BufferFormatToString(buffer_format_info.buffer_format);
    return nullptr;
  }

  if (!CanCreateSharedImage(size, pixel_data, format_info, target)) {
    return nullptr;
  }

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;

  scoped_refptr<gl::GLImage> image;

  // TODO(piman): We pretend the texture was created in an ES2 context, so that
  // it can be used in other ES2 contexts, and so we have to pass gl_format as
  // the internal format in the LevelInfo. https://crbug.com/628064
  GLuint level_info_internal_format = format_info.gl_format;
  bool is_cleared = false;

  // |scoped_progress_reporter| will notify |progress_reporter_| upon
  // construction and destruction. We limit the scope so that progress is
  // reported immediately after allocation/upload and before other GL
  // operations.
  {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    image = image_factory_->CreateAnonymousImage(
        size, buffer_format_info.buffer_format, gfx::BufferUsage::SCANOUT,
        surface_handle, &is_cleared);
  }
  // Scanout images have different constraints than GL images and might fail
  // to allocate even if GL images can be created.
  if (!image) {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    // TODO(dcastagna): Use BufferUsage::GPU_READ_WRITE instead
    // BufferUsage::GPU_READ once we add it.
    image = image_factory_->CreateAnonymousImage(
        size, buffer_format_info.buffer_format, gfx::BufferUsage::GPU_READ,
        surface_handle, &is_cleared);
  }
  // The allocated image should not require copy.
  if (!image || image->ShouldBindOrCopy() != gl::GLImage::BIND) {
    LOG(ERROR) << "CreateSharedImage: Failed to create bindable image";
    return nullptr;
  }
  level_info_internal_format = image->GetInternalFormat();
  if (color_space.IsValid())
    image->SetColorSpace(color_space);

  InitializeGLTextureParams params;
  params.target = target;
  params.internal_format = level_info_internal_format;
  params.format = format_info.gl_format;
  params.type = format_info.gl_type;
  params.is_cleared = pixel_data.empty() ? is_cleared : true;
  params.has_immutable_storage = !image && format_info.supports_storage;
  params.framebuffer_attachment_angle =
      for_framebuffer_attachment && texture_usage_angle_;

  DCHECK(!format_info.swizzle);
  DCHECK(use_passthrough_);
  auto result = std::make_unique<IOSurfaceImageBacking>(
      image, mailbox, format, size, color_space, surface_origin, alpha_type,
      usage, params);
  if (!pixel_data.empty()) {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    result->InitializePixels(format_info.adjusted_format, format_info.gl_type,
                             pixel_data.data());
  }
  return std::move(result);
}

}  // namespace gpu
