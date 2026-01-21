// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc_logging/common/partial_circular_buffer.h"

#include <algorithm>
#include <cstring>

#include "base/check_op.h"
#include "base/compiler_specific.h"

namespace webrtc_logging {

PartialCircularBuffer::PartialCircularBuffer(base::span<uint8_t> buffer)
    : buffer_data_(reinterpret_cast<BufferData*>(buffer.data())),
      position_(0),
      total_read_(0) {
  uint32_t header_size = offsetof(BufferData, data);
  uint32_t data_size = buffer.size() - header_size;
  buffer_data_span_ = base::raw_span<uint8_t>(buffer.subspan(header_size));
  DCHECK_EQ(data_size, buffer_data_span_.size());

  DCHECK(buffer_data_);
  DCHECK_GE(buffer.size(), header_size);
  DCHECK_LE(buffer_data_->total_written, data_size);
  DCHECK_LT(buffer_data_->wrap_position, data_size);
  DCHECK_LT(buffer_data_->end_position, data_size);
}

PartialCircularBuffer::PartialCircularBuffer(base::span<uint8_t> buffer,
                                             uint32_t wrap_position,
                                             bool append)
    : buffer_data_(reinterpret_cast<BufferData*>(buffer.data())),
      position_(0),
      total_read_(0) {
  uint32_t header_size = offsetof(BufferData, data);
  uint32_t data_size = buffer.size() - header_size;
  buffer_data_span_ = base::raw_span<uint8_t>(buffer.subspan(header_size));
  DCHECK_EQ(data_size, buffer_data_span_.size());

  DCHECK(buffer_data_);
  DCHECK_GE(buffer.size(), header_size);

  if (append) {
    DCHECK_LT(buffer_data_->wrap_position, data_size);
    position_ = buffer_data_->end_position;
  } else {
    DCHECK_LT(wrap_position, data_size);
    buffer_data_->total_written = 0;
    buffer_data_->wrap_position = wrap_position;
    buffer_data_->end_position = 0;
  }
}

uint32_t PartialCircularBuffer::Read(base::span<uint8_t> buffer) {
  DCHECK(buffer_data_);
  if (total_read_ >= buffer_data_->total_written)
    return 0;

  uint32_t read = 0;

  // Read from beginning part.
  if (position_ < buffer_data_->wrap_position) {
    uint32_t to_wrap_pos = buffer_data_->wrap_position - position_;
    uint32_t to_eow = buffer_data_->total_written - total_read_;
    uint32_t to_read =
        std::min({buffer.size(), static_cast<size_t>(to_wrap_pos),
                  static_cast<size_t>(to_eow)});
    buffer.copy_prefix_from(buffer_data_span_.subspan(position_, to_read));
    position_ += to_read;
    total_read_ += to_read;
    read += to_read;
    if (position_ == buffer_data_->wrap_position &&
        buffer_data_->total_written == buffer_data_span_.size()) {
      // We've read all the beginning part, set the position to the middle part.
      // (The second condition above checks if the wrapping part is filled, i.e.
      // writing has wrapped.)
      position_ = buffer_data_->end_position;
    }
    if (read >= buffer.size()) {
      DCHECK_EQ(read, buffer.size());
      return read;
    }
    if (read >= to_eow) {
      DCHECK_EQ(read, to_eow);
      DCHECK_EQ(total_read_, buffer_data_->total_written);
      return read;
    }
  }

  // Read from middle part.
  DCHECK_GE(position_, buffer_data_->wrap_position);
  if (position_ >= buffer_data_->end_position) {
    uint32_t remaining_buffer_size = buffer.size() - read;
    uint32_t to_eof = buffer_data_span_.size() - position_;
    uint32_t to_eow = buffer_data_->total_written - total_read_;
    uint32_t to_read = std::min({remaining_buffer_size, to_eof, to_eow});
    buffer.subspan(read).copy_prefix_from(
        buffer_data_span_.subspan(position_, to_read));
    position_ += to_read;
    total_read_ += to_read;
    read += to_read;
    if (position_ == buffer_data_span_.size()) {
      // We've read all the middle part, set position to the end part.
      position_ = buffer_data_->wrap_position;
    }
    if (read >= buffer.size()) {
      DCHECK_EQ(read, buffer.size());
      return read;
    }
    if (total_read_ >= buffer_data_->total_written) {
      DCHECK_EQ(total_read_, buffer_data_->total_written);
      return read;
    }
  }

  // Read from end part.
  DCHECK_GE(position_, buffer_data_->wrap_position);
  DCHECK_LT(position_, buffer_data_->end_position);
  uint32_t remaining_buffer_size = buffer.size() - read;
  uint32_t to_eob = buffer_data_->end_position - position_;
  uint32_t to_eow = buffer_data_->total_written - total_read_;
  uint32_t to_read = std::min({remaining_buffer_size, to_eob, to_eow});
  buffer.subspan(read).copy_prefix_from(
      buffer_data_span_.subspan(position_, to_read));
  position_ += to_read;
  total_read_ += to_read;
  read += to_read;
  DCHECK_LE(read, buffer.size());
  DCHECK_LE(total_read_, buffer_data_->total_written);
  return read;
}

void PartialCircularBuffer::Write(base::span<const uint8_t> buffer) {
  DCHECK(buffer_data_);
  uint32_t wrap_position = buffer_data_->wrap_position;
  uint32_t cycle_size = buffer_data_span_.size() - wrap_position;

  // First write the non-wrapping part.
  if (position_ < wrap_position) {
    uint32_t space_left = wrap_position - position_;
    uint32_t write_size =
        std::min(buffer.size(), static_cast<size_t>(space_left));
    DoWrite(buffer.take_first(write_size));
  }

  // Skip the part that would overlap.
  if (buffer.size() > cycle_size) {
    uint32_t skip = buffer.size() - cycle_size;
    buffer.take_first(skip);
    position_ = wrap_position + (position_ - wrap_position + skip) % cycle_size;
  }

  // Finally write the wrapping part.
  DoWrite(buffer);
}

void PartialCircularBuffer::DoWrite(base::span<const uint8_t> input) {
  DCHECK_LT(position_, buffer_data_span_.size());
  buffer_data_->total_written = std::min(
      buffer_data_->total_written + input.size(), buffer_data_span_.size());

  // Write() skips any overlapping part, so this loop will run at most twice.
  while (input.size() > 0) {
    uint32_t space_left = buffer_data_span_.size() - position_;
    uint32_t write_size =
        std::min(input.size(), static_cast<size_t>(space_left));
    buffer_data_span_.subspan(position_).copy_prefix_from(
        input.take_first(write_size));
    position_ += write_size;
    if (position_ >= buffer_data_span_.size()) {
      DCHECK_EQ(position_, buffer_data_span_.size());
      position_ = buffer_data_->wrap_position;
    }
  }

  buffer_data_->end_position = position_;
}

}  // namespace webrtc_logging
