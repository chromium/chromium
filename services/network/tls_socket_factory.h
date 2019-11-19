// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TLS_SOCKET_FACTORY_H_
#define SERVICES_NETWORK_TLS_SOCKET_FACTORY_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/http/http_network_session.h"
#include "net/socket/ssl_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"

namespace net {
class ClientSocketFactory;
class SSLConfigService;
class StreamSocket;
}  // namespace net

namespace network {

// Helper class that handles TLS socket requests.
class COMPONENT_EXPORT(NETWORK_SERVICE) TLSSocketFactory {
 public:
  class Delegate {
   public:
    virtual const net::StreamSocket* BorrowSocket() = 0;
    virtual std::unique_ptr<net::StreamSocket> TakeSocket() = 0;
  };

  // See documentation of UpgradeToTLS in tcp_socket.mojom for
  // the semantics of the results.
  using UpgradeToTLSCallback =
      base::OnceCallback<void(int32_t net_error,
                              mojo::ScopedDataPipeConsumerHandle receive_stream,
                              mojo::ScopedDataPipeProducerHandle send_stream,
                              const base::Optional<net::SSLInfo>& ssl_info)>;

  // Constructs a TLSSocketFactory. If |net_log| is non-null, it is used to
  // log NetLog events when logging is enabled. |net_log| used to must outlive
  // |this|.  Sockets will be created using, the earliest available from:
  // 1) A ClientSocketFactory set on a non-null |http_context|.
  // 2) A ClientSocketFactory set on |url_request_context|'s
  //    HttpNetworkSession::Context
  // 3) The default ClientSocketFactory.
  TLSSocketFactory(net::URLRequestContext* url_request_context,
                   const net::HttpNetworkSession::Context* http_context);
  virtual ~TLSSocketFactory();

  // Upgrades an existing socket to TLS. The previous pipes and data pump
  // must already have been destroyed before the call to this method.
  void UpgradeToTLS(
      Delegate* socket_delegate,
      const net::HostPortPair& host_port_pair,
      mojom::TLSClientSocketOptionsPtr socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TLSClientSocket> receiver,
      mojo::PendingRemote<mojom::SocketObserver> observer,
      UpgradeToTLSCallback callback);

 private:
  void CreateTLSClientSocket(
      const net::HostPortPair& host_port_pair,
      mojom::TLSClientSocketOptionsPtr socket_options,
      mojo::PendingReceiver<mojom::TLSClientSocket> receiver,
      std::unique_ptr<net::StreamSocket> underlying_socket,
      mojo::PendingRemote<mojom::SocketObserver> observer,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojom::TCPConnectedSocket::UpgradeToTLSCallback callback);

  // The following are used when |unsafely_skip_cert_verification| is specified
  // in upgrade options.
  std::unique_ptr<net::SSLClientContext> no_verification_ssl_client_context_;
  std::unique_ptr<net::CertVerifier> no_verification_cert_verifier_;
  std::unique_ptr<net::TransportSecurityState>
      no_verification_transport_security_state_;
  std::unique_ptr<net::CTVerifier> no_verification_cert_transparency_verifier_;
  std::unique_ptr<net::CTPolicyEnforcer> no_verification_ct_policy_enforcer_;

  net::SSLClientContext ssl_client_context_;
  net::ClientSocketFactory* client_socket_factory_;
  net::SSLConfigService* const ssl_config_service_;
  mojo::UniqueReceiverSet<mojom::TLSClientSocket> tls_socket_receivers_;

  DISALLOW_COPY_AND_ASSIGN(TLSSocketFactory);
};

}  // namespace network

#endif  // SERVICES_NETWORK_SOCKET_FACTORY_H_
