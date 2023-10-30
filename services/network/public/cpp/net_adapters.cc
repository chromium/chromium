// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_adapters.h"

#include <limits>

#include "base/check_op.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"

namespace network {

NetToMojoPendingBuffer::NetToMojoPendingBuffer(
    mojo::ScopedDataPipeProducerHandle handle,
    base::span<char> buffer)
    : handle_(std::move(handle)), buffer_(buffer) {
  CHECK_LE(buffer_.size(), std::numeric_limits<uint32_t>::max());
}

NetToMojoPendingBuffer::~NetToMojoPendingBuffer() {
  if (handle_.is_valid())
    handle_->EndWriteData(0);
}

MojoResult NetToMojoPendingBuffer::BeginWrite(
    mojo::ScopedDataPipeProducerHandle* handle,
    scoped_refptr<NetToMojoPendingBuffer>* pending) {
  void* buf = nullptr;
  const uint32_t kMaxBufSize = features::GetNetAdapterMaxBufSize();
  uint32_t num_bytes = kMaxBufSize;
  MojoResult result =
      (*handle)->BeginWriteData(&buf, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK) {
    *pending = nullptr;
    return result;
  }
  if (num_bytes > kMaxBufSize) {
    num_bytes = kMaxBufSize;
  }
  *pending = new NetToMojoPendingBuffer(
      std::move(*handle),
      {static_cast<char*>(buf), static_cast<size_t>(num_bytes)});
  return MOJO_RESULT_OK;
}

mojo::ScopedDataPipeProducerHandle NetToMojoPendingBuffer::Complete(
    uint32_t num_bytes) {
  handle_->EndWriteData(num_bytes);
  buffer_ = base::span<char>();
  return std::move(handle_);
}

NetToMojoIOBuffer::NetToMojoIOBuffer(NetToMojoPendingBuffer* pending_buffer,
                                     int offset)
    : net::WrappedIOBuffer(pending_buffer->buffer() + offset,
                           pending_buffer->size() - offset),
      pending_buffer_(pending_buffer) {}

NetToMojoIOBuffer::~NetToMojoIOBuffer() {}

MojoToNetPendingBuffer::MojoToNetPendingBuffer(
    mojo::ScopedDataPipeConsumerHandle handle,
    base::span<const char> buffer)
    : handle_(std::move(handle)), buffer_(buffer) {
  CHECK_LE(buffer_.size(), std::numeric_limits<uint32_t>::max());
}

MojoToNetPendingBuffer::~MojoToNetPendingBuffer() = default;

// static
MojoResult MojoToNetPendingBuffer::BeginRead(
    mojo::ScopedDataPipeConsumerHandle* handle,
    scoped_refptr<MojoToNetPendingBuffer>* pending) {
  const void* buffer = nullptr;
  uint32_t num_bytes = 0;
  MojoResult result =
      (*handle)->BeginReadData(&buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK) {
    *pending = nullptr;
    return result;
  }
  *pending = new MojoToNetPendingBuffer(
      std::move(*handle),
      {static_cast<const char*>(buffer), static_cast<size_t>(num_bytes)});
  return MOJO_RESULT_OK;
}

void MojoToNetPendingBuffer::CompleteRead(uint32_t num_bytes) {
  handle_->EndReadData(num_bytes);
  buffer_ = base::span<const char>();
}

mojo::ScopedDataPipeConsumerHandle MojoToNetPendingBuffer::ReleaseHandle() {
  DCHECK(IsComplete());
  return std::move(handle_);
}

bool MojoToNetPendingBuffer::IsComplete() const {
  return buffer_.empty();
}

MojoToNetIOBuffer::MojoToNetIOBuffer(MojoToNetPendingBuffer* pending_buffer,
                                     int bytes_to_be_read)
    : net::WrappedIOBuffer(pending_buffer->buffer(), pending_buffer->size()),
      pending_buffer_(pending_buffer),
      bytes_to_be_read_(bytes_to_be_read) {}

MojoToNetIOBuffer::~MojoToNetIOBuffer() {
  // We can safely notify mojo, that the data has been consumed and can be
  // released at this point.
  pending_buffer_->CompleteRead(bytes_to_be_read_);
}

}  // namespace network
