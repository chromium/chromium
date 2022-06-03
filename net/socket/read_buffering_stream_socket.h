// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_READ_BUFFERING_STREAM_SOCKET_H_
#define NET_SOCKET_READ_BUFFERING_STREAM_SOCKET_H_

#include <memory>

#include "net/base/completion_once_callback.h"
#include "net/socket/socket_test_util.h"

namespace net {

class GrowableIOBuffer;

// Wraps an existing StreamSocket that will ensure a certain amount of data is
// internally buffered before satisfying a Read() request, regardless of how
// quickly the OS receives them from the peer.
class ReadBufferingStreamSocket : public WrappedStreamSocket {
 public:
  explicit ReadBufferingStreamSocket(std::unique_ptr<StreamSocket> transport);
  ~ReadBufferingStreamSocket() override;

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;

  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override;

  // Causes the next Read() or ReadIfReady() call to not return data until it
  // has internally been buffered up to |size| bytes. Once the buffer has been
  // consumed, the buffering is disabled. If the next read requests fewer than
  // |size| bytes, it will not return until 0
  void BufferNextRead(int size);

 private:
  enum State {
    STATE_NONE,
    STATE_READ,
    STATE_READ_COMPLETE,
  };

  int DoLoop(int result);
  int DoRead();
  int DoReadComplete(int result);
  void OnReadCompleted(int result);
  int CopyToCaller(IOBuffer* buf, int buf_len);

  State state_ = STATE_NONE;

  // The buffer that must be filled to capacity before data is released out of
  // Read() or ReadIfReady(). If buffering is disabled, this is zero.
  scoped_refptr<GrowableIOBuffer> read_buffer_;
  // True if |read_buffer_| has been filled, in which case
  // |read_buffer_->offset()| is how much data has been released to the caller.
  // If false, the offset is how much data has been written.
  bool buffer_full_ = false;

  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;
  CompletionOnceCallback user_read_callback_;
};

}  // namespace net

#endif  // NET_SOCKET_READ_BUFFERING_STREAM_SOCKET_H_
