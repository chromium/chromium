// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_fragment.h"

#include "base/compiler_specific.h"

namespace mojo::internal {

void EncodeFragmentPointer(Message& message, size_t index, uint64_t* offset) {
  EncodePointer(index == kInvalidFragmentIndex
                    ? nullptr
                    : message.payload_buffer()->Get<void>(index),
                offset);
}

void* MessageFragmentBase::AllocateFragment(size_t num_bytes) {
  Buffer* buffer = message_.payload_buffer();
  index_ = buffer->Allocate(num_bytes);
  return buffer->Get<void>(index_);
}

NOINLINE void MessageFragmentBase::AllocateUnversionedStructFragment(
    uint32_t num_bytes) {
  void* ptr = AllocateFragment(num_bytes);
  static_cast<StructHeader*>(ptr)->num_bytes = num_bytes;
}

void MessageFragmentBase::AllocateVersionedStructFragment(uint32_t num_bytes,
                                                          uint32_t version) {
  void* ptr = AllocateFragment(num_bytes);
  auto* header = static_cast<StructHeader*>(ptr);
  header->num_bytes = num_bytes;
  header->version = version;
}

void BitPackedArrayMessageFragment::AllocateArrayFragment(
    uint32_t num_bytes,
    uint32_t num_elements) {
  void* ptr = AllocateFragment(num_bytes);
  auto* header = static_cast<ArrayHeader*>(ptr);
  header->num_bytes = num_bytes;
  header->num_elements = num_elements;
}

NOINLINE void* FixedSizeArrayMessageFragment::AllocateArrayFragment(
    size_t num_elements,
    uint32_t element_size) {
  constexpr uint64_t kMaxDataBytes =
      std::numeric_limits<uint32_t>::max() - sizeof(ArrayHeader);
  CHECK(num_elements <= kMaxDataBytes &&
        uint64_t{static_cast<uint32_t>(num_elements)} * element_size <=
            kMaxDataBytes);
  const uint32_t num_bytes =
      sizeof(ArrayHeader) + static_cast<uint32_t>(num_elements) * element_size;
  void* ptr = AllocateFragment(num_bytes);
  auto* header = static_cast<ArrayHeader*>(ptr);
  header->num_bytes = num_bytes;
  header->num_elements = static_cast<uint32_t>(num_elements);
  return ptr;
}

}  // namespace mojo::internal
