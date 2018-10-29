// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/udp_server_socket.h"

#include <utility>

#include "net/base/net_errors.h"

namespace net {

UDPServerSocket::UDPServerSocket(net::NetLog* net_log,
                                 const net::NetLogSource& source)
    : socket_(DatagramSocket::DEFAULT_BIND, net_log, source),
      allow_address_reuse_(false),
      allow_broadcast_(false),
      allow_address_sharing_for_multicast_(false) {}

UDPServerSocket::~UDPServerSocket() = default;

int UDPServerSocket::Listen(const IPEndPoint& address) {
  int rv = socket_.Open(address.GetFamily());
  if (rv != OK)
    return rv;

  if (allow_address_reuse_) {
    rv = socket_.AllowAddressReuse();
    if (rv != OK) {
      socket_.Close();
      return rv;
    }
  }

  if (allow_broadcast_) {
    rv = socket_.SetBroadcast(true);
    if (rv != OK) {
      socket_.Close();
      return rv;
    }
  }

  if (allow_address_sharing_for_multicast_) {
    rv = socket_.AllowAddressSharingForMulticast();
    if (rv != OK) {
      socket_.Close();
      return rv;
    }
  }

  return socket_.Bind(address);
}

int UDPServerSocket::RecvFrom(IOBuffer* buf,
                              int buf_len,
                              IPEndPoint* address,
                              CompletionOnceCallback callback) {
  return socket_.RecvFrom(buf, buf_len, address, std::move(callback));
}

int UDPServerSocket::SendTo(IOBuffer* buf,
                            int buf_len,
                            const IPEndPoint& address,
                            CompletionOnceCallback callback) {
  return socket_.SendTo(buf, buf_len, address, std::move(callback));
}

int UDPServerSocket::SetReceiveBufferSize(int32_t size) {
  return socket_.SetReceiveBufferSize(size);
}

int UDPServerSocket::SetSendBufferSize(int32_t size) {
  return socket_.SetSendBufferSize(size);
}

int UDPServerSocket::SetDoNotFragment() {
  return socket_.SetDoNotFragment();
}

void UDPServerSocket::SetMsgConfirm(bool confirm) {
  return socket_.SetMsgConfirm(confirm);
}

void UDPServerSocket::Close() {
  socket_.Close();
}

int UDPServerSocket::GetPeerAddress(IPEndPoint* address) const {
  return socket_.GetPeerAddress(address);
}

int UDPServerSocket::GetLocalAddress(IPEndPoint* address) const {
  return socket_.GetLocalAddress(address);
}

const NetLogWithSource& UDPServerSocket::NetLog() const {
  return socket_.NetLog();
}

void UDPServerSocket::AllowAddressReuse() {
  allow_address_reuse_ = true;
}

void UDPServerSocket::AllowBroadcast() {
  allow_broadcast_ = true;
}

void UDPServerSocket::AllowAddressSharingForMulticast() {
  allow_address_sharing_for_multicast_ = true;
}

int UDPServerSocket::JoinGroup(const IPAddress& group_address) const {
  return socket_.JoinGroup(group_address);
}

int UDPServerSocket::LeaveGroup(const IPAddress& group_address) const {
  return socket_.LeaveGroup(group_address);
}

int UDPServerSocket::SetMulticastInterface(uint32_t interface_index) {
  return socket_.SetMulticastInterface(interface_index);
}

int UDPServerSocket::SetMulticastTimeToLive(int time_to_live) {
  return socket_.SetMulticastTimeToLive(time_to_live);
}

int UDPServerSocket::SetMulticastLoopbackMode(bool loopback) {
  return socket_.SetMulticastLoopbackMode(loopback);
}

int UDPServerSocket::SetDiffServCodePoint(DiffServCodePoint dscp) {
  return socket_.SetDiffServCodePoint(dscp);
}

void UDPServerSocket::DetachFromThread() {
  socket_.DetachFromThread();
}

void UDPServerSocket::UseNonBlockingIO() {
#if defined(OS_WIN)
  socket_.UseNonBlockingIO();
#endif
}

}  // namespace net
