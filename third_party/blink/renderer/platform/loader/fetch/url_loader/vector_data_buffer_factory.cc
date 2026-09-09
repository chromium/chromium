// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/vector_data_buffer_factory.h"

#include <stdint.h>

#include "base/check_op.h"
#include "base/containers/span.h"

namespace blink {

VectorDataBufferFactory::VectorDataBufferFactory() = default;

VectorDataBufferFactory::~VectorDataBufferFactory() = default;

std::unique_ptr<network::DataBuffer> VectorDataBufferFactory::CreateDataBuffer(
    base::span<const uint8_t> data) {
  return std::make_unique<VectorDataBuffer>(
      base::PassKey<VectorDataBufferFactory>(), data);
}

std::unique_ptr<network::DataBuffer>
VectorDataBufferFactory::AllocateDataBuffer(uint32_t size) {
  return std::make_unique<VectorDataBuffer>(
      base::PassKey<VectorDataBufferFactory>(), size);
}

std::unique_ptr<network::DataBufferList>
VectorDataBufferFactory::CreateDataBufferList() {
  return std::make_unique<SegmentedDataBufferList>(
      base::PassKey<VectorDataBufferFactory>());
}

const void* const VectorDataBuffer::kClassIdentifier =
    &VectorDataBuffer::kClassIdentifier;

VectorDataBuffer::VectorDataBuffer(base::PassKey<VectorDataBufferFactory>,
                                   base::span<const uint8_t> data) {
  buffer_.append_range(base::as_chars(data));
}

VectorDataBuffer::VectorDataBuffer(base::PassKey<VectorDataBufferFactory>,
                                   uint32_t size) {
  buffer_.Grow(size);
}

VectorDataBuffer::~VectorDataBuffer() = default;

base::span<const uint8_t> VectorDataBuffer::data() const {
  return base::as_bytes(base::span(buffer_));
}

base::span<uint8_t> VectorDataBuffer::data() {
  return base::as_writable_bytes(base::span(buffer_));
}

void VectorDataBuffer::Shrink(uint32_t size) {
  CHECK_LE(size, buffer_.size());
  buffer_.Shrink(size);
}

const void* VectorDataBuffer::GetClassIdentifier() const {
  return kClassIdentifier;
}

Vector<char> VectorDataBuffer::TakeInternalBuffer() {
  return std::move(buffer_);
}

// static
const void* const SegmentedDataBufferList::kClassIdentifier =
    &SegmentedDataBufferList::kClassIdentifier;

SegmentedDataBufferList::SegmentedDataBufferList(
    base::PassKey<VectorDataBufferFactory>) {}
SegmentedDataBufferList::~SegmentedDataBufferList() = default;

void SegmentedDataBufferList::Append(
    std::unique_ptr<network::DataBuffer> buffer) {
  CHECK(buffer);
  CHECK(!buffer->data().empty());
  if (buffer->GetClassIdentifier() == VectorDataBuffer::kClassIdentifier) {
    // Zero-copy path: safely downcast to VectorDataBuffer and move its internal
    // buffer into the SegmentedBuffer directly.
    auto* vector_buffer = static_cast<VectorDataBuffer*>(buffer.get());
    buffer_.Append(vector_buffer->TakeInternalBuffer());
  } else {
    // Fallback path: copy data via span for other DataBuffer implementations.
    // Using base::as_chars because SegmentedBuffer::Append takes
    // base::span<const char>.
    buffer_.Append(base::as_chars(buffer->data()));
  }
}

uint32_t SegmentedDataBufferList::size() const {
  return buffer_.GetSegmentCount();
}

base::span<const uint8_t> SegmentedDataBufferList::Get(uint32_t index) const {
  CHECK_LT(index, buffer_.GetSegmentCount());
  return base::as_bytes(buffer_.GetSegment(index));
}

const void* SegmentedDataBufferList::GetClassIdentifier() const {
  return kClassIdentifier;
}

}  // namespace blink
