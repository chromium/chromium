// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/pbuffer_image_backing.h"

#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrContextThreadSafeProxy.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/trace_util.h"

namespace gpu {

// static
std::unique_ptr<PbufferImageBacking> PbufferImageBacking::CreateFromGLTexture(
    base::OnceClosure on_destruction_closure,
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    scoped_refptr<gles2::TexturePassthrough> wrapped_gl_texture) {
  DCHECK(!!wrapped_gl_texture);

  auto si_format = viz::SharedImageFormat::SinglePlane(format);
  auto shared_image =
      base::WrapUnique<PbufferImageBacking>(new PbufferImageBacking(
          std::move(on_destruction_closure), mailbox, si_format, size,
          color_space, surface_origin, alpha_type, usage,
          std::move(wrapped_gl_texture)));

  return shared_image;
}

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
      on_destruction_closure_runner_(std::move(on_destruction_closure)),
      passthrough_texture_(std::move(passthrough_texture)) {}

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

GLenum PbufferImageBacking::GetGLTarget() const {
  return GL_TEXTURE_2D;
}

GLuint PbufferImageBacking::GetGLServiceId() const {
  return passthrough_texture_->service_id();
}

scoped_refptr<gfx::NativePixmap> PbufferImageBacking::GetNativePixmap() {
  return nullptr;
}

void PbufferImageBacking::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDumpGuid client_guid,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  SharedImageBacking::OnMemoryDump(dump_name, client_guid, pmd,
                                   client_tracing_id);

  // Add a |service_guid| which expresses shared ownership between the
  // various GPU dumps.
  if (auto service_id = GetGLServiceId()) {
    auto service_guid = gl::GetGLTextureServiceGUIDForTracing(GetGLServiceId());
    pmd->CreateSharedGlobalAllocatorDump(service_guid);
    pmd->AddOwnershipEdge(client_guid, service_guid, kOwningEdgeImportance);
  }
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
  return std::make_unique<GLTexturePassthroughGLCommonRepresentation>(
      manager, this, this, tracker, passthrough_texture_);
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
    WGPUBackendType backend_type) {
  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return GLTextureImageBackingHelper::ProduceDawnCommon(
      factory(), manager, tracker, device, backend_type, this, true);
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
    GrBackendTexture backend_texture;
    GetGrBackendTexture(context_state->feature_info(), GetGLTarget(), size(),
                        GetGLServiceId(), format().resource_format(),
                        context_state->gr_context()->threadSafeProxy(),
                        &backend_texture);
    cached_promise_texture_ = SkPromiseImageTexture::Make(backend_texture);
  }
  return std::make_unique<SkiaGLCommonRepresentation>(
      manager, this, gl_client, std::move(context_state),
      cached_promise_texture_, tracker);
}

bool PbufferImageBacking::GLTextureImageRepresentationBeginAccess(
    bool readonly) {
  passthrough_texture_->set_is_bind_pending(false);
  return true;
}

void PbufferImageBacking::GLTextureImageRepresentationEndAccess(bool readonly) {
}

void PbufferImageBacking::GLTextureImageRepresentationRelease(
    bool has_context) {
  // No action needed: This class retains the passed-in texture for its
  // lifetime, and releases it in its destructor.
}

}  // namespace gpu
