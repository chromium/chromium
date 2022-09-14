// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/data_buffer.h"

#include <utility>

namespace media {

DataBuffer::DataBuffer(int buffer_size)
    : buffer_size_(buffer_size),
      data_size_(0) {
  CHECK_GE(buffer_size, 0);
  data_ = std::make_unique<uint8_t[]>(buffer_size_);
}

DataBuffer::DataBuffer(std::unique_ptr<uint8_t[]> buffer, int buffer_size)
    : data_(std::move(buffer)),
      buffer_size_(buffer_size),
      data_size_(buffer_size) {
  CHECK(data_.get());
  CHECK_GE(buffer_size, 0);
}

DataBuffer::DataBuffer(const uint8_t* data, int data_size)
    : buffer_size_(data_size), data_size_(data_size) {
  if (!data) {
    CHECK_EQ(data_size, 0);
    return;
  }

  CHECK_GE(data_size, 0);
  data_ = std::make_unique<uint8_t[]>(buffer_size_);
  memcpy(data_.get(), data, data_size_);
}

DataBuffer::~DataBuffer() = default;

// static
scoped_refptr<DataBuffer> DataBuffer::CopyFrom(const uint8_t* data, int size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return base::WrapRefCounted(new DataBuffer(data, size));
}

// static
scoped_refptr<DataBuffer> DataBuffer::CreateEOSBuffer() {
  return base::WrapRefCounted(new DataBuffer(NULL, 0));
}
}  // namespace media
