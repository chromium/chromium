// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/byte_queue.h"

#include <algorithm>
#include <cstring>

#include "base/check_op.h"
#include "base/numerics/checked_math.h"
#include "base/process/memory.h"

namespace media {

ByteQueue::ByteQueue() {
  uint8_t* new_buffer = nullptr;

  // Though ::Push() is allowed to fail memory allocation for `buffer_`, do not
  // allow memory allocation failure here during ByteQueue construction.
  // TODO(crbug.com/40204179): Consider refactoring to an Initialize() method
  // that does this allocation and that can indicate failure, so callers can
  // more gracefully handle the former OOM case that now fails this CHECK. For
  // example, some StreamParsers create additional ByteQueues during Parse, so
  // such handling could be a parse error in that case. Other handling
  // customization could be done where ByteQueues are created as part of
  // StreamParser creation.
  CHECK(base::UncheckedMalloc(size_, reinterpret_cast<void**>(&new_buffer)) &&
        new_buffer);
  buffer_.reset(new_buffer);
}

ByteQueue::~ByteQueue() = default;

void ByteQueue::Reset() {
  offset_ = 0;
  used_ = 0;
}

bool ByteQueue::Push(base::span<const uint8_t> data) {
  DCHECK(!data.empty());

  // This can never overflow since used and size are both ints.
  const size_t size_needed = static_cast<size_t>(used_) + data.size();

  // Check to see if we need a bigger buffer.
  if (size_needed > size_) {
    // Growth is based on base::circular_deque which grows at 25%.
    const size_t safe_size =
        (base::CheckedNumeric<size_t>(size_) + size_ / 4).ValueOrDie();
    const size_t new_size = std::max(size_needed, safe_size);

    // Try to obtain a new backing buffer of `new_size` capacity. Note: If
    // `used_` is positive, we could use realloc() here, but would need an
    // additional move to pack data at offset_ = 0 after a potential internal
    // new allocation + copy by realloc(). In local tests on a few top video
    // sites that ends up being the common case, so just prefer to copy and pack
    // ourselves. Further, we need to handle potential allocation failure, since
    // callers may have fallback paths for that scenario, and the allocation
    // path allowing this must not be used with realloc.
    uint8_t* new_buffer = nullptr;
    if (!base::UncheckedMalloc(new_size,
                               reinterpret_cast<void**>(&new_buffer)) ||
        !new_buffer) {
      return false;
    }

    // Note that the new array is purposely not initialized. Copy the data, if
    // any, from the old buffer to the start of the new one.
    if (used_ > 0) {
      memcpy(new_buffer, Front(), used_);
    }

    buffer_.reset(new_buffer);  // This also frees the previous `buffer_`.
    size_ = new_size;
    offset_ = 0;
  } else if ((offset_ + used_ + data.size()) > size_) {
    // The buffer is big enough, but we need to move the data in the queue.
    memmove(buffer_.get(), Front(), used_);
    offset_ = 0;
  }

  memcpy(Front() + used_, data.data(), data.size());
  used_ += data.size();

  return true;
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
