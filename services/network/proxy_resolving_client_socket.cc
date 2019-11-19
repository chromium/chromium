// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolving_client_socket.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/optional.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_network_session.h"
#include "net/http/proxy_client_socket.h"
#include "net/http/proxy_fallback.h"
#include "net/log/net_log_source_type.h"
#include "net/socket/socket_tag.h"
#include "net/ssl/ssl_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace network {

ProxyResolvingClientSocket::ProxyResolvingClientSocket(
    net::HttpNetworkSession* network_session,
    const net::CommonConnectJobParams* common_connect_job_params,
    const GURL& url,
    bool use_tls)
    : network_session_(network_session),
      common_connect_job_params_(common_connect_job_params),
      url_(url),
      use_tls_(use_tls),
      net_log_(net::NetLogWithSource::Make(network_session_->net_log(),
                                           net::NetLogSourceType::SOCKET)),
      next_state_(STATE_NONE) {
  // TODO(xunjieli): Handle invalid URLs more gracefully (at mojo API layer
  // or when the request is created).
  DCHECK(url_.is_valid());
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

bool ProxyResolvingClientSocket::WasAlpnNegotiated() const {
  if (socket_)
    return socket_->WasAlpnNegotiated();
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

void ProxyResolvingClientSocket::GetConnectionAttempts(
    net::ConnectionAttempts* out) const {
  out->clear();
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
        NOTREACHED() << "bad state";
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
  // TODO(https://crbug.com/1023439): Pass along a NetworkIsolationKey.
  return network_session_->proxy_resolution_service()->ResolveProxy(
      url_, "POST", net::NetworkIsolationKey::Todo(), &proxy_info_,
      base::BindRepeating(&ProxyResolvingClientSocket::OnIOComplete,
                          base::Unretained(this)),
      &proxy_resolve_request_, net_log_);
}

int ProxyResolvingClientSocket::DoProxyResolveComplete(int result) {
  proxy_resolve_request_ = nullptr;
  if (result == net::OK) {
    // Removes unsupported proxies from the list. Currently, this removes
    // just the SCHEME_QUIC proxy.
    // TODO(crbug.com/876885): Allow QUIC proxy once net::QuicProxyClientSocket
    // supports ReadIfReady() and CancelReadIfReady().
    proxy_info_.RemoveProxiesWithoutScheme(
        net::ProxyServer::SCHEME_DIRECT | net::ProxyServer::SCHEME_HTTP |
        net::ProxyServer::SCHEME_HTTPS | net::ProxyServer::SCHEME_SOCKS4 |
        net::ProxyServer::SCHEME_SOCKS5);

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
  DCHECK(!proxy_info_.is_quic());

  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  base::Optional<net::NetworkTrafficAnnotationTag> proxy_annotation_tag =
      proxy_info_.is_direct()
          ? base::nullopt
          : base::Optional<net::NetworkTrafficAnnotationTag>(
                proxy_info_.traffic_annotation());

  // Now that the proxy is resolved, create and start a ConnectJob. Using an
  // empty NetworkIsolationKey means that tunnels over H2 or QUIC proxies will
  // be shared, which may result in privacy leaks, depending on the nature of
  // the consumer.
  //
  // TODO(mmenke): Investigate that.
  net::SSLConfig ssl_config;
  connect_job_ = net::ConnectJob::CreateConnectJob(
      use_tls_, net::HostPortPair::FromURL(url_), proxy_info_.proxy_server(),
      proxy_annotation_tag, &ssl_config, &ssl_config, true /* force_tunnel */,
      net::PRIVACY_MODE_DISABLED, net::OnHostResolutionCallback(),
      net::MAXIMUM_PRIORITY, net::SocketTag(), net::NetworkIsolationKey(),
      false /* disable_secure_dns */, common_connect_job_params_, this);
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
  if (!net::CanFalloverToNextProxy(proxy_info_.proxy_server(), error, &error))
    return error;

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
