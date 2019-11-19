// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_P2P_DATAGRAM_SOCKET_H_
#define REMOTING_PROTOCOL_P2P_DATAGRAM_SOCKET_H_

#include "net/base/completion_repeating_callback.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace remoting {
namespace protocol {

// Peer-to-peer socket with datagram semantics.
class P2PDatagramSocket {
 public:
  virtual ~P2PDatagramSocket() {}

  // Receives a packet, up to |buf_len| bytes, from the socket. Size of the
  // incoming packet is returned in case of success. If the packet is larger
  // than |buf_len| then it is truncated, i.e. only the first |buf_len| bytes
  // are stored in the buffer. In case of failure a net error code is returned.
  // ERR_IO_PENDING is returned if the operation could not be completed
  // synchronously, in which case the result will be passed to the callback when
  // available. If the operation is not completed immediately, the socket
  // acquires a reference to the provided buffer until the callback is invoked
  // or the socket is closed. If the socket is destroyed before the read
  // completes, the callback will not be invoked.
  virtual int Recv(const scoped_refptr<net::IOBuffer>& buf,
                   int buf_len,
                   const net::CompletionRepeatingCallback& callback) = 0;

  // Sends a packet. Returns |buf_len| to indicate success, otherwise a net
  // error code is returned. ERR_IO_PENDING is returned if the operation could
  // not be completed synchronously, in which case the result will be passed to
  // the callback when available. If the operation is not completed immediately,
  // the socket acquires a reference to the provided buffer until the callback
  // is invoked or the socket is closed. Implementations of this method should
  // not modify the contents of the actual buffer that is written to the socket.
  virtual int Send(const scoped_refptr<net::IOBuffer>& buf,
                   int buf_len,
                   const net::CompletionRepeatingCallback& callback) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_P2P_DATAGRAM_SOCKET_H_
