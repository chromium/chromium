// Copyright 2020 The Chromium Authors
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

CodecOutputBufferRenderer::CodecOutputBufferRenderer(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : RefCountedLockHelperDrDc(std::move(drdc_lock)),
      output_buffer_(std::move(output_buffer)),
      codec_buffer_wait_coordinator_(std::move(codec_buffer_wait_coordinator)) {
}

CodecOutputBufferRenderer::~CodecOutputBufferRenderer() = default;

bool CodecOutputBufferRenderer::RenderToTextureOwnerBackBuffer() {
  AssertAcquiredDrDcLock();
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
  AssertAcquiredDrDcLock();
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
  // if |texture_owner| binds image on update, mark that we bound it.
  if (codec_buffer_wait_coordinator_->texture_owner()
          ->binds_texture_on_update()) {
    was_tex_image_bound_ = true;
  }

  EnsureBoundIfNeeded(bindings_mode, service_id);
  return true;
}

void CodecOutputBufferRenderer::EnsureBoundIfNeeded(BindingsMode mode,
                                                    GLuint service_id) {
  AssertAcquiredDrDcLock();
  DCHECK(codec_buffer_wait_coordinator_);

  if (mode == BindingsMode::kBindImage) {
    DCHECK_GT(service_id, 0u);
    codec_buffer_wait_coordinator_->texture_owner()->EnsureTexImageBound(
        service_id);
    was_tex_image_bound_ = true;
  }
}

bool CodecOutputBufferRenderer::RenderToOverlay() {
  AssertAcquiredDrDcLock();
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
  AssertAcquiredDrDcLock();

  // This code is used to trigger early rendering of the image before it is used
  // for compositing, there is no need to bind the image. Hence pass texture
  // service_id as 0.
  return codec_buffer_wait_coordinator_
             ? RenderToTextureOwnerFrontBuffer(BindingsMode::kDontBindImage,
                                               0 /* service_id */)
             : RenderToOverlay();
}

}  // namespace media
