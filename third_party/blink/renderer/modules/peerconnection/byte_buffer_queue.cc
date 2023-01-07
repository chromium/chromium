// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/byte_buffer_queue.h"

namespace blink {

wtf_size_t ByteBufferQueue::ReadInto(base::span<uint8_t> buffer_out) {
  wtf_size_t read_amount = 0;
  while (!buffer_out.empty() && !deque_of_buffers_.empty()) {
    base::span<const uint8_t> front_buffer =
        base::make_span(deque_of_buffers_.front())
            .subspan(front_buffer_offset_);
    DCHECK_GT(front_buffer.size(), 0u);
    wtf_size_t buffer_read_amount =
        std::min(static_cast<wtf_size_t>(buffer_out.size()),
                 static_cast<wtf_size_t>(front_buffer.size()));
    memcpy(buffer_out.data(), front_buffer.data(), buffer_read_amount);
    read_amount += buffer_read_amount;
    buffer_out = buffer_out.subspan(buffer_read_amount);
    if (buffer_read_amount < front_buffer.size()) {
      front_buffer_offset_ += buffer_read_amount;
    } else {
      deque_of_buffers_.pop_front();
      front_buffer_offset_ = 0;
    }
  }
  size_ -= read_amount;
#if DCHECK_IS_ON()
  CheckInvariants();
#endif
  return read_amount;
}

void ByteBufferQueue::Append(Vector<uint8_t> buffer) {
  if (buffer.empty()) {
    return;
  }
  size_ += buffer.size();
  deque_of_buffers_.push_back(std::move(buffer));
#if DCHECK_IS_ON()
  CheckInvariants();
#endif
}

void ByteBufferQueue::Clear() {
  deque_of_buffers_.clear();
  front_buffer_offset_ = 0;
  size_ = 0;
#if DCHECK_IS_ON()
  CheckInvariants();
#endif
}

#if DCHECK_IS_ON()
void ByteBufferQueue::CheckInvariants() const {
  wtf_size_t buffer_size_sum = 0;
  for (const auto& buffer : deque_of_buffers_) {
    DCHECK(!buffer.empty());
    buffer_size_sum += buffer.size();
  }
  DCHECK_EQ(size_, buffer_size_sum - front_buffer_offset_);
  if (deque_of_buffers_.empty()) {
    DCHECK_EQ(front_buffer_offset_, 0u);
  } else {
    DCHECK_LT(front_buffer_offset_, deque_of_buffers_.front().size());
  }
}
#endif

}  // namespace blink
