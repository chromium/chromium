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
#include "ui/gl/scoped_binders.h"
#include "ui/gl/shared_gl_fence_egl.h"
#include "ui/gl/trace_util.h"

#if defined(OS_ANDROID)
#include "gpu/command_buffer/service/shared_image_backing_egl_image.h"
#include "gpu/command_buffer/service/shared_image_batch_access_manager.h"
#endif

#if defined(OS_MAC)
#include "gpu/command_buffer/service/shared_image_backing_factory_iosurface.h"
#endif

namespace gpu {

namespace {

size_t EstimatedSize(viz::ResourceFormat format, const gfx::Size& size) {
  size_t estimated_size = 0;
  viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size);
  return estimated_size;
}

using InitializeGLTextureParams =
    SharedImageBackingGLCommon::InitializeGLTextureParams;

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingGLTexture

SharedImageBackingGLTexture::SharedImageBackingGLTexture(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_passthrough)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         EstimatedSize(format, size),
                         false /* is_thread_safe */),
      is_passthrough_(is_passthrough) {}

SharedImageBackingGLTexture::~SharedImageBackingGLTexture() {
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

GLenum SharedImageBackingGLTexture::GetGLTarget() const {
  return texture_ ? texture_->target() : passthrough_texture_->target();
}

GLuint SharedImageBackingGLTexture::GetGLServiceId() const {
  return texture_ ? texture_->service_id() : passthrough_texture_->service_id();
}

void SharedImageBackingGLTexture::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDump* dump,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  const auto client_guid = GetSharedImageGUIDForTracing(mailbox());
  if (!IsPassthrough()) {
    const auto service_guid =
        gl::GetGLTextureServiceGUIDForTracing(texture_->service_id());
    pmd->CreateSharedGlobalAllocatorDump(service_guid);
    pmd->AddOwnershipEdge(client_guid, service_guid, /* importance */ 2);
    texture_->DumpLevelMemory(pmd, client_tracing_id, dump_name);
  }
}

gfx::Rect SharedImageBackingGLTexture::ClearedRect() const {
  if (IsPassthrough()) {
    // This backing is used exclusively with ANGLE which handles clear tracking
    // internally. Act as though the texture is always cleared.
    return gfx::Rect(size());
  } else {
    return texture_->GetLevelClearedRect(texture_->target(), 0);
  }
}

void SharedImageBackingGLTexture::SetClearedRect(
    const gfx::Rect& cleared_rect) {
  if (!IsPassthrough())
    texture_->SetLevelClearedRect(texture_->target(), 0, cleared_rect);
}

bool SharedImageBackingGLTexture::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  if (IsPassthrough())
    mailbox_manager->ProduceTexture(mailbox(), passthrough_texture_.get());
  else
    mailbox_manager->ProduceTexture(mailbox(), texture_);
  return true;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingGLTexture::ProduceGLTexture(SharedImageManager* manager,
                                              MemoryTypeTracker* tracker) {
  DCHECK(texture_);
  return std::make_unique<SharedImageRepresentationGLTextureImpl>(
      manager, this, nullptr, tracker, texture_);
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingGLTexture::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  DCHECK(passthrough_texture_);
  return std::make_unique<SharedImageRepresentationGLTexturePassthroughImpl>(
      manager, this, nullptr, tracker, passthrough_texture_);
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingGLTexture::ProduceDawn(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker,
                                         WGPUDevice device) {
  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return SharedImageBackingGLCommon::ProduceDawnCommon(
      factory(), manager, tracker, device, this, IsPassthrough());
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingGLTexture::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (!cached_promise_texture_) {
    GrBackendTexture backend_texture;
    GetGrBackendTexture(context_state->feature_info(), GetGLTarget(), size(),
                        GetGLServiceId(), format(), &backend_texture);
    cached_promise_texture_ = SkPromiseImageTexture::Make(backend_texture);
  }
  return std::make_unique<SharedImageRepresentationSkiaImpl>(
      manager, this, nullptr, std::move(context_state), cached_promise_texture_,
      tracker);
}

void SharedImageBackingGLTexture::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {}

void SharedImageBackingGLTexture::InitializeGLTexture(
    GLuint service_id,
    const InitializeGLTextureParams& params) {
  SharedImageBackingGLCommon::MakeTextureAndSetParameters(
      params.target, service_id, params.framebuffer_attachment_angle,
      IsPassthrough() ? &passthrough_texture_ : nullptr,
      IsPassthrough() ? nullptr : &texture_);

  if (IsPassthrough()) {
    passthrough_texture_->SetEstimatedSize(EstimatedSize(format(), size()));
  } else {
    texture_->SetLevelInfo(params.target, 0, params.internal_format,
                           size().width(), size().height(), 1, 0, params.format,
                           params.type,
                           params.is_cleared ? gfx::Rect(size()) : gfx::Rect());
    texture_->SetImmutable(true, params.has_immutable_storage);
  }
}

void SharedImageBackingGLTexture::SetCompatibilitySwizzle(
    const gles2::Texture::CompatibilitySwizzle* swizzle) {
  if (!IsPassthrough())
    texture_->SetCompatibilitySwizzle(swizzle);
}

}  // namespace gpu
