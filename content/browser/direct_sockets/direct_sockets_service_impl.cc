// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_sockets_service_impl.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/direct_sockets/direct_udp_socket_impl.h"
#include "content/browser/direct_sockets/resolve_host_and_open_socket.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_request_headers.h"
#include "net/net_buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/url_constants.h"

using blink::mojom::DirectSocketFailureType;

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

DirectSocketsServiceImpl::~DirectSocketsServiceImpl() = default;

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
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    OpenTcpSocketCallback callback) {
  if (!frame_host_ || frame_host_->GetWebExposedIsolationLevel() <
                          RenderFrameHost::WebExposedIsolationLevel::
                              kMaybeIsolatedApplication) {
    mojo::ReportBadMessage("Insufficient isolation to open socket.");
    return;
  }

  if (const net::Error result = ValidateOptions(*options); result != net::OK) {
    std::move(callback).Run(result, absl::nullopt, absl::nullopt,
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  if (!GetNetworkContext()) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }

  ResolveHostAndOpenSocket* resolver = new ResolveHostAndOpenTCPSocket(
      weak_ptr_factory_.GetWeakPtr(), std::move(options), std::move(receiver),
      std::move(observer), std::move(callback));
  resolver->Start();
}

void DirectSocketsServiceImpl::OpenUdpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::DirectUDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
    OpenUdpSocketCallback callback) {
  if (!frame_host_ || frame_host_->GetWebExposedIsolationLevel() <
                          RenderFrameHost::WebExposedIsolationLevel::
                              kMaybeIsolatedApplication) {
    mojo::ReportBadMessage("Insufficient isolation to open socket.");
    return;
  }

  if (const net::Error result = ValidateOptions(*options); result != net::OK) {
    std::move(callback).Run(result, absl::nullopt, absl::nullopt);
    return;
  }

  network::mojom::NetworkContext* const network_context = GetNetworkContext();
  if (!network_context) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }

  ResolveHostAndOpenSocket* resolver = new ResolveHostAndOpenUDPSocket(
      weak_ptr_factory_.GetWeakPtr(), std::move(options), std::move(receiver),
      std::move(listener), std::move(callback));
  resolver->Start();
}

// static
net::MutableNetworkTrafficAnnotationTag
DirectSocketsServiceImpl::MutableTrafficAnnotation() {
  return net::MutableNetworkTrafficAnnotationTag(
      kDirectSocketsTrafficAnnotation);
}

// static
net::NetworkTrafficAnnotationTag DirectSocketsServiceImpl::TrafficAnnotation() {
  return kDirectSocketsTrafficAnnotation;
}

// static
int32_t DirectSocketsServiceImpl::GetMaxBufferSize() {
  return kMaxBufferSize;
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

// static
absl::optional<net::IPEndPoint>
DirectSocketsServiceImpl::GetLocalAddrForTesting(
    const blink::mojom::DirectSocketOptions& options) {
  if (!options.local_hostname) {
    return {};
  }
  if (net::IPAddress address;
      address.AssignFromIPLiteral(*options.local_hostname)) {
    return net::IPEndPoint(std::move(address), options.local_port);
  }
  return {};
}

void DirectSocketsServiceImpl::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host == frame_host_)
    frame_host_ = nullptr;
}

void DirectSocketsServiceImpl::WebContentsDestroyed() {
  frame_host_ = nullptr;
}

network::mojom::NetworkContext* DirectSocketsServiceImpl::GetNetworkContext() {
  if (GetNetworkContextForTesting())
    return GetNetworkContextForTesting();

  if (!frame_host_)
    return nullptr;

  return frame_host_->GetStoragePartition()->GetNetworkContext();
}

RenderFrameHost* DirectSocketsServiceImpl::GetFrameHost() {
  return frame_host_;
}

net::Error DirectSocketsServiceImpl::ValidateOptions(
    const blink::mojom::DirectSocketOptions& options) {
  if (!frame_host_)
    return net::ERR_CONTEXT_SHUT_DOWN;

  if (GetPermissionCallbackForTesting())
    return GetPermissionCallbackForTesting().Run(options);  // IN-TEST

  if (options.send_buffer_size < 0 || options.receive_buffer_size < 0)
    return net::ERR_INVALID_ARGUMENT;

  return net::OK;
}

void DirectSocketsServiceImpl::AddDirectUDPSocketReceiver(
    std::unique_ptr<DirectUDPSocketImpl> socket,
    mojo::PendingReceiver<blink::mojom::DirectUDPSocket> receiver) {
  direct_udp_socket_receivers_.Add(std::move(socket), std::move(receiver));
}

}  // namespace content