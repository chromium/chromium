// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_adapters.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/strings/string_view_util.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"

namespace network {

namespace {
constexpr size_t kMaxBufSize = 64 * 1024;
}

NetToMojoPendingBuffer::StagingBuffer::StagingBuffer(size_t size)
    : buffer_(base::HeapArray<char>::WithSize(size)) {}

NetToMojoPendingBuffer::StagingBuffer::~StagingBuffer() = default;

NetToMojoPendingBuffer::StagingBufferPool::StagingBufferPool() = default;

NetToMojoPendingBuffer::StagingBufferPool::~StagingBufferPool() = default;

void NetToMojoPendingBuffer::StagingBufferPool::Reset() {
  buffer_.reset();
}

scoped_refptr<NetToMojoPendingBuffer::StagingBuffer>
NetToMojoPendingBuffer::StagingBufferPool::Take(size_t min_size) {
  if (!buffer_) {
    return nullptr;
  }
  CHECK(buffer_->HasOneRef());
  if (buffer_->span().size() >= min_size) {
    return std::move(buffer_);
  }
  buffer_.reset();
  return nullptr;
}

void NetToMojoPendingBuffer::StagingBufferPool::PutIfUniquelyOwned(
    scoped_refptr<StagingBuffer> buffer) {
  if (buffer && buffer->HasOneRef()) {
    buffer_ = std::move(buffer);
  }
}

NetToMojoPendingBuffer::NetToMojoPendingBuffer(
    mojo::ScopedDataPipeProducerHandle handle,
    base::span<uint8_t> mojo_buffer,
    scoped_refptr<StagingBuffer> staging_buffer,
    base::WeakPtr<StagingBufferPool> buffer_pool)
    : handle_(std::move(handle)),
      mojo_buffer_(mojo_buffer),
      staging_buffer_(std::move(staging_buffer)),
      buffer_pool_(std::move(buffer_pool)),
      buffer_(staging_buffer_
                  ? staging_buffer_->span().first(mojo_buffer_.size())
                  : base::as_writable_chars(mojo_buffer_)) {
  CHECK_LE(buffer_.size(), std::numeric_limits<uint32_t>::max());
}

NetToMojoPendingBuffer::~NetToMojoPendingBuffer() {
  buffer_ = base::span<char>();
  mojo_buffer_ = base::span<uint8_t>();
  if (handle_.is_valid()) {
    handle_->EndWriteData(0);
  }
}

// static
MojoResult NetToMojoPendingBuffer::BeginWrite(
    mojo::ScopedDataPipeProducerHandle* handle,
    scoped_refptr<NetToMojoPendingBuffer>* pending,
    StagingBufferPool* buffer_pool) {
  base::span<uint8_t> buf;
  MojoResult result = (*handle)->BeginWriteData(
      kMaxBufSize, MOJO_BEGIN_WRITE_DATA_FLAG_NONE, buf);
  if (result != MOJO_RESULT_OK) {
    *pending = nullptr;
    return result;
  }
  if (buf.size() > kMaxBufSize) {
    buf = buf.first(kMaxBufSize);
  }

  scoped_refptr<StagingBuffer> staging;
  base::WeakPtr<StagingBufferPool> pool_weak_ptr;
  if (base::FeatureList::IsEnabled(features::kStageNetToMojoBuffer)) {
    if (buffer_pool) {
      staging = buffer_pool->Take(buf.size());
      pool_weak_ptr = buffer_pool->GetWeakPtr();
    }
    if (!staging) {
      staging = base::MakeRefCounted<StagingBuffer>(buf.size());
    }
  }

  *pending = base::WrapRefCounted(new NetToMojoPendingBuffer(
      std::move(*handle), buf, std::move(staging), std::move(pool_weak_ptr)));
  return MOJO_RESULT_OK;
}

mojo::ScopedDataPipeProducerHandle NetToMojoPendingBuffer::Complete(
    uint32_t num_bytes) {
  CHECK(handle_.is_valid());
  CHECK_LE(size_t{num_bytes}, buffer_.size());
  if (staging_buffer_) {
    if (num_bytes > 0) {
      mojo_buffer_.first(num_bytes).copy_from(
          base::as_byte_span(buffer_.first(num_bytes)));
    }
    handle_->EndWriteData(num_bytes);
    buffer_ = base::span<char>();
    mojo_buffer_ = base::span<uint8_t>();
    if (buffer_pool_) {
      buffer_pool_->PutIfUniquelyOwned(std::move(staging_buffer_));
    } else {
      staging_buffer_.reset();
    }
    return std::move(handle_);
  }

  handle_->EndWriteData(num_bytes);
  buffer_ = base::span<char>();
  mojo_buffer_ = base::span<uint8_t>();
  return std::move(handle_);
}

NetToMojoIOBuffer::NetToMojoIOBuffer(
    scoped_refptr<NetToMojoPendingBuffer> pending_buffer,
    size_t offset)
    : net::WrappedIOBuffer(base::span(*pending_buffer).subspan(offset)),
      staging_buffer_(pending_buffer->staging_buffer()),
      pending_buffer_(staging_buffer_ ? nullptr : std::move(pending_buffer)) {}

NetToMojoIOBuffer::~NetToMojoIOBuffer() {
  // Avoid dangling ptr should this destructor remove the last reference
  // to `pending_buffer_` or `staging_buffer_`.
  ClearSpan();
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
  ClearSpan();
}

}  // namespace network
