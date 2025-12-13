// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/seekable_buffer.h"

#include <algorithm>

#include "base/check_op.h"
#include "media/base/data_buffer.h"
#include "media/base/timestamp_constants.h"

namespace media {

SeekableBuffer::SeekableBuffer(size_t backward_capacity,
                               size_t forward_capacity)
    : backward_capacity_(backward_capacity),
      forward_capacity_(forward_capacity),
      current_time_(kNoTimestamp) {
  current_buffer_ = buffers_.begin();
}

SeekableBuffer::~SeekableBuffer() = default;

void SeekableBuffer::Clear() {
  buffers_.clear();
  current_buffer_ = buffers_.begin();
  current_buffer_offset_ = 0;
  backward_bytes_ = 0;
  forward_bytes_ = 0;
  current_time_ = kNoTimestamp;
}

size_t SeekableBuffer::Read(base::span<uint8_t> data) {
  return InternalRead(base::SpanOrSize(data), true, 0);
}

size_t SeekableBuffer::Peek(base::span<uint8_t> data, size_t forward_offset) {
  return InternalRead(base::SpanOrSize(data), false, forward_offset);
}

base::span<const uint8_t> SeekableBuffer::GetCurrentChunk() const {
  BufferQueue::iterator current_buffer = current_buffer_;
  size_t current_buffer_offset = current_buffer_offset_;
  // Advance position if we are in the end of the current buffer, skipping
  // any empty buffers.
  while (current_buffer != buffers_.end() &&
         current_buffer_offset >= (*current_buffer)->size()) {
    ++current_buffer;
    current_buffer_offset = 0;
  }
  if (current_buffer == buffers_.end()) {
    return {};
  }
  return (*current_buffer)->data().subspan(current_buffer_offset);
}

bool SeekableBuffer::Append(const scoped_refptr<DataBuffer>& buffer_in) {
  if (buffers_.empty() && buffer_in->timestamp() != kNoTimestamp) {
    current_time_ = buffer_in->timestamp();
  }

  // Since the forward capacity is only used to check the criteria for buffer
  // full, we always append data to the buffer.
  buffers_.push_back(buffer_in);

  // After we have written the first buffer, update `current_buffer_` to point
  // to it.
  if (current_buffer_ == buffers_.end()) {
    current_buffer_ = buffers_.begin();
  }

  // Update the `forward_bytes_` counter since we have more bytes.
  forward_bytes_ += buffer_in->size();

  // Advise the user to stop append if the amount of forward bytes exceeds
  // the forward capacity. A false return value means the user should stop
  // appending more data to this buffer.
  return forward_bytes() < forward_capacity_;
}

bool SeekableBuffer::Append(base::span<const uint8_t> data) {
  if (!data.empty()) {
    return Append(DataBuffer::CopyFrom(data));
  }

  // Return our remaining forward capacity.
  return forward_bytes() < forward_capacity_;
}

bool SeekableBuffer::Seek(ptrdiff_t offset) {
  if (offset > 0) {
    return SeekForward(offset);
  }
  if (offset < 0) {
    return SeekBackward(-offset);
  }
  return true;
}

bool SeekableBuffer::SeekForward(size_t num_bytes) {
  // Perform seeking forward only if we have enough bytes in the queue.
  if (num_bytes > forward_bytes()) {
    return false;
  }

  // Do a read of `num_bytes` bytes.
  const size_t bytes_taken =
      InternalRead(base::SpanOrSize<uint8_t>(num_bytes), true, 0);
  CHECK_EQ(bytes_taken, num_bytes);
  return true;
}

bool SeekableBuffer::SeekBackward(size_t num_bytes) {
  if (num_bytes > backward_bytes()) {
    return false;
  }

  // Loop until we taken enough bytes and rewind by the desired `num_bytes`.
  size_t bytes_taken = 0;
  while (bytes_taken < num_bytes) {
    // `current_buffer_` can never be invalid when we are in this loop. It can
    // only be invalid before any data is appended. The invalid case should be
    // handled by checks before we enter this loop.
    CHECK(current_buffer_ != buffers_.end());

    // We try to consume at most `num_bytes` bytes in the backward direction. We
    // also have to account for the offset we are in the current buffer, so take
    // the minimum between the two to determine the amount of bytes to take from
    // the current buffer.
    const size_t consumed =
        std::min(num_bytes - bytes_taken, current_buffer_offset_);

    // Decreases the offset in the current buffer since we are rewinding.
    current_buffer_offset_ -= consumed;

    // Increase the amount of bytes taken in the backward direction. This
    // determines when to stop the loop.
    bytes_taken += consumed;

    // Forward bytes increases and backward bytes decreases by the amount
    // consumed in the current buffer.
    forward_bytes_ += consumed;
    backward_bytes_ -= consumed;

    // The current buffer pointed by current iterator has been consumed. Move
    // the iterator backward so it points to the previous buffer.
    if (current_buffer_offset_ == 0) {
      if (current_buffer_ == buffers_.begin()) {
        break;
      }
      // Move the iterator backward.
      --current_buffer_;

      // Set the offset into the current buffer to be the buffer size as we
      // are preparing for rewind for next iteration.
      current_buffer_offset_ = (*current_buffer_)->size();
    }
  }

  UpdateCurrentTime(current_buffer_, current_buffer_offset_);
  CHECK_EQ(bytes_taken, num_bytes);
  return true;
}

void SeekableBuffer::EvictBackwardBuffers() {
  // Advances the iterator until we hit the current pointer.
  while (backward_bytes() > backward_capacity_) {
    auto first_buffer = buffers_.begin();
    if (first_buffer == current_buffer_) {
      break;
    }
    backward_bytes_ -= (*first_buffer)->size();
    buffers_.erase(first_buffer);
  }
}

size_t SeekableBuffer::InternalRead(base::SpanOrSize<uint8_t> data,
                                    bool advance_position,
                                    size_t forward_offset) {
  auto current_buffer = current_buffer_;
  size_t current_buffer_offset = current_buffer_offset_;

  size_t bytes_to_skip = forward_offset;
  size_t bytes_taken = 0;
  while (bytes_taken < data.size()) {
    if (current_buffer == buffers_.end()) {
      break;
    }

    scoped_refptr<DataBuffer> buffer = *current_buffer;
    auto buffer_data = buffer->data().subspan(current_buffer_offset);

    if (bytes_to_skip == 0) {
      // Find the right amount to copy from the current buffer referenced by
      // `buffer`. We shall copy no more than `size` bytes in total and
      // each single step copied no more than the current buffer size.
      const size_t copied =
          std::min(data.size() - bytes_taken, buffer_data.size());

      // If seeking forward, data may be empty.
      if (auto data_span = data.span()) {
        // We currently don't support only copying a subsection during reads.
        data_span->subspan(bytes_taken, copied)
            .copy_from(buffer_data.first(copied));
      }

      // Increase total number of bytes copied, which regulates when to end this
      // loop.
      bytes_taken += copied;

      // We have read `copied` bytes from the current buffer. Advances the
      // offset.
      current_buffer_offset += copied;
    } else {
      const size_t skipped = std::min(buffer_data.size(), bytes_to_skip);
      current_buffer_offset += skipped;
      bytes_to_skip -= skipped;
    }

    // The buffer has been consumed.
    if (current_buffer_offset == buffer->size()) {
      if (advance_position) {
        // Next buffer may not have timestamp, so we need to update current
        // timestamp before switching to the next buffer.
        UpdateCurrentTime(current_buffer, current_buffer_offset);
      }

      // If we are at the last buffer, don't advance.
      if (std::next(current_buffer) == buffers_.end()) {
        break;
      }
      ++current_buffer;
      current_buffer_offset = 0;
    }
  }

  if (advance_position) {
    // We have less forward bytes and more backward bytes. Updates these
    // counters by `bytes_taken`.
    forward_bytes_ -= bytes_taken;
    backward_bytes_ += bytes_taken;
    CHECK(current_buffer_ != buffers_.end() || forward_bytes() == 0);

    current_buffer_ = current_buffer;
    current_buffer_offset_ = current_buffer_offset;

    UpdateCurrentTime(current_buffer_, current_buffer_offset_);
    EvictBackwardBuffers();
  }

  return bytes_taken;
}

void SeekableBuffer::UpdateCurrentTime(BufferQueue::iterator buffer,
                                       size_t offset) {
  // Garbage values are unavoidable, so this check will remain.
  if (buffer != buffers_.end() && (*buffer)->timestamp() != kNoTimestamp) {
    CHECK_LE(offset, (*buffer)->size());
    const int64_t time_offset =
        ((*buffer)->duration().InMicroseconds() * offset) / (*buffer)->size();

    current_time_ = (*buffer)->timestamp() + base::Microseconds(time_offset);
  }
}

}  // namespace media
