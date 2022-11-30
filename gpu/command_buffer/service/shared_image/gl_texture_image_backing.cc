// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing.h"

#include <algorithm>
#include <list>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_image_backing.h"
#include "gpu/command_buffer/service/shared_image/gl_repack_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
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
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/shared_gl_fence_egl.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/command_buffer/service/shared_image/egl_image_backing.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"
#endif

#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include "gpu/command_buffer/service/shared_image/dawn_egl_image_representation.h"
#endif

namespace gpu {

namespace {

using InitializeGLTextureParams =
    GLTextureImageBackingHelper::InitializeGLTextureParams;

int BytesPerPixel(viz::SharedImageFormat format) {
  int bits = viz::BitsPerPixel(format);
  DCHECK_GE(bits, 8);
  return bits / 8;
}

bool HasFourByteAlignment(size_t stride) {
  return (stride & 3) == 0;
}

// This value can't be cached as it may change for different contexts.
bool SupportsUnpackSubimage() {
  return gl::g_current_gl_version->is_es3_capable ||
         gl::g_current_gl_driver->ext.b_GL_EXT_unpack_subimage;
}

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageBacking

bool GLTextureImageBacking::SupportsPixelUploadWithFormat(
    viz::SharedImageFormat format) {
  auto resource_format = format.resource_format();
  switch (resource_format) {
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

void GLTextureImageBacking::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDumpGuid client_guid,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  SharedImageBacking::OnMemoryDump(dump_name, client_guid, pmd,
                                   client_tracing_id);

  if (!IsPassthrough()) {
    const auto service_guid =
        gl::GetGLTextureServiceGUIDForTracing(texture_->service_id());
    pmd->CreateSharedGlobalAllocatorDump(service_guid);
    pmd->AddOwnershipEdge(client_guid, service_guid, kOwningEdgeImportance);
    texture_->DumpLevelMemory(pmd, client_tracing_id, dump_name);
  }
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

  const GLuint texture_id = GetGLServiceId();
  const GLenum gl_format = texture_params_.format;
  const GLenum gl_type = texture_params_.type;
  const GLenum gl_target = texture_params_.target;

  size_t pixmap_stride = pixmap.rowBytes();
  DCHECK(HasFourByteAlignment(pixmap_stride));

  size_t expected_stride = gfx::RowSizeForBufferFormat(
      size().width(), viz::BufferFormat(format()), /*plane=*/0);
  DCHECK(HasFourByteAlignment(expected_stride));
  DCHECK_GE(pixmap_stride, expected_stride);

  GLuint gl_unpack_row_length = 0;
  std::vector<uint8_t> repacked_data;
  auto resource_format = format().resource_format();
  if (resource_format == viz::BGRX_8888 || resource_format == viz::RGBX_8888) {
    DCHECK_EQ(gl_format, static_cast<GLenum>(GL_RGB));

    // BGRX and RGBX data is uploaded as GL_RGB. Repack from 4 to 3 bytes per
    // pixel.
    repacked_data =
        RepackPixelDataAsRgb(size(), pixmap, resource_format == viz::BGRX_8888);
  } else if (pixmap_stride > expected_stride) {
    if (SupportsUnpackSubimage()) {
      // Use GL_UNPACK_ROW_LENGTH to skip data past end of each row on upload.
      gl_unpack_row_length =
          base::checked_cast<int>(pixmap_stride) / BytesPerPixel(format());
    } else {
      // If GL_UNPACK_ROW_LENGTH isn't supported then repack pixels with the
      // expected stride.
      repacked_data =
          RepackPixelDataWithStride(size(), pixmap, expected_stride);
    }
  }

  gl::ScopedTextureBinder scoped_texture_binder(gl_target, texture_id);
  ScopedUnpackState scoped_unpack_state(/*uploading_data=*/true,
                                        gl_unpack_row_length);

  const void* pixels =
      !repacked_data.empty() ? repacked_data.data() : pixmap.addr();
  gl::GLApi* api = gl::g_current_gl_context;
  api->glTexSubImage2DFn(gl_target, /*level=*/0, 0, 0, size().width(),
                         size().height(), gl_format, gl_type, pixels);
  DCHECK_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

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
    WGPUBackendType backend_type) {
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
      factory(), manager, tracker, device, backend_type, this, IsPassthrough());
}

std::unique_ptr<SkiaImageRepresentation> GLTextureImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (!cached_promise_texture_) {
    GrBackendTexture backend_texture;
    GetGrBackendTexture(context_state->feature_info(), GetGLTarget(), size(),
                        GetGLServiceId(), format().resource_format(),
                        context_state->gr_context()->threadSafeProxy(),
                        &backend_texture);
    cached_promise_texture_ = SkPromiseImageTexture::Make(backend_texture);
  }
  return std::make_unique<SkiaGLCommonRepresentation>(
      manager, this, nullptr, std::move(context_state), cached_promise_texture_,
      tracker);
}

void GLTextureImageBacking::InitializeGLTexture(
    GLuint service_id,
    const InitializeGLTextureParams& params) {
  GLTextureImageBackingHelper::MakeTextureAndSetParameters(
      params.target, service_id, params.framebuffer_attachment_angle,
      IsPassthrough() ? &passthrough_texture_ : nullptr,
      IsPassthrough() ? nullptr : &texture_);
  texture_params_ = params;
  if (IsPassthrough()) {
    passthrough_texture_->SetEstimatedSize(
        viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size(), format()));
    SetClearedRect(params.is_cleared ? gfx::Rect(size()) : gfx::Rect());
  } else {
    texture_->SetLevelInfo(params.target, 0, params.internal_format,
                           size().width(), size().height(), 1, 0, params.format,
                           params.type,
                           params.is_cleared ? gfx::Rect(size()) : gfx::Rect());
    texture_->SetImmutable(true, params.has_immutable_storage);
  }
}

void GLTextureImageBacking::CreateEGLImage() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || defined(USE_OZONE)
  SharedContextState* shared_context_state = factory()->GetSharedContextState();
  ui::ScopedMakeCurrent smc(shared_context_state->context(),
                            shared_context_state->surface());
  auto image_np = base::MakeRefCounted<gl::GLImageNativePixmap>(
      size(), viz::BufferFormat(format()));
  image_np->InitializeFromTexture(GetGLServiceId());
  image_egl_ = image_np;
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
