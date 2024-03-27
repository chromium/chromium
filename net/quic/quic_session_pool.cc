// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool.h"

#include <memory>
#include <set>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "crypto/openssl_util.h"
#include "net/base/address_list.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/features.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_handle.h"
#include "net/base/proxy_delegate.h"
#include "net/base/session_usage.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/properties_based_quic_server_info.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_session_pool_direct_job.h"
#include "net/quic/quic_session_pool_job.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/udp_client_socket.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/null_decrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/proof_verifier.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_clock.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/boringssl/src/include/openssl/aead.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

namespace {

enum InitialRttEstimateSource {
  INITIAL_RTT_DEFAULT,
  INITIAL_RTT_CACHED,
  INITIAL_RTT_2G,
  INITIAL_RTT_3G,
  INITIAL_RTT_SOURCE_MAX,
};

std::string QuicPlatformNotificationToString(
    QuicPlatformNotification notification) {
  switch (notification) {
    case NETWORK_CONNECTED:
      return "OnNetworkConnected";
    case NETWORK_MADE_DEFAULT:
      return "OnNetworkMadeDefault";
    case NETWORK_DISCONNECTED:
      return "OnNetworkDisconnected";
    case NETWORK_SOON_TO_DISCONNECT:
      return "OnNetworkSoonToDisconnect";
    case NETWORK_IP_ADDRESS_CHANGED:
      return "OnIPAddressChanged";
    default:
      QUICHE_NOTREACHED();
      break;
  }
  return "InvalidNotification";
}

void HistogramCreateSessionFailure(enum CreateSessionFailure error) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.CreationError", error,
                            CREATION_ERROR_MAX);
}

void SetInitialRttEstimate(base::TimeDelta estimate,
                           enum InitialRttEstimateSource source,
                           quic::QuicConfig* config) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.InitialRttEsitmateSource", source,
                            INITIAL_RTT_SOURCE_MAX);
  if (estimate != base::TimeDelta()) {
    config->SetInitialRoundTripTimeUsToSend(
        base::checked_cast<uint64_t>(estimate.InMicroseconds()));
  }
}

// An implementation of quic::QuicCryptoClientConfig::ServerIdFilter that wraps
// an |origin_filter|.
class ServerIdOriginFilter
    : public quic::QuicCryptoClientConfig::ServerIdFilter {
 public:
  explicit ServerIdOriginFilter(
      const base::RepeatingCallback<bool(const GURL&)> origin_filter)
      : origin_filter_(origin_filter) {}

  bool Matches(const quic::QuicServerId& server_id) const override {
    if (origin_filter_.is_null()) {
      return true;
    }

    GURL url(base::StringPrintf("%s%s%s:%d", url::kHttpsScheme,
                                url::kStandardSchemeSeparator,
                                server_id.host().c_str(), server_id.port()));
    DCHECK(url.is_valid());
    return origin_filter_.Run(url);
  }

 private:
  const base::RepeatingCallback<bool(const GURL&)> origin_filter_;
};

std::set<std::string> HostsFromOrigins(std::set<HostPortPair> origins) {
  std::set<std::string> hosts;
  for (const auto& origin : origins) {
    hosts.insert(origin.host());
  }
  return hosts;
}

}  // namespace

QuicSessionRequest::QuicSessionRequest(QuicSessionPool* pool) : pool_(pool) {}

QuicSessionRequest::~QuicSessionRequest() {
  if (pool_ && !callback_.is_null()) {
    pool_->CancelRequest(this);
  }
}

int QuicSessionRequest::Request(
    url::SchemeHostPort destination,
    quic::ParsedQuicVersion quic_version,
    const ProxyChain& proxy_chain,
    const std::optional<NetworkTrafficAnnotationTag> proxy_annotation_tag,
    const HttpUserAgentSettings* http_user_agent_settings,
    SessionUsage session_usage,
    PrivacyMode privacy_mode,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool require_dns_https_alpn,
    int cert_verify_flags,
    const GURL& url,
    const NetLogWithSource& net_log,
    NetErrorDetails* net_error_details,
    CompletionOnceCallback failed_on_default_network_callback,
    CompletionOnceCallback callback) {
  DCHECK_EQ(quic_version.IsKnown(), !require_dns_https_alpn);
  DCHECK(net_error_details);
  DCHECK(callback_.is_null());
  DCHECK(host_resolution_callback_.is_null());
  DCHECK(pool_);

  net_error_details_ = net_error_details;
  failed_on_default_network_callback_ =
      std::move(failed_on_default_network_callback);

  session_key_ =
      QuicSessionKey(HostPortPair::FromURL(url), privacy_mode, proxy_chain,
                     session_usage, socket_tag, network_anonymization_key,
                     secure_dns_policy, require_dns_https_alpn);
  bool use_dns_aliases = session_usage == SessionUsage::kProxy ? false : true;

  int rv = pool_->RequestSession(
      session_key_, std::move(destination), quic_version,
      std::move(proxy_annotation_tag), http_user_agent_settings, priority,
      use_dns_aliases, cert_verify_flags, url, net_log, this);
  if (rv == ERR_IO_PENDING) {
    net_log_ = net_log;
    callback_ = std::move(callback);
  } else {
    DCHECK(!expect_on_host_resolution_);
    pool_ = nullptr;
  }

  if (rv == OK) {
    DCHECK(session_);
  }
  return rv;
}

bool QuicSessionRequest::WaitForHostResolution(
    CompletionOnceCallback callback) {
  DCHECK(host_resolution_callback_.is_null());
  if (expect_on_host_resolution_) {
    host_resolution_callback_ = std::move(callback);
  }
  return expect_on_host_resolution_;
}

void QuicSessionRequest::ExpectOnHostResolution() {
  expect_on_host_resolution_ = true;
}

void QuicSessionRequest::OnHostResolutionComplete(int rv) {
  DCHECK(expect_on_host_resolution_);
  expect_on_host_resolution_ = false;
  if (!host_resolution_callback_.is_null()) {
    std::move(host_resolution_callback_).Run(rv);
  }
}

bool QuicSessionRequest::WaitForQuicSessionCreation(
    CompletionOnceCallback callback) {
  DCHECK(create_session_callback_.is_null());
  if (expect_on_quic_session_creation_) {
    create_session_callback_ = std::move(callback);
  }
  return expect_on_quic_session_creation_;
}

void QuicSessionRequest::ExpectQuicSessionCreation() {
  expect_on_quic_session_creation_ = true;
}

void QuicSessionRequest::OnQuicSessionCreationComplete(int rv) {
  // DCHECK(expect_on_quic_session_creation_);
  expect_on_quic_session_creation_ = false;
  if (!create_session_callback_.is_null()) {
    std::move(create_session_callback_).Run(rv);
  }
}

void QuicSessionRequest::OnRequestComplete(int rv) {
  pool_ = nullptr;
  std::move(callback_).Run(rv);
}

void QuicSessionRequest::OnConnectionFailedOnDefaultNetwork() {
  if (!failed_on_default_network_callback_.is_null()) {
    std::move(failed_on_default_network_callback_).Run(OK);
  }
}

base::TimeDelta QuicSessionRequest::GetTimeDelayForWaitingJob() const {
  if (!pool_) {
    return base::TimeDelta();
  }
  return pool_->GetTimeDelayForWaitingJob(session_key_);
}

void QuicSessionRequest::SetPriority(RequestPriority priority) {
  if (pool_) {
    pool_->SetRequestPriority(this, priority);
  }
}

std::unique_ptr<QuicChromiumClientSession::Handle>
QuicSessionRequest::ReleaseSessionHandle() {
  if (!session_ || !session_->IsConnected()) {
    return nullptr;
  }

  return std::move(session_);
}

void QuicSessionRequest::SetSession(
    std::unique_ptr<QuicChromiumClientSession::Handle> session) {
  session_ = std::move(session);
}

bool QuicSessionRequest::CanUseExistingSession(
    const GURL& url,
    const ProxyChain& proxy_chain,
    PrivacyMode privacy_mode,
    SessionUsage session_usage,
    const SocketTag& socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool require_dns_https_alpn,
    const url::SchemeHostPort& destination) const {
  return pool_->CanUseExistingSession(
      QuicSessionKey(HostPortPair::FromURL(url), privacy_mode, proxy_chain,
                     session_usage, socket_tag, network_anonymization_key,
                     secure_dns_policy, require_dns_https_alpn),
      destination);
}

QuicSessionPool::QuicSessionAliasKey::QuicSessionAliasKey(
    url::SchemeHostPort destination,
    QuicSessionKey session_key)
    : destination_(std::move(destination)),
      session_key_(std::move(session_key)) {}

bool QuicSessionPool::QuicSessionAliasKey::operator<(
    const QuicSessionAliasKey& other) const {
  return std::tie(destination_, session_key_) <
         std::tie(other.destination_, other.session_key_);
}

bool QuicSessionPool::QuicSessionAliasKey::operator==(
    const QuicSessionAliasKey& other) const {
  return destination_ == other.destination_ &&
         session_key_ == other.session_key_;
}

QuicSessionPool::QuicCryptoClientConfigOwner::QuicCryptoClientConfigOwner(
    std::unique_ptr<quic::ProofVerifier> proof_verifier,
    std::unique_ptr<quic::QuicClientSessionCache> session_cache,
    QuicSessionPool* quic_session_pool)
    : config_(std::move(proof_verifier), std::move(session_cache)),
      clock_(base::DefaultClock::GetInstance()),
      quic_session_pool_(quic_session_pool) {
  DCHECK(quic_session_pool_);
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&QuicCryptoClientConfigOwner::OnMemoryPressure,
                          base::Unretained(this)));
  if (quic_session_pool_->ssl_config_service_->GetSSLContextConfig()
          .PostQuantumKeyAgreementEnabled()) {
    config_.set_preferred_groups({SSL_GROUP_X25519_KYBER768_DRAFT00,
                                  SSL_GROUP_X25519, SSL_GROUP_SECP256R1,
                                  SSL_GROUP_SECP384R1});
  }
}
QuicSessionPool::QuicCryptoClientConfigOwner::~QuicCryptoClientConfigOwner() {
  DCHECK_EQ(num_refs_, 0);
}

void QuicSessionPool::QuicCryptoClientConfigOwner::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  quic::SessionCache* session_cache = config_.session_cache();
  if (!session_cache) {
    return;
  }
  time_t now = clock_->Now().ToTimeT();
  uint64_t now_u64 = 0;
  if (now > 0) {
    now_u64 = static_cast<uint64_t>(now);
  }
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      session_cache->RemoveExpiredEntries(
          quic::QuicWallTime::FromUNIXSeconds(now_u64));
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      session_cache->Clear();
      break;
  }
}

QuicSessionPool::CryptoClientConfigHandle::CryptoClientConfigHandle(
    const QuicCryptoClientConfigMap::iterator& map_iterator)
    : map_iterator_(map_iterator) {
  DCHECK_GE(map_iterator_->second->num_refs(), 0);
  map_iterator->second->AddRef();
}

QuicSessionPool::CryptoClientConfigHandle::~CryptoClientConfigHandle() {
  DCHECK_GT(map_iterator_->second->num_refs(), 0);
  map_iterator_->second->ReleaseRef();
  if (map_iterator_->second->num_refs() == 0) {
    map_iterator_->second->quic_session_pool()->OnAllCryptoClientRefReleased(
        map_iterator_);
  }
}

quic::QuicCryptoClientConfig*
QuicSessionPool::CryptoClientConfigHandle::GetConfig() const {
  return map_iterator_->second->config();
}

QuicSessionPool::QuicSessionPool(
    NetLog* net_log,
    HostResolver* host_resolver,
    SSLConfigService* ssl_config_service,
    ClientSocketFactory* client_socket_factory,
    HttpServerProperties* http_server_properties,
    CertVerifier* cert_verifier,
    TransportSecurityState* transport_security_state,
    ProxyDelegate* proxy_delegate,
    SCTAuditingDelegate* sct_auditing_delegate,
    SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
    QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory,
    QuicContext* quic_context)
    : net_log_(
          NetLogWithSource::Make(net_log, NetLogSourceType::QUIC_SESSION_POOL)),
      host_resolver_(host_resolver),
      client_socket_factory_(client_socket_factory),
      http_server_properties_(http_server_properties),
      cert_verifier_(cert_verifier),
      transport_security_state_(transport_security_state),
      proxy_delegate_(proxy_delegate),
      sct_auditing_delegate_(sct_auditing_delegate),
      quic_crypto_client_stream_factory_(quic_crypto_client_stream_factory),
      random_generator_(quic_context->random_generator()),
      clock_(quic_context->clock()),
      // TODO(vasilvv): figure out how to avoid having multiple copies of
      // QuicParams.
      params_(*quic_context->params()),
      clock_skew_detector_(base::TimeTicks::Now(), base::Time::Now()),
      socket_performance_watcher_factory_(socket_performance_watcher_factory),
      recent_crypto_config_map_(kMaxRecentCryptoConfigs),
      config_(InitializeQuicConfig(*quic_context->params())),
      ping_timeout_(quic::QuicTime::Delta::FromSeconds(quic::kPingTimeoutSecs)),
      reduced_ping_timeout_(quic::QuicTime::Delta::FromMicroseconds(
          quic_context->params()->reduced_ping_timeout.InMicroseconds())),
      retransmittable_on_wire_timeout_(quic::QuicTime::Delta::FromMicroseconds(
          quic_context->params()
              ->retransmittable_on_wire_timeout.InMicroseconds())),
      yield_after_packets_(kQuicYieldAfterPacketsRead),
      yield_after_duration_(quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds)),
      default_network_(handles::kInvalidNetworkHandle),
      connectivity_monitor_(default_network_),
      ssl_config_service_(ssl_config_service),
      use_network_anonymization_key_for_crypto_configs_(
          NetworkAnonymizationKey::IsPartitioningEnabled()) {
  DCHECK(transport_security_state_);
  DCHECK(http_server_properties_);
  if (params_.disable_tls_zero_rtt) {
    SetQuicFlag(quic_disable_client_tls_zero_rtt, true);
  }
  InitializeMigrationOptions();
  cert_verifier_->AddObserver(this);
  CertDatabase::GetInstance()->AddObserver(this);
}

QuicSessionPool::~QuicSessionPool() {
  UMA_HISTOGRAM_COUNTS_1000("Net.NumQuicSessionsAtShutdown",
                            all_sessions_.size());
  CloseAllSessions(ERR_ABORTED, quic::QUIC_CONNECTION_CANCELLED);
  while (!all_sessions_.empty()) {
    delete all_sessions_.begin()->first;
    all_sessions_.erase(all_sessions_.begin());
  }
  active_jobs_.clear();

  DCHECK(dns_aliases_by_session_key_.empty());

  // This should have been moved to the recent map when all consumers of
  // QuicCryptoClientConfigs were deleted, in the above lines.
  DCHECK(active_crypto_config_map_.empty());

  CertDatabase::GetInstance()->RemoveObserver(this);
  cert_verifier_->RemoveObserver(this);
  if (params_.close_sessions_on_ip_change ||
      params_.goaway_sessions_on_ip_change) {
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
  }
  if (NetworkChangeNotifier::AreNetworkHandlesSupported()) {
    NetworkChangeNotifier::RemoveNetworkObserver(this);
  }
}

bool QuicSessionPool::CanUseExistingSession(
    const QuicSessionKey& session_key,
    const url::SchemeHostPort& destination) const {
  if (base::Contains(active_sessions_, session_key)) {
    return true;
  }

  for (const auto& key_value : active_sessions_) {
    QuicChromiumClientSession* session = key_value.second;
    const auto& it = all_sessions_.find(session);
    if ((it != all_sessions_.end()) &&
        (destination == it->second.destination()) &&
        session->CanPool(session_key.host(), session_key)) {
      return true;
    }
  }

  return false;
}

int QuicSessionPool::RequestSession(
    const QuicSessionKey& session_key,
    url::SchemeHostPort destination,
    quic::ParsedQuicVersion quic_version,
    const std::optional<NetworkTrafficAnnotationTag> proxy_annotation_tag,
    const HttpUserAgentSettings* http_user_agent_settings,
    RequestPriority priority,
    bool use_dns_aliases,
    int cert_verify_flags,
    const GURL& url,
    const NetLogWithSource& net_log,
    QuicSessionRequest* request) {
  if (clock_skew_detector_.ClockSkewDetected(base::TimeTicks::Now(),
                                             base::Time::Now())) {
    MarkAllActiveSessionsGoingAway(kClockSkewDetected);
  }
  DCHECK(HostPortPair(session_key.server_id().host(),
                      session_key.server_id().port())
             .Equals(HostPortPair::FromURL(url)));

  // Use active session for |session_key| if such exists.
  auto active_session = active_sessions_.find(session_key);
  if (active_session != active_sessions_.end()) {
    QuicChromiumClientSession* session = active_session->second;
    request->SetSession(session->CreateHandle(std::move(destination)));
    return OK;
  }

  // Associate with active job to |session_key| if such exists.
  auto active_job = active_jobs_.find(session_key);
  if (active_job != active_jobs_.end()) {
    const NetLogWithSource& job_net_log = active_job->second->net_log();
    job_net_log.AddEventReferencingSource(
        NetLogEventType::QUIC_SESSION_POOL_JOB_BOUND_TO_HTTP_STREAM_JOB,
        net_log.source());
    net_log.AddEventReferencingSource(
        NetLogEventType::HTTP_STREAM_JOB_BOUND_TO_QUIC_SESSION_POOL_JOB,
        job_net_log.source());
    active_job->second->AddRequest(request);
    return ERR_IO_PENDING;
  }

  // Pool to active session to |destination| if possible.
  if (!active_sessions_.empty()) {
    for (const auto& key_value : active_sessions_) {
      QuicChromiumClientSession* session = key_value.second;
      if (destination == all_sessions_[session].destination() &&
          session->CanPool(session_key.server_id().host(), session_key)) {
        request->SetSession(session->CreateHandle(std::move(destination)));
        return OK;
      }
    }
  }

  // TODO(rtenneti): |task_runner_| is used by the Job. Initialize task_runner_
  // in the constructor after WebRequestActionWithThreadsTest.* tests are fixed.
  if (!task_runner_) {
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }

  if (!tick_clock_) {
    tick_clock_ = base::DefaultTickClock::GetInstance();
  }

  // If a proxy is in use, then a traffic annotation is required.
  if (!session_key.proxy_chain().is_direct()) {
    DCHECK(proxy_annotation_tag);
    DCHECK(http_user_agent_settings);
  }

  QuicSessionAliasKey key(destination, session_key);
  std::unique_ptr<Job> job = std::make_unique<DirectJob>(
      this, quic_version, host_resolver_, key,
      CreateCryptoConfigHandle(session_key.network_anonymization_key()),
      WasQuicRecentlyBroken(session_key),
      params_.retry_on_alternate_network_before_handshake, priority,
      use_dns_aliases, session_key.require_dns_https_alpn(), cert_verify_flags,
      net_log);
  int rv = job->Run(base::BindOnce(&QuicSessionPool::OnJobComplete,
                                   weak_factory_.GetWeakPtr(), job.get()));
  if (rv == ERR_IO_PENDING) {
    job->AddRequest(request);
    active_jobs_[session_key] = std::move(job);
    return rv;
  }
  if (rv == OK) {
    auto it = active_sessions_.find(session_key);
    DCHECK(it != active_sessions_.end());
    if (it == active_sessions_.end()) {
      return ERR_QUIC_PROTOCOL_ERROR;
    }
    QuicChromiumClientSession* session = it->second;
    request->SetSession(session->CreateHandle(std::move(destination)));
  }
  return rv;
}

void QuicSessionPool::OnSessionGoingAway(QuicChromiumClientSession* session) {
  const AliasSet& aliases = session_aliases_[session];
  for (const auto& alias : aliases) {
    const QuicSessionKey& session_key = alias.session_key();
    DCHECK(active_sessions_.count(session_key));
    DCHECK_EQ(session, active_sessions_[session_key]);
    // Track sessions which have recently gone away so that we can disable
    // port suggestions.
    if (session->goaway_received()) {
      gone_away_aliases_.insert(alias);
    }

    active_sessions_.erase(session_key);
    ProcessGoingAwaySession(session, session_key.server_id(), true);
  }
  ProcessGoingAwaySession(session, all_sessions_[session].server_id(), false);
  if (!aliases.empty()) {
    DCHECK(base::Contains(session_peer_ip_, session));
    const IPEndPoint peer_address = session_peer_ip_[session];
    ip_aliases_[peer_address].erase(session);
    if (ip_aliases_[peer_address].empty()) {
      ip_aliases_.erase(peer_address);
    }
    session_peer_ip_.erase(session);
  }
  UnmapSessionFromSessionAliases(session);
}

void QuicSessionPool::OnSessionClosed(QuicChromiumClientSession* session) {
  DCHECK_EQ(0u, session->GetNumActiveStreams());
  OnSessionGoingAway(session);
  delete session;
  all_sessions_.erase(session);
}

void QuicSessionPool::OnBlackholeAfterHandshakeConfirmed(
    QuicChromiumClientSession* session) {
  // Reduce PING timeout when connection blackholes after the handshake.
  if (ping_timeout_ > reduced_ping_timeout_) {
    ping_timeout_ = reduced_ping_timeout_;
  }
}

void QuicSessionPool::CancelRequest(QuicSessionRequest* request) {
  auto job_iter = active_jobs_.find(request->session_key());
  CHECK(job_iter != active_jobs_.end());
  job_iter->second->RemoveRequest(request);
}

void QuicSessionPool::SetRequestPriority(QuicSessionRequest* request,
                                         RequestPriority priority) {
  auto job_iter = active_jobs_.find(request->session_key());
  if (job_iter == active_jobs_.end()) {
    return;
  }
  job_iter->second->SetPriority(priority);
}

void QuicSessionPool::CloseAllSessions(int error,
                                       quic::QuicErrorCode quic_error) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_POOL_CLOSE_ALL_SESSIONS);
  base::UmaHistogramSparse("Net.QuicSession.CloseAllSessionsError", -error);
  while (!active_sessions_.empty()) {
    size_t initial_size = active_sessions_.size();
    active_sessions_.begin()->second->CloseSessionOnError(
        error, quic_error,
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    DCHECK_NE(initial_size, active_sessions_.size());
  }
  while (!all_sessions_.empty()) {
    size_t initial_size = all_sessions_.size();
    all_sessions_.begin()->first->CloseSessionOnError(
        error, quic_error,
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    DCHECK_NE(initial_size, all_sessions_.size());
  }
  DCHECK(all_sessions_.empty());
}

base::Value QuicSessionPool::QuicSessionPoolInfoToValue() const {
  base::Value::List list;

  for (const auto& active_session : active_sessions_) {
    const quic::QuicServerId& server_id = active_session.first.server_id();
    QuicChromiumClientSession* session = active_session.second;
    const AliasSet& aliases = session_aliases_.find(session)->second;
    // Only add a session to the list once.
    if (server_id == aliases.begin()->server_id()) {
      std::set<HostPortPair> hosts;
      for (const auto& alias : aliases) {
        hosts.insert(
            HostPortPair(alias.server_id().host(), alias.server_id().port()));
      }
      list.Append(session->GetInfoAsValue(hosts));
    }
  }
  return base::Value(std::move(list));
}

void QuicSessionPool::ClearCachedStatesInCryptoConfig(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  ServerIdOriginFilter filter(origin_filter);
  for (const auto& crypto_config : active_crypto_config_map_) {
    crypto_config.second->config()->ClearCachedStates(filter);
  }

  for (const auto& crypto_config : recent_crypto_config_map_) {
    crypto_config.second->config()->ClearCachedStates(filter);
  }
}

int QuicSessionPool::ConnectAndConfigureSocket(CompletionOnceCallback callback,
                                               DatagramClientSocket* socket,
                                               IPEndPoint addr,
                                               handles::NetworkHandle network,
                                               const SocketTag& socket_tag) {
  socket->UseNonBlockingIO();

  int rv;
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  CompletionOnceCallback connect_callback =
      base::BindOnce(&QuicSessionPool::FinishConnectAndConfigureSocket,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.first), socket, socket_tag);
  if (!params_.migrate_sessions_on_network_change_v2) {
    rv = socket->ConnectAsync(addr, std::move(connect_callback));
  } else if (network == handles::kInvalidNetworkHandle) {
    // If caller leaves network unspecified, use current default network.
    rv = socket->ConnectUsingDefaultNetworkAsync(addr,
                                                 std::move(connect_callback));
  } else {
    rv = socket->ConnectUsingNetworkAsync(network, addr,
                                          std::move(connect_callback));
  }
  // Both callbacks within `split_callback` will always be run asynchronously,
  // even if a Connect call returns synchronously. Therefore we always return
  // ERR_IO_PENDING.
  if (rv != ERR_IO_PENDING) {
    FinishConnectAndConfigureSocket(std::move(split_callback.second), socket,
                                    socket_tag, rv);
  }
  return ERR_IO_PENDING;
}

void QuicSessionPool::FinishConnectAndConfigureSocket(
    CompletionOnceCallback callback,
    DatagramClientSocket* socket,
    const SocketTag& socket_tag,
    int rv) {
  if (rv != OK) {
    OnFinishConnectAndConfigureSocketError(
        std::move(callback), CREATION_ERROR_CONNECTING_SOCKET, rv);
    return;
  }

  socket->ApplySocketTag(socket_tag);

  rv = socket->SetReceiveBufferSize(kQuicSocketReceiveBufferSize);
  if (rv != OK) {
    OnFinishConnectAndConfigureSocketError(
        std::move(callback), CREATION_ERROR_SETTING_RECEIVE_BUFFER, rv);
    return;
  }

  rv = socket->SetDoNotFragment();
  // SetDoNotFragment is not implemented on all platforms, so ignore errors.
  if (rv != OK && rv != ERR_NOT_IMPLEMENTED) {
    OnFinishConnectAndConfigureSocketError(
        std::move(callback), CREATION_ERROR_SETTING_DO_NOT_FRAGMENT, rv);
    return;
  }

  if (base::FeatureList::IsEnabled(net::features::kReceiveEcn)) {
    rv = socket->SetRecvTos();
    if (rv != OK) {
      OnFinishConnectAndConfigureSocketError(
          std::move(callback), CREATION_ERROR_SETTING_RECEIVE_ECN, rv);
      return;
    }
  }

  // Set a buffer large enough to contain the initial CWND's worth of packet
  // to work around the problem with CHLO packets being sent out with the
  // wrong encryption level, when the send buffer is full.
  rv = socket->SetSendBufferSize(quic::kMaxOutgoingPacketSize * 20);
  if (rv != OK) {
    OnFinishConnectAndConfigureSocketError(
        std::move(callback), CREATION_ERROR_SETTING_SEND_BUFFER, rv);
    return;
  }

  if (params_.ios_network_service_type > 0) {
    socket->SetIOSNetworkServiceType(params_.ios_network_service_type);
  }

  socket->GetLocalAddress(&local_address_);
  if (need_to_check_persisted_supports_quic_) {
    need_to_check_persisted_supports_quic_ = false;
    if (http_server_properties_->WasLastLocalAddressWhenQuicWorked(
            local_address_.address())) {
      is_quic_known_to_work_on_current_network_ = true;
      // Clear the persisted IP address, in case the network no longer supports
      // QUIC so the next restart will require confirmation. It will be
      // re-persisted when the first job completes successfully.
      http_server_properties_->ClearLastLocalAddressWhenQuicWorked();
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&QuicSessionPool::DoCallback, weak_factory_.GetWeakPtr(),
                     std::move(callback), rv));
}

void QuicSessionPool::OnFinishConnectAndConfigureSocketError(
    CompletionOnceCallback callback,
    enum CreateSessionFailure error,
    int rv) {
  DCHECK(callback);
  HistogramCreateSessionFailure(error);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&QuicSessionPool::DoCallback, weak_factory_.GetWeakPtr(),
                     std::move(callback), rv));
}

void QuicSessionPool::DoCallback(CompletionOnceCallback callback, int rv) {
  std::move(callback).Run(rv);
}

int QuicSessionPool::ConfigureSocket(DatagramClientSocket* socket,
                                     IPEndPoint addr,
                                     handles::NetworkHandle network,
                                     const SocketTag& socket_tag) {
  socket->UseNonBlockingIO();

  int rv;
  if (!params_.migrate_sessions_on_network_change_v2) {
    rv = socket->Connect(addr);
  } else if (network == handles::kInvalidNetworkHandle) {
    // If caller leaves network unspecified, use current default network.
    rv = socket->ConnectUsingDefaultNetwork(addr);
  } else {
    rv = socket->ConnectUsingNetwork(network, addr);
  }
  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_CONNECTING_SOCKET);
    return rv;
  }

  socket->ApplySocketTag(socket_tag);

  rv = socket->SetReceiveBufferSize(kQuicSocketReceiveBufferSize);
  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_SETTING_RECEIVE_BUFFER);
    return rv;
  }

  rv = socket->SetDoNotFragment();
  // SetDoNotFragment is not implemented on all platforms, so ignore errors.
  if (rv != OK && rv != ERR_NOT_IMPLEMENTED) {
    HistogramCreateSessionFailure(CREATION_ERROR_SETTING_DO_NOT_FRAGMENT);
    return rv;
  }

  if (base::FeatureList::IsEnabled(net::features::kReceiveEcn)) {
    rv = socket->SetRecvTos();
    if (rv != OK) {
      HistogramCreateSessionFailure(CREATION_ERROR_SETTING_RECEIVE_ECN);
      return rv;
    }
  }

  // Set a buffer large enough to contain the initial CWND's worth of packet
  // to work around the problem with CHLO packets being sent out with the
  // wrong encryption level, when the send buffer is full.
  rv = socket->SetSendBufferSize(quic::kMaxOutgoingPacketSize * 20);
  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_SETTING_SEND_BUFFER);
    return rv;
  }

  if (params_.ios_network_service_type > 0) {
    socket->SetIOSNetworkServiceType(params_.ios_network_service_type);
  }

  socket->GetLocalAddress(&local_address_);
  if (need_to_check_persisted_supports_quic_) {
    need_to_check_persisted_supports_quic_ = false;
    if (http_server_properties_->WasLastLocalAddressWhenQuicWorked(
            local_address_.address())) {
      is_quic_known_to_work_on_current_network_ = true;
      // Clear the persisted IP address, in case the network no longer supports
      // QUIC so the next restart will require confirmation. It will be
      // re-persisted when the first job completes successfully.
      http_server_properties_->ClearLastLocalAddressWhenQuicWorked();
    }
  }

  return OK;
}

handles::NetworkHandle QuicSessionPool::FindAlternateNetwork(
    handles::NetworkHandle old_network) {
  // Find a new network that sessions bound to |old_network| can be migrated to.
  NetworkChangeNotifier::NetworkList network_list;
  NetworkChangeNotifier::GetConnectedNetworks(&network_list);
  for (handles::NetworkHandle new_network : network_list) {
    if (new_network != old_network) {
      return new_network;
    }
  }
  return handles::kInvalidNetworkHandle;
}

std::unique_ptr<DatagramClientSocket> QuicSessionPool::CreateSocket(
    NetLog* net_log,
    const NetLogSource& source) {
  auto socket = client_socket_factory_->CreateDatagramClientSocket(
      DatagramSocket::DEFAULT_BIND, net_log, source);
  if (params_.enable_socket_recv_optimization) {
    socket->EnableRecvOptimization();
  }
  return socket;
}

void QuicSessionPool::OnIPAddressChanged() {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_POOL_ON_IP_ADDRESS_CHANGED);
  CollectDataOnPlatformNotification(NETWORK_IP_ADDRESS_CHANGED,
                                    handles::kInvalidNetworkHandle);
  // Do nothing if connection migration is turned on.
  if (params_.migrate_sessions_on_network_change_v2) {
    return;
  }

  connectivity_monitor_.OnIPAddressChanged();

  set_is_quic_known_to_work_on_current_network(false);
  if (params_.close_sessions_on_ip_change) {
    CloseAllSessions(ERR_NETWORK_CHANGED, quic::QUIC_IP_ADDRESS_CHANGED);
  } else {
    DCHECK(params_.goaway_sessions_on_ip_change);
    MarkAllActiveSessionsGoingAway(kIPAddressChanged);
  }
}

void QuicSessionPool::OnNetworkConnected(handles::NetworkHandle network) {
  CollectDataOnPlatformNotification(NETWORK_CONNECTED, network);
  if (params_.migrate_sessions_on_network_change_v2) {
    net_log_.AddEvent(NetLogEventType::QUIC_SESSION_POOL_PLATFORM_NOTIFICATION,
                      [&] {
                        base::Value::Dict dict;
                        dict.Set("signal", "OnNetworkConnected");
                        dict.Set("network", base::NumberToString(network));
                        return dict;
                      });
  }
  // Broadcast network connected to all sessions.
  // If migration is not turned on, session will not migrate but collect data.
  auto it = all_sessions_.begin();
  // Sessions may be deleted while iterating through the map.
  while (it != all_sessions_.end()) {
    QuicChromiumClientSession* session = it->first;
    ++it;
    session->OnNetworkConnected(network);
  }
}

void QuicSessionPool::OnNetworkDisconnected(handles::NetworkHandle network) {
  CollectDataOnPlatformNotification(NETWORK_DISCONNECTED, network);
  if (params_.migrate_sessions_on_network_change_v2) {
    net_log_.AddEvent(NetLogEventType::QUIC_SESSION_POOL_PLATFORM_NOTIFICATION,
                      [&] {
                        base::Value::Dict dict;
                        dict.Set("signal", "OnNetworkDisconnected");
                        dict.Set("network", base::NumberToString(network));
                        return dict;
                      });
  }
  // Broadcast network disconnected to all sessions.
  // If migration is not turned on, session will not migrate but collect data.
  auto it = all_sessions_.begin();
  // Sessions may be deleted while iterating through the map.
  while (it != all_sessions_.end()) {
    QuicChromiumClientSession* session = it->first;
    ++it;
    session->OnNetworkDisconnectedV2(/*disconnected_network*/ network);
  }
}

// This method is expected to only be called when migrating from Cellular to
// WiFi on Android, and should always be preceded by OnNetworkMadeDefault().
void QuicSessionPool::OnNetworkSoonToDisconnect(
    handles::NetworkHandle network) {
  CollectDataOnPlatformNotification(NETWORK_SOON_TO_DISCONNECT, network);
}

void QuicSessionPool::OnNetworkMadeDefault(handles::NetworkHandle network) {
  CollectDataOnPlatformNotification(NETWORK_MADE_DEFAULT, network);
  connectivity_monitor_.OnDefaultNetworkUpdated(network);

  // Clear alternative services that were marked as broken until default network
  // changes.
  if (params_.retry_on_alternate_network_before_handshake &&
      default_network_ != handles::kInvalidNetworkHandle &&
      network != default_network_) {
    http_server_properties_->OnDefaultNetworkChanged();
  }

  DCHECK_NE(handles::kInvalidNetworkHandle, network);
  default_network_ = network;

  if (params_.migrate_sessions_on_network_change_v2) {
    net_log_.AddEvent(NetLogEventType::QUIC_SESSION_POOL_PLATFORM_NOTIFICATION,
                      [&] {
                        base::Value::Dict dict;
                        dict.Set("signal", "OnNetworkMadeDefault");
                        dict.Set("network", base::NumberToString(network));
                        return dict;
                      });
  }

  auto it = all_sessions_.begin();
  // Sessions may be deleted while iterating through the map.
  while (it != all_sessions_.end()) {
    QuicChromiumClientSession* session = it->first;
    ++it;
    session->OnNetworkMadeDefault(network);
  }
  if (params_.migrate_sessions_on_network_change_v2) {
    set_is_quic_known_to_work_on_current_network(false);
  }
}

void QuicSessionPool::OnTrustStoreChanged() {
  // We should flush the sessions if we removed trust from a
  // cert, because a previously trusted server may have become
  // untrusted.
  //
  // We should not flush the sessions if we added trust to a cert.
  //
  // Since the OnTrustStoreChanged method doesn't tell us what
  // kind of change it is, we have to flush the socket
  // pools to be safe.
  MarkAllActiveSessionsGoingAway(kCertDBChanged);
}

void QuicSessionPool::OnCertVerifierChanged() {
  // Flush sessions if the CertCerifier configuration has changed.
  MarkAllActiveSessionsGoingAway(kCertVerifierChanged);
}

void QuicSessionPool::set_is_quic_known_to_work_on_current_network(
    bool is_quic_known_to_work_on_current_network) {
  is_quic_known_to_work_on_current_network_ =
      is_quic_known_to_work_on_current_network;
  if (!(local_address_ == IPEndPoint())) {
    if (is_quic_known_to_work_on_current_network_) {
      http_server_properties_->SetLastLocalAddressWhenQuicWorked(
          local_address_.address());
    } else {
      http_server_properties_->ClearLastLocalAddressWhenQuicWorked();
    }
  }
}

base::TimeDelta QuicSessionPool::GetTimeDelayForWaitingJob(
    const QuicSessionKey& session_key) {
  // If |is_quic_known_to_work_on_current_network_| is false, then one of the
  // following is true:
  // 1) This is startup and QuicSessionPool::CreateSession() and
  // ConfigureSocket() have yet to be called, and it is not yet known
  // if the current network is the last one where QUIC worked.
  // 2) Startup has been completed, and QUIC has not been used
  // successfully since startup, or on this network before.
  if (!is_quic_known_to_work_on_current_network_) {
    // If |need_to_check_persisted_supports_quic_| is false, this is case 1)
    // above. If HasLastLocalAddressWhenQuicWorked() is also true, then there's
    // a chance the current network is the last one on which QUIC worked. So
    // only delay the request if there's no chance that is the case.
    if (!need_to_check_persisted_supports_quic_ ||
        !http_server_properties_->HasLastLocalAddressWhenQuicWorked()) {
      return base::TimeDelta();
    }
  }

  // QUIC was recently broken. Do not delay the main job.
  if (WasQuicRecentlyBroken(session_key)) {
    return base::TimeDelta();
  }

  int64_t srtt = 1.5 * GetServerNetworkStatsSmoothedRttInMicroseconds(
                           session_key.server_id(),
                           session_key.network_anonymization_key());
  // Picked 300ms based on mean time from
  // Net.QuicSession.HostResolution.HandshakeConfirmedTime histogram.
  const int kDefaultRTT = 300 * quic::kNumMicrosPerMilli;
  if (!srtt) {
    srtt = kDefaultRTT;
  }
  return base::Microseconds(srtt);
}

const std::set<std::string>& QuicSessionPool::GetDnsAliasesForSessionKey(
    const QuicSessionKey& key) const {
  auto it = dns_aliases_by_session_key_.find(key);

  if (it == dns_aliases_by_session_key_.end()) {
    static const base::NoDestructor<std::set<std::string>> emptyvector_result;
    return *emptyvector_result;
  }

  return it->second;
}

quic::ParsedQuicVersion QuicSessionPool::SelectQuicVersion(
    const quic::ParsedQuicVersion& known_quic_version,
    const ConnectionEndpointMetadata& metadata,
    bool svcb_optional) const {
  if (metadata.supported_protocol_alpns.empty()) {
    // `metadata` doesn't contain QUIC ALPN. If we know the QUIC ALPN to use
    // externally, i.e. via Alt-Svc, use it in SVCB-optional mode. Otherwise,
    // the endpoint associated with `metadata` is not eligible for QUIC.
    return svcb_optional ? known_quic_version
                         : quic::ParsedQuicVersion::Unsupported();
  }

  // Otherwise, `metadata` came from an HTTPS/SVCB record. We can use
  // QUIC if a suitable match is found in the record's ALPN list.
  // Additionally, if this connection attempt came from Alt-Svc, the DNS
  // result must be consistent with it. See
  // https://datatracker.ietf.org/doc/html/rfc9460#name-interaction-with-alt-svc
  if (known_quic_version.IsKnown()) {
    std::string expected_alpn = quic::AlpnForVersion(known_quic_version);
    if (base::Contains(metadata.supported_protocol_alpns,
                       quic::AlpnForVersion(known_quic_version))) {
      return known_quic_version;
    }
    return quic::ParsedQuicVersion::Unsupported();
  }

  for (const auto& alpn : metadata.supported_protocol_alpns) {
    for (const auto& supported_version : supported_versions()) {
      if (alpn == AlpnForVersion(supported_version)) {
        return supported_version;
      }
    }
  }

  return quic::ParsedQuicVersion::Unsupported();
}

bool QuicSessionPool::HasMatchingIpSession(
    const QuicSessionAliasKey& key,
    const std::vector<IPEndPoint>& ip_endpoints,
    const std::set<std::string>& aliases,
    bool use_dns_aliases) {
  const quic::QuicServerId& server_id(key.server_id());
  DCHECK(!HasActiveSession(key.session_key()));
  for (const auto& address : ip_endpoints) {
    if (!base::Contains(ip_aliases_, address)) {
      continue;
    }

    const SessionSet& sessions = ip_aliases_[address];
    for (QuicChromiumClientSession* session : sessions) {
      if (!session->CanPool(server_id.host(), key.session_key())) {
        continue;
      }
      active_sessions_[key.session_key()] = session;

      std::set<std::string> dns_aliases;
      if (use_dns_aliases) {
        dns_aliases = aliases;
      }

      MapSessionToAliasKey(session, key, std::move(dns_aliases));

      return true;
    }
  }
  return false;
}

void QuicSessionPool::OnJobComplete(Job* job, int rv) {
  auto iter = active_jobs_.find(job->key().session_key());
  DCHECK(iter != active_jobs_.end());
  if (rv == OK) {
    if (!is_quic_known_to_work_on_current_network_) {
      set_is_quic_known_to_work_on_current_network(true);
    }

    auto session_it = active_sessions_.find(job->key().session_key());
    CHECK(session_it != active_sessions_.end());
    QuicChromiumClientSession* session = session_it->second;
    for (QuicSessionRequest* request : iter->second->requests()) {
      // Do not notify |request| yet.
      request->SetSession(session->CreateHandle(job->key().destination()));
    }
  }

  for (QuicSessionRequest* request : iter->second->requests()) {
    // Even though we're invoking callbacks here, we don't need to worry
    // about |this| being deleted, because the pool is owned by the
    // profile which can not be deleted via callbacks.
    if (rv < 0) {
      job->PopulateNetErrorDetails(request->net_error_details());
    }
    request->OnRequestComplete(rv);
  }
  active_jobs_.erase(iter);
}

bool QuicSessionPool::HasActiveSession(
    const QuicSessionKey& session_key) const {
  return base::Contains(active_sessions_, session_key);
}

bool QuicSessionPool::HasActiveJob(const QuicSessionKey& session_key) const {
  return base::Contains(active_jobs_, session_key);
}

int QuicSessionPool::CreateSessionSync(
    const QuicSessionAliasKey& key,
    quic::ParsedQuicVersion quic_version,
    int cert_verify_flags,
    bool require_confirmation,
    const HostResolverEndpointResult& endpoint_result,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    const NetLogWithSource& net_log,
    raw_ptr<QuicChromiumClientSession>* session,
    handles::NetworkHandle* network) {
  TRACE_EVENT0(NetTracingCategory(), "QuicSessionPool::CreateSession");
  // TODO(https://crbug.com/1416409): This logic only knows how to try one IP
  // endpoint.
  IPEndPoint addr = endpoint_result.ip_endpoints.front();
  std::unique_ptr<DatagramClientSocket> socket(
      CreateSocket(net_log.net_log(), net_log.source()));

  // If migrate_sessions_on_network_change_v2 is on, passing in
  // handles::kInvalidNetworkHandle will bind the socket to the default network.
  int rv = ConfigureSocket(socket.get(), addr, *network,
                           key.session_key().socket_tag());
  if (rv != OK) {
    return rv;
  }
  bool closed_during_initialize = CreateSessionHelper(
      key, quic_version, cert_verify_flags, require_confirmation,
      endpoint_result, dns_resolution_start_time, dns_resolution_end_time,
      net_log, session, network, std::move(socket));
  if (closed_during_initialize) {
    DLOG(DFATAL) << "Session closed during initialize";
    *session = nullptr;

    return ERR_CONNECTION_CLOSED;
  }

  return OK;
}

int QuicSessionPool::CreateSessionAsync(
    CompletionOnceCallback callback,
    const QuicSessionAliasKey& key,
    quic::ParsedQuicVersion quic_version,
    int cert_verify_flags,
    bool require_confirmation,
    const HostResolverEndpointResult& endpoint_result,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    const NetLogWithSource& net_log,
    raw_ptr<QuicChromiumClientSession>* session,
    handles::NetworkHandle* network) {
  TRACE_EVENT0(NetTracingCategory(), "QuicSessionPool::CreateSession");
  // TODO(https://crbug.com/1416409): This logic only knows how to try one IP
  // endpoint.
  IPEndPoint addr = endpoint_result.ip_endpoints.front();
  std::unique_ptr<DatagramClientSocket> socket(
      CreateSocket(net_log.net_log(), net_log.source()));
  DatagramClientSocket* socket_ptr = socket.get();
  CompletionOnceCallback connect_and_configure_callback = base::BindOnce(
      &QuicSessionPool::FinishCreateSession, weak_factory_.GetWeakPtr(),
      std::move(callback), key, quic_version, cert_verify_flags,
      require_confirmation, endpoint_result, dns_resolution_start_time,
      dns_resolution_end_time, net_log, session, network, std::move(socket));

  // If migrate_sessions_on_network_change_v2 is on, passing in
  // handles::kInvalidNetworkHandle will bind the socket to the default network.
  return ConnectAndConfigureSocket(std::move(connect_and_configure_callback),
                                   socket_ptr, addr, *network,
                                   key.session_key().socket_tag());
}

void QuicSessionPool::FinishCreateSession(
    CompletionOnceCallback callback,
    const QuicSessionAliasKey& key,
    quic::ParsedQuicVersion quic_version,
    int cert_verify_flags,
    bool require_confirmation,
    const HostResolverEndpointResult& endpoint_result,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    const NetLogWithSource& net_log,
    raw_ptr<QuicChromiumClientSession>* session,
    handles::NetworkHandle* network,
    std::unique_ptr<DatagramClientSocket> socket,
    int rv) {
  if (rv != OK) {
    std::move(callback).Run(rv);
    return;
  }
  bool closed_during_initialize = CreateSessionHelper(
      key, quic_version, cert_verify_flags, require_confirmation,
      endpoint_result, dns_resolution_start_time, dns_resolution_end_time,
      net_log, session, network, std::move(socket));
  if (closed_during_initialize) {
    DLOG(DFATAL) << "Session closed during initialize";
    *session = nullptr;

    std::move(callback).Run(ERR_CONNECTION_CLOSED);
    return;
  }

  std::move(callback).Run(OK);
}

bool QuicSessionPool::CreateSessionHelper(
    const QuicSessionAliasKey& key,
    quic::ParsedQuicVersion quic_version,
    int cert_verify_flags,
    bool require_confirmation,
    const HostResolverEndpointResult& endpoint_result,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    const NetLogWithSource& net_log,
    raw_ptr<QuicChromiumClientSession>* session,
    handles::NetworkHandle* network,
    std::unique_ptr<DatagramClientSocket> socket) {
  // TODO(https://crbug.com/1416409): This logic only knows how to try one IP
  // endpoint.
  IPEndPoint addr = endpoint_result.ip_endpoints.front();
  const quic::QuicServerId& server_id = key.server_id();

  if (params_.migrate_sessions_on_network_change_v2 &&
      *network == handles::kInvalidNetworkHandle) {
    *network = socket->GetBoundNetwork();
    if (default_network_ == handles::kInvalidNetworkHandle) {
      // QuicSessionPool may miss the default network signal before its
      // creation, update |default_network_| when the first socket is bound
      // to the default network.
      default_network_ = *network;
      connectivity_monitor_.SetInitialDefaultNetwork(default_network_);
    } else {
      UMA_HISTOGRAM_BOOLEAN("Net.QuicStreamFactory.DefaultNetworkMatch",
                            default_network_ == *network);
    }
  }

  if (!helper_.get()) {
    helper_ = std::make_unique<QuicChromiumConnectionHelper>(clock_,
                                                             random_generator_);
  }

  if (!alarm_factory_.get()) {
    alarm_factory_ = std::make_unique<QuicChromiumAlarmFactory>(
        base::SingleThreadTaskRunner::GetCurrentDefault().get(), clock_);
  }

  quic::QuicConnectionId connection_id =
      quic::QuicUtils::CreateRandomConnectionId(random_generator_);
  std::unique_ptr<QuicServerInfo> server_info;
  if (params_.max_server_configs_stored_in_properties > 0) {
    server_info = std::make_unique<PropertiesBasedQuicServerInfo>(
        server_id, key.session_key().network_anonymization_key(),
        http_server_properties_);
  }
  std::unique_ptr<CryptoClientConfigHandle> crypto_config_handle =
      CreateCryptoConfigHandle(key.session_key().network_anonymization_key());
  InitializeCachedStateInCryptoConfig(*crypto_config_handle, server_id,
                                      server_info);

  QuicChromiumPacketWriter* writer =
      new QuicChromiumPacketWriter(socket.get(), task_runner_.get());
  quic::QuicConnection* connection = new quic::QuicConnection(
      connection_id, quic::QuicSocketAddress(), ToQuicSocketAddress(addr),
      helper_.get(), alarm_factory_.get(), writer, true /* owns_writer */,
      quic::Perspective::IS_CLIENT, {quic_version}, connection_id_generator_);
  connection->set_keep_alive_ping_timeout(ping_timeout_);
  connection->SetMaxPacketLength(params_.max_packet_length);

  quic::QuicConfig config = config_;
  ConfigureInitialRttEstimate(
      server_id, key.session_key().network_anonymization_key(), &config);

  // Use the factory to create a new socket performance watcher, and pass the
  // ownership to QuicChromiumClientSession.
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher;
  if (socket_performance_watcher_factory_) {
    socket_performance_watcher =
        socket_performance_watcher_factory_->CreateSocketPerformanceWatcher(
            SocketPerformanceWatcherFactory::PROTOCOL_QUIC, addr.address());
  }

  // Wait for handshake confirmation before allowing streams to be created if
  // either this session or the pool require confirmation.
  if (!is_quic_known_to_work_on_current_network_) {
    require_confirmation = true;
  }

  *session = new QuicChromiumClientSession(
      connection, std::move(socket), this, quic_crypto_client_stream_factory_,
      clock_, transport_security_state_, ssl_config_service_,
      std::move(server_info), key.session_key(), require_confirmation,
      params_.migrate_sessions_early_v2,
      params_.migrate_sessions_on_network_change_v2, default_network_,
      retransmittable_on_wire_timeout_, params_.migrate_idle_sessions,
      params_.allow_port_migration, params_.idle_session_migration_period,
      params_.multi_port_probing_interval,
      params_.max_time_on_non_default_network,
      params_.max_migrations_to_non_default_network_on_write_error,
      params_.max_migrations_to_non_default_network_on_path_degrading,
      yield_after_packets_, yield_after_duration_, cert_verify_flags, config,
      std::move(crypto_config_handle),
      network_connection_.connection_description(), dns_resolution_start_time,
      dns_resolution_end_time, tick_clock_, task_runner_.get(),
      std::move(socket_performance_watcher), endpoint_result,
      net_log.net_log());

  all_sessions_[*session] = key;  // owning pointer
  writer->set_delegate(*session);
  (*session)->AddConnectivityObserver(&connectivity_monitor_);

  (*session)->Initialize();
  bool closed_during_initialize = !base::Contains(all_sessions_, *session) ||
                                  !(*session)->connection()->connected();
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ClosedDuringInitializeSession",
                        closed_during_initialize);
  return closed_during_initialize;
}

void QuicSessionPool::ActivateSession(const QuicSessionAliasKey& key,
                                      QuicChromiumClientSession* session,
                                      std::set<std::string> dns_aliases) {
  DCHECK(!HasActiveSession(key.session_key()));
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicActiveSessions", active_sessions_.size());
  active_sessions_[key.session_key()] = session;
  MapSessionToAliasKey(session, key, std::move(dns_aliases));
  const IPEndPoint peer_address =
      ToIPEndPoint(session->connection()->peer_address());
  DCHECK(!base::Contains(ip_aliases_[peer_address], session));
  ip_aliases_[peer_address].insert(session);
  DCHECK(!base::Contains(session_peer_ip_, session));
  session_peer_ip_[session] = peer_address;
}

void QuicSessionPool::MarkAllActiveSessionsGoingAway(
    AllActiveSessionsGoingAwayReason reason) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_POOL_MARK_ALL_ACTIVE_SESSIONS_GOING_AWAY);
  while (!active_sessions_.empty()) {
    QuicChromiumClientSession* session = active_sessions_.begin()->second;
    // If IP address change is detected, disable session's connectivity
    // monitoring by remove the Delegate.
    if (reason == kIPAddressChanged) {
      connectivity_monitor_.OnSessionGoingAwayOnIPAddressChange(session);
    }
    OnSessionGoingAway(session);
  }
}

void QuicSessionPool::ConfigureInitialRttEstimate(
    const quic::QuicServerId& server_id,
    const NetworkAnonymizationKey& network_anonymization_key,
    quic::QuicConfig* config) {
  const base::TimeDelta* srtt =
      GetServerNetworkStatsSmoothedRtt(server_id, network_anonymization_key);
  // Sometimes *srtt is negative. See https://crbug.com/1225616.
  // TODO(ricea): When the root cause of the negative value is fixed, change the
  // non-negative assertion to a DCHECK.
  if (srtt && srtt->is_positive()) {
    SetInitialRttEstimate(*srtt, INITIAL_RTT_CACHED, config);
    return;
  }

  NetworkChangeNotifier::ConnectionType type =
      network_connection_.connection_type();
  if (type == NetworkChangeNotifier::CONNECTION_2G) {
    SetInitialRttEstimate(base::Milliseconds(1200), INITIAL_RTT_CACHED, config);
    return;
  }

  if (type == NetworkChangeNotifier::CONNECTION_3G) {
    SetInitialRttEstimate(base::Milliseconds(400), INITIAL_RTT_CACHED, config);
    return;
  }

  if (params_.initial_rtt_for_handshake.is_positive()) {
    SetInitialRttEstimate(
        base::Microseconds(params_.initial_rtt_for_handshake.InMicroseconds()),
        INITIAL_RTT_DEFAULT, config);
    return;
  }

  SetInitialRttEstimate(base::TimeDelta(), INITIAL_RTT_DEFAULT, config);
}

int64_t QuicSessionPool::GetServerNetworkStatsSmoothedRttInMicroseconds(
    const quic::QuicServerId& server_id,
    const NetworkAnonymizationKey& network_anonymization_key) const {
  const base::TimeDelta* srtt =
      GetServerNetworkStatsSmoothedRtt(server_id, network_anonymization_key);
  return srtt == nullptr ? 0 : srtt->InMicroseconds();
}

const base::TimeDelta* QuicSessionPool::GetServerNetworkStatsSmoothedRtt(
    const quic::QuicServerId& server_id,
    const NetworkAnonymizationKey& network_anonymization_key) const {
  url::SchemeHostPort server("https", server_id.host(), server_id.port());
  const ServerNetworkStats* stats =
      http_server_properties_->GetServerNetworkStats(server,
                                                     network_anonymization_key);
  if (stats == nullptr) {
    return nullptr;
  }
  return &(stats->srtt);
}

bool QuicSessionPool::WasQuicRecentlyBroken(
    const QuicSessionKey& session_key) const {
  const AlternativeService alternative_service(
      kProtoQUIC, HostPortPair(session_key.server_id().host(),
                               session_key.server_id().port()));
  return http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, session_key.network_anonymization_key());
}

void QuicSessionPool::InitializeMigrationOptions() {
  // The following list of options cannot be set immediately until
  // prerequisites are met. Cache the initial setting in local variables and
  // reset them in |params_|.
  bool migrate_sessions_on_network_change =
      params_.migrate_sessions_on_network_change_v2;
  bool migrate_sessions_early = params_.migrate_sessions_early_v2;
  bool retry_on_alternate_network_before_handshake =
      params_.retry_on_alternate_network_before_handshake;
  bool migrate_idle_sessions = params_.migrate_idle_sessions;
  bool allow_port_migration = params_.allow_port_migration;
  params_.migrate_sessions_on_network_change_v2 = false;
  params_.migrate_sessions_early_v2 = false;
  params_.allow_port_migration = false;
  params_.retry_on_alternate_network_before_handshake = false;
  params_.migrate_idle_sessions = false;

  // TODO(zhongyi): deprecate |goaway_sessions_on_ip_change| if the experiment
  // is no longer needed.
  // goaway_sessions_on_ip_change and close_sessions_on_ip_change should never
  // be simultaneously set to true.
  DCHECK(!(params_.close_sessions_on_ip_change &&
           params_.goaway_sessions_on_ip_change));

  bool handle_ip_change = params_.close_sessions_on_ip_change ||
                          params_.goaway_sessions_on_ip_change;
  // If IP address changes are handled explicitly, connection migration should
  // not be set.
  DCHECK(!(handle_ip_change && migrate_sessions_on_network_change));

  if (handle_ip_change) {
    NetworkChangeNotifier::AddIPAddressObserver(this);
  }

  if (allow_port_migration) {
    params_.allow_port_migration = true;
    if (migrate_idle_sessions) {
      params_.migrate_idle_sessions = true;
    }
  }

  if (!NetworkChangeNotifier::AreNetworkHandlesSupported()) {
    return;
  }

  NetworkChangeNotifier::AddNetworkObserver(this);
  // Perform checks on the connection migration options.
  if (!migrate_sessions_on_network_change) {
    DCHECK(!migrate_sessions_early);
    return;
  }

  // Enable migration on platform notifications.
  params_.migrate_sessions_on_network_change_v2 = true;

  if (!migrate_sessions_early) {
    DCHECK(!retry_on_alternate_network_before_handshake);
    return;
  }

  // Enable migration on path degrading.
  params_.migrate_sessions_early_v2 = true;
  // Set retransmittable on wire timeout for migration on path degrading if no
  // value is specified.
  if (retransmittable_on_wire_timeout_.IsZero()) {
    retransmittable_on_wire_timeout_ = quic::QuicTime::Delta::FromMicroseconds(
        kDefaultRetransmittableOnWireTimeout.InMicroseconds());
  }

  // Enable retry on alternate network before handshake.
  if (retry_on_alternate_network_before_handshake) {
    params_.retry_on_alternate_network_before_handshake = true;
  }

  // Enable migration for idle sessions.
  if (migrate_idle_sessions) {
    params_.migrate_idle_sessions = true;
  }
}

void QuicSessionPool::InitializeCachedStateInCryptoConfig(
    const CryptoClientConfigHandle& crypto_config_handle,
    const quic::QuicServerId& server_id,
    const std::unique_ptr<QuicServerInfo>& server_info) {
  quic::QuicCryptoClientConfig::CachedState* cached =
      crypto_config_handle.GetConfig()->LookupOrCreate(server_id);

  if (!cached->IsEmpty()) {
    return;
  }

  if (!server_info || !server_info->Load()) {
    return;
  }

  cached->Initialize(server_info->state().server_config,
                     server_info->state().source_address_token,
                     server_info->state().certs, server_info->state().cert_sct,
                     server_info->state().chlo_hash,
                     server_info->state().server_config_sig, clock_->WallNow(),
                     quic::QuicWallTime::Zero());
}

void QuicSessionPool::ProcessGoingAwaySession(
    QuicChromiumClientSession* session,
    const quic::QuicServerId& server_id,
    bool session_was_active) {
  if (!http_server_properties_) {
    return;
  }

  const quic::QuicConnectionStats& stats = session->connection()->GetStats();
  const AlternativeService alternative_service(
      kProtoQUIC, HostPortPair(server_id.host(), server_id.port()));

  url::SchemeHostPort server("https", server_id.host(), server_id.port());
  // Do nothing if QUIC is currently marked as broken.
  if (http_server_properties_->IsAlternativeServiceBroken(
          alternative_service,
          session->quic_session_key().network_anonymization_key())) {
    return;
  }

  if (session->OneRttKeysAvailable()) {
    http_server_properties_->ConfirmAlternativeService(
        alternative_service,
        session->quic_session_key().network_anonymization_key());
    ServerNetworkStats network_stats;
    network_stats.srtt = base::Microseconds(stats.srtt_us);
    network_stats.bandwidth_estimate = stats.estimated_bandwidth;
    http_server_properties_->SetServerNetworkStats(
        server, session->quic_session_key().network_anonymization_key(),
        network_stats);
    return;
  }

  http_server_properties_->ClearServerNetworkStats(
      server, session->quic_session_key().network_anonymization_key());

  UMA_HISTOGRAM_COUNTS_1M("Net.QuicHandshakeNotConfirmedNumPacketsReceived",
                          stats.packets_received);

  if (!session_was_active) {
    return;
  }

  // TODO(rch):  In the special case where the session has received no packets
  // from the peer, we should consider blocking this differently so that we
  // still race TCP but we don't consider the session connected until the
  // handshake has been confirmed.
  HistogramBrokenAlternateProtocolLocation(
      BROKEN_ALTERNATE_PROTOCOL_LOCATION_QUIC_SESSION_POOL);

  // Since the session was active, there's no longer an HttpStreamFactory::Job
  // running which can mark it broken, unless the TCP job also fails. So to
  // avoid not using QUIC when we otherwise could, we mark it as recently
  // broken, which means that 0-RTT will be disabled but we'll still race.
  http_server_properties_->MarkAlternativeServiceRecentlyBroken(
      alternative_service,
      session->quic_session_key().network_anonymization_key());
}

void QuicSessionPool::MapSessionToAliasKey(QuicChromiumClientSession* session,
                                           const QuicSessionAliasKey& key,
                                           std::set<std::string> dns_aliases) {
  session_aliases_[session].insert(key);
  dns_aliases_by_session_key_[key.session_key()] = std::move(dns_aliases);
}

void QuicSessionPool::UnmapSessionFromSessionAliases(
    QuicChromiumClientSession* session) {
  for (const auto& key : session_aliases_[session]) {
    dns_aliases_by_session_key_.erase(key.session_key());
  }
  session_aliases_.erase(session);
}

std::unique_ptr<QuicSessionPool::CryptoClientConfigHandle>
QuicSessionPool::CreateCryptoConfigHandle(
    const NetworkAnonymizationKey& network_anonymization_key) {
  NetworkAnonymizationKey actual_network_anonymization_key =
      use_network_anonymization_key_for_crypto_configs_
          ? network_anonymization_key
          : NetworkAnonymizationKey();

  // If there's a matching entry in |active_crypto_config_map_|, create a
  // CryptoClientConfigHandle for it.
  auto map_iterator =
      active_crypto_config_map_.find(actual_network_anonymization_key);
  if (map_iterator != active_crypto_config_map_.end()) {
    DCHECK_GT(map_iterator->second->num_refs(), 0);

    // If there's an active matching crypto config, there shouldn't also be an
    // inactive matching crypto config.
    DCHECK(recent_crypto_config_map_.Peek(actual_network_anonymization_key) ==
           recent_crypto_config_map_.end());

    return std::make_unique<CryptoClientConfigHandle>(map_iterator);
  }

  // If there's a matching entry in |recent_crypto_config_map_|, move it to
  // |active_crypto_config_map_| and create a CryptoClientConfigHandle for it.
  auto mru_iterator =
      recent_crypto_config_map_.Peek(actual_network_anonymization_key);
  if (mru_iterator != recent_crypto_config_map_.end()) {
    DCHECK_EQ(mru_iterator->second->num_refs(), 0);

    map_iterator = active_crypto_config_map_
                       .emplace(actual_network_anonymization_key,
                                std::move(mru_iterator->second))
                       .first;
    recent_crypto_config_map_.Erase(mru_iterator);
    return std::make_unique<CryptoClientConfigHandle>(map_iterator);
  }

  // Otherwise, create a new QuicCryptoClientConfigOwner and add it to
  // |active_crypto_config_map_|.
  std::unique_ptr<QuicCryptoClientConfigOwner> crypto_config_owner =
      std::make_unique<QuicCryptoClientConfigOwner>(
          std::make_unique<ProofVerifierChromium>(
              cert_verifier_, transport_security_state_, sct_auditing_delegate_,
              HostsFromOrigins(params_.origins_to_force_quic_on),
              actual_network_anonymization_key),
          std::make_unique<quic::QuicClientSessionCache>(), this);

  quic::QuicCryptoClientConfig* crypto_config = crypto_config_owner->config();
  crypto_config->AddCanonicalSuffix(".c.youtube.com");
  crypto_config->AddCanonicalSuffix(".ggpht.com");
  crypto_config->AddCanonicalSuffix(".googlevideo.com");
  crypto_config->AddCanonicalSuffix(".googleusercontent.com");
  crypto_config->AddCanonicalSuffix(".gvt1.com");
  crypto_config->set_alps_use_new_codepoint(params_.use_new_alps_codepoint);

  ConfigureQuicCryptoClientConfig(*crypto_config);

  if (!prefer_aes_gcm_recorded_) {
    bool prefer_aes_gcm =
        !crypto_config->aead.empty() && (crypto_config->aead[0] == quic::kAESG);
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.PreferAesGcm", prefer_aes_gcm);
    prefer_aes_gcm_recorded_ = true;
  }

  map_iterator = active_crypto_config_map_
                     .emplace(actual_network_anonymization_key,
                              std::move(crypto_config_owner))
                     .first;
  return std::make_unique<CryptoClientConfigHandle>(map_iterator);
}

void QuicSessionPool::OnAllCryptoClientRefReleased(
    QuicCryptoClientConfigMap::iterator& map_iterator) {
  DCHECK_EQ(0, map_iterator->second->num_refs());
  recent_crypto_config_map_.Put(map_iterator->first,
                                std::move(map_iterator->second));
  active_crypto_config_map_.erase(map_iterator);
}

void QuicSessionPool::CollectDataOnPlatformNotification(
    enum QuicPlatformNotification notification,
    handles::NetworkHandle affected_network) const {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.PlatformNotification",
                            notification, NETWORK_NOTIFICATION_MAX);
  connectivity_monitor_.RecordConnectivityStatsToHistograms(
      QuicPlatformNotificationToString(notification), affected_network);
}

std::unique_ptr<QuicCryptoClientConfigHandle>
QuicSessionPool::GetCryptoConfigForTesting(
    const NetworkAnonymizationKey& network_anonymization_key) {
  return CreateCryptoConfigHandle(network_anonymization_key);
}

bool QuicSessionPool::CryptoConfigCacheIsEmptyForTesting(
    const quic::QuicServerId& server_id,
    const NetworkAnonymizationKey& network_anonymization_key) {
  quic::QuicCryptoClientConfig::CachedState* cached = nullptr;
  NetworkAnonymizationKey actual_network_anonymization_key =
      use_network_anonymization_key_for_crypto_configs_
          ? network_anonymization_key
          : NetworkAnonymizationKey();
  auto map_iterator =
      active_crypto_config_map_.find(actual_network_anonymization_key);
  if (map_iterator != active_crypto_config_map_.end()) {
    cached = map_iterator->second->config()->LookupOrCreate(server_id);
  } else {
    auto mru_iterator =
        recent_crypto_config_map_.Peek(actual_network_anonymization_key);
    if (mru_iterator != recent_crypto_config_map_.end()) {
      cached = mru_iterator->second->config()->LookupOrCreate(server_id);
    }
  }
  return !cached || cached->IsEmpty();
}

}  // namespace net
