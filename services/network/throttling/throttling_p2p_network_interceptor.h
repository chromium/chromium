// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_THROTTLING_THROTTLING_P2P_NETWORK_INTERCEPTOR_H_
#define SERVICES_NETWORK_THROTTLING_THROTTLING_P2P_NETWORK_INTERCEPTOR_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "services/network/p2p/socket_udp.h"
#include "services/network/throttling/network_conditions.h"
#include "third_party/webrtc/call/simulated_network.h"

namespace network {

class P2PSocketUdp;

// ThrottlingP2PNetworkInterceptor emulates network conditions for WebRTC
// communications with specific client id.
class COMPONENT_EXPORT(NETWORK_SERVICE) ThrottlingP2PNetworkInterceptor {
 public:
  ThrottlingP2PNetworkInterceptor();
  virtual ~ThrottlingP2PNetworkInterceptor();

  void RegisterSocket(P2PSocketUdp* socket);
  void UnregisterSocket(P2PSocketUdp* socket);

  void EnqueueSend(P2PPendingPacket packet, P2PSocketUdp* socket);
  void EnqueueReceive(mojom::P2PReceivedPacketPtr packet,
                      scoped_refptr<net::IOBuffer> buffer,
                      P2PSocketUdp* socket);

  // Applies network emulation configuration.
  void UpdateConditions(const NetworkConditions& conditions);

 private:
  void OnSendNetworkTimer();
  void OnReceiveNetworkTimer();

  std::vector<P2PSocketUdp*> sockets_;
  std::unique_ptr<NetworkConditions> conditions_;

  webrtc::SimulatedNetwork send_network_;
  uint64_t send_packet_id_;
  struct StoredSendPacket {
    StoredSendPacket(raw_ptr<P2PSocketUdp> socket, P2PPendingPacket packet);
    ~StoredSendPacket();

    raw_ptr<P2PSocketUdp> socket;
    P2PPendingPacket packet;
  };
  using SendPacketMap = std::map<uint64_t, StoredSendPacket>;
  SendPacketMap send_packets_;
  base::OneShotTimer send_timer_;

  webrtc::SimulatedNetwork receive_network_;
  uint64_t receive_packet_id_;
  struct StoredReceivePacket {
    StoredReceivePacket(raw_ptr<P2PSocketUdp> socket,
                        mojom::P2PReceivedPacketPtr packet,
                        scoped_refptr<net::IOBuffer> buffer);
    ~StoredReceivePacket();

    raw_ptr<P2PSocketUdp> socket;
    mojom::P2PReceivedPacketPtr packet;
    scoped_refptr<net::IOBuffer> buffer;
  };
  using ReceivePacketMap = std::map<uint64_t, StoredReceivePacket>;
  ReceivePacketMap receive_packets_;
  base::OneShotTimer receive_timer_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_THROTTLING_THROTTLING_P2P_NETWORK_INTERCEPTOR_H_
