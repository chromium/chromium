// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_udp_socket.h"

#include "base/functional/bind.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/cpp/simple_host_resolver.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/udp_socket.h"

namespace network {

RestrictedUDPSocket::RestrictedUDPSocket(
    std::unique_ptr<UDPSocket> udp_socket,
    net::MutableNetworkTrafficAnnotationTag traffic_annotation,
    std::unique_ptr<SimpleHostResolver> resolver)
    : udp_socket_(std::move(udp_socket)),
      traffic_annotation_(std::move(traffic_annotation)),
      resolver_(std::move(resolver)) {}

RestrictedUDPSocket::~RestrictedUDPSocket() = default;

void RestrictedUDPSocket::ReceiveMore(uint32_t num_additional_datagrams) {
  udp_socket_->ReceiveMore(num_additional_datagrams);
}

void RestrictedUDPSocket::Send(base::span<const uint8_t> data,
                               SendCallback callback) {
  udp_socket_->Send(std::move(data), traffic_annotation_, std::move(callback));
}

void RestrictedUDPSocket::SendTo(base::span<const uint8_t> data,
                                 const net::HostPortPair& dest_addr,
                                 net::DnsQueryType dns_query_type,
                                 SendToCallback callback) {
  // If a raw IP address is supplied, call SendTo() immediately.
  if (net::IPAddress address; address.AssignFromIPLiteral(dest_addr.host())) {
    udp_socket_->SendTo(net::IPEndPoint(std::move(address), dest_addr.port()),
                        data, traffic_annotation_, std::move(callback));
    return;
  }

  auto params = mojom::ResolveHostParameters::New();
  params->dns_query_type = dns_query_type;

  resolver_->ResolveHost(
      mojom::HostResolverHost::NewHostPortPair(dest_addr),
      net::NetworkAnonymizationKey::CreateTransient(), std::move(params),
      base::BindOnce(&RestrictedUDPSocket::OnResolveCompleteForSendTo,
                     base::Unretained(this),
                     /*data=*/std::vector<uint8_t>(data.begin(), data.end()),
                     /*callback=*/std::move(callback)));
}

#if BUILDFLAG(IS_CHROMEOS)
void RestrictedUDPSocket::AttachConnectionTracker(
    mojo::PendingRemote<mojom::SocketConnectionTracker> connection_tracker) {
  connection_tracker_ = std::move(connection_tracker);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void RestrictedUDPSocket::OnResolveCompleteForSendTo(
    std::vector<uint8_t> data,
    SendToCallback callback,
    int result,
    const net::ResolveErrorInfo&,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&) {
  if (result != net::OK) {
    std::move(callback).Run(result);
    return;
  }

  DCHECK(resolved_addresses);
  udp_socket_->SendTo(resolved_addresses->front(), std::move(data),
                      traffic_annotation_, std::move(callback));
}

}  // namespace network
