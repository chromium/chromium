// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/basic_data_buffer_factory.h"

#include <utility>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"

namespace network {

// static
const void* const BasicDataBuffer::kClassIdentifier =
    &BasicDataBuffer::kClassIdentifier;

BasicDataBuffer::BasicDataBuffer(base::PassKey<BasicDataBufferFactory>,
                                 base::span<const uint8_t> data)
    : buffer_(data.begin(), data.end()) {}

BasicDataBuffer::BasicDataBuffer(base::PassKey<BasicDataBufferFactory>,
                                 uint32_t size)
    : buffer_(size) {}

BasicDataBuffer::~BasicDataBuffer() = default;

base::span<const uint8_t> BasicDataBuffer::data() const {
  return buffer_;
}

base::span<uint8_t> BasicDataBuffer::data() {
  return buffer_;
}

void BasicDataBuffer::Shrink(uint32_t size) {
  CHECK_LE(size, buffer_.size());
  buffer_.resize(size);
}

const void* BasicDataBuffer::GetClassIdentifier() const {
  return kClassIdentifier;
}

// static
const void* const BasicDataBufferList::kClassIdentifier =
    &BasicDataBufferList::kClassIdentifier;

BasicDataBufferList::BasicDataBufferList(
    base::PassKey<BasicDataBufferFactory>) {}

BasicDataBufferList::~BasicDataBufferList() = default;

void BasicDataBufferList::Append(std::unique_ptr<DataBuffer> buffer) {
  CHECK(buffer);
  CHECK(!buffer->data().empty());
  buffers_.push_back(std::move(buffer));
}

uint32_t BasicDataBufferList::size() const {
  return base::checked_cast<uint32_t>(buffers_.size());
}

base::span<const uint8_t> BasicDataBufferList::Get(uint32_t index) const {
  CHECK_LT(index, buffers_.size());
  return buffers_[index]->data();
}

const void* BasicDataBufferList::GetClassIdentifier() const {
  return kClassIdentifier;
}

std::unique_ptr<DataBuffer> BasicDataBufferFactory::CreateDataBuffer(
    base::span<const uint8_t> data) {
  return std::make_unique<BasicDataBuffer>(
      base::PassKey<BasicDataBufferFactory>(), data);
}

std::unique_ptr<DataBuffer> BasicDataBufferFactory::AllocateDataBuffer(
    uint32_t size) {
  return std::make_unique<BasicDataBuffer>(
      base::PassKey<BasicDataBufferFactory>(), size);
}

std::unique_ptr<DataBufferList> BasicDataBufferFactory::CreateDataBufferList() {
  return std::make_unique<BasicDataBufferList>(
      base::PassKey<BasicDataBufferFactory>());
}

}  // namespace network
