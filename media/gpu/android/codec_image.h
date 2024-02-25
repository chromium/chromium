// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_IMAGE_H_
#define MEDIA_GPU_ANDROID_CODEC_IMAGE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/task/sequenced_task_runner.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "media/gpu/android/codec_output_buffer_renderer.h"
#include "media/gpu/android/promotion_hint_aggregator.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace media {

// A StreamTextureSharedImageInterface implementation that renders MediaCodec
// buffers to a TextureOwner or overlay as needed in order to draw them. Note
// that when DrDc is enabled(kEnableDrDc), a per codec dr-dc lock is expected to
// be held while calling methods of this class. This is ensured by adding
// AssertAcquiredDrDcLock() to those methods.  We are not adding a Locked suffix
// on those methods since many of those methods are either overrides or virtual.
class MEDIA_GPU_EXPORT CodecImage
    : public gpu::StreamTextureSharedImageInterface,
      gpu::RefCountedLockHelperDrDc {
 public:
  // Callback to notify that a codec image is now unused in the sense of not
  // being out for display.  This lets us signal interested folks once a video
  // frame is destroyed and the sync token clears, so that that CodecImage may
  // be re-used.  Once legacy mailboxes go away, AndroidVideoImageBacking can
  // manage all of this instead.
  //
  // Also note that, presently, only destruction does this.  However, with
  // pooling, there will be a way to mark a CodecImage as unused without
  // destroying it.
  using UnusedCB = base::OnceCallback<void(CodecImage*)>;

  CodecImage(const gfx::Size& coded_size,
             scoped_refptr<gpu::RefCountedLock> drdc_lock);

  CodecImage(const CodecImage&) = delete;
  CodecImage& operator=(const CodecImage&) = delete;

  // (Re-)Initialize this CodecImage to use |output_buffer| et. al.
  //
  // May be called on a random thread, but only if the CodecImage is otherwise
  // not in use.
  void Initialize(
      std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
      bool is_texture_owner_backed,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb);

  // Add a callback that will be called when we're marked as unused.  Does not
  // replace previous callbacks.  Order of callbacks is not guaranteed.
  void AddUnusedCB(UnusedCB unused_cb);

  // Notify us that we're no longer in-use for display, and may be pointed at
  // another output buffer via a call to Initialize.
  void NotifyUnused();

  // gpu::StreamTextureSharedImageInterface implementation.
  void ReleaseResources() override;
  void UpdateAndBindTexImage() override;
  bool HasTextureOwner() const override;
  gpu::TextureBase* GetTextureBase() const override;
  void NotifyOverlayPromotion(bool promotion, const gfx::Rect& bounds) override;
  // Renders this image to the overlay. Returns true if the buffer is in the
  // overlay front buffer. Returns false if the buffer was invalidated.
  bool RenderToOverlay() override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;
  bool TextureOwnerBindsTextureOnUpdate() override;

  // Whether the codec buffer has been rendered to the front buffer.
  bool was_rendered_to_front_buffer() const {
    return output_buffer_renderer_
               ? output_buffer_renderer_->was_rendered_to_front_buffer()
               : false;
  }

  // Whether this image is backed by a texture owner.
  bool is_texture_owner_backed() const { return is_texture_owner_backed_; }

  scoped_refptr<gpu::TextureOwner> texture_owner() const {
    return output_buffer_renderer_ ? output_buffer_renderer_->texture_owner()
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
  // RenderToTextureOwnerBackBuffer() will not block if there is any previously
  // pending frame and will return false in this case.
  bool RenderToTextureOwnerBackBuffer();

  // Release any codec buffer without rendering, if we have one.
  virtual void ReleaseCodecBuffer();

  CodecOutputBuffer* get_codec_output_buffer_for_testing() const {
    return output_buffer_renderer_
               ? output_buffer_renderer_->get_codec_output_buffer_for_testing()
               : nullptr;
  }

 protected:
  ~CodecImage() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CodecImageTest, RenderAfterUnusedDoesntCrash);

  bool TextureOwnerBindsOnUpdate() const;

  std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer_;

  // Renders this image to the texture owner front buffer by first rendering
  // it to the back buffer if it's not already there, and then waiting for the
  // frame available event before calling UpdateTexImage().
  bool RenderToTextureOwnerFrontBuffer();

  // Whether this image is texture_owner or overlay backed.
  bool is_texture_owner_backed_ = false;

  // The bounds last sent to the overlay.
  gfx::Rect most_recent_bounds_;

  // Coded size of the image.
  gfx::Size coded_size_;

  // Callback to notify about promotion hints and overlay position.
  PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb_;

  std::vector<UnusedCB> unused_cbs_;

  // Bound to the gpu main thread on which this CodecImage is created. Some
  // methods can only be called on this thread.
  THREAD_CHECKER(gpu_main_thread_checker_);
};

// Temporary helper class to prevent touching a non-threadsafe-ref-counted
// CodecImage off the gpu main thread, while still holding a reference to it.
// Passing a raw pointer around isn't safe, since stub destruction could still
// destroy the consumers of the codec image.
class MEDIA_GPU_EXPORT CodecImageHolder
    : public base::RefCountedDeleteOnSequence<CodecImageHolder>,
      public gpu::RefCountedLockHelperDrDc {
 public:
  CodecImageHolder(scoped_refptr<base::SequencedTaskRunner> task_runner,
                   scoped_refptr<CodecImage> codec_image,
                   scoped_refptr<gpu::RefCountedLock> drdc_lock);

  CodecImageHolder(const CodecImageHolder&) = delete;
  CodecImageHolder& operator=(const CodecImageHolder&) = delete;

  // Safe from any thread.
  CodecImage* codec_image_raw() const { return codec_image_.get(); }

 private:
  virtual ~CodecImageHolder();

  friend class base::RefCountedDeleteOnSequence<CodecImageHolder>;
  friend class base::DeleteHelper<CodecImageHolder>;

  scoped_refptr<CodecImage> codec_image_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CODEC_IMAGE_H_
