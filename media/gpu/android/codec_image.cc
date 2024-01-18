// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_image.h"

#include <string.h>

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

CodecImage::CodecImage(const gfx::Size& coded_size,
                       scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : RefCountedLockHelperDrDc(std::move(drdc_lock)), coded_size_(coded_size) {}

CodecImage::~CodecImage() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  NotifyUnused();
}

void CodecImage::Initialize(
    std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
    bool is_texture_owner_backed,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb) {
  DCHECK(output_buffer_renderer);
  output_buffer_renderer_ = std::move(output_buffer_renderer);
  is_texture_owner_backed_ = is_texture_owner_backed;
  promotion_hint_cb_ = std::move(promotion_hint_cb);
}

void CodecImage::AddUnusedCB(UnusedCB unused_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  unused_cbs_.push_back(std::move(unused_cb));
}

void CodecImage::NotifyUnused() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();

  // If we haven't done so yet, release the codec output buffer.  Also drop
  // our reference to the TextureOwner (if any).  In other words, undo anything
  // that we did in Initialize.
  ReleaseCodecBuffer();
  promotion_hint_cb_ = base::NullCallback();

  for (auto& cb : unused_cbs_)
    std::move(cb).Run(this);
  unused_cbs_.clear();
}

void CodecImage::NotifyOverlayPromotion(bool promotion,
                                        const gfx::Rect& bounds) {
  AssertAcquiredDrDcLock();
  // Use-after-release.  It happens if the renderer crashes before getting
  // returns from viz.
  if (!promotion_hint_cb_)
    return;

  if (!is_texture_owner_backed_ && promotion) {
    // When |CodecImage| is already backed by SurfaceView, and it should be used
    // as overlay.

    // Move the overlay if needed.
    if (most_recent_bounds_ != bounds) {
      most_recent_bounds_ = bounds;
      // Note that, if we're actually promoted to overlay, that this is where
      // the hint is sent to the callback.  NotifyPromotionHint detects this
      // case and lets us do it.  If we knew that we were going to get promotion
      // hints, then we could always let NotifyPromotionHint do it.
      // Unfortunately, we don't know that.
      promotion_hint_cb_.Run(PromotionHintAggregator::Hint(bounds, promotion));
    }
  } else {
    // This could be when |CodecImage| is backed by SurfaceTexture but should be
    // promoted, or when this is backed by either SurfaceView or SurfaceTexture
    // but should not be promoted.
    promotion_hint_cb_.Run(PromotionHintAggregator::Hint(bounds, promotion));
  }
}

void CodecImage::ReleaseResources() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  ReleaseCodecBuffer();
}

void CodecImage::UpdateAndBindTexImage() {
  AssertAcquiredDrDcLock();
  RenderToTextureOwnerFrontBuffer();
}

bool CodecImage::HasTextureOwner() const {
  return !!texture_owner();
}

gpu::TextureBase* CodecImage::GetTextureBase() const {
  return texture_owner()->GetTextureBase();
}

bool CodecImage::RenderToFrontBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToFrontBuffer();
}

bool CodecImage::RenderToTextureOwnerBackBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;

  return output_buffer_renderer_->RenderToTextureOwnerBackBuffer();
}

bool CodecImage::RenderToTextureOwnerFrontBuffer() {
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToTextureOwnerFrontBuffer();
}

bool CodecImage::RenderToOverlay() {
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToOverlay();
}

bool CodecImage::TextureOwnerBindsTextureOnUpdate() {
  return const_cast<const CodecImage*>(this)->TextureOwnerBindsOnUpdate();
}

bool CodecImage::TextureOwnerBindsOnUpdate() const {
  AssertAcquiredDrDcLock();
  return texture_owner() ? texture_owner()->binds_texture_on_update() : false;
}

void CodecImage::ReleaseCodecBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  output_buffer_renderer_.reset();
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
CodecImage::GetAHardwareBuffer() {
  AssertAcquiredDrDcLock();

  // It would be nice if this didn't happen, but we can be incorrectly marked
  // as free when viz is still using us for drawing.  This can happen if the
  // renderer crashes before receiving returns.  It's hard to catch elsewhere,
  // so just handle it gracefully here.
  if (!output_buffer_renderer_)
    return nullptr;

  // Render to the front buffer to get the AHardwareBuffer from the latest
  // image.
  RenderToTextureOwnerFrontBuffer();
  return output_buffer_renderer_->texture_owner()->GetAHardwareBuffer();
}

CodecImageHolder::CodecImageHolder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<CodecImage> codec_image,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : base::RefCountedDeleteOnSequence<CodecImageHolder>(
          std::move(task_runner)),
      gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)),
      codec_image_(std::move(codec_image)) {}

CodecImageHolder::~CodecImageHolder() {
  // Note that CodecImageHolder is always destroyed on the thread it was created
  // on which is gpu main thread. CodecImage destructor also has checks to
  // ensure that it is destroyed on gpu main thread.
  // Acquiring DrDc lock here to ensure that the lock is held from all the paths
  // from where |codec_image_| can be destroyed.
  {
    auto scoped_lock = GetScopedDrDcLock();
    codec_image_.reset();
  }
}

}  // namespace media
