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
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_attempt_manager.h"
#include "net/http/http_stream_pool_group.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_http_stream.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

namespace {

NextProtoSet CalculateAllowedAlpns(HttpStreamPool::Job::Delegate* delegate,
                                   HttpStreamPool::Group* group,
                                   NextProto expected_protocol) {
  if (group->force_quic()) {
    return NextProtoSet({NextProto::kProtoQUIC});
  }

  NextProtoSet allowed_alpns = expected_protocol == NextProto::kProtoUnknown
                                   ? NextProtoSet::All()
                                   : NextProtoSet({expected_protocol});

  if (!delegate->is_http1_allowed()) {
    allowed_alpns.RemoveAll(HttpStreamPool::kHttp11Protocols);
  }

  if (!group->pool()->CanUseQuic(
          group->stream_key().destination(),
          group->stream_key().network_anonymization_key(),
          delegate->enable_ip_based_pooling(),
          delegate->enable_alternative_services())) {
    allowed_alpns.Remove(NextProto::kProtoQUIC);
  }

  CHECK(!allowed_alpns.empty());
  return allowed_alpns;
}

// If the destination is forced to use QUIC and the QUIC version is unknown,
// try the preferred QUIC version that is supported by default.
quic::ParsedQuicVersion CalculateQuicVersion(
    quic::ParsedQuicVersion original_quic_version,
    HttpStreamPool::Group* group) {
  return !original_quic_version.IsKnown() && group->force_quic()
             ? group->http_network_session()
                   ->context()
                   .quic_context->params()
                   ->supported_versions[0]
             : original_quic_version;
}

}  // namespace

HttpStreamPool::Job::Job(Delegate* delegate,
                         Group* group,
                         quic::ParsedQuicVersion quic_version,
                         NextProto expected_protocol,
                         const NetLogWithSource& request_net_log,
                         size_t num_streams)
    : delegate_(delegate),
      attempt_manager_(group->EnsureAttemptManager()),
      quic_version_(CalculateQuicVersion(quic_version, group)),
      allowed_alpns_(
          CalculateAllowedAlpns(delegate_, group, expected_protocol)),
      request_net_log_(request_net_log),
      job_net_log_(
          NetLogWithSource::Make(request_net_log.net_log(),
                                 NetLogSourceType::HTTP_STREAM_POOL_JOB)),
      num_streams_(num_streams),
      create_time_(base::TimeTicks::Now()) {
  CHECK(delegate_->is_http1_allowed() ||
        expected_protocol != NextProto::kProtoHTTP11);
  job_net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_POOL_JOB_ALIVE, [&] {
    base::Value::Dict dict;
    dict.Set("stream_key", group->stream_key().ToValue());
    dict.Set("quic_version", quic::ParsedQuicVersionToString(quic_version));
    base::Value::List allowed_alpn_list;
    for (const auto alpn : allowed_alpns_) {
      allowed_alpn_list.Append(NextProtoToString(alpn));
    }
    dict.Set("allowed_alpns", std::move(allowed_alpn_list));
    dict.Set("num_streams", static_cast<int>(num_streams_));
    delegate_->net_log().source().AddToEventParameters(dict);
    return dict;
  });
  delegate_->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_JOB_CONTROLLER_JOB_BOUND,
      job_net_log_.source());
}

HttpStreamPool::Job::~Job() {
  CHECK(attempt_manager_);

  // Record histograms only when `this` has a result. If `this` doesn't have a
  // result that means JobController destroyed `this` since another job
  // completed.
  if (result_.has_value()) {
    constexpr std::string_view kCompleteTimeHistogramName =
        "Net.HttpStreamPool.JobCompleteTime2.";
    base::TimeDelta complete_time = base::TimeTicks::Now() - create_time_;
    if (*result_ == OK) {
      const std::string_view protocol = NegotiatedProtocolToHistogramSuffix(
          negotiated_protocol_.value_or(NextProto::kProtoUnknown));
      base::UmaHistogramTimes(
          base::StrCat({kCompleteTimeHistogramName, protocol}), complete_time);
    } else {
      base::UmaHistogramTimes(
          base::StrCat({kCompleteTimeHistogramName, "Failure"}), complete_time);
      base::UmaHistogramSparse("Net.HttpStreamPool.JobErrorCode", -*result_);
    }
  }

  job_net_log_.EndEvent(NetLogEventType::HTTP_STREAM_POOL_JOB_ALIVE, [&] {
    base::Value::Dict dict;
    if (result_.has_value()) {
      // Use "net_error" for the result as the NetLog viewer converts the value
      // to a human-readable string.
      dict.Set("net_error", *result_);
    }
    if (negotiated_protocol_.has_value()) {
      dict.Set("negotiated_protocol", NextProtoToString(*negotiated_protocol_));
    }
    return dict;
  });

  // `attempt_manager_` may be deleted after this call.
  attempt_manager_.ExtractAsDangling()->OnJobComplete(this);
}

void HttpStreamPool::Job::Start() {
  CHECK(attempt_manager_);
  CHECK(!attempt_manager_->is_shutting_down());

  if (IsPreconnect()) {
    attempt_manager_->Preconnect(this);
  } else {
    attempt_manager_->RequestStream(this);
  }
}

LoadState HttpStreamPool::Job::GetLoadState() const {
  if (!attempt_manager_) {
    return LOAD_STATE_IDLE;
  }
  return attempt_manager_->GetLoadState();
}

void HttpStreamPool::Job::SetPriority(RequestPriority priority) {
  if (attempt_manager_) {
    attempt_manager_->SetJobPriority(this, priority);
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
  CHECK(!negotiated_protocol_);
  CHECK(attempt_manager_);

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
  negotiated_protocol_ = negotiated_protocol;
  attempt_manager_->group()
      ->http_network_session()
      ->proxy_resolution_service()
      ->ReportSuccess(delegate_->proxy_info());
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

void HttpStreamPool::Job::OnPreconnectComplete(int status) {
  CHECK(delegate_);
  CHECK(!result_.has_value());
  result_ = status;
  delegate_->OnPreconnectComplete(this, status);
}

void HttpStreamPool::Job::CallOnPreconnectCompleteLater(int status) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Job::OnPreconnectComplete,
                                weak_ptr_factory_.GetWeakPtr(), status));
}

}  // namespace net
