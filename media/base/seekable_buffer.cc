// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/seekable_buffer.h"

#include <algorithm>

#include "base/check_op.h"
#include "media/base/data_buffer.h"
#include "media/base/timestamp_constants.h"

namespace media {

SeekableBuffer::SeekableBuffer(int backward_capacity, int forward_capacity)
    : current_buffer_offset_(0),
      backward_capacity_(backward_capacity),
      backward_bytes_(0),
      forward_capacity_(forward_capacity),
      forward_bytes_(0),
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

int SeekableBuffer::Read(uint8_t* data, int size) {
  DCHECK(data);
  return InternalRead(data, size, true, 0);
}

int SeekableBuffer::Peek(uint8_t* data, int size, int forward_offset) {
  DCHECK(data);
  return InternalRead(data, size, false, forward_offset);
}

bool SeekableBuffer::GetCurrentChunk(const uint8_t** data, int* size) const {
  BufferQueue::iterator current_buffer = current_buffer_;
  int current_buffer_offset = current_buffer_offset_;
  // Advance position if we are in the end of the current buffer.
  while (current_buffer != buffers_.end() &&
         current_buffer_offset >= (*current_buffer)->data_size()) {
    ++current_buffer;
    current_buffer_offset = 0;
  }
  if (current_buffer == buffers_.end())
    return false;
  *data = (*current_buffer)->data() + current_buffer_offset;
  *size = (*current_buffer)->data_size() - current_buffer_offset;
  return true;
}

bool SeekableBuffer::Append(const scoped_refptr<DataBuffer>& buffer_in) {
  if (buffers_.empty() && buffer_in->timestamp() != kNoTimestamp) {
    current_time_ = buffer_in->timestamp();
  }

  // Since the forward capacity is only used to check the criteria for buffer
  // full, we always append data to the buffer.
  buffers_.push_back(buffer_in);

  // After we have written the first buffer, update |current_buffer_| to point
  // to it.
  if (current_buffer_ == buffers_.end()) {
    DCHECK_EQ(0, forward_bytes_);
    current_buffer_ = buffers_.begin();
  }

  // Update the |forward_bytes_| counter since we have more bytes.
  forward_bytes_ += buffer_in->data_size();

  // Advise the user to stop append if the amount of forward bytes exceeds
  // the forward capacity. A false return value means the user should stop
  // appending more data to this buffer.
  if (forward_bytes_ >= forward_capacity_)
    return false;
  return true;
}

bool SeekableBuffer::Append(const uint8_t* data, int size) {
  if (size > 0) {
    scoped_refptr<DataBuffer> data_buffer =
        DataBuffer::CopyFrom(base::make_span(data, static_cast<size_t>(size)));
    return Append(data_buffer);
  } else {
    // Return true if we have forward capacity.
    return forward_bytes_ < forward_capacity_;
  }
}

bool SeekableBuffer::Seek(int32_t offset) {
  if (offset > 0)
    return SeekForward(offset);
  else if (offset < 0)
    return SeekBackward(-offset);
  return true;
}

bool SeekableBuffer::SeekForward(int size) {
  // Perform seeking forward only if we have enough bytes in the queue.
  if (size > forward_bytes_)
    return false;

  // Do a read of |size| bytes.
  int taken = InternalRead(NULL, size, true, 0);
  DCHECK_EQ(taken, size);
  return true;
}

bool SeekableBuffer::SeekBackward(int size) {
  if (size > backward_bytes_)
    return false;
  // Record the number of bytes taken.
  int taken = 0;
  // Loop until we taken enough bytes and rewind by the desired |size|.
  while (taken < size) {
    // |current_buffer_| can never be invalid when we are in this loop. It can
    // only be invalid before any data is appended. The invalid case should be
    // handled by checks before we enter this loop.
    DCHECK(current_buffer_ != buffers_.end());

    // We try to consume at most |size| bytes in the backward direction. We also
    // have to account for the offset we are in the current buffer, so take the
    // minimum between the two to determine the amount of bytes to take from the
    // current buffer.
    int consumed = std::min(size - taken, current_buffer_offset_);

    // Decreases the offset in the current buffer since we are rewinding.
    current_buffer_offset_ -= consumed;

    // Increase the amount of bytes taken in the backward direction. This
    // determines when to stop the loop.
    taken += consumed;

    // Forward bytes increases and backward bytes decreases by the amount
    // consumed in the current buffer.
    forward_bytes_ += consumed;
    backward_bytes_ -= consumed;
    DCHECK_GE(backward_bytes_, 0);

    // The current buffer pointed by current iterator has been consumed. Move
    // the iterator backward so it points to the previous buffer.
    if (current_buffer_offset_ == 0) {
      if (current_buffer_ == buffers_.begin())
        break;
      // Move the iterator backward.
      --current_buffer_;
      // Set the offset into the current buffer to be the buffer size as we
      // are preparing for rewind for next iteration.
      current_buffer_offset_ = (*current_buffer_)->data_size();
    }
  }

  UpdateCurrentTime(current_buffer_, current_buffer_offset_);

  DCHECK_EQ(taken, size);
  return true;
}

void SeekableBuffer::EvictBackwardBuffers() {
  // Advances the iterator until we hit the current pointer.
  while (backward_bytes_ > backward_capacity_) {
    auto i = buffers_.begin();
    if (i == current_buffer_)
      break;
    scoped_refptr<DataBuffer> buffer = *i;
    backward_bytes_ -= buffer->data_size();
    DCHECK_GE(backward_bytes_, 0);

    buffers_.erase(i);
  }
}

int SeekableBuffer::InternalRead(uint8_t* data,
                                 int size,
                                 bool advance_position,
                                 int forward_offset) {
  // Counts how many bytes are actually read from the buffer queue.
  int taken = 0;

  auto current_buffer = current_buffer_;
  int current_buffer_offset = current_buffer_offset_;

  int bytes_to_skip = forward_offset;
  while (taken < size) {
    // |current_buffer| is valid since the first time this buffer is appended
    // with data.
    if (current_buffer == buffers_.end())
      break;

    scoped_refptr<DataBuffer> buffer = *current_buffer;

    int remaining_bytes_in_buffer =
        buffer->data_size() - current_buffer_offset;

    if (bytes_to_skip == 0) {
      // Find the right amount to copy from the current buffer referenced by
      // |buffer|. We shall copy no more than |size| bytes in total and each
      // single step copied no more than the current buffer size.
      int copied = std::min(size - taken, remaining_bytes_in_buffer);

      // |data| is NULL if we are seeking forward, so there's no need to copy.
      if (data)
        memcpy(data + taken, buffer->data() + current_buffer_offset, copied);

      // Increase total number of bytes copied, which regulates when to end this
      // loop.
      taken += copied;

      // We have read |copied| bytes from the current buffer. Advances the
      // offset.
      current_buffer_offset += copied;
    } else {
      int skipped = std::min(remaining_bytes_in_buffer, bytes_to_skip);
      current_buffer_offset += skipped;
      bytes_to_skip -= skipped;
    }

    // The buffer has been consumed.
    if (current_buffer_offset == buffer->data_size()) {
      if (advance_position) {
        // Next buffer may not have timestamp, so we need to update current
        // timestamp before switching to the next buffer.
        UpdateCurrentTime(current_buffer, current_buffer_offset);
      }

      auto next = current_buffer;
      ++next;
      // If we are at the last buffer, don't advance.
      if (next == buffers_.end())
        break;

      // Advances the iterator.
      current_buffer = next;
      current_buffer_offset = 0;
    }
  }

  if (advance_position) {
    // We have less forward bytes and more backward bytes. Updates these
    // counters by |taken|.
    forward_bytes_ -= taken;
    backward_bytes_ += taken;
    DCHECK_GE(forward_bytes_, 0);
    DCHECK(current_buffer_ != buffers_.end() || forward_bytes_ == 0);

    current_buffer_ = current_buffer;
    current_buffer_offset_ = current_buffer_offset;

    UpdateCurrentTime(current_buffer_, current_buffer_offset_);
    EvictBackwardBuffers();
  }

  return taken;
}

void SeekableBuffer::UpdateCurrentTime(BufferQueue::iterator buffer,
                                       int offset) {
  // Garbage values are unavoidable, so this check will remain.
  if (buffer != buffers_.end() && (*buffer)->timestamp() != kNoTimestamp) {
    int64_t time_offset = ((*buffer)->duration().InMicroseconds() * offset) /
                          (*buffer)->data_size();

    current_time_ = (*buffer)->timestamp() + base::Microseconds(time_offset);
  }
}

}  // namespace media
