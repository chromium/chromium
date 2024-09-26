// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool_direct_job.h"

#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"
#include "net/base/request_priority.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/address_utils.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_pool.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

QuicSessionPool::DirectJob::DirectJob(
    QuicSessionPool* pool,
    quic::ParsedQuicVersion quic_version,
    HostResolver* host_resolver,
    QuicSessionAliasKey key,
    std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
    bool retry_on_alternate_network_before_handshake,
    RequestPriority priority,
    bool use_dns_aliases,
    bool require_dns_https_alpn,
    int cert_verify_flags,
    const NetLogWithSource& net_log)
    : QuicSessionPool::Job::Job(
          pool,
          std::move(key),
          std::move(client_config_handle),
          priority,
          NetLogWithSource::Make(
              net_log.net_log(),
              NetLogSourceType::QUIC_SESSION_POOL_DIRECT_JOB)),
      quic_version_(std::move(quic_version)),
      host_resolver_(host_resolver),
      use_dns_aliases_(use_dns_aliases),
      require_dns_https_alpn_(require_dns_https_alpn),
      cert_verify_flags_(cert_verify_flags),
      retry_on_alternate_network_before_handshake_(
          retry_on_alternate_network_before_handshake) {
  // TODO(davidben): `require_dns_https_alpn_` only exists to be `DCHECK`ed
  // for consistency against `quic_version_`. Remove the parameter?
  DCHECK_EQ(quic_version_.IsKnown(), !require_dns_https_alpn_);
}

QuicSessionPool::DirectJob::~DirectJob() {}

int QuicSessionPool::DirectJob::Run(CompletionOnceCallback callback) {
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  }

  return rv > 0 ? OK : rv;
}

void QuicSessionPool::DirectJob::SetRequestExpectations(
    QuicSessionRequest* request) {
  if (!host_resolution_finished_) {
    request->ExpectOnHostResolution();
  }
  // Callers do not need to wait for OnQuicSessionCreationComplete if the
  // kAsyncQuicSession flag is not set because session creation will be fully
  // synchronous, so no need to call ExpectQuicSessionCreation.
  const bool session_creation_finished =
      session_attempt_ && session_attempt_->session_creation_finished();
  if (base::FeatureList::IsEnabled(net::features::kAsyncQuicSession) &&
      !session_creation_finished) {
    request->ExpectQuicSessionCreation();
  }
}

void QuicSessionPool::DirectJob::UpdatePriority(RequestPriority old_priority,
                                                RequestPriority new_priority) {
  if (old_priority == new_priority) {
    return;
  }

  if (resolve_host_request_ && !host_resolution_finished_) {
    resolve_host_request_->ChangeRequestPriority(new_priority);
  }
}

void QuicSessionPool::DirectJob::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (session_attempt_) {
    session_attempt_->PopulateNetErrorDetails(details);
  }
}

int QuicSessionPool::DirectJob::DoLoop(int rv) {
  TRACE_EVENT0(NetTracingCategory(), "QuicSessionPool::DirectJob::DoLoop");

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
      case STATE_ATTEMPT_SESSION:
        rv = DoAttemptSession();
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "io_state_: " << io_state_;
        break;
    }
  } while (io_state_ != STATE_NONE && rv != ERR_IO_PENDING);
  return rv;
}

int QuicSessionPool::DirectJob::DoResolveHost() {
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
  return resolve_host_request_->Start(
      base::BindOnce(&QuicSessionPool::DirectJob::OnResolveHostComplete,
                     base::Unretained(this)));
}

int QuicSessionPool::DirectJob::DoResolveHostComplete(int rv) {
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
    quic::ParsedQuicVersion endpoint_quic_version = pool_->SelectQuicVersion(
        quic_version_, endpoint.metadata, svcb_optional);
    if (!endpoint_quic_version.IsKnown()) {
      continue;
    }
    if (pool_->HasMatchingIpSession(
            key_, endpoint.ip_endpoints,
            *resolve_host_request_->GetDnsAliasResults(), use_dns_aliases_)) {
      LogConnectionIpPooling(true);
      return OK;
    }
  }
  io_state_ = STATE_ATTEMPT_SESSION;
  return OK;
}

int QuicSessionPool::DirectJob::DoAttemptSession() {
  // TODO(crbug.com/40256842): This logic only knows how to try one
  // endpoint result.
  bool svcb_optional =
      IsSvcbOptional(*resolve_host_request_->GetEndpointResults());
  bool found = false;
  HostResolverEndpointResult endpoint_result;
  quic::ParsedQuicVersion quic_version_used =
      quic::ParsedQuicVersion::Unsupported();
  for (const auto& candidate : *resolve_host_request_->GetEndpointResults()) {
    quic::ParsedQuicVersion endpoint_quic_version = pool_->SelectQuicVersion(
        quic_version_, candidate.metadata, svcb_optional);
    if (endpoint_quic_version.IsKnown()) {
      found = true;
      quic_version_used = endpoint_quic_version;
      endpoint_result = candidate;
      break;
    }
  }
  if (!found) {
    return ERR_DNS_NO_MATCHING_SUPPORTED_ALPN;
  }

  std::set<std::string> dns_aliases =
      use_dns_aliases_ && resolve_host_request_->GetDnsAliasResults()
          ? *resolve_host_request_->GetDnsAliasResults()
          : std::set<std::string>();
  // Passing an empty `crypto_client_config_handle` is safe because this job
  // already owns a handle.
  session_attempt_ = std::make_unique<QuicSessionAttempt>(
      this, endpoint_result.ip_endpoints.front(), endpoint_result.metadata,
      std::move(quic_version_used), cert_verify_flags_,
      dns_resolution_start_time_, dns_resolution_end_time_,
      retry_on_alternate_network_before_handshake_, use_dns_aliases_,
      std::move(dns_aliases), /*crypto_client_config_handle=*/nullptr);

  return session_attempt_->Start(
      base::BindOnce(&DirectJob::OnSessionAttemptComplete, GetWeakPtr()));
}

void QuicSessionPool::DirectJob::OnResolveHostComplete(int rv) {
  DCHECK(!host_resolution_finished_);
  io_state_ = STATE_RESOLVE_HOST_COMPLETE;
  rv = DoLoop(rv);

  for (QuicSessionRequest* request : requests()) {
    request->OnHostResolutionComplete(rv, dns_resolution_start_time_,
                                      dns_resolution_end_time_);
  }

  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

void QuicSessionPool::DirectJob::OnSessionAttemptComplete(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  if (!callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

bool QuicSessionPool::DirectJob::IsSvcbOptional(
    base::span<const HostResolverEndpointResult> results) const {
  // If SVCB/HTTPS resolution succeeded, the client supports ECH, and all
  // routes support ECH, disable the A/AAAA fallback. See Section 10.1 of
  // draft-ietf-dnsop-svcb-https-11.
  if (!pool_->ssl_config_service_->GetSSLContextConfig().ech_enabled) {
    return true;  // ECH is not supported for this request.
  }

  return !HostResolver::AllProtocolEndpointsHaveEch(results);
}

}  // namespace net
