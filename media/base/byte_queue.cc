// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/byte_queue.h"

#include <algorithm>
#include <cstring>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/numerics/checked_math.h"
#include "base/process/memory.h"

namespace media {
namespace {
// Default starting size for the queue.
constexpr size_t kDefaultQueueSize = 1024;

base::HeapArray<uint8_t, base::UncheckedFreeDeleter> MallocHeapArray(
    size_t size) {
  uint8_t* new_buffer_ptr = nullptr;
  if (!base::UncheckedMalloc(size, reinterpret_cast<void**>(&new_buffer_ptr)) ||
      !new_buffer_ptr) {
    return {};
  }

  // SAFETY: `base::UncheckedMalloc` returns a null pointer when the allocation
  // fails. But the above if has already checked the null pointer case. So we
  // can assume it is safe.
  return UNSAFE_BUFFERS(
      base::HeapArray<uint8_t, base::UncheckedFreeDeleter>::FromOwningPointer(
          new_buffer_ptr, size));
}

}  // namespace

ByteQueue::ByteQueue() {
  // Though ::Push() is allowed to fail memory allocation for `buffer_`, do not
  // allow memory allocation failure here during ByteQueue construction.
  // TODO(crbug.com/40204179): Consider refactoring to an Initialize() method
  // that does this allocation and that can indicate failure, so callers can
  // more gracefully handle the former OOM case that now fails this CHECK. For
  // example, some StreamParsers create additional ByteQueues during Parse, so
  // such handling could be a parse error in that case. Other handling
  // customization could be done where ByteQueues are created as part of
  // StreamParser creation.
  buffer_ = MallocHeapArray(kDefaultQueueSize);
  CHECK(!buffer_.empty());
}

ByteQueue::~ByteQueue() = default;

void ByteQueue::Reset() {
  offset_ = 0;
  data_ = {};
}

bool ByteQueue::Push(base::span<const uint8_t> data) {
  DCHECK(!data.empty());

  // This can never overflow since used and size are both ints.
  const size_t size_needed = static_cast<size_t>(data_.size()) + data.size();

  // Check to see if we need a bigger buffer.
  if (size_needed > buffer_.size()) {
    // Growth is based on base::circular_deque which grows at 25%.
    const size_t safe_size =
        (base::CheckedNumeric<size_t>(buffer_.size()) + buffer_.size() / 4)
            .ValueOrDie();
    const size_t new_size = std::max(size_needed, safe_size);

    // Try to obtain a new backing buffer of `new_size` capacity. Note: If
    // `used_` is positive, we could use realloc() here, but would need an
    // additional move to pack data at offset_ = 0 after a potential internal
    // new allocation + copy by realloc(). In local tests on a few top video
    // sites that ends up being the common case, so just prefer to copy and pack
    // ourselves. Further, we need to handle potential allocation failure, since
    // callers may have fallback paths for that scenario, and the allocation
    // path allowing this must not be used with realloc.
    auto new_buffer = MallocHeapArray(new_size);
    if (new_buffer.empty()) {
      return false;
    }

    base::raw_span<uint8_t> new_data = new_buffer.first(data_.size());
    // Note that the new array is purposely not initialized. Copy the data, if
    // any, from the old buffer to the start of the new one.
    if (!data_.empty()) {
      new_data.copy_from_nonoverlapping(data_);
    }

    data_ = new_data;
    buffer_ = std::move(new_buffer);  // This also frees the previous `buffer_`.
    offset_ = 0;
  } else if ((offset_ + size_needed) > buffer_.size()) {
    // The buffer is big enough, but we need to move the data in the queue.
    buffer_.copy_prefix_from(data_);
    offset_ = 0;
  }

  buffer_.subspan(offset_ + data_.size()).copy_prefix_from(data);
  data_ = buffer_.subspan(offset_, size_needed);

  return true;
}

void ByteQueue::Pop(int count) {
  DCHECK_LE(base::checked_cast<size_t>(count), data_.size());

  offset_ += count;
  data_ = data_.subspan(base::checked_cast<size_t>(count));

  // Move the offset back to 0 if we have reached the end of the buffer.
  if (offset_ == buffer_.size()) {
    DCHECK(data_.empty());
    offset_ = 0;
  }
}

}  // namespace media
