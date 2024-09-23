// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/base/big_buffer.h"

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"

namespace mojo_base {

namespace internal {

BigBufferSharedMemoryRegion::BigBufferSharedMemoryRegion() : size_(0) {}

BigBufferSharedMemoryRegion::BigBufferSharedMemoryRegion(
    mojo::ScopedSharedBufferHandle buffer_handle,
    size_t size)
    : size_(size),
      buffer_handle_(std::move(buffer_handle)),
      buffer_mapping_(buffer_handle_->Map(size)) {}

BigBufferSharedMemoryRegion::BigBufferSharedMemoryRegion(
    BigBufferSharedMemoryRegion&& other) = default;

BigBufferSharedMemoryRegion::~BigBufferSharedMemoryRegion() = default;

BigBufferSharedMemoryRegion& BigBufferSharedMemoryRegion::operator=(
    BigBufferSharedMemoryRegion&& other) = default;

mojo::ScopedSharedBufferHandle BigBufferSharedMemoryRegion::TakeBufferHandle() {
  DCHECK(buffer_handle_.is_valid());
  buffer_mapping_.reset();
  return std::move(buffer_handle_);
}

}  // namespace internal

namespace {

void TryCreateSharedMemory(
    size_t size,
    BigBuffer::StorageType* storage_type,
    std::optional<internal::BigBufferSharedMemoryRegion>* shared_memory) {
  if (size > BigBuffer::kMaxInlineBytes) {
    auto buffer = mojo::SharedBufferHandle::Create(size);
    if (buffer.is_valid()) {
      internal::BigBufferSharedMemoryRegion shm_region(std::move(buffer), size);
      if (shm_region.memory()) {
        *storage_type = BigBuffer::StorageType::kSharedMemory;
        shared_memory->emplace(std::move(shm_region));
        return;
      }
    }
  }

  // We can use inline memory, either because the data was small or shared
  // memory allocation failed.
  *storage_type = BigBuffer::StorageType::kBytes;
}

}  // namespace

// static
constexpr size_t BigBuffer::kMaxInlineBytes;

BigBuffer::BigBuffer() : storage_type_(StorageType::kBytes) {}

BigBuffer::BigBuffer(BigBuffer&& other)
    : storage_type_(other.storage_type_),
      bytes_(std::move(other.bytes_)),
      shared_memory_(std::move(other.shared_memory_)) {
  // Make sure |other| looks empty.
  other.storage_type_ = StorageType::kInvalidBuffer;
}

BigBuffer::BigBuffer(base::span<const uint8_t> data) {
  *this = BigBufferView::ToBigBuffer(BigBufferView(data));
}

BigBuffer::BigBuffer(const std::vector<uint8_t>& data)
    : BigBuffer(base::make_span(data)) {}

BigBuffer::BigBuffer(internal::BigBufferSharedMemoryRegion shared_memory)
    : storage_type_(StorageType::kSharedMemory),
      shared_memory_(std::move(shared_memory)) {}

BigBuffer::BigBuffer(size_t size) {
  TryCreateSharedMemory(size, &storage_type_, &shared_memory_);
  if (storage_type_ == BigBuffer::StorageType::kBytes) {
    // Either |size| is small enough or shared memory allocation failed, and
    // fallback to inline allocation is feasible.
    bytes_ = base::HeapArray<uint8_t>::Uninit(size);
  }
}

BigBuffer::~BigBuffer() = default;

BigBuffer& BigBuffer::operator=(BigBuffer&& other) {
  storage_type_ = other.storage_type_;
  bytes_ = std::move(other.bytes_);
  shared_memory_ = std::move(other.shared_memory_);
  // Make sure |other| looks empty.
  other.storage_type_ = StorageType::kInvalidBuffer;
  return *this;
}

BigBuffer BigBuffer::Clone() const {
  return BigBuffer(base::span(*this));
}

uint8_t* BigBuffer::data() {
  return const_cast<uint8_t*>(const_cast<const BigBuffer*>(this)->data());
}

const uint8_t* BigBuffer::data() const {
  switch (storage_type_) {
    case StorageType::kBytes:
      return bytes_.data();
    case StorageType::kSharedMemory:
      DCHECK(shared_memory_->buffer_mapping_);
      return static_cast<const uint8_t*>(
          const_cast<const void*>(shared_memory_->buffer_mapping_.get()));
    case StorageType::kInvalidBuffer:
      // We return null here but do not assert unlike the default case. No
      // consumer is allowed to dereference this when |size()| is zero anyway.
      return nullptr;
    default:
      NOTREACHED();
  }
}

size_t BigBuffer::size() const {
  switch (storage_type_) {
    case StorageType::kBytes:
      return bytes_.size();
    case StorageType::kSharedMemory:
      return shared_memory_->size();
    case StorageType::kInvalidBuffer:
      return 0;
    default:
      NOTREACHED();
  }
}

BigBufferView::BigBufferView() = default;

BigBufferView::BigBufferView(BigBufferView&& other) = default;

BigBufferView::BigBufferView(base::span<const uint8_t> bytes) {
  TryCreateSharedMemory(bytes.size(), &storage_type_, &shared_memory_);
  if (storage_type_ == BigBuffer::StorageType::kSharedMemory) {
    DCHECK(shared_memory_->memory());
    base::ranges::copy(bytes, static_cast<uint8_t*>(shared_memory_->memory()));
    return;
  }
  if (storage_type_ == BigBuffer::StorageType::kBytes) {
    // Either the data is small enough or shared memory allocation failed.
    // Either way we fall back to directly referencing the input bytes.
    bytes_ = bytes;
  }
}

BigBufferView::~BigBufferView() = default;

BigBufferView& BigBufferView::operator=(BigBufferView&& other) = default;

void BigBufferView::SetBytes(base::span<const uint8_t> bytes) {
  DCHECK(bytes_.empty());
  DCHECK(!shared_memory_);
  storage_type_ = BigBuffer::StorageType::kBytes;
  bytes_ = bytes;
}

void BigBufferView::SetSharedMemory(
    internal::BigBufferSharedMemoryRegion shared_memory) {
  DCHECK(bytes_.empty());
  DCHECK(!shared_memory_);
  storage_type_ = BigBuffer::StorageType::kSharedMemory;
  shared_memory_ = std::move(shared_memory);
}

base::span<const uint8_t> BigBufferView::data() const {
  if (storage_type_ == BigBuffer::StorageType::kBytes) {
    return bytes_;
  } else if (storage_type_ == BigBuffer::StorageType::kSharedMemory) {
    DCHECK(shared_memory_.has_value());
    return base::make_span(static_cast<const uint8_t*>(const_cast<const void*>(
                               shared_memory_->memory())),
                           shared_memory_->size());
  }

  return base::span<const uint8_t>();
}

// static
BigBuffer BigBufferView::ToBigBuffer(BigBufferView view) {
  BigBuffer buffer;
  buffer.storage_type_ = view.storage_type_;
  if (view.storage_type_ == BigBuffer::StorageType::kBytes) {
    buffer.bytes_ = base::HeapArray<uint8_t>::CopiedFrom(view.bytes_);
  } else if (view.storage_type_ == BigBuffer::StorageType::kSharedMemory) {
    buffer.shared_memory_ = std::move(*view.shared_memory_);
  }
  return buffer;
}

// static
BigBufferView BigBufferView::CreateInvalidForTest() {
  BigBufferView view;
  view.storage_type_ = BigBuffer::StorageType::kInvalidBuffer;
  return view;
}

}  // namespace mojo_base
