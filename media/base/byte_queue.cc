// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/byte_queue.h"

#include <algorithm>
#include <cstring>

#include "base/check_op.h"
#include "base/numerics/checked_math.h"

namespace media {

ByteQueue::ByteQueue() : buffer_(new uint8_t[size_]) {}

ByteQueue::~ByteQueue() = default;

void ByteQueue::Reset() {
  offset_ = 0;
  used_ = 0;
}

void ByteQueue::Push(const uint8_t* data, int size) {
  DCHECK(data);
  DCHECK_GT(size, 0);

  // This can never overflow since used and size are both ints.
  const size_t size_needed = static_cast<size_t>(used_) + size;

  // Check to see if we need a bigger buffer.
  if (size_needed > size_) {
    // Growth is based on base::circular_deque which grows at 25%.
    const size_t safe_size =
        (base::CheckedNumeric<size_t>(size_) + size_ / 4).ValueOrDie();
    const size_t new_size = std::max(size_needed, safe_size);

    // Copy the data from the old buffer to the start of the new one.
    if (used_ > 0) {
      // Note: We could use realloc() here, but would need an additional move to
      // pack data at offset_ = 0 after a potential internal new allocation +
      // copy by realloc().
      //
      // In local tests on a few top video sites that ends up being the common
      // case, so just prefer to copy and pack ourselves.
      std::unique_ptr<uint8_t[]> new_buffer(new uint8_t[new_size]);
      memcpy(new_buffer.get(), Front(), used_);
      buffer_ = std::move(new_buffer);
    } else {
      // Free the existing |data| first so that the memory can be reused, if
      // possible. Note that the new array is purposely not initialized.
      buffer_.reset();
      buffer_.reset(new uint8_t[new_size]);
    }

    size_ = new_size;
    offset_ = 0;
  } else if ((offset_ + used_ + size) > size_) {
    // The buffer is big enough, but we need to move the data in the queue.
    memmove(buffer_.get(), Front(), used_);
    offset_ = 0;
  }

  memcpy(Front() + used_, data, size);
  used_ += size;
}

void ByteQueue::Peek(const uint8_t** data, int* size) const {
  DCHECK(data);
  DCHECK(size);
  *data = Front();
  *size = used_;
}

void ByteQueue::Pop(int count) {
  DCHECK_LE(count, used_);

  offset_ += count;
  used_ -= count;

  // Move the offset back to 0 if we have reached the end of the buffer.
  if (offset_ == size_) {
    DCHECK_EQ(used_, 0);
    offset_ = 0;
  }
}

uint8_t* ByteQueue::Front() const {
  return buffer_.get() + offset_;
}

}  // namespace media
