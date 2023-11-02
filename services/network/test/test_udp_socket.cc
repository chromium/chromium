// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_udp_socket.h"

#include "base/notreached.h"

namespace network {

TestUDPSocket::TestUDPSocket() = default;

TestUDPSocket::~TestUDPSocket() = default;

void TestUDPSocket::Bind(const net::IPEndPoint& local_addr,
                         network::mojom::UDPSocketOptionsPtr options,
                         BindCallback callback) {
  NOTIMPLEMENTED();
}

void TestUDPSocket::Connect(const net::IPEndPoint& remote_addr,
                            network::mojom::UDPSocketOptionsPtr socket_options,
                            ConnectCallback callback) {
  NOTIMPLEMENTED();
}

void TestUDPSocket::SetBroadcast(bool broadcast,
                                 SetBroadcastCallback callback) {
  NOTIMPLEMENTED();
}

void TestUDPSocket::SetSendBufferSize(int32_t send_buffer_size,
                                      SetSendBufferSizeCallback callback) {
  NOTIMPLEMENTED();
}

void TestUDPSocket::SetReceiveBufferSize(int32_t receive_buffer_size,
                                         SetSendBufferSizeCallback callback) {
  NOTIMPLEMENTED();
}

void TestUDPSocket::JoinGroup(const net::IPAddress& group_address,
                              JoinGroupCallback callback) {
  NOTIMPLEMENTED();
}

void TestUDPSocket::LeaveGroup(const net::IPAddress& group_address,
                               LeaveGroupCallback callback) {
  NOTIMPLEMENTED();
}

void TestUDPSocket::ReceiveMore(uint32_t num_additional_datagrams) {
  NOTIMPLEMENTED();
}

void TestUDPSocket::ReceiveMoreWithBufferSize(uint32_t num_additional_datagrams,
                                              uint32_t buffer_size) {
  NOTIMPLEMENTED();
}

void TestUDPSocket::SendTo(
    const net::IPEndPoint& dest_addr,
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendToCallback callback) {
  NOTIMPLEMENTED();
}

void TestUDPSocket::Send(
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendCallback callback) {
  NOTIMPLEMENTED();
}

void TestUDPSocket::Close() {
  NOTIMPLEMENTED();
}

}  // namespace network
