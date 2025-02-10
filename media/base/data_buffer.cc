// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/data_buffer.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"

namespace media {

DataBuffer::DataBuffer(size_t capacity) {
  CHECK_GE(capacity, 0u);
  data_ = base::HeapArray<uint8_t>::Uninit(capacity);
}
DataBuffer::DataBuffer(base::HeapArray<uint8_t> buffer) : size_(buffer.size()) {
  data_ = std::move(buffer);
  CHECK(data_.data());
}

DataBuffer::DataBuffer(base::PassKey<DataBuffer>,
                       base::span<const uint8_t> data)
    : size_(data.size()) {
  CHECK(!data.empty());
  data_ = base::HeapArray<uint8_t>::CopiedFrom(data);
}
DataBuffer::DataBuffer(base::PassKey<DataBuffer>,
                       DataBufferType data_buffer_type)
    : is_end_of_stream_(data_buffer_type == DataBufferType::kEndOfStream) {}

DataBuffer::~DataBuffer() = default;

// static
scoped_refptr<DataBuffer> DataBuffer::CopyFrom(base::span<const uint8_t> data) {
  return base::MakeRefCounted<DataBuffer>(base::PassKey<DataBuffer>(), data);
}

// static
scoped_refptr<DataBuffer> DataBuffer::CreateEOSBuffer() {
  return base::MakeRefCounted<DataBuffer>(base::PassKey<DataBuffer>(),
                                          DataBufferType::kEndOfStream);
}

void DataBuffer::Append(base::span<const uint8_t> data) {
  CHECK(!end_of_stream());
  data_.subspan(size_, data.size()).copy_from(data);
  size_ += data.size();
}

}  // namespace media
