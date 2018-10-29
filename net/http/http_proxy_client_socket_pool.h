// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_POOL_H_
#define NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_POOL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"
#include "net/http/http_response_info.h"
#include "net/http/proxy_client_socket.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class HttpAuthCache;
class HttpAuthHandlerFactory;
class HttpProxyClientSocketWrapper;
class NetLog;
class NetworkQualityEstimator;
class QuicStreamFactory;
class SSLClientSocketPool;
class SSLSocketParams;
class SpdySessionPool;
class TransportClientSocketPool;
class TransportSocketParams;

// HttpProxySocketParams only needs the socket params for one of the proxy
// types.  The other param must be NULL.  When using an HTTP proxy,
// |transport_params| must be set.  When using an HTTPS proxy or QUIC proxy,
// |ssl_params| must be set. Also, if using a QUIC proxy, |quic_version| must
// not be quic::QUIC_VERSION_UNSUPPORTED.
class NET_EXPORT_PRIVATE HttpProxySocketParams
    : public base::RefCounted<HttpProxySocketParams> {
 public:
  HttpProxySocketParams(
      const scoped_refptr<TransportSocketParams>& transport_params,
      const scoped_refptr<SSLSocketParams>& ssl_params,
      quic::QuicTransportVersion quic_version,
      const std::string& user_agent,
      const HostPortPair& endpoint,
      HttpAuthCache* http_auth_cache,
      HttpAuthHandlerFactory* http_auth_handler_factory,
      SpdySessionPool* spdy_session_pool,
      QuicStreamFactory* quic_stream_factory,
      bool is_trusted_proxy,
      bool tunnel,
      const NetworkTrafficAnnotationTag traffic_annotation);

  const scoped_refptr<TransportSocketParams>& transport_params() const {
    return transport_params_;
  }
  const scoped_refptr<SSLSocketParams>& ssl_params() const {
    return ssl_params_;
  }
  quic::QuicTransportVersion quic_version() const { return quic_version_; }
  const std::string& user_agent() const { return user_agent_; }
  const HostPortPair& endpoint() const { return endpoint_; }
  HttpAuthCache* http_auth_cache() const { return http_auth_cache_; }
  HttpAuthHandlerFactory* http_auth_handler_factory() const {
    return http_auth_handler_factory_;
  }
  SpdySessionPool* spdy_session_pool() {
    return spdy_session_pool_;
  }
  QuicStreamFactory* quic_stream_factory() const {
    return quic_stream_factory_;
  }
  const HostResolver::RequestInfo& destination() const;
  bool is_trusted_proxy() const { return is_trusted_proxy_; }
  bool tunnel() const { return tunnel_; }
  const NetworkTrafficAnnotationTag traffic_annotation() const {
    return traffic_annotation_;
  }

 private:
  friend class base::RefCounted<HttpProxySocketParams>;
  ~HttpProxySocketParams();

  const scoped_refptr<TransportSocketParams> transport_params_;
  const scoped_refptr<SSLSocketParams> ssl_params_;
  quic::QuicTransportVersion quic_version_;
  SpdySessionPool* spdy_session_pool_;
  QuicStreamFactory* quic_stream_factory_;
  const std::string user_agent_;
  const HostPortPair endpoint_;
  HttpAuthCache* const http_auth_cache_;
  HttpAuthHandlerFactory* const http_auth_handler_factory_;
  const bool is_trusted_proxy_;
  const bool tunnel_;
  const NetworkTrafficAnnotationTag traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(HttpProxySocketParams);
};

// HttpProxyConnectJob optionally establishes a tunnel through the proxy
// server after connecting the underlying transport socket.
class HttpProxyConnectJob : public ConnectJob {
 public:
  HttpProxyConnectJob(const std::string& group_name,
                      RequestPriority priority,
                      const SocketTag& socket_tag,
                      ClientSocketPool::RespectLimits respect_limits,
                      const scoped_refptr<HttpProxySocketParams>& params,
                      const base::TimeDelta& timeout_duration,
                      TransportClientSocketPool* transport_pool,
                      SSLClientSocketPool* ssl_pool,
                      Delegate* delegate,
                      NetLog* net_log);
  ~HttpProxyConnectJob() override;

  // ConnectJob methods.
  LoadState GetLoadState() const override;

  void GetAdditionalErrorState(ClientSocketHandle* handle) override;

 private:
  // Begins the tcp connection and the optional Http proxy tunnel.  If the
  // request is not immediately servicable (likely), the request will return
  // ERR_IO_PENDING. An OK return from this function or the callback means
  // that the connection is established; ERR_PROXY_AUTH_REQUESTED means
  // that the tunnel needs authentication credentials, the socket will be
  // returned in this case, and must be release back to the pool; or
  // a standard net error code will be returned.
  int ConnectInternal() override;

  void OnConnectComplete(int result);

  int HandleConnectResult(int result);

  std::unique_ptr<HttpProxyClientSocketWrapper> client_socket_;

  std::unique_ptr<HttpResponseInfo> error_response_info_;

  DISALLOW_COPY_AND_ASSIGN(HttpProxyConnectJob);
};

class NET_EXPORT_PRIVATE HttpProxyClientSocketPool
    : public ClientSocketPool,
      public HigherLayeredPool {
 public:
  typedef HttpProxySocketParams SocketParams;

  HttpProxyClientSocketPool(int max_sockets,
                            int max_sockets_per_group,
                            TransportClientSocketPool* transport_pool,
                            SSLClientSocketPool* ssl_pool,
                            NetworkQualityEstimator* network_quality_estimator,
                            NetLog* net_log);

  ~HttpProxyClientSocketPool() override;

  // ClientSocketPool implementation.
  int RequestSocket(const std::string& group_name,
                    const void* connect_params,
                    RequestPriority priority,
                    const SocketTag& socket_tag,
                    RespectLimits respect_limits,
                    ClientSocketHandle* handle,
                    CompletionOnceCallback callback,
                    const NetLogWithSource& net_log) override;

  void RequestSockets(const std::string& group_name,
                      const void* params,
                      int num_sockets,
                      const NetLogWithSource& net_log) override;

  void SetPriority(const std::string& group_name,
                   ClientSocketHandle* handle,
                   RequestPriority priority) override;

  void CancelRequest(const std::string& group_name,
                     ClientSocketHandle* handle) override;

  void ReleaseSocket(const std::string& group_name,
                     std::unique_ptr<StreamSocket> socket,
                     int id) override;

  void FlushWithError(int error) override;

  void CloseIdleSockets() override;

  void CloseIdleSocketsInGroup(const std::string& group_name) override;

  int IdleSocketCount() const override;

  int IdleSocketCountInGroup(const std::string& group_name) const override;

  LoadState GetLoadState(const std::string& group_name,
                         const ClientSocketHandle* handle) const override;

  std::unique_ptr<base::DictionaryValue> GetInfoAsValue(
      const std::string& name,
      const std::string& type,
      bool include_nested_pools) const override;

  base::TimeDelta ConnectionTimeout() const override;

  // LowerLayeredPool implementation.
  bool IsStalled() const override;

  void AddHigherLayeredPool(HigherLayeredPool* higher_pool) override;

  void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool) override;

  // HigherLayeredPool implementation.
  bool CloseOneIdleConnection() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(HttpProxyClientSocketPoolTest,
                           ProxyPoolTimeoutWithConnectionProperty);

  typedef ClientSocketPoolBase<HttpProxySocketParams> PoolBase;

  class NET_EXPORT_PRIVATE HttpProxyConnectJobFactory
      : public PoolBase::ConnectJobFactory {
   public:
    HttpProxyConnectJobFactory(
        TransportClientSocketPool* transport_pool,
        SSLClientSocketPool* ssl_pool,
        NetworkQualityEstimator* network_quality_estimator,
        NetLog* net_log);

    // ClientSocketPoolBase::ConnectJobFactory methods.
    std::unique_ptr<ConnectJob> NewConnectJob(
        const std::string& group_name,
        const PoolBase::Request& request,
        ConnectJob::Delegate* delegate) const override;

    base::TimeDelta ConnectionTimeout() const override;

   private:
    FRIEND_TEST_ALL_PREFIXES(HttpProxyClientSocketPoolTest,
                             ProxyPoolTimeoutWithConnectionProperty);

    // Returns proxy connection timeout for secure proxies if
    // |is_secure_connection| is true. Otherwise, returns timeout for insecure
    // proxies.
    base::TimeDelta ConnectionTimeoutWithConnectionProperty(
        bool is_secure_connection) const;

    TransportClientSocketPool* const transport_pool_;
    SSLClientSocketPool* const ssl_pool_;
    NetworkQualityEstimator* const network_quality_estimator_;

    // For secure proxies, the connection timeout is set to
    // |ssl_http_rtt_multiplier_| times the HTTP RTT estimate. For insecure
    // proxies, the connection timeout is set to |non_ssl_http_rtt_multiplier_|
    // times the HTTP RTT estimate. In either case, the connection timeout
    // is clamped to be between |min_proxy_connection_timeout_| and
    // |max_proxy_connection_timeout_|.
    const int32_t ssl_http_rtt_multiplier_;
    const int32_t non_ssl_http_rtt_multiplier_;
    const base::TimeDelta min_proxy_connection_timeout_;
    const base::TimeDelta max_proxy_connection_timeout_;

    NetLog* net_log_;

    DISALLOW_COPY_AND_ASSIGN(HttpProxyConnectJobFactory);
  };

  TransportClientSocketPool* const transport_pool_;
  SSLClientSocketPool* const ssl_pool_;
  PoolBase base_;

  DISALLOW_COPY_AND_ASSIGN(HttpProxyClientSocketPool);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_POOL_H_
