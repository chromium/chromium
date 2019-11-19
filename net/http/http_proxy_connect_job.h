// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PROXY_CONNECT_JOB_H_
#define NET_HTTP_HTTP_PROXY_CONNECT_JOB_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/request_priority.h"
#include "net/http/http_auth.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/socket/connect_job.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_session_key.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class HttpAuthController;
class HttpResponseInfo;
class NetworkQualityEstimator;
class SocketTag;
class ProxyClientSocket;
class SpdyStreamRequest;
class SSLSocketParams;
class TransportSocketParams;
class QuicStreamRequest;

// HttpProxySocketParams only needs the socket params for one of the proxy
// types.  The other param must be NULL.  When using an HTTP proxy,
// |transport_params| must be set.  When using an HTTPS proxy or QUIC proxy,
// |ssl_params| must be set. Also, if using a QUIC proxy, |quic_version| must
// not be quic::QUIC_VERSION_UNSUPPORTED.
class NET_EXPORT_PRIVATE HttpProxySocketParams
    : public base::RefCounted<HttpProxySocketParams> {
 public:
  HttpProxySocketParams(scoped_refptr<TransportSocketParams> transport_params,
                        scoped_refptr<SSLSocketParams> ssl_params,
                        bool is_quic,
                        const HostPortPair& endpoint,
                        bool is_trusted_proxy,
                        bool tunnel,
                        const NetworkTrafficAnnotationTag traffic_annotation,
                        const NetworkIsolationKey& network_isolation_key);

  const scoped_refptr<TransportSocketParams>& transport_params() const {
    return transport_params_;
  }
  const scoped_refptr<SSLSocketParams>& ssl_params() const {
    return ssl_params_;
  }
  bool is_quic() const { return is_quic_; }
  const HostPortPair& endpoint() const { return endpoint_; }
  bool is_trusted_proxy() const { return is_trusted_proxy_; }
  bool tunnel() const { return tunnel_; }
  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }
  const NetworkTrafficAnnotationTag traffic_annotation() const {
    return traffic_annotation_;
  }

 private:
  friend class base::RefCounted<HttpProxySocketParams>;
  ~HttpProxySocketParams();

  const scoped_refptr<TransportSocketParams> transport_params_;
  const scoped_refptr<SSLSocketParams> ssl_params_;
  bool is_quic_;
  const HostPortPair endpoint_;
  const bool is_trusted_proxy_;
  const bool tunnel_;
  const NetworkIsolationKey network_isolation_key_;
  const NetworkTrafficAnnotationTag traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(HttpProxySocketParams);
};

// HttpProxyConnectJob optionally establishes a tunnel through the proxy
// server after connecting the underlying transport socket.
class NET_EXPORT_PRIVATE HttpProxyConnectJob : public ConnectJob,
                                               public ConnectJob::Delegate {
 public:
  HttpProxyConnectJob(RequestPriority priority,
                      const SocketTag& socket_tag,
                      const CommonConnectJobParams* common_connect_job_params,
                      scoped_refptr<HttpProxySocketParams> params,
                      ConnectJob::Delegate* delegate,
                      const NetLogWithSource* net_log);
  ~HttpProxyConnectJob() override;

  // A single priority is used for tunnels over H2 and QUIC, which can be shared
  // by multiple requests of different priorities either in series (tunnels for
  // HTTP/1.x requests) or simultaneously (tunnels for H2 requests). Changing
  // the priority of the tunnel based on the current request also potentially
  // leaks private data to the proxy.
  static const RequestPriority kH2QuicTunnelPriority;

  // ConnectJob methods.
  LoadState GetLoadState() const override;
  bool HasEstablishedConnection() const override;
  bool IsSSLError() const override;
  scoped_refptr<SSLCertRequestInfo> GetCertRequestInfo() override;

  // ConnectJob::Delegate implementation.
  void OnConnectJobComplete(int result, ConnectJob* job) override;
  void OnNeedsProxyAuth(const HttpResponseInfo& response,
                        HttpAuthController* auth_controller,
                        base::OnceClosure restart_with_auth_callback,
                        ConnectJob* job) override;

  // In some cases, a timeout that's stricter than the TCP (+SSL, if applicable)
  // is used for HTTP proxies during connection establishment and SSL
  // negotiation for the connection to the proxy itself. In those cases, returns
  // the connection timeout that will be used by a HttpProxyConnectJob created
  // with the specified parameters, given current network conditions. Otherwise,
  // returns base::TimeDelta().
  static base::TimeDelta AlternateNestedConnectionTimeout(
      const HttpProxySocketParams& params,
      const NetworkQualityEstimator* network_quality_estimator);

  // Returns the timeout for establishing a tunnel after a connection has been
  // established.
  static base::TimeDelta TunnelTimeoutForTesting();

  // Updates the field trial parameters used in calculating timeouts.
  static void UpdateFieldTrialParametersForTesting();

 private:
  enum State {
    STATE_BEGIN_CONNECT,
    STATE_TCP_CONNECT,
    STATE_TCP_CONNECT_COMPLETE,
    STATE_SSL_CONNECT,
    STATE_SSL_CONNECT_COMPLETE,
    STATE_HTTP_PROXY_CONNECT,
    STATE_HTTP_PROXY_CONNECT_COMPLETE,
    STATE_SPDY_PROXY_CREATE_STREAM,
    STATE_SPDY_PROXY_CREATE_STREAM_COMPLETE,
    STATE_QUIC_PROXY_CREATE_SESSION,
    STATE_QUIC_PROXY_CREATE_STREAM,
    STATE_QUIC_PROXY_CREATE_STREAM_COMPLETE,
    STATE_RESTART_WITH_AUTH,
    STATE_RESTART_WITH_AUTH_COMPLETE,
    STATE_NONE,
  };

  // Begins the tcp connection and the optional Http proxy tunnel.  If the
  // request is not immediately serviceable (likely), the request will return
  // ERR_IO_PENDING. An OK return from this function or the callback means
  // that the connection is established; ERR_PROXY_AUTH_REQUESTED means
  // that the tunnel needs authentication credentials, the socket will be
  // returned in this case, and must be released back to the pool; or
  // a standard net error code will be returned.
  int ConnectInternal() override;

  ProxyServer::Scheme GetProxyServerScheme() const;

  void OnIOComplete(int result);

  void RestartWithAuthCredentials();

  // Runs the state transition loop.
  int DoLoop(int result);

  // Determine if need to go through TCP or SSL path.
  int DoBeginConnect();
  // Connecting to HTTP Proxy
  int DoTransportConnect();
  int DoTransportConnectComplete(int result);
  // Connecting to HTTPS Proxy
  int DoSSLConnect();
  int DoSSLConnectComplete(int result);

  int DoHttpProxyConnect();
  int DoHttpProxyConnectComplete(int result);

  int DoSpdyProxyCreateStream();
  int DoSpdyProxyCreateStreamComplete(int result);

  int DoQuicProxyCreateSession();
  int DoQuicProxyCreateStream(int result);
  int DoQuicProxyCreateStreamComplete(int result);

  int DoRestartWithAuth();
  int DoRestartWithAuthComplete(int result);

  // ConnectJob implementation.
  void ChangePriorityInternal(RequestPriority priority) override;
  void OnTimedOutInternal() override;

  int HandleConnectResult(int result);

  void OnAuthChallenge();

  const HostPortPair& GetDestination();

  std::string GetUserAgent() const;

  SpdySessionKey CreateSpdySessionKey() const;

  scoped_refptr<HttpProxySocketParams> params_;

  scoped_refptr<SSLCertRequestInfo> ssl_cert_request_info_;

  State next_state_;

  bool has_restarted_;

  bool using_spdy_;
  NextProto negotiated_protocol_;

  // Set to true once a connection has been successfully established. Remains
  // true even if a new socket is being connected to retry with auth.
  bool has_established_connection_;

  std::unique_ptr<ConnectJob> nested_connect_job_;
  std::unique_ptr<ProxyClientSocket> transport_socket_;

  std::unique_ptr<SpdyStreamRequest> spdy_stream_request_;

  std::unique_ptr<QuicStreamRequest> quic_stream_request_;
  std::unique_ptr<QuicChromiumClientSession::Handle> quic_session_;

  scoped_refptr<HttpAuthController> http_auth_controller_;

  NetErrorDetails quic_net_error_details_;

  // Time when the connection to the proxy was started.
  base::TimeTicks connect_start_time_;

  base::WeakPtrFactory<HttpProxyConnectJob> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HttpProxyConnectJob);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PROXY_CONNECT_JOB_H_
