// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/entry_write_buffer.h"

#include <utility>

#include "net/base/io_buffer.h"

namespace disk_cache {

EntryWriteBuffer::EntryWriteBuffer() = default;

EntryWriteBuffer::EntryWriteBuffer(scoped_refptr<net::IOBuffer> buffer,
                                   int size,
                                   int64_t offset)
    : size(size), offset(offset) {
  if (buffer) {
    buffers.push_back(std::move(buffer));
  }
}

EntryWriteBuffer::~EntryWriteBuffer() = default;

EntryWriteBuffer::EntryWriteBuffer(EntryWriteBuffer&& other)
    : buffers(std::move(other.buffers)),
      size(std::exchange(other.size, 0)),
      offset(std::exchange(other.offset, 0)) {}

EntryWriteBuffer& EntryWriteBuffer::operator=(EntryWriteBuffer&& other) {
  if (this != &other) {
    buffers = std::move(other.buffers);
    size = std::exchange(other.size, 0);
    offset = std::exchange(other.offset, 0);
  }
  return *this;
}

}  // namespace disk_cache
