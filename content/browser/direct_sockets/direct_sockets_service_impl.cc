// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_sockets_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/address_list.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

namespace {

constexpr int32_t kMaxBufferSize = 32 * 1024 * 1024;

DirectSocketsServiceImpl::PermissionCallback&
GetPermissionCallbackForTesting() {
  static base::NoDestructor<DirectSocketsServiceImpl::PermissionCallback>
      callback;
  return *callback;
}

network::mojom::NetworkContext*& GetNetworkContextForTesting() {
  static network::mojom::NetworkContext* network_context = nullptr;
  return network_context;
}

}  // namespace

DirectSocketsServiceImpl::DirectSocketsServiceImpl(RenderFrameHost& frame_host)
    : WebContentsObserver(WebContents::FromRenderFrameHost(&frame_host)),
      frame_host_(&frame_host) {}

// static
void DirectSocketsServiceImpl::CreateForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DirectSocketsServiceImpl>(*render_frame_host),
      std::move(receiver));
}

void DirectSocketsServiceImpl::OpenTcpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    OpenTcpSocketCallback callback) {
  if (!options) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }
  net::IPAddress remote_address;
  const net::Error result = ValidateOptions(*options, remote_address);

  // TODO(crbug.com/1119681): Collect metrics for usage and permission checks

  if (result != net::OK) {
    std::move(callback).Run(result, base::nullopt, base::nullopt,
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  // TODO(crbug.com/905818): Populate local_addr from options.
  const base::Optional<net::IPEndPoint> local_addr = base::nullopt;

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

  GetNetworkContext()->CreateTCPConnectedSocket(
      local_addr,
      net::AddressList::CreateFromIPAddress(remote_address,
                                            options->remote_port),
      std::move(tcp_connected_socket_options), traffic_annotation,
      std::move(receiver), std::move(observer), std::move(callback));
}

void DirectSocketsServiceImpl::OpenUdpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenUdpSocketCallback callback) {
  if (!options) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }
  net::IPAddress remote_address;
  const net::Error result = ValidateOptions(*options, remote_address);

  // TODO(crbug.com/1119681): Collect metrics for usage and permission checks

  if (result == net::OK) {
    // TODO(crbug.com/1119620): GetNetworkContext()->CreateUDPSocket
    // TODO(crbug.com/1119620): Connect(remote_addr, udp_socket_options)
    GetNetworkContext();
    NOTIMPLEMENTED();
  }

  std::move(callback).Run(result, base::nullopt, base::nullopt);
}

// static
void DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
    PermissionCallback callback) {
  GetPermissionCallbackForTesting() = std::move(callback);
}

// static
void DirectSocketsServiceImpl::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  GetNetworkContextForTesting() = network_context;
}

void DirectSocketsServiceImpl::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host == frame_host_)
    frame_host_ = nullptr;
}

void DirectSocketsServiceImpl::WebContentsDestroyed() {
  frame_host_ = nullptr;
}

net::Error DirectSocketsServiceImpl::ValidateOptions(
    const blink::mojom::DirectSocketOptions& options,
    net::IPAddress& remote_address) {
  DCHECK(base::FeatureList::IsEnabled(features::kDirectSockets));

  if (!frame_host_)
    return net::ERR_CONTEXT_SHUT_DOWN;

  if (GetPermissionCallbackForTesting())
    return GetPermissionCallbackForTesting().Run(options, remote_address);

  if (options.send_buffer_size < 0 || options.receive_buffer_size < 0)
    return net::ERR_INVALID_ARGUMENT;

  // TODO(crbug.com/1119662): Check for enterprise software policies.
  // TODO(crbug.com/1119659): Check permissions policy.
  // TODO(crbug.com/1119600): Implement rate limiting.

  if (options.remote_port == 443) {
    // TODO(crbug.com/1119601): Issue a CORS preflight request.
    return net::ERR_UNSAFE_PORT;
  }

  // ValidateOptions() will need to become asynchronous:
  // TODO(crbug.com/1119597): Show connection dialog.
  // TODO(crbug.com/1119597): Use the hostname provided by the user.
  // TODO(crbug.com/1119661): Reject hostnames that resolve to non-public
  // addresses.
  if (!options.remote_hostname)
    return net::ERR_NAME_NOT_RESOLVED;

  // TODO(crbug.com/905818): Support resolved hostnames.
  // TODO(crbug.com/1124255): Support mDNS.
  if (!remote_address.AssignFromIPLiteral(*options.remote_hostname))
    return net::ERR_NAME_NOT_RESOLVED;

  return net::ERR_NOT_IMPLEMENTED;
}

network::mojom::NetworkContext* DirectSocketsServiceImpl::GetNetworkContext() {
  if (network::mojom::NetworkContext* network_context =
          GetNetworkContextForTesting()) {
    return network_context;
  }

  return frame_host_->GetStoragePartition()->GetNetworkContext();
}

}  // namespace content
