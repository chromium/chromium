// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/io_buffer.h"

#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/numerics/safe_math.h"
#include "base/pickle.h"

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

IOBuffer::IOBuffer(base::span<char> span)
    : IOBuffer(base::as_writable_bytes(span)) {}

IOBuffer::IOBuffer(base::span<uint8_t> span) : span_(span) {
  AssertValidBufferSize(span_.size());
}

IOBuffer::~IOBuffer() = default;

void IOBuffer::SetSpan(base::span<uint8_t> span) {
  AssertValidBufferSize(span.size());
  span_ = span;
}

void IOBuffer::ClearSpan() {
  span_ = base::span<uint8_t>();
}

IOBufferWithSize::IOBufferWithSize() = default;

IOBufferWithSize::IOBufferWithSize(size_t buffer_size)
    : storage_(base::HeapArray<uint8_t>::Uninit(buffer_size)) {
  SetSpan(storage_);
}

IOBufferWithSize::~IOBufferWithSize() {
  // Clear pointer before this destructor makes it dangle.
  ClearSpan();
}

VectorIOBuffer::VectorIOBuffer(std::vector<uint8_t> vector)
    : vector_(std::move(vector)) {
  SetSpan(vector_);
}

VectorIOBuffer::VectorIOBuffer(base::span<const uint8_t> span)
    : VectorIOBuffer(std::vector(span.begin(), span.end())) {}

VectorIOBuffer::~VectorIOBuffer() {
  // Clear pointer before this destructor makes it dangle.
  ClearSpan();
}

StringIOBuffer::StringIOBuffer(std::string s) : string_data_(std::move(s)) {
  // Can't pass `s.data()` directly to IOBuffer constructor since moving
  // from `s` may invalidate it. This is especially true for libc++ short
  // string optimization where the data may be held in the string variable
  // itself, instead of in a movable backing store.
  SetSpan(base::as_writable_bytes(base::span(string_data_)));
}

StringIOBuffer::~StringIOBuffer() {
  // Clear pointer before this destructor makes it dangle.
  ClearSpan();
}

DrainableIOBuffer::DrainableIOBuffer(scoped_refptr<IOBuffer> base, size_t size)
    : IOBuffer(base->first(size)), base_(std::move(base)) {}

void DrainableIOBuffer::DidConsume(int bytes) {
  SetOffset(used_ + bytes);
}

int DrainableIOBuffer::BytesRemaining() const {
  return size();
}

// Returns the number of consumed bytes.
int DrainableIOBuffer::BytesConsumed() const {
  return used_;
}

void DrainableIOBuffer::SetOffset(int bytes) {
  CHECK_GE(bytes, 0);
  // Length from the start of `base_` to the end of the buffer passed in to the
  // constructor isn't stored anywhere, so need to calculate it.
  int length = size() + used_;
  CHECK_LE(bytes, length);
  used_ = bytes;
  SetSpan(base_->span().subspan(base::checked_cast<size_t>(used_),
                                base::checked_cast<size_t>(length - bytes)));
}

DrainableIOBuffer::~DrainableIOBuffer() {
  // Clear ptr before this destructor destroys the |base_| instance,
  // making it dangle.
  ClearSpan();
}

GrowableIOBuffer::GrowableIOBuffer() = default;

void GrowableIOBuffer::SetCapacity(int capacity) {
  CHECK_GE(capacity, 0);

  // The span will be set again in `set_offset()`. Need to clear raw pointers to
  // the data before reallocating the buffer.
  ClearSpan();

  // realloc will crash if it fails.
  real_data_.reset(
      static_cast<uint8_t*>(realloc(real_data_.release(), capacity)));

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

  UNSAFE_TODO(SetSpan(base::span<uint8_t>(
      real_data_.get() + offset, static_cast<size_t>(capacity_ - offset))));
}

void GrowableIOBuffer::DidConsume(int bytes) {
  CHECK_LE(0, bytes);
  CHECK_LE(bytes, size());
  set_offset(offset_ + bytes);
}

int GrowableIOBuffer::RemainingCapacity() {
  return size();
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
  ClearSpan();
}

PickledIOBuffer::PickledIOBuffer(std::unique_ptr<const base::Pickle> pickle)
    :  // SAFETY: const cast does not affect size.
      IOBuffer(UNSAFE_BUFFERS(
          base::span(const_cast<uint8_t*>(pickle->data()), pickle->size()))),
      pickle_(std::move(pickle)) {}

PickledIOBuffer::~PickledIOBuffer() {
  // Avoid dangling ptr when this destructor destroys the pickle.
  ClearSpan();
}

WrappedIOBuffer::WrappedIOBuffer(base::span<const char> data)
    : WrappedIOBuffer(base::as_byte_span(data)) {}

WrappedIOBuffer::WrappedIOBuffer(base::span<const uint8_t> data)
    // SAFETY: const cast does not affect size.
    : IOBuffer(UNSAFE_BUFFERS(
          base::span(const_cast<uint8_t*>(data.data()), data.size()))) {}

WrappedIOBuffer::~WrappedIOBuffer() = default;

}  // namespace net
