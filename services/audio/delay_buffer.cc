// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/delay_buffer.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "media/base/vector_math.h"

namespace audio {

DelayBuffer::DelayBuffer(int history_size) : history_size_(history_size) {}

DelayBuffer::~DelayBuffer() = default;

void DelayBuffer::Write(FrameTicks position,
                        const media::AudioBus& input_bus,
                        double volume) {
  DCHECK(chunks_.empty() || chunks_.back().GetEndPosition() <= position);

  // Prune-out the oldest InputChunks, but ensure that this DelayBuffer is
  // maintaining at least |history_size_| frames in total when this method
  // returns (i.e., after the current chunk is inserted).
  const FrameTicks prune_position =
      position + input_bus.frames() - history_size_;
  while (!chunks_.empty() &&
         chunks_.front().GetEndPosition() <= prune_position) {
    chunks_.pop_front();
  }

  // Make a copy of the AudioBus for later consumption. Apply the volume setting
  // by scaling the audio signal during the copy.
  auto copy = media::AudioBus::Create(input_bus.channels(), input_bus.frames());
  for (int ch = 0; ch < input_bus.channels(); ++ch) {
    media::vector_math::FMUL(input_bus.channel(ch), volume, input_bus.frames(),
                             copy->channel(ch));
  }

  chunks_.emplace_back(position, std::move(copy));
}

void DelayBuffer::Read(FrameTicks from,
                       int frames_to_read,
                       media::AudioBus* output_bus) {
  DCHECK_LE(frames_to_read, output_bus->frames());

  // Remove all of the oldest chunks until the one in front contains the |from|
  // position (or is the first chunk after it).
  while (!chunks_.empty() && chunks_.front().GetEndPosition() <= from) {
    chunks_.pop_front();
  }

  // Loop, transferring data from each InputChunk to the output AudioBus until
  // the requested number of frames have been read.
  for (int frames_remaining = frames_to_read; frames_remaining > 0;) {
    const int dest_offset = frames_to_read - frames_remaining;

    // If attempting to read past the end of the recorded signal, zero-pad the
    // rest of the output and return.
    if (chunks_.empty()) {
      TRACE_EVENT_INSTANT1("audio", "DelayBuffer::Read underrun",
                           TRACE_EVENT_SCOPE_THREAD, "frames missing",
                           frames_remaining);
      output_bus->ZeroFramesPartial(dest_offset, frames_remaining);
      return;
    }
    const InputChunk& chunk = chunks_.front();

    // This is the offset to the frame within the chunk's AudioBus that
    // corresponds to the offset in the output AudioBus. If this calculated
    // value is out-of-range, there is a gap (i.e., a missing piece of audio
    // signal) in the recording.
    const int source_offset =
        base::saturated_cast<int>(from + dest_offset - chunk.position);

    if (source_offset < 0) {
      // There is a gap in the recording. Fill zeroes in the corresponding part
      // of the output.
      const int frames_to_zero_fill = (source_offset + frames_remaining <= 0)
                                          ? frames_remaining
                                          : -source_offset;
      TRACE_EVENT_INSTANT1("audio", "DelayBuffer::Read gap",
                           TRACE_EVENT_SCOPE_THREAD, "frames missing",
                           frames_to_zero_fill);
      output_bus->ZeroFramesPartial(dest_offset, frames_to_zero_fill);
      frames_remaining -= frames_to_zero_fill;
      continue;
    }
    DCHECK_LE(source_offset, chunk.bus->frames());

    // Copy some or all of the frames in the current chunk to the output; the
    // lesser of: a) the frames available in the chunk, or b) the frames
    // remaining to output.
    const int frames_to_copy_from_chunk = chunk.bus->frames() - source_offset;
    if (frames_to_copy_from_chunk <= frames_remaining) {
      chunk.bus->CopyPartialFramesTo(source_offset, frames_to_copy_from_chunk,
                                     dest_offset, output_bus);
      frames_remaining -= frames_to_copy_from_chunk;
      chunks_.pop_front();  // All frames from this chunk have been consumed.
    } else {
      chunk.bus->CopyPartialFramesTo(source_offset, frames_remaining,
                                     dest_offset, output_bus);
      return;  // The |output_bus| has been fully populated.
    }
  }
}

DelayBuffer::FrameTicks DelayBuffer::GetBeginPosition() const {
  return chunks_.empty() ? 0 : chunks_.front().position;
}

DelayBuffer::FrameTicks DelayBuffer::GetEndPosition() const {
  return chunks_.empty() ? 0 : chunks_.back().GetEndPosition();
}

DelayBuffer::InputChunk::InputChunk(FrameTicks p,
                                    std::unique_ptr<media::AudioBus> b)
    : position(p), bus(std::move(b)) {}
DelayBuffer::InputChunk::InputChunk(DelayBuffer::InputChunk&&) = default;
DelayBuffer::InputChunk& DelayBuffer::InputChunk::operator=(
    DelayBuffer::InputChunk&&) = default;
DelayBuffer::InputChunk::~InputChunk() = default;

DelayBuffer::FrameTicks DelayBuffer::InputChunk::GetEndPosition() const {
  return position + bus->frames();
}

}  // namespace audio
