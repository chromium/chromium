// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_STREAM_FACTORY_H_
#define NET_QUIC_QUIC_STREAM_FACTORY_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"
#include "net/base/proxy_server.h"
#include "net/cert/cert_database.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/network_connection.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_clock_skew_detector.h"
#include "net/quic/quic_connectivity_monitor.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_session_key.h"
#include "net/socket/client_socket_pool.h"
#include "net/ssl/ssl_config_service.h"
#include "net/third_party/quiche/src/quiche/quic/core/deterministic_connection_id_generator.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_crypto_stream.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "url/scheme_host_port.h"

namespace base {
class Value;
}  // namespace base

namespace quic {
class QuicAlarmFactory;
class QuicClock;
}  // namespace quic

namespace quiche {
class QuicRandom;
}  // namespace quiche

namespace net {

class CTPolicyEnforcer;
class CertVerifier;
class ClientSocketFactory;
class HostResolver;
struct HostResolverEndpointResult;
class HttpServerProperties;
class NetLog;
class NetworkAnonymizationKey;
class QuicChromiumConnectionHelper;
class QuicCryptoClientStreamFactory;
class QuicServerInfo;
class QuicStreamFactory;
class QuicContext;
class SCTAuditingDelegate;
class SocketPerformanceWatcherFactory;
class SocketTag;
class TransportSecurityState;

namespace test {
class QuicStreamFactoryPeer;
}  // namespace test

// Maximum number of not currently in use QuicCryptoClientConfig that can be
// stored in |recent_crypto_config_map_|.
//
// TODO(mmenke): Should figure out a reasonable value of this, using field
// trials. The optimal value may increase over time, as QUIC becomes more
// prevalent. Whether or not NetworkAnonymizationKeys end up including subframe
// URLs will also influence the ideal value.
const int kMaxRecentCryptoConfigs = 100;

enum QuicPlatformNotification {
  NETWORK_CONNECTED,
  NETWORK_MADE_DEFAULT,
  NETWORK_DISCONNECTED,
  NETWORK_SOON_TO_DISCONNECT,
  NETWORK_IP_ADDRESS_CHANGED,
  NETWORK_NOTIFICATION_MAX
};

enum AllActiveSessionsGoingAwayReason {
  kClockSkewDetected,
  kIPAddressChanged,
  kCertDBChanged,
  kCertVerifierChanged
};

enum CreateSessionFailure {
  CREATION_ERROR_CONNECTING_SOCKET,
  CREATION_ERROR_SETTING_RECEIVE_BUFFER,
  CREATION_ERROR_SETTING_SEND_BUFFER,
  CREATION_ERROR_SETTING_DO_NOT_FRAGMENT,
  CREATION_ERROR_SETTING_RECEIVE_ECN,
  CREATION_ERROR_MAX
};

// Encapsulates a pending request for a QuicChromiumClientSession.
// If the request is still pending when it is destroyed, it will
// cancel the request with the factory.
class NET_EXPORT_PRIVATE QuicStreamRequest {
 public:
  explicit QuicStreamRequest(QuicStreamFactory* factory);

  QuicStreamRequest(const QuicStreamRequest&) = delete;
  QuicStreamRequest& operator=(const QuicStreamRequest&) = delete;

  ~QuicStreamRequest();

  // |cert_verify_flags| is bitwise OR'd of CertVerifier::VerifyFlags and it is
  // passed to CertVerifier::Verify.
  // |destination| will be resolved and resulting IPEndPoint used to open a
  // quic::QuicConnection.  This can be different than
  // HostPortPair::FromURL(url).
  // When |use_dns_aliases| is true, any DNS aliases found in host resolution
  // are stored in the |dns_aliases_by_session_key_| map. |use_dns_aliases|
  // should be false in the case of a proxy.
  int Request(url::SchemeHostPort destination,
              quic::ParsedQuicVersion quic_version,
              PrivacyMode privacy_mode,
              RequestPriority priority,
              const SocketTag& socket_tag,
              const NetworkAnonymizationKey& network_anonymization_key,
              SecureDnsPolicy secure_dns_policy,
              bool use_dns_aliases,
              bool require_dns_https_alpn,
              int cert_verify_flags,
              const GURL& url,
              const NetLogWithSource& net_log,
              NetErrorDetails* net_error_details,
              CompletionOnceCallback failed_on_default_network_callback,
              CompletionOnceCallback callback);

  // This function must be called after Request() returns ERR_IO_PENDING.
  // Returns true if Request() requires host resolution and it hasn't completed
  // yet. If true is returned, |callback| will run when host resolution
  // completes. It will be called with the result after host resolution during
  // the connection process. For example, if host resolution returns OK and then
  // crypto handshake returns ERR_IO_PENDING, then |callback| will run with
  // ERR_IO_PENDING.
  bool WaitForHostResolution(CompletionOnceCallback callback);

  // Tells QuicStreamRequest it should expect OnHostResolutionComplete()
  // to be called in the future.
  void ExpectOnHostResolution();

  // Will be called by the associated QuicStreamFactory::Job when host
  // resolution completes asynchronously after Request().
  void OnHostResolutionComplete(int rv);

  // This function must be called after Request() returns ERR_IO_PENDING.
  // Returns true if no QUIC session has been created yet. If true is returned,
  // `callback` will be run when the QUIC session has been created and will be
  // called with the result of OnCreateSessionComplete. For example, if session
  // creation returned OK but CryptoConnect returns ERR_IO_PENDING then
  // `callback` will be run with ERR_IO_PENDING.
  bool WaitForQuicSessionCreation(CompletionOnceCallback callback);

  // Tells QuicStreamRequest it should expect OnQuicSessionCreationComplete()
  // to be called in the future.
  void ExpectQuicSessionCreation();

  // Will be called by the associated QuicStreamFactory::Job when session
  // creation completes asynchronously after Request().
  void OnQuicSessionCreationComplete(int rv);

  void OnRequestComplete(int rv);

  // Called when the original connection created on the default network for
  // |this| fails and a new connection has been created on the alternate
  // network.
  void OnConnectionFailedOnDefaultNetwork();

  // Helper method that calls |factory_|'s GetTimeDelayForWaitingJob(). It
  // returns the amount of time waiting job should be delayed.
  base::TimeDelta GetTimeDelayForWaitingJob() const;

  // If host resolution is underway, changes the priority of the host resolver
  // request.
  void SetPriority(RequestPriority priority);

  // Releases the handle to the QUIC session retrieved as a result of Request().
  std::unique_ptr<QuicChromiumClientSession::Handle> ReleaseSessionHandle();

  // Sets |session_|.
  void SetSession(std::unique_ptr<QuicChromiumClientSession::Handle> session);

  NetErrorDetails* net_error_details() { return net_error_details_; }

  const QuicSessionKey& session_key() const { return session_key_; }

  const NetLogWithSource& net_log() const { return net_log_; }

  bool CanUseExistingSession(
      const GURL& url,
      PrivacyMode privacy_mode,
      const SocketTag& socket_tag,
      const NetworkAnonymizationKey& network_anonymization_key,
      SecureDnsPolicy secure_dns_policy,
      bool require_dns_https_alpn,
      const url::SchemeHostPort& destination) const;

 private:
  raw_ptr<QuicStreamFactory> factory_;
  QuicSessionKey session_key_;
  NetLogWithSource net_log_;
  CompletionOnceCallback callback_;
  CompletionOnceCallback failed_on_default_network_callback_;
  raw_ptr<NetErrorDetails> net_error_details_;  // Unowned.
  std::unique_ptr<QuicChromiumClientSession::Handle> session_;

  // Set in Request(). If true, then OnHostResolutionComplete() is expected to
  // be called in the future.
  bool expect_on_host_resolution_ = false;

  bool expect_on_quic_session_creation_ = false;
  // Callback passed to WaitForHostResolution().
  CompletionOnceCallback host_resolution_callback_;

  CompletionOnceCallback create_session_callback_;
};

// A factory for fetching QuicChromiumClientSessions.
class NET_EXPORT_PRIVATE QuicStreamFactory
    : public NetworkChangeNotifier::IPAddressObserver,
      public NetworkChangeNotifier::NetworkObserver,
      public CertDatabase::Observer,
      public CertVerifier::Observer {
 public:
  // This class encompasses |destination| and |server_id|.
  // |destination| is a HostPortPair which is resolved
  // and a quic::QuicConnection is made to the resulting IP address.
  // |server_id| identifies the origin of the request,
  // the crypto handshake advertises |server_id.host()| to the server,
  // and the certificate is also matched against |server_id.host()|.
  class NET_EXPORT_PRIVATE QuicSessionAliasKey {
   public:
    QuicSessionAliasKey() = default;
    QuicSessionAliasKey(url::SchemeHostPort destination,
                        QuicSessionKey session_key);
    ~QuicSessionAliasKey() = default;

    // Needed to be an element of std::set.
    bool operator<(const QuicSessionAliasKey& other) const;
    bool operator==(const QuicSessionAliasKey& other) const;

    const url::SchemeHostPort& destination() const { return destination_; }
    const quic::QuicServerId& server_id() const {
      return session_key_.server_id();
    }
    const QuicSessionKey& session_key() const { return session_key_; }

    // Returns the estimate of dynamically allocated memory in bytes.

   private:
    url::SchemeHostPort destination_;
    QuicSessionKey session_key_;
  };

  QuicStreamFactory(
      NetLog* net_log,
      HostResolver* host_resolver,
      SSLConfigService* ssl_config_service,
      ClientSocketFactory* client_socket_factory,
      HttpServerProperties* http_server_properties,
      CertVerifier* cert_verifier,
      CTPolicyEnforcer* ct_policy_enforcer,
      TransportSecurityState* transport_security_state,
      SCTAuditingDelegate* sct_auditing_delegate,
      SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
      QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory,
      QuicContext* context);

  QuicStreamFactory(const QuicStreamFactory&) = delete;
  QuicStreamFactory& operator=(const QuicStreamFactory&) = delete;

  ~QuicStreamFactory() override;

  // Returns true if there is an existing session for |session_key| or if the
  // request can be pooled to an existing session to the IP address of
  // |destination|.
  bool CanUseExistingSession(const QuicSessionKey& session_key,
                             const url::SchemeHostPort& destination) const;

  // Fetches a QuicChromiumClientSession to |host_port_pair| which will be
  // owned by |request|.
  // If a matching session already exists, this method will return OK.  If no
  // matching session exists, this will return ERR_IO_PENDING and will invoke
  // OnRequestComplete asynchronously.
  // When |use_dns_aliases| is true, any DNS aliases found in host resolution
  // are stored in the |dns_aliases_by_session_key_| map. |use_dns_aliases|
  // should be false in the case of a proxy.
  int Create(const QuicSessionKey& session_key,
             url::SchemeHostPort destination,
             quic::ParsedQuicVersion quic_version,
             RequestPriority priority,
             bool use_dns_aliases,
             int cert_verify_flags,
             const GURL& url,
             const NetLogWithSource& net_log,
             QuicStreamRequest* request);

  // Called by a session when it is going away and no more streams should be
  // created on it.
  void OnSessionGoingAway(QuicChromiumClientSession* session);

  // Called by a session after it shuts down.
  void OnSessionClosed(QuicChromiumClientSession* session);

  // Called by a session when it blackholes after the handshake is confirmed.
  void OnBlackholeAfterHandshakeConfirmed(QuicChromiumClientSession* session);

  // Cancels a pending request.
  void CancelRequest(QuicStreamRequest* request);

  // Sets priority of a request.
  void SetRequestPriority(QuicStreamRequest* request, RequestPriority priority);

  // Closes all current sessions with specified network, QUIC error codes.
  // It sends connection close packet when closing connections.
  void CloseAllSessions(int error, quic::QuicErrorCode quic_error);

  base::Value QuicStreamFactoryInfoToValue() const;

  // Delete cached state objects in |crypto_config_|. If |origin_filter| is not
  // null, only objects on matching origins will be deleted.
  void ClearCachedStatesInCryptoConfig(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter);

  // Helper method that connects a DatagramClientSocket. Socket is
  // bound to the default network if the |network| param is
  // handles::kInvalidNetworkHandle. This method calls
  // DatagramClientSocket::ConnectAsync and completes asynchronously. Returns
  // ERR_IO_PENDING.
  int ConnectAndConfigureSocket(CompletionOnceCallback callback,
                                DatagramClientSocket* socket,
                                IPEndPoint addr,
                                handles::NetworkHandle network,
                                const SocketTag& socket_tag);

  // Helper method that configures a DatagramClientSocket once
  // DatagramClientSocket::ConnectAsync completes. Posts a task to run
  // `callback` with a net_error code.
  virtual void FinishConnectAndConfigureSocket(CompletionOnceCallback callback,
                                               DatagramClientSocket* socket,
                                               const SocketTag& socket_tag,
                                               int rv);

  void OnFinishConnectAndConfigureSocketError(CompletionOnceCallback callback,
                                              enum CreateSessionFailure error,
                                              int rv);

  void DoCallback(CompletionOnceCallback callback, int rv);

  // Helper method that configures a DatagramClientSocket. Socket is
  // bound to the default network if the |network| param is
  // handles::kInvalidNetworkHandle. This method calls
  // DatagramClientSocket::Connect and completes synchronously. Returns
  // net_error code.
  // TODO(liza): Remove this once QuicStreamFactory::Job calls
  // ConnectAndConfigureSocket.
  int ConfigureSocket(DatagramClientSocket* socket,
                      IPEndPoint addr,
                      handles::NetworkHandle network,
                      const SocketTag& socket_tag);

  // Finds an alternative to |old_network| from the platform's list of connected
  // networks. Returns handles::kInvalidNetworkHandle if no
  // alternative is found.
  handles::NetworkHandle FindAlternateNetwork(
      handles::NetworkHandle old_network);

  // Creates a datagram socket. |source| is the NetLogSource for the entity
  // trying to create the socket, if it has one.
  std::unique_ptr<DatagramClientSocket> CreateSocket(
      NetLog* net_log,
      const NetLogSource& source);

  // NetworkChangeNotifier::IPAddressObserver methods:

  // Until the servers support roaming, close all connections when the local
  // IP address changes.
  void OnIPAddressChanged() override;

  // NetworkChangeNotifier::NetworkObserver methods:
  void OnNetworkConnected(handles::NetworkHandle network) override;
  void OnNetworkDisconnected(handles::NetworkHandle network) override;
  void OnNetworkSoonToDisconnect(handles::NetworkHandle network) override;
  void OnNetworkMadeDefault(handles::NetworkHandle network) override;

  // CertDatabase::Observer methods:

  // We close all sessions when certificate database is changed.
  void OnTrustStoreChanged() override;

  // CertVerifier::Observer:
  // We close all sessions when certificate verifier settings have changed.
  void OnCertVerifierChanged() override;

  bool is_quic_known_to_work_on_current_network() const {
    return is_quic_known_to_work_on_current_network_;
  }

  bool allow_server_migration() const { return params_.allow_server_migration; }

  // Returns true is gQUIC 0-RTT is disabled from quic_context.
  bool gquic_zero_rtt_disabled() const {
    return params_.disable_gquic_zero_rtt;
  }

  void set_is_quic_known_to_work_on_current_network(
      bool is_quic_known_to_work_on_current_network);

  // It returns the amount of time waiting job should be delayed.
  base::TimeDelta GetTimeDelayForWaitingJob(const QuicSessionKey& session_key);

  QuicChromiumConnectionHelper* helper() { return helper_.get(); }

  quic::QuicAlarmFactory* alarm_factory() { return alarm_factory_.get(); }

  handles::NetworkHandle default_network() const { return default_network_; }

  // Returns the stored DNS aliases for the session key.
  const std::set<std::string>& GetDnsAliasesForSessionKey(
      const QuicSessionKey& key) const;

 private:
  class Job;
  class QuicCryptoClientConfigOwner;
  class CryptoClientConfigHandle;
  friend class MockQuicStreamFactory;
  friend class test::QuicStreamFactoryPeer;

  using SessionMap = std::map<QuicSessionKey, QuicChromiumClientSession*>;
  using SessionIdMap =
      std::map<QuicChromiumClientSession*, QuicSessionAliasKey>;
  using AliasSet = std::set<QuicSessionAliasKey>;
  using SessionAliasMap = std::map<QuicChromiumClientSession*, AliasSet>;
  using SessionSet = std::set<QuicChromiumClientSession*>;
  using IPAliasMap = std::map<IPEndPoint, SessionSet>;
  using SessionPeerIPMap = std::map<QuicChromiumClientSession*, IPEndPoint>;
  using JobMap = std::map<QuicSessionKey, std::unique_ptr<Job>>;
  using DnsAliasesBySessionKeyMap =
      std::map<QuicSessionKey, std::set<std::string>>;
  using QuicCryptoClientConfigMap =
      std::map<NetworkAnonymizationKey,
               std::unique_ptr<QuicCryptoClientConfigOwner>>;

  bool HasMatchingIpSession(const QuicSessionAliasKey& key,
                            const std::vector<IPEndPoint>& ip_endpoints,
                            const std::set<std::string>& aliases,
                            bool use_dns_aliases);
  void OnJobComplete(Job* job, int rv);
  bool HasActiveSession(const QuicSessionKey& session_key) const;
  bool HasActiveJob(const QuicSessionKey& session_key) const;
  int CreateSessionSync(const QuicSessionAliasKey& key,
                        quic::ParsedQuicVersion quic_version,
                        int cert_verify_flags,
                        bool require_confirmation,
                        const HostResolverEndpointResult& endpoint_result,
                        base::TimeTicks dns_resolution_start_time,
                        base::TimeTicks dns_resolution_end_time,
                        const NetLogWithSource& net_log,
                        QuicChromiumClientSession** session,
                        handles::NetworkHandle* network);
  int CreateSessionAsync(CompletionOnceCallback callback,
                         const QuicSessionAliasKey& key,
                         quic::ParsedQuicVersion quic_version,
                         int cert_verify_flags,
                         bool require_confirmation,
                         const HostResolverEndpointResult& endpoint_result,
                         base::TimeTicks dns_resolution_start_time,
                         base::TimeTicks dns_resolution_end_time,
                         const NetLogWithSource& net_log,
                         QuicChromiumClientSession** session,
                         handles::NetworkHandle* network);
  void FinishCreateSession(CompletionOnceCallback callback,
                           const QuicSessionAliasKey& key,
                           quic::ParsedQuicVersion quic_version,
                           int cert_verify_flags,
                           bool require_confirmation,
                           const HostResolverEndpointResult& endpoint_result,
                           base::TimeTicks dns_resolution_start_time,
                           base::TimeTicks dns_resolution_end_time,
                           const NetLogWithSource& net_log,
                           QuicChromiumClientSession** session,
                           handles::NetworkHandle* network,
                           std::unique_ptr<DatagramClientSocket> socket,
                           int rv);
  bool CreateSessionHelper(const QuicSessionAliasKey& key,
                           quic::ParsedQuicVersion quic_version,
                           int cert_verify_flags,
                           bool require_confirmation,
                           const HostResolverEndpointResult& endpoint_result,
                           base::TimeTicks dns_resolution_start_time,
                           base::TimeTicks dns_resolution_end_time,
                           const NetLogWithSource& net_log,
                           QuicChromiumClientSession** session,
                           handles::NetworkHandle* network,
                           std::unique_ptr<DatagramClientSocket> socket);
  void ActivateSession(const QuicSessionAliasKey& key,
                       QuicChromiumClientSession* session,
                       std::set<std::string> dns_aliases);
  // Go away all active sessions. May disable session's connectivity monitoring
  // based on the |reason|.
  void MarkAllActiveSessionsGoingAway(AllActiveSessionsGoingAwayReason reason);

  void ConfigureInitialRttEstimate(
      const quic::QuicServerId& server_id,
      const NetworkAnonymizationKey& network_anonymization_key,
      quic::QuicConfig* config);

  // Returns |srtt| in micro seconds from ServerNetworkStats. Returns 0 if there
  // is no |http_server_properties_| or if |http_server_properties_| doesn't
  // have ServerNetworkStats for the given |server_id|.
  int64_t GetServerNetworkStatsSmoothedRttInMicroseconds(
      const quic::QuicServerId& server_id,
      const NetworkAnonymizationKey& network_anonymization_key) const;

  // Returns |srtt| from ServerNetworkStats. Returns null if there
  // is no |http_server_properties_| or if |http_server_properties_| doesn't
  // have ServerNetworkStats for the given |server_id|.
  const base::TimeDelta* GetServerNetworkStatsSmoothedRtt(
      const quic::QuicServerId& server_id,
      const NetworkAnonymizationKey& network_anonymization_key) const;

  // Helper methods.
  bool WasQuicRecentlyBroken(const QuicSessionKey& session_key) const;

  // Helper method to initialize the following migration options and check
  // pre-requisites:
  // - |params_.migrate_sessions_on_network_change_v2|
  // - |params_.migrate_sessions_early_v2|
  // - |params_.migrate_idle_sessions|
  // - |params_.retry_on_alternate_network_before_handshake|
  // If pre-requisites are not met, turn off the corresponding options.
  void InitializeMigrationOptions();

  // Initializes the cached state associated with |server_id| in
  // |crypto_config_| with the information in |server_info|.
  void InitializeCachedStateInCryptoConfig(
      const CryptoClientConfigHandle& crypto_config_handle,
      const quic::QuicServerId& server_id,
      const std::unique_ptr<QuicServerInfo>& server_info);

  void ProcessGoingAwaySession(QuicChromiumClientSession* session,
                               const quic::QuicServerId& server_id,
                               bool was_session_active);

  // Insert the given alias `key` in the AliasSet for the given `session` in
  // the map `session_aliases_`, and add the given `dns_aliases` for
  // `key.session_key()` in `dns_aliases_by_session_key_`.
  void MapSessionToAliasKey(QuicChromiumClientSession* session,
                            const QuicSessionAliasKey& key,
                            std::set<std::string> dns_aliases);

  // For all alias keys for `session` in `session_aliases_`, erase the
  // corresponding DNS aliases in `dns_aliases_by_session_key_`. Then erase
  // `session` from `session_aliases_`.
  void UnmapSessionFromSessionAliases(QuicChromiumClientSession* session);

  // Creates a CreateCryptoConfigHandle for the specified
  // NetworkAnonymizationKey. If there's already a corresponding entry in
  // |active_crypto_config_map_|, reuses it. If there's a corresponding entry in
  // |recent_crypto_config_map_|, promotes it to |active_crypto_config_map_| and
  // then reuses it. Otherwise, creates a new entry in
  // |active_crypto_config_map_|.
  std::unique_ptr<CryptoClientConfigHandle> CreateCryptoConfigHandle(
      const NetworkAnonymizationKey& network_anonymization_key);

  // Salled when the indicated member of |active_crypto_config_map_| has no
  // outstanding references. The QuicCryptoClientConfigOwner is then moved to
  // |recent_crypto_config_map_|, an MRU cache.
  void OnAllCryptoClientRefReleased(
      QuicCryptoClientConfigMap::iterator& map_iterator);

  // Called when a network change happens.
  // Collect platform notification metrics, and if the change affects the
  // original default network interface, collect connectivity degradation
  // metrics from |connectivity_monitor_| and add to histograms.
  void CollectDataOnPlatformNotification(
      enum QuicPlatformNotification notification,
      handles::NetworkHandle affected_network) const;

  std::unique_ptr<QuicCryptoClientConfigHandle> GetCryptoConfigForTesting(
      const NetworkAnonymizationKey& network_anonymization_key);

  bool CryptoConfigCacheIsEmptyForTesting(
      const quic::QuicServerId& server_id,
      const NetworkAnonymizationKey& network_anonymization_key);

  const quic::ParsedQuicVersionVector& supported_versions() const {
    return params_.supported_versions;
  }

  // Whether QUIC is known to work on current network. This is true when QUIC is
  // expected to work in general, rather than whether QUIC was broken / recently
  // broken when used with a particular server. That information is stored in
  // the broken alternative service map in HttpServerProperties.
  bool is_quic_known_to_work_on_current_network_ = false;

  NetLogWithSource net_log_;
  raw_ptr<HostResolver> host_resolver_;
  raw_ptr<ClientSocketFactory> client_socket_factory_;
  raw_ptr<HttpServerProperties> http_server_properties_;
  const raw_ptr<CertVerifier> cert_verifier_;
  const raw_ptr<CTPolicyEnforcer> ct_policy_enforcer_;
  const raw_ptr<TransportSecurityState> transport_security_state_;
  const raw_ptr<SCTAuditingDelegate> sct_auditing_delegate_;
  raw_ptr<QuicCryptoClientStreamFactory> quic_crypto_client_stream_factory_;
  raw_ptr<quic::QuicRandom> random_generator_;  // Unowned.
  raw_ptr<const quic::QuicClock> clock_;        // Unowned.
  QuicParams params_;
  QuicClockSkewDetector clock_skew_detector_;

  // Factory which is used to create socket performance watcher. A new watcher
  // is created for every QUIC connection.
  // |socket_performance_watcher_factory_| may be null.
  raw_ptr<SocketPerformanceWatcherFactory> socket_performance_watcher_factory_;

  // The helper used for all connections.
  std::unique_ptr<QuicChromiumConnectionHelper> helper_;

  // The alarm factory used for all connections.
  std::unique_ptr<quic::QuicAlarmFactory> alarm_factory_;

  // Contains owning pointers to all sessions that currently exist.
  SessionIdMap all_sessions_;
  // Contains non-owning pointers to currently active session
  // (not going away session, once they're implemented).
  SessionMap active_sessions_;
  // Map from session to set of aliases that this session is known by.
  SessionAliasMap session_aliases_;
  // Map from IP address to sessions which are connected to this address.
  IPAliasMap ip_aliases_;
  // Map from session to its original peer IP address.
  SessionPeerIPMap session_peer_ip_;

  // Origins which have gone away recently.
  AliasSet gone_away_aliases_;

  // A map of DNS alias vectors by session keys.
  DnsAliasesBySessionKeyMap dns_aliases_by_session_key_;

  // When a QuicCryptoClientConfig is in use, it has one or more live
  // CryptoClientConfigHandles, and is stored in |active_crypto_config_map_|.
  // Once all the handles are deleted, it's moved to
  // |recent_crypto_config_map_|. If reused before it is evicted from LRUCache,
  // it will be removed from the cache and return to the active config map.
  // These two maps should never both have entries with the same
  // NetworkAnonymizationKey.
  QuicCryptoClientConfigMap active_crypto_config_map_;
  base::LRUCache<NetworkAnonymizationKey,
                 std::unique_ptr<QuicCryptoClientConfigOwner>>
      recent_crypto_config_map_;

  const quic::QuicConfig config_;

  JobMap active_jobs_;

  // PING timeout for connections.
  quic::QuicTime::Delta ping_timeout_;
  quic::QuicTime::Delta reduced_ping_timeout_;

  // Timeout for how long the wire can have no retransmittable packets.
  quic::QuicTime::Delta retransmittable_on_wire_timeout_;

  // If more than |yield_after_packets_| packets have been read or more than
  // |yield_after_duration_| time has passed, then
  // QuicChromiumPacketReader::StartReading() yields by doing a PostTask().
  int yield_after_packets_;
  quic::QuicTime::Delta yield_after_duration_;

  // If |migrate_sessions_early_v2_| is true, tracks the current default
  // network, and is updated OnNetworkMadeDefault.
  // Otherwise, always set to NetworkChangeNotifier::kInvalidNetwork.
  handles::NetworkHandle default_network_;

  // Local address of socket that was created in CreateSession.
  IPEndPoint local_address_;
  // True if we need to check HttpServerProperties if QUIC was supported last
  // time.
  bool need_to_check_persisted_supports_quic_ = true;
  bool prefer_aes_gcm_recorded_ = false;

  NetworkConnection network_connection_;

  QuicConnectivityMonitor connectivity_monitor_;

  raw_ptr<const base::TickClock, DanglingUntriaged> tick_clock_ = nullptr;

  raw_ptr<base::SequencedTaskRunner, DanglingUntriaged> task_runner_ = nullptr;

  const raw_ptr<SSLConfigService> ssl_config_service_;

  // Whether NetworkAnonymizationKeys should be used for
  // |active_crypto_config_map_|. If false, there will just be one config with
  // an empty NetworkAnonymizationKey. Whether QuicSessionAliasKeys all have an
  // empty NIK is based on whether socket pools are respecting NIKs, but whether
  // those NIKs are also used when accessing |active_crypto_config_map_| is also
  // gated this, which is set based on whether HttpServerProperties is
  // respecting NIKs, as that data is fed into the crypto config map using the
  // corresponding NIK.
  const bool use_network_anonymization_key_for_crypto_configs_;

  quic::DeterministicConnectionIdGenerator connection_id_generator_{
      quic::kQuicDefaultConnectionIdLength};

  base::WeakPtrFactory<QuicStreamFactory> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_STREAM_FACTORY_H_
