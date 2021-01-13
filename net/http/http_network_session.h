// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "build/buildflag.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_stream_factory.h"
#include "net/net_buildflags.h"
#include "net/quic/quic_stream_factory.h"
#include "net/socket/connect_job.h"
#include "net/socket/next_proto.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_client_session_cache.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"

namespace base {
class Value;
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace net {

class CTPolicyEnforcer;
class CertVerifier;
class ClientSocketFactory;
class ClientSocketPool;
class ClientSocketPoolManager;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpNetworkSessionPeer;
class HttpResponseBodyDrainer;
class HttpServerProperties;
class HttpUserAgentSettings;
class NetLog;
#if BUILDFLAG(ENABLE_REPORTING)
class NetworkErrorLoggingService;
#endif
class NetworkQualityEstimator;
class ProxyDelegate;
class ProxyResolutionService;
class ProxyServer;
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

// Specifies the maximum concurrent streams server could send (via push).
const uint32_t kSpdyMaxConcurrentPushedStreams = 1000;

// This class holds session objects used by HttpNetworkTransaction objects.
class NET_EXPORT HttpNetworkSession {
 public:
  // Self-contained structure with all the simple configuration options
  // supported by the HttpNetworkSession.
  struct NET_EXPORT Params {
    Params();
    Params(const Params& other);
    ~Params();

    bool enable_server_push_cancellation;
    HostMappingRules host_mapping_rules;
    bool ignore_certificate_errors;
    uint16_t testing_fixed_http_port;
    uint16_t testing_fixed_https_port;
    bool enable_user_alternate_protocol_ports;

    // Use SPDY ping frames to test for connection health after idle.
    bool enable_spdy_ping_based_connection_checking;
    bool enable_http2;
    size_t spdy_session_max_recv_window_size;
    // Maximum number of capped frames that can be queued at any time.
    int spdy_session_max_queued_capped_frames;
    // HTTP/2 connection settings.
    // Unknown settings will still be sent to the server.
    // Might contain unknown setting identifiers from a predefined set that
    // servers are supposed to ignore, see
    // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
    // The same setting will be sent on every connection to prevent the retry
    // logic from hiding broken servers.
    spdy::SettingsMap http2_settings;
    // If set, an HTTP/2 frame with a reserved frame type will be sent after
    // every HTTP/2 SETTINGS frame and before every HTTP/2 DATA frame.
    // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
    // The same frame will be sent out on all connections to prevent the retry
    // logic from hiding broken servers.
    base::Optional<SpdySessionPool::GreasedHttp2Frame> greased_http2_frame;
    // If set, the HEADERS frame carrying a request without body will not have
    // the END_STREAM flag set.  The stream will be closed by a subsequent empty
    // DATA frame with END_STREAM.  Does not affect bidirectional or proxy
    // streams.
    // If unset, the HEADERS frame will have the END_STREAM flag set on.
    // This is useful in conjuction with |greased_http2_frame| so that a frame
    // of reserved type can be sent out even on requests without a body.
    bool http2_end_stream_with_data_frame;
    // Source of time for SPDY connections.
    SpdySessionPool::TimeFunc time_func;
    // Whether to enable HTTP/2 Alt-Svc entries.
    bool enable_http2_alternative_service;
    // Whether to enable Websocket over HTTP/2.
    bool enable_websocket_over_http2;

    // Enables 0-RTT support.
    bool enable_early_data;

    // Enables QUIC support.
    bool enable_quic;

    // If true, HTTPS URLs can be sent to QUIC proxies.
    bool enable_quic_proxies_for_https_urls;

    // If non-empty, QUIC will only be spoken to hosts in this list.
    base::flat_set<std::string> quic_host_allowlist;

    // If true, idle sockets won't be closed when memory pressure happens.
    bool disable_idle_sockets_close_on_memory_pressure;

    bool key_auth_cache_server_entries_by_network_isolation_key;

    // If true, enable sending PRIORITY_UPDATE frames until SETTINGS frame
    // arrives.  After SETTINGS frame arrives, do not send PRIORITY_UPDATE
    // frames any longer if SETTINGS_DEPRECATE_HTTP2_PRIORITIES is missing or
    // has zero 0, but continue and also stop sending HTTP/2-style priority
    // information in HEADERS frames and PRIORITY frames if it has value 1.
    bool enable_priority_update;
  };

  // Structure with pointers to the dependencies of the HttpNetworkSession.
  // These objects must all outlive the HttpNetworkSession.
  struct NET_EXPORT Context {
    Context();
    Context(const Context& other);
    ~Context();

    ClientSocketFactory* client_socket_factory;
    HostResolver* host_resolver;
    CertVerifier* cert_verifier;
    TransportSecurityState* transport_security_state;
    CTPolicyEnforcer* ct_policy_enforcer;
    SCTAuditingDelegate* sct_auditing_delegate;
    ProxyResolutionService* proxy_resolution_service;
    ProxyDelegate* proxy_delegate;
    const HttpUserAgentSettings* http_user_agent_settings;
    SSLConfigService* ssl_config_service;
    HttpAuthHandlerFactory* http_auth_handler_factory;
    HttpServerProperties* http_server_properties;
    NetLog* net_log;
    SocketPerformanceWatcherFactory* socket_performance_watcher_factory;
    NetworkQualityEstimator* network_quality_estimator;
    QuicContext* quic_context;
#if BUILDFLAG(ENABLE_REPORTING)
    ReportingService* reporting_service;
    NetworkErrorLoggingService* network_error_logging_service;
#endif

    // Optional factory to use for creating QuicCryptoClientStreams.
    QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory;
  };

  enum SocketPoolType {
    NORMAL_SOCKET_POOL,
    WEBSOCKET_SOCKET_POOL,
    NUM_SOCKET_POOL_TYPES
  };

  HttpNetworkSession(const Params& params, const Context& context);
  ~HttpNetworkSession();

  HttpAuthCache* http_auth_cache() { return &http_auth_cache_; }
  SSLClientContext* ssl_client_context() { return &ssl_client_context_; }

  void AddResponseDrainer(std::unique_ptr<HttpResponseBodyDrainer> drainer);

  // Removes the drainer from the session. Does not dispose of it.
  void RemoveResponseDrainer(HttpResponseBodyDrainer* drainer);

  // Returns the socket pool of the given type for use with the specified
  // ProxyServer. Use ProxyServer::Direct() to get the pool for use with direct
  // connections.
  ClientSocketPool* GetSocketPool(SocketPoolType pool_type,
                                  const ProxyServer& proxy_server);

  CertVerifier* cert_verifier() { return cert_verifier_; }
  ProxyResolutionService* proxy_resolution_service() {
    return proxy_resolution_service_;
  }
  SSLConfigService* ssl_config_service() { return ssl_config_service_; }
  WebSocketEndpointLockManager* websocket_endpoint_lock_manager() {
    return &websocket_endpoint_lock_manager_;
  }
  SpdySessionPool* spdy_session_pool() { return &spdy_session_pool_; }
  QuicStreamFactory* quic_stream_factory() { return &quic_stream_factory_; }
  HttpAuthHandlerFactory* http_auth_handler_factory() {
    return http_auth_handler_factory_;
  }
  HttpServerProperties* http_server_properties() {
    return http_server_properties_;
  }
  HttpStreamFactory* http_stream_factory() {
    return http_stream_factory_.get();
  }
  NetLog* net_log() {
    return net_log_;
  }
  HostResolver* host_resolver() { return host_resolver_; }
#if BUILDFLAG(ENABLE_REPORTING)
  ReportingService* reporting_service() const { return reporting_service_; }
  NetworkErrorLoggingService* network_error_logging_service() const {
    return network_error_logging_service_;
  }
#endif

  // Creates a Value summary of the state of the socket pools.
  std::unique_ptr<base::Value> SocketPoolInfoToValue() const;

  // Creates a Value summary of the state of the SPDY sessions.
  std::unique_ptr<base::Value> SpdySessionPoolInfoToValue() const;

  // Creates a Value summary of the state of the QUIC sessions and
  // configuration.
  base::Value QuicInfoToValue() const;

  void CloseAllConnections(int net_error, const char* net_log_reason_utf8);
  void CloseIdleConnections(const char* net_log_reason_utf8);

  // Returns the original Params used to construct this session.
  const Params& params() const { return params_; }
  // Returns the original Context used to construct this session.
  const Context& context() const { return context_; }

  void SetServerPushDelegate(std::unique_ptr<ServerPushDelegate> push_delegate);

  // Populates |*alpn_protos| with protocols to be used with ALPN.
  void GetAlpnProtos(NextProtoVector* alpn_protos) const;

  // Populates |server_config| and |proxy_config| based on this session.
  void GetSSLConfig(SSLConfig* server_config, SSLConfig* proxy_config) const;

  // Dumps memory allocation stats. |parent_dump_absolute_name| is the name
  // used by the parent MemoryAllocatorDump in the memory dump hierarchy.
  void DumpMemoryStats(base::trace_event::ProcessMemoryDump* pmd,
                       const std::string& parent_absolute_name) const;

  // Evaluates if QUIC is enabled for new streams.
  bool IsQuicEnabled() const;

  // Disable QUIC for new streams.
  void DisableQuic();

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

  NetLog* const net_log_;
  HttpServerProperties* const http_server_properties_;
  CertVerifier* const cert_verifier_;
  HttpAuthHandlerFactory* const http_auth_handler_factory_;
  HostResolver* const host_resolver_;

#if BUILDFLAG(ENABLE_REPORTING)
  ReportingService* const reporting_service_;
  NetworkErrorLoggingService* const network_error_logging_service_;
#endif
  ProxyResolutionService* const proxy_resolution_service_;
  SSLConfigService* const ssl_config_service_;

  HttpAuthCache http_auth_cache_;
  SSLClientSessionCache ssl_client_session_cache_;
  SSLClientContext ssl_client_context_;
  WebSocketEndpointLockManager websocket_endpoint_lock_manager_;
  std::unique_ptr<ClientSocketPoolManager> normal_socket_pool_manager_;
  std::unique_ptr<ClientSocketPoolManager> websocket_socket_pool_manager_;
  std::unique_ptr<ServerPushDelegate> push_delegate_;
  QuicStreamFactory quic_stream_factory_;
  SpdySessionPool spdy_session_pool_;
  std::unique_ptr<HttpStreamFactory> http_stream_factory_;
  std::map<HttpResponseBodyDrainer*, std::unique_ptr<HttpResponseBodyDrainer>>
      response_drainers_;
  NextProtoVector next_protos_;

  Params params_;
  Context context_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_H_
