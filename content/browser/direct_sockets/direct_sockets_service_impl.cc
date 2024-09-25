// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_sockets_service_impl.h"

#include <optional>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/direct_sockets_delegate.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/isolated_context_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/simple_host_resolver.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/restricted_udp_socket.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX)
#include <sys/socket.h>
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/firewall_hole/firewall_hole.h"
#include "services/network/public/mojom/socket_connection_tracker.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace content {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
bool g_always_open_firewall_hole_for_testing = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

constexpr net::NetworkTrafficAnnotationTag kDirectSocketsTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("direct_sockets", R"(
        semantics {
          sender: "Direct Sockets API"
          description: "Web app request to communicate with network device"
          trigger: "User completes network connection dialog"
          data: "Any data sent by web app"
          destination: OTHER
          destination_other: "Address entered by user in connection dialog"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot yet be controlled by settings."
          policy_exception_justification: "To be implemented"
        }
      )");

network::mojom::NetworkContext*& GetNetworkContextForTesting() {
  static network::mojom::NetworkContext* network_context = nullptr;
  return network_context;
}

// Runs the supplied `callback` with `net_error` and default params for other
// args.
template <typename... Args>
void FulfillWithError(base::OnceCallback<void(int32_t, Args...)> callback,
                      int32_t net_error) {
  std::move(callback).Run(net_error, std::remove_cvref_t<Args>()...);
}

bool IsAPIAccessAllowed(RenderFrameHost& rfh) {
  auto* delegate = GetContentClient()->browser()->GetDirectSocketsDelegate();
  if (!delegate) {
    // No additional rules from the embedder.
    return true;
  }
  return delegate->IsAPIAccessAllowed(rfh);
}

bool ValidateAddressAndPort(RenderFrameHost& rfh,
                            const std::string& address,
                            uint16_t port,
                            DirectSocketsDelegate::ProtocolType protocol) {
  auto* delegate = GetContentClient()->browser()->GetDirectSocketsDelegate();
  if (!delegate) {
    // No additional rules from the embedder.
    return true;
  }
  return delegate->ValidateAddressAndPort(rfh, address, port, protocol);
}

bool ValidateAddressAndPort(RenderFrameHost& rfh,
                            const net::IPEndPoint& ip_endpoint,
                            DirectSocketsDelegate::ProtocolType protocol) {
  return ValidateAddressAndPort(rfh, ip_endpoint.address().ToString(),
                                ip_endpoint.port(), protocol);
}

bool ValidateAddressAndPort(RenderFrameHost& rfh,
                            const net::HostPortPair& host_port_pair,
                            DirectSocketsDelegate::ProtocolType protocol) {
  return ValidateAddressAndPort(rfh, host_port_pair.host(),
                                host_port_pair.port(), protocol);
}

#if BUILDFLAG(IS_CHROMEOS)
bool ShouldOpenFirewallHole(const net::IPAddress& address) {
  if (g_always_open_firewall_hole_for_testing) {
    return true;
  }
  return !address.IsLoopback();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool RequiresPrivateNetworkAccess(const net::AddressList& addresses) {
  return base::ranges::any_of(
      addresses.endpoints(), [](const net::IPEndPoint& ip_endpoint) {
        return network::IPAddressToIPAddressSpace(ip_endpoint.address()) ==
               network::mojom::IPAddressSpace::kPrivate;
      });
}

void RequestPrivateNetworkAccess(content::RenderFrameHost& rfh,
                                 base::OnceCallback<void(bool)> callback) {
  if (!rfh.IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kDirectSocketsPrivate)) {
    std::move(callback).Run(/*access_allowed=*/false);
    return;
  }

  auto* delegate = GetContentClient()->browser()->GetDirectSocketsDelegate();
  if (!delegate) {
    // No additional rules from the embedder.
    std::move(callback).Run(/*access_allowed=*/true);
    return;
  }
  return delegate->RequestPrivateNetworkAccess(rfh, std::move(callback));
}

template <typename FinishCallback>
void CreateSocketIfAllowed(
    base::OnceCallback<void(FinishCallback)> create_socket_callback,
    FinishCallback finish_callback,
    bool access_allowed) {
  if (access_allowed) {
    std::move(create_socket_callback).Run(std::move(finish_callback));
    return;
  }
  FulfillWithError(std::move(finish_callback),
                   net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
}

// Queries the embedder whether private network access is allowed, and on
// success invokes `create_socket_callback` with `finish_callback`. Upon failure
// discards `create_socket_callback` and errors `finish_callback` with
// net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS.
template <typename FinishCallback>
void RequestPrivateNetworkAccessAndCreateSocket(
    content::RenderFrameHost& rfh,
    base::OnceCallback<void(FinishCallback)> create_socket_callback,
    FinishCallback finish_callback) {
  RequestPrivateNetworkAccess(
      rfh, base::BindOnce(&CreateSocketIfAllowed<FinishCallback>,
                          std::move(create_socket_callback),
                          std::move(finish_callback)));
}

}  // namespace

#if BUILDFLAG(IS_CHROMEOS)
// This class inherits from SocketConnectionTracker so that all stored firewall
// hole handles reference |this| in the internal ReceiverSet.
class DirectSocketsServiceImpl::FirewallHoleDelegate
    : public network::mojom::SocketConnectionTracker {
 public:
  void OpenTCPFirewallHole(
      mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
          connection_tracker,
      OpenTCPServerSocketCallback callback,
      int32_t result,
      const std::optional<net::IPEndPoint>& local_addr) {
    if (result != net::OK) {
      FulfillWithError(std::move(callback), result);
      return;
    }
    if (!ShouldOpenFirewallHole(local_addr->address())) {
      std::move(callback).Run(net::OK, *local_addr);
      return;
    }
    auto [callback_a, callback_b] =
        base::SplitOnceCallback(std::move(callback));
    chromeos::FirewallHole::Open(
        chromeos::FirewallHole::PortType::kTcp, local_addr->port(),
        "" /*all interfaces*/,
        base::BindOnce(
            &FirewallHoleDelegate::OnFirewallHoleOpened, GetWeakPtr(),
            std::move(connection_tracker),
            /*on_success=*/
            base::BindOnce(std::move(callback_a), net::OK, *local_addr),
            /*on_failure=*/
            base::BindOnce(std::move(callback_b),
                           net::ERR_NETWORK_ACCESS_DENIED, std::nullopt)));
  }

  void OpenUDPFirewallHole(
      mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
          connection_tracker,
      OpenBoundUDPSocketCallback callback,
      int32_t result,
      const std::optional<net::IPEndPoint>& local_addr) {
    if (result != net::OK) {
      FulfillWithError(std::move(callback), result);
      return;
    }
    if (!ShouldOpenFirewallHole(local_addr->address())) {
      std::move(callback).Run(net::OK, *local_addr);
      return;
    }
    auto [callback_a, callback_b] =
        base::SplitOnceCallback(std::move(callback));
    chromeos::FirewallHole::Open(
        chromeos::FirewallHole::PortType::kUdp, local_addr->port(),
        "" /*all interfaces*/,
        base::BindOnce(
            &FirewallHoleDelegate::OnFirewallHoleOpened, GetWeakPtr(),
            std::move(connection_tracker),
            /*on_success=*/
            base::BindOnce(std::move(callback_a), net::OK, *local_addr),
            /*on_failure=*/
            base::BindOnce(std::move(callback_b),
                           net::ERR_NETWORK_ACCESS_DENIED, std::nullopt)));
  }

  base::WeakPtr<FirewallHoleDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnFirewallHoleOpened(
      mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
          connection_tracker,
      base::OnceClosure on_success,
      base::OnceClosure on_failure,
      std::unique_ptr<chromeos::FirewallHole> firewall_hole) {
    if (!firewall_hole) {
      std::move(on_failure).Run();
      return;
    }
    receivers_.Add(this, std::move(connection_tracker),
                   std::move(firewall_hole));
    std::move(on_success).Run();
  }

  mojo::ReceiverSet<network::mojom::SocketConnectionTracker,
                    std::unique_ptr<chromeos::FirewallHole>>
      receivers_;
  base::WeakPtrFactory<FirewallHoleDelegate> weak_factory_{this};
};
#endif  // BUILDFLAG(IS_CHROMEOS)

DirectSocketsServiceImpl::DirectSocketsServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver)
    : DocumentService(*render_frame_host, std::move(receiver)),
      resolver_(network::SimpleHostResolver::Create(
          /*network_context_factory=*/base::BindRepeating(
              &DirectSocketsServiceImpl::GetNetworkContext,
              base::Unretained(this)))) {
#if BUILDFLAG(IS_CHROMEOS)
  firewall_hole_delegate_ = std::make_unique<FirewallHoleDelegate>();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

DirectSocketsServiceImpl::~DirectSocketsServiceImpl() = default;

// static
void DirectSocketsServiceImpl::CreateForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(blink::features::kDirectSockets)) {
    mojo::ReportBadMessage(
        "features::kDirectSockets is disabled by command line parameters or a "
        "Finch experiment.");
    return;
  }
  if (!render_frame_host->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kDirectSockets)) {
    mojo::ReportBadMessage(
        "Permissions policy blocks access to Direct Sockets.");
    return;
  }
  if (!HasIsolatedContextCapability(render_frame_host)) {
    mojo::ReportBadMessage(
        "Frame is not sufficiently isolated to use Direct Sockets.");
    return;
  }
  new DirectSocketsServiceImpl(render_frame_host, std::move(receiver));
}

void DirectSocketsServiceImpl::OpenTCPSocket(
    blink::mojom::DirectTCPSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    OpenTCPSocketCallback callback) {
  net::HostPortPair remote_addr = options->remote_addr;

  if (!IsAPIAccessAllowed(render_frame_host()) ||
      !ValidateAddressAndPort(render_frame_host(), remote_addr,
                              DirectSocketsDelegate::ProtocolType::kTcp)) {
    FulfillWithError(std::move(callback), net::ERR_ACCESS_DENIED);
    return;
  }

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  if (options->dns_query_type.has_value()) {
    parameters->dns_query_type = *options->dns_query_type;
  }

  // Unretained(this) is safe here because the callback will be owned by
  // |resolver_| which in turn is owned by |this|.
  resolver_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(std::move(remote_addr)),
      net::NetworkAnonymizationKey::CreateTransient(), std::move(parameters),
      base::BindOnce(&DirectSocketsServiceImpl::OnResolveCompleteForTCPSocket,
                     base::Unretained(this), std::move(options),
                     std::move(receiver), std::move(observer),
                     std::move(callback)));
}

void DirectSocketsServiceImpl::OpenConnectedUDPSocket(
    blink::mojom::DirectConnectedUDPSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::RestrictedUDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenConnectedUDPSocketCallback callback) {
  net::HostPortPair remote_addr = options->remote_addr;

  if (!IsAPIAccessAllowed(render_frame_host()) ||
      !ValidateAddressAndPort(
          render_frame_host(), remote_addr,
          DirectSocketsDelegate::ProtocolType::kConnectedUdp)) {
    FulfillWithError(std::move(callback), net::ERR_ACCESS_DENIED);
    return;
  }

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  if (options->dns_query_type.has_value()) {
    parameters->dns_query_type = *options->dns_query_type;
  }

  // Unretained(this) is safe here because the callback will be owned by
  // |resolver_| which in turn is owned by |this|.
  resolver_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(std::move(remote_addr)),
      net::NetworkAnonymizationKey::CreateTransient(), std::move(parameters),
      base::BindOnce(&DirectSocketsServiceImpl::OnResolveCompleteForUDPSocket,
                     base::Unretained(this), std::move(options),
                     std::move(receiver), std::move(listener),
                     std::move(callback)));
}

void DirectSocketsServiceImpl::OpenBoundUDPSocket(
    blink::mojom::DirectBoundUDPSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::RestrictedUDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenBoundUDPSocketCallback callback) {
  if (!IsAPIAccessAllowed(render_frame_host()) ||
      !ValidateAddressAndPort(render_frame_host(), options->local_addr,
                              DirectSocketsDelegate::ProtocolType::kBoundUdp)) {
    FulfillWithError(std::move(callback), net::ERR_ACCESS_DENIED);
    return;
  }

  auto socket_options = network::mojom::UDPSocketOptions::New();
  if (options->ipv6_only.has_value()) {
    socket_options->ipv6_only = *options->ipv6_only
                                    ? network::mojom::OptionalBool::kTrue
                                    : network::mojom::OptionalBool::kFalse;
  }
  if (options->send_buffer_size.has_value()) {
    socket_options->send_buffer_size = *options->send_buffer_size;
  }
  if (options->receive_buffer_size.has_value()) {
    socket_options->receive_buffer_size = *options->receive_buffer_size;
  }

  auto params = network::mojom::RestrictedUDPSocketParams::New();
  params->socket_options = std::move(socket_options);

#if BUILDFLAG(IS_CHROMEOS)
  mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
      connection_tracker;
  params->connection_tracker =
      connection_tracker.InitWithNewPipeAndPassRemote();
#endif  // BUILDFLAG(IS_CHROMEOS)

  RequestPrivateNetworkAccessAndCreateSocket(
      render_frame_host(),
      /*create_socket_callback=*/
      base::BindOnce(&DirectSocketsServiceImpl::CreateRestrictedUDPSocketImpl,
                     weak_factory_.GetWeakPtr(), options->local_addr,
                     network::mojom::RestrictedUDPSocketMode::BOUND,
                     std::move(params), std::move(receiver),
                     std::move(listener)),
  /*finish_callback=*/
#if !BUILDFLAG(IS_CHROMEOS)
      std::move(callback)
#else   // BUILDFLAG(IS_CHROMEOS)
      // On ChromeOS the original callback will be invoked after punching a
      // firewall hole.
      base::BindOnce(&FirewallHoleDelegate::OpenUDPFirewallHole,
                     firewall_hole_delegate_->GetWeakPtr(),
                     std::move(connection_tracker), std::move(callback))
#endif  // BUILDFLAG(IS_CHROMEOS)
  );
}

void DirectSocketsServiceImpl::OpenTCPServerSocket(
    blink::mojom::DirectTCPServerSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::TCPServerSocket> socket,
    OpenTCPServerSocketCallback callback) {
  if (!IsAPIAccessAllowed(render_frame_host()) ||
      !ValidateAddressAndPort(
          render_frame_host(), options->local_addr,
          DirectSocketsDelegate::ProtocolType::kTcpServer)) {
    FulfillWithError(std::move(callback), net::ERR_ACCESS_DENIED);
    return;
  }

  auto server_options = network::mojom::TCPServerSocketOptions::New();

  if (options->ipv6_only.has_value()) {
    server_options->ipv6_only = *options->ipv6_only
                                    ? network::mojom::OptionalBool::kTrue
                                    : network::mojom::OptionalBool::kFalse;
  }
  // Substitute |options->backlog| with SOMAXCONN if not specified.
  server_options->backlog =
      std::min<uint32_t>(SOMAXCONN, options->backlog.value_or(SOMAXCONN));

#if BUILDFLAG(IS_CHROMEOS)
  mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
      connection_tracker;
  server_options->connection_tracker =
      connection_tracker.InitWithNewPipeAndPassRemote();
#endif  // BUILDFLAG(IS_CHROMEOS)

  GetNetworkContext()->CreateTCPServerSocket(
      options->local_addr, std::move(server_options),
      net::MutableNetworkTrafficAnnotationTag(kDirectSocketsTrafficAnnotation),
      std::move(socket),
#if !BUILDFLAG(IS_CHROMEOS)
      std::move(callback)
#else   // BUILDFLAG(IS_CHROMEOS)
      // On ChromeOS the original callback will be invoked after punching a
      // firewall hole.
      base::BindOnce(&FirewallHoleDelegate::OpenTCPFirewallHole,
                     firewall_hole_delegate_->GetWeakPtr(),
                     std::move(connection_tracker), std::move(callback))
#endif  // BUILDFLAG(IS_CHROMEOS)
  );
}

// static
void DirectSocketsServiceImpl::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  GetNetworkContextForTesting() = network_context;
}

#if BUILDFLAG(IS_CHROMEOS)
// static
void DirectSocketsServiceImpl::SetAlwaysOpenFirewallHoleForTesting() {
  g_always_open_firewall_hole_for_testing = true;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

network::mojom::NetworkContext* DirectSocketsServiceImpl::GetNetworkContext()
    const {
  if (auto* network_context = GetNetworkContextForTesting()) {
    return network_context;
  }
  return render_frame_host().GetStoragePartition()->GetNetworkContext();
}

void DirectSocketsServiceImpl::OnResolveCompleteForTCPSocket(
    blink::mojom::DirectTCPSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    OpenTCPSocketCallback callback,
    int result,
    const net::ResolveErrorInfo&,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&) {
  if (result != net::OK) {
    FulfillWithError(std::move(callback), result);
    return;
  }

  DCHECK(resolved_addresses && !resolved_addresses->empty());

  auto socket_options = network::mojom::TCPConnectedSocketOptions::New();
  if (options->send_buffer_size.has_value()) {
    socket_options->send_buffer_size = *options->send_buffer_size;
  }
  if (options->receive_buffer_size.has_value()) {
    socket_options->receive_buffer_size = *options->receive_buffer_size;
  }
  socket_options->no_delay = options->no_delay;
  if (options->keep_alive_options) {
    // options->keep_alive_options will be invalidated.
    socket_options->keep_alive_options = std::move(options->keep_alive_options);
  }

  if (!RequiresPrivateNetworkAccess(*resolved_addresses)) {
    CreateTCPConnectedSocketImpl(*resolved_addresses, std::move(socket_options),
                                 std::move(socket), std::move(observer),
                                 std::move(callback));
    return;
  }

  RequestPrivateNetworkAccessAndCreateSocket(
      render_frame_host(),
      /*create_socket_callback=*/
      base::BindOnce(&DirectSocketsServiceImpl::CreateTCPConnectedSocketImpl,
                     weak_factory_.GetWeakPtr(), *resolved_addresses,
                     std::move(socket_options), std::move(socket),
                     std::move(observer)),
      /*finish_callback=*/std::move(callback));
}

void DirectSocketsServiceImpl::CreateTCPConnectedSocketImpl(
    const net::AddressList& resolved_addresses,
    network::mojom::TCPConnectedSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    OpenTCPSocketCallback callback) {
  GetNetworkContext()->CreateTCPConnectedSocket(
      /*local_addr=*/std::nullopt,
      /*remote_addr_list=*/resolved_addresses, std::move(options),
      net::MutableNetworkTrafficAnnotationTag(kDirectSocketsTrafficAnnotation),
      std::move(socket), std::move(observer), std::move(callback));
}

void DirectSocketsServiceImpl::OnResolveCompleteForUDPSocket(
    blink::mojom::DirectConnectedUDPSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::RestrictedUDPSocket>
        restricted_udp_socket_receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenConnectedUDPSocketCallback callback,
    int result,
    const net::ResolveErrorInfo&,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&) {
  if (result != net::OK) {
    FulfillWithError(std::move(callback), result);
    return;
  }

  DCHECK(resolved_addresses && !resolved_addresses->empty());

  auto socket_options = network::mojom::UDPSocketOptions::New();
  if (options->send_buffer_size.has_value()) {
    socket_options->send_buffer_size = *options->send_buffer_size;
  }
  if (options->receive_buffer_size.has_value()) {
    socket_options->receive_buffer_size = *options->receive_buffer_size;
  }

  auto params = network::mojom::RestrictedUDPSocketParams::New();
  params->socket_options = std::move(socket_options);

  const auto& peer_addr = resolved_addresses->front();
  auto finish_callback = base::BindOnce(
      [](OpenConnectedUDPSocketCallback callback, net::IPEndPoint peer_addr,
         int result, const std::optional<net::IPEndPoint>& local_addr) {
        std::move(callback).Run(result, local_addr, peer_addr);
      },
      std::move(callback), peer_addr);

  if (!RequiresPrivateNetworkAccess(*resolved_addresses)) {
    CreateRestrictedUDPSocketImpl(
        resolved_addresses->front(),
        network::mojom::RestrictedUDPSocketMode::CONNECTED, std::move(params),
        std::move(restricted_udp_socket_receiver), std::move(listener),
        std::move(finish_callback));
    return;
  }

  RequestPrivateNetworkAccessAndCreateSocket(
      render_frame_host(),
      /*create_socket_callback=*/
      base::BindOnce(
          &DirectSocketsServiceImpl::CreateRestrictedUDPSocketImpl,
          weak_factory_.GetWeakPtr(), peer_addr,
          network::mojom::RestrictedUDPSocketMode::CONNECTED, std::move(params),
          std::move(restricted_udp_socket_receiver), std::move(listener)),
      /*finish_callback=*/std::move(finish_callback));
}

void DirectSocketsServiceImpl::CreateRestrictedUDPSocketImpl(
    const net::IPEndPoint& peer_addr,
    network::mojom::RestrictedUDPSocketMode mode,
    network::mojom::RestrictedUDPSocketParamsPtr options,
    mojo::PendingReceiver<network::mojom::RestrictedUDPSocket> socket,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    base::OnceCallback<void(int32_t, const std::optional<net::IPEndPoint>&)>
        callback) {
  GetNetworkContext()->CreateRestrictedUDPSocket(
      peer_addr, mode,
      /*traffic_annotation=*/
      net::MutableNetworkTrafficAnnotationTag(kDirectSocketsTrafficAnnotation),
      std::move(options), std::move(socket), std::move(listener),
      std::move(callback));
}

}  // namespace content
