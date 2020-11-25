// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_STREAM_FACTORY_H_
#define NET_QUIC_QUIC_STREAM_FACTORY_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/mru_cache.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_server.h"
#include "net/cert/cert_database.h"
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
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"
#include "net/third_party/quiche/src/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quic/core/quic_crypto_stream.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_server_id.h"

namespace base {
class Value;
namespace trace_event {
class ProcessMemoryDump;
}
}  // namespace base

namespace quic {
class QuicAlarmFactory;
class QuicClock;
class QuicRandom;
}  // namespace quic

namespace net {

class CTPolicyEnforcer;
class CertVerifier;
class ClientSocketFactory;
class HostResolver;
class HttpServerProperties;
class NetLog;
class NetworkIsolationKey;
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
// prevalent. Whether or not NetworkIsolationKeys end up including subframe URLs
// will also influence the ideal value.
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
  kCertDBChanged
};

// Encapsulates a pending request for a QuicChromiumClientSession.
// If the request is still pending when it is destroyed, it will
// cancel the request with the factory.
class NET_EXPORT_PRIVATE QuicStreamRequest {
 public:
  explicit QuicStreamRequest(QuicStreamFactory* factory);
  ~QuicStreamRequest();

  // |cert_verify_flags| is bitwise OR'd of CertVerifier::VerifyFlags and it is
  // passed to CertVerifier::Verify.
  // |destination| will be resolved and resulting IPEndPoint used to open a
  // quic::QuicConnection.  This can be different than
  // HostPortPair::FromURL(url).
  int Request(const HostPortPair& destination,
              quic::ParsedQuicVersion quic_version,
              PrivacyMode privacy_mode,
              RequestPriority priority,
              const SocketTag& socket_tag,
              const NetworkIsolationKey& network_isolation_key,
              bool disable_secure_dns,
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

 private:
  QuicStreamFactory* factory_;
  QuicSessionKey session_key_;
  NetLogWithSource net_log_;
  CompletionOnceCallback callback_;
  CompletionOnceCallback failed_on_default_network_callback_;
  NetErrorDetails* net_error_details_;  // Unowned.
  std::unique_ptr<QuicChromiumClientSession::Handle> session_;

  // Set in Request(). If true, then OnHostResolutionComplete() is expected to
  // be called in the future.
  bool expect_on_host_resolution_;
  // Callback passed to WaitForHostResolution().
  CompletionOnceCallback host_resolution_callback_;

  DISALLOW_COPY_AND_ASSIGN(QuicStreamRequest);
};

// A factory for fetching QuicChromiumClientSessions.
class NET_EXPORT_PRIVATE QuicStreamFactory
    : public NetworkChangeNotifier::IPAddressObserver,
      public NetworkChangeNotifier::NetworkObserver,
      public CertDatabase::Observer {
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
    QuicSessionAliasKey(const HostPortPair& destination,
                        const QuicSessionKey& session_key);
    ~QuicSessionAliasKey() = default;

    // Needed to be an element of std::set.
    bool operator<(const QuicSessionAliasKey& other) const;
    bool operator==(const QuicSessionAliasKey& other) const;

    const HostPortPair& destination() const { return destination_; }
    const quic::QuicServerId& server_id() const {
      return session_key_.server_id();
    }
    const QuicSessionKey& session_key() const { return session_key_; }

    // Returns the estimate of dynamically allocated memory in bytes.
    size_t EstimateMemoryUsage() const;

   private:
    HostPortPair destination_;
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
  ~QuicStreamFactory() override;

  // Returns true if there is an existing session for |session_key| or if the
  // request can be pooled to an existing session to the IP address of
  // |destination|.
  bool CanUseExistingSession(const QuicSessionKey& session_key,
                             const HostPortPair& destination);

  // Fetches a QuicChromiumClientSession to |host_port_pair| which will be
  // owned by |request|.
  // If a matching session already exists, this method will return OK.  If no
  // matching session exists, this will return ERR_IO_PENDING and will invoke
  // OnRequestComplete asynchronously.
  int Create(const QuicSessionKey& session_key,
             const HostPortPair& destination,
             quic::ParsedQuicVersion quic_version,
             RequestPriority priority,
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

  std::unique_ptr<base::Value> QuicStreamFactoryInfoToValue() const;

  // Delete cached state objects in |crypto_config_|. If |origin_filter| is not
  // null, only objects on matching origins will be deleted.
  void ClearCachedStatesInCryptoConfig(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter);

  // Helper method that configures a DatagramClientSocket. Socket is
  // bound to the default network if the |network| param is
  // NetworkChangeNotifier::kInvalidNetworkHandle.
  // Returns net_error code.
  int ConfigureSocket(DatagramClientSocket* socket,
                      IPEndPoint addr,
                      NetworkChangeNotifier::NetworkHandle network,
                      const SocketTag& socket_tag);

  // Finds an alternative to |old_network| from the platform's list of connected
  // networks. Returns NetworkChangeNotifier::kInvalidNetworkHandle if no
  // alternative is found.
  NetworkChangeNotifier::NetworkHandle FindAlternateNetwork(
      NetworkChangeNotifier::NetworkHandle old_network);

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
  void OnNetworkConnected(
      NetworkChangeNotifier::NetworkHandle network) override;
  void OnNetworkDisconnected(
      NetworkChangeNotifier::NetworkHandle network) override;
  void OnNetworkSoonToDisconnect(
      NetworkChangeNotifier::NetworkHandle network) override;
  void OnNetworkMadeDefault(
      NetworkChangeNotifier::NetworkHandle network) override;

  // CertDatabase::Observer methods:

  // We close all sessions when certificate database is changed.
  void OnCertDBChanged() override;

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

  void set_server_push_delegate(ServerPushDelegate* push_delegate) {
    push_delegate_ = push_delegate;
  }

  NetworkChangeNotifier::NetworkHandle default_network() const {
    return default_network_;
  }

  // Dumps memory allocation stats. |parent_dump_absolute_name| is the name
  // used by the parent MemoryAllocatorDump in the memory dump hierarchy.
  void DumpMemoryStats(base::trace_event::ProcessMemoryDump* pmd,
                       const std::string& parent_absolute_name) const;

 private:
  class Job;
  class QuicCryptoClientConfigOwner;
  class CryptoClientConfigHandle;
  friend class test::QuicStreamFactoryPeer;

  typedef std::map<QuicSessionKey, QuicChromiumClientSession*> SessionMap;
  typedef std::map<QuicChromiumClientSession*, QuicSessionAliasKey>
      SessionIdMap;
  typedef std::set<QuicSessionAliasKey> AliasSet;
  typedef std::map<QuicChromiumClientSession*, AliasSet> SessionAliasMap;
  typedef std::set<QuicChromiumClientSession*> SessionSet;
  typedef std::map<IPEndPoint, SessionSet> IPAliasMap;
  typedef std::map<QuicChromiumClientSession*, IPEndPoint> SessionPeerIPMap;
  typedef std::map<QuicSessionKey, std::unique_ptr<Job>> JobMap;
  using QuicCryptoClientConfigMap =
      std::map<NetworkIsolationKey,
               std::unique_ptr<QuicCryptoClientConfigOwner>>;

  bool HasMatchingIpSession(const QuicSessionAliasKey& key,
                            const AddressList& address_list);
  void OnJobComplete(Job* job, int rv);
  bool HasActiveSession(const QuicSessionKey& session_key) const;
  bool HasActiveJob(const QuicSessionKey& session_key) const;
  int CreateSession(const QuicSessionAliasKey& key,
                    quic::ParsedQuicVersion quic_version,
                    int cert_verify_flags,
                    bool require_confirmation,
                    const AddressList& address_list,
                    base::TimeTicks dns_resolution_start_time,
                    base::TimeTicks dns_resolution_end_time,
                    const NetLogWithSource& net_log,
                    QuicChromiumClientSession** session,
                    NetworkChangeNotifier::NetworkHandle* network);
  void ActivateSession(const QuicSessionAliasKey& key,
                       QuicChromiumClientSession* session);
  // Go away all active sessions. May disable session's connectivity monitoring
  // based on the |reason|.
  void MarkAllActiveSessionsGoingAway(AllActiveSessionsGoingAwayReason reason);

  void ConfigureInitialRttEstimate(
      const quic::QuicServerId& server_id,
      const NetworkIsolationKey& network_isolation_key,
      quic::QuicConfig* config);

  // Returns |srtt| in micro seconds from ServerNetworkStats. Returns 0 if there
  // is no |http_server_properties_| or if |http_server_properties_| doesn't
  // have ServerNetworkStats for the given |server_id|.
  int64_t GetServerNetworkStatsSmoothedRttInMicroseconds(
      const quic::QuicServerId& server_id,
      const NetworkIsolationKey& network_isolation_key) const;

  // Returns |srtt| from ServerNetworkStats. Returns null if there
  // is no |http_server_properties_| or if |http_server_properties_| doesn't
  // have ServerNetworkStats for the given |server_id|.
  const base::TimeDelta* GetServerNetworkStatsSmoothedRtt(
      const quic::QuicServerId& server_id,
      const NetworkIsolationKey& network_isolation_key) const;

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
  // |crypto_config_| with the information in |server_info|. Populates
  // |connection_id| with the next server designated connection id,
  // if any, and otherwise leaves it unchanged.
  void InitializeCachedStateInCryptoConfig(
      const CryptoClientConfigHandle& crypto_config_handle,
      const quic::QuicServerId& server_id,
      const std::unique_ptr<QuicServerInfo>& server_info,
      quic::QuicConnectionId* connection_id);

  void ProcessGoingAwaySession(QuicChromiumClientSession* session,
                               const quic::QuicServerId& server_id,
                               bool was_session_active);

  // Creates a CreateCryptoConfigHandle for the specified NetworkIsolationKey.
  // If there's already a corresponding entry in |active_crypto_config_map_|,
  // reuses it. If there's a corresponding entry in |recent_crypto_config_map_|,
  // promotes it to |active_crypto_config_map_| and then reuses it. Otherwise,
  // creates a new entry in |active_crypto_config_map_|.
  std::unique_ptr<CryptoClientConfigHandle> CreateCryptoConfigHandle(
      const NetworkIsolationKey& network_isolation_key);

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
      NetworkChangeNotifier::NetworkHandle affected_network) const;

  std::unique_ptr<QuicCryptoClientConfigHandle> GetCryptoConfigForTesting(
      const NetworkIsolationKey& network_isolation_key);

  bool CryptoConfigCacheIsEmptyForTesting(
      const quic::QuicServerId& server_id,
      const NetworkIsolationKey& network_isolation_key);

  // Whether QUIC is known to work on current network. This is true when QUIC is
  // expected to work in general, rather than whether QUIC was broken / recently
  // broken when used with a particular server. That information is stored in
  // the broken alternative service map in HttpServerProperties.
  bool is_quic_known_to_work_on_current_network_;

  NetLog* net_log_;
  HostResolver* host_resolver_;
  ClientSocketFactory* client_socket_factory_;
  HttpServerProperties* http_server_properties_;
  ServerPushDelegate* push_delegate_;
  CertVerifier* const cert_verifier_;
  CTPolicyEnforcer* const ct_policy_enforcer_;
  TransportSecurityState* const transport_security_state_;
  SCTAuditingDelegate* const sct_auditing_delegate_;
  QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory_;
  quic::QuicRandom* random_generator_;  // Unowned.
  const quic::QuicClock* clock_;        // Unowned.
  QuicParams params_;
  QuicClockSkewDetector clock_skew_detector_;

  // Factory which is used to create socket performance watcher. A new watcher
  // is created for every QUIC connection.
  // |socket_performance_watcher_factory_| may be null.
  SocketPerformanceWatcherFactory* socket_performance_watcher_factory_;

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

  // When a QuicCryptoClientConfig is in use, it has one or more live
  // CryptoClientConfigHandles, and is stored in |active_crypto_config_map_|.
  // Once all the handles are deleted, it's moved to
  // |recent_crypto_config_map_|. If reused before it is evicted from MRUCache,
  // it will be removed from the cache and return to the active config map.
  // These two maps should never both have entries with the same
  // NetworkIsolationKey.
  QuicCryptoClientConfigMap active_crypto_config_map_;
  base::MRUCache<NetworkIsolationKey,
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
  NetworkChangeNotifier::NetworkHandle default_network_;

  // Local address of socket that was created in CreateSession.
  IPEndPoint local_address_;
  // True if we need to check HttpServerProperties if QUIC was supported last
  // time.
  bool need_to_check_persisted_supports_quic_;
  bool prefer_aes_gcm_recorded_;

  NetworkConnection network_connection_;

  int num_push_streams_created_;

  QuicConnectivityMonitor connectivity_monitor_;

  const base::TickClock* tick_clock_;

  base::SequencedTaskRunner* task_runner_;

  SSLConfigService* const ssl_config_service_;

  // Whether NetworkIsolationKeys should be used for
  // |active_crypto_config_map_|. If false, there will just be one config with
  // an empty NetworkIsolationKey. Whether QuicSessionAliasKeys all have an
  // empty NIK is based on whether socket pools are respecting NIKs, but whether
  // those NIKs are also used when accessing |active_crypto_config_map_| is also
  // gated this, which is set based on whether HttpServerProperties is
  // respecting NIKs, as that data is fed into the crypto config map using the
  // corresponding NIK.
  const bool use_network_isolation_key_for_crypto_configs_;

  base::WeakPtrFactory<QuicStreamFactory> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuicStreamFactory);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_STREAM_FACTORY_H_
