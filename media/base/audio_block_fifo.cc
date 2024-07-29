// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <algorithm>

#include "media/base/audio_block_fifo.h"

#include "base/check_op.h"
#include "base/trace_event/trace_event.h"

namespace media {

AudioBlockFifo::AudioBlockFifo(int channels, int frames, int blocks)
    : channels_(channels),
      block_frames_(frames),
      write_block_(0),
      read_block_(0),
      available_blocks_(0),
      write_pos_(0) {
  IncreaseCapacity(blocks);
}

AudioBlockFifo::~AudioBlockFifo() = default;

void AudioBlockFifo::Push(const void* source,
                          int frames,
                          int bytes_per_sample) {
  TRACE_EVENT2("audio", "AudioBlockFifo::Push", "pushed frames", frames,
               "available frames", GetAvailableFrames());
  PushInternal(source, frames, bytes_per_sample);
}

void AudioBlockFifo::PushSilence(int frames) {
  TRACE_EVENT2("audio", "AudioBlockFifo::PushSilence", "pushed frames", frames,
               "available frames", GetAvailableFrames());
  PushInternal(nullptr, frames, 0);
}

const AudioBus* AudioBlockFifo::Consume() {
  DCHECK(available_blocks_);
  TRACE_EVENT1("audio", "AudioBlockFifo::Consume", "available frames",
               GetAvailableFrames());
  AudioBus* audio_bus = audio_blocks_[read_block_].get();
  read_block_ = (read_block_ + 1) % audio_blocks_.size();
  --available_blocks_;
  return audio_bus;
}

void AudioBlockFifo::Clear() {
  write_pos_ = 0;
  write_block_ = 0;
  read_block_ = 0;
  available_blocks_ = 0;
}

int AudioBlockFifo::GetAvailableFrames() const {
  return available_blocks_ * block_frames_ + write_pos_;
}

int AudioBlockFifo::GetUnfilledFrames() const {
  const int unfilled_frames =
      (audio_blocks_.size() - available_blocks_) * block_frames_ - write_pos_;
  DCHECK_GE(unfilled_frames, 0);
  return unfilled_frames;
}

void AudioBlockFifo::IncreaseCapacity(int blocks) {
  DCHECK_GT(blocks, 0);

  // Create |blocks| of audio buses and insert them to the containers.
  audio_blocks_.reserve(audio_blocks_.size() + blocks);

  const int original_size = audio_blocks_.size();
  for (int i = 0; i < blocks; ++i)
    audio_blocks_.push_back(AudioBus::Create(channels_, block_frames_));

  if (!original_size)
    return;

  std::rotate(audio_blocks_.begin() + read_block_,
              audio_blocks_.begin() + original_size, audio_blocks_.end());

  // Update the write pointer if it is on top of the new inserted blocks.
  if (write_block_ >= read_block_)
    write_block_ += blocks;

  // Update the read pointers correspondingly.
  read_block_ += blocks;

  DCHECK_LT(read_block_, static_cast<int>(audio_blocks_.size()));
  DCHECK_LT(write_block_, static_cast<int>(audio_blocks_.size()));
}

void AudioBlockFifo::PushInternal(const void* source,
                                  int frames,
                                  int bytes_per_sample) {
  // |source| may be nullptr if bytes_per_sample is 0. In that case,
  // we inject silence.
  DCHECK((source && bytes_per_sample > 0) || (!source && !bytes_per_sample));
  DCHECK_GT(frames, 0);
  DCHECK_LT(available_blocks_, static_cast<int>(audio_blocks_.size()));
  CHECK_LE(frames, GetUnfilledFrames());

  const uint8_t* source_ptr = static_cast<const uint8_t*>(source);
  int frames_to_push = frames;
  while (frames_to_push) {
    // Get the current write block.
    AudioBus* current_block = audio_blocks_[write_block_].get();

    // Figure out what segment sizes we need when adding the new content to
    // the FIFO.
    const int push_frames =
        std::min(block_frames_ - write_pos_, frames_to_push);

    if (source) {
      // Deinterleave the content to the FIFO.
      switch (bytes_per_sample) {
        case 1:
          current_block->FromInterleavedPartial<UnsignedInt8SampleTypeTraits>(
              source_ptr, write_pos_, push_frames);
          break;
        case 2:
          current_block->FromInterleavedPartial<SignedInt16SampleTypeTraits>(
              reinterpret_cast<const int16_t*>(source_ptr), write_pos_,
              push_frames);
          break;
        case 4:
          current_block->FromInterleavedPartial<SignedInt32SampleTypeTraits>(
              reinterpret_cast<const int32_t*>(source_ptr), write_pos_,
              push_frames);
          break;
        default:
          NOTREACHED_IN_MIGRATION()
              << "Unsupported bytes per sample encountered: "
              << bytes_per_sample;
          current_block->ZeroFramesPartial(write_pos_, push_frames);
      }
    } else {
      current_block->ZeroFramesPartial(write_pos_, push_frames);
    }

    write_pos_ = (write_pos_ + push_frames) % block_frames_;

    if (!write_pos_) {
      // The current block is completely filled, increment |write_block_| and
      // |available_blocks_|.
      write_block_ = (write_block_ + 1) % audio_blocks_.size();
      ++available_blocks_;
    }

    if (source_ptr)
      source_ptr += push_frames * bytes_per_sample * channels_;
    frames_to_push -= push_frames;
    DCHECK_GE(frames_to_push, 0);
  }
}

}  // namespace media
