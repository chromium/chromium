// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include <memory>
#include <set>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
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
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "crypto/openssl_util.h"
#include "net/base/address_list.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
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
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_server_info.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/udp_client_socket.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/null_decrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/proof_verifier.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_client_session_cache.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_clock.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"
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

enum class JobProtocolErrorLocation {
  kSessionStartReadingFailedAsync = 0,
  kSessionStartReadingFailedSync = 1,
  kCreateSessionFailedAsync = 2,
  kCreateSessionFailedSync = 3,
  kCryptoConnectFailedSync = 4,
  kCryptoConnectFailedAsync = 5,
  kMaxValue = kCryptoConnectFailedAsync,
};

base::Value::Dict NetLogQuicStreamFactoryJobParams(
    const QuicStreamFactory::QuicSessionAliasKey* key) {
  return base::Value::Dict()
      .Set("host", key->server_id().host())
      .Set("port", key->server_id().port())
      .Set("privacy_mode",
           PrivacyModeToDebugString(key->session_key().privacy_mode()))
      .Set("network_anonymization_key",
           key->session_key().network_anonymization_key().ToDebugString());
}

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

void HistogramProtocolErrorLocation(enum JobProtocolErrorLocation location) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicStreamFactory.DoConnectFailureLocation",
                            location);
}

void LogConnectionIpPooling(bool pooled) {
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ConnectionIpPooled", pooled);
}

void LogStaleConnectionTime(base::TimeTicks start_time) {
  UMA_HISTOGRAM_TIMES("Net.QuicSession.StaleConnectionTime",
                      base::TimeTicks::Now() - start_time);
}

void LogValidConnectionTime(base::TimeTicks start_time) {
  UMA_HISTOGRAM_TIMES("Net.QuicSession.ValidConnectionTime",
                      base::TimeTicks::Now() - start_time);
}

void SetInitialRttEstimate(base::TimeDelta estimate,
                           enum InitialRttEstimateSource source,
                           quic::QuicConfig* config) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.InitialRttEsitmateSource", source,
                            INITIAL_RTT_SOURCE_MAX);
  if (estimate != base::TimeDelta())
    config->SetInitialRoundTripTimeUsToSend(
        base::checked_cast<uint64_t>(estimate.InMicroseconds()));
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
    if (origin_filter_.is_null())
      return true;

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

// Refcounted class that owns quic::QuicCryptoClientConfig and tracks how many
// consumers are using it currently. When the last reference is freed, the
// QuicCryptoClientConfigHandle informs the owning QuicStreamFactory, moves it
// into an MRU cache.
class QuicStreamFactory::QuicCryptoClientConfigOwner {
 public:
  QuicCryptoClientConfigOwner(
      std::unique_ptr<quic::ProofVerifier> proof_verifier,
      std::unique_ptr<quic::QuicClientSessionCache> session_cache,
      QuicStreamFactory* quic_stream_factory)
      : config_(std::move(proof_verifier), std::move(session_cache)),
        clock_(base::DefaultClock::GetInstance()),
        quic_stream_factory_(quic_stream_factory) {
    DCHECK(quic_stream_factory_);
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE,
        base::BindRepeating(&QuicCryptoClientConfigOwner::OnMemoryPressure,
                            base::Unretained(this)));
    if (quic_stream_factory_->ssl_config_service_->GetSSLContextConfig()
            .PostQuantumKeyAgreementEnabled()) {
      config_.set_preferred_groups({SSL_GROUP_X25519_KYBER768_DRAFT00,
                                    SSL_GROUP_X25519, SSL_GROUP_SECP256R1,
                                    SSL_GROUP_SECP384R1});
    }
  }

  QuicCryptoClientConfigOwner(const QuicCryptoClientConfigOwner&) = delete;
  QuicCryptoClientConfigOwner& operator=(const QuicCryptoClientConfigOwner&) =
      delete;

  ~QuicCryptoClientConfigOwner() { DCHECK_EQ(num_refs_, 0); }

  quic::QuicCryptoClientConfig* config() { return &config_; }

  int num_refs() const { return num_refs_; }

  QuicStreamFactory* quic_stream_factory() { return quic_stream_factory_; }

  void OnMemoryPressure(
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

 private:
  friend class CryptoClientConfigHandle;

  // Simple ref counting. Not using scoped_refptr allows for both keeping around
  // an MRU cache of 0-reference objects, and DCHECKing that there are no
  // outstanding referenced QuicCryptoClientConfigOwner on destruction. Private
  // so that only CryptoClientConfigHandle can add and remove refs.

  void AddRef() { num_refs_++; }

  void ReleaseRef() {
    DCHECK_GT(num_refs_, 0);
    num_refs_--;
  }

  int num_refs_ = 0;
  quic::QuicCryptoClientConfig config_;
  raw_ptr<base::Clock> clock_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  const raw_ptr<QuicStreamFactory> quic_stream_factory_;
};

// Class that owns a reference to a QuicCryptoClientConfigOwner. Handles
// incrementing the refcount on construction, and decrementing it on
// destruction.
class QuicStreamFactory::CryptoClientConfigHandle
    : public QuicCryptoClientConfigHandle {
 public:
  explicit CryptoClientConfigHandle(
      const QuicCryptoClientConfigMap::iterator& map_iterator)
      : map_iterator_(map_iterator) {
    DCHECK_GE(map_iterator_->second->num_refs(), 0);
    map_iterator->second->AddRef();
  }

  CryptoClientConfigHandle(const CryptoClientConfigHandle& other)
      : CryptoClientConfigHandle(other.map_iterator_) {}

  CryptoClientConfigHandle& operator=(const CryptoClientConfigHandle&) = delete;

  ~CryptoClientConfigHandle() override {
    DCHECK_GT(map_iterator_->second->num_refs(), 0);
    map_iterator_->second->ReleaseRef();
    if (map_iterator_->second->num_refs() == 0) {
      map_iterator_->second->quic_stream_factory()
          ->OnAllCryptoClientRefReleased(map_iterator_);
    }
  }

  quic::QuicCryptoClientConfig* GetConfig() const override {
    return map_iterator_->second->config();
  }

 private:
  QuicCryptoClientConfigMap::iterator map_iterator_;
};

// Responsible for creating a new QUIC session to the specified server, and
// for notifying any associated requests when complete. |client_config_handle|
// is not actually used, but serves to keep the corresponding CryptoClientConfig
// alive until the Job completes.
class QuicStreamFactory::Job {
 public:
  Job(QuicStreamFactory* factory,
      quic::ParsedQuicVersion quic_version,
      HostResolver* host_resolver,
      const QuicSessionAliasKey& key,
      std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
      bool was_alternative_service_recently_broken,
      bool retry_on_alternate_network_before_handshake,
      RequestPriority priority,
      bool use_dns_aliases,
      bool require_dns_https_alpn,
      int cert_verify_flags,
      const NetLogWithSource& net_log);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job();

  int Run(CompletionOnceCallback callback);

  int DoLoop(int rv);
  int DoResolveHost();
  int DoResolveHostComplete(int rv);
  int DoCreateSession();
  int DoCreateSessionComplete(int rv);
  int DoConnect(int rv);
  int DoConfirmConnection(int rv);

  void OnCreateSessionComplete(int rv);
  void OnResolveHostComplete(int rv);
  void OnCryptoConnectComplete(int rv);

  const QuicSessionAliasKey& key() const { return key_; }

  const NetLogWithSource& net_log() const { return net_log_; }

  base::WeakPtr<Job> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  void PopulateNetErrorDetails(NetErrorDetails* details) const;

  void AddRequest(QuicStreamRequest* request) {
    stream_requests_.insert(request);
    if (!host_resolution_finished_) {
      request->ExpectOnHostResolution();
    }
    // Callers do not need to wait for OnQuicSessionCreationComplete if the
    // kAsyncQuicSession flag is not set because session creation will be fully
    // synchronous, so no need to call ExpectQuicSessionCreation.
    if (base::FeatureList::IsEnabled(net::features::kAsyncQuicSession) &&
        !session_creation_finished_) {
      request->ExpectQuicSessionCreation();
    }
  }

  void RemoveRequest(QuicStreamRequest* request) {
    auto request_iter = stream_requests_.find(request);
    DCHECK(request_iter != stream_requests_.end());
    stream_requests_.erase(request_iter);
  }

  void SetPriority(RequestPriority priority) {
    if (priority_ == priority)
      return;

    priority_ = priority;
    if (resolve_host_request_ && !host_resolution_finished_) {
      resolve_host_request_->ChangeRequestPriority(priority);
    }
  }

  const std::set<QuicStreamRequest*>& stream_requests() {
    return stream_requests_;
  }

  RequestPriority priority() const { return priority_; }

 private:
  enum IoState {
    STATE_NONE,
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_CREATE_SESSION,
    STATE_CREATE_SESSION_COMPLETE,
    STATE_CONNECT,
    STATE_CONFIRM_CONNECTION,
  };

  void CloseStaleHostConnection() {
    DVLOG(1) << "Closing connection from stale host.";
    if (session_) {
      QuicChromiumClientSession* session = session_;
      session_ = nullptr;
      // Use ERR_FAILED instead of ERR_ABORTED out of paranoia - ERR_ABORTED
      // should only be used when the next layer up cancels a request, and has
      // special semantic meaning for some consumers when they see it.
      session->CloseSessionOnErrorLater(
          ERR_FAILED, quic::QUIC_STALE_CONNECTION_CANCELLED,
          quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    }
  }

  // Returns whether the client should be SVCB-optional when connecting to
  // `results`.
  bool IsSvcbOptional(
      base::span<const HostResolverEndpointResult> results) const {
    // If SVCB/HTTPS resolution succeeded, the client supports ECH, and all
    // routes support ECH, disable the A/AAAA fallback. See Section 10.1 of
    // draft-ietf-dnsop-svcb-https-11.
    if (!factory_->ssl_config_service_->GetSSLContextConfig()
             .EncryptedClientHelloEnabled() ||
        !base::FeatureList::IsEnabled(features::kEncryptedClientHelloQuic)) {
      return true;  // ECH is not supported for this request.
    }

    return !HostResolver::AllProtocolEndpointsHaveEch(results);
  }

  // Returns the QUIC version that would be used with `endpoint_result`, or
  // `quic::ParsedQuicVersion::Unsupported()` if `endpoint_result` cannot be
  // used with QUIC.
  quic::ParsedQuicVersion SelectQuicVersion(
      const HostResolverEndpointResult& endpoint_result,
      bool svcb_optional) const {
    // TODO(davidben): `require_dns_https_alpn_` only exists to be `DCHECK`ed
    // for consistency against `quic_version_`. Remove the parameter?
    DCHECK_EQ(require_dns_https_alpn_, !quic_version_.IsKnown());

    if (endpoint_result.metadata.supported_protocol_alpns.empty()) {
      // `endpoint_result` came from A/AAAA records directly, without HTTPS/SVCB
      // records. If we know the QUIC ALPN to use externally, i.e. via Alt-Svc,
      // use it in SVCB-optional mode. Otherwise, `endpoint_result` is not
      // eligible for QUIC.
      return svcb_optional ? quic_version_
                           : quic::ParsedQuicVersion::Unsupported();
    }

    // Otherwise, `endpoint_result` came from an HTTPS/SVCB record. We can use
    // QUIC if a suitable match is found in the record's ALPN list.
    // Additionally, if this connection attempt came from Alt-Svc, the DNS
    // result must be consistent with it. See
    // https://www.ietf.org/archive/id/draft-ietf-dnsop-svcb-https-11.html#name-interaction-with-alt-svc
    if (quic_version_.IsKnown()) {
      std::string expected_alpn = quic::AlpnForVersion(quic_version_);
      if (base::Contains(endpoint_result.metadata.supported_protocol_alpns,
                         quic::AlpnForVersion(quic_version_))) {
        return quic_version_;
      }
      return quic::ParsedQuicVersion::Unsupported();
    }

    for (const auto& alpn : endpoint_result.metadata.supported_protocol_alpns) {
      for (const auto& supported_version : factory_->supported_versions()) {
        if (alpn == AlpnForVersion(supported_version)) {
          return supported_version;
        }
      }
    }

    return quic::ParsedQuicVersion::Unsupported();
  }

  void LogStaleAndFreshHostMatched(bool matched) {
    if (matched) {
      net_log_.AddEvent(
          NetLogEventType::
              QUIC_STREAM_FACTORY_JOB_STALE_HOST_RESOLUTION_MATCHED);
    } else {
      net_log_.AddEvent(
          NetLogEventType::
              QUIC_STREAM_FACTORY_JOB_STALE_HOST_RESOLUTION_NO_MATCH);
    }
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.StaleAndFreshHostMatched", matched);
  }

  IoState io_state_ = STATE_RESOLVE_HOST;
  raw_ptr<QuicStreamFactory> factory_;
  quic::ParsedQuicVersion quic_version_;
  quic::ParsedQuicVersion quic_version_used_ =
      quic::ParsedQuicVersion::Unsupported();
  raw_ptr<HostResolver> host_resolver_;
  const QuicSessionAliasKey key_;
  const std::unique_ptr<CryptoClientConfigHandle> client_config_handle_;
  RequestPriority priority_;
  const bool use_dns_aliases_;
  const bool require_dns_https_alpn_;
  const int cert_verify_flags_;
  const bool was_alternative_service_recently_broken_;
  const bool retry_on_alternate_network_before_handshake_;
  const NetLogWithSource net_log_;
  bool host_resolution_finished_ = false;
  bool session_creation_finished_ = false;
  bool connection_retried_ = false;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION QuicChromiumClientSession* session_ = nullptr;
  HostResolverEndpointResult endpoint_result_;
  // If connection migraiton is supported, |network_| denotes the network on
  // which |session_| is created.
  handles::NetworkHandle network_;
  CompletionOnceCallback host_resolution_callback_;
  CompletionOnceCallback callback_;
  std::unique_ptr<HostResolver::ResolveHostRequest> resolve_host_request_;
  base::TimeTicks dns_resolution_start_time_;
  base::TimeTicks dns_resolution_end_time_;
  base::TimeTicks quic_connection_start_time_;
  std::set<QuicStreamRequest*> stream_requests_;
  base::WeakPtrFactory<Job> weak_factory_{this};
};

QuicStreamFactory::Job::Job(
    QuicStreamFactory* factory,
    quic::ParsedQuicVersion quic_version,
    HostResolver* host_resolver,
    const QuicSessionAliasKey& key,
    std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
    bool was_alternative_service_recently_broken,
    bool retry_on_alternate_network_before_handshake,
    RequestPriority priority,
    bool use_dns_aliases,
    bool require_dns_https_alpn,
    int cert_verify_flags,
    const NetLogWithSource& net_log)
    : factory_(factory),
      quic_version_(quic_version),
      host_resolver_(host_resolver),
      key_(key),
      client_config_handle_(std::move(client_config_handle)),
      priority_(priority),
      use_dns_aliases_(use_dns_aliases),
      require_dns_https_alpn_(require_dns_https_alpn),
      cert_verify_flags_(cert_verify_flags),
      was_alternative_service_recently_broken_(
          was_alternative_service_recently_broken),
      retry_on_alternate_network_before_handshake_(
          retry_on_alternate_network_before_handshake),
      net_log_(
          NetLogWithSource::Make(net_log.net_log(),
                                 NetLogSourceType::QUIC_STREAM_FACTORY_JOB)),
      network_(handles::kInvalidNetworkHandle) {
  DCHECK_EQ(quic_version.IsKnown(), !require_dns_https_alpn);
  net_log_.BeginEvent(NetLogEventType::QUIC_STREAM_FACTORY_JOB,
                      [&] { return NetLogQuicStreamFactoryJobParams(&key_); });
  // Associate |net_log_| with |net_log|.
  net_log_.AddEventReferencingSource(
      NetLogEventType::QUIC_STREAM_FACTORY_JOB_BOUND_TO_HTTP_STREAM_JOB,
      net_log.source());
  net_log.AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_JOB_BOUND_TO_QUIC_STREAM_FACTORY_JOB,
      net_log_.source());
}

QuicStreamFactory::Job::~Job() {
  net_log_.EndEvent(NetLogEventType::QUIC_STREAM_FACTORY_JOB);
  // If |this| is destroyed in QuicStreamFactory's destructor, |callback_| is
  // non-null.
}

int QuicStreamFactory::Job::Run(CompletionOnceCallback callback) {
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);

  return rv > 0 ? OK : rv;
}

int QuicStreamFactory::Job::DoLoop(int rv) {
  TRACE_EVENT0(NetTracingCategory(), "QuicStreamFactory::Job::DoLoop");

  do {
    IoState state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_HOST:
        CHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      case STATE_CREATE_SESSION:
        rv = DoCreateSession();
        break;
      case STATE_CREATE_SESSION_COMPLETE:
        rv = DoCreateSessionComplete(rv);
        break;
      case STATE_CONNECT:
        rv = DoConnect(rv);
        break;
      case STATE_CONFIRM_CONNECTION:
        rv = DoConfirmConnection(rv);
        break;
      default:
        NOTREACHED() << "io_state_: " << io_state_;
        break;
    }
  } while (io_state_ != STATE_NONE && rv != ERR_IO_PENDING);
  return rv;
}

void QuicStreamFactory::Job::OnResolveHostComplete(int rv) {
  DCHECK(!host_resolution_finished_);
  io_state_ = STATE_RESOLVE_HOST_COMPLETE;
  rv = DoLoop(rv);

  for (auto* request : stream_requests_) {
    request->OnHostResolutionComplete(rv);
  }

  if (rv != ERR_IO_PENDING && !callback_.is_null())
    std::move(callback_).Run(rv);
}

void QuicStreamFactory::Job::OnCryptoConnectComplete(int rv) {
  // This early return will be triggered when CloseSessionOnError is called
  // before crypto handshake has completed.
  if (!session_) {
    LogStaleConnectionTime(quic_connection_start_time_);
    return;
  }

  if (rv == ERR_QUIC_PROTOCOL_ERROR) {
    HistogramProtocolErrorLocation(
        JobProtocolErrorLocation::kCryptoConnectFailedAsync);
  }

  io_state_ = STATE_CONFIRM_CONNECTION;
  rv = DoLoop(rv);
  if (rv != ERR_IO_PENDING && !callback_.is_null())
    std::move(callback_).Run(rv);
}

void QuicStreamFactory::Job::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (!session_)
    return;
  details->connection_info = QuicHttpStream::ConnectionInfoFromQuicVersion(
      session_->connection()->version());
  details->quic_connection_error = session_->error();
}

int QuicStreamFactory::Job::DoResolveHost() {
  dns_resolution_start_time_ = base::TimeTicks::Now();

  io_state_ = STATE_RESOLVE_HOST_COMPLETE;

  HostResolver::ResolveHostParameters parameters;
  parameters.initial_priority = priority_;
  parameters.secure_dns_policy = key_.session_key().secure_dns_policy();
  resolve_host_request_ = host_resolver_->CreateRequest(
      key_.destination(), key_.session_key().network_anonymization_key(),
      net_log_, parameters);
  // Unretained is safe because |this| owns the request, ensuring cancellation
  // on destruction.
  return resolve_host_request_->Start(base::BindOnce(
      &QuicStreamFactory::Job::OnResolveHostComplete, base::Unretained(this)));
}

int QuicStreamFactory::Job::DoResolveHostComplete(int rv) {
  host_resolution_finished_ = true;
  dns_resolution_end_time_ = base::TimeTicks::Now();
  if (rv != OK)
    return rv;

  DCHECK(!factory_->HasActiveSession(key_.session_key()));

  // Inform the factory of this resolution, which will set up
  // a session alias, if possible.
  const bool svcb_optional =
      IsSvcbOptional(*resolve_host_request_->GetEndpointResults());
  for (const auto& endpoint : *resolve_host_request_->GetEndpointResults()) {
    // Only consider endpoints that would have been eligible for QUIC.
    if (!SelectQuicVersion(endpoint, svcb_optional).IsKnown()) {
      continue;
    }
    if (factory_->HasMatchingIpSession(
            key_, endpoint.ip_endpoints,
            *resolve_host_request_->GetDnsAliasResults(), use_dns_aliases_)) {
      LogConnectionIpPooling(true);
      return OK;
    }
  }
  io_state_ = STATE_CREATE_SESSION;
  return OK;
}

void QuicStreamFactory::Job::OnCreateSessionComplete(int rv) {
  if (rv != OK) {
    DCHECK(!session_);
    if (rv == ERR_QUIC_PROTOCOL_ERROR) {
      HistogramProtocolErrorLocation(
          JobProtocolErrorLocation::kCreateSessionFailedAsync);
    }
    for (auto* request : stream_requests_) {
      request->OnQuicSessionCreationComplete(rv);
    }
    if (!callback_.is_null()) {
      std::move(callback_).Run(rv);
    }
    return;
  }
  DCHECK(session_);
  DVLOG(1) << "Created session on network: " << network_;
  io_state_ = STATE_CREATE_SESSION_COMPLETE;
  rv = DoLoop(rv);

  for (auto* request : stream_requests_) {
    request->OnQuicSessionCreationComplete(rv);
  }

  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

int QuicStreamFactory::Job::DoCreateSession() {
  // TODO(https://crbug.com/1416409): This logic only knows how to try one
  // endpoint result.
  bool svcb_optional =
      IsSvcbOptional(*resolve_host_request_->GetEndpointResults());
  bool found = false;
  for (const auto& candidate : *resolve_host_request_->GetEndpointResults()) {
    quic::ParsedQuicVersion version =
        SelectQuicVersion(candidate, svcb_optional);
    if (version.IsKnown()) {
      found = true;
      quic_version_used_ = version;
      endpoint_result_ = candidate;
      break;
    }
  }
  if (!found) {
    return ERR_DNS_NO_MATCHING_SUPPORTED_ALPN;
  }

  quic_connection_start_time_ = base::TimeTicks::Now();
  DCHECK(dns_resolution_end_time_ != base::TimeTicks());
  io_state_ = STATE_CREATE_SESSION_COMPLETE;
  bool require_confirmation = was_alternative_service_recently_broken_;
  net_log_.AddEntryWithBoolParams(
      NetLogEventType::QUIC_STREAM_FACTORY_JOB_CONNECT, NetLogEventPhase::BEGIN,
      "require_confirmation", require_confirmation);

  DCHECK_NE(quic_version_used_, quic::ParsedQuicVersion::Unsupported());
  if (base::FeatureList::IsEnabled(net::features::kAsyncQuicSession)) {
    return factory_->CreateSessionAsync(
        base::BindOnce(&QuicStreamFactory::Job::OnCreateSessionComplete,
                       GetWeakPtr()),
        key_, quic_version_used_, cert_verify_flags_, require_confirmation,
        endpoint_result_, dns_resolution_start_time_, dns_resolution_end_time_,
        net_log_, &session_, &network_);
  }
  int rv = factory_->CreateSessionSync(
      key_, quic_version_used_, cert_verify_flags_, require_confirmation,
      endpoint_result_, dns_resolution_start_time_, dns_resolution_end_time_,
      net_log_, &session_, &network_);

  DVLOG(1) << "Created session on network: " << network_;

  if (rv == ERR_QUIC_PROTOCOL_ERROR) {
    DCHECK(!session_);
    HistogramProtocolErrorLocation(
        JobProtocolErrorLocation::kCreateSessionFailedSync);
  }

  return rv;
}
int QuicStreamFactory::Job::DoCreateSessionComplete(int rv) {
  session_creation_finished_ = true;
  if (rv != OK) {
    return rv;
  }
  io_state_ = STATE_CONNECT;
  if (!session_->connection()->connected()) {
    return ERR_CONNECTION_CLOSED;
  }

  session_->StartReading();
  if (!session_->connection()->connected()) {
    if (base::FeatureList::IsEnabled(net::features::kAsyncQuicSession)) {
      HistogramProtocolErrorLocation(
          JobProtocolErrorLocation::kSessionStartReadingFailedAsync);
    } else {
      HistogramProtocolErrorLocation(
          JobProtocolErrorLocation::kSessionStartReadingFailedSync);
    }
    return ERR_QUIC_PROTOCOL_ERROR;
  }
  return OK;
}

int QuicStreamFactory::Job::DoConnect(int rv) {
  if (rv != OK) {
    return rv;
  }
  DCHECK(session_);
  io_state_ = STATE_CONFIRM_CONNECTION;
  rv = session_->CryptoConnect(base::BindOnce(
      &QuicStreamFactory::Job::OnCryptoConnectComplete, GetWeakPtr()));

  if (rv != ERR_IO_PENDING) {
    LogValidConnectionTime(quic_connection_start_time_);
  }

  if (!session_->connection()->connected() &&
      session_->error() == quic::QUIC_PROOF_INVALID) {
    return ERR_QUIC_HANDSHAKE_FAILED;
  }

  if (rv == ERR_QUIC_PROTOCOL_ERROR) {
    HistogramProtocolErrorLocation(
        JobProtocolErrorLocation::kCryptoConnectFailedSync);
  }

  return rv;
}

int QuicStreamFactory::Job::DoConfirmConnection(int rv) {
  UMA_HISTOGRAM_TIMES("Net.QuicSession.TimeFromResolveHostToConfirmConnection",
                      base::TimeTicks::Now() - dns_resolution_start_time_);
  net_log_.EndEvent(NetLogEventType::QUIC_STREAM_FACTORY_JOB_CONNECT);

  if (was_alternative_service_recently_broken_)
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ConnectAfterBroken", rv == OK);

  if (retry_on_alternate_network_before_handshake_ && session_ &&
      !session_->OneRttKeysAvailable() &&
      network_ == factory_->default_network()) {
    if (session_->error() == quic::QUIC_NETWORK_IDLE_TIMEOUT ||
        session_->error() == quic::QUIC_HANDSHAKE_TIMEOUT ||
        session_->error() == quic::QUIC_PACKET_WRITE_ERROR) {
      // Retry the connection on an alternate network if crypto handshake failed
      // with network idle time out or handshake time out.
      DCHECK(network_ != handles::kInvalidNetworkHandle);
      network_ = factory_->FindAlternateNetwork(network_);
      connection_retried_ = network_ != handles::kInvalidNetworkHandle;
      UMA_HISTOGRAM_BOOLEAN(
          "Net.QuicStreamFactory.AttemptMigrationBeforeHandshake",
          connection_retried_);
      UMA_HISTOGRAM_ENUMERATION(
          "Net.QuicStreamFactory.AttemptMigrationBeforeHandshake."
          "FailedConnectionType",
          NetworkChangeNotifier::GetNetworkConnectionType(
              factory_->default_network()),
          NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
      if (connection_retried_) {
        UMA_HISTOGRAM_ENUMERATION(
            "Net.QuicStreamFactory.MigrationBeforeHandshake.NewConnectionType",
            NetworkChangeNotifier::GetNetworkConnectionType(network_),
            NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
        net_log_.AddEvent(
            NetLogEventType::
                QUIC_STREAM_FACTORY_JOB_RETRY_ON_ALTERNATE_NETWORK);
        // Notify requests that connection on the default network failed.
        for (auto* request : stream_requests_) {
          request->OnConnectionFailedOnDefaultNetwork();
        }
        DVLOG(1) << "Retry connection on alternate network: " << network_;
        session_ = nullptr;
        io_state_ = STATE_CREATE_SESSION;
        return OK;
      }
    }
  }

  if (connection_retried_) {
    UMA_HISTOGRAM_BOOLEAN("Net.QuicStreamFactory.MigrationBeforeHandshake2",
                          rv == OK);
    if (rv == OK) {
      UMA_HISTOGRAM_BOOLEAN(
          "Net.QuicStreamFactory.NetworkChangeDuringMigrationBeforeHandshake",
          network_ == factory_->default_network());
    } else {
      base::UmaHistogramSparse(
          "Net.QuicStreamFactory.MigrationBeforeHandshakeFailedReason", -rv);
    }
  } else if (network_ != handles::kInvalidNetworkHandle &&
             network_ != factory_->default_network()) {
    UMA_HISTOGRAM_BOOLEAN("Net.QuicStreamFactory.ConnectionOnNonDefaultNetwork",
                          rv == OK);
  }

  if (rv != OK)
    return rv;

  DCHECK(!factory_->HasActiveSession(key_.session_key()));
  // There may well now be an active session for this IP.  If so, use the
  // existing session instead.
  if (factory_->HasMatchingIpSession(
          key_, {ToIPEndPoint(session_->connection()->peer_address())},
          /*aliases=*/{}, use_dns_aliases_)) {
    LogConnectionIpPooling(true);
    session_->connection()->CloseConnection(
        quic::QUIC_CONNECTION_IP_POOLED,
        "An active session exists for the given IP.",
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    session_ = nullptr;
    return OK;
  }
  LogConnectionIpPooling(false);

  std::set<std::string> dns_aliases =
      use_dns_aliases_ && resolve_host_request_->GetDnsAliasResults()
          ? *resolve_host_request_->GetDnsAliasResults()
          : std::set<std::string>();
  factory_->ActivateSession(key_, session_, std::move(dns_aliases));

  return OK;
}

QuicStreamRequest::QuicStreamRequest(QuicStreamFactory* factory)
    : factory_(factory) {}

QuicStreamRequest::~QuicStreamRequest() {
  if (factory_ && !callback_.is_null())
    factory_->CancelRequest(this);
}

int QuicStreamRequest::Request(
    url::SchemeHostPort destination,
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
    CompletionOnceCallback callback) {
  DCHECK_EQ(quic_version.IsKnown(), !require_dns_https_alpn);
  DCHECK(net_error_details);
  DCHECK(callback_.is_null());
  DCHECK(host_resolution_callback_.is_null());
  DCHECK(factory_);

  net_error_details_ = net_error_details;
  failed_on_default_network_callback_ =
      std::move(failed_on_default_network_callback);

  session_key_ = QuicSessionKey(HostPortPair::FromURL(url), privacy_mode,
                                socket_tag, network_anonymization_key,
                                secure_dns_policy, require_dns_https_alpn);

  int rv = factory_->Create(session_key_, std::move(destination), quic_version,
                            priority, use_dns_aliases, cert_verify_flags, url,
                            net_log, this);
  if (rv == ERR_IO_PENDING) {
    net_log_ = net_log;
    callback_ = std::move(callback);
  } else {
    DCHECK(!expect_on_host_resolution_);
    factory_ = nullptr;
  }

  if (rv == OK)
    DCHECK(session_);
  return rv;
}

bool QuicStreamRequest::WaitForHostResolution(CompletionOnceCallback callback) {
  DCHECK(host_resolution_callback_.is_null());
  if (expect_on_host_resolution_) {
    host_resolution_callback_ = std::move(callback);
  }
  return expect_on_host_resolution_;
}

void QuicStreamRequest::ExpectOnHostResolution() {
  expect_on_host_resolution_ = true;
}

void QuicStreamRequest::OnHostResolutionComplete(int rv) {
  DCHECK(expect_on_host_resolution_);
  expect_on_host_resolution_ = false;
  if (!host_resolution_callback_.is_null()) {
    std::move(host_resolution_callback_).Run(rv);
  }
}

bool QuicStreamRequest::WaitForQuicSessionCreation(
    CompletionOnceCallback callback) {
  DCHECK(create_session_callback_.is_null());
  if (expect_on_quic_session_creation_) {
    create_session_callback_ = std::move(callback);
  }
  return expect_on_quic_session_creation_;
}

void QuicStreamRequest::ExpectQuicSessionCreation() {
  expect_on_quic_session_creation_ = true;
}

void QuicStreamRequest::OnQuicSessionCreationComplete(int rv) {
  // DCHECK(expect_on_quic_session_creation_);
  expect_on_quic_session_creation_ = false;
  if (!create_session_callback_.is_null()) {
    std::move(create_session_callback_).Run(rv);
  }
}

void QuicStreamRequest::OnRequestComplete(int rv) {
  factory_ = nullptr;
  std::move(callback_).Run(rv);
}

void QuicStreamRequest::OnConnectionFailedOnDefaultNetwork() {
  if (!failed_on_default_network_callback_.is_null())
    std::move(failed_on_default_network_callback_).Run(OK);
}

base::TimeDelta QuicStreamRequest::GetTimeDelayForWaitingJob() const {
  if (!factory_)
    return base::TimeDelta();
  return factory_->GetTimeDelayForWaitingJob(session_key_);
}

void QuicStreamRequest::SetPriority(RequestPriority priority) {
  if (factory_)
    factory_->SetRequestPriority(this, priority);
}

std::unique_ptr<QuicChromiumClientSession::Handle>
QuicStreamRequest::ReleaseSessionHandle() {
  if (!session_ || !session_->IsConnected())
    return nullptr;

  return std::move(session_);
}

void QuicStreamRequest::SetSession(
    std::unique_ptr<QuicChromiumClientSession::Handle> session) {
  session_ = std::move(session);
}

bool QuicStreamRequest::CanUseExistingSession(
    const GURL& url,
    PrivacyMode privacy_mode,
    const SocketTag& socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool require_dns_https_alpn,
    const url::SchemeHostPort& destination) const {
  return factory_->CanUseExistingSession(
      QuicSessionKey(HostPortPair::FromURL(url), privacy_mode, socket_tag,
                     network_anonymization_key, secure_dns_policy,
                     require_dns_https_alpn),
      destination);
}

QuicStreamFactory::QuicSessionAliasKey::QuicSessionAliasKey(
    url::SchemeHostPort destination,
    QuicSessionKey session_key)
    : destination_(std::move(destination)),
      session_key_(std::move(session_key)) {}

bool QuicStreamFactory::QuicSessionAliasKey::operator<(
    const QuicSessionAliasKey& other) const {
  return std::tie(destination_, session_key_) <
         std::tie(other.destination_, other.session_key_);
}

bool QuicStreamFactory::QuicSessionAliasKey::operator==(
    const QuicSessionAliasKey& other) const {
  return destination_ == other.destination_ &&
         session_key_ == other.session_key_;
}

QuicStreamFactory::QuicStreamFactory(
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
    QuicContext* quic_context)
    : net_log_(NetLogWithSource::Make(net_log,
                                      NetLogSourceType::QUIC_STREAM_FACTORY)),
      host_resolver_(host_resolver),
      client_socket_factory_(client_socket_factory),
      http_server_properties_(http_server_properties),
      cert_verifier_(cert_verifier),
      ct_policy_enforcer_(ct_policy_enforcer),
      transport_security_state_(transport_security_state),
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
  if (params_.disable_tls_zero_rtt)
    SetQuicFlag(quic_disable_client_tls_zero_rtt, true);
  InitializeMigrationOptions();
  cert_verifier_->AddObserver(this);
  CertDatabase::GetInstance()->AddObserver(this);
}

QuicStreamFactory::~QuicStreamFactory() {
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

bool QuicStreamFactory::CanUseExistingSession(
    const QuicSessionKey& session_key,
    const url::SchemeHostPort& destination) const {
  if (base::Contains(active_sessions_, session_key))
    return true;

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

int QuicStreamFactory::Create(const QuicSessionKey& session_key,
                              url::SchemeHostPort destination,
                              quic::ParsedQuicVersion quic_version,
                              RequestPriority priority,
                              bool use_dns_aliases,
                              int cert_verify_flags,
                              const GURL& url,
                              const NetLogWithSource& net_log,
                              QuicStreamRequest* request) {
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
        NetLogEventType::QUIC_STREAM_FACTORY_JOB_BOUND_TO_HTTP_STREAM_JOB,
        net_log.source());
    net_log.AddEventReferencingSource(
        NetLogEventType::HTTP_STREAM_JOB_BOUND_TO_QUIC_STREAM_FACTORY_JOB,
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
  if (!task_runner_)
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault().get();

  if (!tick_clock_)
    tick_clock_ = base::DefaultTickClock::GetInstance();

  QuicSessionAliasKey key(destination, session_key);
  std::unique_ptr<Job> job = std::make_unique<Job>(
      this, quic_version, host_resolver_, key,
      CreateCryptoConfigHandle(session_key.network_anonymization_key()),
      WasQuicRecentlyBroken(session_key),
      params_.retry_on_alternate_network_before_handshake, priority,
      use_dns_aliases, session_key.require_dns_https_alpn(), cert_verify_flags,
      net_log);
  int rv = job->Run(base::BindOnce(&QuicStreamFactory::OnJobComplete,
                                   weak_factory_.GetWeakPtr(), job.get()));
  if (rv == ERR_IO_PENDING) {
    job->AddRequest(request);
    active_jobs_[session_key] = std::move(job);
    return rv;
  }
  if (rv == OK) {
    auto it = active_sessions_.find(session_key);
    DCHECK(it != active_sessions_.end());
    if (it == active_sessions_.end())
      return ERR_QUIC_PROTOCOL_ERROR;
    QuicChromiumClientSession* session = it->second;
    request->SetSession(session->CreateHandle(std::move(destination)));
  }
  return rv;
}

void QuicStreamFactory::OnSessionGoingAway(QuicChromiumClientSession* session) {
  const AliasSet& aliases = session_aliases_[session];
  for (const auto& alias : aliases) {
    const QuicSessionKey& session_key = alias.session_key();
    DCHECK(active_sessions_.count(session_key));
    DCHECK_EQ(session, active_sessions_[session_key]);
    // Track sessions which have recently gone away so that we can disable
    // port suggestions.
    if (session->goaway_received())
      gone_away_aliases_.insert(alias);

    active_sessions_.erase(session_key);
    ProcessGoingAwaySession(session, session_key.server_id(), true);
  }
  ProcessGoingAwaySession(session, all_sessions_[session].server_id(), false);
  if (!aliases.empty()) {
    DCHECK(base::Contains(session_peer_ip_, session));
    const IPEndPoint peer_address = session_peer_ip_[session];
    ip_aliases_[peer_address].erase(session);
    if (ip_aliases_[peer_address].empty())
      ip_aliases_.erase(peer_address);
    session_peer_ip_.erase(session);
  }
  UnmapSessionFromSessionAliases(session);
}

void QuicStreamFactory::OnSessionClosed(QuicChromiumClientSession* session) {
  DCHECK_EQ(0u, session->GetNumActiveStreams());
  OnSessionGoingAway(session);
  delete session;
  all_sessions_.erase(session);
}

void QuicStreamFactory::OnBlackholeAfterHandshakeConfirmed(
    QuicChromiumClientSession* session) {
  // Reduce PING timeout when connection blackholes after the handshake.
  if (ping_timeout_ > reduced_ping_timeout_)
    ping_timeout_ = reduced_ping_timeout_;
}

void QuicStreamFactory::CancelRequest(QuicStreamRequest* request) {
  auto job_iter = active_jobs_.find(request->session_key());
  CHECK(job_iter != active_jobs_.end());
  job_iter->second->RemoveRequest(request);
}

void QuicStreamFactory::SetRequestPriority(QuicStreamRequest* request,
                                           RequestPriority priority) {
  auto job_iter = active_jobs_.find(request->session_key());
  if (job_iter == active_jobs_.end())
    return;
  job_iter->second->SetPriority(priority);
}

void QuicStreamFactory::CloseAllSessions(int error,
                                         quic::QuicErrorCode quic_error) {
  net_log_.AddEvent(NetLogEventType::QUIC_STREAM_FACTORY_CLOSE_ALL_SESSIONS);
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

base::Value QuicStreamFactory::QuicStreamFactoryInfoToValue() const {
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

void QuicStreamFactory::ClearCachedStatesInCryptoConfig(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  ServerIdOriginFilter filter(origin_filter);
  for (const auto& crypto_config : active_crypto_config_map_) {
    crypto_config.second->config()->ClearCachedStates(filter);
  }

  for (const auto& crypto_config : recent_crypto_config_map_) {
    crypto_config.second->config()->ClearCachedStates(filter);
  }
}

int QuicStreamFactory::ConnectAndConfigureSocket(
    CompletionOnceCallback callback,
    DatagramClientSocket* socket,
    IPEndPoint addr,
    handles::NetworkHandle network,
    const SocketTag& socket_tag) {
  socket->UseNonBlockingIO();

  int rv;
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  CompletionOnceCallback connect_callback =
      base::BindOnce(&QuicStreamFactory::FinishConnectAndConfigureSocket,
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

void QuicStreamFactory::FinishConnectAndConfigureSocket(
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
    rv = socket->SetRecvEcn();
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
      base::BindOnce(&QuicStreamFactory::DoCallback, weak_factory_.GetWeakPtr(),
                     std::move(callback), rv));
}

void QuicStreamFactory::OnFinishConnectAndConfigureSocketError(
    CompletionOnceCallback callback,
    enum CreateSessionFailure error,
    int rv) {
  DCHECK(callback);
  HistogramCreateSessionFailure(error);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&QuicStreamFactory::DoCallback, weak_factory_.GetWeakPtr(),
                     std::move(callback), rv));
}

void QuicStreamFactory::DoCallback(CompletionOnceCallback callback, int rv) {
  std::move(callback).Run(rv);
}

int QuicStreamFactory::ConfigureSocket(DatagramClientSocket* socket,
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
    rv = socket->SetRecvEcn();
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

handles::NetworkHandle QuicStreamFactory::FindAlternateNetwork(
    handles::NetworkHandle old_network) {
  // Find a new network that sessions bound to |old_network| can be migrated to.
  NetworkChangeNotifier::NetworkList network_list;
  NetworkChangeNotifier::GetConnectedNetworks(&network_list);
  for (handles::NetworkHandle new_network : network_list) {
    if (new_network != old_network)
      return new_network;
  }
  return handles::kInvalidNetworkHandle;
}

std::unique_ptr<DatagramClientSocket> QuicStreamFactory::CreateSocket(
    NetLog* net_log,
    const NetLogSource& source) {
  auto socket = client_socket_factory_->CreateDatagramClientSocket(
      DatagramSocket::DEFAULT_BIND, net_log, source);
  if (params_.enable_socket_recv_optimization)
    socket->EnableRecvOptimization();
  return socket;
}

void QuicStreamFactory::OnIPAddressChanged() {
  net_log_.AddEvent(NetLogEventType::QUIC_STREAM_FACTORY_ON_IP_ADDRESS_CHANGED);
  CollectDataOnPlatformNotification(NETWORK_IP_ADDRESS_CHANGED,
                                    handles::kInvalidNetworkHandle);
  // Do nothing if connection migration is turned on.
  if (params_.migrate_sessions_on_network_change_v2)
    return;

  connectivity_monitor_.OnIPAddressChanged();

  set_is_quic_known_to_work_on_current_network(false);
  if (params_.close_sessions_on_ip_change) {
    CloseAllSessions(ERR_NETWORK_CHANGED, quic::QUIC_IP_ADDRESS_CHANGED);
  } else {
    DCHECK(params_.goaway_sessions_on_ip_change);
    MarkAllActiveSessionsGoingAway(kIPAddressChanged);
  }
}

void QuicStreamFactory::OnNetworkConnected(handles::NetworkHandle network) {
  CollectDataOnPlatformNotification(NETWORK_CONNECTED, network);
  if (params_.migrate_sessions_on_network_change_v2) {
    net_log_.AddEventWithStringParams(
        NetLogEventType::QUIC_STREAM_FACTORY_PLATFORM_NOTIFICATION, "signal",
        "OnNetworkConnected");
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

void QuicStreamFactory::OnNetworkDisconnected(handles::NetworkHandle network) {
  CollectDataOnPlatformNotification(NETWORK_DISCONNECTED, network);
  if (params_.migrate_sessions_on_network_change_v2) {
    net_log_.AddEventWithStringParams(
        NetLogEventType::QUIC_STREAM_FACTORY_PLATFORM_NOTIFICATION, "signal",
        "OnNetworkDisconnected");
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
void QuicStreamFactory::OnNetworkSoonToDisconnect(
    handles::NetworkHandle network) {
  CollectDataOnPlatformNotification(NETWORK_SOON_TO_DISCONNECT, network);
}

void QuicStreamFactory::OnNetworkMadeDefault(handles::NetworkHandle network) {
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
    net_log_.AddEventWithStringParams(
        NetLogEventType::QUIC_STREAM_FACTORY_PLATFORM_NOTIFICATION, "signal",
        "OnNetworkMadeDefault");
  }

  auto it = all_sessions_.begin();
  // Sessions may be deleted while iterating through the map.
  while (it != all_sessions_.end()) {
    QuicChromiumClientSession* session = it->first;
    ++it;
    session->OnNetworkMadeDefault(network);
  }
  if (params_.migrate_sessions_on_network_change_v2)
    set_is_quic_known_to_work_on_current_network(false);
}

void QuicStreamFactory::OnTrustStoreChanged() {
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

void QuicStreamFactory::OnCertVerifierChanged() {
  // Flush sessions if the CertCerifier configuration has changed.
  MarkAllActiveSessionsGoingAway(kCertVerifierChanged);
}

void QuicStreamFactory::set_is_quic_known_to_work_on_current_network(
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

base::TimeDelta QuicStreamFactory::GetTimeDelayForWaitingJob(
    const QuicSessionKey& session_key) {
  // If |is_quic_known_to_work_on_current_network_| is false, then one of the
  // following is true:
  // 1) This is startup and QuicStreamFactory::CreateSession() and
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
  if (!srtt)
    srtt = kDefaultRTT;
  return base::Microseconds(srtt);
}

const std::set<std::string>& QuicStreamFactory::GetDnsAliasesForSessionKey(
    const QuicSessionKey& key) const {
  auto it = dns_aliases_by_session_key_.find(key);

  if (it == dns_aliases_by_session_key_.end()) {
    static const base::NoDestructor<std::set<std::string>> emptyvector_result;
    return *emptyvector_result;
  }

  return it->second;
}

bool QuicStreamFactory::HasMatchingIpSession(
    const QuicSessionAliasKey& key,
    const std::vector<IPEndPoint>& ip_endpoints,
    const std::set<std::string>& aliases,
    bool use_dns_aliases) {
  const quic::QuicServerId& server_id(key.server_id());
  DCHECK(!HasActiveSession(key.session_key()));
  for (const auto& address : ip_endpoints) {
    if (!base::Contains(ip_aliases_, address))
      continue;

    const SessionSet& sessions = ip_aliases_[address];
    for (QuicChromiumClientSession* session : sessions) {
      if (!session->CanPool(server_id.host(), key.session_key()))
        continue;
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

void QuicStreamFactory::OnJobComplete(Job* job, int rv) {
  auto iter = active_jobs_.find(job->key().session_key());
  DCHECK(iter != active_jobs_.end());
  if (rv == OK) {
    if (!is_quic_known_to_work_on_current_network_) {
      set_is_quic_known_to_work_on_current_network(true);
    }

    auto session_it = active_sessions_.find(job->key().session_key());
    CHECK(session_it != active_sessions_.end());
    QuicChromiumClientSession* session = session_it->second;
    for (auto* request : iter->second->stream_requests()) {
      // Do not notify |request| yet.
      request->SetSession(session->CreateHandle(job->key().destination()));
    }
  }

  for (auto* request : iter->second->stream_requests()) {
    // Even though we're invoking callbacks here, we don't need to worry
    // about |this| being deleted, because the factory is owned by the
    // profile which can not be deleted via callbacks.
    if (rv < 0) {
      job->PopulateNetErrorDetails(request->net_error_details());
    }
    request->OnRequestComplete(rv);
  }
  active_jobs_.erase(iter);
}

bool QuicStreamFactory::HasActiveSession(
    const QuicSessionKey& session_key) const {
  return base::Contains(active_sessions_, session_key);
}

bool QuicStreamFactory::HasActiveJob(const QuicSessionKey& session_key) const {
  return base::Contains(active_jobs_, session_key);
}

int QuicStreamFactory::CreateSessionSync(
    const QuicSessionAliasKey& key,
    quic::ParsedQuicVersion quic_version,
    int cert_verify_flags,
    bool require_confirmation,
    const HostResolverEndpointResult& endpoint_result,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    const NetLogWithSource& net_log,
    QuicChromiumClientSession** session,
    handles::NetworkHandle* network) {
  TRACE_EVENT0(NetTracingCategory(), "QuicStreamFactory::CreateSession");
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

int QuicStreamFactory::CreateSessionAsync(
    CompletionOnceCallback callback,
    const QuicSessionAliasKey& key,
    quic::ParsedQuicVersion quic_version,
    int cert_verify_flags,
    bool require_confirmation,
    const HostResolverEndpointResult& endpoint_result,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    const NetLogWithSource& net_log,
    QuicChromiumClientSession** session,
    handles::NetworkHandle* network) {
  TRACE_EVENT0(NetTracingCategory(), "QuicStreamFactory::CreateSession");
  // TODO(https://crbug.com/1416409): This logic only knows how to try one IP
  // endpoint.
  IPEndPoint addr = endpoint_result.ip_endpoints.front();
  std::unique_ptr<DatagramClientSocket> socket(
      CreateSocket(net_log.net_log(), net_log.source()));
  DatagramClientSocket* socket_ptr = socket.get();
  CompletionOnceCallback connect_and_configure_callback = base::BindOnce(
      &QuicStreamFactory::FinishCreateSession, weak_factory_.GetWeakPtr(),
      std::move(callback), key, quic_version, cert_verify_flags,
      require_confirmation, endpoint_result, dns_resolution_start_time,
      dns_resolution_end_time, net_log, session, network, std::move(socket));

  // If migrate_sessions_on_network_change_v2 is on, passing in
  // handles::kInvalidNetworkHandle will bind the socket to the default network.
  return ConnectAndConfigureSocket(std::move(connect_and_configure_callback),
                                   socket_ptr, addr, *network,
                                   key.session_key().socket_tag());
}

void QuicStreamFactory::FinishCreateSession(
    CompletionOnceCallback callback,
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

bool QuicStreamFactory::CreateSessionHelper(
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
    std::unique_ptr<DatagramClientSocket> socket) {
  // TODO(https://crbug.com/1416409): This logic only knows how to try one IP
  // endpoint.
  IPEndPoint addr = endpoint_result.ip_endpoints.front();
  const quic::QuicServerId& server_id = key.server_id();

  if (params_.migrate_sessions_on_network_change_v2 &&
      *network == handles::kInvalidNetworkHandle) {
    *network = socket->GetBoundNetwork();
    if (default_network_ == handles::kInvalidNetworkHandle) {
      // QuicStreamFactory may miss the default network signal before its
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
      new QuicChromiumPacketWriter(socket.get(), task_runner_);
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
  // either this session or the factory require confirmation.
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
      std::move(crypto_config_handle), dns_resolution_start_time,
      dns_resolution_end_time, tick_clock_, task_runner_,
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

void QuicStreamFactory::ActivateSession(const QuicSessionAliasKey& key,
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

void QuicStreamFactory::MarkAllActiveSessionsGoingAway(
    AllActiveSessionsGoingAwayReason reason) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_STREAM_FACTORY_MARK_ALL_ACTIVE_SESSIONS_GOING_AWAY);
  while (!active_sessions_.empty()) {
    QuicChromiumClientSession* session = active_sessions_.begin()->second;
    // If IP address change is detected, disable session's connectivity
    // monitoring by remove the Delegate.
    if (reason == kIPAddressChanged)
      connectivity_monitor_.OnSessionGoingAwayOnIPAddressChange(session);
    OnSessionGoingAway(session);
  }
}

void QuicStreamFactory::ConfigureInitialRttEstimate(
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

int64_t QuicStreamFactory::GetServerNetworkStatsSmoothedRttInMicroseconds(
    const quic::QuicServerId& server_id,
    const NetworkAnonymizationKey& network_anonymization_key) const {
  const base::TimeDelta* srtt =
      GetServerNetworkStatsSmoothedRtt(server_id, network_anonymization_key);
  return srtt == nullptr ? 0 : srtt->InMicroseconds();
}

const base::TimeDelta* QuicStreamFactory::GetServerNetworkStatsSmoothedRtt(
    const quic::QuicServerId& server_id,
    const NetworkAnonymizationKey& network_anonymization_key) const {
  url::SchemeHostPort server("https", server_id.host(), server_id.port());
  const ServerNetworkStats* stats =
      http_server_properties_->GetServerNetworkStats(server,
                                                     network_anonymization_key);
  if (stats == nullptr)
    return nullptr;
  return &(stats->srtt);
}

bool QuicStreamFactory::WasQuicRecentlyBroken(
    const QuicSessionKey& session_key) const {
  const AlternativeService alternative_service(
      kProtoQUIC, HostPortPair(session_key.server_id().host(),
                               session_key.server_id().port()));
  return http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, session_key.network_anonymization_key());
}

void QuicStreamFactory::InitializeMigrationOptions() {
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

  if (handle_ip_change)
    NetworkChangeNotifier::AddIPAddressObserver(this);

  if (allow_port_migration) {
    params_.allow_port_migration = true;
    if (migrate_idle_sessions) {
      params_.migrate_idle_sessions = true;
    }
  }

  if (!NetworkChangeNotifier::AreNetworkHandlesSupported())
    return;

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
  if (retry_on_alternate_network_before_handshake)
    params_.retry_on_alternate_network_before_handshake = true;

  // Enable migration for idle sessions.
  if (migrate_idle_sessions)
    params_.migrate_idle_sessions = true;
}

void QuicStreamFactory::InitializeCachedStateInCryptoConfig(
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

void QuicStreamFactory::ProcessGoingAwaySession(
    QuicChromiumClientSession* session,
    const quic::QuicServerId& server_id,
    bool session_was_active) {
  if (!http_server_properties_)
    return;

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

  if (!session_was_active)
    return;

  // TODO(rch):  In the special case where the session has received no packets
  // from the peer, we should consider blocking this differently so that we
  // still race TCP but we don't consider the session connected until the
  // handshake has been confirmed.
  HistogramBrokenAlternateProtocolLocation(
      BROKEN_ALTERNATE_PROTOCOL_LOCATION_QUIC_STREAM_FACTORY);

  // Since the session was active, there's no longer an HttpStreamFactory::Job
  // running which can mark it broken, unless the TCP job also fails. So to
  // avoid not using QUIC when we otherwise could, we mark it as recently
  // broken, which means that 0-RTT will be disabled but we'll still race.
  http_server_properties_->MarkAlternativeServiceRecentlyBroken(
      alternative_service,
      session->quic_session_key().network_anonymization_key());
}

void QuicStreamFactory::MapSessionToAliasKey(
    QuicChromiumClientSession* session,
    const QuicSessionAliasKey& key,
    std::set<std::string> dns_aliases) {
  session_aliases_[session].insert(key);
  dns_aliases_by_session_key_[key.session_key()] = std::move(dns_aliases);
}

void QuicStreamFactory::UnmapSessionFromSessionAliases(
    QuicChromiumClientSession* session) {
  for (const auto& key : session_aliases_[session])
    dns_aliases_by_session_key_.erase(key.session_key());
  session_aliases_.erase(session);
}

std::unique_ptr<QuicStreamFactory::CryptoClientConfigHandle>
QuicStreamFactory::CreateCryptoConfigHandle(
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
                       .emplace(std::make_pair(actual_network_anonymization_key,
                                               std::move(mru_iterator->second)))
                       .first;
    recent_crypto_config_map_.Erase(mru_iterator);
    return std::make_unique<CryptoClientConfigHandle>(map_iterator);
  }

  // Otherwise, create a new QuicCryptoClientConfigOwner and add it to
  // |active_crypto_config_map_|.
  std::unique_ptr<QuicCryptoClientConfigOwner> crypto_config_owner =
      std::make_unique<QuicCryptoClientConfigOwner>(
          std::make_unique<ProofVerifierChromium>(
              cert_verifier_, ct_policy_enforcer_, transport_security_state_,
              sct_auditing_delegate_,
              HostsFromOrigins(params_.origins_to_force_quic_on),
              actual_network_anonymization_key),
          std::make_unique<quic::QuicClientSessionCache>(), this);

  quic::QuicCryptoClientConfig* crypto_config = crypto_config_owner->config();
  crypto_config->AddCanonicalSuffix(".c.youtube.com");
  crypto_config->AddCanonicalSuffix(".ggpht.com");
  crypto_config->AddCanonicalSuffix(".googlevideo.com");
  crypto_config->AddCanonicalSuffix(".googleusercontent.com");
  crypto_config->AddCanonicalSuffix(".gvt1.com");

  ConfigureQuicCryptoClientConfig(*crypto_config);

  if (!prefer_aes_gcm_recorded_) {
    bool prefer_aes_gcm =
        !crypto_config->aead.empty() && (crypto_config->aead[0] == quic::kAESG);
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.PreferAesGcm", prefer_aes_gcm);
    prefer_aes_gcm_recorded_ = true;
  }

  map_iterator = active_crypto_config_map_
                     .emplace(std::make_pair(actual_network_anonymization_key,
                                             std::move(crypto_config_owner)))
                     .first;
  return std::make_unique<CryptoClientConfigHandle>(map_iterator);
}

void QuicStreamFactory::OnAllCryptoClientRefReleased(
    QuicCryptoClientConfigMap::iterator& map_iterator) {
  DCHECK_EQ(0, map_iterator->second->num_refs());
  recent_crypto_config_map_.Put(map_iterator->first,
                                std::move(map_iterator->second));
  active_crypto_config_map_.erase(map_iterator);
}

void QuicStreamFactory::CollectDataOnPlatformNotification(
    enum QuicPlatformNotification notification,
    handles::NetworkHandle affected_network) const {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.PlatformNotification",
                            notification, NETWORK_NOTIFICATION_MAX);
  connectivity_monitor_.RecordConnectivityStatsToHistograms(
      QuicPlatformNotificationToString(notification), affected_network);
}

std::unique_ptr<QuicCryptoClientConfigHandle>
QuicStreamFactory::GetCryptoConfigForTesting(
    const NetworkAnonymizationKey& network_anonymization_key) {
  return CreateCryptoConfigHandle(network_anonymization_key);
}

bool QuicStreamFactory::CryptoConfigCacheIsEmptyForTesting(
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
