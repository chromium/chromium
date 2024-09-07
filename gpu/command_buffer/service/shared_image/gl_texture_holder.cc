// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_holder.h"

#include "base/bits.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_repack_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_gl_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/gpu/ganesh/GrContextThreadSafeProxy.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/progress_reporter.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_restore_texture.h"

namespace gpu {
namespace {

// This value can't be cached as it may change for different contexts.
bool SupportsUnpackSubimage() {
  return gl::g_current_gl_version->IsAtLeastGLES(3, 0) ||
         gl::g_current_gl_driver->ext.b_GL_EXT_unpack_subimage;
}

// This value can't be cached as it may change for different contexts.
bool SupportsPackSubimage() {
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86_FAMILY)
  // GL_PACK_ROW_LENGTH is broken in the Android emulator. glReadPixels()
  // modifies bytes between the last pixel in a row and the end of the stride
  // for that row.
  return false;
#else
  return gl::g_current_gl_version->IsAtLeastGLES(3, 0);
#endif
}

// Shared memory GMBs tend to use a stride that is 4 bytes aligned. If
// bytes_per_pixel is 1 or 2 and `stride` is already 4 byte aligned then
// use a 4 byte alignment. Otherwise use `bytes_per_pixel` as alignment. This
// can avoid a copy if GL_[UN]PACK_ROW_LENGTH isn't supported, eg. `stride` is
// 12 and `bytes_per_pixel` is 1 but there are only 10 bytes of data per row.
constexpr int ComputeBestAlignment(size_t bytes_per_pixel, size_t stride) {
  if (bytes_per_pixel < 4 && (stride & 0b11) == 0) {
    return 4;
  }

  return bytes_per_pixel;
}

}  // anonymous namespace

// static
// TODO(hitawala): Check GLFormatCaps for format support.
viz::SharedImageFormat GLTextureHolder::GetPlaneFormat(
    viz::SharedImageFormat format,
    int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  if (format.is_single_plane()) {
    return format;
  }

  int num_channels = format.NumChannelsInPlane(plane_index);
  DCHECK_LE(num_channels, 2);
  switch (format.channel_format()) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      return num_channels == 2 ? viz::SinglePlaneFormat::kRG_88
                               : viz::SinglePlaneFormat::kR_8;
    case viz::SharedImageFormat::ChannelFormat::k10:
    case viz::SharedImageFormat::ChannelFormat::k16:
      return num_channels == 2 ? viz::SinglePlaneFormat::kRG_1616
                               : viz::SinglePlaneFormat::kR_16;
    case viz::SharedImageFormat::ChannelFormat::k16F:
      CHECK_EQ(num_channels, 1);
      return viz::SinglePlaneFormat::kR_F16;
  }
  NOTREACHED();
}

GLTextureHolder::GLTextureHolder(viz::SharedImageFormat format,
                                 const gfx::Size& size,
                                 bool is_passthrough,
                                 gl::ProgressReporter* progress_reporter)
    : format_(format),
      size_(size),
      is_passthrough_(is_passthrough),
      progress_reporter_(progress_reporter) {
  CHECK(format_.is_single_plane());
}

// TODO(kylechar): When `texture_` is removed with validating command decoder
// move constructor/assignment can be defaulted.
GLTextureHolder::GLTextureHolder(GLTextureHolder&& other) {
  operator=(std::move(other));
}

GLTextureHolder& GLTextureHolder::operator=(GLTextureHolder&& other) {
  format_ = other.format_;
  size_ = other.size_;
  is_passthrough_ = other.is_passthrough_;
  context_lost_ = other.context_lost_;
  texture_ = other.texture_;
  other.texture_ = nullptr;
  passthrough_texture_ = std::move(other.passthrough_texture_);
  format_desc_ = other.format_desc_;
  progress_reporter_ = other.progress_reporter_;
  return *this;
}

GLTextureHolder::~GLTextureHolder() {
  if (is_passthrough_) {
    if (passthrough_texture_) {
      if (context_lost_) {
        passthrough_texture_->MarkContextLost();
      }
      passthrough_texture_.reset();
    }
  } else {
    if (texture_) {
      texture_.ExtractAsDangling()->RemoveLightweightRef(!context_lost_);
    }
  }
}

GLuint GLTextureHolder::GetServiceId() const {
  return is_passthrough_ ? passthrough_texture_->service_id()
                         : texture_->service_id();
}

void GLTextureHolder::Initialize(
    const GLCommonImageBackingFactory::FormatInfo& format_info,
    bool framebuffer_attachment_angle,
    base::span<const uint8_t> pixel_data,
    const std::string& debug_label) {
  DCHECK(!texture_ && !passthrough_texture_);

  format_desc_.target = GL_TEXTURE_2D;
  format_desc_.data_format = format_info.gl_format;
  format_desc_.data_type = format_info.gl_type;
  format_desc_.image_internal_format = format_info.image_internal_format;
  format_desc_.storage_internal_format = format_info.storage_internal_format;

  MakeTextureAndSetParameters(format_desc_.target, framebuffer_attachment_angle,
                              is_passthrough_ ? &passthrough_texture_ : nullptr,
                              is_passthrough_ ? nullptr : &texture_);

  if (is_passthrough_) {
    passthrough_texture_->SetEstimatedSize(format_.EstimatedSizeInBytes(size_));
  } else {
    // TODO(piman): We pretend the texture was created in an ES2 context, so
    // that it can be used in other ES2 contexts, and so we have to pass
    // gl_format as the internal format in the LevelInfo.
    // https://crbug.com/628064
    texture_->SetLevelInfo(format_desc_.target, 0, format_desc_.data_format,
                           size_.width(), size_.height(), /*depth=*/1, 0,
                           format_desc_.data_format, format_desc_.data_type,
                           /*cleared_rect=*/gfx::Rect());
    texture_->SetImmutable(true, format_info.supports_storage);
  }

  gl::GLApi* api = gl::g_current_gl_context;
  gl::ScopedRestoreTexture scoped_restore(api, format_desc_.target,
                                          GetServiceId());

  // Initialize the texture storage/image parameters and upload initial pixels
  // if available.
  if (format_info.supports_storage) {
    {
#if BUILDFLAG(IS_ANDROID)
      // When using angle via enabling passthrough command decoder on android,
      // disable renderability validation in angle for this texture since it is
      // being created in ES3 context with a format which could be
      // invalid/non-renderable in ES2/WEBGL1 context when this texture gets
      // imported into the ES2/WEBGL1 context.
      if (gl::g_current_gl_driver->ext.b_GL_ANGLE_renderability_validation) {
        api->glTexParameteriFn(format_desc_.target,
                               GL_RENDERABILITY_VALIDATION_ANGLE, GL_FALSE);
      }
#endif
      gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
      api->glTexStorage2DEXTFn(format_desc_.target, /*levels=*/1,
                               format_info.adjusted_storage_internal_format,
                               size_.width(), size_.height());
    }

    if (!pixel_data.empty()) {
      ScopedUnpackState scoped_unpack_state(
          /*uploading_data=*/true);
      gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
      api->glTexSubImage2DFn(format_desc_.target, /*level=*/0, /*xoffset=*/0,
                             /*yoffset=*/0, size_.width(), size_.height(),
                             format_info.adjusted_format,
                             format_desc_.data_type, pixel_data.data());
    }
  } else if (format_info.is_compressed) {
    ScopedUnpackState scoped_unpack_state(!pixel_data.empty());
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    api->glCompressedTexImage2DFn(format_desc_.target, 0,
                                  format_desc_.image_internal_format,
                                  size_.width(), size_.height(), /*border=*/0,
                                  pixel_data.size(), pixel_data.data());
  } else {
    ScopedUnpackState scoped_unpack_state(!pixel_data.empty());
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    api->glTexImage2DFn(
        format_desc_.target, /*level=*/0, format_desc_.image_internal_format,
        size_.width(), size_.height(), /*border=*/0,
        format_info.adjusted_format, format_desc_.data_type, pixel_data.data());
  }

  if (!is_passthrough_) {
    // Must be set after initial pixel upload.
    texture_->SetCompatibilitySwizzle(format_info.swizzle);
  }

  // If the extension does not exist, do not pass debug label to avoid crashes.
  if (!debug_label.empty() && gl::g_current_gl_driver->ext.b_GL_KHR_debug) {
    api->glObjectLabelFn(GL_TEXTURE, GetServiceId(), -1, debug_label.c_str());
  }
}

void GLTextureHolder::InitializeWithTexture(
    const GLFormatDesc& format_desc,
    scoped_refptr<gles2::TexturePassthrough> texture) {
  DCHECK(!texture_ && !passthrough_texture_);
  DCHECK(is_passthrough_);

  format_desc_ = format_desc;
  passthrough_texture_ = std::move(texture);
}

void GLTextureHolder::InitializeWithTexture(const GLFormatDesc& format_desc,
                                            gles2::Texture* texture) {
  DCHECK(!texture_ && !passthrough_texture_);
  DCHECK(!is_passthrough_);

  format_desc_ = format_desc;
  texture_ = texture;
}

bool GLTextureHolder::UploadFromMemory(const SkPixmap& pixmap) {
  DCHECK_EQ(pixmap.width(), size_.width());
  DCHECK_EQ(pixmap.height(), size_.height());

  const GLuint texture_id = GetServiceId();
  const GLenum gl_format = format_desc_.data_format;
  const GLenum gl_type = format_desc_.data_type;
  const GLenum gl_target = format_desc_.target;

  size_t src_stride = pixmap.rowBytes();
  size_t src_total_bytes = pixmap.computeByteSize();
  size_t src_bytes_per_pixel = pixmap.info().bytesPerPixel();

  const int gl_unpack_alignment =
      ComputeBestAlignment(src_bytes_per_pixel, src_stride);
  DCHECK_EQ(src_stride % gl_unpack_alignment, 0u);

  std::vector<uint8_t> repacked_data;
  if (format_ == viz::SinglePlaneFormat::kBGRX_8888 ||
      format_ == viz::SinglePlaneFormat::kRGBX_8888) {
    DCHECK_EQ(gl_format, static_cast<GLenum>(GL_RGB));
    DCHECK_EQ(gl_unpack_alignment, 4);

    // BGRX and RGBX data is uploaded as GL_RGB. Repack from 4 to 3 bytes per
    // pixel.
    repacked_data = RepackPixelDataAsRgb(
        size_, pixmap, format_ == viz::SinglePlaneFormat::kBGRX_8888);
    src_stride =
        base::bits::AlignUp<size_t>(size_.width() * 3, gl_unpack_alignment);
    src_total_bytes = repacked_data.size();
    src_bytes_per_pixel = 3;
  }

  // Compute expected stride and total bytes glTexSubImage2D() will access and
  // verify that works with source pixel data.
  uint32_t expected_total_bytes = 0;
  uint32_t expected_stride = 0;
  bool result = gles2::GLES2Util::ComputeImageDataSizes(
      size_.width(), size_.height(), /*depth=*/1, gl_format, gl_type,
      gl_unpack_alignment, &expected_total_bytes, nullptr, &expected_stride);
  DCHECK(result);
  DCHECK_GE(src_total_bytes, expected_total_bytes);
  DCHECK_GE(src_stride, expected_stride);

  GLuint gl_unpack_row_length = 0;
  if (src_stride != expected_stride) {
    // RGBX/BGRX has been repacked so it should always have expected stride.
    DCHECK(repacked_data.empty());

    if (SupportsUnpackSubimage()) {
      // Use GL_UNPACK_ROW_LENGTH to skip data past end of each row on upload.
      gl_unpack_row_length = src_stride / src_bytes_per_pixel;
    } else {
      // If GL_UNPACK_ROW_LENGTH isn't supported then repack pixels with the
      // expected stride.
      repacked_data = RepackPixelDataWithStride(size_, pixmap, expected_stride);
    }
  }

  gl::ScopedTextureBinder scoped_texture_binder(gl_target, texture_id);
  ScopedUnpackState scoped_unpack_state(
      /*uploading_data=*/true, gl_unpack_row_length, gl_unpack_alignment);

  const void* pixels =
      !repacked_data.empty() ? repacked_data.data() : pixmap.addr();
  gl::GLApi* api = gl::g_current_gl_context;
  {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    api->glTexSubImage2DFn(gl_target, /*level=*/0, 0, 0, size_.width(),
                           size_.height(), gl_format, gl_type, pixels);
  }

  return true;
}

bool GLTextureHolder::ReadbackToMemory(const SkPixmap& pixmap) {
  DCHECK_EQ(pixmap.width(), size_.width());
  DCHECK_EQ(pixmap.height(), size_.height());

  const GLuint texture_id = GetServiceId();
  GLenum gl_format = format_desc_.data_format;
  GLenum gl_type = format_desc_.data_type;

  if (format_ == viz::SinglePlaneFormat::kBGRX_8888 ||
      format_ == viz::SinglePlaneFormat::kRGBX_8888) {
    DCHECK_EQ(gl_format, static_cast<GLenum>(GL_RGB));
    DCHECK_EQ(gl_type, static_cast<GLenum>(GL_UNSIGNED_BYTE));

    // Always readback RGBX/BGRX as RGBA/BGRA instead of RGB to avoid needing a
    // temporary buffer.
    gl_format =
        format_ == viz::SinglePlaneFormat::kBGRX_8888 ? GL_BGRA_EXT : GL_RGBA;
  }

  gl::GLApi* api = gl::g_current_gl_context;
  GLuint framebuffer;
  api->glGenFramebuffersEXTFn(1, &framebuffer);
  gl::ScopedFramebufferBinder scoped_framebuffer_binder(framebuffer);
  // This uses GL_FRAMEBUFFER instead of GL_READ_FRAMEBUFFER as the target for
  // GLES2 compatibility.
  api->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, texture_id, /*level=*/0);
  DCHECK_EQ(api->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER),
            static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE));

  bool needs_rb_swizzle = false;

  // GL_RGBA + GL_UNSIGNED_BYTE are always supported. Otherwise there is a
  // preferred format + type that can be queried and is based on what is bound
  // to GL_READ_FRAMEBUFFER.
  if (gl_format != GL_RGBA || gl_type != GL_UNSIGNED_BYTE) {
    GLint preferred_format = 0;
    api->glGetIntegervFn(GL_IMPLEMENTATION_COLOR_READ_FORMAT,
                         &preferred_format);
    GLint preferred_type = 0;
    api->glGetIntegervFn(GL_IMPLEMENTATION_COLOR_READ_TYPE, &preferred_type);

    if (gl_format != static_cast<GLenum>(preferred_format) ||
        gl_type != static_cast<GLenum>(preferred_type)) {
      if (format_ == viz::SinglePlaneFormat::kBGRA_8888 ||
          format_ == viz::SinglePlaneFormat::kBGRX_8888) {
        DCHECK_EQ(gl_format, static_cast<GLenum>(GL_BGRA_EXT));
        DCHECK_EQ(gl_type, static_cast<GLenum>(GL_UNSIGNED_BYTE));

        // If BGRA readback isn't support then use RGBA and swizzle.
        gl_format = GL_RGBA;
        needs_rb_swizzle = true;
      } else {
        DLOG(ERROR) << format_.ToString()
                    << " is not supported by glReadPixels()";
        return false;
      }
    }
  }

  const size_t dst_stride = pixmap.rowBytes();
  const size_t dst_bytes_per_pixel = pixmap.info().bytesPerPixel();

  const int gl_pack_alignment =
      ComputeBestAlignment(dst_bytes_per_pixel, dst_stride);
  DCHECK_EQ(dst_stride % gl_pack_alignment, 0u);

  // Compute expected stride and total bytes glReadPixels() will access and
  // verify that works with destination pixel data.
  uint32_t expected_total_bytes = 0;
  uint32_t expected_stride = 0;
  bool result = gles2::GLES2Util::ComputeImageDataSizes(
      size_.width(), size_.height(), /*depth=*/1, gl_format, gl_type,
      gl_pack_alignment, &expected_total_bytes, nullptr, &expected_stride);
  DCHECK(result);
  DCHECK_GE(pixmap.computeByteSize(), expected_total_bytes);
  DCHECK_GE(dst_stride, expected_stride);

  std::vector<uint8_t> unpack_buffer;
  GLuint gl_pack_row_length = 0;
  if (dst_stride != expected_stride) {
    if (SupportsPackSubimage()) {
      // Use GL_PACK_ROW_LENGTH to avoid temporary buffer.
      gl_pack_row_length = dst_stride / dst_bytes_per_pixel;
    } else {
      // If GL_PACK_ROW_LENGTH isn't supported then readback to a temporary
      // buffer with expected stride.
      unpack_buffer = std::vector<uint8_t>(expected_stride * size_.height());
    }
  }

  ScopedPackState scoped_pack_state(gl_pack_row_length, gl_pack_alignment);

  void* pixels =
      !unpack_buffer.empty() ? unpack_buffer.data() : pixmap.writable_addr();
  {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    api->glReadPixelsFn(0, 0, size_.width(), size_.height(), gl_format, gl_type,
                        pixels);
  }

  api->glDeleteFramebuffersEXTFn(1, &framebuffer);

  if (!unpack_buffer.empty()) {
    DCHECK_GT(dst_stride, expected_stride);
    UnpackPixelDataWithStride(size_, unpack_buffer, expected_stride, pixmap);
  }

  if (needs_rb_swizzle) {
    SwizzleRedAndBlue(pixmap);
  }

  return true;
}

sk_sp<GrPromiseImageTexture> GLTextureHolder::GetPromiseImage(
    SharedContextState* context_state) {
  GrBackendTexture backend_texture;
  GetGrBackendTexture(context_state->feature_info(), format_desc_.target, size_,
                      GetServiceId(), format_desc_.storage_internal_format,
                      context_state->gr_context()->threadSafeProxy(),
                      &backend_texture);
  return GrPromiseImageTexture::Make(backend_texture);
}

gfx::Rect GLTextureHolder::GetClearedRect() const {
  DCHECK(!is_passthrough_);
  return texture_->GetLevelClearedRect(format_desc_.target, 0);
}

void GLTextureHolder::SetClearedRect(const gfx::Rect& cleared_rect) {
  DCHECK(!is_passthrough_);
  texture_->SetLevelClearedRect(format_desc_.target, 0, cleared_rect);
}

void GLTextureHolder::SetContextLost() {
  context_lost_ = true;
}

}  // namespace gpu
