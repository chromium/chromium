// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/pbuffer_image_backing.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrContextThreadSafeProxy.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {

PbufferImageBacking::PbufferImageBacking(
    base::OnceClosure on_destruction_closure,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    scoped_refptr<gles2::TexturePassthrough> passthrough_texture)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      format.EstimatedSizeInBytes(size),
                                      false /* is_thread_safe */),
      on_destruction_closure_runner_(std::move(on_destruction_closure)),
      passthrough_texture_(std::move(passthrough_texture)) {
  DCHECK(!!passthrough_texture_);
}

PbufferImageBacking::~PbufferImageBacking() {
  // If the cached promise texture is referencing the GL texture, then it needs
  // to be deleted, too.
  if (cached_promise_texture_) {
    if (cached_promise_texture_->backendTexture().backend() ==
        GrBackendApi::kOpenGL) {
      cached_promise_texture_.reset();
    }
  }

  if (!have_context())
    passthrough_texture_->MarkContextLost();
  passthrough_texture_.reset();
}

scoped_refptr<gfx::NativePixmap> PbufferImageBacking::GetNativePixmap() {
  return nullptr;
}

SharedImageBackingType PbufferImageBacking::GetType() const {
  return SharedImageBackingType::kGLImage;
}

std::unique_ptr<GLTextureImageRepresentation>
PbufferImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                      MemoryTypeTracker* tracker) {
  NOTREACHED();
  return nullptr;
}
std::unique_ptr<GLTexturePassthroughImageRepresentation>
PbufferImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                 MemoryTypeTracker* tracker) {
  DCHECK(passthrough_texture_);
  std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures = {
      passthrough_texture_};
  return std::make_unique<GLTexturePassthroughGLCommonRepresentation>(
      manager, this, this, tracker, std::move(gl_textures));
}

std::unique_ptr<OverlayImageRepresentation> PbufferImageBacking::ProduceOverlay(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  // PbufferPictureBuffer does not support overlays (
  // PbufferPictureBuffer::AllowOverlay() returns false), and so this method
  // should never be invoked.
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<DawnImageRepresentation> PbufferImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats) {
  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return GLTextureImageBackingHelper::ProduceDawnCommon(
      factory(), manager, tracker, device, backend_type,
      std::move(view_formats), this, true);
}

std::unique_ptr<SkiaImageRepresentation> PbufferImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  GLTextureImageRepresentationClient* gl_client = nullptr;
  if (context_state->GrContextIsGL()) {
    gl_client = this;
  }

  if (!cached_promise_texture_) {
    bool angle_rgbx_internal_format = context_state->feature_info()
                                          ->feature_flags()
                                          .angle_rgbx_internal_format;
    GLenum gl_texture_storage_format = TextureStorageFormat(
        format(), angle_rgbx_internal_format, /*plane_index=*/0);
    GrBackendTexture backend_texture;
    GetGrBackendTexture(
        context_state->feature_info(), GL_TEXTURE_2D, size(),
        passthrough_texture_->service_id(), gl_texture_storage_format,
        context_state->gr_context()->threadSafeProxy(), &backend_texture);
    cached_promise_texture_ = SkPromiseImageTexture::Make(backend_texture);
  }

  std::vector<sk_sp<SkPromiseImageTexture>> promise_textures = {
      cached_promise_texture_};
  return std::make_unique<SkiaGLCommonRepresentation>(
      manager, this, gl_client, std::move(context_state),
      std::move(promise_textures), tracker);
}

bool PbufferImageBacking::GLTextureImageRepresentationBeginAccess(
    bool readonly) {
  passthrough_texture_->clear_bind_pending();
  return true;
}

void PbufferImageBacking::GLTextureImageRepresentationEndAccess(bool readonly) {
}

}  // namespace gpu
