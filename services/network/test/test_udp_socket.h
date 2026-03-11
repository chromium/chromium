// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_UDP_SOCKET_H_
#define SERVICES_NETWORK_TEST_TEST_UDP_SOCKET_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace net {
class IPAddress;
class IPEndPoint;
struct MutableNetworkTrafficAnnotationTag;
}  // namespace net

namespace network {

// Noop implementation of mojom::UDPSocket.  Useful to override to create
// specialized mocks or fakes.
class TestUDPSocket : public mojom::UDPSocket {
 public:
  TestUDPSocket();
  TestUDPSocket(const TestUDPSocket&) = delete;
  TestUDPSocket& operator=(const TestUDPSocket&) = delete;
  ~TestUDPSocket() override;

  // mojom::UDPSocket:
  void Bind(const net::IPEndPoint& local_addr,
            network::mojom::UDPSocketOptionsPtr options,
            BindCallback callback) override;
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr socket_options,
               ConnectCallback callback) override;
  void SetBroadcast(bool broadcast, SetBroadcastCallback callback) override;
  void SetSendBufferSize(int32_t send_buffer_size,
                         SetSendBufferSizeCallback callback) override;
  void SetReceiveBufferSize(int32_t receive_buffer_size,
                            SetSendBufferSizeCallback callback) override;
  void JoinGroup(const net::IPAddress& group_address,
                 JoinGroupCallback callback) override;
  void LeaveGroup(const net::IPAddress& group_address,
                  LeaveGroupCallback callback) override;
  void ReceiveMore(uint32_t num_additional_datagrams) override;
  void ReceiveMoreWithBufferSize(uint32_t num_additional_datagrams,
                                 uint32_t buffer_size) override;
  void SendTo(const net::IPEndPoint& dest_addr,
              base::span<const uint8_t> data,
              const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
              SendToCallback callback) override;
  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override;
  void Close() override;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_UDP_SOCKET_H_
