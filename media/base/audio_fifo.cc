// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_fifo.h"

#include <cstring>

#include "base/check_op.h"
#include "base/numerics/safe_math.h"
#include "base/trace_event/trace_event.h"
#include "base/types/zip.h"

namespace media {

// Given current position in the FIFO, the maximum number of elements in the
// FIFO and the size of the input; this method provides two output results:
// |size| and |wrap_size|. These two results can then be utilized for memcopy
// operations to and from the FIFO.
// Under "normal" circumstances, |size| will be equal to |in_size| and
// |wrap_size| will be zero. This case corresponding to the non-wrapping case
// where we have not yet reached the "edge" of the FIFO. If |pos| + |in_size|
// exceeds the total size of the FIFO, we must wrap around and start reusing
// a part the allocated memory. The size of this part is given by |wrap_size|.
static void GetSizes(size_t pos,
                     size_t max_size,
                     size_t in_size,
                     size_t* size,
                     size_t* wrap_size) {
  const size_t frames_before_end = base::CheckSub(max_size, pos).ValueOrDie();
  if (in_size > frames_before_end) {
    // Wrapping is required => derive size of each segment.
    *size = max_size - pos;
    *wrap_size = in_size - *size;
  } else {
    // Wrapping is not required.
    *size = in_size;
    *wrap_size = 0u;
  }
}

// Updates the read/write position with |step| modulo the maximum number of
// elements in the FIFO to ensure that the position counters wraps around at
// the endpoint.
static size_t UpdatePos(size_t pos, size_t step, int max_size) {
  return ((pos + step) % max_size);
}

AudioFifo::AudioFifo(int channels, int frames)
    : audio_bus_(AudioBus::Create(channels, frames)),
      max_frames_(base::checked_cast<size_t>(frames)) {}

AudioFifo::~AudioFifo() = default;

void AudioFifo::Push(const AudioBus* source) {
  Push(source, source->frames());
}

void AudioFifo::Push(const AudioBus* source, int source_size) {
  DCHECK(source);
  DCHECK_EQ(source->channels(), audio_bus_->channels());
  DCHECK_LE(source_size, source->frames());

  // Ensure that there is space for the new data in the FIFO.
  const size_t source_frames = base::checked_cast<size_t>(source_size);
  const size_t remaining_frames =
      base::CheckSub(max_frames_, frames_).ValueOrDie();
  CHECK_LE(source_frames, remaining_frames);

  TRACE_EVENT_BEGIN2(TRACE_DISABLED_BY_DEFAULT("audio"), "AudioFifo::Push",
                     "this", static_cast<void*>(this), "fifo frames", frames_);

  // Figure out if wrapping is needed and if so what segment sizes we need
  // when adding the new audio bus content to the FIFO.
  size_t append_size = 0u;
  size_t wrap_size = 0u;
  GetSizes(write_pos_, max_frames_, source_frames, &append_size, &wrap_size);

  // Copy all channels from the source to the FIFO. Wrap around if needed.
  for (auto [data_src, fifo_dest] :
       base::zip(source->AllChannels(), audio_bus_->AllChannels())) {
    auto [append_data, wrap_data] = data_src.split_at(append_size);

    // Append part of (or the complete) source to the FIFO.
    fifo_dest.subspan(write_pos_, append_size)
        .copy_from_nonoverlapping(append_data);

    if (wrap_size > 0) {
      // Wrapping is needed: copy remaining part from the source to the FIFO.
      fifo_dest.first(wrap_size).copy_from_nonoverlapping(wrap_data);
    }
  }

  frames_ += source_frames;
  DCHECK_LE(frames_, max_frames_);
  write_pos_ = UpdatePos(write_pos_, source_frames, max_frames_);
  TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("audio"), "AudioFifo::Push",
                   "frames", source_frames);
}

void AudioFifo::Consume(AudioBus* destination,
                        int start_frame,
                        int frames_to_consume) {
  DCHECK(destination);
  DCHECK_EQ(destination->channels(), audio_bus_->channels());

  const size_t dest_offset = base::checked_cast<size_t>(start_frame);
  const size_t frame_count = base::checked_cast<size_t>(frames_to_consume);

  // It is not possible to ask for more data than what is available in the FIFO.
  CHECK_LE(frame_count, frames_);

  // A copy from the FIFO to |destination| will only be performed if the
  // allocated memory in |destination| is sufficient.
  const size_t free_frames_in_dest =
      base::CheckSub(destination->frames(), dest_offset).ValueOrDie();
  CHECK_LE(frame_count, free_frames_in_dest);

  TRACE_EVENT_BEGIN2(TRACE_DISABLED_BY_DEFAULT("audio"), "AudioFifo::Consume",
                     "this", static_cast<void*>(this), "fifo frames", frames_);

  // Figure out if wrapping is needed and if so what segment sizes we need
  // when removing audio bus content from the FIFO.
  size_t consume_size = 0u;
  size_t wrap_size = 0u;
  GetSizes(read_pos_, max_frames_, frame_count, &consume_size, &wrap_size);

  // For all channels, remove the requested amount of data from the FIFO
  // and copy the content to the destination. Wrap around if needed.
  for (auto [data_dest, fifo_src] :
       base::zip(destination->AllChannels(), audio_bus_->AllChannels())) {
    auto [consume_data, wrap_data] =
        data_dest.subspan(dest_offset, frame_count).split_at(consume_size);

    // Copy a selected part of the FIFO to the destination.
    consume_data.copy_from_nonoverlapping(
        fifo_src.subspan(read_pos_, consume_size));
    if (wrap_size > 0) {
      // Wrapping is needed: copy remaining part to the destination.
      wrap_data.copy_from_nonoverlapping(fifo_src.first(wrap_size));
    }
  }

  frames_ -= frame_count;
  read_pos_ = UpdatePos(read_pos_, frame_count, max_frames_);
  TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("audio"), "AudioFifo::Consume",
                   "frames", frame_count);
}

void AudioFifo::Clear() {
  frames_ = 0u;
  read_pos_ = 0u;
  write_pos_ = 0u;
}

}  // namespace media
