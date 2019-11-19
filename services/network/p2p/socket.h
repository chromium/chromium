// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_P2P_SOCKET_H_
#define SERVICES_NETWORK_P2P_SOCKET_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/datagram_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/public/mojom/p2p.mojom.h"

namespace net {
class NetLog;
}

namespace network {

class ProxyResolvingClientSocketFactory;
class P2PMessageThrottler;

// Base class for P2P sockets.
class COMPONENT_EXPORT(NETWORK_SERVICE) P2PSocket : public mojom::P2PSocket {
 public:
  // Interface implemented in P2PSocketManager.
  class Delegate {
   public:
    Delegate() = default;

    // Destroys |socket| and removes it from the list of sockets.
    virtual void DestroySocket(P2PSocket* socket) = 0;

    // Called by P2PSocketTcpServer after a new socket is created for an
    // incoming connection.
    virtual void AddAcceptedConnection(std::unique_ptr<P2PSocket> socket) = 0;

    // Called for each incoming/outgoing packet.
    virtual void DumpPacket(base::span<const uint8_t> data, bool incoming) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  static const int kStunHeaderSize = 20;
  static const size_t kMaximumPacketSize = 32768;

  // Creates P2PSocket of the specific type.
  static std::unique_ptr<P2PSocket> Create(
      Delegate* delegate,
      mojo::PendingRemote<mojom::P2PSocketClient> client,
      mojo::PendingReceiver<mojom::P2PSocket> socket,
      P2PSocketType type,
      net::NetLog* net_log,
      ProxyResolvingClientSocketFactory* proxy_resolving_socket_factory,
      P2PMessageThrottler* throttler);

  ~P2PSocket() override;

  // Initializes the socket. Returns false when initialization fails.
  // |min_port| and |max_port| specify the valid range of allowed ports.
  // |min_port| must be less than or equal to |max_port|.
  // If |min_port| is zero, |max_port| must also be zero and it means all ports
  // are valid.
  // If |local_address.port()| is zero, the socket will be initialized to a port
  // in the valid range.
  // If |local_address.port()| is nonzero and not in the valid range,
  // initialization will fail.
  virtual void Init(const net::IPEndPoint& local_address,
                    uint16_t min_port,
                    uint16_t max_port,
                    const P2PHostAndIPEndPoint& remote_address) = 0;

  mojo::PendingRemote<mojom::P2PSocketClient> ReleaseClientForTesting();
  mojo::PendingReceiver<mojom::P2PSocket> ReleaseReceiverForTesting();

 protected:
  friend class P2PSocketTcpTestBase;

  // This should match suffix IPProtocolType defined in histograms.xml.
  enum ProtocolType { UDP = 0x1, TCP = 0x2 };

  // TODO(mallinath) - Remove this below enum and use one defined in
  // libjingle/souce/talk/p2p/base/stun.h
  enum StunMessageType {
    STUN_BINDING_REQUEST = 0x0001,
    STUN_BINDING_RESPONSE = 0x0101,
    STUN_BINDING_ERROR_RESPONSE = 0x0111,
    STUN_SHARED_SECRET_REQUEST = 0x0002,
    STUN_SHARED_SECRET_RESPONSE = 0x0102,
    STUN_SHARED_SECRET_ERROR_RESPONSE = 0x0112,
    STUN_ALLOCATE_REQUEST = 0x0003,
    STUN_ALLOCATE_RESPONSE = 0x0103,
    STUN_ALLOCATE_ERROR_RESPONSE = 0x0113,
    STUN_SEND_REQUEST = 0x0004,
    STUN_SEND_RESPONSE = 0x0104,
    STUN_SEND_ERROR_RESPONSE = 0x0114,
    STUN_DATA_INDICATION = 0x0115,
    TURN_SEND_INDICATION = 0x0016,
    TURN_DATA_INDICATION = 0x0017,
    TURN_CREATE_PERMISSION_REQUEST = 0x0008,
    TURN_CREATE_PERMISSION_RESPONSE = 0x0108,
    TURN_CREATE_PERMISSION_ERROR_RESPONSE = 0x0118,
    TURN_CHANNEL_BIND_REQUEST = 0x0009,
    TURN_CHANNEL_BIND_RESPONSE = 0x0109,
    TURN_CHANNEL_BIND_ERROR_RESPONSE = 0x0119,
  };

  enum State {
    STATE_UNINITIALIZED,
    STATE_CONNECTING,
    STATE_OPEN,
  };

  P2PSocket(Delegate* delegate,
            mojo::PendingRemote<mojom::P2PSocketClient> client,
            mojo::PendingReceiver<mojom::P2PSocket> socket,
            ProtocolType protocol_type);

  // Verifies that the packet |data| has a valid STUN header. In case
  // of success stores type of the message in |type|.
  static bool GetStunPacketType(const uint8_t* data,
                                int data_size,
                                StunMessageType* type);
  static bool IsRequestOrResponse(StunMessageType type);

  static void ReportSocketError(int result, const char* histogram_name);

  // Should be called by subclasses on protocol errors. Closes P2PSocket and
  // P2PSocketClient channels and calls delegate_->DestroySocket() to
  // destroy the socket.
  void OnError();

  // Used by subclasses to track the metrics of delayed bytes and packets.
  void IncrementDelayedPackets();
  void IncrementTotalSentPackets();
  void IncrementDelayedBytes(uint32_t size);
  void DecrementDelayedBytes(uint32_t size);

  Delegate* delegate_;
  mojo::Remote<mojom::P2PSocketClient> client_;
  mojo::Receiver<mojom::P2PSocket> receiver_;

  ProtocolType protocol_type_;

 private:
  // Track total delayed packets for calculating how many packets are
  // delayed by system at the end of call.
  uint32_t send_packets_delayed_total_ = 0;
  uint32_t send_packets_total_ = 0;

  // Track the maximum of consecutive delayed bytes caused by system's
  // EWOULDBLOCK.
  int32_t send_bytes_delayed_max_ = 0;
  int32_t send_bytes_delayed_cur_ = 0;

  base::WeakPtrFactory<P2PSocket> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(P2PSocket);
};

}  // namespace network

#endif  // SERVICES_NETWORK_P2P_SOCKET_H_
