// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/read_buffering_stream_socket.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"
#include "net/base/io_buffer.h"

namespace net {

ReadBufferingStreamSocket::ReadBufferingStreamSocket(
    std::unique_ptr<StreamSocket> transport)
    : WrappedStreamSocket(std::move(transport)) {}

ReadBufferingStreamSocket::~ReadBufferingStreamSocket() = default;

void ReadBufferingStreamSocket::BufferNextRead(int size) {
  DCHECK(!user_read_buf_);
  read_buffer_ = base::MakeRefCounted<GrowableIOBuffer>();
  read_buffer_->SetCapacity(size);
  buffer_full_ = false;
}

int ReadBufferingStreamSocket::Read(IOBuffer* buf,
                                    int buf_len,
                                    CompletionOnceCallback callback) {
  DCHECK(!user_read_buf_);
  if (!read_buffer_)
    return transport_->Read(buf, buf_len, std::move(callback));
  int rv = ReadIfReady(buf, buf_len, std::move(callback));
  if (rv == ERR_IO_PENDING) {
    user_read_buf_ = buf;
    user_read_buf_len_ = buf_len;
  }
  return rv;
}

int ReadBufferingStreamSocket::ReadIfReady(IOBuffer* buf,
                                           int buf_len,
                                           CompletionOnceCallback callback) {
  DCHECK(!user_read_buf_);
  if (!read_buffer_)
    return transport_->ReadIfReady(buf, buf_len, std::move(callback));

  if (buffer_full_)
    return CopyToCaller(buf, buf_len);

  state_ = STATE_READ;
  int rv = DoLoop(OK);
  if (rv == OK) {
    rv = CopyToCaller(buf, buf_len);
  } else if (rv == ERR_IO_PENDING) {
    user_read_callback_ = std::move(callback);
  }
  return rv;
}

int ReadBufferingStreamSocket::DoLoop(int result) {
  int rv = result;
  do {
    State current_state = state_;
    state_ = STATE_NONE;
    switch (current_state) {
      case STATE_READ:
        rv = DoRead();
        break;
      case STATE_READ_COMPLETE:
        rv = DoReadComplete(rv);
        break;
      case STATE_NONE:
      default:
        NOTREACHED_IN_MIGRATION() << "Unexpected state: " << current_state;
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && state_ != STATE_NONE);
  return rv;
}

int ReadBufferingStreamSocket::DoRead() {
  DCHECK(read_buffer_);
  DCHECK(!buffer_full_);

  state_ = STATE_READ_COMPLETE;
  return transport_->Read(
      read_buffer_.get(), read_buffer_->RemainingCapacity(),
      base::BindOnce(&ReadBufferingStreamSocket::OnReadCompleted,
                     base::Unretained(this)));
}

int ReadBufferingStreamSocket::DoReadComplete(int result) {
  state_ = STATE_NONE;

  if (result <= 0)
    return result;

  read_buffer_->set_offset(read_buffer_->offset() + result);
  if (read_buffer_->RemainingCapacity() > 0) {
    // Keep reading until |read_buffer_| is full.
    state_ = STATE_READ;
  } else {
    read_buffer_->set_offset(0);
    buffer_full_ = true;
  }
  return OK;
}

void ReadBufferingStreamSocket::OnReadCompleted(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(user_read_callback_);

  result = DoLoop(result);
  if (result == ERR_IO_PENDING)
    return;
  if (result == OK && user_read_buf_) {
    // If the user called Read(), return the data to the caller.
    result = CopyToCaller(user_read_buf_.get(), user_read_buf_len_);
    user_read_buf_ = nullptr;
    user_read_buf_len_ = 0;
  }
  std::move(user_read_callback_).Run(result);
}

int ReadBufferingStreamSocket::CopyToCaller(IOBuffer* buf, int buf_len) {
  DCHECK(read_buffer_);
  DCHECK(buffer_full_);

  buf_len = std::min(buf_len, read_buffer_->RemainingCapacity());
  memcpy(buf->data(), read_buffer_->data(), buf_len);
  read_buffer_->set_offset(read_buffer_->offset() + buf_len);
  if (read_buffer_->RemainingCapacity() == 0) {
    read_buffer_ = nullptr;
    buffer_full_ = false;
  }
  return buf_len;
}

}  // namespace net
