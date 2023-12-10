// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/io_buffer.h"

#include <utility>

#include "base/check_op.h"
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

IOBuffer::IOBuffer(char* data, size_t size) : data_(data), size_(size) {
  AssertValidBufferSize(size);
}

IOBuffer::~IOBuffer() = default;

IOBufferWithSize::IOBufferWithSize() = default;

IOBufferWithSize::IOBufferWithSize(size_t buffer_size) {
  AssertValidBufferSize(buffer_size);
  if (buffer_size) {
    size_ = buffer_size;
    data_ = new char[buffer_size];
  }
}

IOBufferWithSize::~IOBufferWithSize() {
  data_.ClearAndDeleteArray();
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
    : IOBuffer(base->data(), size), base_(std::move(base)) {}

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

char* GrowableIOBuffer::StartOfBuffer() {
  return real_data_.get();
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

WrappedIOBuffer::WrappedIOBuffer(const char* data, size_t size)
    : IOBuffer(const_cast<char*>(data), size) {}

WrappedIOBuffer::~WrappedIOBuffer() = default;

}  // namespace net
