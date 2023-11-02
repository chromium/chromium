// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TCP_SOCKET_RESOURCE_CONSTANTS_H_
#define PPAPI_PROXY_TCP_SOCKET_RESOURCE_CONSTANTS_H_

#include <stdint.h>

namespace ppapi {
namespace proxy {

class TCPSocketResourceConstants {
 public:
  TCPSocketResourceConstants(const TCPSocketResourceConstants&) = delete;
  TCPSocketResourceConstants& operator=(const TCPSocketResourceConstants&) =
      delete;

  // The maximum number of bytes that each PpapiHostMsg_PPBTCPSocket_Read
  // message is allowed to request.
  enum { kMaxReadSize = 1024 * 1024 };
  // The maximum number of bytes that each PpapiHostMsg_PPBTCPSocket_Write
  // message is allowed to carry.
  enum { kMaxWriteSize = 1024 * 1024 };

  // The maximum number that we allow for setting
  // PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE. This number is only for input
  // argument sanity check, it doesn't mean the browser guarantees to support
  // such a buffer size.
  enum { kMaxSendBufferSize = 1024 * kMaxWriteSize };
  // The maximum number that we allow for setting
  // PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE. This number is only for input
  // argument sanity check, it doesn't mean the browser guarantees to support
  // such a buffer size.
  enum { kMaxReceiveBufferSize = 1024 * kMaxReadSize };
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TCP_SOCKET_RESOURCE_CONSTANTS_H_
