// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/address_list.h"
#include "net/dns/public/host_resolver_results.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"

namespace network {
class SimpleHostResolver;
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

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

  // blink::mojom::DirectSocketsService:
  void OpenTCPSocket(
      blink::mojom::DirectTCPSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      OpenTCPSocketCallback callback) override;
  void OpenConnectedUDPSocket(
      blink::mojom::DirectConnectedUDPSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::RestrictedUDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
      OpenConnectedUDPSocketCallback callback) override;
  void OpenBoundUDPSocket(
      blink::mojom::DirectBoundUDPSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::RestrictedUDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener,
      OpenBoundUDPSocketCallback callback) override;
  void OpenTCPServerSocket(
      blink::mojom::DirectTCPServerSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::TCPServerSocket> socket,
      OpenTCPServerSocketCallback callback) override;

  // Testing:
  static void SetNetworkContextForTesting(network::mojom::NetworkContext*);

#if BUILDFLAG(IS_CHROMEOS)
  static void SetAlwaysOpenFirewallHoleForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  DirectSocketsServiceImpl(
      RenderFrameHost*,
      mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver);

  network::mojom::NetworkContext* GetNetworkContext() const;

  void OnResolveCompleteForTCPSocket(
      blink::mojom::DirectTCPSocketOptionsPtr,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket>,
      mojo::PendingRemote<network::mojom::SocketObserver>,
      OpenTCPSocketCallback,
      int result,
      const net::ResolveErrorInfo&,
      const std::optional<net::AddressList>& resolved_addresses,
      const std::optional<net::HostResolverEndpointResults>&);

  void CreateTCPConnectedSocketImpl(
      const net::AddressList& resolved_addresses,
      network::mojom::TCPConnectedSocketOptionsPtr options,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket>,
      mojo::PendingRemote<network::mojom::SocketObserver>,
      OpenTCPSocketCallback);

  void OnResolveCompleteForUDPSocket(
      blink::mojom::DirectConnectedUDPSocketOptionsPtr,
      mojo::PendingReceiver<network::mojom::RestrictedUDPSocket>,
      mojo::PendingRemote<network::mojom::UDPSocketListener>,
      OpenConnectedUDPSocketCallback,
      int result,
      const net::ResolveErrorInfo&,
      const std::optional<net::AddressList>& resolved_addresses,
      const std::optional<net::HostResolverEndpointResults>&);

  void CreateRestrictedUDPSocketImpl(
      const net::IPEndPoint& peer_addr,
      network::mojom::RestrictedUDPSocketMode mode,
      network::mojom::RestrictedUDPSocketParamsPtr options,
      mojo::PendingReceiver<network::mojom::RestrictedUDPSocket>,
      mojo::PendingRemote<network::mojom::UDPSocketListener>,
      base::OnceCallback<void(int32_t, const std::optional<net::IPEndPoint>&)>
          callback);

  std::unique_ptr<network::SimpleHostResolver> resolver_;

#if BUILDFLAG(IS_CHROMEOS)
  class FirewallHoleDelegate;
  std::unique_ptr<FirewallHoleDelegate> firewall_hole_delegate_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::WeakPtrFactory<DirectSocketsServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
