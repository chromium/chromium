// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool_job.h"

#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"
#include "net/base/request_priority.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/address_utils.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_pool.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

namespace {

base::Value::Dict NetLogQuicSessionPoolJobParams(
    const QuicSessionPool::QuicSessionAliasKey* key) {
  return base::Value::Dict()
      .Set("host", key->server_id().host())
      .Set("port", key->server_id().port())
      .Set("privacy_mode",
           PrivacyModeToDebugString(key->session_key().privacy_mode()))
      .Set("network_anonymization_key",
           key->session_key().network_anonymization_key().ToDebugString());
}

enum class JobProtocolErrorLocation {
  kSessionStartReadingFailedAsync = 0,
  kSessionStartReadingFailedSync = 1,
  kCreateSessionFailedAsync = 2,
  kCreateSessionFailedSync = 3,
  kCryptoConnectFailedSync = 4,
  kCryptoConnectFailedAsync = 5,
  kMaxValue = kCryptoConnectFailedAsync,
};

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

}  // namespace

QuicSessionPool::Job::Job(
    QuicSessionPool* pool,
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
    : pool_(pool),
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
      net_log_(NetLogWithSource::Make(net_log.net_log(),
                                      NetLogSourceType::QUIC_SESSION_POOL_JOB)),
      network_(handles::kInvalidNetworkHandle) {
  DCHECK_EQ(quic_version.IsKnown(), !require_dns_https_alpn);
  net_log_.BeginEvent(NetLogEventType::QUIC_SESSION_POOL_JOB,
                      [&] { return NetLogQuicSessionPoolJobParams(&key_); });
  // Associate |net_log_| with |net_log|.
  net_log_.AddEventReferencingSource(
      NetLogEventType::QUIC_SESSION_POOL_JOB_BOUND_TO_HTTP_STREAM_JOB,
      net_log.source());
  net_log.AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_JOB_BOUND_TO_QUIC_SESSION_POOL_JOB,
      net_log_.source());
}

QuicSessionPool::Job::~Job() {
  net_log_.EndEvent(NetLogEventType::QUIC_SESSION_POOL_JOB);
  // If |this| is destroyed in QuicSessionPool's destructor, |callback_| is
  // non-null.
}

int QuicSessionPool::Job::Run(CompletionOnceCallback callback) {
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  }

  return rv > 0 ? OK : rv;
}

void QuicSessionPool::Job::AddRequest(QuicSessionRequest* request) {
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

void QuicSessionPool::Job::RemoveRequest(QuicSessionRequest* request) {
  auto request_iter = stream_requests_.find(request);
  DCHECK(request_iter != stream_requests_.end());
  stream_requests_.erase(request_iter);
}

void QuicSessionPool::Job::SetPriority(RequestPriority priority) {
  if (priority_ == priority) {
    return;
  }

  priority_ = priority;
  if (resolve_host_request_ && !host_resolution_finished_) {
    resolve_host_request_->ChangeRequestPriority(priority);
  }
}

void QuicSessionPool::Job::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (!session_) {
    return;
  }
  details->connection_info = QuicHttpStream::ConnectionInfoFromQuicVersion(
      session_->connection()->version());
  details->quic_connection_error = session_->error();
}

int QuicSessionPool::Job::DoLoop(int rv) {
  TRACE_EVENT0(NetTracingCategory(), "QuicSessionPool::Job::DoLoop");

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

int QuicSessionPool::Job::DoResolveHost() {
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
      &QuicSessionPool::Job::OnResolveHostComplete, base::Unretained(this)));
}

int QuicSessionPool::Job::DoResolveHostComplete(int rv) {
  host_resolution_finished_ = true;
  dns_resolution_end_time_ = base::TimeTicks::Now();
  if (rv != OK) {
    return rv;
  }

  DCHECK(!pool_->HasActiveSession(key_.session_key()));

  // Inform the pool of this resolution, which will set up
  // a session alias, if possible.
  const bool svcb_optional =
      IsSvcbOptional(*resolve_host_request_->GetEndpointResults());
  for (const auto& endpoint : *resolve_host_request_->GetEndpointResults()) {
    // Only consider endpoints that would have been eligible for QUIC.
    if (!SelectQuicVersion(endpoint, svcb_optional).IsKnown()) {
      continue;
    }
    if (pool_->HasMatchingIpSession(
            key_, endpoint.ip_endpoints,
            *resolve_host_request_->GetDnsAliasResults(), use_dns_aliases_)) {
      LogConnectionIpPooling(true);
      return OK;
    }
  }
  io_state_ = STATE_CREATE_SESSION;
  return OK;
}

int QuicSessionPool::Job::DoCreateSession() {
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
      NetLogEventType::QUIC_SESSION_POOL_JOB_CONNECT, NetLogEventPhase::BEGIN,
      "require_confirmation", require_confirmation);

  DCHECK_NE(quic_version_used_, quic::ParsedQuicVersion::Unsupported());
  if (base::FeatureList::IsEnabled(net::features::kAsyncQuicSession)) {
    return pool_->CreateSessionAsync(
        base::BindOnce(&QuicSessionPool::Job::OnCreateSessionComplete,
                       GetWeakPtr()),
        key_, quic_version_used_, cert_verify_flags_, require_confirmation,
        endpoint_result_, dns_resolution_start_time_, dns_resolution_end_time_,
        net_log_, &session_, &network_);
  }
  int rv = pool_->CreateSessionSync(
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

int QuicSessionPool::Job::DoCreateSessionComplete(int rv) {
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

int QuicSessionPool::Job::DoConnect(int rv) {
  if (rv != OK) {
    return rv;
  }
  DCHECK(session_);
  io_state_ = STATE_CONFIRM_CONNECTION;
  rv = session_->CryptoConnect(base::BindOnce(
      &QuicSessionPool::Job::OnCryptoConnectComplete, GetWeakPtr()));

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

int QuicSessionPool::Job::DoConfirmConnection(int rv) {
  UMA_HISTOGRAM_TIMES("Net.QuicSession.TimeFromResolveHostToConfirmConnection",
                      base::TimeTicks::Now() - dns_resolution_start_time_);
  net_log_.EndEvent(NetLogEventType::QUIC_SESSION_POOL_JOB_CONNECT);

  if (was_alternative_service_recently_broken_) {
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ConnectAfterBroken", rv == OK);
  }

  if (retry_on_alternate_network_before_handshake_ && session_ &&
      !session_->OneRttKeysAvailable() &&
      network_ == pool_->default_network()) {
    if (session_->error() == quic::QUIC_NETWORK_IDLE_TIMEOUT ||
        session_->error() == quic::QUIC_HANDSHAKE_TIMEOUT ||
        session_->error() == quic::QUIC_PACKET_WRITE_ERROR) {
      // Retry the connection on an alternate network if crypto handshake failed
      // with network idle time out or handshake time out.
      DCHECK(network_ != handles::kInvalidNetworkHandle);
      network_ = pool_->FindAlternateNetwork(network_);
      connection_retried_ = network_ != handles::kInvalidNetworkHandle;
      UMA_HISTOGRAM_BOOLEAN(
          "Net.QuicStreamFactory.AttemptMigrationBeforeHandshake",
          connection_retried_);
      UMA_HISTOGRAM_ENUMERATION(
          "Net.QuicStreamFactory.AttemptMigrationBeforeHandshake."
          "FailedConnectionType",
          NetworkChangeNotifier::GetNetworkConnectionType(
              pool_->default_network()),
          NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
      if (connection_retried_) {
        UMA_HISTOGRAM_ENUMERATION(
            "Net.QuicStreamFactory.MigrationBeforeHandshake.NewConnectionType",
            NetworkChangeNotifier::GetNetworkConnectionType(network_),
            NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
        net_log_.AddEvent(
            NetLogEventType::QUIC_SESSION_POOL_JOB_RETRY_ON_ALTERNATE_NETWORK);
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
          network_ == pool_->default_network());
    } else {
      base::UmaHistogramSparse(
          "Net.QuicStreamFactory.MigrationBeforeHandshakeFailedReason", -rv);
    }
  } else if (network_ != handles::kInvalidNetworkHandle &&
             network_ != pool_->default_network()) {
    UMA_HISTOGRAM_BOOLEAN("Net.QuicStreamFactory.ConnectionOnNonDefaultNetwork",
                          rv == OK);
  }

  if (rv != OK) {
    return rv;
  }

  DCHECK(!pool_->HasActiveSession(key_.session_key()));
  // There may well now be an active session for this IP.  If so, use the
  // existing session instead.
  if (pool_->HasMatchingIpSession(
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
  pool_->ActivateSession(key_, session_, std::move(dns_aliases));

  return OK;
}

void QuicSessionPool::Job::OnResolveHostComplete(int rv) {
  DCHECK(!host_resolution_finished_);
  io_state_ = STATE_RESOLVE_HOST_COMPLETE;
  rv = DoLoop(rv);

  for (auto* request : stream_requests_) {
    request->OnHostResolutionComplete(rv);
  }

  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

void QuicSessionPool::Job::OnCreateSessionComplete(int rv) {
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

void QuicSessionPool::Job::OnCryptoConnectComplete(int rv) {
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
  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

void QuicSessionPool::Job::CloseStaleHostConnection() {
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
bool QuicSessionPool::Job::IsSvcbOptional(
    base::span<const HostResolverEndpointResult> results) const {
  // If SVCB/HTTPS resolution succeeded, the client supports ECH, and all
  // routes support ECH, disable the A/AAAA fallback. See Section 10.1 of
  // draft-ietf-dnsop-svcb-https-11.
  if (!pool_->ssl_config_service_->GetSSLContextConfig().ech_enabled) {
    return true;  // ECH is not supported for this request.
  }

  return !HostResolver::AllProtocolEndpointsHaveEch(results);
}

// Returns the QUIC version that would be used with `endpoint_result`, or
// `quic::ParsedQuicVersion::Unsupported()` if `endpoint_result` cannot be
// used with QUIC.
quic::ParsedQuicVersion QuicSessionPool::Job::SelectQuicVersion(
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
    for (const auto& supported_version : pool_->supported_versions()) {
      if (alpn == AlpnForVersion(supported_version)) {
        return supported_version;
      }
    }
  }

  return quic::ParsedQuicVersion::Unsupported();
}

void QuicSessionPool::Job::LogStaleAndFreshHostMatched(bool matched) {
  if (matched) {
    net_log_.AddEvent(
        NetLogEventType::QUIC_SESSION_POOL_JOB_STALE_HOST_RESOLUTION_MATCHED);
  } else {
    net_log_.AddEvent(
        NetLogEventType::QUIC_SESSION_POOL_JOB_STALE_HOST_RESOLUTION_NO_MATCH);
  }
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.StaleAndFreshHostMatched", matched);
}

}  // namespace net
