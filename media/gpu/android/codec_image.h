// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_IMAGE_H_
#define MEDIA_GPU_ANDROID_CODEC_IMAGE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "media/gpu/android/codec_buffer_wait_coordinator.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/promotion_hint_aggregator.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace media {

// A GLImage that renders MediaCodec buffers to a TextureOwner or overlay
// as needed in order to draw them.
class MEDIA_GPU_EXPORT CodecImage
    : public gpu::StreamTextureSharedImageInterface {
 public:
  // Whether RenderToTextureOwnerBackBuffer may block or not.
  enum class BlockingMode { kForbidBlocking, kAllowBlocking };

  // Callback to notify that a codec image is now unused in the sense of not
  // being out for display.  This lets us signal interested folks once a video
  // frame is destroyed and the sync token clears, so that that CodecImage may
  // be re-used.  Once legacy mailboxes go away, SharedImageVideo can manage all
  // of this instead.
  //
  // Also note that, presently, only destruction does this.  However, with
  // pooling, there will be a way to mark a CodecImage as unused without
  // destroying it.
  using UnusedCB = base::OnceCallback<void(CodecImage*)>;

  CodecImage();

  // (Re-)Initialize this CodecImage to use |output_buffer| et. al.
  //
  // May be called on a random thread, but only if the CodecImage is otherwise
  // not in use.
  void Initialize(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb);

  // Add a callback that will be called when we're marked as unused.  Does not
  // replace previous callbacks.  Order of callbacks is not guaranteed.
  void AddUnusedCB(UnusedCB unused_cb);

  // gl::GLImage implementation
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  unsigned GetDataType() override;
  BindOrCopy ShouldBindOrCopy() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  // Currently this API is depended on the implementation of
  // NotifyOverlayPromotion. since we expect overlay to use SharedImage in the
  // future.
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  void SetColorSpace(const gfx::ColorSpace& color_space) override {}
  void Flush() override {}
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;
  // gpu::gles2::GLStreamTextureMatrix implementation
  void GetTextureMatrix(float xform[16]) override;
  // Currently this API is implemented by the NotifyOverlayPromotion, since this
  // API is expected to be removed.
  void NotifyPromotionHint(bool promotion_hint,
                           int display_x,
                           int display_y,
                           int display_width,
                           int display_height) override;
  // If we re-use one CodecImage with different output buffers, then we must
  // not claim to have mutable state.  Otherwise, CopyTexImage is only called
  // once.  For pooled shared images, this must return false.  For single-use
  // images, it works either way.
  bool HasMutableState() const override;

  // Notify us that we're no longer in-use for display, and may be pointed at
  // another output buffer via a call to Initialize.
  void NotifyUnused();

  // gpu::StreamTextureSharedImageInterface implementation.
  void ReleaseResources() override;
  bool IsUsingGpuMemory() const override;
  void UpdateAndBindTexImage() override;
  bool HasTextureOwner() const override;
  gpu::gles2::Texture* GetTexture() const override;
  void NotifyOverlayPromotion(bool promotion, const gfx::Rect& bounds) override;
  // Renders this image to the overlay. Returns true if the buffer is in the
  // overlay front buffer. Returns false if the buffer was invalidated.
  bool RenderToOverlay() override;

  // Whether the codec buffer has been rendered to the front buffer.
  bool was_rendered_to_front_buffer() const {
    return phase_ == Phase::kInFrontBuffer;
  }

  // Whether the TextureOwner's texture is in the front buffer and bound to the
  // latest image.
  bool was_tex_image_bound() const { return was_tex_image_bound_; }

  // Whether this image is backed by a texture owner.
  // We want to check for texture_owner owned by
  // |codec_buffer_wait_coordinator_| and hence only checking for
  // |codec_buffer_wait_coordinator_| is enough here.
  // TODO(vikassoni): Update the method name in future refactorings.
  bool is_texture_owner_backed() const {
    return !!codec_buffer_wait_coordinator_;
  }

  scoped_refptr<gpu::TextureOwner> texture_owner() const {
    return codec_buffer_wait_coordinator_
               ? codec_buffer_wait_coordinator_->texture_owner()
               : nullptr;
  }

  // Renders this image to the front buffer of its backing surface.
  // Returns true if the buffer is in the front buffer. Returns false if the
  // buffer was invalidated. After an image is invalidated it's no longer
  // possible to render it.
  bool RenderToFrontBuffer();

  // Renders this image to the back buffer of its texture owner. Only valid if
  // is_texture_owner_backed(). Returns true if the buffer is in the back
  // buffer. Returns false if the buffer was invalidated.
  // |blocking_mode| indicates whether this should (a) wait for any previously
  // pending rendered frame before rendering this one, or (b) fail if a wait
  // is required.
  bool RenderToTextureOwnerBackBuffer(
      BlockingMode blocking_mode = BlockingMode::kAllowBlocking);

  // Release any codec buffer without rendering, if we have one.
  virtual void ReleaseCodecBuffer();

  CodecOutputBuffer* get_codec_output_buffer_for_testing() const {
    return output_buffer_.get();
  }

 protected:
  ~CodecImage() override;

 private:
  // The lifecycle phases of an image.
  // The only possible transitions are from left to right. Both
  // kInFrontBuffer and kInvalidated are terminal.
  enum class Phase { kInCodec, kInBackBuffer, kInFrontBuffer, kInvalidated };

  // Renders this image to the texture owner front buffer by first rendering
  // it to the back buffer if it's not already there, and then waiting for the
  // frame available event before calling UpdateTexImage().
  bool RenderToTextureOwnerFrontBuffer(BindingsMode bindings_mode);
  void EnsureBoundIfNeeded(BindingsMode mode);

  // The phase of the image buffer's lifecycle.
  Phase phase_ = Phase::kInvalidated;

  // The buffer backing this image.
  std::unique_ptr<CodecOutputBuffer> output_buffer_;

  // The CodecBufferWaitCoordinator that |output_buffer_| will be rendered to.
  // Or null, if this image is backed by an overlay.
  scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator_;

  // The bounds last sent to the overlay.
  gfx::Rect most_recent_bounds_;

  // Callback to notify about promotion hints and overlay position.
  PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb_;

  std::vector<UnusedCB> unused_cbs_;

  bool was_tex_image_bound_ = false;

  DISALLOW_COPY_AND_ASSIGN(CodecImage);
};

// Temporary helper class to prevent touching a non-threadsafe-ref-counted
// CodecImage off the gpu main thread, while still holding a reference to it.
// Passing a raw pointer around isn't safe, since stub destruction could still
// destroy the consumers of the codec image.
class MEDIA_GPU_EXPORT CodecImageHolder
    : public base::RefCountedDeleteOnSequence<CodecImageHolder> {
 public:
  CodecImageHolder(scoped_refptr<base::SequencedTaskRunner> task_runner,
                   scoped_refptr<CodecImage> codec_image);

  // Safe from any thread.
  CodecImage* codec_image_raw() const { return codec_image_.get(); }

 private:
  virtual ~CodecImageHolder();

  friend class base::RefCountedDeleteOnSequence<CodecImageHolder>;
  friend class base::DeleteHelper<CodecImageHolder>;

  scoped_refptr<CodecImage> codec_image_;

  DISALLOW_COPY_AND_ASSIGN(CodecImageHolder);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CODEC_IMAGE_H_
