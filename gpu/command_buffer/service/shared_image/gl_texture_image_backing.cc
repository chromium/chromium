// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing.h"

#include <algorithm>
#include <list>
#include <string>
#include <utility>

#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_common_representations.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/scoped_make_current.h"

#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include "gpu/command_buffer/service/shared_image/dawn_egl_image_representation.h"
#endif

namespace gpu {

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageBacking

bool GLTextureImageBacking::SupportsPixelReadbackWithFormat(
    viz::SharedImageFormat format) {
  if (!format.is_single_plane()) {
    return false;
  }

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
  if (!format.is_single_plane()) {
    return false;
  }

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
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      format.EstimatedSizeInBytes(size),
                                      /*is_thread_safe=*/false),
      is_passthrough_(is_passthrough),
      texture_(format.resource_format(), size, is_passthrough) {}

GLTextureImageBacking::~GLTextureImageBacking() {
  if (!have_context()) {
    texture_.SetContextLost();
  }
}

SharedImageBackingType GLTextureImageBacking::GetType() const {
  return SharedImageBackingType::kGLTexture;
}

gfx::Rect GLTextureImageBacking::ClearedRect() const {
  if (!IsPassthrough()) {
    return texture_.GetClearedRect();
  }

  // Use shared image based tracking for passthrough, because we don't always
  // use angle robust initialization.
  return ClearTrackingSharedImageBacking::ClearedRect();
}

void GLTextureImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  if (!IsPassthrough()) {
    texture_.SetClearedRect(cleared_rect);
    return;
  }

  // Use shared image based tracking for passthrough, because we don't always
  // use angle robust initialization.
  ClearTrackingSharedImageBacking::SetClearedRect(cleared_rect);
}

void GLTextureImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {}

bool GLTextureImageBacking::UploadFromMemory(
    const std::vector<SkPixmap>& pixmaps) {
  DCHECK_EQ(pixmaps.size(), 1u);
  DCHECK(SupportsPixelUploadWithFormat(format()));
  DCHECK(gl::GLContext::GetCurrent());

  return texture_.UploadFromMemory(pixmaps[0]);
}

bool GLTextureImageBacking::ReadbackToMemory(SkPixmap& pixmap) {
  DCHECK(gl::GLContext::GetCurrent());

  // TODO(kylechar): Ideally there would be a usage that stated readback was
  // required so support could be verified at creation time and then asserted
  // here instead.
  if (!SupportsPixelReadbackWithFormat(format())) {
    return false;
  }

  return texture_.ReadbackToMemory(pixmap);
}

std::unique_ptr<GLTextureImageRepresentation>
GLTextureImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker) {
  DCHECK(texture_.texture());
  std::vector<raw_ptr<gles2::Texture>> gl_textures = {texture_.texture()};
  return std::make_unique<GLTextureGLCommonRepresentation>(
      manager, this, nullptr, tracker, std::move(gl_textures));
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
GLTextureImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                   MemoryTypeTracker* tracker) {
  DCHECK(texture_.passthrough_texture());
  std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures = {
      texture_.passthrough_texture()};
  return std::make_unique<GLTexturePassthroughGLCommonRepresentation>(
      manager, this, nullptr, tracker, std::move(gl_textures));
}

std::unique_ptr<DawnImageRepresentation> GLTextureImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats) {
#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  if (backend_type == WGPUBackendType_OpenGLES) {
    // GLImageNativePixmap is only compiled on below os.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
    if (!gl_image_native_pixmap_) {
      SharedContextState* shared_context_state =
          factory()->GetSharedContextState();
      ui::ScopedMakeCurrent smc(shared_context_state->context(),
                                shared_context_state->surface());
      gl_image_native_pixmap_ = gl::GLImageNativePixmap::CreateFromTexture(
          size(), ToBufferFormat(format()), texture_.GetServiceId());
      if (!gl_image_native_pixmap_) {
        DLOG(ERROR) << "Unable to create a GLImage";
        return nullptr;
      }
    }
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation;
    if (IsPassthrough()) {
      gl_representation = ProduceGLTexturePassthrough(manager, tracker);
    } else {
      gl_representation = ProduceGLTexture(manager, tracker);
    }
    return std::make_unique<DawnEGLImageRepresentation>(
        std::move(gl_representation), gl_image_native_pixmap_->GetEGLImage(),
        manager, this, tracker, device);
#else
    DLOG(ERROR) << "Dawn representation not supported";
    return nullptr;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
  }
#endif  // BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)

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
    cached_promise_texture_ = texture_.GetPromiseImage(context_state.get());
  }
  std::vector<sk_sp<SkPromiseImageTexture>> promise_textures = {
      cached_promise_texture_};
  return std::make_unique<SkiaGLCommonRepresentation>(
      manager, this, nullptr, std::move(context_state),
      std::move(promise_textures), tracker);
}

void GLTextureImageBacking::InitializeGLTexture(
    const GLCommonImageBackingFactory::FormatInfo& format_info,
    base::span<const uint8_t> pixel_data,
    gl::ProgressReporter* progress_reporter,
    bool framebuffer_attachment_angle) {
  std::string debug_label;
  if (gl::g_current_gl_driver->ext.b_GL_KHR_debug) {
    debug_label =
        "SharedImage_GLTexture" + CreateLabelForSharedImageUsage(usage());
  }
  texture_.Initialize(format_info, framebuffer_attachment_angle, pixel_data,
                      progress_reporter, debug_label);

  if (!pixel_data.empty()) {
    SetCleared();
  }
}

}  // namespace gpu
