// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_P2P_STREAM_SOCKET_H_
#define REMOTING_PROTOCOL_P2P_STREAM_SOCKET_H_

#include "net/base/completion_once_callback.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace remoting::protocol {

// Peer-to-peer socket with stream semantics.
class P2PStreamSocket {
 public:
  virtual ~P2PStreamSocket() {}

  // Reads data, up to |buf_len| bytes, from the socket. The number of bytes
  // read is returned, or an error is returned upon failure. ERR_IO_PENDING
  // is returned if the operation could not be completed synchronously, in which
  // case the result will be passed to the callback when available. If the
  // operation is not completed immediately, the socket acquires a reference to
  // the provided buffer until the callback is invoked or the socket is
  // closed. If the socket is destroyed before the read completes, the
  // callback will not be invoked.
  virtual int Read(const scoped_refptr<net::IOBuffer>& buf,
                   int buf_len,
                   net::CompletionOnceCallback callback) = 0;

  // Writes data, up to |buf_len| bytes, to the socket. Note: data may be
  // written partially. The number of bytes written is returned, or an error
  // is returned upon failure. ERR_IO_PENDING is returned if the operation could
  // not be completed synchronously, in which case the result will be passed to
  // the callback when available.  If the operation is not completed
  // immediately, the socket acquires a reference to the provided buffer until
  // the callback is invoked or the socket is closed. Implementations of this
  // method should not modify the contents of the actual buffer that is written
  // to the socket.
  virtual int Write(
      const scoped_refptr<net::IOBuffer>& buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_P2P_STREAM_SOCKET_H_
