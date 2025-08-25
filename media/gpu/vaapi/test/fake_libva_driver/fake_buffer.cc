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
      data_(base::HeapArray<uint8_t>::Uninit(
          CalculateDataSize(size_per_element, num_elements))) {
  if (data) {
    UNSAFE_TODO(memcpy(const_cast<uint8_t*>(data_.data()), data, data_.size()));
  }
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

size_t FakeBuffer::GetDataSize() const {
  return data_.size();
}

void* FakeBuffer::GetData() const {
  return static_cast<void*>(const_cast<uint8_t*>(data_.data()));
}

}  // namespace media::internal
