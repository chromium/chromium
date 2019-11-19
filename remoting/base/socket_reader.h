// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SOCKET_READER_H_
#define REMOTING_BASE_SOCKET_READER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace net {
class IOBuffer;
class Socket;
}  // namespace net

namespace remoting {

// SocketReader reads data from a socket and then calls a callback for each
// completed read. Note that when this object is destroyed it may leave a
// pending read request for the socket, so the calling code should never try
// reading from the same socket again (e.g. it may result in data being lost).
class SocketReader {
 public:
  // Callback that is called for each finished read. |data| may be set to NULL
  // in case of an error (result < 0).
  typedef base::Callback<void(scoped_refptr<net::IOBuffer> data,
                              int result)> ReadResultCallback;

  SocketReader();
  ~SocketReader();

  // Starts reading from |socket|. |read_result_callback| is called for each
  // completed read. Reading stops on the first error. Must not be called more
  // than once.
  void Init(net::Socket* socket, ReadResultCallback read_result_callback);

 private:
  void DoRead();
  void OnRead(int result);
  void HandleReadResult(int result);
  void CallCallback(scoped_refptr<net::IOBuffer> data, int result);

  net::Socket* socket_;
  ReadResultCallback read_result_callback_;
  scoped_refptr<net::IOBuffer> read_buffer_;

  base::WeakPtrFactory<SocketReader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SocketReader);
};

}  // namespace remoting

#endif  // REMOTING_BASE_SOCKET_READER_H_
