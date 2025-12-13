// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_block_fifo.h"

#include <stdint.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/containers/span_reader.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/sample_format.h"
namespace media {

AudioBlockFifo::AudioBlockFifo(int channels, int frames, int blocks)
    : channels_(channels), block_frames_(frames) {
  IncreaseCapacity(blocks);
}

AudioBlockFifo::~AudioBlockFifo() = default;

void AudioBlockFifo::Push(base::span<const uint8_t> source,
                          int frames,
                          SampleFormat sample_format) {
  TRACE_EVENT2("audio", "AudioBlockFifo::Push", "pushed frames", frames,
               "available frames", GetAvailableFrames());
  CHECK(!source.empty());
  PushInternal(source, frames, sample_format);
}

void AudioBlockFifo::PushSilence(int frames) {
  TRACE_EVENT2("audio", "AudioBlockFifo::PushSilence", "pushed frames", frames,
               "available frames", GetAvailableFrames());
  PushInternal({}, frames, kUnknownSampleFormat);
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

void AudioBlockFifo::PushInternal(base::span<const uint8_t> source,
                                  int frames,
                                  SampleFormat sample_format) {
  // |source| may be empty if bytes_per_sample is 0. In that case, we inject
  // silence.
  const bool push_silence = source.empty();
  CHECK_EQ(push_silence, (sample_format == kUnknownSampleFormat));

  const int bytes_per_sample = SampleFormatToBytesPerChannel(sample_format);

  DCHECK_GT(frames, 0);
  DCHECK_LT(available_blocks_, static_cast<int>(audio_blocks_.size()));
  CHECK_LE(frames, GetUnfilledFrames());

  int frames_to_push = frames;

  base::SpanReader span_reader(source);
  const size_t bytes_per_frame = channels_ * bytes_per_sample;
  while (frames_to_push) {
    // Get the current write block.
    AudioBus* current_block = audio_blocks_[write_block_].get();

    // Figure out what segment sizes we need when adding the new content to
    // the FIFO.
    const size_t push_frames =
        std::min(block_frames_ - write_pos_, frames_to_push);

    if (!push_silence) {
      CHECK_GT(bytes_per_frame, 0u);
      auto data_for_current_block = span_reader.Read(
          base::CheckMul<size_t>(bytes_per_frame, push_frames).ValueOrDie());
      // Deinterleave the content to the FIFO.
      switch (sample_format) {
        case kSampleFormatU8:
          current_block->FromInterleavedPartial<UnsignedInt8SampleTypeTraits>(
              data_for_current_block->data(), write_pos_, push_frames);
          break;
        case kSampleFormatS16:
          current_block->FromInterleavedPartial<SignedInt16SampleTypeTraits>(
              reinterpret_cast<const int16_t*>(data_for_current_block->data()),
              write_pos_, push_frames);
          break;
        case kSampleFormatS32:
          current_block->FromInterleavedPartial<SignedInt32SampleTypeTraits>(
              reinterpret_cast<const int32_t*>(data_for_current_block->data()),
              write_pos_, push_frames);
          break;
        case kSampleFormatF32:
          current_block->FromInterleavedPartial<Float32SampleTypeTraits>(
              reinterpret_cast<const float*>(data_for_current_block->data()),
              write_pos_, push_frames);
          break;
        default:
          NOTREACHED() << "Unsupported sample format encountered: "
                       << SampleFormatToString(sample_format);
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

    frames_to_push -= push_frames;
    DCHECK_GE(frames_to_push, 0);
  }

  CHECK_EQ(span_reader.remaining(), 0u);
}

}  // namespace media
