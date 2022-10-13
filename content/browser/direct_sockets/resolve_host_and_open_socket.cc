// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/resolve_host_and_open_socket.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece_forward.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/public/browser/direct_sockets_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_response_headers.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {
namespace {

constexpr char kPermissionDeniedHistogramName[] =
    "DirectSockets.PermissionDeniedFailures";

constexpr base::TimeDelta kCorsRequestTimeout = base::Seconds(15);

constexpr int kHttpsPort = 443;

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

absl::optional<int> g_https_port_for_testing = {};

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

// static
int ResolveHostAndOpenSocket::GetHttpsPort() {
  if (g_https_port_for_testing) {
    return *g_https_port_for_testing;
  }
  return kHttpsPort;
}

// static
void ResolveHostAndOpenSocket::SetHttpsPortForTesting(
    absl::optional<int> https_port) {
  g_https_port_for_testing = https_port;
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

  if (result == net::OK && resolved_addresses) {
    if (port_ == GetHttpsPort()) {
      // Delegates to OpenSocket(...) after the check.
      // We cannot use the resolved address here since it causes problems
      // with SSL :(
      PerformCORSCheck(host_, *resolved_addresses);
      return;
    }
  }

  OpenSocket(result, resolved_addresses);
}

void ResolveHostAndOpenSocket::OpenSocket(
    int result,
    const absl::optional<net::AddressList>& resolved_addresses) {
  std::move(callback_).Run(result, resolved_addresses);
  delete this;
}

void ResolveHostAndOpenSocket::PerformCORSCheck(
    const std::string& address,
    net::AddressList resolved_addresses) {
  auto* frame = service_->GetFrameHost();

  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  frame->CreateNetworkServiceDefaultFactory(
      factory.BindNewPipeAndPassReceiver());

  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url =
      url::SchemeHostPort(url::kHttpsScheme, address, GetHttpsPort()).GetURL();
  if (!resource_request->url.is_valid()) {
    OpenSocket(net::ERR_INVALID_URL, {});
    return;
  }

  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  resource_request->mode =
      network::mojom::RequestMode::kCorsWithForcedPreflight;
  resource_request->request_initiator = frame->GetLastCommittedOrigin();

  auto loader = network::SimpleURLLoader::Create(
      std::move(resource_request),
      DirectSocketsServiceImpl::TrafficAnnotation());
  auto* loader_ptr = loader.get();

  loader_ptr->SetTimeoutDuration(kCorsRequestTimeout);

  loader_ptr->DownloadHeadersOnly(
      factory.get(),
      base::BindOnce(&ResolveHostAndOpenSocket::OnCORSCheckComplete,
                     base::Unretained(this), std::move(loader),
                     std::move(resolved_addresses)));
}

void ResolveHostAndOpenSocket::OnCORSCheckComplete(
    std::unique_ptr<network::SimpleURLLoader> loader,
    net::AddressList resolved_addresses,
    scoped_refptr<net::HttpResponseHeaders>) {
  if (const auto& completion_status = loader->CompletionStatus()) {
    if (auto status = completion_status->cors_error_status) {
      LOG(ERROR) << "Preflight failed: " << *status;
      base::UmaHistogramEnumeration(
          kPermissionDeniedHistogramName,
          blink::mojom::DirectSocketFailureType::kCORS);
      OpenSocket(net::ERR_BLOCKED_BY_RESPONSE, {});
      return;
    }
    if (completion_status->error_code != net::OK) {
      OpenSocket(completion_status->error_code, {});
      return;
    }
    OpenSocket(net::OK, std::move(resolved_addresses));
    return;
  }

  OpenSocket(net::ERR_TIMED_OUT, {});
}

}  // namespace content