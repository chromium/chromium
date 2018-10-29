// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_POOL_H_
#define NET_SPDY_SPDY_SESSION_POOL_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_server.h"
#include "net/cert/cert_database.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/spdy/http2_push_promise_index.h"
#include "net/spdy/server_push_delegate.h"
#include "net/spdy/spdy_session_key.h"
#include "net/ssl/ssl_config_service.h"
#include "net/third_party/quic/core/quic_versions.h"
#include "net/third_party/spdy/core/spdy_protocol.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace net {

class ClientSocketHandle;
class HostResolver;
class HttpServerProperties;
class HttpStreamRequest;
class NetLogWithSource;
class NetworkQualityEstimator;
class SpdySession;
class TransportSecurityState;

// This is a very simple pool for open SpdySessions.
class NET_EXPORT SpdySessionPool
    : public NetworkChangeNotifier::IPAddressObserver,
      public SSLConfigService::Observer,
      public CertDatabase::Observer {
 public:
  typedef base::TimeTicks (*TimeFunc)(void);

  // Struct to hold randomly generated frame parameters to be used for sending
  // frames on the wire to "grease" frame type.  Frame type has to be one of
  // the reserved values defined in
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  struct GreasedHttp2Frame {
    uint8_t type;
    uint8_t flags;
    std::string payload;
  };

  SpdySessionPool(
      HostResolver* host_resolver,
      SSLConfigService* ssl_config_service,
      HttpServerProperties* http_server_properties,
      TransportSecurityState* transport_security_state,
      const quic::QuicTransportVersionVector& quic_supported_versions,
      bool enable_ping_based_connection_checking,
      bool support_ietf_format_quic_altsvc,
      size_t session_max_recv_window_size,
      const spdy::SettingsMap& initial_settings,
      const base::Optional<GreasedHttp2Frame>& greased_http2_frame,
      SpdySessionPool::TimeFunc time_func,
      NetworkQualityEstimator* network_quality_estimator);
  ~SpdySessionPool() override;

  // In the functions below, a session is "available" if this pool has
  // a reference to it and there is some SpdySessionKey for which
  // FindAvailableSession() will return it. A session is "unavailable"
  // if this pool has a reference to it but it won't be returned by
  // FindAvailableSession() for any SpdySessionKey; for example, this
  // can happen when a session receives a GOAWAY frame and is still
  // processing existing streams.

  // Create a new SPDY session from an existing socket.  There must
  // not already be a session for the given key.
  //
  // Returns the new SpdySession. Note that the SpdySession begins reading from
  // |connection| on a subsequent event loop iteration, so it may be closed
  // immediately afterwards if the first read of |connection| fails.
  base::WeakPtr<SpdySession> CreateAvailableSessionFromSocket(
      const SpdySessionKey& key,
      bool is_trusted_proxy,
      std::unique_ptr<ClientSocketHandle> connection,
      const NetLogWithSource& net_log);

  // If there is an available session for |key|, return it.
  // Otherwise if there is a session to pool to based on IP address:
  //   * if |enable_ip_based_pooling == true|,
  //     then mark it as available for |key| and return it;
  //   * if |enable_ip_based_pooling == false|,
  //     then remove it from the available sessions, and return nullptr.
  // Otherwise return nullptr.
  base::WeakPtr<SpdySession> FindAvailableSession(
      const SpdySessionKey& key,
      bool enable_ip_based_pooling,
      bool is_websocket,
      const NetLogWithSource& net_log);

  // Remove all mappings and aliases for the given session, which must
  // still be available. Except for in tests, this must be called by
  // the given session itself.
  void MakeSessionUnavailable(
      const base::WeakPtr<SpdySession>& available_session);

  // Removes an unavailable session from the pool.  Except for in
  // tests, this must be called by the given session itself.
  void RemoveUnavailableSession(
      const base::WeakPtr<SpdySession>& unavailable_session);

  // Note that the next three methods close sessions, potentially notifing
  // delegates of error or synchronously invoking callbacks, which might trigger
  // retries, thus opening new sessions.

  // Close only the currently existing SpdySessions with |error|.
  // Let any new ones created while this method is running continue to
  // live.
  void CloseCurrentSessions(Error error);

  // Close only the currently existing SpdySessions that are idle.
  // Let any new ones created while this method is running continue to
  // live.
  void CloseCurrentIdleSessions();

  // Repeatedly close all SpdySessions until all of them (including new ones
  // created in the process of closing the current ones, and new ones created in
  // the process of closing those new ones, etc.) are unavailable.
  void CloseAllSessions();

  // Creates a Value summary of the state of the spdy session pool.
  std::unique_ptr<base::Value> SpdySessionPoolInfoToValue() const;

  HttpServerProperties* http_server_properties() {
    return http_server_properties_;
  }

  Http2PushPromiseIndex* push_promise_index() { return &push_promise_index_; }

  void set_server_push_delegate(ServerPushDelegate* push_delegate) {
    push_delegate_ = push_delegate;
  }

  // NetworkChangeNotifier::IPAddressObserver methods:

  // We flush all idle sessions and release references to the active ones so
  // they won't get re-used.  The active ones will either complete successfully
  // or error out due to the IP address change.
  void OnIPAddressChanged() override;

  // SSLConfigService::Observer methods:

  // We perform the same flushing as described above when SSL settings change.
  void OnSSLConfigChanged() override;

  // CertDatabase::Observer methods:

  // We perform the same flushing as described above when certificate database
  // is changed.
  void OnCertDBChanged() override;

  void DumpMemoryStats(base::trace_event::ProcessMemoryDump* pmd,
                       const std::string& parent_dump_absolute_name) const;

  // Called when a SpdySession is ready. It will find appropriate Requests and
  // fulfill them.
  void OnNewSpdySessionReady(const base::WeakPtr<SpdySession>& spdy_session,
                             const SSLConfig& used_ssl_config,
                             const ProxyInfo& used_proxy_info,
                             bool was_alpn_negotiated,
                             NextProto negotiated_protocol,
                             bool using_spdy,
                             NetLogSource source_dependency);

  // Called when a HttpStreamRequest is started with |spdy_session_key|.
  // Returns true if the request should continue. Returns false if the request
  // should wait until |callback| is invoked before continuing.
  bool StartRequest(const SpdySessionKey& spdy_session_key,
                    const base::Closure& callback);

  // Resumes pending requests with |spdy_session_key|.
  void ResumePendingRequests(const SpdySessionKey& spdy_session_key);

  // Adds |request| to |spdy_session_request_map_| under |spdy_session_key| Key.
  // Sets |spdy_session_key| as |request|'s SpdySessionKey.
  void AddRequestToSpdySessionRequestMap(const SpdySessionKey& spdy_session_key,
                                         HttpStreamRequest* request);

  // Removes |request| from |spdy_session_request_map_|. No-op if |request| does
  // not have a SpdySessionKey.
  void RemoveRequestFromSpdySessionRequestMap(HttpStreamRequest* request);

  void set_network_quality_estimator(
      NetworkQualityEstimator* network_quality_estimator) {
    network_quality_estimator_ = network_quality_estimator;
  }

 private:
  friend class SpdySessionPoolPeer;  // For testing.

  typedef std::set<HttpStreamRequest*> RequestSet;
  typedef std::map<SpdySessionKey, RequestSet> SpdySessionRequestMap;
  typedef std::set<SpdySession*> SessionSet;
  typedef std::vector<base::WeakPtr<SpdySession> > WeakSessionList;
  typedef std::map<SpdySessionKey, base::WeakPtr<SpdySession> >
      AvailableSessionMap;
  typedef std::multimap<IPEndPoint, SpdySessionKey> AliasMap;

  // Returns true iff |session| is in |available_sessions_|.
  bool IsSessionAvailable(const base::WeakPtr<SpdySession>& session) const;

  // Map the given key to the given session. There must not already be
  // a mapping for |key|.
  void MapKeyToAvailableSession(const SpdySessionKey& key,
                                const base::WeakPtr<SpdySession>& session);

  // Returns an iterator into |available_sessions_| for the given key,
  // which may be equal to |available_sessions_.end()|.
  AvailableSessionMap::iterator LookupAvailableSessionByKey(
      const SpdySessionKey& key);

  // Remove the mapping of the given key, which must exist.
  void UnmapKey(const SpdySessionKey& key);

  // Remove all aliases for |key| from the aliases table.
  void RemoveAliases(const SpdySessionKey& key);

  // Get a copy of the current sessions as a list of WeakPtrs. Used by
  // CloseCurrentSessionsHelper() below.
  WeakSessionList GetCurrentSessions() const;

  // Close only the currently existing SpdySessions with |error|.  Let
  // any new ones created while this method is running continue to
  // live. If |idle_only| is true only idle sessions are closed.
  void CloseCurrentSessionsHelper(Error error,
                                  const std::string& description,
                                  bool idle_only);

  HttpServerProperties* http_server_properties_;

  TransportSecurityState* transport_security_state_;

  // The set of all sessions. This is a superset of the sessions in
  // |available_sessions_|.
  //
  // |sessions_| owns all its SpdySession objects.
  SessionSet sessions_;

  // This is a map of available sessions by key. A session may appear
  // more than once in this map if it has aliases.
  AvailableSessionMap available_sessions_;

  // A map of IPEndPoint aliases for sessions.
  AliasMap aliases_;

  // The index of all unclaimed pushed streams of all SpdySessions in this pool.
  Http2PushPromiseIndex push_promise_index_;

  SSLConfigService* const ssl_config_service_;
  HostResolver* const resolver_;

  // Versions of QUIC which may be used.
  const quic::QuicTransportVersionVector quic_supported_versions_;

  // Defaults to true. May be controlled via SpdySessionPoolPeer for tests.
  bool enable_sending_initial_data_;
  bool enable_ping_based_connection_checking_;

  // If true, alt-svc headers advertising QUIC in IETF format will be supported.
  bool support_ietf_format_quic_altsvc_;

  size_t session_max_recv_window_size_;

  // Settings that are sent in the initial SETTINGS frame
  // (if |enable_sending_initial_data_| is true),
  // and also control SpdySession parameters like initial receive window size
  // and maximum HPACK dynamic table size.
  const spdy::SettingsMap initial_settings_;

  // If set, an HTTP/2 frame with a reserved frame type will be sent after every
  // valid HTTP/2 frame.  See
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  const base::Optional<GreasedHttp2Frame> greased_http2_frame_;

  // TODO(xunjieli): Merge these two.
  SpdySessionRequestMap spdy_session_request_map_;
  typedef std::map<SpdySessionKey, std::list<base::Closure>>
      SpdySessionPendingRequestMap;
  SpdySessionPendingRequestMap spdy_session_pending_request_map_;

  TimeFunc time_func_;
  ServerPushDelegate* push_delegate_;

  NetworkQualityEstimator* network_quality_estimator_;

  DISALLOW_COPY_AND_ASSIGN(SpdySessionPool);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_POOL_H_
