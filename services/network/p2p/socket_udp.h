// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_P2P_SOCKET_UDP_H_
#define SERVICES_NETWORK_P2P_SOCKET_UDP_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/diff_serv_code_point.h"
#include "net/socket/udp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/p2p/socket.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"

namespace net {
class NetLog;
}  // namespace net

namespace network {

class P2PMessageThrottler;

class COMPONENT_EXPORT(NETWORK_SERVICE) P2PSocketUdp : public P2PSocket {
 public:
  typedef base::Callback<std::unique_ptr<net::DatagramServerSocket>(
      net::NetLog* net_log)>
      DatagramServerSocketFactory;
  P2PSocketUdp(Delegate* delegate,
               mojo::PendingRemote<mojom::P2PSocketClient> client,
               mojo::PendingReceiver<mojom::P2PSocket> socket,
               P2PMessageThrottler* throttler,
               net::NetLog* net_log,
               const DatagramServerSocketFactory& socket_factory);
  P2PSocketUdp(Delegate* delegate,
               mojo::PendingRemote<mojom::P2PSocketClient> client,
               mojo::PendingReceiver<mojom::P2PSocket> socket,
               P2PMessageThrottler* throttler,
               net::NetLog* net_log);
  ~P2PSocketUdp() override;

  // P2PSocket overrides.
  void Init(const net::IPEndPoint& local_address,
            uint16_t min_port,
            uint16_t max_port,
            const P2PHostAndIPEndPoint& remote_address) override;

  // mojom::P2PSocket implementation:
  void Send(const std::vector<int8_t>& data,
            const P2PPacketInfo& packet_info,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void SetOption(P2PSocketOption option, int32_t value) override;

 private:
  friend class P2PSocketUdpTest;

  typedef std::set<net::IPEndPoint> ConnectedPeerSet;

  struct PendingPacket {
    PendingPacket(const net::IPEndPoint& to,
                  const std::vector<int8_t>& content,
                  const rtc::PacketOptions& options,
                  uint64_t id,
                  const net::NetworkTrafficAnnotationTag traffic_annotation);
    PendingPacket(const PendingPacket& other);
    ~PendingPacket();
    net::IPEndPoint to;
    scoped_refptr<net::IOBuffer> data;
    int size;
    rtc::PacketOptions packet_options;
    uint64_t id;
    const net::NetworkTrafficAnnotationTag traffic_annotation;
  };

  void DoRead();
  void OnRecv(int result);

  // Following 3 methods return false if the result was an error and the socket
  // was destroyed. The caller should stop using |this| in that case.
  WARN_UNUSED_RESULT bool HandleReadResult(int result);
  WARN_UNUSED_RESULT bool HandleSendResult(uint64_t packet_id,
                                           int32_t transport_sequence_number,
                                           int64_t send_time_ms,
                                           int result);
  WARN_UNUSED_RESULT bool DoSend(const PendingPacket& packet);

  void OnSend(uint64_t packet_id,
              int32_t transport_sequence_number,
              int64_t send_time_ms,
              int result);

  int SetSocketDiffServCodePointInternal(net::DiffServCodePoint dscp);

  std::unique_ptr<net::DatagramServerSocket> socket_;
  scoped_refptr<net::IOBuffer> recv_buffer_;
  net::IPEndPoint recv_address_;

  base::circular_deque<PendingPacket> send_queue_;
  bool send_pending_ = false;
  net::DiffServCodePoint last_dscp_ = net::DSCP_CS0;

  // Set of peer for which we have received STUN binding request or
  // response or relay allocation request or response.
  ConnectedPeerSet connected_peers_;
  P2PMessageThrottler* throttler_;

  net::NetLog* net_log_;

  // Callback object that returns a new socket when invoked.
  DatagramServerSocketFactory socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketUdp);
};

}  // namespace network

#endif  // SERVICES_NETWORK_P2P_SOCKET_UDP_H_
