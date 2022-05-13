// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_gl_image.h"

#include <list>
#include <utility>

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_image.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {

namespace {

using InitializeGLTextureParams =
    SharedImageBackingGLCommon::InitializeGLTextureParams;

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingFactoryGLImage

SharedImageBackingFactoryGLImage::SharedImageBackingFactoryGLImage(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const gles2::FeatureInfo* feature_info,
    ImageFactory* image_factory,
    gl::ProgressReporter* progress_reporter,
    const bool for_shared_memory_gmbs)
    : SharedImageBackingFactoryGLCommon(gpu_preferences,
                                        workarounds,
                                        feature_info,
                                        progress_reporter),
      image_factory_(image_factory),
      for_shared_memory_gmbs_(for_shared_memory_gmbs) {
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
      case gfx::BufferFormat::BGRA_8888:
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

SharedImageBackingFactoryGLImage::~SharedImageBackingFactoryGLImage() = default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLImage::CreateSharedImage(
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
SharedImageBackingFactoryGLImage::CreateSharedImage(
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
SharedImageBackingFactoryGLImage::CreateSharedImage(
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

  const gfx::GpuMemoryBufferType handle_type = handle.type;
  GLenum target =
      (handle_type == gfx::SHARED_MEMORY_BUFFER ||
       !NativeBufferNeedsPlatformSpecificTextureTarget(buffer_format, plane))
          ? GL_TEXTURE_2D
          : gpu::GetPlatformSpecificTextureTarget();
  scoped_refptr<gl::GLImage> image = MakeGLImage(
      client_id, std::move(handle), buffer_format, plane, surface_handle, size);
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
#if BUILDFLAG(IS_MAC)
  // If the PlatformSpecificTextureTarget on Mac is GL_TEXTURE_2D, this is
  // supported.
  texture_2d_support =
      (gpu::GetPlatformSpecificTextureTarget() == GL_TEXTURE_2D);
#endif  // BUILDFLAG(IS_MAC)
  DCHECK(handle_type == gfx::SHARED_MEMORY_BUFFER || target != GL_TEXTURE_2D ||
         texture_2d_support || image->ShouldBindOrCopy() == gl::GLImage::BIND);
#endif  // DCHECK_IS_ON()
  if (color_space.IsValid())
    image->SetColorSpace(color_space);
  if (usage & SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX)
    image->DisableInUseByWindowServer();

  const viz::ResourceFormat plane_format =
      viz::GetResourceFormat(GetPlaneBufferFormat(plane, buffer_format));

  const gfx::Size plane_size = gpu::GetPlaneSize(plane, size);
  DCHECK_EQ(image->GetSize(), plane_size);

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;
  const bool is_rgb_emulation = (usage & SHARED_IMAGE_USAGE_RGB_EMULATION) != 0;

  InitializeGLTextureParams params;
  params.target = target;
  params.internal_format =
      is_rgb_emulation ? GL_RGB : image->GetInternalFormat();
  params.format = is_rgb_emulation ? GL_RGB : image->GetDataFormat();
  params.type = image->GetDataType();
  params.is_cleared = true;
  params.is_rgb_emulation = is_rgb_emulation;
  params.framebuffer_attachment_angle =
      for_framebuffer_attachment && texture_usage_angle_;
  return std::make_unique<SharedImageBackingGLImage>(
      image, mailbox, plane_format, plane_size, color_space, surface_origin,
      alpha_type, usage, params, attribs_, use_passthrough_);
}

scoped_refptr<gl::GLImage> SharedImageBackingFactoryGLImage::MakeGLImage(
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size) {
  if (handle.type == gfx::SHARED_MEMORY_BUFFER) {
    if (plane != gfx::BufferPlane::DEFAULT)
      return nullptr;
    if (!base::IsValueInRangeForNumericType<size_t>(handle.stride))
      return nullptr;
    auto image = base::MakeRefCounted<gl::GLImageSharedMemory>(size);
    if (!image->Initialize(handle.region, handle.id, format, handle.offset,
                           handle.stride)) {
      return nullptr;
    }

    return image;
  }

  if (!image_factory_)
    return nullptr;

  return image_factory_->CreateImageForGpuMemoryBuffer(
      std::move(handle), size, format, plane, client_id, surface_handle);
}

bool SharedImageBackingFactoryGLImage::IsSupported(
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
  // If the GLImage factory is created specifically for SHARED_MEMORY Gmbs,
  // make sure that it used for that purpose based on flag
  if ((for_shared_memory_gmbs_ && gmb_type != gfx::SHARED_MEMORY_BUFFER) ||
      (!for_shared_memory_gmbs_ && gmb_type == gfx::SHARED_MEMORY_BUFFER)) {
    return false;
  }
#if BUILDFLAG(IS_MAC)
  // On macOS, there is no separate interop factory. Any GpuMemoryBuffer-backed
  // image can be used with both OpenGL and Metal
  *allow_legacy_mailbox = gr_context_type == GrContextType::kGL;
  return true;
#else
  // Doesn't support contexts other than GL for OOPR Canvas
  if (gr_context_type != GrContextType::kGL &&
      ((usage & SHARED_IMAGE_USAGE_DISPLAY) ||
       (usage & SHARED_IMAGE_USAGE_RASTER))) {
    return false;
  }
  if ((usage & SHARED_IMAGE_USAGE_WEBGPU) ||
      (usage & SHARED_IMAGE_USAGE_VIDEO_DECODE)) {
    // return false if it needs interop factory
    return false;
  }
  *allow_legacy_mailbox = gr_context_type == GrContextType::kGL;
  return true;
#endif
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLImage::CreateSharedImageInternal(
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
  const BufferFormatInfo& buffer_format_info = buffer_format_info_[format];
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
  if (usage & SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX)
    image->DisableInUseByWindowServer();

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
  auto result = std::make_unique<SharedImageBackingGLImage>(
      image, mailbox, format, size, color_space, surface_origin, alpha_type,
      usage, params, attribs_, use_passthrough_);
  if (!pixel_data.empty()) {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    result->InitializePixels(format_info.adjusted_format, format_info.gl_type,
                             pixel_data.data());
  }
  return std::move(result);
}

}  // namespace gpu
