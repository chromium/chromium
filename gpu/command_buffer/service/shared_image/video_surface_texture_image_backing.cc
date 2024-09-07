// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/video_surface_texture_image_backing.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

VideoSurfaceTextureImageBacking::VideoSurfaceTextureImageBacking(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    std::string debug_label,
    scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
    scoped_refptr<SharedContextState> context_state)
    : AndroidVideoImageBacking(mailbox,
                               size,
                               color_space,
                               surface_origin,
                               alpha_type,
                               std::move(debug_label),
                               /*is_thread_safe=*/false),
      stream_texture_sii_(std::move(stream_texture_sii)),
      context_state_(std::move(context_state)),
      gpu_main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(stream_texture_sii_);
  DCHECK(context_state_);

  context_state_->AddContextLostObserver(this);
}

VideoSurfaceTextureImageBacking::~VideoSurfaceTextureImageBacking() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  if (context_state_)
    context_state_->RemoveContextLostObserver(this);
  context_state_.reset();
  stream_texture_sii_->ReleaseResources();
  stream_texture_sii_.reset();
}

void VideoSurfaceTextureImageBacking::OnContextLost() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  // We release codec buffers when shared image context is lost. This is
  // because texture owner's texture was created on shared context. Once
  // shared context is lost, no one should try to use that texture.
  stream_texture_sii_->ReleaseResources();
  context_state_->RemoveContextLostObserver(this);
  context_state_ = nullptr;
}

// Representation of VideoSurfaceTextureImageBacking as a GL Texture.
class VideoSurfaceTextureImageBacking::GLTextureVideoImageRepresentation
    : public GLTextureImageRepresentation {
 public:
  GLTextureVideoImageRepresentation(
      SharedImageManager* manager,
      VideoSurfaceTextureImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<AbstractTextureAndroid> texture)
      : GLTextureImageRepresentation(manager, backing, tracker),
        texture_(std::move(texture)) {}

  ~GLTextureVideoImageRepresentation() override {
    if (!has_context()) {
      texture_->NotifyOnContextLost();
    }
  }

  // Disallow copy and assign.
  GLTextureVideoImageRepresentation(const GLTextureVideoImageRepresentation&) =
      delete;
  GLTextureVideoImageRepresentation& operator=(
      const GLTextureVideoImageRepresentation&) = delete;

  gles2::Texture* GetTexture(int plane_index) override {
    DCHECK_EQ(plane_index, 0);

    auto* texture = gles2::Texture::CheckedCast(texture_->GetTextureBase());
    DCHECK(texture);

    return texture;
  }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    auto* video_backing =
        static_cast<VideoSurfaceTextureImageBacking*>(backing());
    video_backing->BeginGLReadAccess(texture_->service_id());

    return true;
  }

  void EndAccess() override {}

 private:
  std::unique_ptr<AbstractTextureAndroid> texture_;
};

// Representation of VideoSurfaceTextureImageBacking as a GL Texture.
class VideoSurfaceTextureImageBacking::
    GLTexturePassthroughVideoImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTexturePassthroughVideoImageRepresentation(
      SharedImageManager* manager,
      VideoSurfaceTextureImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<AbstractTextureAndroid> abstract_texture)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        abstract_texture_(std::move(abstract_texture)),
        passthrough_texture_(gles2::TexturePassthrough::CheckedCast(
            abstract_texture_->GetTextureBase())) {
    // TODO(crbug.com/40166788): Remove this CHECK.
    CHECK(passthrough_texture_);
  }

  ~GLTexturePassthroughVideoImageRepresentation() override {
    if (!has_context()) {
      abstract_texture_->NotifyOnContextLost();
    }
  }

  // Disallow copy and assign.
  GLTexturePassthroughVideoImageRepresentation(
      const GLTexturePassthroughVideoImageRepresentation&) = delete;
  GLTexturePassthroughVideoImageRepresentation& operator=(
      const GLTexturePassthroughVideoImageRepresentation&) = delete;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override {
    DCHECK_EQ(plane_index, 0);
    return passthrough_texture_;
  }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    auto* video_backing =
        static_cast<VideoSurfaceTextureImageBacking*>(backing());
    video_backing->BeginGLReadAccess(passthrough_texture_->service_id());

    return true;
  }

  void EndAccess() override {}

 private:
  std::unique_ptr<AbstractTextureAndroid> abstract_texture_;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;
};

std::unique_ptr<GLTextureImageRepresentation>
VideoSurfaceTextureImageBacking::ProduceGLTexture(SharedImageManager* manager,
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
  // texture_id in abstract texture via BindToServiceId().
  DCHECK(stream_texture_sii_->TextureOwnerBindsTextureOnUpdate());
  texture->BindToServiceId(stream_texture_sii_->GetTextureBase()->service_id());

  return std::make_unique<GLTextureVideoImageRepresentation>(
      manager, this, tracker, std::move(texture));
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
VideoSurfaceTextureImageBacking::ProduceGLTexturePassthrough(
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
  // texture_id in abstract texture via BindToServiceId().
  DCHECK(stream_texture_sii_->TextureOwnerBindsTextureOnUpdate());
  texture->BindToServiceId(stream_texture_sii_->GetTextureBase()->service_id());

  return std::make_unique<GLTexturePassthroughVideoImageRepresentation>(
      manager, this, tracker, std::move(texture));
}

std::unique_ptr<SkiaGaneshImageRepresentation>
VideoSurfaceTextureImageBacking::ProduceSkiaGanesh(
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
  // texture_id in abstract texture via BindToServiceId().
  DCHECK(stream_texture_sii_->TextureOwnerBindsTextureOnUpdate());
  texture->BindToServiceId(stream_texture_sii_->GetTextureBase()->service_id());

  std::unique_ptr<gpu::GLTextureImageRepresentationBase> gl_representation;
  if (passthrough) {
    gl_representation =
        std::make_unique<GLTexturePassthroughVideoImageRepresentation>(
            manager, this, tracker, std::move(texture));
  } else {
    gl_representation = std::make_unique<GLTextureVideoImageRepresentation>(
        manager, this, tracker, std::move(texture));
  }
  return SkiaGLImageRepresentation::Create(std::move(gl_representation),
                                           std::move(context_state), manager,
                                           this, tracker);
}

void VideoSurfaceTextureImageBacking::BeginGLReadAccess(
    const GLuint service_id) {
  stream_texture_sii_->UpdateAndBindTexImage();
}

// Representation of VideoSurfaceTextureImageBacking as an overlay plane.
class VideoSurfaceTextureImageBacking::OverlayVideoImageRepresentation
    : public gpu::LegacyOverlayImageRepresentation {
 public:
  OverlayVideoImageRepresentation(gpu::SharedImageManager* manager,
                                  VideoSurfaceTextureImageBacking* backing,
                                  gpu::MemoryTypeTracker* tracker)
      : gpu::LegacyOverlayImageRepresentation(manager, backing, tracker) {}

  // Disallow copy and assign.
  OverlayVideoImageRepresentation(const OverlayVideoImageRepresentation&) =
      delete;
  OverlayVideoImageRepresentation& operator=(
      const OverlayVideoImageRepresentation&) = delete;

 protected:
  void RenderToOverlay() override {
    DCHECK(!stream_image()->HasTextureOwner())
        << "CodecImage must be already in overlay";
    TRACE_EVENT0("media", "OverlayVideoImageRepresentation::RenderToOverlay");
    stream_image()->RenderToOverlay();
  }

  void NotifyOverlayPromotion(bool promotion,
                              const gfx::Rect& bounds) override {
    stream_image()->NotifyOverlayPromotion(promotion, bounds);
  }

 private:
  StreamTextureSharedImageInterface* stream_image() {
    auto* video_backing =
        static_cast<VideoSurfaceTextureImageBacking*>(backing());
    DCHECK(video_backing);
    return video_backing->stream_texture_sii_.get();
  }
};

std::unique_ptr<gpu::LegacyOverlayImageRepresentation>
VideoSurfaceTextureImageBacking::ProduceLegacyOverlay(
    gpu::SharedImageManager* manager,
    gpu::MemoryTypeTracker* tracker) {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  return std::make_unique<OverlayVideoImageRepresentation>(manager, this,
                                                           tracker);
}

}  // namespace gpu
