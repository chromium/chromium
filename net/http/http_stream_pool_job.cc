// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/port_util.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_attempt_manager.h"
#include "net/http/http_stream_pool_group.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

namespace {

NextProtoSet CalculateAllowedAlpns(NextProto expected_protocol,
                                   bool is_http1_allowed) {
  NextProtoSet allowed_alpns = expected_protocol == NextProto::kProtoUnknown
                                   ? NextProtoSet::All()
                                   : NextProtoSet({expected_protocol});
  if (!is_http1_allowed) {
    static constexpr NextProtoSet kHttp11Protocols = {NextProto::kProtoUnknown,
                                                      NextProto::kProtoHTTP11};
    allowed_alpns.RemoveAll(kHttp11Protocols);
  }
  return allowed_alpns;
}

}  // namespace

HttpStreamPool::Job::Job(Delegate* delegate,
                         Group* group,
                         quic::ParsedQuicVersion quic_version,
                         NextProto expected_protocol,
                         const NetLogWithSource& request_net_log)
    : delegate_(delegate),
      group_(group),
      quic_version_(quic_version),
      allowed_alpns_(CalculateAllowedAlpns(expected_protocol,
                                           delegate_->is_http1_allowed())),
      request_net_log_(request_net_log),
      job_net_log_(
          NetLogWithSource::Make(request_net_log.net_log(),
                                 NetLogSourceType::HTTP_STREAM_POOL_JOB)),
      create_time_(base::TimeTicks::Now()) {
  CHECK(delegate_->is_http1_allowed() ||
        expected_protocol != NextProto::kProtoHTTP11);
  job_net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_POOL_JOB_ALIVE, [&] {
    base::Value::Dict dict;
    dict.Set("stream_key", group_->stream_key().ToValue());
    dict.Set("quic_version", quic::ParsedQuicVersionToString(quic_version));
    base::Value::List allowed_alpn_list;
    for (const auto alpn : allowed_alpns_) {
      allowed_alpn_list.Append(NextProtoToString(alpn));
    }
    dict.Set("allowed_alpns", std::move(allowed_alpn_list));
    delegate_->net_log().source().AddToEventParameters(dict);
    return dict;
  });
  delegate_->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_JOB_CONTROLLER_JOB_BOUND,
      job_net_log_.source());
}

HttpStreamPool::Job::~Job() {
  CHECK(group_);

  // Record histograms only when `this` has a result. If `this` doesn't have a
  // result that means JobController destroyed `this` since another job
  // completed.
  if (result_.has_value()) {
    const std::string_view suffix = *result_ == OK ? "Success" : "Failure";
    base::UmaHistogramTimes(
        base::StrCat({"Net.HttpStreamPool.JobCompleteTime.", suffix}),
        base::TimeTicks::Now() - create_time_);
    base::UmaHistogramTimes(
        base::StrCat({"Net.HttpStreamPool.JobCreateToResumeTime.", suffix}),
        CreateToResumeTime());

    if (*result_ != OK) {
      base::UmaHistogramSparse("Net.HttpStreamPool.JobErrorCode", -*result_);
    }
  }

  job_net_log_.EndEvent(NetLogEventType::HTTP_STREAM_POOL_JOB_ALIVE);

  // `group_` may be deleted after this call.
  group_.ExtractAsDangling()->OnJobComplete(this);
}

void HttpStreamPool::Job::Start() {
  CHECK(group_);

  if (!group_->CanStartJob(this)) {
    return;
  }

  StartInternal();
}

void HttpStreamPool::Job::Resume() {
  resume_time_ = base::TimeTicks::Now();
  StartInternal();
}

LoadState HttpStreamPool::Job::GetLoadState() const {
  if (!attempt_manager()) {
    return LOAD_STATE_IDLE;
  }
  return attempt_manager()->GetLoadState();
}

void HttpStreamPool::Job::SetPriority(RequestPriority priority) {
  if (attempt_manager()) {
    attempt_manager()->SetJobPriority(this, priority);
  }
}

void HttpStreamPool::Job::AddConnectionAttempts(
    const ConnectionAttempts& attempts) {
  for (const auto& attempt : attempts) {
    connection_attempts_.emplace_back(attempt);
  }
}

void HttpStreamPool::Job::OnStreamReady(std::unique_ptr<HttpStream> stream,
                                        NextProto negotiated_protocol) {
  CHECK(delegate_);
  CHECK(!result_.has_value());

  int result = OK;
  if (!allowed_alpns_.Has(negotiated_protocol)) {
    const bool is_h2_or_h3_required = !delegate_->is_http1_allowed();
    const bool is_h2_or_h3 = negotiated_protocol == NextProto::kProtoHTTP2 ||
                             negotiated_protocol == NextProto::kProtoQUIC;
    if (is_h2_or_h3_required && !is_h2_or_h3) {
      result = ERR_H2_OR_QUIC_REQUIRED;
    } else {
      result = ERR_ALPN_NEGOTIATION_FAILED;
    }
  }

  if (result != OK) {
    OnStreamFailed(result, NetErrorDetails(), ResolveErrorInfo());
    return;
  }

  result_ = OK;
  group_->http_network_session()->proxy_resolution_service()->ReportSuccess(
      delegate_->proxy_info());
  delegate_->OnStreamReady(this, std::move(stream), negotiated_protocol);
}

void HttpStreamPool::Job::OnStreamFailed(
    int status,
    const NetErrorDetails& net_error_details,
    ResolveErrorInfo resolve_error_info) {
  CHECK(delegate_);
  CHECK(!result_.has_value());
  result_ = status;
  delegate_->OnStreamFailed(this, status, net_error_details,
                            resolve_error_info);
}

void HttpStreamPool::Job::OnCertificateError(int status,
                                             const SSLInfo& ssl_info) {
  CHECK(delegate_);
  CHECK(!result_.has_value());
  result_ = status;
  delegate_->OnCertificateError(this, status, ssl_info);
}

void HttpStreamPool::Job::OnNeedsClientAuth(SSLCertRequestInfo* cert_info) {
  CHECK(delegate_);
  CHECK(!result_.has_value());
  result_ = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
  delegate_->OnNeedsClientAuth(this, cert_info);
}

base::TimeDelta HttpStreamPool::Job::CreateToResumeTime() const {
  if (resume_time_.is_null()) {
    return base::TimeDelta();
  }
  return resume_time_ - create_time_;
}

HttpStreamPool::AttemptManager* HttpStreamPool::Job::attempt_manager() const {
  CHECK(group_);
  return group_->attempt_manager();
}

void HttpStreamPool::Job::StartInternal() {
  CHECK(attempt_manager());
  CHECK(!attempt_manager()->is_failing());

  const url::SchemeHostPort& destination = group_->stream_key().destination();
  if (!IsPortAllowedForScheme(destination.port(), destination.scheme())) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&Job::OnStreamFailed, weak_ptr_factory_.GetWeakPtr(),
                       ERR_UNSAFE_PORT, NetErrorDetails(), ResolveErrorInfo()));
    return;
  }

  attempt_manager()->StartJob(this, priority(), delegate_->allowed_bad_certs(),
                              quic_version_, request_net_log_,
                              delegate_->net_log());
}

}  // namespace net
