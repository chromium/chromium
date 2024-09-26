// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool_proxy_job.h"

#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"
#include "net/base/request_priority.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/address_utils.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_pool.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packet_writer.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

QuicSessionPool::ProxyJob::ProxyJob(
    QuicSessionPool* pool,
    quic::ParsedQuicVersion target_quic_version,
    QuicSessionAliasKey key,
    NetworkTrafficAnnotationTag proxy_annotation_tag,
    const HttpUserAgentSettings* http_user_agent_settings,
    std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
    RequestPriority priority,
    int cert_verify_flags,
    const NetLogWithSource& net_log)
    : QuicSessionPool::Job::Job(
          pool,
          std::move(key),
          std::move(client_config_handle),
          priority,
          NetLogWithSource::Make(
              net_log.net_log(),
              NetLogSourceType::QUIC_SESSION_POOL_PROXY_JOB)),
      io_callback_(base::BindRepeating(&QuicSessionPool::ProxyJob::OnIOComplete,
                                       base::Unretained(this))),
      target_quic_version_(target_quic_version),
      proxy_annotation_tag_(proxy_annotation_tag),
      cert_verify_flags_(cert_verify_flags),
      http_user_agent_settings_(http_user_agent_settings) {
  DCHECK(!Job::key().session_key().proxy_chain().is_direct());
  // The job relies on the the proxy to resolve DNS for the destination, so
  // cannot determine protocol information from DNS. We must know the QUIC
  // version already.
  CHECK(target_quic_version.IsKnown())
      << "Cannot make QUIC proxy connections without a known QUIC version";
}

QuicSessionPool::ProxyJob::~ProxyJob() = default;

int QuicSessionPool::ProxyJob::Run(CompletionOnceCallback callback) {
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  }

  return rv > 0 ? OK : rv;
}

void QuicSessionPool::ProxyJob::SetRequestExpectations(
    QuicSessionRequest* request) {
  // This Job does not do host resolution, but can notify when the session
  // creation is finished.
  const bool session_creation_finished =
      session_attempt_ && session_attempt_->session_creation_finished();
  if (!session_creation_finished) {
    request->ExpectQuicSessionCreation();
  }
}

void QuicSessionPool::ProxyJob::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  // First, prefer any error details reported from creating the session over
  // which this job is carried.
  if (net_error_details_.quic_connection_error != quic::QUIC_NO_ERROR) {
    *details = net_error_details_;
    return;
  }

  // Second, prefer to include error details from the session over which this
  // job is carried, as any error in that session is "closer to" the client.
  if (proxy_session_) {
    proxy_session_->PopulateNetErrorDetails(details);
    if (details->quic_connection_error != quic::QUIC_NO_ERROR) {
      return;
    }
  }

  // Finally, return the error from the session attempt.
  if (session_attempt_) {
    session_attempt_->PopulateNetErrorDetails(details);
  }
}

int QuicSessionPool::ProxyJob::DoLoop(int rv) {
  do {
    IoState state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      case STATE_CREATE_PROXY_SESSION:
        CHECK_EQ(OK, rv);
        rv = DoCreateProxySession();
        break;
      case STATE_CREATE_PROXY_SESSION_COMPLETE:
        rv = DoCreateProxySessionComplete(rv);
        break;
      case STATE_CREATE_PROXY_STREAM:
        CHECK_EQ(OK, rv);
        rv = DoCreateProxyStream();
        break;
      case STATE_CREATE_PROXY_STREAM_COMPLETE:
        rv = DoCreateProxyStreamComplete(rv);
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

void QuicSessionPool::ProxyJob::OnSessionAttemptComplete(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  if (!callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

void QuicSessionPool::ProxyJob::OnIOComplete(int rv) {
  rv = DoLoop(rv);
  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

int QuicSessionPool::ProxyJob::DoCreateProxySession() {
  io_state_ = STATE_CREATE_PROXY_SESSION_COMPLETE;

  net_log().BeginEvent(NetLogEventType::QUIC_SESSION_POOL_PROXY_JOB_CONNECT);

  const QuicSessionKey& session_key = key_.session_key();
  auto [proxy_chain_prefix, last_proxy_server] =
      session_key.proxy_chain().SplitLast();
  auto last_server = last_proxy_server.host_port_pair();
  url::SchemeHostPort destination(url::kHttpsScheme, last_server.host(),
                                  last_server.port());

  net_log_.BeginEventWithStringParams(
      NetLogEventType::QUIC_SESSION_POOL_PROXY_JOB_CREATE_PROXY_SESSION,
      "destination", destination.Serialize());

  // Select the default QUIC version for the session to the proxy, since there
  // is no DNS or Alt-Svc information to use.
  quic::ParsedQuicVersion quic_version = SupportedQuicVersionForProxying();

  // In order to support connection re-use in multi-proxy chains, without
  // sacrificing partitioning, use an empty NAK for connections to a proxy that
  // are carrying a connection to another proxy. For example, given chain
  // [proxy1, proxy2, proxy3], the connections to proxy1 and proxy2 need not be
  // partitioned and can use an empty NAK. This situation is identified by the
  // session usage of the tunneled connection being kProxy.
  bool use_empty_nak = false;
  if (!base::FeatureList::IsEnabled(net::features::kPartitionProxyChains) &&
      session_key.session_usage() == SessionUsage::kProxy) {
    use_empty_nak = true;
  }

  proxy_session_request_ = std::make_unique<QuicSessionRequest>(pool_);
  return proxy_session_request_->Request(
      destination, quic_version, proxy_chain_prefix, proxy_annotation_tag_,
      http_user_agent_settings_.get(), SessionUsage::kProxy,
      session_key.privacy_mode(), priority(), session_key.socket_tag(),
      use_empty_nak ? NetworkAnonymizationKey()
                    : session_key.network_anonymization_key(),
      session_key.secure_dns_policy(), session_key.require_dns_https_alpn(),
      cert_verify_flags_, GURL("https://" + last_server.ToString()), net_log(),
      &net_error_details_,
      /*failed_on_default_network_callback=*/CompletionOnceCallback(),
      io_callback_);
}

int QuicSessionPool::ProxyJob::DoCreateProxySessionComplete(int rv) {
  net_log().EndEventWithNetErrorCode(
      NetLogEventType::QUIC_SESSION_POOL_PROXY_JOB_CREATE_PROXY_SESSION, rv);
  if (rv != 0) {
    proxy_session_request_.reset();
    return rv;
  }
  io_state_ = STATE_CREATE_PROXY_STREAM;
  proxy_session_ = proxy_session_request_->ReleaseSessionHandle();
  proxy_session_request_.reset();

  return OK;
}

int QuicSessionPool::ProxyJob::DoCreateProxyStream() {
  // Requiring confirmation here means more confidence that the underlying
  // connection is working before building the proxy tunnel, at the cost of one
  // more round-trip.
  io_state_ = STATE_CREATE_PROXY_STREAM_COMPLETE;
  return proxy_session_->RequestStream(/*requires_confirmation=*/true,
                                       io_callback_, proxy_annotation_tag_);
}

int QuicSessionPool::ProxyJob::DoCreateProxyStreamComplete(int rv) {
  if (rv != 0) {
    return rv;
  }
  proxy_stream_ = proxy_session_->ReleaseStream();

  DCHECK(proxy_stream_);
  if (!proxy_stream_->IsOpen()) {
    return ERR_CONNECTION_CLOSED;
  }

  io_state_ = STATE_ATTEMPT_SESSION;
  return OK;
}

int QuicSessionPool::ProxyJob::DoAttemptSession() {
  IPEndPoint local_address;
  int rv = proxy_session_->GetSelfAddress(&local_address);
  if (rv != 0) {
    return rv;
  }

  IPEndPoint peer_address;
  rv = proxy_session_->GetPeerAddress(&peer_address);
  if (rv != 0) {
    return rv;
  }

  session_attempt_ = std::make_unique<QuicSessionAttempt>(
      this, std::move(local_address), std::move(peer_address),
      target_quic_version_, cert_verify_flags_, std::move(proxy_stream_),
      http_user_agent_settings_);

  return session_attempt_->Start(
      base::BindOnce(&ProxyJob::OnSessionAttemptComplete, GetWeakPtr()));
}

}  // namespace net
