// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_IMAGE_H_
#define MEDIA_GPU_ANDROID_CODEC_IMAGE_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/promotion_hint_aggregator.h"
#include "media/gpu/android/surface_texture_gl_owner.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// A GLImage that renders MediaCodec buffers to a TextureOwner or overlay
// as needed in order to draw them.
class MEDIA_GPU_EXPORT CodecImage : public gpu::gles2::GLStreamTextureImage {
 public:
  // A callback for observing CodecImage destruction.  This is a repeating cb
  // since CodecImageGroup calls the same cb for multiple images.
  using DestructionCb = base::RepeatingCallback<void(CodecImage*)>;

  CodecImage(std::unique_ptr<CodecOutputBuffer> output_buffer,
             scoped_refptr<TextureOwner> texture_owner,
             PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb);

  void SetDestructionCb(DestructionCb destruction_cb);

  // gl::GLImage implementation
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
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
  std::unique_ptr<ScopedHardwareBuffer> GetAHardwareBuffer() override;
  // gpu::gles2::GLStreamTextureMatrix implementation
  void GetTextureMatrix(float xform[16]) override;
  void NotifyPromotionHint(bool promotion_hint,
                           int display_x,
                           int display_y,
                           int display_width,
                           int display_height) override;

  // Whether the codec buffer has been rendered to the front buffer.
  bool was_rendered_to_front_buffer() const {
    return phase_ == Phase::kInFrontBuffer;
  }

  // Whether this image is backed by a texture owner.
  bool is_texture_owner_backed() const { return !!texture_owner_; }

  scoped_refptr<TextureOwner> texture_owner() const { return texture_owner_; }

  // Renders this image to the front buffer of its backing surface.
  // Returns true if the buffer is in the front buffer. Returns false if the
  // buffer was invalidated. After an image is invalidated it's no longer
  // possible to render it.
  bool RenderToFrontBuffer();

  // Renders this image to the back buffer of its texture owner. Only valid if
  // is_texture_owner_backed(). Returns true if the buffer is in the back
  // buffer. Returns false if the buffer was invalidated.
  bool RenderToTextureOwnerBackBuffer();

  // Release any codec buffer without rendering, if we have one.
  virtual void ReleaseCodecBuffer();

 protected:
  ~CodecImage() override;

 private:
  // The lifecycle phases of an image.
  // The only possible transitions are from left to right. Both
  // kInFrontBuffer and kInvalidated are terminal.
  enum class Phase { kInCodec, kInBackBuffer, kInFrontBuffer, kInvalidated };

  // Renders this image to the texture owner front buffer by first rendering
  // it to the back buffer if it's not already there, and then waiting for the
  // frame available event before calling UpdateTexImage(). Passing
  // BindingsMode::kDontRestore skips the work of restoring the current texture
  // bindings if the texture owner's context is already current. Otherwise,
  // this switches contexts and preserves the texture bindings.
  // Returns true if the buffer is in the front buffer. Returns false if the
  // buffer was invalidated.
  enum class BindingsMode { kRestore, kDontRestore };
  bool RenderToTextureOwnerFrontBuffer(BindingsMode bindings_mode);

  // Renders this image to the overlay. Returns true if the buffer is in the
  // overlay front buffer. Returns false if the buffer was invalidated.
  bool RenderToOverlay();

  // The phase of the image buffer's lifecycle.
  Phase phase_;

  // The buffer backing this image.
  std::unique_ptr<CodecOutputBuffer> output_buffer_;

  // The TextureOwner that |output_buffer_| will be rendered to. Or null, if
  // this image is backed by an overlay.
  scoped_refptr<TextureOwner> texture_owner_;

  // The bounds last sent to the overlay.
  gfx::Rect most_recent_bounds_;

  // Callback to notify about promotion hints and overlay position.
  PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb_;

  DestructionCb destruction_cb_;

  DISALLOW_COPY_AND_ASSIGN(CodecImage);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CODEC_IMAGE_H_
