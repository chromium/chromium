// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/data_buffer.h"

#include <utility>

namespace media {

DataBuffer::DataBuffer(int buffer_size) : data_size_(buffer_size) {
  CHECK_GE(buffer_size, 0);
  data_ = base::HeapArray<uint8_t>::Uninit(buffer_size);
}

DataBuffer::DataBuffer(base::HeapArray<uint8_t> buffer)
    : data_size_(buffer.size()) {
  data_ = std::move(buffer);
  CHECK(data_.data());
}

DataBuffer::DataBuffer(base::span<const uint8_t> data)
    : data_size_(data.size()) {
  CHECK(!data.empty());
  data_ = base::HeapArray<uint8_t>::CopiedFrom(data);
}
DataBuffer::DataBuffer(DataBufferType data_buffer_type)
    : is_end_of_stream_(data_buffer_type == DataBufferType::kEndOfStream) {}

DataBuffer::~DataBuffer() = default;

// static
scoped_refptr<DataBuffer> DataBuffer::CopyFrom(base::span<const uint8_t> data) {
  return base::WrapRefCounted(new DataBuffer(data));
}

// static
scoped_refptr<DataBuffer> DataBuffer::CreateEOSBuffer() {
  return base::WrapRefCounted(new DataBuffer(DataBufferType::kEndOfStream));
}
}  // namespace media
