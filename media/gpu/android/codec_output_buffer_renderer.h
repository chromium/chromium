// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_OUTPUT_BUFFER_RENDERER_H_
#define MEDIA_GPU_ANDROID_CODEC_OUTPUT_BUFFER_RENDERER_H_

#include <stdint.h>

#include <memory>

#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "media/gpu/android/codec_buffer_wait_coordinator.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// A class that holds CodecOutputBuffer and renders it to TextureOwner or
// overlay as necessary. Unit tests for this class are part of CodecImage unit
// tests. Note that when DrDc is enabled(kEnableDrDc),
// a per codec dr-dc lock is expected to be held while calling methods of this
// class. This is ensured by adding AssertAcquiredDrDcLock() to those methods.
class MEDIA_GPU_EXPORT CodecOutputBufferRenderer
    : public gpu::RefCountedLockHelperDrDc {
 public:
  using BindingsMode = gpu::StreamTextureSharedImageInterface::BindingsMode;

  CodecOutputBufferRenderer(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator,
      scoped_refptr<gpu::RefCountedLock> drdc_lock);
  ~CodecOutputBufferRenderer();

  CodecOutputBufferRenderer(const CodecOutputBufferRenderer&) = delete;
  CodecOutputBufferRenderer& operator=(const CodecOutputBufferRenderer&) =
      delete;

  // Renders this image to the overlay. Returns true if the buffer is in the
  // overlay front buffer. Returns false if the buffer was invalidated.
  bool RenderToOverlay();

  // Renders this image to the texture owner front buffer by first rendering
  // it to the back buffer if it's not already there, and then waiting for the
  // frame available event before calling UpdateTexImage().
  // Also bind the latest imagecto the provided |service_id| if TextureOwner
  // does not binds texture on update. If |bindings_mode| is other than
  // kEnsureTexImageBound, then |service_id| is not required.
  bool RenderToTextureOwnerFrontBuffer(BindingsMode bindings_mode,
                                       GLuint service_id);

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

  // Whether the codec buffer has been rendered to the front buffer.
  bool was_rendered_to_front_buffer() const {
    AssertAcquiredDrDcLock();
    return phase_ == Phase::kInFrontBuffer;
  }

  gfx::Size size() const { return output_buffer_->size(); }

  // Color space of the image.
  const gfx::ColorSpace& color_space() const {
    return output_buffer_->color_space();
  }

  bool was_tex_image_bound() const { return was_tex_image_bound_; }

  scoped_refptr<gpu::TextureOwner> texture_owner() const {
    return codec_buffer_wait_coordinator_
               ? codec_buffer_wait_coordinator_->texture_owner()
               : nullptr;
  }

  CodecOutputBuffer* get_codec_output_buffer_for_testing() const {
    return output_buffer_.get();
  }

 private:
  friend class FrameInfoHelperTest;
  // The lifecycle phases of an buffer.
  // The only possible transitions are from left to right. Both
  // kInFrontBuffer and kInvalidated are terminal.
  enum class Phase { kInCodec, kInBackBuffer, kInFrontBuffer, kInvalidated };

  // Ensure that the latest image is bound to the texture |service_id| if
  // TextureOwner does not binds texture on update. If TextureOwner binds
  // texture on update, then it will always be bound to the TextureOwners
  // texture and |service_id| will be ignored.
  void EnsureBoundIfNeeded(BindingsMode mode, GLuint service_id);

  void set_phase_for_testing(Phase phase) { phase_ = phase; }

  // The phase of the image buffer's lifecycle.
  Phase phase_ = Phase::kInCodec;

  // The buffer backing this image.
  std::unique_ptr<CodecOutputBuffer> output_buffer_;

  // The CodecBufferWaitCoordinator that |output_buffer_| will be rendered to.
  // Or null, if this image is backed by an overlay.
  scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator_;

  bool was_tex_image_bound_ = false;
};

}  // namespace media
#endif  // MEDIA_GPU_ANDROID_CODEC_OUTPUT_BUFFER_RENDERER_H_
