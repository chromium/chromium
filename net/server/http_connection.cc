// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/http_connection.h"

#include <ranges>
#include <utility>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "net/server/web_socket.h"
#include "net/socket/stream_socket.h"

namespace net {

HttpConnection::ReadIOBuffer::ReadIOBuffer()
    : base_(base::MakeRefCounted<GrowableIOBuffer>()) {
  SetCapacity(kInitialBufSize);
}

HttpConnection::ReadIOBuffer::~ReadIOBuffer() {
  // Avoid dangling ptr when `base_` is destroyed.
  ClearSpan();
}

int HttpConnection::ReadIOBuffer::GetCapacity() const {
  return base_->capacity();
}

void HttpConnection::ReadIOBuffer::SetCapacity(int capacity) {
  CHECK_LE(base_->offset(), capacity);
  // Clear current span to avoid warning about dangling pointer, as
  // SetCapacity() may destroy the old buffer.
  ClearSpan();
  base_->SetCapacity(capacity);
  SetSpan(base_->span());
}

bool HttpConnection::ReadIOBuffer::IncreaseCapacity() {
  if (GetCapacity() >= max_buffer_size_) {
    LOG(ERROR) << "Too large read data is pending: capacity=" << GetCapacity()
               << ", max_buffer_size=" << max_buffer_size_
               << ", read=" << base_->offset();
    return false;
  }

  int new_capacity = GetCapacity() * kCapacityIncreaseFactor;
  if (new_capacity > max_buffer_size_)
    new_capacity = max_buffer_size_;
  SetCapacity(new_capacity);
  return true;
}

base::span<const uint8_t> HttpConnection::ReadIOBuffer::readable_bytes() const {
  return base_->span_before_offset();
}

void HttpConnection::ReadIOBuffer::DidRead(int bytes) {
  DCHECK_GE(RemainingCapacity(), bytes);
  base_->set_offset(base_->offset() + bytes);
  SetSpan(base_->span());
}

int HttpConnection::ReadIOBuffer::RemainingCapacity() const {
  return base_->RemainingCapacity();
}

void HttpConnection::ReadIOBuffer::DidConsume(int bytes) {
  int previous_size = base_->offset();
  int unconsumed_size = previous_size - bytes;
  DCHECK_LE(0, unconsumed_size);
  if (unconsumed_size > 0) {
    // Move unconsumed data to the start of buffer. readable_bytes() returns a
    // read-only buffer, so need to call the non-constant overload in the base
    // class instead, to get a writeable span.
    base::span<uint8_t> buffer = base_->span_before_offset();
    std::ranges::copy(buffer.subspan(base::checked_cast<size_t>(bytes)),
                      buffer.begin());
  }
  base_->set_offset(unconsumed_size);
  SetSpan(base_->span());

  // If capacity is too big, reduce it.
  if (GetCapacity() > kMinimumBufSize &&
      GetCapacity() > previous_size * kCapacityIncreaseFactor) {
    int new_capacity = GetCapacity() / kCapacityIncreaseFactor;
    if (new_capacity < kMinimumBufSize)
      new_capacity = kMinimumBufSize;
    // this avoids the pointer to dangle until `SetCapacity` gets called.
    ClearSpan();
    // realloc() within GrowableIOBuffer::SetCapacity() could move data even
    // when size is reduced. If unconsumed_size == 0, i.e. no data exists in
    // the buffer, free internal buffer first to guarantee no data move.
    if (!unconsumed_size)
      base_->SetCapacity(0);
    SetCapacity(new_capacity);
  }
}

HttpConnection::QueuedWriteIOBuffer::QueuedWriteIOBuffer() = default;

HttpConnection::QueuedWriteIOBuffer::~QueuedWriteIOBuffer() {
  // `pending_data_` owns the underlying data.
  ClearSpan();
}

bool HttpConnection::QueuedWriteIOBuffer::IsEmpty() const {
  return pending_data_.empty();
}

bool HttpConnection::QueuedWriteIOBuffer::Append(const std::string& data) {
  if (data.empty())
    return true;

  if (total_size_ + static_cast<int>(data.size()) > max_buffer_size_) {
    LOG(ERROR) << "Too large write data is pending: size="
               << total_size_ + data.size()
               << ", max_buffer_size=" << max_buffer_size_;
    return false;
  }

  pending_data_.push(std::make_unique<std::string>(data));
  total_size_ += data.size();

  // If new data is the first pending data, updates data_.
  if (pending_data_.size() == 1) {
    SetSpan(base::as_writable_bytes(base::span(*pending_data_.front())));
  }
  return true;
}

void HttpConnection::QueuedWriteIOBuffer::DidConsume(int size) {
  DCHECK_GE(total_size_, size);
  DCHECK_GE(GetSizeToWrite(), size);
  if (size == 0)
    return;

  if (size < GetSizeToWrite()) {
    SetSpan(span().subspan(base::checked_cast<size_t>(size)));
  } else {
    // size == GetSizeToWrite(). Updates data_ to next pending data.
    ClearSpan();
    pending_data_.pop();
    if (!IsEmpty()) {
      SetSpan(base::as_writable_bytes(base::span(*pending_data_.front())));
    }
  }
  total_size_ -= size;
}

int HttpConnection::QueuedWriteIOBuffer::GetSizeToWrite() const {
  if (IsEmpty()) {
    DCHECK_EQ(0, total_size_);
    return 0;
  }
  // Return the unconsumed size of the current pending write.
  return size();
}

HttpConnection::HttpConnection(int id, std::unique_ptr<StreamSocket> socket)
    : id_(id),
      socket_(std::move(socket)),
      read_buf_(base::MakeRefCounted<ReadIOBuffer>()),
      write_buf_(base::MakeRefCounted<QueuedWriteIOBuffer>()) {}

HttpConnection::~HttpConnection() = default;

void HttpConnection::SetWebSocket(std::unique_ptr<WebSocket> web_socket) {
  DCHECK(!web_socket_);
  web_socket_ = std::move(web_socket);
}

}  // namespace net
