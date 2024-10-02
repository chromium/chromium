// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_POOL_H_
#define NET_SPDY_SPDY_SESSION_POOL_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_server.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/log/net_log_source.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/socket/connect_job.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_session_key.h"
#include "net/ssl/ssl_config_service.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

class StreamSocketHandle;
class HostResolver;
class HttpServerProperties;
class NetLogWithSource;
class NetworkQualityEstimator;
class SpdySession;
class StreamSocket;
class TransportSecurityState;

// This is a very simple pool for open SpdySessions.
class NET_EXPORT SpdySessionPool
    : public NetworkChangeNotifier::IPAddressObserver,
      public SSLClientContext::Observer {
 public:
  typedef base::TimeTicks (*TimeFunc)();

  // Struct to hold randomly generated frame parameters to be used for sending
  // frames on the wire to "grease" frame type.  Frame type has to be one of
  // the reserved values defined in
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  struct GreasedHttp2Frame {
    uint8_t type;
    uint8_t flags;
    std::string payload;
  };

  // A request for a SpdySession with a particular SpdySessionKey. The
  // SpdySessionPool's RequestSession() creates these. The Delegate's
  // OnSpdySessionAvailable() method will be invoked when a matching SpdySession
  // is added to the pool. The Delegate's OnSpdySessionAvailable() method will
  // be invoked at most once for a single SpdySessionRequest.
  //
  // Destroying the request will stop watching the pool for such a session. The
  // request must be destroyed before the SpdySessionPool is.
  class NET_EXPORT_PRIVATE SpdySessionRequest {
   public:
    // Interface for watching for when a SpdySession with a provided key is
    // created.
    class NET_EXPORT_PRIVATE Delegate {
     public:
      Delegate();

      Delegate(const Delegate&) = delete;
      Delegate& operator=(const Delegate&) = delete;

      virtual ~Delegate();

      // |spdy_session| will not be null.
      virtual void OnSpdySessionAvailable(
          base::WeakPtr<SpdySession> spdy_session) = 0;
    };

    // Constructor - this is called by the SpdySessionPool.
    SpdySessionRequest(const SpdySessionKey& key,
                       bool enable_ip_based_pooling,
                       bool is_websocket,
                       bool is_blocking_request_for_session,
                       Delegate* delegate,
                       SpdySessionPool* spdy_session_pool);

    SpdySessionRequest(const SpdySessionRequest&) = delete;
    SpdySessionRequest& operator=(const SpdySessionRequest&) = delete;

    ~SpdySessionRequest();

    // Called by SpdySessionPool to signal that the request has been removed
    // from the SpdySessionPool.
    void OnRemovedFromPool();

    const SpdySessionKey& key() const { return key_; }
    bool enable_ip_based_pooling() const { return enable_ip_based_pooling_; }
    bool is_websocket() const { return is_websocket_; }
    bool is_blocking_request_for_session() const {
      return is_blocking_request_for_session_;
    }
    Delegate* delegate() { return delegate_; }

    // The associated SpdySessionPool, or nullptr if OnRemovedFromPool() has
    // been called.
    SpdySessionPool* spdy_session_pool() { return spdy_session_pool_; }

   private:
    const SpdySessionKey key_;
    const bool enable_ip_based_pooling_;
    const bool is_websocket_;
    const bool is_blocking_request_for_session_;
    const raw_ptr<Delegate> delegate_;
    raw_ptr<SpdySessionPool> spdy_session_pool_;
  };

  SpdySessionPool(HostResolver* host_resolver,
                  SSLClientContext* ssl_client_context,
                  HttpServerProperties* http_server_properties,
                  TransportSecurityState* transport_security_state,
                  const quic::ParsedQuicVersionVector& quic_supported_versions,
                  bool enable_ping_based_connection_checking,
                  bool is_http_enabled,
                  bool is_quic_enabled,
                  size_t session_max_recv_window_size,
                  int session_max_queued_capped_frames,
                  const spdy::SettingsMap& initial_settings,
                  bool enable_http2_settings_grease,
                  const std::optional<GreasedHttp2Frame>& greased_http2_frame,
                  bool http2_end_stream_with_data_frame,
                  bool enable_priority_update,
                  bool go_away_on_ip_change,
                  SpdySessionPool::TimeFunc time_func,
                  NetworkQualityEstimator* network_quality_estimator,
                  bool cleanup_sessions_on_ip_address_changed);

  SpdySessionPool(const SpdySessionPool&) = delete;
  SpdySessionPool& operator=(const SpdySessionPool&) = delete;

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
  // Returns OK on success and sets |*session| to point to the new SpdySession.
  // Returns a net error code on failure, in which case the value of |*session|
  // is undefined.
  //
  // Note that the SpdySession begins reading from |stream_socket_handle| on a
  // subsequent event loop iteration, so it may be closed immediately afterwards
  // if the first read of |stream_socket_handle| fails.
  int CreateAvailableSessionFromSocketHandle(
      const SpdySessionKey& key,
      std::unique_ptr<StreamSocketHandle> stream_socket_handle,
      const NetLogWithSource& net_log,
      base::WeakPtr<SpdySession>* session);

  // Just like the above method, except it takes a SocketStream instead of a
  // StreamSocketHandle, and separate connect timing information. When this
  // constructor is used, there is no socket pool beneath the SpdySession.
  // Instead, the session takes exclusive ownership of the underting socket, and
  // destroying the session will directly destroy the socket, as opposed to
  // disconnected it and then returning it to the socket pool. This is intended
  // for use with H2 proxies, which are layered beneath the socket pools and
  // can have sockets above them for tunnels, which are put in a socket pool.
  base::expected<base::WeakPtr<SpdySession>, int>
  CreateAvailableSessionFromSocket(
      const SpdySessionKey& key,
      std::unique_ptr<StreamSocket> socket_stream,
      const LoadTimingInfo::ConnectTiming& connect_timing,
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

  // Returns an available session if there is active session for `key` and the
  // session can be used for IP addresses in `service_endpoint`. Should be
  // called only when IP-based pooling is enabled.
  base::WeakPtr<SpdySession> FindMatchingIpSessionForServiceEndpoint(
      const SpdySessionKey& key,
      const ServiceEndpoint& service_endpoint,
      const std::set<std::string>& dns_aliases);

  // Returns true if there is an available session for |key|.
  bool HasAvailableSession(const SpdySessionKey& key, bool is_websocket) const;

  // Just like FindAvailableSession.
  //
  // Additionally, if it returns nullptr, populates |spdy_session_request| with
  // a request that will invoke |delegate| once a matching SPDY session becomes
  // available through the creation of a new SpdySession (as opposed to by
  // creating an alias for an existing session with a new host).
  //
  // |is_blocking_request_for_session| will be set to |true| if there is not
  // another "blocking" request already pending. For example, the first request
  // created will be considered "blocking", but subsequent requests will not as
  // long as the "blocking" request is not destroyed.  Once the "blocking"
  // request is destroyed, the next created request will be marked "blocking".
  //
  // If a request is created, that request is not the "blocking" request, and
  // |on_blocking_request_destroyed_callback| is non-null, then
  // |on_blocking_request_destroyed_callback| will be invoked asynchronously
  // when the "blocking" request is destroyed. The callback associated with the
  // "blocking" request is never invoked.
  //
  // |delegate|, |spdy_session_request|, and |is_blocking_request_for_session|
  // must all be non-null.
  //
  // TODO(mmenke): Merge this into FindAvailableSession().
  // TODO(mmenke): Don't invoke |on_blocking_request_destroyed_callback| when
  // all requests for a session have been successfully responded to.
  base::WeakPtr<SpdySession> RequestSession(
      const SpdySessionKey& key,
      bool enable_ip_based_pooling,
      bool is_websocket,
      const NetLogWithSource& net_log,
      base::RepeatingClosure on_blocking_request_destroyed_callback,
      SpdySessionRequest::Delegate* delegate,
      std::unique_ptr<SpdySessionRequest>* spdy_session_request,
      bool* is_blocking_request_for_session);

  // Invoked when a host resolution completes. Returns
  // OnHostResolutionCallbackResult::kMayBeDeletedAsync if there's a SPDY
  // session that's a suitable alias for |key|, setting up the alias if needed.
  OnHostResolutionCallbackResult OnHostResolutionComplete(
      const SpdySessionKey& key,
      bool is_websocket,
      const std::vector<HostResolverEndpointResult>& endpoint_results,
      const std::set<std::string>& aliases);

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
  void CloseCurrentIdleSessions(const std::string& description);

  // Repeatedly close all SpdySessions until all of them (including new ones
  // created in the process of closing the current ones, and new ones created in
  // the process of closing those new ones, etc.) are unavailable.
  void CloseAllSessions();

  // Mark all current sessions as going away.
  void MakeCurrentSessionsGoingAway(Error error);

  // Creates a Value summary of the state of the spdy session pool.
  std::unique_ptr<base::Value> SpdySessionPoolInfoToValue() const;

  HttpServerProperties* http_server_properties() {
    return http_server_properties_;
  }

  // NetworkChangeNotifier::IPAddressObserver methods:

  // We flush all idle sessions and release references to the active ones so
  // they won't get re-used.  The active ones will either complete successfully
  // or error out due to the IP address change.
  void OnIPAddressChanged() override;

  // SSLClientContext::Observer methods:

  // We perform the same flushing as described above when SSL settings change.
  void OnSSLConfigChanged(
      SSLClientContext::SSLConfigChangeType change_type) override;

  // Makes all sessions using |server|'s SSL configuration unavailable, meaning
  // they will not be used to service new streams. Does not close any existing
  // streams.
  void OnSSLConfigForServersChanged(
      const base::flat_set<HostPortPair>& servers) override;

  void set_network_quality_estimator(
      NetworkQualityEstimator* network_quality_estimator) {
    network_quality_estimator_ = network_quality_estimator;
  }

  // Returns the stored DNS aliases for the session key.
  std::set<std::string> GetDnsAliasesForSessionKey(
      const SpdySessionKey& key) const;

 private:
  friend class SpdySessionPoolPeer;  // For testing.

  using SessionSet = std::set<raw_ptr<SpdySession>>;
  using WeakSessionList = std::vector<base::WeakPtr<SpdySession>>;
  using AvailableSessionMap =
      std::map<SpdySessionKey, base::WeakPtr<SpdySession>>;
  using AliasMap = std::multimap<IPEndPoint, SpdySessionKey>;
  using DnsAliasesBySessionKeyMap =
      std::map<SpdySessionKey, std::set<std::string>>;
  using RequestSet = std::set<raw_ptr<SpdySessionRequest>>;

  struct RequestInfoForKey {
    RequestInfoForKey();
    ~RequestInfoForKey();

    // Whether one of the requests in |RequestSet| has its
    // is_blocking_request_for_session() bit set.
    bool has_blocking_request = false;

    RequestSet request_set;

    // Set of callbacks watching for the blocking request to be destroyed.
    std::list<base::RepeatingClosure> deferred_callbacks;
  };

  using SpdySessionRequestMap = std::map<SpdySessionKey, RequestInfoForKey>;

  // Removes |request| from |spdy_session_request_map_|.
  void RemoveRequestForSpdySession(SpdySessionRequest* request);

  // Returns true iff |session| is in |available_sessions_|.
  bool IsSessionAvailable(const base::WeakPtr<SpdySession>& session) const;

  // Map the given key to the given session. There must not already be a
  // mapping for `key`. Also adds an entry for `key` and `dns_aliases` in
  // `dns_aliases_by_session_key_`. If there are already DNS aliases for the
  // given key, replaces them.
  void MapKeyToAvailableSession(const SpdySessionKey& key,
                                const base::WeakPtr<SpdySession>& session,
                                std::set<std::string> dns_aliases);

  // Returns an iterator into |available_sessions_| for the given key,
  // which may be equal to |available_sessions_.end()|.
  AvailableSessionMap::iterator LookupAvailableSessionByKey(
      const SpdySessionKey& key);

  // Remove the mapping of the given key, which must exist. Also erases the
  // key-value pair of SpdySessionKey and DNS aliases from the
  // `dns_aliases_by_session_key_` map.
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

  // Creates a new session. The session must be initialized before
  // InsertSession() is invoked.
  std::unique_ptr<SpdySession> CreateSession(const SpdySessionKey& key,
                                             NetLog* net_log);
  // Adds a new session previously created with CreateSession to the pool.
  // |source_net_log| is the NetLog for the object that created the session.
  base::expected<base::WeakPtr<SpdySession>, int> InsertSession(
      const SpdySessionKey& key,
      std::unique_ptr<SpdySession> new_session,
      const NetLogWithSource& source_net_log,
      std::set<std::string> dns_aliases,
      bool perform_post_insertion_checks);

  // If a session with the specified |key| exists, invokes
  // OnSpdySessionAvailable on all matching members of
  // |spdy_session_request_map_|, removing them from the map. Regardless of
  // whether or not such key exists, invokes all corresponding callbacks
  // currently in |spdy_session_pending_request_map_|.
  void UpdatePendingRequests(const SpdySessionKey& key);

  // Removes the SpdySessionRequest at |request_set_iterator| from the
  // RequestSet at |request_map_iterator| and calls OnRemovedFromPool() on the
  // request. If the RequestSet becomes empty, also removes it from
  // |spdy_session_request_map_|.
  void RemoveRequestInternal(
      SpdySessionRequestMap::iterator request_map_iterator,
      RequestSet::iterator request_set_iterator);

  // Helper method of `FindMatchingIpSessionForServiceEndpoint()`. This is
  // basically a subset of OnHostResolutionComplete(), i.e.,:
  // * Doesn't support SocketTag.
  // * Assumes there is only one host resolution for `key` at the same time.
  base::WeakPtr<SpdySession> FindMatchingIpSession(
      const SpdySessionKey& key,
      const std::vector<IPEndPoint>& ip_endpoints,
      const std::set<std::string>& dns_aliases);

  raw_ptr<HttpServerProperties> http_server_properties_;

  raw_ptr<TransportSecurityState> transport_security_state_;

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

  // A map of DNS alias vectors by session keys.
  DnsAliasesBySessionKeyMap dns_aliases_by_session_key_;

  const raw_ptr<SSLClientContext> ssl_client_context_;
  const raw_ptr<HostResolver> resolver_;

  // Versions of QUIC which may be used.
  const quic::ParsedQuicVersionVector quic_supported_versions_;

  // Defaults to true. May be controlled via SpdySessionPoolPeer for tests.
  bool enable_sending_initial_data_ = true;
  bool enable_ping_based_connection_checking_;

  const bool is_http2_enabled_;
  const bool is_quic_enabled_;

  size_t session_max_recv_window_size_;

  // Maximum number of capped frames that can be queued at any time.
  int session_max_queued_capped_frames_;

  // Settings that are sent in the initial SETTINGS frame
  // (if |enable_sending_initial_data_| is true),
  // and also control SpdySession parameters like initial receive window size
  // and maximum HPACK dynamic table size.
  const spdy::SettingsMap initial_settings_;

  // If true, a setting parameter with reserved identifier will be sent in every
  // initial SETTINGS frame, see
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  // The setting identifier and value will be drawn independently for each
  // connection to prevent tracking of the client.
  const bool enable_http2_settings_grease_;

  // If set, an HTTP/2 frame with a reserved frame type will be sent after
  // every HTTP/2 SETTINGS frame and before every HTTP/2 DATA frame. See
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  const std::optional<GreasedHttp2Frame> greased_http2_frame_;

  // If set, the HEADERS frame carrying a request without body will not have the
  // END_STREAM flag set.  The stream will be closed by a subsequent empty DATA
  // frame with END_STREAM.  Does not affect bidirectional or proxy streams.
  // If unset, the HEADERS frame will have the END_STREAM flag set on.
  // This is useful in conjuction with |greased_http2_frame_| so that a frame
  // of reserved type can be sent out even on requests without a body.
  const bool http2_end_stream_with_data_frame_;

  // If true, enable sending PRIORITY_UPDATE frames until SETTINGS frame
  // arrives.  After SETTINGS frame arrives, do not send PRIORITY_UPDATE frames
  // any longer if SETTINGS_DEPRECATE_HTTP2_PRIORITIES is missing or has zero 0,
  // but continue and also stop sending HTTP/2-style priority information in
  // HEADERS frames and PRIORITY frames if it has value 1.
  const bool enable_priority_update_;

  // If set, sessions will be marked as going away upon relevant network changes
  // (instead of being closed).
  const bool go_away_on_ip_change_;

  SpdySessionRequestMap spdy_session_request_map_;

  TimeFunc time_func_;

  raw_ptr<NetworkQualityEstimator> network_quality_estimator_;

  const bool cleanup_sessions_on_ip_address_changed_;

  base::WeakPtrFactory<SpdySessionPool> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_POOL_H_
