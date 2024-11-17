// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_stream_factory.h"
#include "net/net_buildflags.h"
#include "net/quic/quic_session_pool.h"
#include "net/socket/connect_job.h"
#include "net/socket/next_proto.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_client_session_cache.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"

namespace base {
class Value;
}

namespace net {

class CertVerifier;
class ClientSocketFactory;
class ClientSocketPool;
class ClientSocketPoolManager;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpNetworkSessionPeer;
class HttpResponseBodyDrainer;
class HttpServerProperties;
class HttpStreamPool;
class HttpUserAgentSettings;
class NetLog;
#if BUILDFLAG(ENABLE_REPORTING)
class NetworkErrorLoggingService;
#endif
class NetworkQualityEstimator;
class ProxyDelegate;
class ProxyResolutionService;
class ProxyChain;
class QuicCryptoClientStreamFactory;
#if BUILDFLAG(ENABLE_REPORTING)
class ReportingService;
#endif
class SCTAuditingDelegate;
class SocketPerformanceWatcherFactory;
class SSLConfigService;
class TransportSecurityState;

// Specifies the maximum HPACK dynamic table size the server is allowed to set.
const uint32_t kSpdyMaxHeaderTableSize = 64 * 1024;

// The maximum size of header list that the server is allowed to send.
const uint32_t kSpdyMaxHeaderListSize = 256 * 1024;

// Self-contained structure with all the simple configuration options
// supported by the HttpNetworkSession.
struct NET_EXPORT HttpNetworkSessionParams {
  HttpNetworkSessionParams();
  HttpNetworkSessionParams(const HttpNetworkSessionParams& other);
  ~HttpNetworkSessionParams();

  HostMappingRules host_mapping_rules;
  bool ignore_certificate_errors = false;
  uint16_t testing_fixed_http_port = 0;
  uint16_t testing_fixed_https_port = 0;
  bool enable_user_alternate_protocol_ports = false;

  // Use SPDY ping frames to test for connection health after idle.
  bool enable_spdy_ping_based_connection_checking = true;
  bool enable_http2 = true;
  size_t spdy_session_max_recv_window_size;
  // Maximum number of capped frames that can be queued at any time.
  int spdy_session_max_queued_capped_frames;
  // Whether SPDY pools should mark sessions as going away upon relevant network
  // changes (instead of closing them). Default value is OS specific.
  // For OSs that terminate TCP connections upon relevant network changes,
  // attempt to preserve active streams by marking all sessions as going
  // away, rather than explicitly closing them. Streams may still fail due
  // to a generated TCP reset.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
  bool spdy_go_away_on_ip_change = true;
#else
  bool spdy_go_away_on_ip_change = false;
#endif
  // HTTP/2 connection settings.
  // Unknown settings will still be sent to the server.
  // Might contain unknown setting identifiers from a predefined set that
  // servers are supposed to ignore, see
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  // The same setting will be sent on every connection to prevent the retry
  // logic from hiding broken servers.
  spdy::SettingsMap http2_settings;
  // If true, a setting parameter with reserved identifier will be sent in every
  // initial SETTINGS frame, see
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  // The setting identifier and value will be drawn independently for each
  // connection to prevent tracking of the client.
  bool enable_http2_settings_grease = false;
  // If set, an HTTP/2 frame with a reserved frame type will be sent after
  // every HTTP/2 SETTINGS frame and before every HTTP/2 DATA frame.
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  // The same frame will be sent out on all connections to prevent the retry
  // logic from hiding broken servers.
  std::optional<SpdySessionPool::GreasedHttp2Frame> greased_http2_frame;
  // If set, the HEADERS frame carrying a request without body will not have
  // the END_STREAM flag set.  The stream will be closed by a subsequent empty
  // DATA frame with END_STREAM.  Does not affect bidirectional or proxy
  // streams.
  // If unset, the HEADERS frame will have the END_STREAM flag set on.
  // This is useful in conjunction with |greased_http2_frame| so that a frame
  // of reserved type can be sent out even on requests without a body.
  bool http2_end_stream_with_data_frame = false;
  // Source of time for SPDY connections.
  SpdySessionPool::TimeFunc time_func;
  // Whether to enable HTTP/2 Alt-Svc entries.
  bool enable_http2_alternative_service = false;

  // Enables 0-RTT support.
  bool enable_early_data;

  // Enables QUIC support.
  bool enable_quic = true;

  // If non-empty, QUIC will only be spoken to hosts in this list.
  base::flat_set<std::string> quic_host_allowlist;

  // If true, idle sockets won't be closed when memory pressure happens.
  bool disable_idle_sockets_close_on_memory_pressure = false;

  bool key_auth_cache_server_entries_by_network_anonymization_key = false;

  // If true, enable sending PRIORITY_UPDATE frames until SETTINGS frame
  // arrives.  After SETTINGS frame arrives, do not send PRIORITY_UPDATE
  // frames any longer if SETTINGS_DEPRECATE_HTTP2_PRIORITIES is missing or
  // has zero 0, but continue and also stop sending HTTP/2-style priority
  // information in HEADERS frames and PRIORITY frames if it has value 1.
  bool enable_priority_update = false;

  // If true, objects used by a HttpNetworkTransaction are asked not to perform
  // disruptive work after there has been an IP address change (which usually
  // means that the "default network" has possibly changed).
  // This is currently used by HttpNetworkSessions that are bound to a specific
  // network: for these, the underlying network does never change, even if the
  // default network does (hence underlying objects should not drop their
  // state).
  bool ignore_ip_address_changes = false;

  // Whether to use the ALPN information in the DNS HTTPS record.
  bool use_dns_https_svcb_alpn = false;
};

// Structure with pointers to the dependencies of the HttpNetworkSession.
// These objects must all outlive the HttpNetworkSession.
struct NET_EXPORT HttpNetworkSessionContext {
  HttpNetworkSessionContext();
  HttpNetworkSessionContext(const HttpNetworkSessionContext& other);
  ~HttpNetworkSessionContext();

  raw_ptr<ClientSocketFactory> client_socket_factory;
  raw_ptr<HostResolver> host_resolver;
  raw_ptr<CertVerifier> cert_verifier;
  raw_ptr<TransportSecurityState> transport_security_state;
  raw_ptr<SCTAuditingDelegate> sct_auditing_delegate;
  raw_ptr<ProxyResolutionService> proxy_resolution_service;
  raw_ptr<ProxyDelegate> proxy_delegate;
  raw_ptr<const HttpUserAgentSettings> http_user_agent_settings;
  raw_ptr<SSLConfigService> ssl_config_service;
  raw_ptr<HttpAuthHandlerFactory> http_auth_handler_factory;
  raw_ptr<HttpServerProperties> http_server_properties;
  raw_ptr<NetLog> net_log;
  raw_ptr<SocketPerformanceWatcherFactory> socket_performance_watcher_factory;
  raw_ptr<NetworkQualityEstimator> network_quality_estimator;
  raw_ptr<QuicContext> quic_context;
#if BUILDFLAG(ENABLE_REPORTING)
  raw_ptr<ReportingService> reporting_service;
  raw_ptr<NetworkErrorLoggingService> network_error_logging_service;
#endif

  // Optional factory to use for creating QuicCryptoClientStreams.
  raw_ptr<QuicCryptoClientStreamFactory> quic_crypto_client_stream_factory;
};

// This class holds session objects used by HttpNetworkTransaction objects.
class NET_EXPORT HttpNetworkSession {
 public:
  enum SocketPoolType {
    NORMAL_SOCKET_POOL,
    WEBSOCKET_SOCKET_POOL,
    NUM_SOCKET_POOL_TYPES
  };

  HttpNetworkSession(const HttpNetworkSessionParams& params,
                     const HttpNetworkSessionContext& context);
  ~HttpNetworkSession();

  HttpAuthCache* http_auth_cache() { return &http_auth_cache_; }
  SSLClientContext* ssl_client_context() { return &ssl_client_context_; }

  void StartResponseDrainer(std::unique_ptr<HttpResponseBodyDrainer> drainer);

  // Removes the drainer from the session.
  void RemoveResponseDrainer(HttpResponseBodyDrainer* drainer);

  // Returns the socket pool of the given type for use with the specified
  // ProxyChain. Use ProxyChain::Direct() to get the pool for use with direct
  // connections.
  ClientSocketPool* GetSocketPool(SocketPoolType pool_type,
                                  const ProxyChain& proxy_chain);

  CertVerifier* cert_verifier() { return cert_verifier_; }
  ProxyResolutionService* proxy_resolution_service() {
    return proxy_resolution_service_;
  }
  SSLConfigService* ssl_config_service() { return ssl_config_service_; }
  WebSocketEndpointLockManager* websocket_endpoint_lock_manager() {
    return &websocket_endpoint_lock_manager_;
  }
  SpdySessionPool* spdy_session_pool() { return &spdy_session_pool_; }
  QuicSessionPool* quic_session_pool() { return &quic_session_pool_; }
  HttpAuthHandlerFactory* http_auth_handler_factory() {
    return http_auth_handler_factory_;
  }
  HttpServerProperties* http_server_properties() {
    return http_server_properties_;
  }
  HttpStreamPool* http_stream_pool() { return http_stream_pool_.get(); }
  HttpStreamFactory* http_stream_factory() {
    return http_stream_factory_.get();
  }
  NetLog* net_log() { return net_log_; }
  HostResolver* host_resolver() { return host_resolver_; }
#if BUILDFLAG(ENABLE_REPORTING)
  ReportingService* reporting_service() const { return reporting_service_; }
  NetworkErrorLoggingService* network_error_logging_service() const {
    return network_error_logging_service_;
  }
#endif

  // Creates a Value summary of the state of the socket pools.
  base::Value SocketPoolInfoToValue() const;

  // Creates a Value summary of the state of the SPDY sessions.
  std::unique_ptr<base::Value> SpdySessionPoolInfoToValue() const;

  // Creates a Value summary of the state of the QUIC sessions and
  // configuration.
  base::Value QuicInfoToValue() const;

  void CloseAllConnections(int net_error, const char* net_log_reason_utf8);
  void CloseIdleConnections(const char* net_log_reason_utf8);

  // Returns the original Params used to construct this session.
  const HttpNetworkSessionParams& params() const { return params_; }
  // Returns the original Context used to construct this session.
  const HttpNetworkSessionContext& context() const { return context_; }

  // Returns protocols to be used with ALPN.
  const NextProtoVector& GetAlpnProtos() const { return next_protos_; }

  // Returns ALPS data to be sent to server for each NextProto.
  // Data might be empty.
  const SSLConfig::ApplicationSettings& GetApplicationSettings() const {
    return application_settings_;
  }

  // Evaluates if QUIC is enabled for new streams.
  bool IsQuicEnabled() const;

  // Disable QUIC for new streams.
  void DisableQuic();

  // Returns true when QUIC is forcibly used for `destination`.
  bool ShouldForceQuic(const url::SchemeHostPort& destination,
                       const ProxyInfo& proxy_info,
                       bool is_websocket);

  // Ignores certificate errors on new connection attempts.
  void IgnoreCertificateErrorsForTesting();

  // Clear the SSL session cache.
  void ClearSSLSessionCache();

  // Returns a CommonConnectJobParams that references the NetworkSession's
  // components. If |for_websockets| is true, the Params'
  // |websocket_endpoint_lock_manager| field will be populated. Otherwise, it
  // will be nullptr.
  CommonConnectJobParams CreateCommonConnectJobParams(
      bool for_websockets = false);

 private:
  friend class HttpNetworkSessionPeer;

  ClientSocketPoolManager* GetSocketPoolManager(SocketPoolType pool_type);

  // Flush sockets on low memory notifications callback.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  const raw_ptr<NetLog> net_log_;
  const raw_ptr<HttpServerProperties> http_server_properties_;
  const raw_ptr<CertVerifier> cert_verifier_;
  const raw_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  const raw_ptr<HostResolver> host_resolver_;

#if BUILDFLAG(ENABLE_REPORTING)
  const raw_ptr<ReportingService> reporting_service_;
  const raw_ptr<NetworkErrorLoggingService> network_error_logging_service_;
#endif
  const raw_ptr<ProxyResolutionService> proxy_resolution_service_;
  const raw_ptr<SSLConfigService> ssl_config_service_;

  HttpAuthCache http_auth_cache_;
  SSLClientSessionCache ssl_client_session_cache_;
  SSLClientContext ssl_client_context_;
  WebSocketEndpointLockManager websocket_endpoint_lock_manager_;
  std::unique_ptr<ClientSocketPoolManager> normal_socket_pool_manager_;
  std::unique_ptr<ClientSocketPoolManager> websocket_socket_pool_manager_;
  QuicSessionPool quic_session_pool_;
  // `http_stream_pool_` needs to outlive `spdy_session_pool_` because it owns
  // SpdySessions, which own HttpStreamHandle and handles are owned by
  // `http_stream_pool_`.
  // `http_stream_pool_` needs to be destroyed before `quic_session_pool_`
  // because an HttpStreamPool::QuicTask, which is owned by `http_stream_pool_`,
  // may have a QuicSessionAttempt that must be destroyed before
  // `quic_session_pool_`.
  std::unique_ptr<HttpStreamPool> http_stream_pool_;
  SpdySessionPool spdy_session_pool_;
  std::unique_ptr<HttpStreamFactory> http_stream_factory_;
  std::set<std::unique_ptr<HttpResponseBodyDrainer>, base::UniquePtrComparator>
      response_drainers_;
  NextProtoVector next_protos_;
  SSLConfig::ApplicationSettings application_settings_;

  HttpNetworkSessionParams params_;
  HttpNetworkSessionContext context_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_H_
