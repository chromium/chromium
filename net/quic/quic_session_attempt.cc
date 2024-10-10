// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_attempt.h"

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/address_utils.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_pool.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

namespace {

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

void LogStaleConnectionTime(base::TimeTicks start_time) {
  UMA_HISTOGRAM_TIMES("Net.QuicSession.StaleConnectionTime",
                      base::TimeTicks::Now() - start_time);
}

void LogValidConnectionTime(base::TimeTicks start_time) {
  UMA_HISTOGRAM_TIMES("Net.QuicSession.ValidConnectionTime",
                      base::TimeTicks::Now() - start_time);
}

}  // namespace

QuicSessionAttempt::QuicSessionAttempt(
    Delegate* delegate,
    IPEndPoint ip_endpoint,
    ConnectionEndpointMetadata metadata,
    quic::ParsedQuicVersion quic_version,
    int cert_verify_flags,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    bool retry_on_alternate_network_before_handshake,
    bool use_dns_aliases,
    std::set<std::string> dns_aliases,
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_client_config_handle)
    : delegate_(delegate),
      ip_endpoint_(std::move(ip_endpoint)),
      metadata_(std::move(metadata)),
      quic_version_(std::move(quic_version)),
      cert_verify_flags_(cert_verify_flags),
      dns_resolution_start_time_(dns_resolution_start_time),
      dns_resolution_end_time_(dns_resolution_end_time),
      was_alternative_service_recently_broken_(
          pool()->WasQuicRecentlyBroken(key().session_key())),
      retry_on_alternate_network_before_handshake_(
          retry_on_alternate_network_before_handshake),
      use_dns_aliases_(use_dns_aliases),
      dns_aliases_(std::move(dns_aliases)),
      crypto_client_config_handle_(std::move(crypto_client_config_handle)) {
  CHECK(delegate_);
  DCHECK_NE(quic_version_, quic::ParsedQuicVersion::Unsupported());
}

QuicSessionAttempt::QuicSessionAttempt(
    Delegate* delegate,
    IPEndPoint local_endpoint,
    IPEndPoint proxy_peer_endpoint,
    quic::ParsedQuicVersion quic_version,
    int cert_verify_flags,
    std::unique_ptr<QuicChromiumClientStream::Handle> proxy_stream,
    const HttpUserAgentSettings* http_user_agent_settings)
    : delegate_(delegate),
      ip_endpoint_(std::move(proxy_peer_endpoint)),
      quic_version_(std::move(quic_version)),
      cert_verify_flags_(cert_verify_flags),
      was_alternative_service_recently_broken_(
          pool()->WasQuicRecentlyBroken(key().session_key())),
      retry_on_alternate_network_before_handshake_(false),
      use_dns_aliases_(false),
      proxy_stream_(std::move(proxy_stream)),
      http_user_agent_settings_(http_user_agent_settings),
      local_endpoint_(std::move(local_endpoint)) {
  CHECK(delegate_);
  DCHECK_NE(quic_version_, quic::ParsedQuicVersion::Unsupported());
}

QuicSessionAttempt::~QuicSessionAttempt() = default;

int QuicSessionAttempt::Start(CompletionOnceCallback callback) {
  CHECK_EQ(next_state_, State::kNone);

  next_state_ = State::kCreateSession;
  int rv = DoLoop(OK);
  if (rv != ERR_IO_PENDING) {
    return rv;
  }

  callback_ = std::move(callback);
  return rv;
}

void QuicSessionAttempt::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (session_) {
    details->connection_info = QuicHttpStream::ConnectionInfoFromQuicVersion(
        session_->connection()->version());
    details->quic_connection_error = session_->error();
  } else {
    details->connection_info = connection_info_;
    details->quic_connection_error = quic_connection_error_;
  }
}

int QuicSessionAttempt::DoLoop(int rv) {
  CHECK(!in_loop_);
  CHECK_NE(next_state_, State::kNone);

  base::AutoReset<bool> auto_reset(&in_loop_, true);
  do {
    State state = next_state_;
    next_state_ = State::kNone;
    switch (state) {
      case State::kNone:
        CHECK(false) << "Invalid state";
        break;
      case State::kCreateSession:
        rv = DoCreateSession();
        break;
      case State::kCreateSessionComplete:
        rv = DoCreateSessionComplete(rv);
        break;
      case State::kCryptoConnect:
        rv = DoCryptoConnect(rv);
        break;
      case State::kConfirmConnection:
        rv = DoConfirmConnection(rv);
        break;
    }
  } while (next_state_ != State::kNone && rv != ERR_IO_PENDING);
  return rv;
}

int QuicSessionAttempt::DoCreateSession() {
  quic_connection_start_time_ = base::TimeTicks::Now();
  next_state_ = State::kCreateSessionComplete;

  const bool require_confirmation = was_alternative_service_recently_broken_;
  net_log().AddEntryWithBoolParams(
      NetLogEventType::QUIC_SESSION_POOL_JOB_CONNECT, NetLogEventPhase::BEGIN,
      "require_confirmation", require_confirmation);

  if (proxy_stream_) {
    std::string user_agent;
    if (http_user_agent_settings_) {
      user_agent = http_user_agent_settings_->GetUserAgent();
    }
    // Proxied connections are not on any specific network.
    network_ = handles::kInvalidNetworkHandle;
    pool()->CreateSessionOnProxyStream(
        base::BindOnce(&QuicSessionAttempt::OnCreateSessionComplete,
                       weak_ptr_factory_.GetWeakPtr()),
        key(), quic_version_, cert_verify_flags_, require_confirmation,
        std::move(local_endpoint_), std::move(ip_endpoint_),
        std::move(proxy_stream_), user_agent, net_log(), network_);
    return ERR_IO_PENDING;
  } else {
    if (base::FeatureList::IsEnabled(net::features::kAsyncQuicSession)) {
      pool()->CreateSessionAsync(
          base::BindOnce(&QuicSessionAttempt::OnCreateSessionComplete,
                         weak_ptr_factory_.GetWeakPtr()),
          key(), quic_version_, cert_verify_flags_, require_confirmation,
          ip_endpoint_, metadata_, dns_resolution_start_time_,
          dns_resolution_end_time_, net_log(), network_);
      return ERR_IO_PENDING;
    }
    int rv = pool()->CreateSessionSync(
        key(), quic_version_, cert_verify_flags_, require_confirmation,
        ip_endpoint_, metadata_, dns_resolution_start_time_,
        dns_resolution_end_time_, net_log(), &session_, &network_);
    if (rv == ERR_QUIC_PROTOCOL_ERROR) {
      DCHECK(!session_);
      HistogramProtocolErrorLocation(
          JobProtocolErrorLocation::kCreateSessionFailedSync);
    }
    DVLOG(1) << "Created session on network: " << network_;
    return rv;
  }
}

int QuicSessionAttempt::DoCreateSessionComplete(int rv) {
  session_creation_finished_ = true;
  if (rv != OK) {
    CHECK(!session_);
    return rv;
  }

  next_state_ = State::kCryptoConnect;
  if (!session_->connection()->connected()) {
    return ERR_CONNECTION_CLOSED;
  }

  CHECK(session_);
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

int QuicSessionAttempt::DoCryptoConnect(int rv) {
  if (rv != OK) {
    // Reset `session_` to avoid dangling pointer.
    ResetSession();
    return rv;
  }

  DCHECK(session_);
  next_state_ = State::kConfirmConnection;
  rv = session_->CryptoConnect(
      base::BindOnce(&QuicSessionAttempt::OnCryptoConnectComplete,
                     weak_ptr_factory_.GetWeakPtr()));

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

int QuicSessionAttempt::DoConfirmConnection(int rv) {
  UMA_HISTOGRAM_TIMES("Net.QuicSession.TimeFromResolveHostToConfirmConnection",
                      base::TimeTicks::Now() - dns_resolution_start_time_);
  net_log().EndEvent(NetLogEventType::QUIC_SESSION_POOL_JOB_CONNECT);

  if (was_alternative_service_recently_broken_) {
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ConnectAfterBroken", rv == OK);
  }

  if (retry_on_alternate_network_before_handshake_ && session_ &&
      !session_->OneRttKeysAvailable() &&
      network_ == pool()->default_network()) {
    if (session_->error() == quic::QUIC_NETWORK_IDLE_TIMEOUT ||
        session_->error() == quic::QUIC_HANDSHAKE_TIMEOUT ||
        session_->error() == quic::QUIC_PACKET_WRITE_ERROR) {
      // Retry the connection on an alternate network if crypto handshake failed
      // with network idle time out or handshake time out.
      DCHECK(network_ != handles::kInvalidNetworkHandle);
      network_ = pool()->FindAlternateNetwork(network_);
      connection_retried_ = network_ != handles::kInvalidNetworkHandle;
      UMA_HISTOGRAM_BOOLEAN(
          "Net.QuicStreamFactory.AttemptMigrationBeforeHandshake",
          connection_retried_);
      UMA_HISTOGRAM_ENUMERATION(
          "Net.QuicStreamFactory.AttemptMigrationBeforeHandshake."
          "FailedConnectionType",
          NetworkChangeNotifier::GetNetworkConnectionType(
              pool()->default_network()),
          NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
      if (connection_retried_) {
        UMA_HISTOGRAM_ENUMERATION(
            "Net.QuicStreamFactory.MigrationBeforeHandshake.NewConnectionType",
            NetworkChangeNotifier::GetNetworkConnectionType(network_),
            NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
        net_log().AddEvent(
            NetLogEventType::QUIC_SESSION_POOL_JOB_RETRY_ON_ALTERNATE_NETWORK);
        // Notify requests that connection on the default network failed.
        delegate_->OnConnectionFailedOnDefaultNetwork();
        DVLOG(1) << "Retry connection on alternate network: " << network_;
        session_ = nullptr;
        next_state_ = State::kCreateSession;
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
          network_ == pool()->default_network());
    } else {
      base::UmaHistogramSparse(
          "Net.QuicStreamFactory.MigrationBeforeHandshakeFailedReason", -rv);
    }
  } else if (network_ != handles::kInvalidNetworkHandle &&
             network_ != pool()->default_network()) {
    UMA_HISTOGRAM_BOOLEAN("Net.QuicStreamFactory.ConnectionOnNonDefaultNetwork",
                          rv == OK);
  }

  if (rv != OK) {
    // Reset `session_` to avoid dangling pointer.
    ResetSession();
    return rv;
  }

  DCHECK(!pool()->HasActiveSession(key().session_key()));
  // There may well now be an active session for this IP.  If so, use the
  // existing session instead.
  if (pool()->HasMatchingIpSession(
          key(), {ToIPEndPoint(session_->connection()->peer_address())},
          /*aliases=*/{}, use_dns_aliases_)) {
    QuicSessionPool::LogConnectionIpPooling(true);
    session_->connection()->CloseConnection(
        quic::QUIC_CONNECTION_IP_POOLED,
        "An active session exists for the given IP.",
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    session_ = nullptr;
    return OK;
  }
  QuicSessionPool::LogConnectionIpPooling(false);

  pool()->ActivateSession(
      key(), session_,
      use_dns_aliases_ ? std::move(dns_aliases_) : std::set<std::string>());

  return OK;
}

void QuicSessionAttempt::OnCreateSessionComplete(
    base::expected<CreateSessionResult, int> result) {
  CHECK_EQ(next_state_, State::kCreateSessionComplete);
  if (result.has_value()) {
    session_ = result->session;
    network_ = result->network;
    DVLOG(1) << "Created session on network: " << network_;
  } else {
    if (result.error() == ERR_QUIC_PROTOCOL_ERROR) {
      HistogramProtocolErrorLocation(
          JobProtocolErrorLocation::kCreateSessionFailedAsync);
    }
  }

  int rv = DoLoop(result.error_or(OK));

  delegate_->OnQuicSessionCreationComplete(rv);

  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

void QuicSessionAttempt::OnCryptoConnectComplete(int rv) {
  CHECK_EQ(next_state_, State::kConfirmConnection);

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

  rv = DoLoop(rv);
  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

void QuicSessionAttempt::ResetSession() {
  CHECK(session_);
  connection_info_ = QuicHttpStream::ConnectionInfoFromQuicVersion(
      session_->connection()->version());
  quic_connection_error_ = session_->error();
  session_ = nullptr;
}

}  // namespace net
