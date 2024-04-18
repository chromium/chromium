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

DataBuffer::DataBuffer(const uint8_t* data, int data_size)
    : data_size_(data_size) {
  if (!data) {
    CHECK_EQ(data_size, 0);
    return;
  }

  CHECK_GE(data_size, 0);
  data_ = base::HeapArray<uint8_t>::Uninit(data_size);
  memcpy(data_.data(), data, data_size_);
}
DataBuffer::DataBuffer(DataBufferType data_buffer_type)
    : is_end_of_stream_(data_buffer_type == DataBufferType::kEndOfStream) {}

DataBuffer::~DataBuffer() = default;

// static
scoped_refptr<DataBuffer> DataBuffer::CopyFrom(const uint8_t* data, int size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return base::WrapRefCounted(new DataBuffer(data, size));
}

// static
scoped_refptr<DataBuffer> DataBuffer::CreateEOSBuffer() {
  return base::WrapRefCounted(new DataBuffer(DataBufferType::kEndOfStream));
}
}  // namespace media
