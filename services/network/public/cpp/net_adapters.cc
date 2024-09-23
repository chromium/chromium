// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_adapters.h"

#include <limits>

#include "base/check_op.h"
#include "base/containers/span.h"
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
  base::span<uint8_t> buf;
  const size_t kMaxBufSize = features::GetNetAdapterMaxBufSize();
  MojoResult result =
      (*handle)->BeginWriteData(kMaxBufSize, MOJO_WRITE_DATA_FLAG_NONE, buf);
  if (result != MOJO_RESULT_OK) {
    *pending = nullptr;
    return result;
  }
  if (buf.size() > kMaxBufSize) {
    buf = buf.first(kMaxBufSize);
  }
  *pending = new NetToMojoPendingBuffer(std::move(*handle),
                                        base::as_writable_chars(buf));
  return MOJO_RESULT_OK;
}

mojo::ScopedDataPipeProducerHandle NetToMojoPendingBuffer::Complete(
    uint32_t num_bytes) {
  handle_->EndWriteData(num_bytes);
  buffer_ = base::span<char>();
  return std::move(handle_);
}

NetToMojoIOBuffer::NetToMojoIOBuffer(
    scoped_refptr<NetToMojoPendingBuffer> pending_buffer,
    int offset)
    : net::WrappedIOBuffer(base::make_span(*pending_buffer).subspan(offset)),
      pending_buffer_(std::move(pending_buffer)) {}

NetToMojoIOBuffer::~NetToMojoIOBuffer() {
  // Avoid dangling ptr should this destructor remove the last reference
  // to `pending_buffer_`.
  data_ = nullptr;
}

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
  base::span<const uint8_t> buffer;
  MojoResult result =
      (*handle)->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
  if (result != MOJO_RESULT_OK) {
    *pending = nullptr;
    return result;
  }
  *pending = new MojoToNetPendingBuffer(std::move(*handle),
                                        base::as_string_view(buffer));
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

MojoToNetIOBuffer::MojoToNetIOBuffer(
    scoped_refptr<MojoToNetPendingBuffer> pending_buffer,
    int bytes_to_be_read)
    : net::WrappedIOBuffer(*pending_buffer),
      pending_buffer_(std::move(pending_buffer)),
      bytes_to_be_read_(bytes_to_be_read) {}

MojoToNetIOBuffer::~MojoToNetIOBuffer() {
  // We can safely notify mojo, that the data has been consumed and can be
  // released at this point.
  pending_buffer_->CompleteRead(bytes_to_be_read_);

  // Prevent dangling ptr should this destructor remove the last reference
  // to `pending_buffer_`.
  data_ = nullptr;
}

}  // namespace network
