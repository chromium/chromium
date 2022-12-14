// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_sockets_service_impl.h"

#include "build/build_config.h"
#include "content/browser/direct_sockets/direct_udp_socket_impl.h"
#include "content/browser/direct_sockets/resolve_host_and_open_socket.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/isolated_context_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/direct_sockets_delegate.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-shared.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

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
    blink::mojom::DirectSocketOptionsPtr options) {
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
    blink::mojom::DirectSocketOptionsPtr options) {
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

absl::optional<net::IPEndPoint> GetLocalAddress(
    const blink::mojom::DirectSocketOptions& options) {
  if (net::IPAddress address;
      options.local_hostname &&
      address.AssignFromIPLiteral(*options.local_hostname)) {
    return net::IPEndPoint{std::move(address), options.local_port};
  }
  return {};
}

}  // namespace

DirectSocketsServiceImpl::DirectSocketsServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver)
    : DocumentService(*render_frame_host, std::move(receiver)) {}

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

content::DirectSocketsDelegate* DirectSocketsServiceImpl::GetDelegate() {
  return GetContentClient()->browser()->GetDirectSocketsDelegate();
}

void DirectSocketsServiceImpl::OpenTcpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    OpenTcpSocketCallback callback) {
  const std::string remote_host = options->remote_hostname;
  const uint16_t remote_port = options->remote_port;

  if (auto* delegate = GetDelegate();
      delegate &&
      !delegate->ValidateAddressAndPort(
          render_frame_host().GetBrowserContext(),
          render_frame_host().GetProcess()->GetProcessLock().lock_url(),
          remote_host, remote_port,
          blink::mojom::DirectSocketProtocolType::kTcp)) {
    std::move(callback).Run(net::ERR_ACCESS_DENIED, absl::nullopt,
                            absl::nullopt, mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  ResolveHostAndOpenSocket::Create(
      remote_host, remote_port,
      base::BindOnce(&DirectSocketsServiceImpl::OnResolveCompleteForTcpSocket,
                     weak_ptr_factory_.GetWeakPtr(), std::move(options),
                     std::move(receiver), std::move(observer),
                     std::move(callback)))
      ->Start(GetNetworkContext());
}

void DirectSocketsServiceImpl::OpenUdpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::DirectUDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenUdpSocketCallback callback) {
  const std::string remote_host = options->remote_hostname;
  const uint16_t remote_port = options->remote_port;

  if (auto* delegate = GetDelegate();
      delegate &&
      !delegate->ValidateAddressAndPort(
          render_frame_host().GetBrowserContext(),
          render_frame_host().GetProcess()->GetProcessLock().lock_url(),
          remote_host, remote_port,
          blink::mojom::DirectSocketProtocolType::kUdp)) {
    std::move(callback).Run(net::ERR_ACCESS_DENIED, absl::nullopt,
                            absl::nullopt);
    return;
  }

  ResolveHostAndOpenSocket::Create(
      remote_host, remote_port,
      base::BindOnce(&DirectSocketsServiceImpl::OnResolveCompleteForUdpSocket,
                     weak_ptr_factory_.GetWeakPtr(), std::move(options),
                     std::move(receiver), std::move(listener),
                     std::move(callback)))
      ->Start(GetNetworkContext());
}

// static
net::NetworkTrafficAnnotationTag DirectSocketsServiceImpl::TrafficAnnotation() {
  return kDirectSocketsTrafficAnnotation;
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

void DirectSocketsServiceImpl::OnResolveCompleteForTcpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    OpenTcpSocketCallback callback,
    int result,
    const absl::optional<net::AddressList>& resolved_addresses) {
  if (result != net::OK) {
    std::move(callback).Run(result, absl::nullopt, absl::nullopt,
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  DCHECK(resolved_addresses && !resolved_addresses->empty());
  absl::optional<net::IPEndPoint> local_addr = GetLocalAddress(*options);

  GetNetworkContext()->CreateTCPConnectedSocket(
      std::move(local_addr), *resolved_addresses,
      CreateTCPConnectedSocketOptions(std::move(options)),
      net::MutableNetworkTrafficAnnotationTag{
          DirectSocketsServiceImpl::TrafficAnnotation()},
      std::move(socket), std::move(observer), std::move(callback));
}

void DirectSocketsServiceImpl::OnResolveCompleteForUdpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::DirectUDPSocket>
        direct_udp_socket_receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenUdpSocketCallback callback,
    int result,
    const absl::optional<net::AddressList>& resolved_addresses) {
  if (result != net::OK) {
    std::move(callback).Run(result, absl::nullopt, absl::nullopt);
    return;
  }

  DCHECK(resolved_addresses && !resolved_addresses->empty());

  net::IPEndPoint peer_addr = resolved_addresses->front();
  auto direct_udp_socket = std::make_unique<DirectUDPSocketImpl>(
      GetNetworkContext(), std::move(listener));

  direct_udp_socket->Connect(
      resolved_addresses->front(), CreateUDPSocketOptions(std::move(options)),
      base::BindOnce(
          [](OpenUdpSocketCallback callback, net::IPEndPoint peer_addr,
             int result, const absl::optional<net::IPEndPoint>& local_addr) {
            std::move(callback).Run(result, local_addr, peer_addr);
          },
          std::move(callback), resolved_addresses->front()));

  direct_udp_socket_receivers_.Add(std::move(direct_udp_socket),
                                   std::move(direct_udp_socket_receiver));
}

}  // namespace content
