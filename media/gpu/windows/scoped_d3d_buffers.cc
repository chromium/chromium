// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/windows/scoped_d3d_buffers.h"

#include <algorithm>

namespace media {

ScopedD3DBuffer::ScopedD3DBuffer(base::span<uint8_t> data) : data_(data) {}
ScopedD3DBuffer::~ScopedD3DBuffer() = default;

D3DInputBuffer::D3DInputBuffer(std::unique_ptr<ScopedD3DBuffer> buffer)
    : buffer_(std::move(buffer)) {}

D3DInputBuffer::~D3DInputBuffer() = default;

bool D3DInputBuffer::Commit() {
  return buffer_->Commit();
}

ScopedSequenceD3DInputBuffer::~ScopedSequenceD3DInputBuffer() {
  buffer_->Commit(BytesWritten());
}

size_t ScopedSequenceD3DInputBuffer::BytesAvailable() const {
  DCHECK_LE(offset_, size());
  return size() - offset_;
}

size_t ScopedSequenceD3DInputBuffer::Write(base::span<const uint8_t> source) {
  size_t bytes_to_write = std::min(source.size(), BytesAvailable());
  memcpy(buffer_->data() + offset_, source.data(), bytes_to_write);
  offset_ += bytes_to_write;
  return bytes_to_write;
}

bool ScopedSequenceD3DInputBuffer::Commit() {
  return buffer_->Commit(BytesWritten());
}

}  // namespace media
