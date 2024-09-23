// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_WRAPPER_H_
#define MEDIA_GPU_ANDROID_CODEC_WRAPPER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/decoder_buffer.h"
#include "media/base/status.h"
#include "media/gpu/android/codec_surface_bundle.h"
#include "media/gpu/android/device_info.h"
#include "media/gpu/media_gpu_export.h"

namespace media {
class CodecWrapper;
class CodecWrapperImpl;

using CodecSurfacePair = std::pair<std::unique_ptr<MediaCodecBridge>,
                                   scoped_refptr<CodecSurfaceBundle>>;

// A MediaCodec output buffer that can be released on any thread. Releasing a
// CodecOutputBuffer implicitly discards all CodecOutputBuffers that
// precede it in presentation order; i.e., the only supported use case is to
// render output buffers in order. This lets us return buffers to the codec as
// soon as we know we no longer need them.
class MEDIA_GPU_EXPORT CodecOutputBuffer {
 public:
  CodecOutputBuffer(const CodecOutputBuffer&) = delete;
  CodecOutputBuffer& operator=(const CodecOutputBuffer&) = delete;

  // Releases the buffer without rendering it.
  ~CodecOutputBuffer();

  // Releases this buffer and renders it to the surface.
  bool ReleaseToSurface();

  // The size of the image.
  gfx::Size size() const { return size_; }

  // Returns true if a coded size guess based on `size_` is available.
  bool CanGuessCodedSize() const;

  // Attempts to guess the coded size. `CanGuessCodedSize` must be true.
  gfx::Size GuessCodedSize() const;

  // Sets a callback that will be called when we're released to the surface.
  // Will not be called if we're dropped.
  void set_render_cb(base::OnceClosure render_cb) {
    render_cb_ = std::move(render_cb);
  }

  // Color space of the image.
  const gfx::ColorSpace& color_space() const { return color_space_; }

  // Note that you can't use the first ctor, since CodecWrapperImpl isn't
  // defined here.  Use the second, and it'll be nullptr.
  template <typename... Args>
  static std::unique_ptr<CodecOutputBuffer> CreateForTesting(Args&&... args) {
    // std::make_unique can't access the constructor.
    return std::unique_ptr<CodecOutputBuffer>(
        new CodecOutputBuffer(std::forward<Args>(args)...));
  }

 private:
  // Let CodecWrapperImpl call the constructor.
  friend class CodecWrapperImpl;
  CodecOutputBuffer(scoped_refptr<CodecWrapperImpl> codec,
                    int64_t id,
                    const gfx::Size& size,
                    const gfx::ColorSpace& color_space,
                    std::optional<gfx::Size> coded_size_alignment);

  // For testing, since CodecWrapperImpl isn't available.  Uses nullptr.
  CodecOutputBuffer(int64_t id,
                    const gfx::Size& size,
                    const gfx::ColorSpace& color_space,
                    std::optional<gfx::Size> coded_size_alignment);

  scoped_refptr<CodecWrapperImpl> codec_;
  int64_t id_;
  bool was_rendered_ = false;
  gfx::Size size_;
  base::OnceClosure render_cb_;
  gfx::ColorSpace color_space_;

  // The alignment to use for width, height when guessing coded size.
  const std::optional<gfx::Size> coded_size_alignment_;
};

// This wraps a MediaCodecBridge and provides higher level features and tracks
// more state that is useful for video decoding.
// CodecWrapper is not threadsafe, but the CodecOutputBuffers it outputs
// can be released on any thread.
class MEDIA_GPU_EXPORT CodecWrapper {
 public:
  // The given codec should be in the flushed state, i.e., freshly configured or
  // after a Flush(). The surface must be the one that the codec was configured
  // with. |output_buffer_release_cb| will be run whenever an output buffer is
  // released back to the codec (whether it's rendered or not). This is a signal
  // that the codec might be ready to accept more input. It may be run on any
  // thread.
  //
  // OutputReleasedCB will be called with a bool indicating if CodecWrapper is
  // currently draining, is drained, or has run out of output buffers.
  //
  // If not null, then we will only release codec buffers without rendering
  // on |release_task_runner|, posting if needed.  This does not change where
  // we release them with rendering; that has to be done inline.  This helps
  // us avoid a common case of hanging up the GPU main thread.
  //
  // `coded_size_alignment` describes how to translate a CodecOutputBuffer's
  // visible size into its coded size. It's used to improve coded size guesses
  // when rendering the output buffer early isn't allowed. During guessing, the
  // output's visible size will be aligned-up by the values specified. E.g., a
  // size of 1,1 applies no alignment while a size of 64,1 would round up the
  // visible width to the nearest multiple of 64.
  using OutputReleasedCB = base::RepeatingCallback<void(bool)>;
  CodecWrapper(CodecSurfacePair codec_surface_pair,
               OutputReleasedCB output_buffer_release_cb,
               scoped_refptr<base::SequencedTaskRunner> release_task_runner,
               const gfx::Size& initial_expected_size,
               const gfx::ColorSpace& config_color_space,
               std::optional<gfx::Size> coded_size_alignment,
               bool use_block_model);

  CodecWrapper(const CodecWrapper&) = delete;
  CodecWrapper& operator=(const CodecWrapper&) = delete;

  ~CodecWrapper();

  // Takes the backing codec and surface, implicitly discarding all outstanding
  // codec buffers. It's safe to use CodecOutputBuffers after this is called,
  // but they can no longer be rendered.
  CodecSurfacePair TakeCodecSurfacePair();

  // Whether the codec is in the flushed state.
  bool IsFlushed() const;

  // Whether an EOS has been queued but not yet dequeued.
  bool IsDraining() const;

  // Whether an EOS has been dequeued but the codec hasn't been flushed yet.
  bool IsDrained() const;

  // Whether there are any dequeued output buffers that have not been released.
  bool HasUnreleasedOutputBuffers() const;

  // Releases all dequeued output buffers back to the codec without rendering.
  void DiscardOutputBuffers();

  // Flushes the codec and discards all output buffers.
  bool Flush();

  // Sets the given surface and returns true on success.
  bool SetSurface(scoped_refptr<CodecSurfaceBundle> surface_bundle);

  // Returns the surface bundle that the codec is currently configured with.
  // Returns null after TakeCodecSurfacePair() is called.
  scoped_refptr<CodecSurfaceBundle> SurfaceBundle();

  // Queues |buffer| if the codec has an available input buffer.
  struct QueueStatusTraits {
    enum class Codes { kOk, kError, kTryAgainLater, kNoKey };
    static constexpr StatusGroupType Group() { return "QueueStatus"; }
  };
  using QueueStatus = TypedStatus<QueueStatusTraits>;
  QueueStatus QueueInputBuffer(const DecoderBuffer& buffer);

  // Like MediaCodecBridge::DequeueOutputBuffer() but it outputs a
  // CodecOutputBuffer instead of an index. |*codec_buffer| must be null.
  // If this returns kOk then either |*end_of_stream| will be set to true or
  // |*codec_buffer| will be non-null. The EOS buffer is returned to the codec
  // immediately. Unlike MediaCodecBridge, this does not return
  // kOutputBuffersChanged or kOutputFormatChanged.
  // It tries to dequeue another buffer instead.
  struct DequeueStatusTraits {
    enum class Codes : StatusCodeType { kOk, kError, kTryAgainLater };
    static constexpr StatusGroupType Group() { return "DequeueStatus"; }
  };
  using DequeueStatus = TypedStatus<DequeueStatusTraits>;
  DequeueStatus DequeueOutputBuffer(
      base::TimeDelta* presentation_time,
      bool* end_of_stream,
      std::unique_ptr<CodecOutputBuffer>* codec_buffer);

  size_t GetUnreleasedOutputBufferCount() const;

 private:
  scoped_refptr<CodecWrapperImpl> impl_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CODEC_WRAPPER_H_
