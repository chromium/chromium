// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_FACTORY_MOJO_H_
#define SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_FACTORY_MOJO_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/proxy_resolving_client_socket_factory.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/tls_socket_factory.h"

namespace net {
class NetworkAnonymizationKey;
class URLRequestContext;
}  // namespace net

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) ProxyResolvingSocketFactoryMojo
    : public mojom::ProxyResolvingSocketFactory {
 public:
  ProxyResolvingSocketFactoryMojo(net::URLRequestContext* request_context);

  ProxyResolvingSocketFactoryMojo(const ProxyResolvingSocketFactoryMojo&) =
      delete;
  ProxyResolvingSocketFactoryMojo& operator=(
      const ProxyResolvingSocketFactoryMojo&) = delete;

  ~ProxyResolvingSocketFactoryMojo() override;

  // mojom::ProxyResolvingSocketFactory implementation.
  void CreateProxyResolvingSocket(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojom::ProxyResolvingSocketOptionsPtr options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::ProxyResolvingSocket> receiver,
      mojo::PendingRemote<mojom::SocketObserver> observer,
      CreateProxyResolvingSocketCallback callback) override;

 private:
  ProxyResolvingClientSocketFactory factory_impl_;
  TLSSocketFactory tls_socket_factory_;
  mojo::UniqueReceiverSet<mojom::ProxyResolvingSocket>
      proxy_resolving_socket_receivers_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_FACTORY_MOJO_H_
