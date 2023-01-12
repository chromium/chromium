// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/address_list.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace content {

class DirectSocketsDelegate;

// Implementation of the DirectSocketsService Mojo service.
class CONTENT_EXPORT DirectSocketsServiceImpl
    : public DocumentService<blink::mojom::DirectSocketsService> {
 public:
  ~DirectSocketsServiceImpl() override;

  static void CreateForFrame(
      RenderFrameHost*,
      mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver);

  static content::DirectSocketsDelegate* GetDelegate();

  // blink::mojom::DirectSocketsService:
  void OpenTcpSocket(
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      OpenTcpSocketCallback callback) override;
  void OpenUdpSocket(
      blink::mojom::DirectSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::RestrictedUDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
      OpenUdpSocketCallback callback) override;

  // Testing:
  static void SetNetworkContextForTesting(network::mojom::NetworkContext*);

 private:
  DirectSocketsServiceImpl(
      RenderFrameHost*,
      mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver);

  network::mojom::NetworkContext* GetNetworkContext() const;

  void OnResolveCompleteForTcpSocket(
      blink::mojom::DirectSocketOptionsPtr,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket>,
      mojo::PendingRemote<network::mojom::SocketObserver>,
      OpenTcpSocketCallback,
      int result,
      const absl::optional<net::AddressList>& resolved_addresses);

  void OnResolveCompleteForUdpSocket(
      blink::mojom::DirectSocketOptionsPtr,
      mojo::PendingReceiver<network::mojom::RestrictedUDPSocket>,
      mojo::PendingRemote<network::mojom::UDPSocketListener>,
      OpenUdpSocketCallback,
      int result,
      const absl::optional<net::AddressList>& resolved_addresses);

  base::WeakPtrFactory<DirectSocketsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
