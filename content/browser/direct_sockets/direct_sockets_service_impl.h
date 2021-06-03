// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace content {

// Implementation of the DirectSocketsService Mojo service.
class CONTENT_EXPORT DirectSocketsServiceImpl
    : public blink::mojom::DirectSocketsService,
      public WebContentsObserver {
 public:
  // This enum is used to track how often each permission check cause
  // Permission Denied failures.
  enum class FailureType {
    kPermissionsPolicy = 0,
    kTransientActivation = 1,
    kUserDialog = 2,
    kResolvingToNonPublic = 3,
    kRateLimiting = 4,
    kCORS = 5,
    kEnterprisePolicy = 6,
    kMaxValue = kEnterprisePolicy,
  };

  enum class ProtocolType { kTcp, kUdp };

  using PermissionCallback = base::RepeatingCallback<net::Error(
      const blink::mojom::DirectSocketOptions&)>;

  explicit DirectSocketsServiceImpl(RenderFrameHost& frame_host);
  ~DirectSocketsServiceImpl() override;

  DirectSocketsServiceImpl(const DirectSocketsServiceImpl&) = delete;
  DirectSocketsServiceImpl& operator=(const DirectSocketsServiceImpl&) = delete;

  static void CreateForFrame(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver);

  // blink::mojom::DirectSocketsService override:
  void OpenTcpSocket(
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      OpenTcpSocketCallback callback) override;
  void OpenUdpSocket(
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<blink::mojom::DirectUDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
      OpenUdpSocketCallback callback) override;

  // WebContentsObserver override:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  network::mojom::NetworkContext* GetNetworkContext();

  mojo::UniqueReceiverSet<blink::mojom::DirectUDPSocket>&
  direct_udp_socket_receivers() {
    return direct_udp_socket_receivers_;
  }

  static net::MutableNetworkTrafficAnnotationTag TrafficAnnotation();

  static void SetConnectionDialogBypassForTesting(bool bypass);

  static void SetEnterpriseManagedForTesting(bool enterprise_managed);

  static void SetPermissionCallbackForTesting(PermissionCallback callback);

  static void SetNetworkContextForTesting(network::mojom::NetworkContext*);

  static absl::optional<net::IPEndPoint> GetLocalAddrForTesting(
      const blink::mojom::DirectSocketOptions& options);

 private:
  friend class DirectSocketsUnitTest;

  class ResolveHostAndOpenSocket;

  // Returns net::OK if the options are valid and the connection is permitted.
  net::Error ValidateOptions(const blink::mojom::DirectSocketOptions& options);

  // DirectSocketsRequestDialogController:
  void OnDialogProceedTcp(
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      OpenTcpSocketCallback callback,
      bool accepted,
      const std::string& address,
      const std::string& port);
  void OnDialogProceedUdp(
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<blink::mojom::DirectUDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
      OpenUdpSocketCallback callback,
      bool accepted,
      const std::string& address,
      const std::string& port);

  RenderFrameHost* frame_host_;
  mojo::UniqueReceiverSet<blink::mojom::DirectUDPSocket>
      direct_udp_socket_receivers_;

  base::WeakPtrFactory<DirectSocketsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
