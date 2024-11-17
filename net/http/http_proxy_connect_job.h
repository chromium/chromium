// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PROXY_CONNECT_JOB_H_
#define NET_HTTP_HTTP_PROXY_CONNECT_JOB_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/request_priority.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_auth.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/socket/connect_job.h"
#include "net/socket/connect_job_params.h"
#include "net/socket/next_proto.h"
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
class QuicSessionRequest;

// HttpProxySocketParams only needs the socket params for one of the proxy
// types.  The other param must be NULL.  When using an HTTP proxy,
// `transport_params` must be set.  When using an HTTPS proxy, `ssl_params` must
// be set. When using a QUIC proxy, both must be `nullptr` but `quic_ssl_config`
// must be set.

class NET_EXPORT_PRIVATE HttpProxySocketParams
    : public base::RefCounted<HttpProxySocketParams> {
 public:
  // Construct an `HttpProxyConnectJob` over a transport or SSL connection
  // defined by the `ConnectJobParams`.
  HttpProxySocketParams(
      ConnectJobParams nested_params,
      const HostPortPair& endpoint,
      const ProxyChain& proxy_chain,
      size_t proxy_chain_index,
      bool tunnel,
      const NetworkTrafficAnnotationTag traffic_annotation,
      const NetworkAnonymizationKey& network_anonymization_key,
      SecureDnsPolicy secure_dns_policy);

  // Construct an `HttpProxyConnectJob` over a QUIC connection using the given
  // SSL config.
  HttpProxySocketParams(
      SSLConfig quic_ssl_config,
      const HostPortPair& endpoint,
      const ProxyChain& proxy_chain,
      size_t proxy_chain_index,
      bool tunnel,
      const NetworkTrafficAnnotationTag traffic_annotation,
      const NetworkAnonymizationKey& network_anonymization_key,
      SecureDnsPolicy secure_dns_policy);

  HttpProxySocketParams(const HttpProxySocketParams&) = delete;
  HttpProxySocketParams& operator=(const HttpProxySocketParams&) = delete;

  bool is_over_transport() const {
    return nested_params_ && nested_params_->is_transport();
  }
  bool is_over_ssl() const {
    return nested_params_ && nested_params_->is_ssl();
  }
  bool is_over_quic() const { return quic_ssl_config_.has_value(); }

  // Get the nested transport params, or fail if not `is_over_transport()`.
  const scoped_refptr<TransportSocketParams>& transport_params() const {
    return nested_params_->transport();
  }

  // Get the nested SSL params, or fail if not `is_over_ssl()`.
  const scoped_refptr<SSLSocketParams>& ssl_params() const {
    return nested_params_->ssl();
  }

  // Get the QUIC ssl config, or fail if not `is_over_quic()`.
  const std::optional<SSLConfig>& quic_ssl_config() const {
    return quic_ssl_config_;
  }

  const HostPortPair& endpoint() const { return endpoint_; }
  const ProxyChain& proxy_chain() const { return proxy_chain_; }
  const ProxyServer& proxy_server() const {
    return proxy_chain_.GetProxyServer(proxy_chain_index_);
  }
  size_t proxy_chain_index() const { return proxy_chain_index_; }
  bool tunnel() const { return tunnel_; }
  const NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
  }
  const NetworkTrafficAnnotationTag traffic_annotation() const {
    return traffic_annotation_;
  }
  SecureDnsPolicy secure_dns_policy() { return secure_dns_policy_; }

 private:
  friend class base::RefCounted<HttpProxySocketParams>;
  HttpProxySocketParams(
      std::optional<ConnectJobParams> nested_params,
      std::optional<SSLConfig> quic_ssl_config,
      const HostPortPair& endpoint,
      const ProxyChain& proxy_chain,
      size_t proxy_chain_index,
      bool tunnel,
      const NetworkTrafficAnnotationTag traffic_annotation,
      const NetworkAnonymizationKey& network_anonymization_key,
      SecureDnsPolicy secure_dns_policy);
  ~HttpProxySocketParams();

  const std::optional<ConnectJobParams> nested_params_;
  const std::optional<SSLConfig> quic_ssl_config_;
  const HostPortPair endpoint_;
  const ProxyChain proxy_chain_;
  const size_t proxy_chain_index_;
  const bool tunnel_;
  const NetworkAnonymizationKey network_anonymization_key_;
  const NetworkTrafficAnnotationTag traffic_annotation_;
  const SecureDnsPolicy secure_dns_policy_;
};

// HttpProxyConnectJob optionally establishes a tunnel through the proxy
// server after connecting the underlying transport socket.
class NET_EXPORT_PRIVATE HttpProxyConnectJob : public ConnectJob,
                                               public ConnectJob::Delegate {
 public:
  class NET_EXPORT_PRIVATE Factory {
   public:
    Factory() = default;
    virtual ~Factory() = default;

    virtual std::unique_ptr<HttpProxyConnectJob> Create(
        RequestPriority priority,
        const SocketTag& socket_tag,
        const CommonConnectJobParams* common_connect_job_params,
        scoped_refptr<HttpProxySocketParams> params,
        ConnectJob::Delegate* delegate,
        const NetLogWithSource* net_log);
  };

  HttpProxyConnectJob(RequestPriority priority,
                      const SocketTag& socket_tag,
                      const CommonConnectJobParams* common_connect_job_params,
                      scoped_refptr<HttpProxySocketParams> params,
                      ConnectJob::Delegate* delegate,
                      const NetLogWithSource* net_log);

  HttpProxyConnectJob(const HttpProxyConnectJob&) = delete;
  HttpProxyConnectJob& operator=(const HttpProxyConnectJob&) = delete;

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
  ResolveErrorInfo GetResolveErrorInfo() const override;
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

  enum class HttpConnectResult {
    kSuccess,
    kError,
    kTimedOut,
  };

  // Emit a Net.HttpProxy.ConnectLatency.* metric. This is used both by this
  // class and by QuicSessionPool, which handles QUIC tunnels which will carry
  // QUIC.
  static void EmitConnectLatency(NextProto http_version,
                                 ProxyServer::Scheme scheme,
                                 HttpConnectResult result,
                                 base::TimeDelta latency);

 private:
  enum State {
    STATE_BEGIN_CONNECT,
    STATE_TRANSPORT_CONNECT,
    STATE_TRANSPORT_CONNECT_COMPLETE,
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
  // Connecting to HTTP or HTTPS Proxy
  int DoTransportConnect();
  int DoTransportConnectComplete(int result);

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

  void OnAuthChallenge();

  const HostPortPair& GetDestination() const;

  std::string GetUserAgent() const;

  SpdySessionKey CreateSpdySessionKey() const;

  scoped_refptr<HttpProxySocketParams> params_;

  scoped_refptr<SSLCertRequestInfo> ssl_cert_request_info_;

  State next_state_ = STATE_NONE;

  bool has_restarted_ = false;

  // Set to true once a connection has been successfully established. Remains
  // true even if a new socket is being connected to retry with auth.
  bool has_established_connection_ = false;

  ResolveErrorInfo resolve_error_info_;

  std::unique_ptr<ConnectJob> nested_connect_job_;
  std::unique_ptr<ProxyClientSocket> transport_socket_;

  std::unique_ptr<SpdyStreamRequest> spdy_stream_request_;

  std::unique_ptr<QuicSessionRequest> quic_session_request_;
  std::unique_ptr<QuicChromiumClientSession::Handle> quic_session_;

  scoped_refptr<HttpAuthController> http_auth_controller_;

  NetErrorDetails quic_net_error_details_;

  // Time when the connection to the proxy was started.
  base::TimeTicks connect_start_time_;

  base::WeakPtrFactory<HttpProxyConnectJob> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PROXY_CONNECT_JOB_H_
