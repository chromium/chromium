// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_pull_fifo.h"

#include <algorithm>

#include "base/check_op.h"
#include "media/base/audio_bus.h"

namespace media {

AudioPullFifo::AudioPullFifo(int channels, int frames, ReadCB read_cb)
    : read_cb_(std::move(read_cb)),
      fifo_(AudioBus::Create(channels, frames)),
      fifo_index_(frames) {}

AudioPullFifo::~AudioPullFifo() = default;

void AudioPullFifo::Consume(AudioBus* destination, int frames_to_consume) {
  DCHECK_LE(frames_to_consume, destination->frames());

  int remaining_frames_to_provide = frames_to_consume;

  // Try to fulfill the request using what's available in the FIFO.
  int frames_read = ReadFromFifo(destination, remaining_frames_to_provide, 0);
  int write_pos = frames_read;
  remaining_frames_to_provide -= frames_read;

  // Get the remaining audio frames from the producer using the callback.
  while (remaining_frames_to_provide > 0) {
    DCHECK_EQ(fifo_index_, fifo_->frames());
    fifo_index_ = 0;

    // Fill up the FIFO by acquiring audio data from the producer.
    read_cb_.Run(write_pos, fifo_.get());

    // Try to fulfill the request using what's available in the FIFO.
    frames_read =
        ReadFromFifo(destination, remaining_frames_to_provide, write_pos);
    write_pos += frames_read;
    remaining_frames_to_provide -= frames_read;
  }
}

void AudioPullFifo::Clear() { fifo_index_ = fifo_->frames(); }

int AudioPullFifo::SizeInFrames() const {
  return fifo_->frames();
}

int AudioPullFifo::ReadFromFifo(AudioBus* destination,
                                int frames_to_provide,
                                int write_pos) {
  int frames = std::min(frames_to_provide, fifo_->frames() - fifo_index_);
  if (frames <= 0)
    return 0;

  for (int ch = 0; ch < fifo_->channels(); ++ch) {
    const float* src = fifo_->channel(ch) + fifo_index_;
    float* dest = destination->channel(ch) + write_pos;
    memcpy(dest, src, frames * sizeof(*src));
  }

  fifo_index_ += frames;
  return frames;
}

}  // namespace media
