// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_WRAPPER_H_
#define MEDIA_GPU_ANDROID_CODEC_WRAPPER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/decoder_buffer.h"
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
  // Releases the buffer without rendering it.
  ~CodecOutputBuffer();

  // Releases this buffer and renders it to the surface.
  bool ReleaseToSurface();

  // The size of the image.
  gfx::Size size() const { return size_; }

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
                    const gfx::Size& size);

  // For testing, since CodecWrapperImpl isn't available.  Uses nullptr.
  CodecOutputBuffer(int64_t id, const gfx::Size& size);

  scoped_refptr<CodecWrapperImpl> codec_;
  int64_t id_;
  bool was_rendered_ = false;
  gfx::Size size_;
  DISALLOW_COPY_AND_ASSIGN(CodecOutputBuffer);
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
  // currently draining or in the drained state.
  //
  // If not null, then we will only release codec buffers without rendering
  // on |release_task_runner|, posting if needed.  This does not change where
  // we release them with rendering; that has to be done inline.  This helps
  // us avoid a common case of hanging up the GPU main thread.
  using OutputReleasedCB = base::RepeatingCallback<void(bool)>;
  CodecWrapper(CodecSurfacePair codec_surface_pair,
               OutputReleasedCB output_buffer_release_cb,
               scoped_refptr<base::SequencedTaskRunner> release_task_runner);
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

  // Whether the codec supports Flush().
  bool SupportsFlush(DeviceInfo* device_info) const;

  // Flushes the codec and discards all output buffers.
  bool Flush();

  // Sets the given surface and returns true on success.
  bool SetSurface(scoped_refptr<CodecSurfaceBundle> surface_bundle);

  // Returns the surface bundle that the codec is currently configured with.
  // Returns null after TakeCodecSurfacePair() is called.
  scoped_refptr<CodecSurfaceBundle> SurfaceBundle();

  // Queues |buffer| if the codec has an available input buffer.
  enum class QueueStatus { kOk, kError, kTryAgainLater, kNoKey };
  QueueStatus QueueInputBuffer(const DecoderBuffer& buffer);

  // Like MediaCodecBridge::DequeueOutputBuffer() but it outputs a
  // CodecOutputBuffer instead of an index. |*codec_buffer| must be null.
  // If this returns MEDIA_CODEC_OK then either |*end_of_stream| will be set to
  // true or |*codec_buffer| will be non-null. The EOS buffer is returned to the
  // codec immediately. Unlike MediaCodecBridge, this does not return
  // MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED or MEDIA_CODEC_OUTPUT_FORMAT_CHANGED. It
  // tries to dequeue another buffer instead.
  enum class DequeueStatus { kOk, kError, kTryAgainLater };
  DequeueStatus DequeueOutputBuffer(
      base::TimeDelta* presentation_time,
      bool* end_of_stream,
      std::unique_ptr<CodecOutputBuffer>* codec_buffer);

 private:
  scoped_refptr<CodecWrapperImpl> impl_;
  DISALLOW_COPY_AND_ASSIGN(CodecWrapper);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CODEC_WRAPPER_H_
