// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/bindings/lib/buffer.h"

#include <cstring>
#include <tuple>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"

namespace mojo {
namespace internal {

Buffer::Buffer() = default;

Buffer::Buffer(void* data, size_t size, size_t cursor)
    : data_(data), size_(size), cursor_(cursor) {
  DCHECK(IsAligned(data_));
}

Buffer::Buffer(MessageHandle message,
               size_t message_payload_size,
               void* data,
               size_t size)
    : message_(message),
      message_payload_size_(message_payload_size),
      data_(data),
      size_(size),
      cursor_(0) {
  DCHECK(IsAligned(data_));
}

Buffer::Buffer(Buffer&& other) {
  *this = std::move(other);
}

Buffer::~Buffer() = default;

Buffer& Buffer::operator=(Buffer&& other) {
  message_ = other.message_;
  message_payload_size_ = other.message_payload_size_;
  data_ = other.data_;
  size_ = other.size_;
  cursor_ = other.cursor_;
  other.Reset();
  return *this;
}

size_t Buffer::Allocate(size_t num_bytes) {
  const size_t aligned_num_bytes = Align(num_bytes);
  const size_t new_cursor = cursor_ + aligned_num_bytes;
  if (new_cursor < cursor_ || (new_cursor > size_ && !message_.is_valid())) {
    // Either we've overflowed or exceeded a fixed capacity.
    NOTREACHED();
  }

  if (new_cursor > size_) {
    // If we have an underlying message object we can extend its payload to
    // obtain more storage capacity.
    DCHECK_LE(message_payload_size_, new_cursor);
    size_t additional_bytes = new_cursor - message_payload_size_;
    DCHECK(base::IsValueInRangeForNumericType<uint32_t>(additional_bytes));
    uint32_t new_size;
    MojoResult rv = MojoAppendMessageData(
        message_.value(), static_cast<uint32_t>(additional_bytes), nullptr, 0,
        nullptr, &data_, &new_size);
    DCHECK_EQ(MOJO_RESULT_OK, rv);
    message_payload_size_ = new_cursor;
    size_ = new_size;
  }

  DCHECK_LE(new_cursor, size_);
  size_t block_start = cursor_;
  cursor_ = new_cursor;

  // Ensure that all the allocated space is zeroed to avoid uninitialized bits
  // leaking into messages.
  //
  // TODO(rockot): We should consider only clearing the alignment padding. This
  // means being careful about generated bindings zeroing padding explicitly,
  // which itself gets particularly messy with e.g. packed bool bitfields.
  memset(static_cast<uint8_t*>(data_) + block_start, 0, aligned_num_bytes);

  return block_start;
}

bool Buffer::AttachHandles(std::vector<ScopedHandle>* handles) {
  DCHECK(message_.is_valid());

  uint32_t new_size = 0;
  MojoResult rv = MojoAppendMessageData(
      message_.value(), 0, reinterpret_cast<MojoHandle*>(handles->data()),
      static_cast<uint32_t>(handles->size()), nullptr, &data_, &new_size);
  if (rv != MOJO_RESULT_OK)
    return false;

  size_ = new_size;
  for (auto& handle : *handles)
    std::ignore = handle.release();
  handles->clear();
  return true;
}

void Buffer::Seal() {
  if (!message_.is_valid())
    return;

  // Ensure that the backing message has the final accumulated payload size.
  DCHECK_LE(message_payload_size_, cursor_);
  size_t additional_bytes = cursor_ - message_payload_size_;
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(additional_bytes));

  MojoAppendMessageDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
  void* data;
  uint32_t size;
  MojoResult rv = MojoAppendMessageData(message_.value(),
                                        static_cast<uint32_t>(additional_bytes),
                                        nullptr, 0, &options, &data, &size);
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  message_ = MessageHandle();
  message_payload_size_ = cursor_;
  data_ = data;
  size_ = size;
}

void Buffer::Reset() {
  message_ = MessageHandle();
  data_ = nullptr;
  size_ = 0;
  cursor_ = 0;
}

}  // namespace internal
}  // namespace mojo
