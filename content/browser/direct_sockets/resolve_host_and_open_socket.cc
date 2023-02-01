// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/resolve_host_and_open_socket.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/address_list.h"
#include "net/net_buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {
namespace {

#if BUILDFLAG(ENABLE_MDNS)
bool ResemblesMulticastDNSName(const std::string& hostname) {
  return base::EndsWith(hostname, ".local") ||
         base::EndsWith(hostname, ".local.");
}
#endif  // !BUILDFLAG(ENABLE_MDNS)

}  // namespace

ResolveHostAndOpenSocket::ResolveHostAndOpenSocket(net::HostPortPair addr,
                                                   OpenSocketCallback callback)
    : addr_(addr), callback_(std::move(callback)) {}

ResolveHostAndOpenSocket::~ResolveHostAndOpenSocket() = default;

// static
ResolveHostAndOpenSocket* ResolveHostAndOpenSocket::Create(
    net::HostPortPair addr,
    OpenSocketCallback callback) {
  return new ResolveHostAndOpenSocket(std::move(addr), std::move(callback));
}

void ResolveHostAndOpenSocket::Start(
    network::mojom::NetworkContext* network_context) {
  DCHECK(!receiver_.is_bound());
  DCHECK(!resolver_.is_bound());

  network_context->CreateHostResolver(
      /*config_overrides=*/absl::nullopt,
      resolver_.BindNewPipeAndPassReceiver());

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
#if BUILDFLAG(ENABLE_MDNS)
  if (ResemblesMulticastDNSName(addr_.host())) {
    parameters->source = net::HostResolverSource::MULTICAST_DNS;
  }
#endif  // !BUILDFLAG(ENABLE_MDNS)
  // Intentionally using a HostPortPair because scheme isn't specified.
  resolver_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(addr_),
      net::NetworkAnonymizationKey::CreateTransient(), std::move(parameters),
      receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &ResolveHostAndOpenSocket::OnComplete, base::Unretained(this),
      net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
      /*resolved_addresses=*/absl::nullopt,
      /*endpoint_results_with_metadata=*/absl::nullopt));
}

void ResolveHostAndOpenSocket::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const absl::optional<net::AddressList>& resolved_addresses,
    const absl::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata) {
  DCHECK(receiver_.is_bound());
  receiver_.reset();
  resolver_.reset();

  std::move(callback_).Run(result, resolved_addresses);
  delete this;
}

}  // namespace content
