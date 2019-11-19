// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tls_socket_factory.h"

#include <string>
#include <utility>

#include "base/optional.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request_context.h"
#include "services/network/ssl_config_type_converter.h"
#include "services/network/tls_client_socket.h"

namespace network {
namespace {
// Cert verifier which blindly accepts all certificates, regardless of validity.
class FakeCertVerifier : public net::CertVerifier {
 public:
  FakeCertVerifier() {}
  ~FakeCertVerifier() override {}

  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback,
             std::unique_ptr<Request>*,
             const net::NetLogWithSource&) override {
    verify_result->Reset();
    verify_result->verified_cert = params.certificate();
    return net::OK;
  }
  void SetConfig(const Config& config) override {}
};
}  // namespace

TLSSocketFactory::TLSSocketFactory(
    net::URLRequestContext* url_request_context,
    const net::HttpNetworkSession::Context* http_context)
    : ssl_client_context_(url_request_context->ssl_config_service(),
                          url_request_context->cert_verifier(),
                          url_request_context->transport_security_state(),
                          url_request_context->cert_transparency_verifier(),
                          url_request_context->ct_policy_enforcer(),
                          nullptr /* Disables SSL session caching */),
      client_socket_factory_(nullptr),
      ssl_config_service_(url_request_context->ssl_config_service()) {
  if (http_context) {
    client_socket_factory_ = http_context->client_socket_factory;
  }

  if (!client_socket_factory_ &&
      url_request_context->GetNetworkSessionContext()) {
    client_socket_factory_ =
        url_request_context->GetNetworkSessionContext()->client_socket_factory;
  }
  if (!client_socket_factory_)
    client_socket_factory_ = net::ClientSocketFactory::GetDefaultFactory();
}

TLSSocketFactory::~TLSSocketFactory() {}

void TLSSocketFactory::UpgradeToTLS(
    Delegate* socket_delegate,
    const net::HostPortPair& host_port_pair,
    mojom::TLSClientSocketOptionsPtr socket_options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TLSClientSocket> receiver,
    mojo::PendingRemote<mojom::SocketObserver> observer,
    UpgradeToTLSCallback callback) {
  const net::StreamSocket* socket = socket_delegate->BorrowSocket();
  if (!socket || !socket->IsConnected()) {
    std::move(callback).Run(
        net::ERR_SOCKET_NOT_CONNECTED, mojo::ScopedDataPipeConsumerHandle(),
        mojo::ScopedDataPipeProducerHandle(), base::nullopt);
    return;
  }
  CreateTLSClientSocket(
      host_port_pair, std::move(socket_options), std::move(receiver),
      socket_delegate->TakeSocket(), std::move(observer),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(callback));
}

void TLSSocketFactory::CreateTLSClientSocket(
    const net::HostPortPair& host_port_pair,
    mojom::TLSClientSocketOptionsPtr socket_options,
    mojo::PendingReceiver<mojom::TLSClientSocket> receiver,
    std::unique_ptr<net::StreamSocket> underlying_socket,
    mojo::PendingRemote<mojom::SocketObserver> observer,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojom::TCPConnectedSocket::UpgradeToTLSCallback callback) {
  auto socket = std::make_unique<TLSClientSocket>(
      std::move(observer),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation));
  TLSClientSocket* socket_raw = socket.get();
  tls_socket_receivers_.Add(std::move(socket), std::move(receiver));

  net::SSLClientContext* ssl_client_context = &ssl_client_context_;

  bool send_ssl_info = false;
  net::SSLConfig ssl_config;
  if (socket_options) {
    ssl_config.version_min_override =
        mojo::MojoSSLVersionToNetSSLVersion(socket_options->version_min);
    ssl_config.version_max_override =
        mojo::MojoSSLVersionToNetSSLVersion(socket_options->version_max);

    send_ssl_info = socket_options->send_ssl_info;

    if (socket_options->unsafely_skip_cert_verification) {
      if (!no_verification_ssl_client_context_) {
        no_verification_cert_verifier_ = std::make_unique<FakeCertVerifier>();
        no_verification_transport_security_state_ =
            std::make_unique<net::TransportSecurityState>();
        no_verification_cert_transparency_verifier_ =
            std::make_unique<net::MultiLogCTVerifier>();
        no_verification_ct_policy_enforcer_ =
            std::make_unique<net::DefaultCTPolicyEnforcer>();
        no_verification_ssl_client_context_ =
            std::make_unique<net::SSLClientContext>(
                ssl_config_service_, no_verification_cert_verifier_.get(),
                no_verification_transport_security_state_.get(),
                no_verification_cert_transparency_verifier_.get(),
                no_verification_ct_policy_enforcer_.get(),
                nullptr /* no session cache */);
      }
      ssl_client_context = no_verification_ssl_client_context_.get();
      send_ssl_info = true;
    }
  }
  socket_raw->Connect(host_port_pair, ssl_config, std::move(underlying_socket),
                      ssl_client_context, client_socket_factory_,
                      std::move(callback), send_ssl_info);
}

}  // namespace network
