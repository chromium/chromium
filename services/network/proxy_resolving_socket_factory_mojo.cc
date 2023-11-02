// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolving_socket_factory_mojo.h"

#include <utility>

#include "components/webrtc/fake_ssl_client_socket.h"
#include "net/url_request/url_request_context.h"
#include "services/network/proxy_resolving_client_socket.h"
#include "services/network/proxy_resolving_client_socket_factory.h"
#include "services/network/proxy_resolving_socket_mojo.h"
#include "url/gurl.h"

namespace network {

ProxyResolvingSocketFactoryMojo::ProxyResolvingSocketFactoryMojo(
    net::URLRequestContext* request_context)
    : factory_impl_(request_context), tls_socket_factory_(request_context) {}

ProxyResolvingSocketFactoryMojo::~ProxyResolvingSocketFactoryMojo() {}

void ProxyResolvingSocketFactoryMojo::CreateProxyResolvingSocket(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    mojom::ProxyResolvingSocketOptionsPtr options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::ProxyResolvingSocket> receiver,
    mojo::PendingRemote<mojom::SocketObserver> observer,
    CreateProxyResolvingSocketCallback callback) {
  std::unique_ptr<net::StreamSocket> net_socket = factory_impl_.CreateSocket(
      url, network_anonymization_key, options && options->use_tls);
  if (options && options->fake_tls_handshake) {
    DCHECK(!options->use_tls);
    net_socket =
        std::make_unique<webrtc::FakeSSLClientSocket>(std::move(net_socket));
  }

  auto socket = std::make_unique<ProxyResolvingSocketMojo>(
      std::move(net_socket),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(observer), &tls_socket_factory_);
  ProxyResolvingSocketMojo* socket_raw = socket.get();
  proxy_resolving_socket_receivers_.Add(std::move(socket), std::move(receiver));
  socket_raw->Connect(std::move(callback));
}

}  // namespace network
