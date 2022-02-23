// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_video_surface_texture.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

SharedImageVideoSurfaceTexture::SharedImageVideoSurfaceTexture(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
    scoped_refptr<SharedContextState> context_state)
    : SharedImageVideo(mailbox,
                       size,
                       color_space,
                       surface_origin,
                       alpha_type,
                       /*is_thread_safe=*/false),
      stream_texture_sii_(std::move(stream_texture_sii)),
      context_state_(std::move(context_state)),
      gpu_main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(stream_texture_sii_);
  DCHECK(context_state_);

  context_state_->AddContextLostObserver(this);
}

SharedImageVideoSurfaceTexture::~SharedImageVideoSurfaceTexture() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  if (context_state_)
    context_state_->RemoveContextLostObserver(this);
  context_state_.reset();
  stream_texture_sii_->ReleaseResources();
  stream_texture_sii_.reset();
}

size_t SharedImageVideoSurfaceTexture::EstimatedSizeForMemTracking() const {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  // This backing contributes to gpu memory only if its bound to the texture
  // and not when the backing is created.
  return stream_texture_sii_->IsUsingGpuMemory() ? estimated_size() : 0;
}

void SharedImageVideoSurfaceTexture::OnContextLost() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  // We release codec buffers when shared image context is lost. This is
  // because texture owner's texture was created on shared context. Once
  // shared context is lost, no one should try to use that texture.
  stream_texture_sii_->ReleaseResources();
  context_state_->RemoveContextLostObserver(this);
  context_state_ = nullptr;
}

// Representation of SharedImageVideoSurfaceTexture as a GL Texture.
class SharedImageVideoSurfaceTexture::SharedImageRepresentationGLTextureVideo
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureVideo(
      SharedImageManager* manager,
      SharedImageVideoSurfaceTexture* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<gles2::AbstractTexture> texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(std::move(texture)) {}

  // Disallow copy and assign.
  SharedImageRepresentationGLTextureVideo(
      const SharedImageRepresentationGLTextureVideo&) = delete;
  SharedImageRepresentationGLTextureVideo& operator=(
      const SharedImageRepresentationGLTextureVideo&) = delete;

  gles2::Texture* GetTexture() override {
    auto* texture = gles2::Texture::CheckedCast(texture_->GetTextureBase());
    DCHECK(texture);

    return texture;
  }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    auto* video_backing =
        static_cast<SharedImageVideoSurfaceTexture*>(backing());
    video_backing->BeginGLReadAccess(texture_->service_id());
    return true;
  }

  void EndAccess() override {}

 private:
  std::unique_ptr<gles2::AbstractTexture> texture_;
};

// Representation of SharedImageVideoSurfaceTexture as a GL Texture.
class SharedImageVideoSurfaceTexture::
    SharedImageRepresentationGLTexturePassthroughVideo
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughVideo(
      SharedImageManager* manager,
      SharedImageVideoSurfaceTexture* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<gles2::AbstractTexture> abstract_texture)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        abstract_texture_(std::move(abstract_texture)),
        passthrough_texture_(gles2::TexturePassthrough::CheckedCast(
            abstract_texture_->GetTextureBase())) {
    // TODO(https://crbug.com/1172769): Remove this CHECK.
    CHECK(passthrough_texture_);
  }

  // Disallow copy and assign.
  SharedImageRepresentationGLTexturePassthroughVideo(
      const SharedImageRepresentationGLTexturePassthroughVideo&) = delete;
  SharedImageRepresentationGLTexturePassthroughVideo& operator=(
      const SharedImageRepresentationGLTexturePassthroughVideo&) = delete;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    return passthrough_texture_;
  }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    auto* video_backing =
        static_cast<SharedImageVideoSurfaceTexture*>(backing());
    video_backing->BeginGLReadAccess(passthrough_texture_->service_id());
    return true;
  }

  void EndAccess() override {}

 private:
  std::unique_ptr<gles2::AbstractTexture> abstract_texture_;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;
};

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageVideoSurfaceTexture::ProduceGLTexture(SharedImageManager* manager,
                                                 MemoryTypeTracker* tracker) {
  // Note that for DrDc this method will never be called for
  // this(SurfaceTexture) implementation and for Webview, this method is called
  // only on gpu main thread.
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;

  // Generate an abstract texture.
  auto texture = GenAbstractTexture(/*passthrough=*/false);
  if (!texture)
    return nullptr;

  // If TextureOwner binds texture implicitly on update, that means it will
  // use TextureOwner texture_id to update and bind. Hence use TextureOwner
  // texture_id in abstract texture via BindStreamTextureImage().
  DCHECK(stream_texture_sii_->TextureOwnerBindsTextureOnUpdate());
  texture->BindStreamTextureImage(
      stream_texture_sii_.get(),
      stream_texture_sii_->GetTextureBase()->service_id());

  return std::make_unique<SharedImageRepresentationGLTextureVideo>(
      manager, this, tracker, std::move(texture));
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageVideoSurfaceTexture::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;

  // Generate an abstract texture.
  auto texture = GenAbstractTexture(/*passthrough=*/true);
  if (!texture)
    return nullptr;

  // If TextureOwner binds texture implicitly on update, that means it will
  // use TextureOwner texture_id to update and bind. Hence use TextureOwner
  // texture_id in abstract texture via BindStreamTextureImage().
  DCHECK(stream_texture_sii_->TextureOwnerBindsTextureOnUpdate());
  texture->BindStreamTextureImage(
      stream_texture_sii_.get(),
      stream_texture_sii_->GetTextureBase()->service_id());

  return std::make_unique<SharedImageRepresentationGLTexturePassthroughVideo>(
      manager, this, tracker, std::move(texture));
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageVideoSurfaceTexture::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_state);

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;

  if (!context_state->GrContextIsGL()) {
    DCHECK(false);
    return nullptr;
  }

  DCHECK(context_state->GrContextIsGL());
  auto* texture_base = stream_texture_sii_->GetTextureBase();
  DCHECK(texture_base);
  const bool passthrough =
      (texture_base->GetType() == gpu::TextureBase::Type::kPassthrough);

  auto texture = GenAbstractTexture(passthrough);
  if (!texture)
    return nullptr;

  // If TextureOwner binds texture implicitly on update, that means it will
  // use TextureOwner texture_id to update and bind. Hence use TextureOwner
  // texture_id in abstract texture via BindStreamTextureImage().
  DCHECK(stream_texture_sii_->TextureOwnerBindsTextureOnUpdate());
  texture->BindStreamTextureImage(
      stream_texture_sii_.get(),
      stream_texture_sii_->GetTextureBase()->service_id());

  std::unique_ptr<gpu::SharedImageRepresentationGLTextureBase>
      gl_representation;
  if (passthrough) {
    gl_representation =
        std::make_unique<SharedImageRepresentationGLTexturePassthroughVideo>(
            manager, this, tracker, std::move(texture));
  } else {
    gl_representation =
        std::make_unique<SharedImageRepresentationGLTextureVideo>(
            manager, this, tracker, std::move(texture));
  }
  return SharedImageRepresentationSkiaGL::Create(std::move(gl_representation),
                                                 std::move(context_state),
                                                 manager, this, tracker);
}

void SharedImageVideoSurfaceTexture::BeginGLReadAccess(
    const GLuint service_id) {
  stream_texture_sii_->UpdateAndBindTexImage(service_id);
}

// Representation of SharedImageVideoSurfaceTexture as an overlay plane.
class SharedImageVideoSurfaceTexture::SharedImageRepresentationOverlayVideo
    : public gpu::SharedImageRepresentationLegacyOverlay {
 public:
  SharedImageRepresentationOverlayVideo(gpu::SharedImageManager* manager,
                                        SharedImageVideoSurfaceTexture* backing,
                                        gpu::MemoryTypeTracker* tracker)
      : gpu::SharedImageRepresentationLegacyOverlay(manager, backing, tracker) {
  }

  // Disallow copy and assign.
  SharedImageRepresentationOverlayVideo(
      const SharedImageRepresentationOverlayVideo&) = delete;
  SharedImageRepresentationOverlayVideo& operator=(
      const SharedImageRepresentationOverlayVideo&) = delete;

 protected:
  void RenderToOverlay() override {
    DCHECK(!stream_image()->HasTextureOwner())
        << "CodecImage must be already in overlay";
    TRACE_EVENT0("media",
                 "SharedImageRepresentationOverlayVideo::RenderToOverlay");
    stream_image()->RenderToOverlay();
  }

  void NotifyOverlayPromotion(bool promotion,
                              const gfx::Rect& bounds) override {
    stream_image()->NotifyOverlayPromotion(promotion, bounds);
  }

 private:
  StreamTextureSharedImageInterface* stream_image() {
    auto* video_backing =
        static_cast<SharedImageVideoSurfaceTexture*>(backing());
    DCHECK(video_backing);
    return video_backing->stream_texture_sii_.get();
  }
};

std::unique_ptr<gpu::SharedImageRepresentationLegacyOverlay>
SharedImageVideoSurfaceTexture::ProduceLegacyOverlay(
    gpu::SharedImageManager* manager,
    gpu::MemoryTypeTracker* tracker) {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  return std::make_unique<SharedImageRepresentationOverlayVideo>(manager, this,
                                                                 tracker);
}

}  // namespace gpu
