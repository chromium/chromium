// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/io_buffer.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/numerics/safe_math.h"

namespace net {

// TODO(eroman): IOBuffer is being converted to require buffer sizes and offsets
// be specified as "size_t" rather than "int" (crbug.com/488553). To facilitate
// this move (since LOTS of code needs to be updated), this function ensures
// that sizes can be safely converted to an "int" without truncation. The
// assert ensures calling this with an "int" argument is also safe.
void IOBuffer::AssertValidBufferSize(size_t size) {
  static_assert(sizeof(size_t) >= sizeof(int));
  base::CheckedNumeric<int>(size).ValueOrDie();
}

IOBuffer::IOBuffer() = default;

IOBuffer::IOBuffer(base::span<char> data)
    : data_(data.data()), size_(data.size()) {
  AssertValidBufferSize(size_);
}

IOBuffer::IOBuffer(base::span<uint8_t> data)
    : IOBuffer(base::as_writable_chars(data)) {}

IOBuffer::~IOBuffer() = default;

IOBufferWithSize::IOBufferWithSize() = default;

IOBufferWithSize::IOBufferWithSize(size_t buffer_size) {
  AssertValidBufferSize(buffer_size);
  storage_ = base::HeapArray<char>::Uninit(buffer_size);
  size_ = storage_.size();
  data_ = storage_.data();
}

IOBufferWithSize::~IOBufferWithSize() {
  // Clear pointer before this destructor makes it dangle.
  data_ = nullptr;
}

StringIOBuffer::StringIOBuffer(std::string s) : string_data_(std::move(s)) {
  // Can't pass `s.data()` directly to IOBuffer constructor since moving
  // from `s` may invalidate it. This is especially true for libc++ short
  // string optimization where the data may be held in the string variable
  // itself, instead of in a movable backing store.
  AssertValidBufferSize(string_data_.size());
  data_ = string_data_.data();
  size_ = string_data_.size();
}

StringIOBuffer::~StringIOBuffer() {
  // Clear pointer before this destructor makes it dangle.
  data_ = nullptr;
}

DrainableIOBuffer::DrainableIOBuffer(scoped_refptr<IOBuffer> base, size_t size)
    : IOBuffer(base->span().first(size)), base_(std::move(base)) {}

void DrainableIOBuffer::DidConsume(int bytes) {
  SetOffset(used_ + bytes);
}

int DrainableIOBuffer::BytesRemaining() const {
  return size_ - used_;
}

// Returns the number of consumed bytes.
int DrainableIOBuffer::BytesConsumed() const {
  return used_;
}

void DrainableIOBuffer::SetOffset(int bytes) {
  CHECK_GE(bytes, 0);
  CHECK_LE(bytes, size_);
  used_ = bytes;
  data_ = base_->data() + used_;
}

DrainableIOBuffer::~DrainableIOBuffer() {
  // Clear ptr before this destructor destroys the |base_| instance,
  // making it dangle.
  data_ = nullptr;
}

GrowableIOBuffer::GrowableIOBuffer() = default;

void GrowableIOBuffer::SetCapacity(int capacity) {
  CHECK_GE(capacity, 0);
  // this will get reset in `set_offset`.
  data_ = nullptr;
  size_ = 0;

  // realloc will crash if it fails.
  real_data_.reset(static_cast<char*>(realloc(real_data_.release(), capacity)));

  capacity_ = capacity;
  if (offset_ > capacity)
    set_offset(capacity);
  else
    set_offset(offset_);  // The pointer may have changed.
}

void GrowableIOBuffer::set_offset(int offset) {
  CHECK_GE(offset, 0);
  CHECK_LE(offset, capacity_);
  offset_ = offset;
  data_ = real_data_.get() + offset;
  size_ = capacity_ - offset;
}

int GrowableIOBuffer::RemainingCapacity() {
  return capacity_ - offset_;
}

base::span<uint8_t> GrowableIOBuffer::everything() {
  return base::as_writable_bytes(
      // SAFETY: The capacity_ is the size of the allocation.
      UNSAFE_BUFFERS(
          base::span(real_data_.get(), base::checked_cast<size_t>(capacity_))));
}

base::span<const uint8_t> GrowableIOBuffer::everything() const {
  return base::as_bytes(
      // SAFETY: The capacity_ is the size of the allocation.
      UNSAFE_BUFFERS(
          base::span(real_data_.get(), base::checked_cast<size_t>(capacity_))));
}

base::span<uint8_t> GrowableIOBuffer::span_before_offset() {
  return everything().first(base::checked_cast<size_t>(offset_));
}

base::span<const uint8_t> GrowableIOBuffer::span_before_offset() const {
  return everything().first(base::checked_cast<size_t>(offset_));
}

GrowableIOBuffer::~GrowableIOBuffer() {
  data_ = nullptr;
}

PickledIOBuffer::PickledIOBuffer() = default;

void PickledIOBuffer::Done() {
  data_ = const_cast<char*>(pickle_.data_as_char());
  size_ = pickle_.size();
}

PickledIOBuffer::~PickledIOBuffer() {
  // Avoid dangling ptr when this destructor destroys the pickle.
  data_ = nullptr;
}

WrappedIOBuffer::WrappedIOBuffer(base::span<const char> data)
    : IOBuffer(base::make_span(const_cast<char*>(data.data()), data.size())) {}

WrappedIOBuffer::WrappedIOBuffer(base::span<const uint8_t> data)
    : WrappedIOBuffer(base::as_chars(data)) {}

WrappedIOBuffer::~WrappedIOBuffer() = default;

}  // namespace net
