// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
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
                                   ? HttpStreamPool::kAllProtocols
                                   : NextProtoSet({expected_protocol});

  allowed_alpns = Intersection(allowed_alpns, delegate->allowed_alpns());

  // Remove QUIC from the list if QUIC cannot be used for some reason.
  //
  // Note that this does not check RequiresHTTP11(), as despite its name, it
  // only means H2 is not allowed.
  //
  // Inlining this logic instead of calling HttpStreamPool::CanUseQuic() is an
  // optimization, to avoid the extra ShouldForceQuic() call.
  if (!group->http_network_session()->IsQuicEnabled() ||
      !delegate->enable_alternative_services() ||
      !GURL::SchemeIsCryptographic(
          group->stream_key().destination().scheme()) ||
      group->pool()->IsQuicBroken(
          group->stream_key().destination(),
          group->stream_key().network_anonymization_key())) {
    allowed_alpns.RemoveAll(HttpStreamPool::kQuicBasedProtocols);
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
                         JobType type,
                         Group* group,
                         quic::ParsedQuicVersion quic_version,
                         NextProto expected_protocol,
                         const NetLogWithSource& request_net_log,
                         size_t num_streams)
    : delegate_(delegate),
      type_(type),
      attempt_manager_(group->GetAttemptManagerForJob(this)),
      quic_version_(CalculateQuicVersion(quic_version, group)),
      allowed_alpns_(
          CalculateAllowedAlpns(delegate_, group, expected_protocol)),
      request_net_log_(request_net_log),
      job_net_log_(
          NetLogWithSource::Make(request_net_log.net_log(),
                                 NetLogSourceType::HTTP_STREAM_POOL_JOB)),
      num_streams_(num_streams),
      create_time_(base::TimeTicks::Now()) {
  CHECK(attempt_manager_);
  job_net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_POOL_JOB_ALIVE, [&] {
    base::Value::Dict dict;
    dict.Set("stream_key", group->stream_key().ToValue());
    dict.Set("quic_version", quic::ParsedQuicVersionToString(quic_version));
    base::Value::List allowed_alpn_list;
    for (const auto alpn : allowed_alpns_) {
      allowed_alpn_list.Append(NextProtoToString(alpn));
    }
    dict.Set("allowed_alpns", std::move(allowed_alpn_list));
    dict.Set("type", static_cast<int>(type_));
    dict.Set("num_streams", static_cast<int>(num_streams_));
    delegate_->net_log().source().AddToEventParameters(dict);
    return dict;
  });
  delegate_->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_JOB_CONTROLLER_JOB_BOUND,
      job_net_log_.source());
}

HttpStreamPool::Job::~Job() {
  if (attempt_manager_) {
    attempt_manager_->OnJobCancelled(this);
    OnDone(std::nullopt);
  }
}

void HttpStreamPool::Job::Start() {
  CHECK(attempt_manager_);
  CHECK(!attempt_manager_->is_shutting_down());

  switch (type_) {
    case JobType::kRequest:
      attempt_manager_->RequestStream(this);
      break;
    case JobType::kPreconnect:
    case JobType::kAltSvcQuicPreconnect:
      attempt_manager_->Preconnect(this);
      break;
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

void HttpStreamPool::Job::OnStreamReady(
    std::unique_ptr<HttpStream> stream,
    NextProto negotiated_protocol,
    std::optional<SessionSource> session_source) {
  CHECK(delegate_);
  CHECK(!result_.has_value());
  CHECK(!negotiated_protocol_);
  CHECK(attempt_manager_);

  // `allowed_alpns_` never includes kProtoUnknown, which when making a request,
  // can mean "any protocol", but when receiving a response means "not H2 and
  // not H3", thus implying H1 (or some other protocol), so when comparing the
  // protocol of the received stream, replace kProtoUnknown with kProtoHTTP11.
  NextProto logical_protocol = (negotiated_protocol != NextProto::kProtoUnknown
                                    ? negotiated_protocol
                                    : NextProto::kProtoHTTP11);
  if (!allowed_alpns_.Has(logical_protocol)) {
    OnStreamFailed(ERR_ALPN_NEGOTIATION_FAILED, NetErrorDetails(),
                   ResolveErrorInfo());
    return;
  }

  negotiated_protocol_ = negotiated_protocol;
  attempt_manager_->group()
      ->http_network_session()
      ->proxy_resolution_service()
      ->ReportSuccess(delegate_->proxy_info());
  OnDone(OK);
  delegate_->OnStreamReady(this, std::move(stream), negotiated_protocol,
                           session_source);
}

void HttpStreamPool::Job::OnStreamFailed(
    int status,
    const NetErrorDetails& net_error_details,
    ResolveErrorInfo resolve_error_info) {
  CHECK(delegate_);
  CHECK(!result_.has_value());
  OnDone(status);
  delegate_->OnStreamFailed(this, status, net_error_details,
                            resolve_error_info);
}

void HttpStreamPool::Job::OnCertificateError(int status,
                                             const SSLInfo& ssl_info) {
  CHECK(delegate_);
  CHECK(!result_.has_value());
  OnDone(status);
  delegate_->OnCertificateError(this, status, ssl_info);
}

void HttpStreamPool::Job::OnNeedsClientAuth(SSLCertRequestInfo* cert_info) {
  CHECK(delegate_);
  CHECK(!result_.has_value());
  OnDone(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  delegate_->OnNeedsClientAuth(this, cert_info);
}

void HttpStreamPool::Job::OnPreconnectComplete(int status) {
  CHECK(delegate_);
  CHECK(!result_.has_value());
  OnDone(status);
  delegate_->OnPreconnectComplete(this, status);
}

void HttpStreamPool::Job::CallOnPreconnectCompleteLater(int status) {
  // Currently the notification is only used for testing so using IDLE priority.
  TaskRunner(IDLE)->PostTask(
      FROM_HERE, base::BindOnce(&Job::OnPreconnectComplete,
                                weak_ptr_factory_.GetWeakPtr(), status));
}

void HttpStreamPool::Job::OnDone(std::optional<int> result) {
  CHECK(attempt_manager_);
  attempt_manager_ = nullptr;

  result_ = result;

  // Record histograms only when `this` has a result. If `this` doesn't have a
  // result that means JobController destroyed `this` since another job
  // completed.
  if (result_.has_value()) {
    constexpr std::string_view kCompleteTimeHistogramName =
        "Net.HttpStreamPool.JobCompleteTime4.";
    base::TimeDelta complete_time = base::TimeTicks::Now() - create_time_;
    if (*result_ == OK) {
      const std::string_view protocol =
          NegotiatedProtocolToHistogramSuffixCoalesced(
              negotiated_protocol_.value_or(NextProto::kProtoUnknown));
      base::UmaHistogramLongTimes100(
          base::StrCat({kCompleteTimeHistogramName, protocol}), complete_time);
    } else {
      base::UmaHistogramLongTimes100(
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
}

}  // namespace net
