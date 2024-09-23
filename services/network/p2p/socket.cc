// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/p2p/socket.h"

#include <utility>

#include "base/containers/span_reader.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "net/base/net_errors.h"
#include "services/network/p2p/socket_tcp.h"
#include "services/network/p2p/socket_udp.h"
#include "services/network/proxy_resolving_client_socket_factory.h"

namespace {

// Used to back histogram value of "WebRTC.ICE.TcpSocketErrorCode" and
// "WebRTC.ICE.UdpSocketErrorCode".
enum class SocketErrorCode {
  ERR_MSG_TOO_BIG,
  ERR_ADDRESS_UNREACHABLE,
  ERR_ADDRESS_INVALID,
  ERR_INTERNET_DISCONNECTED,
  ERR_TIMED_OUT,
  ERR_INSUFFICIENT_RESOURCES,
  ERR_OUT_OF_MEMORY,
  ERR_OTHER  // For all the others
};

const uint32_t kStunMagicCookie = 0x2112A442;

// Map the network error to SocketErrorCode for the UMA histogram.
// static
static SocketErrorCode MapNetErrorToSocketErrorCode(int net_err) {
  switch (net_err) {
    case net::OK:
      NOTREACHED_IN_MIGRATION();
      return SocketErrorCode::ERR_OTHER;
    case net::ERR_MSG_TOO_BIG:
      return SocketErrorCode::ERR_MSG_TOO_BIG;
    case net::ERR_ADDRESS_UNREACHABLE:
      return SocketErrorCode::ERR_ADDRESS_UNREACHABLE;
    case net::ERR_ADDRESS_INVALID:
      return SocketErrorCode::ERR_ADDRESS_INVALID;
    case net::ERR_INTERNET_DISCONNECTED:
      return SocketErrorCode::ERR_INTERNET_DISCONNECTED;
    case net::ERR_TIMED_OUT:
      return SocketErrorCode::ERR_TIMED_OUT;
    case net::ERR_INSUFFICIENT_RESOURCES:
      return SocketErrorCode::ERR_INSUFFICIENT_RESOURCES;
    case net::ERR_OUT_OF_MEMORY:
      return SocketErrorCode::ERR_OUT_OF_MEMORY;
    default:
      return SocketErrorCode::ERR_OTHER;
  }
}
}  // namespace

namespace network {

P2PSocket::P2PSocket(Delegate* delegate,
                     mojo::PendingRemote<mojom::P2PSocketClient> client,
                     mojo::PendingReceiver<mojom::P2PSocket> socket,
                     ProtocolType protocol_type)
    : delegate_(delegate),
      client_(std::move(client)),
      receiver_(this, std::move(socket)),
      protocol_type_(protocol_type) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&P2PSocket::OnError, base::Unretained(this)));
}

P2PSocket::~P2PSocket() = default;

// Verifies that the packet |data| has a valid STUN header.
// static
bool P2PSocket::GetStunPacketType(base::span<const uint8_t> data,
                                  StunMessageType* type) {
  // See https://www.rfc-editor.org/rfc/rfc5389.html#section-6
  auto reader = base::SpanReader(data);
  uint16_t message_type, length;
  uint32_t cookie;
  if (data.size() < kStunHeaderSize ||            //
      !reader.ReadU16BigEndian(message_type) ||   //
      !reader.ReadU16BigEndian(length) ||         //
      length != data.size() - kStunHeaderSize ||  //
      !reader.ReadU32BigEndian(cookie) ||         //
      cookie != kStunMagicCookie) {
    return false;
  }

  // Verify that the type is known:
  switch (message_type) {
    case STUN_BINDING_REQUEST:
    case STUN_BINDING_RESPONSE:
    case STUN_BINDING_ERROR_RESPONSE:
    case STUN_SHARED_SECRET_REQUEST:
    case STUN_SHARED_SECRET_RESPONSE:
    case STUN_SHARED_SECRET_ERROR_RESPONSE:
    case STUN_ALLOCATE_REQUEST:
    case STUN_ALLOCATE_RESPONSE:
    case STUN_ALLOCATE_ERROR_RESPONSE:
    case STUN_SEND_REQUEST:
    case STUN_SEND_RESPONSE:
    case STUN_SEND_ERROR_RESPONSE:
    case STUN_DATA_INDICATION:
      *type = static_cast<StunMessageType>(message_type);
      return true;

    default:
      return false;
  }
}

// static
bool P2PSocket::IsRequestOrResponse(StunMessageType type) {
  return type == STUN_BINDING_REQUEST || type == STUN_BINDING_RESPONSE ||
         type == STUN_ALLOCATE_REQUEST || type == STUN_ALLOCATE_RESPONSE;
}

// static
void P2PSocket::ReportSocketError(int result, const char* histogram_name) {
  SocketErrorCode error_code = MapNetErrorToSocketErrorCode(result);
  UMA_HISTOGRAM_ENUMERATION(histogram_name, static_cast<int>(error_code),
                            static_cast<int>(SocketErrorCode::ERR_OTHER) + 1);
}

// static
std::unique_ptr<P2PSocket> P2PSocket::Create(
    Delegate* delegate,
    mojo::PendingRemote<mojom::P2PSocketClient> client,
    mojo::PendingReceiver<mojom::P2PSocket> socket,
    P2PSocketType type,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    net::NetLog* net_log,
    ProxyResolvingClientSocketFactory* proxy_resolving_socket_factory,
    P2PMessageThrottler* throttler,
    std::optional<base::UnguessableToken> devtools_token) {
  switch (type) {
    case P2P_SOCKET_UDP:
      return std::make_unique<P2PSocketUdp>(
          delegate, std::move(client), std::move(socket), throttler,
          traffic_annotation, net_log, devtools_token);
    case P2P_SOCKET_TCP_CLIENT:
    case P2P_SOCKET_SSLTCP_CLIENT:
    case P2P_SOCKET_TLS_CLIENT:
      return std::make_unique<P2PSocketTcp>(
          delegate, std::move(client), std::move(socket), type,
          traffic_annotation, proxy_resolving_socket_factory);

    case P2P_SOCKET_STUN_TCP_CLIENT:
    case P2P_SOCKET_STUN_SSLTCP_CLIENT:
    case P2P_SOCKET_STUN_TLS_CLIENT:
      return std::make_unique<P2PSocketStunTcp>(
          delegate, std::move(client), std::move(socket), type,
          traffic_annotation, proxy_resolving_socket_factory);
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

mojo::PendingRemote<mojom::P2PSocketClient>
P2PSocket::ReleaseClientForTesting() {
  return client_.Unbind();
}

mojo::PendingReceiver<mojom::P2PSocket> P2PSocket::ReleaseReceiverForTesting() {
  return receiver_.Unbind();
}

void P2PSocket::OnError() {
  receiver_.reset();
  client_.reset();
  delegate_->DestroySocket(this);
}

}  // namespace network
