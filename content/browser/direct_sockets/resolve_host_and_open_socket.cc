// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/resolve_host_and_open_socket.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/public/browser/direct_sockets_delegate.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_response_headers.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-shared.h"

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

bool ContainNonPubliclyRoutableAddress(const net::AddressList& addresses) {
  DCHECK(!addresses.empty());
  for (auto ip : addresses) {
    if (!ip.address().IsPubliclyRoutable())
      return true;
  }
  return false;
}

absl::optional<net::IPEndPoint> GetLocalAddr(
    const blink::mojom::DirectSocketOptions& options) {
  absl::optional<net::IPEndPoint> local_addr = absl::nullopt;
  if (!options.local_hostname) {
    return {};
  }

  if (net::IPAddress local_address;
      local_address.AssignFromIPLiteral(*options.local_hostname)) {
    return net::IPEndPoint(local_address, options.local_port);
  }

  return {};
}

absl::optional<int> g_https_port_for_testing = {};

}  // namespace

namespace content {

// ResolveHostAndOpenSocket implementation.

ResolveHostAndOpenSocket::ResolveHostAndOpenSocket(
    base::WeakPtr<DirectSocketsServiceImpl> service,
    blink::mojom::DirectSocketOptionsPtr options)
    : service_(std::move(service)), options_(std::move(options)) {}

ResolveHostAndOpenSocket::~ResolveHostAndOpenSocket() = default;

void ResolveHostAndOpenSocket::Start() {
  auto* network_context = service_->GetNetworkContext();
  DCHECK(network_context);
  DCHECK(!receiver_.is_bound());
  DCHECK(!resolver_.is_bound());

  if (net::IPAddress().AssignFromIPLiteral(options_->remote_hostname)) {
    is_raw_address_ = true;
  }

  network_context->CreateHostResolver(absl::nullopt,
                                      resolver_.BindNewPipeAndPassReceiver());

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
#if BUILDFLAG(ENABLE_MDNS)
  if (ResemblesMulticastDNSName(options_->remote_hostname)) {
    parameters->source = net::HostResolverSource::MULTICAST_DNS;
    is_mdns_name_ = true;
  }
#endif  // !BUILDFLAG(ENABLE_MDNS)
  // Intentionally using a HostPortPair because scheme isn't specified.
  resolver_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair(options_->remote_hostname, options_->remote_port)),
      net::NetworkAnonymizationKey::CreateTransient(), std::move(parameters),
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

  if (!service_) {
    OpenSocket(net::ERR_UNEXPECTED, {});
    return;
  }

  auto* frame = service_->GetFrameHost();
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
  if (!is_raw_address_ && !is_mdns_name_ && resolved_addresses &&
      ContainNonPubliclyRoutableAddress(*resolved_addresses)) {
    base::UmaHistogramEnumeration(
        kPermissionDeniedHistogramName,
        blink::mojom::DirectSocketFailureType::kResolvingToNonPublic);
    OpenSocket(net::ERR_NETWORK_ACCESS_DENIED, {});
    return;
  }

  if (result == net::OK && resolved_addresses) {
    if (options_->remote_port == GetHttpsPort()) {
      // Delegates to OpenSocket(...) after the check.
      // We cannot use the resolved address here since it causes problems
      // with SSL :(
      PerformCORSCheck(options_->remote_hostname, *resolved_addresses);
      return;
    }
  }

  OpenSocket(result, resolved_addresses);
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

// ResolveHostAndOpenTCPSocket implementation.

ResolveHostAndOpenTCPSocket::ResolveHostAndOpenTCPSocket(
    base::WeakPtr<DirectSocketsServiceImpl> service,
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    OpenTcpSocketCallback callback)
    : ResolveHostAndOpenSocket(std::move(service), std::move(options)),
      receiver_(std::move(receiver)),
      observer_(std::move(observer)),
      callback_(std::move(callback)) {}

ResolveHostAndOpenTCPSocket::~ResolveHostAndOpenTCPSocket() = default;

void ResolveHostAndOpenTCPSocket::OpenSocket(
    int result,
    const absl::optional<net::AddressList>& resolved_addresses) {
  network::mojom::NetworkContext* network_context = nullptr;
  if (service_) {
    network_context = service_->GetNetworkContext();
  }
  if (!network_context) {
    delete this;
    return;
  }

  if (result != net::OK) {
    std::move(callback_).Run(result, absl::nullopt, absl::nullopt,
                             mojo::ScopedDataPipeConsumerHandle(),
                             mojo::ScopedDataPipeProducerHandle());
    delete this;
    return;
  }

  DCHECK(resolved_addresses && !resolved_addresses->empty());
  const absl::optional<net::IPEndPoint> local_addr = GetLocalAddr(*options_);

  network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options =
      network::mojom::TCPConnectedSocketOptions::New();
  if (options_->send_buffer_size > 0) {
    tcp_connected_socket_options->send_buffer_size =
        std::min(options_->send_buffer_size,
                 DirectSocketsServiceImpl::GetMaxBufferSize());
  }
  if (options_->receive_buffer_size > 0) {
    tcp_connected_socket_options->receive_buffer_size =
        std::min(options_->receive_buffer_size,
                 DirectSocketsServiceImpl::GetMaxBufferSize());
  }
  tcp_connected_socket_options->no_delay = options_->no_delay;
  if (options_->keep_alive_options) {
    // options_->keep_alive_options will be invalidated.
    tcp_connected_socket_options->keep_alive_options =
        std::move(options_->keep_alive_options);
  }
  // invalidate options_.
  options_.reset();

  network_context->CreateTCPConnectedSocket(
      local_addr, *resolved_addresses, std::move(tcp_connected_socket_options),
      DirectSocketsServiceImpl::MutableTrafficAnnotation(),
      std::move(receiver_), std::move(observer_), std::move(callback_));
  delete this;
}

// ResolveHostAndOpenUDPSocket implementation.

ResolveHostAndOpenUDPSocket::ResolveHostAndOpenUDPSocket(
    base::WeakPtr<DirectSocketsServiceImpl> service,
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::DirectUDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenUdpSocketCallback callback)
    : ResolveHostAndOpenSocket(std::move(service), std::move(options)),
      receiver_(std::move(receiver)),
      listener_(std::move(listener)),
      callback_(std::move(callback)) {}

ResolveHostAndOpenUDPSocket::~ResolveHostAndOpenUDPSocket() = default;

void ResolveHostAndOpenUDPSocket::OpenSocket(
    int result,
    const absl::optional<net::AddressList>& resolved_addresses) {
  network::mojom::NetworkContext* network_context = nullptr;
  if (service_) {
    network_context = service_->GetNetworkContext();
  }
  if (!network_context) {
    delete this;
    return;
  }

  if (result != net::OK) {
    std::move(callback_).Run(result, absl::nullopt, absl::nullopt);
    delete this;
    return;
  }

  DCHECK(resolved_addresses && !resolved_addresses->empty());

  network::mojom::UDPSocketOptionsPtr udp_socket_options =
      network::mojom::UDPSocketOptions::New();
  if (options_->send_buffer_size > 0) {
    udp_socket_options->send_buffer_size =
        std::min(options_->send_buffer_size,
                 DirectSocketsServiceImpl::GetMaxBufferSize());
  }
  if (options_->receive_buffer_size > 0) {
    udp_socket_options->receive_buffer_size =
        std::min(options_->receive_buffer_size,
                 DirectSocketsServiceImpl::GetMaxBufferSize());
  }

  net::IPEndPoint peer_addr = resolved_addresses->front();
  auto direct_udp_socket = std::make_unique<DirectUDPSocketImpl>(
      network_context, std::move(listener_));
  direct_udp_socket->Connect(
      peer_addr, std::move(udp_socket_options),
      base::BindOnce(&ResolveHostAndOpenUDPSocket::OnUdpConnectCompleted,
                     base::Unretained(this), peer_addr));
  service_->AddDirectUDPSocketReceiver(std::move(direct_udp_socket),
                                       std::move(receiver_));
}

void ResolveHostAndOpenUDPSocket::OnUdpConnectCompleted(
    net::IPEndPoint peer_addr,
    int result,
    const absl::optional<net::IPEndPoint>& local_addr) {
  std::move(callback_).Run(result, local_addr, peer_addr);
  delete this;
}

}  // namespace content