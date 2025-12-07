// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/fake_libva_driver/fake_buffer.h"

#include <string.h>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"

namespace {

size_t CalculateDataSize(unsigned int size_per_element,
                         unsigned int num_elements) {
  base::CheckedNumeric<size_t> data_size(
      base::strict_cast<size_t>(size_per_element));
  data_size *= base::strict_cast<size_t>(num_elements);
  return data_size.ValueOrDie();
}

base::HeapArray<uint8_t> InitializeData(const void* data, size_t data_size) {
  // SAFETY: The `data` pointer originates from the `vaCreateBuffer` function,
  // which is part of the VA-API C interface and is invoked via the
  // `VADriverVTable`. This C-style API provides a raw pointer and a size, but
  // there is no way to guarantee that the provided `data_size` is a valid
  // buffer size for the given `data` pointer.
  // See: https://intel.github.io/libva/group__api__core.html
  if (data == nullptr) {
    return base::HeapArray<uint8_t>::Uninit(data_size);
  }

  return UNSAFE_BUFFERS(base::HeapArray<uint8_t>::CopiedFrom(
      base::span(static_cast<const uint8_t*>(data), data_size)));
}
}  // namespace

namespace media::internal {

FakeBuffer::FakeBuffer(IdType id,
                       VAContextID context,
                       VABufferType type,
                       unsigned int size_per_element,
                       unsigned int num_elements,
                       const void* data)
    : id_(id),
      context_(context),
      type_(type),
      data_(InitializeData(data,
                           CalculateDataSize(size_per_element, num_elements))) {
}

FakeBuffer::~FakeBuffer() = default;

FakeBuffer::IdType FakeBuffer::GetID() const {
  return id_;
}

VAContextID FakeBuffer::GetContextID() const {
  return context_;
}

VABufferType FakeBuffer::GetType() const {
  return type_;
}

base::span<uint8_t> FakeBuffer::GetData() const {
  // SAFETY: Cast away constness from `data_.data()` because
  // base::HeapArray::data() returns const uint8_t*, but we need to provide
  // mutable access to the data for the caller.
  return UNSAFE_BUFFERS(
      base::span<uint8_t>(const_cast<uint8_t*>(data_.data()), data_.size()));
}

}  // namespace media::internal
