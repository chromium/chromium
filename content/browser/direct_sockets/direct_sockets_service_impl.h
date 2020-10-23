// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
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
  using PermissionCallback = base::RepeatingCallback<
      net::Error(const blink::mojom::DirectSocketOptions&, net::IPAddress&)>;

  explicit DirectSocketsServiceImpl(RenderFrameHost& frame_host);
  ~DirectSocketsServiceImpl() override = default;

  DirectSocketsServiceImpl(const DirectSocketsServiceImpl&) = delete;
  DirectSocketsServiceImpl& operator=(const DirectSocketsServiceImpl&) = delete;

  static void CreateForFrame(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver);

  // blink::mojom::DirectSocketsService override:
  void OpenTcpSocket(
      blink::mojom::DirectSocketOptionsPtr options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      OpenTcpSocketCallback callback) override;
  void OpenUdpSocket(
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
      OpenUdpSocketCallback callback) override;

  // WebContentsObserver override:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  static void SetPermissionCallbackForTesting(PermissionCallback callback);

  static void SetNetworkContextForTesting(network::mojom::NetworkContext*);

 private:
  friend class DirectSocketsUnitTest;

  // Returns net::OK and populates |remote_address| if the options are valid and
  // the connection is permitted.
  net::Error ValidateOptions(const blink::mojom::DirectSocketOptions& options,
                             net::IPAddress& remote_address);

  network::mojom::NetworkContext* GetNetworkContext();

  RenderFrameHost* frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
