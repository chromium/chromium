// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_buffer_queue.h"

#include <algorithm>

#include "base/check_op.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

AudioBufferQueue::AudioBufferQueue() { Clear(); }
AudioBufferQueue::~AudioBufferQueue() = default;

void AudioBufferQueue::Clear() {
  buffers_.clear();
  front_buffer_offset_ = 0;
  frames_ = 0;
}

void AudioBufferQueue::Append(scoped_refptr<AudioBuffer> buffer_in) {
  // Update the |frames_| counter since we have added frames.
  frames_ += buffer_in->frame_count();
  CHECK_GT(frames_, 0);  // make sure it doesn't overflow.

  // Add the buffer to the queue. Inserting into deque invalidates all
  // iterators, so point to the first buffer.
  buffers_.push_back(std::move(buffer_in));
}

int AudioBufferQueue::ReadFrames(int frames,
                                 int dest_frame_offset,
                                 AudioBus* dest) {
  DCHECK_GE(dest->frames(), frames + dest_frame_offset);
  return InternalRead(frames, true, 0, dest_frame_offset, dest);
}

int AudioBufferQueue::PeekFrames(int frames,
                                 int source_frame_offset,
                                 int dest_frame_offset,
                                 AudioBus* dest) {
  DCHECK_GE(dest->frames(), frames);
  return InternalRead(
      frames, false, source_frame_offset, dest_frame_offset, dest);
}

void AudioBufferQueue::SeekFrames(int frames) {
  // Perform seek only if we have enough bytes in the queue.
  CHECK_LE(frames, frames_);
  int taken = InternalRead(frames, true, 0, 0, NULL);
  DCHECK_EQ(taken, frames);
}

std::optional<base::TimeDelta> AudioBufferQueue::FrontTimestamp() const {
  if (buffers_.empty()) {
    return std::nullopt;
  }

  return buffers_.front()->timestamp() +
         AudioTimestampHelper::FramesToTime(front_buffer_offset_,
                                            buffers_.front()->sample_rate());
}

int AudioBufferQueue::InternalRead(int frames,
                                   bool advance_position,
                                   int source_frame_offset,
                                   int dest_frame_offset,
                                   AudioBus* dest) {
  if (buffers_.empty())
    return 0;

  if (buffers_.front()->IsBitstreamFormat()) {
    // For compressed bitstream formats, a partial compressed audio frame is
    // less useful, since it can't generate any PCM frame out of it. Also, we
    // want to keep the granularity as fine as possible so that discarding a
    // small chunk of PCM frames is still possible. Thus, we only transfer a
    // complete AudioBuffer at a time.
    DCHECK(!dest_frame_offset ||
           dest_frame_offset == dest->GetBitstreamFrames());
    DCHECK(!source_frame_offset);

    const auto& buffer = buffers_.front();
    int taken = buffer->frame_count();

    // if |dest| is NULL, there's no need to copy.
    if (dest) {
      // We always copy a whole bitstream buffer. Make sure we have space.
      CHECK_GE(frames, buffer->frame_count());
      buffer->ReadFrames(buffer->frame_count(), 0, dest_frame_offset, dest);
    }

    if (advance_position) {
      // Update the appropriate values since |taken| frames have been copied
      // out.
      frames_ -= taken;
      DCHECK_GE(frames_, 0);

      // Remove any buffers before the current buffer as there is no going
      // backwards.
      buffers_.pop_front();
    }

    return taken;
  }

  // Counts how many frames are actually read from the buffer queue.
  int taken = 0;
  BufferQueue::iterator current_buffer = buffers_.begin();
  int current_buffer_offset = front_buffer_offset_;

  int frames_to_skip = source_frame_offset;
  while (taken < frames) {
    // |current_buffer| is valid since the first time this buffer is appended
    // with data. Make sure there is data to be processed.
    if (current_buffer == buffers_.end())
      break;

    scoped_refptr<AudioBuffer> buffer = *current_buffer;

    int remaining_frames_in_buffer =
        buffer->frame_count() - current_buffer_offset;

    if (frames_to_skip > 0) {
      // If there are frames to skip, do it first. May need to skip into
      // subsequent buffers.
      int skipped = std::min(remaining_frames_in_buffer, frames_to_skip);
      current_buffer_offset += skipped;
      frames_to_skip -= skipped;
    } else {
      // Find the right amount to copy from the current buffer. We shall copy no
      // more than |frames| frames in total and each single step copies no more
      // than the current buffer size.
      int copied = std::min(frames - taken, remaining_frames_in_buffer);

      // if |dest| is NULL, there's no need to copy.
      if (dest) {
        buffer->ReadFrames(
            copied, current_buffer_offset, dest_frame_offset + taken, dest);
      }

      // Increase total number of frames copied, which regulates when to end
      // this loop.
      taken += copied;

      // We have read |copied| frames from the current buffer. Advance the
      // offset.
      current_buffer_offset += copied;
    }

    // Has the buffer has been consumed?
    if (current_buffer_offset == buffer->frame_count()) {
      // If we are at the last buffer, no more data to be copied, so stop.
      BufferQueue::iterator next = current_buffer + 1;
      if (next == buffers_.end())
        break;

      // Advances the iterator.
      current_buffer = next;
      current_buffer_offset = 0;
    }
  }

  if (advance_position) {
    // Update the appropriate values since |taken| frames have been copied out.
    frames_ -= taken;
    DCHECK_GE(frames_, 0);

    // Remove any buffers before the current buffer as there is no going
    // backwards.
    buffers_.erase(buffers_.begin(), current_buffer);
    front_buffer_offset_ = current_buffer_offset;
  }

  return taken;
}

}  // namespace media
