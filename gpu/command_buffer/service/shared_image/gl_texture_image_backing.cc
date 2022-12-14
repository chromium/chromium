// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing.h"

#include <algorithm>
#include <list>
#include <string>
#include <utility>

#include "base/bits.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_repack_utils.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_common_representations.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrContextThreadSafeProxy.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/shared_gl_fence_egl.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(IS_MAC)
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"
#endif

#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include "gpu/command_buffer/service/shared_image/dawn_egl_image_representation.h"
#endif

namespace gpu {

namespace {

// This value can't be cached as it may change for different contexts.
bool SupportsUnpackSubimage() {
  return gl::g_current_gl_version->is_es3_capable ||
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
  return gl::g_current_gl_version->is_es3_capable;
#endif
}

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageBacking

bool GLTextureImageBacking::SupportsPixelReadbackWithFormat(
    viz::SharedImageFormat format) {
  if (!format.is_single_plane())
    return false;

  switch (format.resource_format()) {
    case viz::ResourceFormat::RGBA_8888:
    case viz::ResourceFormat::BGRA_8888:
    case viz::ResourceFormat::RED_8:
    case viz::ResourceFormat::RG_88:
    case viz::ResourceFormat::RGBX_8888:
    case viz::ResourceFormat::BGRX_8888:
      return true;
    default:
      return false;
  }
}

bool GLTextureImageBacking::SupportsPixelUploadWithFormat(
    viz::SharedImageFormat format) {
  if (!format.is_single_plane())
    return false;

  switch (format.resource_format()) {
    case viz::ResourceFormat::RGBA_8888:
    case viz::ResourceFormat::RGBA_4444:
    case viz::ResourceFormat::BGRA_8888:
    case viz::ResourceFormat::RED_8:
    case viz::ResourceFormat::RG_88:
    case viz::ResourceFormat::RGBA_F16:
    case viz::ResourceFormat::R16_EXT:
    case viz::ResourceFormat::RG16_EXT:
    case viz::ResourceFormat::RGBX_8888:
    case viz::ResourceFormat::BGRX_8888:
    case viz::ResourceFormat::RGBA_1010102:
    case viz::ResourceFormat::BGRA_1010102:
      return true;
    case viz::ResourceFormat::ALPHA_8:
    case viz::ResourceFormat::LUMINANCE_8:
    case viz::ResourceFormat::RGB_565:
    case viz::ResourceFormat::BGR_565:
    case viz::ResourceFormat::ETC1:
    case viz::ResourceFormat::LUMINANCE_F16:
    case viz::ResourceFormat::YVU_420:
    case viz::ResourceFormat::YUV_420_BIPLANAR:
    case viz::ResourceFormat::YUVA_420_TRIPLANAR:
    case viz::ResourceFormat::P010:
      return false;
  }
}

GLTextureImageBacking::GLTextureImageBacking(const Mailbox& mailbox,
                                             viz::SharedImageFormat format,
                                             const gfx::Size& size,
                                             const gfx::ColorSpace& color_space,
                                             GrSurfaceOrigin surface_origin,
                                             SkAlphaType alpha_type,
                                             uint32_t usage,
                                             bool is_passthrough)
    : ClearTrackingSharedImageBacking(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size, format),
          false /* is_thread_safe */),
      is_passthrough_(is_passthrough) {}

GLTextureImageBacking::~GLTextureImageBacking() {
  if (IsPassthrough()) {
    if (passthrough_texture_) {
      if (!have_context())
        passthrough_texture_->MarkContextLost();
      passthrough_texture_.reset();
    }
  } else {
    if (texture_) {
      texture_->RemoveLightweightRef(have_context());
      texture_ = nullptr;
    }
  }
}

GLenum GLTextureImageBacking::GetGLTarget() const {
  return texture_ ? texture_->target() : passthrough_texture_->target();
}

GLuint GLTextureImageBacking::GetGLServiceId() const {
  return texture_ ? texture_->service_id() : passthrough_texture_->service_id();
}

SharedImageBackingType GLTextureImageBacking::GetType() const {
  return SharedImageBackingType::kGLTexture;
}

gfx::Rect GLTextureImageBacking::ClearedRect() const {
  if (!IsPassthrough())
    return texture_->GetLevelClearedRect(texture_->target(), 0);

  // Use shared image based tracking for passthrough, because we don't always
  // use angle robust initialization.
  return ClearTrackingSharedImageBacking::ClearedRect();
}

void GLTextureImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  if (!IsPassthrough()) {
    texture_->SetLevelClearedRect(texture_->target(), 0, cleared_rect);
    return;
  }

  // Use shared image based tracking for passthrough, because we don't always
  // use angle robust initialization.
  ClearTrackingSharedImageBacking::SetClearedRect(cleared_rect);
}

void GLTextureImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {}

bool GLTextureImageBacking::UploadFromMemory(const SkPixmap& pixmap) {
  DCHECK(SupportsPixelUploadWithFormat(format()));
  DCHECK(gl::GLContext::GetCurrent());

  auto resource_format = format().resource_format();

  const GLuint texture_id = GetGLServiceId();
  const GLenum gl_format = format_desc_.data_format;
  const GLenum gl_type = format_desc_.data_type;
  const GLenum gl_target = format_desc_.target;

  // Actual stride of the given pixmap, not necessarily with the expected
  // alignment, or equal to expected stride.
  const size_t pixmap_stride = pixmap.rowBytes();
  const size_t expected_stride = pixmap.info().minRowBytes64();
  const GLuint gl_unpack_alignment = pixmap.info().bytesPerPixel();
  DCHECK_GE(pixmap_stride, expected_stride);
  DCHECK_EQ(expected_stride % gl_unpack_alignment, 0u);

  GLuint gl_unpack_row_length = 0;
  std::vector<uint8_t> repacked_data;
  if (resource_format == viz::BGRX_8888 || resource_format == viz::RGBX_8888) {
    DCHECK_EQ(gl_format, static_cast<GLenum>(GL_RGB));
    // BGRX and RGBX data is uploaded as GL_RGB. Repack from 4 to 3 bytes per
    // pixel.
    repacked_data =
        RepackPixelDataAsRgb(size(), pixmap, resource_format == viz::BGRX_8888);
  } else if (pixmap_stride != expected_stride) {
    if (SupportsUnpackSubimage()) {
      // Use GL_UNPACK_ROW_LENGTH to skip data past end of each row on upload.
      gl_unpack_row_length = pixmap_stride / gl_unpack_alignment;
    } else {
      // If GL_UNPACK_ROW_LENGTH isn't supported then repack pixels with the
      // expected stride.
      repacked_data =
          RepackPixelDataWithStride(size(), pixmap, expected_stride);
    }
  }

  gl::ScopedTextureBinder scoped_texture_binder(gl_target, texture_id);
  ScopedUnpackState scoped_unpack_state(
      /*uploading_data=*/true, gl_unpack_row_length, gl_unpack_alignment);

  const void* pixels =
      !repacked_data.empty() ? repacked_data.data() : pixmap.addr();
  gl::GLApi* api = gl::g_current_gl_context;
  api->glTexSubImage2DFn(gl_target, /*level=*/0, 0, 0, size().width(),
                         size().height(), gl_format, gl_type, pixels);
  DCHECK_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  return true;
}

bool GLTextureImageBacking::ReadbackToMemory(SkPixmap& pixmap) {
  DCHECK(gl::GLContext::GetCurrent());

  // TODO(kylechar): Ideally there would be a usage that stated readback was
  // required so support could be verified at creation time and then asserted
  // here instead.
  if (!SupportsPixelReadbackWithFormat(format()))
    return false;

  viz::ResourceFormat resource_format = format().resource_format();

  const GLuint texture_id = GetGLServiceId();
  GLenum gl_format = format_desc_.data_format;
  GLenum gl_type = format_desc_.data_type;

  if (resource_format == viz::BGRX_8888 || resource_format == viz::RGBX_8888) {
    DCHECK_EQ(gl_format, static_cast<GLenum>(GL_RGB));
    DCHECK_EQ(gl_type, static_cast<GLenum>(GL_UNSIGNED_BYTE));

    // Always readback RGBX/BGRX as RGBA/BGRA instead of RGB to avoid needing a
    // temporary buffer.
    gl_format = resource_format == viz::BGRX_8888 ? GL_BGRA_EXT : GL_RGBA;
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
      if (resource_format == viz::BGRA_8888 ||
          resource_format == viz::BGRX_8888) {
        DCHECK_EQ(gl_format, static_cast<GLenum>(GL_BGRA_EXT));
        DCHECK_EQ(gl_type, static_cast<GLenum>(GL_UNSIGNED_BYTE));

        // If BGRA readback isn't support then use RGBA and swizzle.
        gl_format = GL_RGBA;
        needs_rb_swizzle = true;
      } else {
        DLOG(ERROR) << format().ToString()
                    << " is not supported by glReadPixels()";
        return false;
      }
    }
  }

  const size_t pixmap_stride = pixmap.rowBytes();
  const size_t expected_stride = pixmap.info().minRowBytes64();
  const GLuint gl_pack_alignment = pixmap.info().bytesPerPixel();
  DCHECK_GE(pixmap_stride, expected_stride);
  DCHECK_EQ(expected_stride % gl_pack_alignment, 0u);

  std::vector<uint8_t> unpack_buffer;
  GLuint gl_pack_row_length = 0;
  if (pixmap_stride != expected_stride) {
    if (SupportsPackSubimage()) {
      // Use GL_PACK_ROW_LENGTH to avoid temporary buffer.
      gl_pack_row_length = pixmap_stride / gl_pack_alignment;
    } else {
      // If GL_PACK_ROW_LENGTH isn't supported then readback to a temporary
      // buffer with expected stride.
      unpack_buffer = std::vector<uint8_t>(expected_stride * size().height());
    }
  }

  ScopedPackState scoped_pack_state(gl_pack_row_length, gl_pack_alignment);

  void* pixels =
      !unpack_buffer.empty() ? unpack_buffer.data() : pixmap.writable_addr();
  api->glReadPixelsFn(0, 0, size().width(), size().height(), gl_format, gl_type,
                      pixels);
  DCHECK_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  api->glDeleteFramebuffersEXTFn(1, &framebuffer);

  if (!unpack_buffer.empty()) {
    DCHECK_GT(pixmap_stride, expected_stride);
    UnpackPixelDataWithStride(size(), unpack_buffer, expected_stride, pixmap);
  }

  if (needs_rb_swizzle) {
    SwizzleRedAndBlue(pixmap);
  }

  return true;
}

std::unique_ptr<GLTextureImageRepresentation>
GLTextureImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker) {
  DCHECK(texture_);
  return std::make_unique<GLTextureGLCommonRepresentation>(
      manager, this, nullptr, tracker, texture_);
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
GLTextureImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                   MemoryTypeTracker* tracker) {
  DCHECK(passthrough_texture_);
  return std::make_unique<GLTexturePassthroughGLCommonRepresentation>(
      manager, this, nullptr, tracker, passthrough_texture_);
}

std::unique_ptr<DawnImageRepresentation> GLTextureImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats) {
#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  if (backend_type == WGPUBackendType_OpenGLES) {
    if (!image_egl_) {
      CreateEGLImage();
    }
    std::unique_ptr<GLTextureImageRepresentationBase> texture;
    if (IsPassthrough()) {
      texture = ProduceGLTexturePassthrough(manager, tracker);
    } else {
      texture = ProduceGLTexture(manager, tracker);
    }
    return std::make_unique<DawnEGLImageRepresentation>(
        std::move(texture), manager, this, tracker, device);
  }
#endif

  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return GLTextureImageBackingHelper::ProduceDawnCommon(
      factory(), manager, tracker, device, backend_type,
      std::move(view_formats), this, IsPassthrough());
}

std::unique_ptr<SkiaImageRepresentation> GLTextureImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (!cached_promise_texture_) {
    bool angle_rgbx_internal_format = context_state->feature_info()
                                          ->feature_flags()
                                          .angle_rgbx_internal_format;
    GLenum gl_texture_storage_format = TextureStorageFormat(
        format(), angle_rgbx_internal_format, /*plane_index=*/0);
    GrBackendTexture backend_texture;
    GetGrBackendTexture(context_state->feature_info(), GetGLTarget(), size(),
                        GetGLServiceId(), gl_texture_storage_format,
                        context_state->gr_context()->threadSafeProxy(),
                        &backend_texture);
    cached_promise_texture_ = SkPromiseImageTexture::Make(backend_texture);
  }
  return std::make_unique<SkiaGLCommonRepresentation>(
      manager, this, nullptr, std::move(context_state), cached_promise_texture_,
      tracker);
}

void GLTextureImageBacking::InitializeGLTexture(
    const GLCommonImageBackingFactory::FormatInfo& format_info,
    bool is_cleared,
    bool framebuffer_attachment_angle) {
  format_desc_.target = GL_TEXTURE_2D;
  format_desc_.data_format = format_info.gl_format;
  format_desc_.data_type = format_info.gl_type;
  format_desc_.image_internal_format = format_info.image_internal_format;
  format_desc_.storage_internal_format = format_info.storage_internal_format;

  GLTextureImageBackingHelper::MakeTextureAndSetParameters(
      format_desc_.target, /*service_id=*/0, framebuffer_attachment_angle,
      IsPassthrough() ? &passthrough_texture_ : nullptr,
      IsPassthrough() ? nullptr : &texture_);

  if (IsPassthrough()) {
    passthrough_texture_->SetEstimatedSize(GetEstimatedSize());
    SetClearedRect(is_cleared ? gfx::Rect(size()) : gfx::Rect());
  } else {
    // TODO(piman): We pretend the texture was created in an ES2 context, so
    // that it can be used in other ES2 contexts, and so we have to pass
    // gl_format as the internal format in the LevelInfo.
    // https://crbug.com/628064
    texture_->SetLevelInfo(format_desc_.target, 0, format_desc_.data_format,
                           size().width(), size().height(), /*depth=*/1, 0,
                           format_desc_.data_format, format_desc_.data_type,
                           is_cleared ? gfx::Rect(size()) : gfx::Rect());
    texture_->SetImmutable(true, format_info.supports_storage);
  }
}

void GLTextureImageBacking::CreateEGLImage() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
  SharedContextState* shared_context_state = factory()->GetSharedContextState();
  ui::ScopedMakeCurrent smc(shared_context_state->context(),
                            shared_context_state->surface());
  image_egl_ = gl::GLImageNativePixmap::CreateFromTexture(
      size(), ToBufferFormat(format()), GetGLServiceId());
  if (passthrough_texture_) {
    passthrough_texture_->SetLevelImage(passthrough_texture_->target(), 0,
                                        image_egl_.get());
  } else if (texture_) {
    texture_->SetLevelImage(texture_->target(), 0, image_egl_.get(),
                            gles2::Texture::ImageState::BOUND);
  }
#endif
}

void GLTextureImageBacking::SetCompatibilitySwizzle(
    const gles2::Texture::CompatibilitySwizzle* swizzle) {
  if (!IsPassthrough())
    texture_->SetCompatibilitySwizzle(swizzle);
}

}  // namespace gpu
