// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"

#include <algorithm>
#include <list>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_image.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_texture.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/progress_reporter.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/shared_gl_fence_egl.h"
#include "ui/gl/trace_util.h"

#if defined(OS_ANDROID)
#include "gpu/command_buffer/service/shared_image_backing_egl_image.h"
#include "gpu/command_buffer/service/shared_image_batch_access_manager.h"
#endif

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
    const GpuFeatureInfo& gpu_feature_info,
    ImageFactory* image_factory,
    SharedImageBatchAccessManager* batch_access_manager,
    gl::ProgressReporter* progress_reporter)
    : use_passthrough_(gpu_preferences.use_passthrough_cmd_decoder &&
                       gles2::PassthroughCommandDecoderSupported()),
      image_factory_(image_factory),
      workarounds_(workarounds),
      progress_reporter_(progress_reporter) {
#if defined(OS_ANDROID)
  batch_access_manager_ = batch_access_manager;
#endif
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_texture_size_);
  // When the passthrough command decoder is used, the max_texture_size
  // workaround is implemented by ANGLE. Trying to adjust the max size here
  // would cause discrepency between what we think the max size is and what
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
  gpu_memory_buffer_formats_ =
      feature_info->feature_flags().gpu_memory_buffer_formats;
  texture_usage_angle_ = feature_info->feature_flags().angle_texture_usage;
  attribs.es3_capable = feature_info->IsES3Capable();
  attribs.desktop_gl = !feature_info->gl_version_info().is_es;
  // Can't use the value from feature_info, as we unconditionally enable this
  // extension, and assume it can't be used if PBOs are not used (which isn't
  // true for Skia used direclty against GL).
  attribs.supports_unpack_subimage =
      gl::g_current_gl_driver->ext.b_GL_EXT_unpack_subimage;
  bool enable_texture_storage =
      feature_info->feature_flags().ext_texture_storage;
  bool enable_scanout_images =
      (image_factory_ && image_factory_->SupportsCreateAnonymousImage());
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
      info.swizzle = gles2::TextureManager::GetCompatibilitySwizzle(
          feature_info.get(), gl_format);
      info.image_internal_format =
          gles2::TextureManager::AdjustTexInternalFormat(
              feature_info.get(), image_internal_format, gl_type);
      info.adjusted_format =
          gles2::TextureManager::AdjustTexFormat(feature_info.get(), gl_format);
    }
    if (!info.enabled)
      continue;
    if (enable_texture_storage && !info.is_compressed) {
      GLuint storage_internal_format = viz::TextureStorageFormat(format);
      if (validators->texture_internal_format_storage.IsValid(
              storage_internal_format)) {
        info.supports_storage = true;
        info.storage_internal_format =
            gles2::TextureManager::AdjustTexStorageFormat(
                feature_info.get(), storage_internal_format);
      }
    }
    if (!info.enabled || !enable_scanout_images ||
        !IsGpuMemoryBufferFormatSupported(format)) {
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
    info.allow_scanout = true;
    info.buffer_format = buffer_format;
    DCHECK_EQ(info.image_internal_format,
              gl::BufferFormatToGLInternalFormat(buffer_format));
    if (base::Contains(gpu_preferences.texture_target_exception_list,
                       gfx::BufferUsageAndFormat(gfx::BufferUsage::SCANOUT,
                                                 buffer_format))) {
      info.target_for_scanout = gpu::GetPlatformSpecificTextureTarget();
    }
  }
}

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
  if (is_thread_safe) {
    return MakeEglImageBacking(mailbox, format, size, color_space,
                               surface_origin, alpha_type, usage);
  } else {
    return CreateSharedImageInternal(mailbox, format, surface_handle, size,
                                     color_space, surface_origin, alpha_type,
                                     usage, base::span<const uint8_t>());
  }
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

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    LOG(ERROR) << "Invalid image size " << size.ToString() << " for "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  GLenum target =
      (handle.type == gfx::SHARED_MEMORY_BUFFER ||
       !NativeBufferNeedsPlatformSpecificTextureTarget(buffer_format))
          ? GL_TEXTURE_2D
          : gpu::GetPlatformSpecificTextureTarget();
  scoped_refptr<gl::GLImage> image = MakeGLImage(
      client_id, std::move(handle), buffer_format, surface_handle, size);
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
#if defined(OS_MAC)
  // If the PlatformSpecificTextureTarget on Mac is GL_TEXTURE_2D, this is
  // supported.
  texture_2d_support =
      (gpu::GetPlatformSpecificTextureTarget() == GL_TEXTURE_2D);
#endif  // defined(OS_MAC)
  DCHECK(handle.type == gfx::SHARED_MEMORY_BUFFER || target != GL_TEXTURE_2D ||
         texture_2d_support || image->ShouldBindOrCopy() == gl::GLImage::BIND);
#endif  // DCHECK_IS_ON()
  if (color_space.IsValid())
    image->SetColorSpace(color_space);
  if (usage & SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX)
    image->DisableInUseByWindowServer();

  viz::ResourceFormat format = viz::GetResourceFormat(buffer_format);
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
      image, mailbox, format, size, color_space, surface_origin, alpha_type,
      usage, params, attribs, use_passthrough_);
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

scoped_refptr<gl::GLImage> SharedImageBackingFactoryGLTexture::MakeGLImage(
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size) {
  if (handle.type == gfx::SHARED_MEMORY_BUFFER) {
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
      std::move(handle), size, format, client_id, surface_handle);
}

bool SharedImageBackingFactoryGLTexture::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  // SharedImageFactory may call CanImportGpuMemoryBuffer() in all other
  // SharedImageBackingFactory implementations except this one.
  NOTREACHED();
  return true;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::MakeEglImageBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
#if defined(OS_ANDROID)
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
      batch_access_manager_, workarounds_);
#else
  return nullptr;
#endif
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
  if (!format_info.enabled) {
    LOG(ERROR) << "CreateSharedImage: invalid format";
    return nullptr;
  }

#if defined(OS_MAC)
  const bool use_buffer =
      usage & (SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_WEBGPU);
#else
  const bool use_buffer = usage & SHARED_IMAGE_USAGE_SCANOUT;
#endif
  if (use_buffer && !format_info.allow_scanout) {
    LOG(ERROR) << "CreateSharedImage: SCANOUT shared images unavailable. "
                  "Buffer format= "
               << gfx::BufferFormatToString(format_info.buffer_format);
    return nullptr;
  }

  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_texture_size_ || size.height() > max_texture_size_) {
    LOG(ERROR) << "CreateSharedImage: invalid size";
    return nullptr;
  }

  GLenum target = use_buffer ? format_info.target_for_scanout : GL_TEXTURE_2D;

  // If we have initial data to upload, ensure it is sized appropriately.
  if (!pixel_data.empty()) {
    if (format_info.is_compressed) {
      const char* error_message = "unspecified";
      if (!gles2::ValidateCompressedTexDimensions(
              target, 0 /* level */, size.width(), size.height(), 1 /* depth */,
              format_info.image_internal_format, &error_message)) {
        LOG(ERROR) << "CreateSharedImage: "
                      "ValidateCompressedTexDimensionsFailed with error: "
                   << error_message;
        return nullptr;
      }

      GLsizei bytes_required = 0;
      if (!gles2::GetCompressedTexSizeInBytes(
              nullptr /* function_name */, size.width(), size.height(),
              1 /* depth */, format_info.image_internal_format, &bytes_required,
              nullptr /* error_state */)) {
        LOG(ERROR) << "CreateSharedImage: Unable to compute required size for "
                      "initial texture upload.";
        return nullptr;
      }

      if (bytes_required < 0 ||
          pixel_data.size() != static_cast<size_t>(bytes_required)) {
        LOG(ERROR) << "CreateSharedImage: Initial data does not have expected "
                      "size.";
        return nullptr;
      }
    } else {
      uint32_t bytes_required;
      uint32_t unpadded_row_size = 0u;
      uint32_t padded_row_size = 0u;
      if (!gles2::GLES2Util::ComputeImageDataSizes(
              size.width(), size.height(), 1 /* depth */, format_info.gl_format,
              format_info.gl_type, 4 /* alignment */, &bytes_required,
              &unpadded_row_size, &padded_row_size)) {
        LOG(ERROR) << "CreateSharedImage: Unable to compute required size for "
                      "initial texture upload.";
        return nullptr;
      }

      // The GL spec, used in the computation for required bytes in the function
      // above, assumes no padding is required for the last row in the image.
      // But the client data does include this padding, so we add it for the
      // data validation check here.
      uint32_t padding = padded_row_size - unpadded_row_size;
      bytes_required += padding;
      if (pixel_data.size() != bytes_required) {
        LOG(ERROR) << "CreateSharedImage: Initial data does not have expected "
                      "size.";
        return nullptr;
      }
    }
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
  if (use_buffer) {
    {
      gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
      image = image_factory_->CreateAnonymousImage(
          size, format_info.buffer_format, gfx::BufferUsage::SCANOUT,
          surface_handle, &is_cleared);
    }
    // Scanout images have different constraints than GL images and might fail
    // to allocate even if GL images can be created.
    if (!image) {
      gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
      // TODO(dcastagna): Use BufferUsage::GPU_READ_WRITE instead
      // BufferUsage::GPU_READ once we add it.
      image = image_factory_->CreateAnonymousImage(
          size, format_info.buffer_format, gfx::BufferUsage::GPU_READ,
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
  }

  InitializeGLTextureParams params;
  params.target = target;
  params.internal_format = level_info_internal_format;
  params.format = format_info.gl_format;
  params.type = format_info.gl_type;
  params.is_cleared = pixel_data.empty() ? is_cleared : true;
  params.has_immutable_storage = !image && format_info.supports_storage;
  params.framebuffer_attachment_angle =
      for_framebuffer_attachment && texture_usage_angle_;

  if (image) {
    DCHECK(!format_info.swizzle);
    auto result = std::make_unique<SharedImageBackingGLImage>(
        image, mailbox, format, size, color_space, surface_origin, alpha_type,
        usage, params, attribs, use_passthrough_);
    if (!pixel_data.empty()) {
      gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
      result->InitializePixels(format_info.adjusted_format, format_info.gl_type,
                               pixel_data.data());
    }
    return std::move(result);
  } else {
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
            api, attribs, true /* uploading_data */);
        gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
        api->glTexSubImage2DFn(target, 0, 0, 0, size.width(), size.height(),
                               format_info.adjusted_format, format_info.gl_type,
                               pixel_data.data());
      }
    } else if (format_info.is_compressed) {
      ScopedResetAndRestoreUnpackState scoped_unpack_state(api, attribs,
                                                           !pixel_data.empty());
      gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
      api->glCompressedTexImage2DFn(
          target, 0, format_info.image_internal_format, size.width(),
          size.height(), 0, pixel_data.size(), pixel_data.data());
    } else {
      ScopedResetAndRestoreUnpackState scoped_unpack_state(api, attribs,
                                                           !pixel_data.empty());
      gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
      api->glTexImage2DFn(target, 0, format_info.image_internal_format,
                          size.width(), size.height(), 0,
                          format_info.adjusted_format, format_info.gl_type,
                          pixel_data.data());
    }
    result->SetCompatibilitySwizzle(format_info.swizzle);
    return std::move(result);
  }
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingFactoryGLTexture::FormatInfo

SharedImageBackingFactoryGLTexture::FormatInfo::FormatInfo() = default;
SharedImageBackingFactoryGLTexture::FormatInfo::~FormatInfo() = default;

}  // namespace gpu
