// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/simple_cdm_buffer.h"

#include <limits>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"

namespace media {

// static
SimpleCdmBuffer* SimpleCdmBuffer::Create(size_t capacity) {
  DCHECK(capacity);

  // cdm::Buffer interface limits capacity to uint32.
  DCHECK_LE(capacity, std::numeric_limits<uint32_t>::max());
  return new SimpleCdmBuffer(base::checked_cast<uint32_t>(capacity));
}

SimpleCdmBuffer::SimpleCdmBuffer(uint32_t capacity)
    : buffer_(capacity), size_(0) {}

SimpleCdmBuffer::~SimpleCdmBuffer() = default;

void SimpleCdmBuffer::Destroy() {
  delete this;
}

uint32_t SimpleCdmBuffer::Capacity() const {
  return buffer_.size();
}

uint8_t* SimpleCdmBuffer::Data() {
  return buffer_.data();
}

void SimpleCdmBuffer::SetSize(uint32_t size) {
  DCHECK(size <= Capacity());
  size_ = size > Capacity() ? 0 : size;
}

uint32_t SimpleCdmBuffer::Size() const {
  return size_;
}

}  // namespace media
