// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_push_fifo.h"

#include <algorithm>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"

namespace media {

AudioPushFifo::AudioPushFifo(const OutputCallback& callback)
    : callback_(callback), frames_per_buffer_(0) {
  DCHECK(!callback_.is_null());
}

AudioPushFifo::~AudioPushFifo() = default;

void AudioPushFifo::Reset(int frames_per_buffer) {
  DCHECK_GT(frames_per_buffer, 0);

  audio_queue_.reset();
  queued_frames_ = 0;

  frames_per_buffer_ = frames_per_buffer;
}

void AudioPushFifo::Push(const AudioBus& input_bus) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio"), "AudioPushFifo::Push");

  DCHECK_GT(frames_per_buffer_, 0);

  // Fast path: No buffering required.
  if ((queued_frames_ == 0) && (input_bus.frames() == frames_per_buffer_)) {
    callback_.Run(input_bus, 0);
    return;
  }

  // Lazy-create the |audio_queue_| if needed.
  if (!audio_queue_ || audio_queue_->channels() != input_bus.channels())
    audio_queue_ = AudioBus::Create(input_bus.channels(), frames_per_buffer_);

  // Start with a frame offset that refers to the position of the first sample
  // in |audio_queue_| relative to the first sample in |input_bus|.
  int frame_delay = -queued_frames_;

  // Repeatedly fill up |audio_queue_| with more sample frames from |input_bus|
  // and deliver batches until all sample frames in |input_bus| have been
  // consumed.
  int input_offset = 0;
  do {
    // Attempt to fill |audio_queue_| completely.
    const int frames_to_enqueue =
        std::min(static_cast<int>(input_bus.frames() - input_offset),
                 frames_per_buffer_ - queued_frames_);
    if (frames_to_enqueue > 0) {
      DVLOG(2) << "Enqueuing " << frames_to_enqueue << " frames.";
      input_bus.CopyPartialFramesTo(input_offset, frames_to_enqueue,
                                    queued_frames_, audio_queue_.get());
      queued_frames_ += frames_to_enqueue;
      input_offset += frames_to_enqueue;
    }

    // If |audio_queue_| has been filled completely, deliver the re-buffered
    // audio to the consumer.
    if (queued_frames_ == frames_per_buffer_) {
      DVLOG(2) << "Delivering another " << queued_frames_ << " frames.";
      callback_.Run(*audio_queue_, frame_delay);
      frame_delay += frames_per_buffer_;
      queued_frames_ = 0;
    } else {
      // Not enough frames queued-up yet to deliver more frames.
    }
  } while (input_offset < input_bus.frames());
}

void AudioPushFifo::Flush() {
  if (queued_frames_ == 0)
    return;

  audio_queue_->ZeroFramesPartial(queued_frames_,
                                  audio_queue_->frames() - queued_frames_);
  callback_.Run(*audio_queue_, -queued_frames_);
  queued_frames_ = 0;
}

}  // namespace media
