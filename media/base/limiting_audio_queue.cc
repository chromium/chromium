// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/limiting_audio_queue.h"

#include "base/logging.h"
#include "media/base/audio_timestamp_helper.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/373960632): Replace unsafe usage once AudioBus is spanified.
#pragma allow_unsafe_buffers
#endif

namespace media {

LimitingAudioQueue::LimitingAudioQueue(ChannelLayout channel_layout,
                                       int sample_rate,
                                       int channels,
                                       int max_buffer_size)
    : channel_layout_(channel_layout),
      max_buffer_size_(max_buffer_size),
      channels_(channels),
      sample_rate_(sample_rate),
      limiter_(std::make_unique<AudioLimiter>(sample_rate_, channels_)),
      pool_(base::MakeRefCounted<AudioBufferMemoryPool>()) {}

LimitingAudioQueue::~LimitingAudioQueue() = default;

void LimitingAudioQueue::Push(const AudioBus& input,
                              int num_frames,
                              base::TimeDelta timestamp,
                              OutputCB output_cb) {
  CHECK_GT(num_frames, 0);
  CHECK_LE(num_frames, max_buffer_size_);
  CHECK_LE(input.channels(), channels_);

  // Always create buffers using `default_buffer_size_`, so they can be reused
  // by the buffer pool.
  auto buffer = AudioBuffer::CreateBuffer(
      kSampleFormatPlanarF32, channel_layout_, channels_, sample_rate_,
      max_buffer_size_, pool_);

  buffer->set_timestamp(timestamp);
  buffer->TrimEnd(max_buffer_size_ - num_frames);

  AudioLimiter::OutputChannels output_spans;
  for (uint8_t* data : buffer->channel_data()) {
    output_spans.emplace_back(data, num_frames * sizeof(float));
  }

  limiter_->LimitPeaksPartial(
      input, num_frames, output_spans,
      base::BindOnce(std::move(output_cb), std::move(buffer)));
}

void LimitingAudioQueue::Flush() {
  limiter_->Flush();
}
void LimitingAudioQueue::Clear() {
  limiter_ = std::make_unique<AudioLimiter>(sample_rate_, channels_);
}

}  // namespace media
