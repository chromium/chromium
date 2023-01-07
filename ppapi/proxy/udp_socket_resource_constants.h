// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_UDP_SOCKET_RESOURCE_CONSTANTS_H_
#define PPAPI_PROXY_UDP_SOCKET_RESOURCE_CONSTANTS_H_

#include <stdint.h>

namespace ppapi {
namespace proxy {

class UDPSocketResourceConstants {
 public:
  UDPSocketResourceConstants(const UDPSocketResourceConstants&) = delete;
  UDPSocketResourceConstants& operator=(const UDPSocketResourceConstants&) =
      delete;

  // The maximum number of bytes that each
  // PpapiPluginMsg_PPBUDPSocket_PushRecvResult message is allowed to carry.
  enum { kMaxReadSize = 128 * 1024 };
  // The maximum number of bytes that each PpapiHostMsg_PPBUDPSocket_SendTo
  // message is allowed to carry.
  enum { kMaxWriteSize = 128 * 1024 };

  // The maximum number that we allow for setting
  // PP_UDPSOCKET_OPTION_SEND_BUFFER_SIZE. This number is only for input
  // argument sanity check, it doesn't mean the browser guarantees to support
  // such a buffer size.
  enum { kMaxSendBufferSize = 1024 * kMaxWriteSize };
  // The maximum number that we allow for setting
  // PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE. This number is only for input
  // argument sanity check, it doesn't mean the browser guarantees to support
  // such a buffer size.
  enum { kMaxReceiveBufferSize = 1024 * kMaxReadSize };

  // The maximum number of received packets that we allow instances of this
  // class to buffer.
  enum { kPluginReceiveBufferSlots = 32u };
  // The maximum number of buffers that we allow instances of this class to be
  // sending before we block the plugin.
  enum { kPluginSendBufferSlots = 8u };
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_UDP_SOCKET_RESOURCE_CONSTANTS_H_
