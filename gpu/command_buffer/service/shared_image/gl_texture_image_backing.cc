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

size_t EstimatedSize(viz::ResourceFormat format, const gfx::Size& size) {
  size_t estimated_size = 0;
  viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size);
  return estimated_size;
}

using InitializeGLTextureParams =
    GLTextureImageBackingHelper::InitializeGLTextureParams;

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageBacking

GLTextureImageBacking::GLTextureImageBacking(const Mailbox& mailbox,
                                             viz::ResourceFormat format,
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
                                      EstimatedSize(format, size),
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
                        GetGLServiceId(), format(),
                        context_state->gr_context()->threadSafeProxy(),
                        &backend_texture);
    cached_promise_texture_ = SkPromiseImageTexture::Make(backend_texture);
  }
  return std::make_unique<SkiaGLCommonRepresentation>(
      manager, this, nullptr, std::move(context_state), cached_promise_texture_,
      tracker);
}

void GLTextureImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {}

void GLTextureImageBacking::InitializeGLTexture(
    GLuint service_id,
    const InitializeGLTextureParams& params) {
  GLTextureImageBackingHelper::MakeTextureAndSetParameters(
      params.target, service_id, params.framebuffer_attachment_angle,
      IsPassthrough() ? &passthrough_texture_ : nullptr,
      IsPassthrough() ? nullptr : &texture_);

  if (IsPassthrough()) {
    passthrough_texture_->SetEstimatedSize(EstimatedSize(format(), size()));
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
