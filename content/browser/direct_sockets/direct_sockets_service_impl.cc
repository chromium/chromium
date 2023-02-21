// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_sockets_service_impl.h"

#include "build/build_config.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/isolated_context_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/direct_sockets_delegate.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_anonymization_key.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_host_resolver.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/restricted_udp_socket.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX)
#include <sys/socket.h>
#endif  // BUILDFLAG(IS_POSIX)

namespace content {

namespace {

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

constexpr int32_t kMaxBufferSize = 32 * 1024 * 1024;

network::mojom::NetworkContext*& GetNetworkContextForTesting() {
  static network::mojom::NetworkContext* network_context = nullptr;
  return network_context;
}

network::mojom::TCPConnectedSocketOptionsPtr CreateTCPConnectedSocketOptions(
    blink::mojom::DirectTCPSocketOptionsPtr options) {
  network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options =
      network::mojom::TCPConnectedSocketOptions::New();
  if (options->send_buffer_size > 0) {
    tcp_connected_socket_options->send_buffer_size =
        std::min(options->send_buffer_size, kMaxBufferSize);
  }
  if (options->receive_buffer_size > 0) {
    tcp_connected_socket_options->receive_buffer_size =
        std::min(options->receive_buffer_size, kMaxBufferSize);
  }
  tcp_connected_socket_options->no_delay = options->no_delay;
  if (options->keep_alive_options) {
    // options->keep_alive_options will be invalidated.
    tcp_connected_socket_options->keep_alive_options =
        std::move(options->keep_alive_options);
  }
  return tcp_connected_socket_options;
}

network::mojom::UDPSocketOptionsPtr CreateUDPSocketOptions(
    blink::mojom::DirectUDPSocketOptionsPtr options) {
  network::mojom::UDPSocketOptionsPtr udp_socket_options =
      network::mojom::UDPSocketOptions::New();
  if (options->send_buffer_size > 0) {
    udp_socket_options->send_buffer_size =
        std::min(options->send_buffer_size, kMaxBufferSize);
  }
  if (options->receive_buffer_size > 0) {
    udp_socket_options->receive_buffer_size =
        std::min(options->receive_buffer_size, kMaxBufferSize);
  }
  return udp_socket_options;
}

content::DirectSocketsDelegate* GetDelegate() {
  return GetContentClient()->browser()->GetDirectSocketsDelegate();
}

#if BUILDFLAG(ENABLE_MDNS)
bool ResemblesMulticastDNSName(base::StringPiece hostname) {
  return base::EndsWith(hostname, ".local") ||
         base::EndsWith(hostname, ".local.");
}
#endif  // !BUILDFLAG(ENABLE_MDNS)

}  // namespace

DirectSocketsServiceImpl::DirectSocketsServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver)
    : DocumentService(*render_frame_host, std::move(receiver)),
      resolver_(network::SimpleHostResolver::Create(GetNetworkContext())) {}

DirectSocketsServiceImpl::~DirectSocketsServiceImpl() = default;

// static
void DirectSocketsServiceImpl::CreateForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!render_frame_host->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kDirectSockets)) {
    mojo::ReportBadMessage(
        "Permissions policy blocks access to Direct Sockets.");
  }
  if (!IsFrameSufficientlyIsolated(render_frame_host)) {
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

  if (auto* delegate = GetDelegate();
      delegate &&
      !delegate->ValidateAddressAndPort(
          render_frame_host().GetBrowserContext(),
          render_frame_host().GetProcess()->GetProcessLock().lock_url(),
          remote_addr.host(), remote_addr.port(),
          blink::mojom::DirectSocketProtocolType::kTcp)) {
    std::move(callback).Run(net::ERR_ACCESS_DENIED, absl::nullopt,
                            absl::nullopt, mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = options->dns_query_type;
#if BUILDFLAG(ENABLE_MDNS)
  if (ResemblesMulticastDNSName(remote_addr.host())) {
    parameters->source = net::HostResolverSource::MULTICAST_DNS;
  }
#endif  // !BUILDFLAG(ENABLE_MDNS)

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

void DirectSocketsServiceImpl::OpenUDPSocket(
    blink::mojom::DirectUDPSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::RestrictedUDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenUDPSocketCallback callback) {
  // Ensure that only one of |remote_addr| and |local_addr| is supplied.
  if ((options->remote_addr && options->local_addr) ||
      (!options->remote_addr && !options->local_addr)) {
    std::move(callback).Run(net::ERR_INVALID_ARGUMENT, absl::nullopt,
                            absl::nullopt);
    return;
  }

  auto* browser_context = render_frame_host().GetBrowserContext();
  auto lock_url = render_frame_host().GetProcess()->GetProcessLock().lock_url();

  if (options->remote_addr) {
    // Handle CONNECTED mode request.
    net::HostPortPair remote_addr = *options->remote_addr;

    if (auto* delegate = GetDelegate();
        delegate &&
        !delegate->ValidateAddressAndPort(
            browser_context, lock_url, remote_addr.host(), remote_addr.port(),
            blink::mojom::DirectSocketProtocolType::kUdp)) {
      std::move(callback).Run(net::ERR_ACCESS_DENIED, absl::nullopt,
                              absl::nullopt);
      return;
    }

    network::mojom::ResolveHostParametersPtr parameters =
        network::mojom::ResolveHostParameters::New();
    parameters->dns_query_type = options->dns_query_type;
#if BUILDFLAG(ENABLE_MDNS)
    if (ResemblesMulticastDNSName(remote_addr.host())) {
      parameters->source = net::HostResolverSource::MULTICAST_DNS;
    }
#endif  // !BUILDFLAG(ENABLE_MDNS)

    // Unretained(this) is safe here because the callback will be owned by
    // |resolver_| which in turn is owned by |this|.
    resolver_->ResolveHost(
        network::mojom::HostResolverHost::NewHostPortPair(
            std::move(remote_addr)),
        net::NetworkAnonymizationKey::CreateTransient(), std::move(parameters),
        base::BindOnce(&DirectSocketsServiceImpl::OnResolveCompleteForUDPSocket,
                       base::Unretained(this), std::move(options),
                       std::move(receiver), std::move(listener),
                       std::move(callback)));
  } else {
    // Handle BOUND mode request.
    DCHECK(options->local_addr);
    net::IPEndPoint local_addr = *options->local_addr;

    if (auto* delegate = GetDelegate();
        delegate && !delegate->ValidateAddressAndPort(
                        browser_context, lock_url,
                        local_addr.ToStringWithoutPort(), local_addr.port(),
                        blink::mojom::DirectSocketProtocolType::kUdpServer)) {
      std::move(callback).Run(net::ERR_ACCESS_DENIED, absl::nullopt,
                              absl::nullopt);
      return;
    }

    GetNetworkContext()->CreateRestrictedUDPSocket(
        std::move(local_addr),
        /*mode=*/network::mojom::RestrictedUDPSocketMode::BOUND,
        /*traffic_annotation=*/
        net::MutableNetworkTrafficAnnotationTag(
            kDirectSocketsTrafficAnnotation),
        /*options=*/CreateUDPSocketOptions(std::move(options)),
        std::move(receiver), std::move(listener),
        base::BindOnce(
            [](OpenUDPSocketCallback callback, int result,
               const absl::optional<net::IPEndPoint>& local_addr) {
              std::move(callback).Run(result, local_addr, /*peer_addr=*/{});
            },
            std::move(callback)));
  }
}

void DirectSocketsServiceImpl::OpenTCPServerSocket(
    blink::mojom::DirectTCPServerSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::TCPServerSocket> socket,
    OpenTCPServerSocketCallback callback) {
  // Default if |options->backlog| is 0.
  uint32_t backlog = SOMAXCONN;
  if (options->backlog > 0) {
    // Truncate the provided value if it is larger than allowed by the platform.
    backlog = std::min<uint32_t>(options->backlog, SOMAXCONN);
  }
  GetNetworkContext()->CreateTCPServerSocket(
      options->local_addr, backlog,
      net::MutableNetworkTrafficAnnotationTag(kDirectSocketsTrafficAnnotation),
      std::move(socket), std::move(callback));
}

// static
void DirectSocketsServiceImpl::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  GetNetworkContextForTesting() = network_context;
}

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
    const absl::optional<net::AddressList>& resolved_addresses,
    const absl::optional<net::HostResolverEndpointResults>&) {
  if (result != net::OK) {
    std::move(callback).Run(result, absl::nullopt, absl::nullopt,
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  DCHECK(resolved_addresses && !resolved_addresses->empty());

  absl::optional<net::IPEndPoint> local_addr = options->local_addr;
  GetNetworkContext()->CreateTCPConnectedSocket(
      /*local_addr=*/std::move(local_addr),
      /*remote_addr_list=*/*resolved_addresses,
      CreateTCPConnectedSocketOptions(std::move(options)),
      net::MutableNetworkTrafficAnnotationTag(kDirectSocketsTrafficAnnotation),
      std::move(socket), std::move(observer), std::move(callback));
}

void DirectSocketsServiceImpl::OnResolveCompleteForUDPSocket(
    blink::mojom::DirectUDPSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::RestrictedUDPSocket>
        restricted_udp_socket_receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenUDPSocketCallback callback,
    int result,
    const net::ResolveErrorInfo&,
    const absl::optional<net::AddressList>& resolved_addresses,
    const absl::optional<net::HostResolverEndpointResults>&) {
  if (result != net::OK) {
    std::move(callback).Run(result, absl::nullopt, absl::nullopt);
    return;
  }

  DCHECK(resolved_addresses && !resolved_addresses->empty());

  const net::IPEndPoint& peer_addr = resolved_addresses->front();
  GetNetworkContext()->CreateRestrictedUDPSocket(
      peer_addr,
      /*mode=*/network::mojom::RestrictedUDPSocketMode::CONNECTED,
      /*traffic_annotation=*/
      net::MutableNetworkTrafficAnnotationTag(kDirectSocketsTrafficAnnotation),
      /*options=*/CreateUDPSocketOptions(std::move(options)),
      std::move(restricted_udp_socket_receiver), std::move(listener),
      base::BindOnce(
          [](OpenUDPSocketCallback callback, net::IPEndPoint peer_addr,
             int result, const absl::optional<net::IPEndPoint>& local_addr) {
            std::move(callback).Run(result, local_addr, peer_addr);
          },
          std::move(callback), peer_addr));
}

}  // namespace content
