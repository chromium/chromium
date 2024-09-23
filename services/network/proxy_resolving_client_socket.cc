// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolving_client_socket.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/proxy_client_socket.h"
#include "net/http/proxy_fallback.h"
#include "net/log/net_log_source_type.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/socket/connect_job_factory.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace network {

ProxyResolvingClientSocket::ProxyResolvingClientSocket(
    net::HttpNetworkSession* network_session,
    const net::CommonConnectJobParams* common_connect_job_params,
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    bool use_tls,
    const net::ConnectJobFactory* connect_job_factory)
    : network_session_(network_session),
      common_connect_job_params_(common_connect_job_params),
      connect_job_factory_(connect_job_factory),
      url_(url),
      network_anonymization_key_(network_anonymization_key),
      use_tls_(use_tls),
      net_log_(net::NetLogWithSource::Make(network_session_->net_log(),
                                           net::NetLogSourceType::SOCKET)),
      next_state_(STATE_NONE) {
  // TODO(xunjieli): Handle invalid URLs more gracefully (at mojo API layer
  // or when the request is created).
  DCHECK(url_.is_valid());
  DCHECK(connect_job_factory_);
}

ProxyResolvingClientSocket::~ProxyResolvingClientSocket() {}

int ProxyResolvingClientSocket::Read(net::IOBuffer* buf,
                                     int buf_len,
                                     net::CompletionOnceCallback callback) {
  if (socket_)
    return socket_->Read(buf, buf_len, std::move(callback));
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::ReadIfReady(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback) {
  if (socket_)
    return socket_->ReadIfReady(buf, buf_len, std::move(callback));
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::CancelReadIfReady() {
  if (socket_)
    return socket_->CancelReadIfReady();
  // Return net::OK as ReadIfReady() is canceled when socket is disconnected.
  return net::OK;
}

int ProxyResolvingClientSocket::Write(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  if (socket_) {
    return socket_->Write(buf, buf_len, std::move(callback),
                          traffic_annotation);
  }
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::SetReceiveBufferSize(int32_t size) {
  if (socket_)
    return socket_->SetReceiveBufferSize(size);
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::SetSendBufferSize(int32_t size) {
  if (socket_)
    return socket_->SetSendBufferSize(size);
  return net::ERR_SOCKET_NOT_CONNECTED;
}

int ProxyResolvingClientSocket::Connect(net::CompletionOnceCallback callback) {
  DCHECK(user_connect_callback_.is_null());
  DCHECK(!socket_);

  next_state_ = STATE_PROXY_RESOLVE;
  int result = DoLoop(net::OK);
  if (result == net::ERR_IO_PENDING) {
    user_connect_callback_ = std::move(callback);
  }
  return result;
}

void ProxyResolvingClientSocket::Disconnect() {
  connect_job_.reset();
  socket_.reset();
  if (proxy_resolve_request_)
    proxy_resolve_request_.reset();
  user_connect_callback_.Reset();
}

bool ProxyResolvingClientSocket::IsConnected() const {
  if (!socket_)
    return false;
  return socket_->IsConnected();
}

bool ProxyResolvingClientSocket::IsConnectedAndIdle() const {
  if (!socket_)
    return false;
  return socket_->IsConnectedAndIdle();
}

int ProxyResolvingClientSocket::GetPeerAddress(net::IPEndPoint* address) const {
  if (!socket_)
    return net::ERR_SOCKET_NOT_CONNECTED;

  if (proxy_info_.is_direct())
    return socket_->GetPeerAddress(address);

  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(url_.HostNoBrackets())) {
    // Do not expose the proxy IP address to the caller.
    return net::ERR_NAME_NOT_RESOLVED;
  }

  *address = net::IPEndPoint(ip_address, url_.EffectiveIntPort());
  return net::OK;
}

int ProxyResolvingClientSocket::GetLocalAddress(
    net::IPEndPoint* address) const {
  if (socket_)
    return socket_->GetLocalAddress(address);
  return net::ERR_SOCKET_NOT_CONNECTED;
}

const net::NetLogWithSource& ProxyResolvingClientSocket::NetLog() const {
  if (socket_)
    return socket_->NetLog();
  return net_log_;
}

bool ProxyResolvingClientSocket::WasEverUsed() const {
  if (socket_)
    return socket_->WasEverUsed();
  return false;
}

net::NextProto ProxyResolvingClientSocket::GetNegotiatedProtocol() const {
  if (socket_)
    return socket_->GetNegotiatedProtocol();
  return net::kProtoUnknown;
}

bool ProxyResolvingClientSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  if (socket_)
    return socket_->GetSSLInfo(ssl_info);
  return false;
}

int64_t ProxyResolvingClientSocket::GetTotalReceivedBytes() const {
  NOTIMPLEMENTED();
  return 0;
}

void ProxyResolvingClientSocket::ApplySocketTag(const net::SocketTag& tag) {
  NOTIMPLEMENTED();
}

void ProxyResolvingClientSocket::OnIOComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  int net_error = DoLoop(result);
  if (net_error != net::ERR_IO_PENDING)
    std::move(user_connect_callback_).Run(net_error);
}

int ProxyResolvingClientSocket::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_PROXY_RESOLVE:
        DCHECK_EQ(net::OK, rv);
        rv = DoProxyResolve();
        break;
      case STATE_PROXY_RESOLVE_COMPLETE:
        rv = DoProxyResolveComplete(rv);
        break;
      case STATE_INIT_CONNECTION:
        DCHECK_EQ(net::OK, rv);
        rv = DoInitConnection();
        break;
      case STATE_INIT_CONNECTION_COMPLETE:
        rv = DoInitConnectionComplete(rv);
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "bad state";
        rv = net::ERR_FAILED;
        break;
    }
  } while (rv != net::ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int ProxyResolvingClientSocket::DoProxyResolve() {
  next_state_ = STATE_PROXY_RESOLVE_COMPLETE;
  // base::Unretained(this) is safe because resolution request is canceled when
  // |proxy_resolve_request_| is destroyed.
  //
  // TODO(crbug.com/40658165): Pass along a NetworkAnonymizationKey.
  return network_session_->proxy_resolution_service()->ResolveProxy(
      url_, net::HttpRequestHeaders::kPostMethod, network_anonymization_key_,
      &proxy_info_,
      base::BindOnce(&ProxyResolvingClientSocket::OnIOComplete,
                     base::Unretained(this)),
      &proxy_resolve_request_, net_log_);
}

int ProxyResolvingClientSocket::DoProxyResolveComplete(int result) {
  proxy_resolve_request_ = nullptr;
  if (result == net::OK) {
    // Removes unsupported proxies from the list. Currently, this removes
    // just the SCHEME_QUIC proxy.
    // TODO(crbug.com/41409577): Allow QUIC proxy once
    // net::QuicProxyClientSocket supports ReadIfReady() and
    // CancelReadIfReady().
    proxy_info_.RemoveProxiesWithoutScheme(
        net::ProxyServer::SCHEME_HTTP | net::ProxyServer::SCHEME_HTTPS |
        net::ProxyServer::SCHEME_SOCKS4 | net::ProxyServer::SCHEME_SOCKS5);

    if (proxy_info_.is_empty()) {
      // No proxies/direct to choose from. This happens when we don't support
      // any of the proxies in the returned list.
      return net::ERR_NO_SUPPORTED_PROXIES;
    }
    next_state_ = STATE_INIT_CONNECTION;
    return net::OK;
  }
  return result;
}

int ProxyResolvingClientSocket::DoInitConnection() {
  DCHECK(!socket_);
  // QUIC proxies are currently not supported.
  DCHECK(proxy_info_.is_direct() ||
         !proxy_info_.proxy_chain().Last().is_quic());

  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  std::optional<net::NetworkTrafficAnnotationTag> proxy_annotation_tag =
      proxy_info_.is_direct() ? std::nullopt
                              : std::optional<net::NetworkTrafficAnnotationTag>(
                                    proxy_info_.traffic_annotation());

  // Now that the proxy is resolved, create and start a ConnectJob. Using an
  // empty NetworkAnonymizationKey means that tunnels over H2 or QUIC proxies
  // will be shared, which may result in privacy leaks, depending on the nature
  // of the consumer.
  //
  // TODO(mmenke): Investigate that.
  connect_job_ = connect_job_factory_->CreateConnectJob(
      use_tls_, net::HostPortPair::FromURL(url_), proxy_info_.proxy_chain(),
      proxy_annotation_tag, /*force_tunnel=*/true, net::PRIVACY_MODE_DISABLED,
      net::OnHostResolutionCallback(), net::MAXIMUM_PRIORITY, net::SocketTag(),
      network_anonymization_key_, net::SecureDnsPolicy::kAllow,
      common_connect_job_params_, this);
  return connect_job_->Connect();
}

int ProxyResolvingClientSocket::DoInitConnectionComplete(int result) {
  if (result != net::OK) {
    connect_job_.reset();

    // ReconsiderProxyAfterError either returns an error (in which case it is
    // not reconsidering a proxy) or returns ERR_IO_PENDING if it is considering
    // another proxy.
    return ReconsiderProxyAfterError(result);
  }

  socket_ = connect_job_->PassSocket();
  connect_job_.reset();

  network_session_->proxy_resolution_service()->ReportSuccess(proxy_info_);
  return net::OK;
}

void ProxyResolvingClientSocket::OnConnectJobComplete(int result,
                                                      net::ConnectJob* job) {
  DCHECK_EQ(next_state_, STATE_INIT_CONNECTION_COMPLETE);
  DCHECK_EQ(connect_job_.get(), job);

  OnIOComplete(result);
}

void ProxyResolvingClientSocket::OnNeedsProxyAuth(
    const net::HttpResponseInfo& response,
    net::HttpAuthController* auth_controller,
    base::OnceClosure restart_with_auth_callback,
    net::ConnectJob* job) {
  DCHECK_EQ(next_state_, STATE_INIT_CONNECTION_COMPLETE);
  DCHECK_EQ(connect_job_.get(), job);

  // If there are credentials available to try and use, use them.
  if (auth_controller->HaveAuth()) {
    std::move(restart_with_auth_callback).Run();
    return;
  }

  // Otherwise, cancel the ConnectJob and continue.
  connect_job_.reset();

  OnIOComplete(net::ERR_PROXY_AUTH_REQUESTED);
}

int ProxyResolvingClientSocket::ReconsiderProxyAfterError(int error) {
  DCHECK(!socket_);
  DCHECK(!proxy_resolve_request_);
  DCHECK_NE(error, net::OK);
  DCHECK_NE(error, net::ERR_IO_PENDING);

  // Check if the error was a proxy failure.
  if (!net::CanFalloverToNextProxy(proxy_info_.proxy_chain(), error, &error,
                                   proxy_info_.is_for_ip_protection())) {
    return error;
  }

  // TODO(davidben): When adding proxy client certificate support to this class,
  // clear the SSLClientAuthCache entries on error.

  // There was nothing left to fall-back to, so fail the transaction
  // with the last connection error we got.
  if (!proxy_info_.Fallback(error, net_log_))
    return error;

  next_state_ = STATE_INIT_CONNECTION;
  return net::OK;
}

}  // namespace network
