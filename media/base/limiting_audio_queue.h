// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_LIMITING_AUDIO_QUEUE_H_
#define MEDIA_BASE_LIMITING_AUDIO_QUEUE_H_

#include "base/functional/callback.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_limiter.h"
#include "media/base/media_export.h"

namespace media {

// Simple wrapper around an AudioLimiter which takes in input as a AudioBus,
// limits the audio to the [-1.0, 1.0] range, and outputs it as an AudioBuffer.
class MEDIA_EXPORT LimitingAudioQueue {
 public:
  using OutputCB = base::OnceCallback<void(scoped_refptr<AudioBuffer>)>;

  // `channel_layout`, `sample_rate` and `channels` are used to create all
  // AudioBuffer outputs. `max_buffers_size` is the biggest size an
  // `AudioBuffer` can have, and is used to allocate memory chunks of the same
  // size for `pool_`.
  LimitingAudioQueue(ChannelLayout channel_layout,
                     int sample_rate,
                     int channels,
                     int max_buffer_size);
  ~LimitingAudioQueue();

  LimitingAudioQueue(const LimitingAudioQueue&) = delete;
  LimitingAudioQueue& operator=(const LimitingAudioQueue&) = delete;

  // Pushes in `num_frames` of audio from `input`. The gain adjusted audio will
  // be synchronously delivered as an AudioBuffer with `num_frames` of data, via
  // `output_cb` once enough data has been pushed in, or Flush() is called.
  // `input` must have exactly `channels_` channels, and at least `num_frames`
  // frames. `num_frames` cannot be bigger than `max_buffer_size_`.
  // Note: if Flush() was called, Clear() must be called before `this` can
  //       accept more input.
  void Push(const AudioBus& input,
            int num_frames,
            base::TimeDelta timestamp,
            OutputCB output_cb);

  // Outputs the remaining audio from 'limiter_' through the appropriate
  // OutputCB that pushed the audio in. Noop if no audio was ever pushed in.
  // Note: can only be called once, unless Clear() is called.
  void Flush();

  // Resets internal state, dropping any pending frames and pending `output_cb`.
  void Clear();

 private:
  // Parameters used to create the AudioBus.
  const ChannelLayout channel_layout_;
  const int max_buffer_size_;
  const int channels_;
  const int sample_rate_;

  // Limits the incoming audio to the [-1.0, 1.0] range.
  std::unique_ptr<AudioLimiter> limiter_;

  // Memory pool used to prevent memory thrashing.
  scoped_refptr<AudioBufferMemoryPool> pool_;
};

}  // namespace media

#endif  // MEDIA_BASE_LIMITING_AUDIO_QUEUE_H_
