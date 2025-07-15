// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_quic_attempt.h"

#include <memory>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/values.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_attempt_manager.h"
#include "net/http/http_stream_pool_group.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_attempt_manager.h"
#include "net/quic/quic_session_attempt_request.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_session_pool.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

HttpStreamPool::QuicAttempt::QuicAttempt(AttemptManager* manager,
                                         QuicEndpoint quic_endpoint)
    : manager_(manager),
      quic_endpoint_(std::move(quic_endpoint)),
      start_time_(base::TimeTicks::Now()),
      net_log_(NetLogWithSource::Make(
          manager->net_log().net_log(),
          NetLogSourceType::HTTP_STREAM_POOL_QUIC_ATTEMPT)),
      track_(base::trace_event::GetNextGlobalTraceId()),
      flow_(perfetto::Flow::ProcessScoped(
          base::trace_event::GetNextGlobalTraceId())) {
  CHECK(manager_);
  TRACE_EVENT_INSTANT("net.stream", "QuicAttemptStart", manager_->track(),
                      flow_);
  TRACE_EVENT_BEGIN("net.stream", "QuicAttempt::QuicAttempt", track_, flow_,
                    "ip_endpoint", quic_endpoint_.ip_endpoint.ToString());

  net_log_.BeginEvent(
      NetLogEventType::HTTP_STREAM_POOL_QUIC_ATTEMPT_ALIVE, [&] {
        base::Value::Dict dict;
        dict.Set("quic_version",
                 quic::ParsedQuicVersionToString(quic_endpoint_.quic_version));
        dict.Set("ip_endpoint", quic_endpoint_.ip_endpoint.ToString());
        dict.Set("metadata", quic_endpoint_.metadata.ToValue());
        manager_->net_log().source().AddToEventParameters(dict);
        return dict;
      });
  manager_->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_QUIC_ATTEMPT_BOUND,
      net_log_.source());

  request_ = quic_session_pool()->session_attempt_manager()->CreateRequest(
      quic_session_alias_key());
}

HttpStreamPool::QuicAttempt::~QuicAttempt() {
  net_log_.EndEventWithNetErrorCode(
      NetLogEventType::HTTP_STREAM_POOL_QUIC_ATTEMPT_ALIVE,
      result_.value_or(ERR_ABORTED));
  TRACE_EVENT_END("net.stream", track_, "result",
                  result_.value_or(ERR_ABORTED));
  TRACE_EVENT_INSTANT("net.stream", "QuicAttemptEnd", manager_->track(), flow_);
}

void HttpStreamPool::QuicAttempt::Start() {
  if (GetTcpBasedAttemptDelayBehavior() ==
      TcpBasedAttemptDelayBehavior::kStartTimerOnFirstQuicAttempt) {
    manager_->MaybeRunTcpBasedAttemptDelayTimer();
  }

  SSLConfig ssl_config;
  ssl_config.disable_cert_verification_network_fetches =
      stream_key().disable_cert_network_fetches();
  int cert_verify_flags = ssl_config.GetCertVerifyFlags();

  base::TimeTicks dns_resolution_start_time =
      manager_->dns_resolution_start_time();
  // The DNS resolution end time could be null when the resolution is still
  // ongoing. In that case, use the current time to make sure the connect
  // start time is already greater than the DNS resolution end time.
  base::TimeTicks dns_resolution_end_time =
      manager_->dns_resolution_end_time().is_null()
          ? base::TimeTicks::Now()
          : manager_->dns_resolution_end_time();

  std::set<std::string> dns_aliases =
      manager_->service_endpoint_request()->GetDnsAliasResults();
  int rv = request_->RequestSession(
      quic_endpoint_, cert_verify_flags, dns_resolution_start_time,
      dns_resolution_end_time, /*use_dns_aliases=*/true, std::move(dns_aliases),
      manager_->CalculateMultiplexedSessionCreationInitiator(),
      /*connection_management_config=*/std::nullopt, net_log_,
      base::BindOnce(&QuicAttempt::OnSessionAttemptComplete,
                     weak_ptr_factory_.GetWeakPtr()));

  if (rv == ERR_IO_PENDING) {
    slow_timer_.Start(FROM_HERE, HttpStreamPool::GetConnectionAttemptDelay(),
                      base::BindOnce(&QuicAttempt::OnSessionAttemptSlow,
                                     base::Unretained(this)));
  } else {
    OnSessionAttemptComplete(rv);
  }
}

base::Value::Dict HttpStreamPool::QuicAttempt::GetInfoAsValue() const {
  base::Value::Dict dict;
  dict.Set("quic_version",
           quic::ParsedQuicVersionToString(quic_endpoint_.quic_version));
  dict.Set("ip_endpoint", quic_endpoint_.ip_endpoint.ToString());
  base::TimeDelta elapsed = base::TimeTicks::Now() - start_time_;
  dict.Set("elapsed_ms", static_cast<int>(elapsed.InMilliseconds()));
  if (result_.has_value()) {
    dict.Set("result", *result_);
  }
  return dict;
}

const HttpStreamKey& HttpStreamPool::QuicAttempt::stream_key() const {
  return manager_->group()->stream_key();
}

const QuicSessionAliasKey& HttpStreamPool::QuicAttempt::quic_session_alias_key()
    const {
  return manager_->group()->quic_session_alias_key();
}

QuicSessionPool* HttpStreamPool::QuicAttempt::quic_session_pool() {
  return manager_->group()->http_network_session()->quic_session_pool();
}

void HttpStreamPool::QuicAttempt::OnSessionAttemptSlow() {
  CHECK(!is_slow_);
  is_slow_ = true;
  manager_->OnQuicAttemptSlow();
}

void HttpStreamPool::QuicAttempt::OnSessionAttemptComplete(int rv) {
  slow_timer_.Stop();
  if (rv == OK) {
    if (!manager_->CanUseExistingQuicSession()) {
      // QUIC session is closed or marked broken before stream can be created.
      rv = ERR_CONNECTION_CLOSED;
    }
  }

  if (rv == OK &&
      !quic_session_pool()->has_quic_ever_worked_on_current_network()) {
    quic_session_pool()->set_has_quic_ever_worked_on_current_network(true);
  }

  result_ = rv;
  QuicAttemptOutcome outcome(rv);
  if (request_) {
    outcome.session = request_->session();
    outcome.error_details = request_->error_details();
  }
  request_.reset();
  manager_->OnQuicAttemptComplete(std::move(outcome));
  // `this` is deleted.
}

}  // namespace net
