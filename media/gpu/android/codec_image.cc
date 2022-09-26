// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_image.h"

#include <string.h>

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/callback_helpers.h"
#include "base/debug/dump_without_crashing.h"
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

gfx::Size CodecImage::GetSize() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  return coded_size_;
}

unsigned CodecImage::GetInternalFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  return GL_RGBA;
}

unsigned CodecImage::GetDataType() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  return GL_UNSIGNED_BYTE;
}

CodecImage::BindOrCopy CodecImage::ShouldBindOrCopy() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);

  // If we're using an overlay, then pretend it's bound.  That way, we'll get
  // calls to ScheduleOverlayPlane.  Otherwise, CopyTexImage needs to be called.
  return is_texture_owner_backed_ ? COPY : BIND;
}

bool CodecImage::BindTexImage(unsigned target) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  DCHECK_EQ(BIND, ShouldBindOrCopy());
  return true;
}

void CodecImage::ReleaseTexImage(unsigned target) {}

bool CodecImage::CopyTexImage(unsigned target) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);

  // This method is only called for SurfaceTexture implementation which can't be
  // thread-safe. Hence the lock which ensures thread safety should be null.
  DCHECK(!GetDrDcLockPtr());

  TRACE_EVENT0("media", "CodecImage::CopyTexImage");
  DCHECK_EQ(COPY, ShouldBindOrCopy());

  if (target != GL_TEXTURE_EXTERNAL_OES)
    return false;

  if (!output_buffer_renderer_)
    return true;

  GLint texture_id = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &texture_id);

  // CopyTexImage will only be called for TextureOwner's SurfaceTexture
  // implementation which binds texture to TextureOwner's texture_id on update.
  DCHECK(output_buffer_renderer_->texture_owner()->binds_texture_on_update());
  if (texture_id > 0 &&
      static_cast<unsigned>(texture_id) !=
          output_buffer_renderer_->texture_owner()->GetTextureId()) {
    return false;
  }

  // Our hypothesis is that in actuality the rendering to the front buffer and
  // binding of the image, if possible, have always already occurred by the time
  // that this method is called. The below DumpWithoutCrashing() call serves to
  // verify whether this hypothesis is correct. See crbug.com/1310020 for
  // details.
  // TODO(crbug.com/1310020): Remove this code as part of removing this entire
  // function once we have verified that it is indeed no longer needed.
  if (!output_buffer_renderer_
           ->render_to_front_buffer_will_be_noop_for_debugging()) {
    base::debug::DumpWithoutCrashing();
  }

  // On some devices GL_TEXTURE_BINDING_EXTERNAL_OES is not supported as
  // glGetIntegerv() parameter. In this case the value of |texture_id| will be
  // zero and we assume that it is properly bound to TextureOwner's texture id.
  output_buffer_renderer_->RenderToTextureOwnerFrontBuffer(
      BindingsMode::kBindImage,
      output_buffer_renderer_->texture_owner()->GetTextureId());
  return true;
}

bool CodecImage::CopyTexSubImage(unsigned target,
                                 const gfx::Point& offset,
                                 const gfx::Rect& rect) {
  return false;
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

void CodecImage::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                              uint64_t process_tracing_id,
                              const std::string& dump_name) {}

void CodecImage::ReleaseResources() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  ReleaseCodecBuffer();
}

bool CodecImage::IsUsingGpuMemory() const {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;

  // Only the images which are bound to texture accounts for gpu memory.
  return output_buffer_renderer_->was_tex_image_bound();
}

void CodecImage::UpdateAndBindTexImage(GLuint service_id) {
  AssertAcquiredDrDcLock();
  RenderToTextureOwnerFrontBuffer(BindingsMode::kBindImage, service_id);
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

bool CodecImage::RenderToTextureOwnerFrontBuffer(BindingsMode bindings_mode,
                                                 GLuint service_id) {
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToTextureOwnerFrontBuffer(bindings_mode,
                                                                  service_id);
}

bool CodecImage::RenderToOverlay() {
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToOverlay();
}

bool CodecImage::TextureOwnerBindsTextureOnUpdate() {
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->texture_owner()->binds_texture_on_update();
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

  // Using BindingsMode::kDontBindImage here since we do not want to bind
  // the image. We just want to get the AHardwareBuffer from the latest image.
  // Hence pass service_id as 0.
  RenderToTextureOwnerFrontBuffer(BindingsMode::kDontBindImage,
                                  0 /* service_id */);
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
