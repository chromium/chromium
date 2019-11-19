// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines some basic types used by the P2P-related IPC
// messages.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_P2P_SOCKET_TYPE_H_
#define SERVICES_NETWORK_PUBLIC_CPP_P2P_SOCKET_TYPE_H_

#include <stdint.h>

#include <string>

#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"

namespace network {

enum P2PSocketOption {
  P2P_SOCKET_OPT_RCVBUF,  // Receive buffer size.
  P2P_SOCKET_OPT_SNDBUF,  // Send buffer size.
  P2P_SOCKET_OPT_DSCP,    // DSCP code.
  P2P_SOCKET_OPT_MAX
};

// Type of P2P Socket.
enum P2PSocketType {
  P2P_SOCKET_UDP,
  P2P_SOCKET_TCP_SERVER,
  P2P_SOCKET_STUN_TCP_SERVER,
  P2P_SOCKET_TCP_CLIENT,
  P2P_SOCKET_STUN_TCP_CLIENT,
  P2P_SOCKET_SSLTCP_CLIENT,
  P2P_SOCKET_STUN_SSLTCP_CLIENT,
  P2P_SOCKET_TLS_CLIENT,
  P2P_SOCKET_STUN_TLS_CLIENT,
  P2P_SOCKET_TYPE_LAST = P2P_SOCKET_STUN_TLS_CLIENT
};

// Struct which carries both resolved IP address and host string literal.
// Port number will be part of |ip_address|.
struct P2PHostAndIPEndPoint {
  P2PHostAndIPEndPoint() {}
  P2PHostAndIPEndPoint(const std::string& hostname,
                       const net::IPEndPoint& ip_address)
      : hostname(hostname), ip_address(ip_address) {}

  std::string hostname;
  net::IPEndPoint ip_address;
};

// Struct which keeps track of metrics during a send operation on P2P sockets.
struct P2PSendPacketMetrics {
  P2PSendPacketMetrics() {}
  P2PSendPacketMetrics(uint64_t packet_id,
                       int32_t rtc_packet_id,
                       int64_t send_time_ms)
      : packet_id(packet_id),
        rtc_packet_id(rtc_packet_id),
        send_time_ms(send_time_ms) {}

  uint64_t packet_id = 0;
  // rtc_packet_id is a sequential packet counter written in the RTP header and
  // used by RTP receivers to ACK received packets. It is sent back with a
  // corresponding send time to WebRTC in the browser process so that it can be
  // combined with ACKs to compute inter-packet delay variations.
  int32_t rtc_packet_id = -1;

  // The time the packet was sent. Should be set using the webrtc clock
  // rtc::TimeMillis()
  int64_t send_time_ms = -1;
};

// Struct that carries a port range.
struct P2PPortRange {
  P2PPortRange() : P2PPortRange(0, 0) {}
  P2PPortRange(uint16_t min_port, uint16_t max_port)
      : min_port(min_port), max_port(max_port) {
    DCHECK_LE(min_port, max_port);
    DCHECK((min_port == 0 && max_port == 0) || min_port > 0);
  }
  uint16_t min_port;
  uint16_t max_port;
};

// Struct that carries information about an outgoing packet.
struct P2PPacketInfo {
  P2PPacketInfo() {}
  P2PPacketInfo(const net::IPEndPoint& destination,
                const rtc::PacketOptions& packet_options,
                uint64_t packet_id)
      : destination(destination),
        packet_options(packet_options),
        packet_id(packet_id) {}
  net::IPEndPoint destination;
  rtc::PacketOptions packet_options;
  uint64_t packet_id;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_P2P_SOCKET_TYPE_H_
