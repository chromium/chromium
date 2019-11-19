// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_RESOLVING_CLIENT_SOCKET_H_
#define SERVICES_NETWORK_PROXY_RESOLVING_CLIENT_SOCKET_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/connect_job.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace net {
struct CommonConnectJobParams;
class HttpAuthController;
class HttpResponseInfo;
class HttpNetworkSession;
}  // namespace net

namespace network {

// This class represents a net::StreamSocket implementation that does proxy
// resolution for the provided url before establishing a connection. If there is
// a proxy configured, a connection will be established to the proxy.
class COMPONENT_EXPORT(NETWORK_SERVICE) ProxyResolvingClientSocket
    : public net::StreamSocket,
      public net::ConnectJob::Delegate {
 public:
  // Constructs a new ProxyResolvingClientSocket. |url|'s host and port specify
  // where a connection will be established to. The full URL will be only used
  // for proxy resolution. Caller doesn't need to explicitly sanitize the url,
  // any sensitive data (like embedded usernames and passwords), and local data
  // (i.e. reference fragment) will be sanitized by
  // net::ProxyResolutionService::ResolveProxyHelper() before the url is
  // disclosed to the proxy. If |use_tls|, this will try to do a tls connect
  // instead of a regular tcp connect. |network_session| and
  // |common_connect_job_params| must outlive |this|.
  ProxyResolvingClientSocket(
      net::HttpNetworkSession* network_session,
      const net::CommonConnectJobParams* common_connect_job_params,
      const GURL& url,
      bool use_tls);
  ~ProxyResolvingClientSocket() override;

  // net::StreamSocket implementation.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int ReadIfReady(net::IOBuffer* buf,
                  int buf_len,
                  net::CompletionOnceCallback callback) override;
  int CancelReadIfReady() override;
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int Connect(net::CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(net::IPEndPoint* address) const override;
  int GetLocalAddress(net::IPEndPoint* address) const override;
  const net::NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  bool WasAlpnNegotiated() const override;
  net::NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(net::SSLInfo* ssl_info) override;
  void GetConnectionAttempts(net::ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const net::ConnectionAttempts& attempts) override {
  }
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const net::SocketTag& tag) override;

 private:
  enum State {
    STATE_PROXY_RESOLVE,
    STATE_PROXY_RESOLVE_COMPLETE,
    STATE_INIT_CONNECTION,
    STATE_INIT_CONNECTION_COMPLETE,
    STATE_DONE,
    STATE_NONE,
  };

  FRIEND_TEST_ALL_PREFIXES(ProxyResolvingClientSocketTest, ConnectToProxy);
  FRIEND_TEST_ALL_PREFIXES(ProxyResolvingClientSocketTest, ReadWriteErrors);
  FRIEND_TEST_ALL_PREFIXES(ProxyResolvingClientSocketTest,
                           ResetSocketAfterTunnelAuth);

  void OnIOComplete(int result);

  int DoLoop(int result);
  int DoProxyResolve();
  int DoProxyResolveComplete(int result);
  int DoInitConnection();
  int DoInitConnectionComplete(int result);

  // net::ConnectJob::Delegate implementation:
  void OnConnectJobComplete(int result, net::ConnectJob* job) override;
  void OnNeedsProxyAuth(const net::HttpResponseInfo& response,
                        net::HttpAuthController* auth_controller,
                        base::OnceClosure restart_with_auth_callback,
                        net::ConnectJob* job) override;

  int ReconsiderProxyAfterError(int error);

  net::HttpNetworkSession* network_session_;

  const net::CommonConnectJobParams* common_connect_job_params_;
  std::unique_ptr<net::ConnectJob> connect_job_;
  std::unique_ptr<net::StreamSocket> socket_;

  std::unique_ptr<net::ProxyResolutionService::Request> proxy_resolve_request_;
  net::ProxyInfo proxy_info_;
  const GURL url_;
  const bool use_tls_;

  net::NetLogWithSource net_log_;

  // The callback passed to Connect().
  net::CompletionOnceCallback user_connect_callback_;

  State next_state_;

  base::WeakPtrFactory<ProxyResolvingClientSocket> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProxyResolvingClientSocket);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_RESOLVING_CLIENT_SOCKET_H_
