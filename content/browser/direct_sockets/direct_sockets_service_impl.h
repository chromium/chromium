// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "content/browser/direct_sockets/direct_udp_socket_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/direct_sockets_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
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
  explicit DirectSocketsServiceImpl(RenderFrameHost& frame_host);
  ~DirectSocketsServiceImpl() override;

  DirectSocketsServiceImpl(const DirectSocketsServiceImpl&) = delete;
  DirectSocketsServiceImpl& operator=(const DirectSocketsServiceImpl&) = delete;

  static void CreateForFrame(
      RenderFrameHost* frame,
      mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver);

  static content::DirectSocketsDelegate* GetDelegate();

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
  RenderFrameHost* GetFrameHost();

  void AddDirectUDPSocketReceiver(
      std::unique_ptr<DirectUDPSocketImpl> socket,
      mojo::PendingReceiver<blink::mojom::DirectUDPSocket> receiver);

  static net::MutableNetworkTrafficAnnotationTag MutableTrafficAnnotation();
  static net::NetworkTrafficAnnotationTag TrafficAnnotation();
  static int32_t GetMaxBufferSize();

  static void SetEnterpriseManagedForTesting(bool enterprise_managed);

  static void SetNetworkContextForTesting(network::mojom::NetworkContext*);

  static absl::optional<net::IPEndPoint> GetLocalAddrForTesting(
      const blink::mojom::DirectSocketOptions& options);

 private:
  friend class DirectSocketsUnitTest;

  raw_ptr<RenderFrameHost> frame_host_;
  mojo::UniqueReceiverSet<blink::mojom::DirectUDPSocket>
      direct_udp_socket_receivers_;

  std::unique_ptr<network::SimpleURLLoader> loader_;
  base::WeakPtrFactory<DirectSocketsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
