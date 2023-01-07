// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_sockets_service_impl.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/direct_sockets/direct_udp_socket_impl.h"
#include "content/browser/direct_sockets/resolve_host_and_open_socket.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/direct_sockets_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-shared.h"
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

bool IsFrameSufficientlyIsolated(content::RenderFrameHost* frame) {
  return frame->GetWebExposedIsolationLevel() >=
             content::RenderFrameHost::WebExposedIsolationLevel::
                 kMaybeIsolatedApplication &&
         frame->IsFeatureEnabled(
             blink::mojom::PermissionsPolicyFeature::kDirectSockets);
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

DirectSocketsServiceImpl::DirectSocketsServiceImpl(RenderFrameHost* frame_host)
    : WebContentsObserver(WebContents::FromRenderFrameHost(frame_host)),
      frame_host_(frame_host) {}

DirectSocketsServiceImpl::~DirectSocketsServiceImpl() = default;

// static
void DirectSocketsServiceImpl::CreateForFrame(
    RenderFrameHost* frame,
    mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsFrameSufficientlyIsolated(frame)) {
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new DirectSocketsServiceImpl(frame)),
      std::move(receiver));
}

content::DirectSocketsDelegate* DirectSocketsServiceImpl::GetDelegate() {
  return GetContentClient()->browser()->GetDirectSocketsDelegate();
}

void DirectSocketsServiceImpl::OpenTcpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    OpenTcpSocketCallback callback) {
  if (!IsFrameSufficientlyIsolated(frame_host_)) {
    mojo::ReportBadMessage("Insufficient isolation to open socket");
    return;
  }

  if (!GetNetworkContext()) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }

  if (auto* delegate = GetDelegate();
      delegate &&
      !delegate->ValidateAddressAndPort(
          frame_host_, options->remote_hostname, options->remote_port,
          blink::mojom::DirectSocketProtocolType::kTcp)) {
    std::move(callback).Run(net::ERR_ACCESS_DENIED, absl::nullopt,
                            absl::nullopt, mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  const std::string remote_host = options->remote_hostname;
  const uint16_t remote_port = options->remote_port;

  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  ResolveHostAndOpenSocket::Create(
      weak_ptr, remote_host, remote_port,
      base::BindOnce(&DirectSocketsServiceImpl::OnResolveCompleteForTcpSocket,
                     weak_ptr, std::move(options), std::move(receiver),
                     std::move(observer), std::move(callback)))
      ->Start();
}

void DirectSocketsServiceImpl::OpenUdpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::DirectUDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenUdpSocketCallback callback) {
  if (!IsFrameSufficientlyIsolated(frame_host_)) {
    mojo::ReportBadMessage("Insufficient isolation to open socket");
    return;
  }

  if (!GetNetworkContext()) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }

  if (auto* delegate = GetDelegate();
      delegate &&
      !delegate->ValidateAddressAndPort(
          frame_host_, options->remote_hostname, options->remote_port,
          blink::mojom::DirectSocketProtocolType::kUdp)) {
    std::move(callback).Run(net::ERR_ACCESS_DENIED, absl::nullopt,
                            absl::nullopt);
    return;
  }

  const std::string remote_host = options->remote_hostname;
  const uint16_t remote_port = options->remote_port;

  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  ResolveHostAndOpenSocket::Create(
      weak_ptr, remote_host, remote_port,
      base::BindOnce(&DirectSocketsServiceImpl::OnResolveCompleteForUdpSocket,
                     weak_ptr, std::move(options), std::move(receiver),
                     std::move(listener), std::move(callback)))
      ->Start();
}

// static
net::MutableNetworkTrafficAnnotationTag
DirectSocketsServiceImpl::MutableTrafficAnnotation() {
  return net::MutableNetworkTrafficAnnotationTag{TrafficAnnotation()};
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

void DirectSocketsServiceImpl::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host == frame_host_) {
    frame_host_ = nullptr;
  }
}

void DirectSocketsServiceImpl::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  if (old_host == frame_host_) {
    frame_host_ = nullptr;
  }
}

void DirectSocketsServiceImpl::WebContentsDestroyed() {
  frame_host_ = nullptr;
}

network::mojom::NetworkContext* DirectSocketsServiceImpl::GetNetworkContext()
    const {
  if (GetNetworkContextForTesting())
    return GetNetworkContextForTesting();

  if (!frame_host_)
    return nullptr;

  return frame_host_->GetStoragePartition()->GetNetworkContext();
}

RenderFrameHost* DirectSocketsServiceImpl::GetFrameHost() const {
  return frame_host_;
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

  auto* network_context = GetNetworkContext();
  if (!network_context) {
    return;
  }

  DCHECK(resolved_addresses && !resolved_addresses->empty());
  absl::optional<net::IPEndPoint> local_addr = GetLocalAddress(*options);

  network_context->CreateTCPConnectedSocket(
      std::move(local_addr), *resolved_addresses,
      CreateTCPConnectedSocketOptions(std::move(options)),
      DirectSocketsServiceImpl::MutableTrafficAnnotation(), std::move(socket),
      std::move(observer), std::move(callback));
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

  auto* network_context = GetNetworkContext();
  if (!network_context) {
    return;
  }

  DCHECK(resolved_addresses && !resolved_addresses->empty());

  net::IPEndPoint peer_addr = resolved_addresses->front();
  auto direct_udp_socket = std::make_unique<DirectUDPSocketImpl>(
      network_context, std::move(listener));

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