// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_SERVER_SOCKET_H_
#define NET_SOCKET_UDP_SERVER_SOCKET_H_

#include <stdint.h>

#include "base/macros.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/socket/datagram_server_socket.h"
#include "net/socket/udp_socket.h"

namespace net {

class IPAddress;
class IPEndPoint;
class NetLog;
struct NetLogSource;

// A server socket that uses UDP as the transport layer.
class NET_EXPORT UDPServerSocket : public DatagramServerSocket {
 public:
  UDPServerSocket(net::NetLog* net_log, const net::NetLogSource& source);
  ~UDPServerSocket() override;

  // Implement DatagramServerSocket:
  int Listen(const IPEndPoint& address) override;
  int RecvFrom(IOBuffer* buf,
               int buf_len,
               IPEndPoint* address,
               CompletionOnceCallback callback) override;
  int SendTo(IOBuffer* buf,
             int buf_len,
             const IPEndPoint& address,
             CompletionOnceCallback callback) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int SetDoNotFragment() override;
  void SetMsgConfirm(bool confirm) override;
  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  void UseNonBlockingIO() override;
  const NetLogWithSource& NetLog() const override;
  void AllowAddressReuse() override;
  void AllowBroadcast() override;
  void AllowAddressSharingForMulticast() override;
  int JoinGroup(const IPAddress& group_address) const override;
  int LeaveGroup(const IPAddress& group_address) const override;
  int SetMulticastInterface(uint32_t interface_index) override;
  int SetMulticastTimeToLive(int time_to_live) override;
  int SetMulticastLoopbackMode(bool loopback) override;
  int SetDiffServCodePoint(DiffServCodePoint dscp) override;
  void DetachFromThread() override;

 private:
  UDPSocket socket_;
  bool allow_address_reuse_;
  bool allow_broadcast_;
  bool allow_address_sharing_for_multicast_;
  DISALLOW_COPY_AND_ASSIGN(UDPServerSocket);
};

}  // namespace net

#endif  // NET_SOCKET_UDP_SERVER_SOCKET_H_
