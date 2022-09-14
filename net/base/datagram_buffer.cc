// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/datagram_buffer.h"

#include "base/memory/ptr_util.h"

#include <cstring>

namespace net {

DatagramBufferPool::DatagramBufferPool(size_t max_buffer_size)
    : max_buffer_size_(max_buffer_size) {}

DatagramBufferPool::~DatagramBufferPool() = default;

void DatagramBufferPool::Enqueue(const char* buffer,
                                 size_t buf_len,
                                 DatagramBuffers* buffers) {
  DCHECK_LE(buf_len, max_buffer_size_);
  std::unique_ptr<DatagramBuffer> datagram_buffer;
  if (free_list_.empty()) {
    datagram_buffer = base::WrapUnique(new DatagramBuffer(max_buffer_size_));
  } else {
    datagram_buffer = std::move(free_list_.front());
    free_list_.pop_front();
  }
  datagram_buffer->Set(buffer, buf_len);
  buffers->emplace_back(std::move(datagram_buffer));
}

void DatagramBufferPool::Dequeue(DatagramBuffers* buffers) {
  if (buffers->size() == 0)
    return;

  free_list_.splice(free_list_.cend(), *buffers);
}

DatagramBuffer::DatagramBuffer(size_t max_buffer_size)
    : data_(std::make_unique<char[]>(max_buffer_size)) {}

DatagramBuffer::~DatagramBuffer() = default;

void DatagramBuffer::Set(const char* buffer, size_t buf_len) {
  length_ = buf_len;
  std::memcpy(data_.get(), buffer, buf_len);
}

char* DatagramBuffer::data() const {
  return data_.get();
}

size_t DatagramBuffer::length() const {
  return length_;
}

}  // namespace net
