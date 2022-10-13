// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/resolve_host_and_open_socket.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/public/browser/direct_sockets_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/net_buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {
namespace {

constexpr char kPermissionDeniedHistogramName[] =
    "DirectSockets.PermissionDeniedFailures";

#if BUILDFLAG(ENABLE_MDNS)
bool ResemblesMulticastDNSName(const std::string& hostname) {
  return base::EndsWith(hostname, ".local") ||
         base::EndsWith(hostname, ".local.");
}
#endif  // !BUILDFLAG(ENABLE_MDNS)

bool IsRawIPAddress(const std::string& address) {
  net::IPAddress ip;
  return ip.AssignFromIPLiteral(address);
}

bool ContainNonPubliclyRoutableAddress(const net::AddressList& addresses) {
  DCHECK(!addresses.empty());
  return !base::ranges::all_of(addresses, &net::IPAddress::IsPubliclyRoutable,
                               &net::IPEndPoint::address);
}

RenderFrameHost* GetFrameHostFromService(
    base::WeakPtr<DirectSocketsServiceImpl> service) {
  if (!service) {
    return nullptr;
  }
  return service->GetFrameHost();
}

}  // namespace

ResolveHostAndOpenSocket::ResolveHostAndOpenSocket(
    base::WeakPtr<DirectSocketsServiceImpl> service,
    const std::string& host,
    uint16_t port,
    OpenSocketCallback callback)
    : service_(service),
      host_(host),
      port_(port),
      callback_(std::move(callback)) {}

ResolveHostAndOpenSocket::~ResolveHostAndOpenSocket() = default;

// static
ResolveHostAndOpenSocket* ResolveHostAndOpenSocket::Create(
    base::WeakPtr<DirectSocketsServiceImpl> service,
    const std::string& host,
    uint16_t port,
    OpenSocketCallback callback) {
  return new ResolveHostAndOpenSocket(std::move(service), host, port,
                                      std::move(callback));
}

void ResolveHostAndOpenSocket::Start() {
  auto* network_context = service_->GetNetworkContext();
  DCHECK(network_context);
  DCHECK(!receiver_.is_bound());
  DCHECK(!resolver_.is_bound());

  network_context->CreateHostResolver(/*config_overrides=*/absl::nullopt,
                                      resolver_.BindNewPipeAndPassReceiver());

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
#if BUILDFLAG(ENABLE_MDNS)
  if (ResemblesMulticastDNSName(host_)) {
    parameters->source = net::HostResolverSource::MULTICAST_DNS;
    is_mdns_name_ = true;
  }
#endif  // !BUILDFLAG(ENABLE_MDNS)
  // Intentionally using a HostPortPair because scheme isn't specified.
  resolver_->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                             net::HostPortPair{host_, port_}),
                         net::NetworkAnonymizationKey::CreateTransient(),
                         std::move(parameters),
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

  auto* frame = GetFrameHostFromService(service_);
  if (!frame) {
    OpenSocket(net::ERR_UNEXPECTED, {});
    return;
  }

  if (auto* delegate = DirectSocketsServiceImpl::GetDelegate();
      delegate && delegate->ShouldSkipPostResolveChecks(frame)) {
    OpenSocket(result, resolved_addresses);
    return;
  }

  // Reject hostnames that resolve to non-public exception unless a raw IP
  // address or a *.local hostname is entered by the user.
  if (!IsRawIPAddress(host_) && !is_mdns_name_ && resolved_addresses &&
      ContainNonPubliclyRoutableAddress(*resolved_addresses)) {
    base::UmaHistogramEnumeration(
        kPermissionDeniedHistogramName,
        blink::mojom::DirectSocketFailureType::kResolvingToNonPublic);
    OpenSocket(net::ERR_NETWORK_ACCESS_DENIED, {});
    return;
  }

  OpenSocket(result, resolved_addresses);
}

void ResolveHostAndOpenSocket::OpenSocket(
    int result,
    const absl::optional<net::AddressList>& resolved_addresses) {
  std::move(callback_).Run(result, resolved_addresses);
  delete this;
}

}  // namespace content