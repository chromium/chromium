// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_output_buffer_renderer.h"
#include <string.h>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/callback_helpers.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

namespace {

// Makes |texture_owner|'s context current if it isn't already.
std::unique_ptr<ui::ScopedMakeCurrent> MakeCurrentIfNeeded(
    gpu::TextureOwner* texture_owner) {
  gl::GLContext* context = texture_owner->GetContext();
  // Note: this works for virtual contexts too, because IsCurrent() returns true
  // if their shared platform context is current, regardless of which virtual
  // context is current.
  if (context->IsCurrent(nullptr))
    return nullptr;

  auto scoped_current = std::make_unique<ui::ScopedMakeCurrent>(
      context, texture_owner->GetSurface());
  // Log an error if ScopedMakeCurrent failed for debugging
  // https://crbug.com/878042.
  // TODO(ericrk): Remove this once debugging is completed.
  if (!context->IsCurrent(nullptr)) {
    LOG(ERROR) << "Failed to make context current in CodecImage. Subsequent "
                  "UpdateTexImage may fail.";
  }
  return scoped_current;
}

}  // namespace

CodecOutputBufferRenderer::CodecOutputBufferRenderer(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator)
    : output_buffer_(std::move(output_buffer)),
      codec_buffer_wait_coordinator_(std::move(codec_buffer_wait_coordinator)) {

}

CodecOutputBufferRenderer::~CodecOutputBufferRenderer() = default;

bool CodecOutputBufferRenderer::RenderToTextureOwnerBackBuffer() {
  DCHECK_NE(phase_, Phase::kInFrontBuffer);
  if (phase_ == Phase::kInBackBuffer)
    return true;
  if (phase_ == Phase::kInvalidated)
    return false;

  // Normally, we should have a wait coordinator if we're called.  However, if
  // the renderer is torn down (either VideoFrameSubmitter or the whole process)
  // before we get returns back from viz, then we can be notified that we're
  // no longer in use (erroneously) when the VideoFrame is destroyed.  So, if
  // we don't have a wait coordinator, then just fail.
  if (!codec_buffer_wait_coordinator_)
    return false;

  // Don't render frame if one is already pending.
  // RenderToTextureOwnerFrontBuffer will wait before calling this.
  if (codec_buffer_wait_coordinator_->IsExpectingFrameAvailable()) {
    return false;
  }
  if (!output_buffer_->ReleaseToSurface()) {
    phase_ = Phase::kInvalidated;
    return false;
  }
  phase_ = Phase::kInBackBuffer;
  codec_buffer_wait_coordinator_->SetReleaseTimeToNow();
  return true;
}

bool CodecOutputBufferRenderer::RenderToTextureOwnerFrontBuffer(
    BindingsMode bindings_mode,
    GLuint service_id) {
  // Normally, we should have a wait coordinator if we're called.  However, if
  // the renderer is torn down (either VideoFrameSubmitter or the whole process)
  // before we get returns back from viz, then we can be notified that we're
  // no longer in use (erroneously) when the VideoFrame is destroyed.  So, if
  // we don't have a wait coordinator, then just fail.
  if (!codec_buffer_wait_coordinator_)
    return false;

  if (phase_ == Phase::kInFrontBuffer) {
    EnsureBoundIfNeeded(bindings_mode, service_id);
    return true;
  }
  if (phase_ == Phase::kInvalidated)
    return false;

  std::unique_ptr<ui::ScopedMakeCurrent> scoped_make_current;
  absl::optional<gpu::ScopedRestoreTextureBinding> scoped_restore_texture;

  if (codec_buffer_wait_coordinator_->texture_owner()
          ->binds_texture_on_update()) {
    // If the texture_owner() binds the texture while doing the texture update
    // (UpdateTexImage), like in SurfaceTexture case, then make sure that the
    // texture owner's context is made current. This is because the texture
    // which will be bound was generated on TextureOwner's context.
    // For AImageReader case, the texture which will be bound will not
    // necessarily be TextureOwner's texture and hence caller is responsible to
    // handle making correct context current before binding the texture.
    scoped_make_current = MakeCurrentIfNeeded(
        codec_buffer_wait_coordinator_->texture_owner().get());

    // If updating the image will implicitly update the texture bindings then
    // restore if requested or the update needed a context switch.
    if (bindings_mode == BindingsMode::kRestoreIfBound ||
        !!scoped_make_current) {
      scoped_restore_texture.emplace();
    }
  }

  // Render it to the back buffer if it's not already there.
  if (phase_ != Phase::kInBackBuffer) {
    // Wait for a previous frame available so we don't confuse it with the one
    // we're about to render.
    if (codec_buffer_wait_coordinator_->IsExpectingFrameAvailable()) {
      codec_buffer_wait_coordinator_->WaitForFrameAvailable();

      // We must call update tex image if we did get OnFrameAvailable, otherwise
      // we will stop receiving callbacks (see https://crbug.com/c/1113203)
      codec_buffer_wait_coordinator_->texture_owner()->UpdateTexImage();
    }
    if (!RenderToTextureOwnerBackBuffer()) {
      // RenderTotextureOwnerBackBuffer can fail now only if ReleaseToSurface
      // failed.
      DCHECK(phase_ == Phase::kInvalidated);
      return false;
    }
  }

  // The image is now in the back buffer, so promote it to the front buffer.
  phase_ = Phase::kInFrontBuffer;
  if (codec_buffer_wait_coordinator_->IsExpectingFrameAvailable())
    codec_buffer_wait_coordinator_->WaitForFrameAvailable();

  codec_buffer_wait_coordinator_->texture_owner()->UpdateTexImage();
  EnsureBoundIfNeeded(bindings_mode, service_id);
  return true;
}

void CodecOutputBufferRenderer::EnsureBoundIfNeeded(BindingsMode mode,
                                                    GLuint service_id) {
  DCHECK(codec_buffer_wait_coordinator_);

  if (codec_buffer_wait_coordinator_->texture_owner()
          ->binds_texture_on_update()) {
    if (mode == BindingsMode::kEnsureTexImageBound) {
      DCHECK_EQ(
          service_id,
          codec_buffer_wait_coordinator_->texture_owner()->GetTextureId());
    }
    was_tex_image_bound_ = true;
    return;
  }
  if (mode != BindingsMode::kEnsureTexImageBound)
    return;

  DCHECK_GT(service_id, 0u);
  codec_buffer_wait_coordinator_->texture_owner()->EnsureTexImageBound(
      service_id);
  was_tex_image_bound_ = true;
}

bool CodecOutputBufferRenderer::RenderToOverlay() {
  if (phase_ == Phase::kInFrontBuffer)
    return true;
  if (phase_ == Phase::kInvalidated)
    return false;

  if (!output_buffer_->ReleaseToSurface()) {
    phase_ = Phase::kInvalidated;
    return false;
  }
  phase_ = Phase::kInFrontBuffer;
  return true;
}

bool CodecOutputBufferRenderer::RenderToFrontBuffer() {
  // This code is used to trigger early rendering of the image before it is used
  // for compositing, there is no need to bind the image. Hence pass texture
  // service_id as 0.
  return codec_buffer_wait_coordinator_
             ? RenderToTextureOwnerFrontBuffer(BindingsMode::kRestoreIfBound,
                                               0 /* service_id */)
             : RenderToOverlay();
}

}  // namespace media
