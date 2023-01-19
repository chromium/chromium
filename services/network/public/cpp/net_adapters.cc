// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_adapters.h"

#include "net/base/net_errors.h"

namespace network {

namespace {
const uint32_t kMaxBufSize = 64 * 1024;
}

NetToMojoPendingBuffer::NetToMojoPendingBuffer(
    mojo::ScopedDataPipeProducerHandle handle,
    void* buffer)
    : handle_(std::move(handle)), buffer_(buffer) {}

NetToMojoPendingBuffer::~NetToMojoPendingBuffer() {
  if (handle_.is_valid())
    handle_->EndWriteData(0);
}

MojoResult NetToMojoPendingBuffer::BeginWrite(
    mojo::ScopedDataPipeProducerHandle* handle,
    scoped_refptr<NetToMojoPendingBuffer>* pending,
    uint32_t* num_bytes) {
  void* buf = nullptr;
  *num_bytes = kMaxBufSize;
  MojoResult result =
      (*handle)->BeginWriteData(&buf, num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_OK) {
    if (*num_bytes > kMaxBufSize) {
      *num_bytes = kMaxBufSize;
    }

    *pending = new NetToMojoPendingBuffer(std::move(*handle), buf);
  }
  return result;
}

mojo::ScopedDataPipeProducerHandle NetToMojoPendingBuffer::Complete(
    uint32_t num_bytes) {
  handle_->EndWriteData(num_bytes);
  buffer_ = nullptr;
  return std::move(handle_);
}

NetToMojoIOBuffer::NetToMojoIOBuffer(NetToMojoPendingBuffer* pending_buffer,
                                     int offset)
    : net::WrappedIOBuffer(pending_buffer->buffer() + offset),
      pending_buffer_(pending_buffer) {}

NetToMojoIOBuffer::~NetToMojoIOBuffer() {}

MojoToNetPendingBuffer::MojoToNetPendingBuffer(
    mojo::ScopedDataPipeConsumerHandle handle,
    const void* buffer)
    : handle_(std::move(handle)), buffer_(buffer) {}

MojoToNetPendingBuffer::~MojoToNetPendingBuffer() {}

// static
MojoResult MojoToNetPendingBuffer::BeginRead(
    mojo::ScopedDataPipeConsumerHandle* handle,
    scoped_refptr<MojoToNetPendingBuffer>* pending,
    uint32_t* num_bytes) {
  const void* buffer = nullptr;
  *num_bytes = 0;
  MojoResult result =
      (*handle)->BeginReadData(&buffer, num_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_OK)
    *pending = new MojoToNetPendingBuffer(std::move(*handle), buffer);
  return result;
}

void MojoToNetPendingBuffer::CompleteRead(uint32_t num_bytes) {
  handle_->EndReadData(num_bytes);
  buffer_ = nullptr;
}

mojo::ScopedDataPipeConsumerHandle MojoToNetPendingBuffer::ReleaseHandle() {
  DCHECK(IsComplete());
  return std::move(handle_);
}

bool MojoToNetPendingBuffer::IsComplete() const {
  return buffer_ == nullptr;
}

MojoToNetIOBuffer::MojoToNetIOBuffer(MojoToNetPendingBuffer* pending_buffer,
                                     int bytes_to_be_read)
    : net::WrappedIOBuffer(pending_buffer->buffer()),
      pending_buffer_(pending_buffer),
      bytes_to_be_read_(bytes_to_be_read) {}

MojoToNetIOBuffer::~MojoToNetIOBuffer() {
  // We can safely notify mojo, that the data has been consumed and can be
  // released at this point.
  pending_buffer_->CompleteRead(bytes_to_be_read_);
}

}  // namespace network
