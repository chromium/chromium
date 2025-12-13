// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_job_controller.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_internal_info.h"
#include "net/base/net_errors.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_string_util.h"
#include "net/base/session_usage.h"
#include "net/base/task/task_runner.h"
#include "net/base/url_util.h"
#include "net/http/alternate_protocol_usage.h"
#include "net/http/alternative_service.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_request_info.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_session_key.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

namespace {

// Returns parameters associated with the proxy resolution.
base::Value::Dict NetLogHttpStreamJobProxyChainResolved(
    const ProxyChain& proxy_chain) {
  base::Value::Dict dict;

  dict.Set("proxy_chain",
           proxy_chain.IsValid() ? proxy_chain.ToDebugString() : std::string());
  return dict;
}

GURL CreateAltSvcUrl(const GURL& origin_url,
                     const HostPortPair& alternative_destination) {
  DCHECK(origin_url.is_valid());
  DCHECK(origin_url.IsStandard());

  GURL::Replacements replacements;
  std::string port_str = base::NumberToString(alternative_destination.port());
  replacements.SetPortStr(port_str);
  replacements.SetHostStr(alternative_destination.host());

  return origin_url.ReplaceComponents(replacements);
}

void ConvertWsToHttp(url::SchemeHostPort& input) {
  if (base::EqualsCaseInsensitiveASCII(input.scheme(), url::kHttpScheme) ||
      base::EqualsCaseInsensitiveASCII(input.scheme(), url::kHttpsScheme)) {
    return;
  }

  if (base::EqualsCaseInsensitiveASCII(input.scheme(), url::kWsScheme)) {
    input = url::SchemeHostPort(url::kHttpScheme, input.host(), input.port());
    return;
  }

  DCHECK(base::EqualsCaseInsensitiveASCII(input.scheme(), url::kWssScheme));
  input = url::SchemeHostPort(url::kHttpsScheme, input.host(), input.port());
}

void HistogramProxyUsed(const ProxyInfo& proxy_info, bool success) {
  const ProxyServer::Scheme max_scheme = ProxyServer::Scheme::SCHEME_QUIC;
  ProxyServer::Scheme proxy_scheme = ProxyServer::Scheme::SCHEME_INVALID;
  if (!proxy_info.is_empty() && !proxy_info.is_direct()) {
    if (proxy_info.proxy_chain().is_multi_proxy()) {
      // TODO(crbug.com/40284947): Update this histogram to have a new
      // bucket for multi-chain proxies. Until then, don't influence the
      // existing metric counts which have historically been only for single-hop
      // proxies.
      return;
    }
    proxy_scheme = proxy_info.proxy_chain().is_direct()
                       ? static_cast<ProxyServer::Scheme>(1)
                       : proxy_info.proxy_chain().First().scheme();
  }
  if (success) {
    UMA_HISTOGRAM_ENUMERATION("Net.HttpJob.ProxyTypeSuccess", proxy_scheme,
                              max_scheme);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Net.HttpJob.ProxyTypeFailed", proxy_scheme,
                              max_scheme);
  }
}

// Generate a AlternativeService for DNS alt job. Note: Chrome does not yet
// support different port DNS alpn.
AlternativeService GetAlternativeServiceForDnsJob(const GURL& url) {
  return AlternativeService(NextProto::kProtoQUIC, HostPortPair::FromURL(url));
}

base::Value::Dict NetLogAltSvcParams(const AlternativeServiceInfo* alt_svc_info,
                                     bool is_broken) {
  base::Value::Dict dict;
  dict.Set("alt_svc", alt_svc_info->ToString());
  dict.Set("is_broken", is_broken);
  return dict;
}

const scoped_refptr<base::SingleThreadTaskRunner>& TaskRunner(
    net::RequestPriority priority) {
  if (features::kNetTaskSchedulerHttpStreamFactoryJobController.Get()) {
    return net::GetTaskRunner(priority);
  }
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

}  // namespace

// The maximum time to wait for the alternate job to complete before resuming
// the main job.
const int kMaxDelayTimeForMainJobSecs = 3;

HttpStreamFactory::JobController::JobController(
    HttpStreamFactory* factory,
    HttpStreamRequest::Delegate* delegate,
    HttpNetworkSession* session,
    JobFactory* job_factory,
    const HttpRequestInfo& http_request_info,
    bool is_preconnect,
    bool is_websocket,
    bool enable_ip_based_pooling_for_h2,
    bool enable_alternative_services,
    bool delay_main_job_with_available_spdy_session,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs)
    : factory_(factory),
      session_(session),
      job_factory_(job_factory),
      delegate_(delegate),
      is_preconnect_(is_preconnect),
      is_websocket_(is_websocket),
      enable_ip_based_pooling_for_h2_(enable_ip_based_pooling_for_h2),
      enable_alternative_services_(enable_alternative_services),
      delay_main_job_with_available_spdy_session_(
          delay_main_job_with_available_spdy_session),
      management_config_(http_request_info.connection_management_config),
      request_info_(http_request_info),
      allowed_bad_certs_(allowed_bad_certs),
      net_log_(NetLogWithSource::Make(
          session->net_log(),
          NetLogSourceType::HTTP_STREAM_JOB_CONTROLLER)) {
  DCHECK(factory_);
  DCHECK(session_);
  DCHECK(job_factory_);
  DCHECK(request_info_.url.SchemeIsHTTPOrHTTPS() ||
         request_info_.url.SchemeIsWSOrWSS());

  net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_JOB_CONTROLLER, [&] {
    base::Value::Dict dict;
    dict.Set("url", request_info_.url.possibly_invalid_spec());
    dict.Set("is_preconnect", is_preconnect_);
    dict.Set("privacy_mode",
             PrivacyModeToDebugString(request_info_.privacy_mode));
    base::Value::List allowed_bad_certs_list;
    for (const auto& cert_and_status : allowed_bad_certs_) {
      allowed_bad_certs_list.Append(
          cert_and_status.cert->subject().GetDisplayName());
    }
    dict.Set("allowed_bad_certs", std::move(allowed_bad_certs_list));
    return dict;
  });
}

HttpStreamFactory::JobController::~JobController() {
  bound_job_ = nullptr;
  main_job_.reset();
  alternative_job_.reset();
  dns_alpn_h3_job_.reset();
  if (proxy_resolve_request_) {
    DCHECK_EQ(STATE_RESOLVE_PROXY_COMPLETE, next_state_);
    proxy_resolve_request_.reset();
  }
  net_log_.EndEvent(NetLogEventType::HTTP_STREAM_JOB_CONTROLLER);
}

std::unique_ptr<HttpStreamRequest> HttpStreamFactory::JobController::Start(
    HttpStreamRequest::Delegate* delegate,
    WebSocketHandshakeStreamBase::CreateHelper*
        websocket_handshake_stream_create_helper,
    const NetLogWithSource& source_net_log,
    HttpStreamRequest::StreamType stream_type,
    RequestPriority priority) {
  DCHECK(!request_);

  stream_type_ = stream_type;
  priority_ = priority;

  auto request = std::make_unique<HttpStreamRequest>(
      this, websocket_handshake_stream_create_helper, source_net_log,
      stream_type);
  // Keep a raw pointer but release ownership of HttpStreamRequest instance.
  request_ = request.get();

  // Associates |net_log_| with |source_net_log|.
  source_net_log.AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_JOB_CONTROLLER_BOUND, net_log_.source());
  net_log_.AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_JOB_CONTROLLER_BOUND,
      source_net_log.source());

  RunLoop(OK);
  // `this` may be deleted at this point.

  return request;
}

void HttpStreamFactory::JobController::Preconnect(int num_streams,
                                                  base::OnceClosure callback) {
  DCHECK(!main_job_);
  DCHECK(!alternative_job_);
  DCHECK(is_preconnect_);

  stream_type_ = HttpStreamRequest::HTTP_STREAM;
  num_streams_ = num_streams;
  preconnect_callback_ = std::move(callback);

  RunLoop(OK);
  // `this` may be deleted at this point.
}

LoadState HttpStreamFactory::JobController::GetLoadState() const {
  DCHECK(request_);
  if (next_state_ == STATE_RESOLVE_PROXY_COMPLETE) {
    return proxy_resolve_request_->GetLoadState();
  }
  if (bound_job_) {
    return bound_job_->GetLoadState();
  }
  if (main_job_) {
    return main_job_->GetLoadState();
  }
  if (alternative_job_) {
    return alternative_job_->GetLoadState();
  }
  if (dns_alpn_h3_job_) {
    return dns_alpn_h3_job_->GetLoadState();
  }

  // When proxy resolution fails, there is no job created and
  // NotifyRequestFailed() is executed one message loop iteration later.
  return LOAD_STATE_IDLE;
}

void HttpStreamFactory::JobController::OnRequestComplete() {
  DCHECK(request_);
  CHECK(!switched_to_http_stream_pool_);

  request_ = nullptr;
  // This is called when the delegate is destroying its HttpStreamRequest, so
  // it's no longer safe to call into it after this point.
  delegate_ = nullptr;

  if (!job_bound_) {
    alternative_job_.reset();
    main_job_.reset();
    dns_alpn_h3_job_.reset();
  } else {
    if (bound_job_->job_type() == MAIN) {
      bound_job_ = nullptr;
      main_job_.reset();
    } else if (bound_job_->job_type() == ALTERNATIVE) {
      bound_job_ = nullptr;
      alternative_job_.reset();
    } else {
      DCHECK(bound_job_->job_type() == DNS_ALPN_H3);
      bound_job_ = nullptr;
      dns_alpn_h3_job_.reset();
    }
  }
  MaybeNotifyFactoryOfCompletion();
}

int HttpStreamFactory::JobController::RestartTunnelWithProxyAuth() {
  DCHECK(bound_job_);
  return bound_job_->RestartTunnelWithProxyAuth();
}

void HttpStreamFactory::JobController::SetPriority(RequestPriority priority) {
  if (main_job_) {
    main_job_->SetPriority(priority);
  }
  if (alternative_job_) {
    alternative_job_->SetPriority(priority);
  }
  if (dns_alpn_h3_job_) {
    dns_alpn_h3_job_->SetPriority(priority);
  }
  if (preconnect_backup_job_) {
    preconnect_backup_job_->SetPriority(priority);
  }
}

void HttpStreamFactory::JobController::OnStreamReady(Job* job) {
  DCHECK(job);

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }

  NotifyOnStreamCreationAttempted(std::nullopt);
  std::unique_ptr<HttpStream> stream = job->ReleaseStream();
  DCHECK(stream);

  MarkRequestComplete(job);

  if (!request_) {
    return;
  }
  DCHECK(!is_websocket_);
  DCHECK_EQ(HttpStreamRequest::HTTP_STREAM, request_->stream_type());
  OnJobSucceeded(job);

  // TODO(bnc): Remove when https://crbug.com/461981 is fixed.
  CHECK(request_);

  DCHECK(request_->completed());

  HistogramProxyUsed(job->proxy_info(), /*success=*/true);
  delegate_->OnStreamReady(job->proxy_info(), std::move(stream));
}

void HttpStreamFactory::JobController::OnBidirectionalStreamImplReady(
    Job* job,
    const ProxyInfo& used_proxy_info) {
  DCHECK(job);

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }

  MarkRequestComplete(job);

  if (!request_) {
    return;
  }
  std::unique_ptr<BidirectionalStreamImpl> stream =
      job->ReleaseBidirectionalStream();
  DCHECK(stream);
  DCHECK(!is_websocket_);
  DCHECK_EQ(HttpStreamRequest::BIDIRECTIONAL_STREAM, request_->stream_type());

  OnJobSucceeded(job);
  DCHECK(request_->completed());
  delegate_->OnBidirectionalStreamImplReady(used_proxy_info, std::move(stream));
}

void HttpStreamFactory::JobController::OnWebSocketHandshakeStreamReady(
    Job* job,
    const ProxyInfo& used_proxy_info,
    std::unique_ptr<WebSocketHandshakeStreamBase> stream) {
  DCHECK(job);
  MarkRequestComplete(job);

  if (!request_) {
    return;
  }
  DCHECK(is_websocket_);
  DCHECK_EQ(HttpStreamRequest::HTTP_STREAM, request_->stream_type());
  DCHECK(stream);

  OnJobSucceeded(job);
  DCHECK(request_->completed());
  delegate_->OnWebSocketHandshakeStreamReady(used_proxy_info,
                                             std::move(stream));
}

void HttpStreamFactory::JobController::OnQuicHostResolution(
    const url::SchemeHostPort& destination,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time) {
  if (!request_) {
    return;
  }
  if (destination != url::SchemeHostPort(request_info_.url)) {
    // Ignores different destination alternative job's DNS resolution time.
    return;
  }
  // QUIC jobs (ALTERNATIVE, DNS_ALPN_H3) are started before the non-QUIC (MAIN)
  // job. So we set the DNS resolution overrides to use the DNS timing of the
  // QUIC jobs.
  request_->SetDnsResolutionTimeOverrides(dns_resolution_start_time,
                                          dns_resolution_end_time);
}

void HttpStreamFactory::JobController::OnStreamFailed(Job* job, int status) {
  DCHECK_NE(OK, status);
  if (job->job_type() == MAIN) {
    DCHECK_EQ(main_job_.get(), job);
    main_job_net_error_ = status;
  } else if (job->job_type() == ALTERNATIVE) {
    DCHECK_EQ(alternative_job_.get(), job);
    DCHECK_NE(NextProto::kProtoUnknown, advertised_alt_svc_.info.protocol());
    alternative_job_net_error_ = status;
  } else {
    DCHECK_EQ(job->job_type(), DNS_ALPN_H3);
    DCHECK_EQ(dns_alpn_h3_job_.get(), job);
    dns_alpn_h3_job_net_error_ = status;
  }

  MaybeResumeMainJob(job, base::TimeDelta());

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }

  if (!request_) {
    return;
  }
  DCHECK_NE(OK, status);
  DCHECK(job);

  if (!bound_job_) {
    if (GetJobCount() >= 2) {
      // Hey, we've got other jobs! Maybe one of them will succeed, let's just
      // ignore this failure.
      if (job->job_type() == MAIN) {
        DCHECK_EQ(main_job_.get(), job);
        main_job_.reset();
      } else if (job->job_type() == ALTERNATIVE) {
        DCHECK_EQ(alternative_job_.get(), job);
        alternative_job_.reset();
      } else {
        DCHECK_EQ(job->job_type(), DNS_ALPN_H3);
        DCHECK_EQ(dns_alpn_h3_job_.get(), job);
        dns_alpn_h3_job_.reset();
      }
      return;
    } else {
      BindJob(job);
    }
  }

  NotifyOnStreamCreationAttempted(status);
  status = ReconsiderProxyAfterError(job, status);
  if (next_state_ == STATE_RESOLVE_PROXY_COMPLETE) {
    if (status == ERR_IO_PENDING) {
      return;
    }
    DCHECK_EQ(OK, status);
    RunLoop(status);
    // `this` may be deleted at this point.
    return;
  }

  HistogramProxyUsed(job->proxy_info(), /*success=*/false);
  delegate_->OnStreamFailed(status, *job->net_error_details(),
                            job->proxy_info(), job->resolve_error_info());
}

void HttpStreamFactory::JobController::OnFailedOnDefaultNetwork(Job* job) {
  if (job->job_type() == ALTERNATIVE) {
    DCHECK_EQ(alternative_job_.get(), job);
    alternative_job_failed_on_default_network_ = true;
  } else {
    DCHECK_EQ(job->job_type(), DNS_ALPN_H3);
    DCHECK_EQ(dns_alpn_h3_job_.get(), job);
    dns_alpn_h3_job_failed_on_default_network_ = true;
  }
}

void HttpStreamFactory::JobController::OnCertificateError(
    Job* job,
    int status,
    const SSLInfo& ssl_info) {
  MaybeResumeMainJob(job, base::TimeDelta());

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }

  if (!request_) {
    return;
  }
  DCHECK_NE(OK, status);
  if (!bound_job_) {
    BindJob(job);
  }

  delegate_->OnCertificateError(status, ssl_info);
}

void HttpStreamFactory::JobController::OnNeedsClientAuth(
    Job* job,
    SSLCertRequestInfo* cert_info) {
  MaybeResumeMainJob(job, base::TimeDelta());

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }
  if (!request_) {
    return;
  }
  if (!bound_job_) {
    BindJob(job);
  }

  delegate_->OnNeedsClientAuth(cert_info);
}

void HttpStreamFactory::JobController::OnNeedsProxyAuth(
    Job* job,
    const HttpResponseInfo& proxy_response,
    const ProxyInfo& used_proxy_info,
    HttpAuthController* auth_controller) {
  MaybeResumeMainJob(job, base::TimeDelta());

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }

  if (!request_) {
    return;
  }
  if (!bound_job_) {
    BindJob(job);
  }
  delegate_->OnNeedsProxyAuth(proxy_response, used_proxy_info, auth_controller);
}

void HttpStreamFactory::JobController::OnPreconnectsComplete(Job* job,
                                                             int result) {
  // Preconnects only run as `main_job_`, never `alternative_job_` or
  // `dns_alpn_h3_job_`.
  DCHECK_EQ(main_job_.get(), job);

  // If the job failed because there were no matching HTTPS records in DNS, run
  // the backup job. A TCP-based protocol may work instead.
  if (result == ERR_DNS_NO_MATCHING_SUPPORTED_ALPN && preconnect_backup_job_) {
    DCHECK_EQ(job->job_type(), PRECONNECT_DNS_ALPN_H3);
    main_job_ = std::move(preconnect_backup_job_);
    main_job_->Preconnect(num_streams_);
    return;
  }

  main_job_.reset();
  preconnect_backup_job_.reset();
  ResetErrorStatusForJobs();
  if (preconnect_callback_) {
    std::move(preconnect_callback_).Run();
  }
  MaybeNotifyFactoryOfCompletion();
}

void HttpStreamFactory::JobController::OnOrphanedJobComplete(const Job* job) {
  if (job->job_type() == MAIN) {
    DCHECK_EQ(main_job_.get(), job);
    main_job_.reset();
  } else if (job->job_type() == ALTERNATIVE) {
    DCHECK_EQ(alternative_job_.get(), job);
    alternative_job_.reset();
  } else {
    DCHECK_EQ(job->job_type(), DNS_ALPN_H3);
    DCHECK_EQ(dns_alpn_h3_job_.get(), job);
    dns_alpn_h3_job_.reset();
  }

  MaybeNotifyFactoryOfCompletion();
}

void HttpStreamFactory::JobController::AddConnectionAttemptsToRequest(
    Job* job,
    const ConnectionAttempts& attempts) {
  if (is_preconnect_ || IsJobOrphaned(job)) {
    return;
  }

  request_->AddConnectionAttempts(attempts);
}

void HttpStreamFactory::JobController::ResumeMainJobLater(
    const base::TimeDelta& delay) {
  net_log_.AddEventWithInt64Params(NetLogEventType::HTTP_STREAM_JOB_DELAYED,
                                   "delay", delay.InMilliseconds());
  resume_main_job_callback_.Reset(
      base::BindOnce(&HttpStreamFactory::JobController::ResumeMainJob,
                     ptr_factory_.GetWeakPtr()));
  TaskRunner(priority_)->PostDelayedTask(
      FROM_HERE, resume_main_job_callback_.callback(), delay);
}

void HttpStreamFactory::JobController::ResumeMainJob() {
  DCHECK(main_job_);

  if (main_job_is_resumed_) {
    return;
  }
  main_job_is_resumed_ = true;
  main_job_->net_log().AddEventWithInt64Params(
      NetLogEventType::HTTP_STREAM_JOB_RESUMED, "delay",
      main_job_wait_time_.InMilliseconds());

  main_job_->Resume();
  main_job_wait_time_ = base::TimeDelta();
}

void HttpStreamFactory::JobController::ResetErrorStatusForJobs() {
  main_job_net_error_ = OK;
  alternative_job_net_error_ = OK;
  alternative_job_failed_on_default_network_ = false;
  dns_alpn_h3_job_net_error_ = OK;
  dns_alpn_h3_job_failed_on_default_network_ = false;
}

bool HttpStreamFactory::JobController::
    disable_cert_verification_network_fetches() const {
  return !!(request_info_.load_flags & LOAD_DISABLE_CERT_NETWORK_FETCHES);
}

void HttpStreamFactory::JobController::MaybeResumeMainJob(
    Job* job,
    const base::TimeDelta& delay) {
  DCHECK(delay == base::TimeDelta() || delay == main_job_wait_time_);
  DCHECK(job == main_job_.get() || job == alternative_job_.get() ||
         job == dns_alpn_h3_job_.get());

  if (job == main_job_.get()) {
    return;
  }
  if (job == dns_alpn_h3_job_.get() && alternative_job_) {
    return;
  }
  if (!main_job_) {
    return;
  }

  main_job_is_blocked_ = false;

  if (!main_job_->is_waiting()) {
    // There are two cases where the main job is not in WAIT state:
    //   1) The main job hasn't got to waiting state, do not yet post a task to
    //      resume since that will happen in ShouldWait().
    //   2) The main job has passed waiting state, so the main job does not need
    //      to be resumed.
    return;
  }

  main_job_wait_time_ = delay;

  ResumeMainJobLater(main_job_wait_time_);
}

void HttpStreamFactory::JobController::OnConnectionInitialized(Job* job,
                                                               int rv) {
  if (rv != OK) {
    // Resume the main job as there's an error raised in connection
    // initiation.
    return MaybeResumeMainJob(job, main_job_wait_time_);
  }
}

bool HttpStreamFactory::JobController::ShouldWait(Job* job) {
  // The alternative job never waits.
  if (job == alternative_job_.get() || job == dns_alpn_h3_job_.get()) {
    return false;
  }
  DCHECK_EQ(main_job_.get(), job);
  if (main_job_is_blocked_) {
    return true;
  }

  if (main_job_wait_time_.is_zero()) {
    return false;
  }

  ResumeMainJobLater(main_job_wait_time_);
  return true;
}

const NetLogWithSource* HttpStreamFactory::JobController::GetNetLog() const {
  return &net_log_;
}

void HttpStreamFactory::JobController::MaybeSetWaitTimeForMainJob(
    const base::TimeDelta& delay) {
  if (main_job_is_blocked_) {
    const bool has_available_spdy_session =
        main_job_->HasAvailableSpdySession();
    if (!delay_main_job_with_available_spdy_session_ &&
        has_available_spdy_session) {
      main_job_wait_time_ = base::TimeDelta();
    } else {
      main_job_wait_time_ =
          std::min(delay, base::Seconds(kMaxDelayTimeForMainJobSecs));
    }
    if (has_available_spdy_session) {
      UMA_HISTOGRAM_TIMES("Net.HttpJob.MainJobWaitTimeWithAvailableSpdySession",
                          main_job_wait_time_);
    } else {
      UMA_HISTOGRAM_TIMES(
          "Net.HttpJob.MainJobWaitTimeWithoutAvailableSpdySession",
          main_job_wait_time_);
    }
  }
}

bool HttpStreamFactory::JobController::HasPendingMainJob() const {
  return main_job_.get() != nullptr;
}

bool HttpStreamFactory::JobController::HasPendingAltJob() const {
  return alternative_job_.get() != nullptr;
}

WebSocketHandshakeStreamBase::CreateHelper*
HttpStreamFactory::JobController::websocket_handshake_stream_create_helper() {
  DCHECK(request_);
  return request_->websocket_handshake_stream_create_helper();
}

void HttpStreamFactory::JobController::OnIOComplete(int result) {
  RunLoop(result);
  // `this` may be deleted at this point.
}

void HttpStreamFactory::JobController::RunLoop(int result) {
  int rv = DoLoop(result);
  if (rv == ERR_IO_PENDING) {
    return;
  }

  if (switched_to_http_stream_pool_) {
    // The request is handed over to the HttpStreamPool. Complete `this`.
    DCHECK_EQ(rv, OK);
    MaybeNotifyFactoryOfCompletion();
    // `this` is deleted.
    return;
  }

  if (rv != OK) {
    // DoLoop can only fail during proxy resolution step which happens before
    // any jobs are created. Notify |request_| of the failure one message loop
    // iteration later to avoid re-entrancy.
    DCHECK(!main_job_);
    DCHECK(!alternative_job_);
    DCHECK(!dns_alpn_h3_job_);
    TaskRunner(priority_)->PostTask(
        FROM_HERE,
        base::BindOnce(&HttpStreamFactory::JobController::NotifyRequestFailed,
                       ptr_factory_.GetWeakPtr(), rv));
  }
}

int HttpStreamFactory::JobController::DoLoop(int rv) {
  DCHECK_NE(next_state_, STATE_NONE);
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_PROXY:
        DCHECK_EQ(OK, rv);
        rv = DoResolveProxy();
        break;
      case STATE_RESOLVE_PROXY_COMPLETE:
        rv = DoResolveProxyComplete(rv);
        break;
      case STATE_CREATE_JOBS:
        DCHECK_EQ(OK, rv);
        rv = DoCreateJobs();
        break;
      default:
        NOTREACHED() << "bad state";
    }
  } while (next_state_ != STATE_NONE && rv != ERR_IO_PENDING);
  return rv;
}

int HttpStreamFactory::JobController::DoResolveProxy() {
  DCHECK(!proxy_resolve_request_);

  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;

  if (request_info_.load_flags & LOAD_BYPASS_PROXY) {
    proxy_info_.UseDirect();
    return OK;
  }

  CompletionOnceCallback io_callback =
      base::BindOnce(&JobController::OnIOComplete, base::Unretained(this));
  return session_->proxy_resolution_service()->ResolveProxy(
      request_info_.url, request_info_.method,
      request_info_.network_anonymization_key, &proxy_info_,
      std::move(io_callback), &proxy_resolve_request_, net_log_, priority_);
}

int HttpStreamFactory::JobController::DoResolveProxyComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);

  proxy_resolve_request_ = nullptr;
  net_log_.AddEvent(
      NetLogEventType::HTTP_STREAM_JOB_CONTROLLER_PROXY_SERVER_RESOLVED, [&] {
        return NetLogHttpStreamJobProxyChainResolved(
            proxy_info_.is_empty() ? ProxyChain() : proxy_info_.proxy_chain());
      });

  if (rv != OK) {
    return rv;
  }
  // Remove unsupported proxies from the list.
  int supported_proxies = ProxyServer::SCHEME_HTTP | ProxyServer::SCHEME_HTTPS |
                          ProxyServer::SCHEME_SOCKS4 |
                          ProxyServer::SCHEME_SOCKS5;
  // WebSockets is not supported over QUIC.
  if (session_->IsQuicEnabled() && !is_websocket_) {
    supported_proxies |= ProxyServer::SCHEME_QUIC;
  }
  proxy_info_.RemoveProxiesWithoutScheme(supported_proxies);

  if (proxy_info_.is_empty()) {
    // No proxies/direct to choose from.
    return ERR_NO_SUPPORTED_PROXIES;
  }

  next_state_ = STATE_CREATE_JOBS;
  return rv;
}

int HttpStreamFactory::JobController::DoCreateJobs() {
  DCHECK(!main_job_);
  DCHECK(!alternative_job_);
  DCHECK(request_info_.url.is_valid());
  DCHECK(request_info_.url.IsStandard());
  stream_creation_attempt_start_time_ = base::TimeTicks::Now();

  url::SchemeHostPort destination(request_info_.url);
  DCHECK(destination.IsValid());
  ConvertWsToHttp(destination);

  // Create an alternative job if alternative service is set up for this domain.
  // This is applicable even if the connection will be made via a proxy.
  advertised_alt_svc_ =
      GetAdvertisedAltSvcFor(request_info_, delegate_, stream_type_);

  if (session_->host_resolver()->IsHappyEyeballsV3Enabled() &&
      proxy_info_.is_direct() && !is_websocket_) {
    SwitchToHttpStreamPool();
    return OK;
  }

  quic::ParsedQuicVersion quic_version = quic::ParsedQuicVersion::Unsupported();
  if (advertised_alt_svc_.info.protocol() == NextProto::kProtoQUIC) {
    quic_version =
        SelectQuicVersion(advertised_alt_svc_.info.advertised_versions());
    DCHECK_NE(quic_version, quic::ParsedQuicVersion::Unsupported());
  }

  // Getting ALPN for H3 from DNS has a lot of preconditions. Among them:
  // - proxied connections perform DNS on the proxy, so they can't get supported
  //   ALPNs from DNS
  const bool dns_alpn_h3_job_enabled =
      !session_->ShouldForceQuic(destination, proxy_info_, is_websocket_) &&
      enable_alternative_services_ &&
      session_->params().use_dns_https_svcb_alpn &&
      base::EqualsCaseInsensitiveASCII(request_info_.url.GetScheme(),
                                       url::kHttpsScheme) &&
      session_->IsQuicEnabled() && proxy_info_.is_direct() &&
      !session_->http_server_properties()->IsAlternativeServiceBroken(
          GetAlternativeServiceForDnsJob(request_info_.url),
          request_info_.network_anonymization_key);

  if (is_preconnect_) {
    // Due to how the socket pools handle priorities and idle sockets, only IDLE
    // priority currently makes sense for preconnects. The priority for
    // preconnects is currently ignored (see RequestSocketsForPool()), but could
    // be used at some point for proxy resolution or something.
    // Note: When `dns_alpn_h3_job_enabled` is true, we create a
    // PRECONNECT_DNS_ALPN_H3 job. If no matching HTTPS DNS ALPN records are
    // received, the PRECONNECT_DNS_ALPN_H3 job will fail with
    // ERR_DNS_NO_MATCHING_SUPPORTED_ALPN, and `preconnect_backup_job_` will
    // be started in OnPreconnectsComplete().
    std::unique_ptr<Job> preconnect_job = job_factory_->CreateJob(
        this, dns_alpn_h3_job_enabled ? PRECONNECT_DNS_ALPN_H3 : PRECONNECT,
        session_, request_info_, IDLE, proxy_info_, allowed_bad_certs_,
        destination, is_websocket_, enable_ip_based_pooling_for_h2_,
        net_log_.net_log(), NextProto::kProtoUnknown,
        quic::ParsedQuicVersion::Unsupported(), management_config_);
    // When there is an valid alternative service info, and `preconnect_job`
    // has no existing QUIC session, create a job for the alternative service.
    if (advertised_alt_svc_.info.protocol() != NextProto::kProtoUnknown &&
        !preconnect_job->HasAvailableQuicSession()) {
      GURL alternative_url = CreateAltSvcUrl(
          request_info_.url, advertised_alt_svc_.info.GetHostPortPair());

      url::SchemeHostPort alternative_destination =
          url::SchemeHostPort(alternative_url);
      ConvertWsToHttp(alternative_destination);

      main_job_ = job_factory_->CreateJob(
          this, PRECONNECT, session_, request_info_, IDLE, proxy_info_,
          allowed_bad_certs_, std::move(alternative_destination), is_websocket_,
          enable_ip_based_pooling_for_h2_, session_->net_log(),
          advertised_alt_svc_.info.protocol(), quic_version,
          management_config_);
    } else {
      main_job_ = std::move(preconnect_job);

      if (dns_alpn_h3_job_enabled) {
        preconnect_backup_job_ = job_factory_->CreateJob(
            this, PRECONNECT, session_, request_info_, IDLE, proxy_info_,
            allowed_bad_certs_, std::move(destination), is_websocket_,
            enable_ip_based_pooling_for_h2_, net_log_.net_log(),
            NextProto::kProtoUnknown, quic::ParsedQuicVersion::Unsupported(),
            management_config_);
      }
    }
    main_job_->Preconnect(num_streams_);
    return OK;
  }
  main_job_ = job_factory_->CreateJob(
      this, MAIN, session_, request_info_, priority_, proxy_info_,
      allowed_bad_certs_, std::move(destination), is_websocket_,
      enable_ip_based_pooling_for_h2_, net_log_.net_log(),
      NextProto::kProtoUnknown, quic::ParsedQuicVersion::Unsupported(),
      management_config_);

  // Alternative Service can only be set for HTTPS requests while Alternative
  // Proxy is set for HTTP requests.
  // The main job may use HTTP/3 if the origin is specified in
  // `--origin-to-force-quic-on` switch. In that case, do not create
  // `alternative_job_` and `dns_alpn_h3_job_`.
  if ((advertised_alt_svc_.info.protocol() != NextProto::kProtoUnknown) &&
      !main_job_->using_quic()) {
    DCHECK(request_info_.url.SchemeIs(url::kHttpsScheme));
    DCHECK(!is_websocket_);
    DVLOG(1) << "Selected alternative service (host: "
             << advertised_alt_svc_.info.GetHostPortPair().host()
             << " port: " << advertised_alt_svc_.info.GetHostPortPair().port()
             << " version: " << quic_version << ")";

    GURL alternative_url = CreateAltSvcUrl(
        request_info_.url, advertised_alt_svc_.info.GetHostPortPair());

    url::SchemeHostPort alternative_destination =
        url::SchemeHostPort(alternative_url);
    ConvertWsToHttp(alternative_destination);

    alternative_job_ = job_factory_->CreateJob(
        this, ALTERNATIVE, session_, request_info_, priority_, proxy_info_,
        allowed_bad_certs_, std::move(alternative_destination), is_websocket_,
        enable_ip_based_pooling_for_h2_, net_log_.net_log(),
        advertised_alt_svc_.info.protocol(), quic_version, management_config_);
  }

  if (dns_alpn_h3_job_enabled && !main_job_->using_quic()) {
    DCHECK(!is_websocket_);
    url::SchemeHostPort dns_alpn_h3_destination =
        url::SchemeHostPort(request_info_.url);
    dns_alpn_h3_job_ = job_factory_->CreateJob(
        this, DNS_ALPN_H3, session_, request_info_, priority_, proxy_info_,
        allowed_bad_certs_, std::move(dns_alpn_h3_destination), is_websocket_,
        enable_ip_based_pooling_for_h2_, net_log_.net_log(),
        NextProto::kProtoUnknown, quic::ParsedQuicVersion::Unsupported(),
        management_config_);
  }

  ClearInappropriateJobs();

  if (main_job_ && (alternative_job_ ||
                    (dns_alpn_h3_job_ &&
                     (!main_job_->TargettedSocketGroupHasActiveSocket() &&
                      !main_job_->HasAvailableSpdySession())))) {
    // We don't block |main_job_| when |alternative_job_| doesn't exists and
    // |dns_alpn_h3_job_| exists and an active socket is available for
    // |main_job_|. This is intended to make the fallback logic faster.
    main_job_is_blocked_ = true;
  }

  if (alternative_job_) {
    alternative_job_->Start(request_->stream_type());
  }

  if (dns_alpn_h3_job_) {
    dns_alpn_h3_job_->Start(request_->stream_type());
  }

  if (main_job_) {
    main_job_->Start(request_->stream_type());
  }
  return OK;
}

void HttpStreamFactory::JobController::ClearInappropriateJobs() {
  if (dns_alpn_h3_job_ && dns_alpn_h3_job_->HasAvailableQuicSession()) {
    // Clear |main_job_| and |alternative_job_| here not to start them when
    // there is an active session available for |dns_alpn_h3_job_|.
    main_job_.reset();
    alternative_job_.reset();
  }

  if (alternative_job_ && dns_alpn_h3_job_ &&
      (alternative_job_->HasAvailableQuicSession() ||
       (advertised_alt_svc_.info.alternative_service() ==
        GetAlternativeServiceForDnsJob(request_info_.url)))) {
    // Clear |dns_alpn_h3_job_|, when there is an active session available for
    // |alternative_job_| or |alternative_job_| was created for the same
    // destination.
    dns_alpn_h3_job_.reset();
  }
}

void HttpStreamFactory::JobController::BindJob(Job* job) {
  DCHECK(request_);
  DCHECK(job);
  DCHECK(job == alternative_job_.get() || job == main_job_.get() ||
         job == dns_alpn_h3_job_.get());
  DCHECK(!job_bound_);
  DCHECK(!bound_job_);

  job_bound_ = true;
  bound_job_ = job;

  request_->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_REQUEST_BOUND_TO_JOB,
      job->net_log().source());
  job->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_JOB_BOUND_TO_REQUEST,
      request_->net_log().source());
  net_log_.AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_JOB_BOUND_TO_REQUEST,
      request_->net_log().source());

  OrphanUnboundJob();
}

void HttpStreamFactory::JobController::OrphanUnboundJob() {
  DCHECK(request_);
  DCHECK(bound_job_);

  if (bound_job_->job_type() == MAIN) {
    // Allow |alternative_job_| and |dns_alpn_h3_job_| to run to completion,
    // rather than resetting them to check if there is any broken alternative
    // service to report. OnOrphanedJobComplete() will clean up |this| when the
    // jobs complete.
    if (alternative_job_) {
      DCHECK(!is_websocket_);
      alternative_job_->Orphan();
    }
    if (dns_alpn_h3_job_) {
      DCHECK(!is_websocket_);
      dns_alpn_h3_job_->Orphan();
    }
    return;
  }

  if (bound_job_->job_type() == ALTERNATIVE) {
    if (!alternative_job_failed_on_default_network_ && !dns_alpn_h3_job_) {
      // |request_| is bound to the alternative job and the alternative job
      // succeeds on the default network, and there is no DNS alt job. This
      // means that the main job is no longer needed, so cancel it now. Pending
      // ConnectJobs will return established sockets to socket pools if
      // applicable.
      // https://crbug.com/757548.
      // The main job still needs to run if the alternative job succeeds on the
      // alternate network in order to figure out whether QUIC should be marked
      // as broken until the default network changes. And also the main job
      // still needs to run if the DNS alt job exists to figure out whether
      // the DNS alpn service is broken.
      DCHECK(!main_job_ || (alternative_job_net_error_ == OK));
      main_job_.reset();
    }
    // Allow |dns_alpn_h3_job_| to run to completion, rather than resetting
    // it to check if there is any broken alternative service to report.
    // OnOrphanedJobComplete() will clean up |this| when the job completes.
    if (dns_alpn_h3_job_) {
      DCHECK(!is_websocket_);
      dns_alpn_h3_job_->Orphan();
    }
  }
  if (bound_job_->job_type() == DNS_ALPN_H3) {
    if (!dns_alpn_h3_job_failed_on_default_network_ && !alternative_job_) {
      DCHECK(!main_job_ || (dns_alpn_h3_job_net_error_ == OK));
      main_job_.reset();
    }
    // Allow |alternative_job_| to run to completion, rather than resetting
    // it to check if there is any broken alternative service to report.
    // OnOrphanedJobComplete() will clean up |this| when the job completes.
    if (alternative_job_) {
      DCHECK(!is_websocket_);
      alternative_job_->Orphan();
    }
  }
}

void HttpStreamFactory::JobController::OnJobSucceeded(Job* job) {
  DCHECK(job);
  if (!bound_job_) {
    BindJob(job);
    return;
  }
}

void HttpStreamFactory::JobController::MarkRequestComplete(Job* job) {
  if (request_) {
    AlternateProtocolUsage alternate_protocol_usage =
        CalculateAlternateProtocolUsage(job);
    std::optional<SessionSource> session_source = std::nullopt;
    switch (job->negotiated_protocol()) {
      case NextProto::kProtoUnknown:
      case NextProto::kProtoHTTP11:
        break;
      case NextProto::kProtoHTTP2:
        session_source = job->used_existing_spdy_session()
                             ? SessionSource::kExisting
                             : SessionSource::kNew;
        break;
      case NextProto::kProtoQUIC:
        session_source = job->using_existing_quic_session()
                             ? SessionSource::kExisting
                             : SessionSource::kNew;
        break;
    }
    request_->Complete({.negotiated_protocol = job->negotiated_protocol(),
                        .alternate_protocol_usage = alternate_protocol_usage,
                        .session_source = session_source,
                        .advertised_alt_svc_state = advertised_alt_svc_.state});
    ReportAlternateProtocolUsage(alternate_protocol_usage,
                                 HasGoogleHost(request_info_.url));
  }
}

void HttpStreamFactory::JobController::MaybeReportBrokenAlternativeService(
    const AlternativeService& alt_service,
    int alt_job_net_error,
    bool alt_job_failed_on_default_network,
    const std::string& histogram_name_for_failure) {
  // If alternative job succeeds on the default network, no brokenness to
  // report.
  if (alt_job_net_error == OK && !alt_job_failed_on_default_network) {
    return;
  }

  // No brokenness to report if the main job fails.
  if (main_job_net_error_ != OK) {
    return;
  }

  // No need to record DNS_NO_MATCHING_SUPPORTED_ALPN error.
  if (alt_job_net_error == ERR_DNS_NO_MATCHING_SUPPORTED_ALPN) {
    return;
  }

  if (alt_job_failed_on_default_network && alt_job_net_error == OK) {
    // Alternative job failed on the default network but succeeds on the
    // non-default network, mark alternative service broken until the default
    // network changes.
    session_->http_server_properties()
        ->MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
            alt_service, request_info_.network_anonymization_key);
    return;
  }

  if (alt_job_net_error == ERR_NETWORK_CHANGED ||
      alt_job_net_error == ERR_INTERNET_DISCONNECTED ||
      (alt_job_net_error == ERR_NAME_NOT_RESOLVED &&
       request_info_.url.GetHost() == alt_service.host)) {
    // No need to mark alternative service as broken.
    return;
  }

  // Report brokenness if alternative job failed.
  base::UmaHistogramSparse(histogram_name_for_failure, -alt_job_net_error);

  HistogramBrokenAlternateProtocolLocation(
      BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_JOB_ALT);
  session_->http_server_properties()->MarkAlternativeServiceBroken(
      alt_service, request_info_.network_anonymization_key);
}

void HttpStreamFactory::JobController::MaybeNotifyFactoryOfCompletion() {
  if (switched_to_http_stream_pool_) {
    factory_->OnJobControllerComplete(this);
    return;
  }

  if (main_job_ || alternative_job_ || dns_alpn_h3_job_) {
    return;
  }

  // All jobs are gone.
  // Report brokenness for the alternate jobs if apply.
  MaybeReportBrokenAlternativeService(
      advertised_alt_svc_.info.alternative_service(),
      alternative_job_net_error_, alternative_job_failed_on_default_network_,
      "Net.AlternateServiceFailed");
  // Report for the DNS alt job if apply.
  MaybeReportBrokenAlternativeService(
      GetAlternativeServiceForDnsJob(request_info_.url),
      dns_alpn_h3_job_net_error_, dns_alpn_h3_job_failed_on_default_network_,
      "Net.AlternateServiceForDnsAlpnH3Failed");

  // Reset error status for Jobs after reporting brokenness to avoid redundant
  // reporting.
  ResetErrorStatusForJobs();

  if (request_) {
    return;
  }
  DCHECK(!bound_job_);
  factory_->OnJobControllerComplete(this);
}

void HttpStreamFactory::JobController::NotifyRequestFailed(int rv) {
  if (!request_) {
    return;
  }
  delegate_->OnStreamFailed(rv, NetErrorDetails(), ProxyInfo(),
                            ResolveErrorInfo());
}

HttpStreamFactory::JobController::AdvertisedAlternativeService
HttpStreamFactory::JobController::GetAdvertisedAltSvcFor(
    const StreamRequestInfo& request_info,
    HttpStreamRequest::Delegate* delegate,
    HttpStreamRequest::StreamType stream_type) {
  if (!enable_alternative_services_) {
    return AdvertisedAlternativeService();
  }

  AdvertisedAlternativeService alternative_service_info =
      GetAdvertisedAltSvcInternal(request_info, delegate, stream_type);
  AlternativeServiceType type;
  if (alternative_service_info.info.protocol() == NextProto::kProtoUnknown) {
    type = NO_ALTERNATIVE_SERVICE;
  } else if (alternative_service_info.info.protocol() ==
             NextProto::kProtoQUIC) {
    if (request_info.url.host() ==
        alternative_service_info.info.alternative_service().host) {
      type = QUIC_SAME_DESTINATION;
    } else {
      type = QUIC_DIFFERENT_DESTINATION;
    }
  } else {
    if (request_info.url.host() ==
        alternative_service_info.info.alternative_service().host) {
      type = NOT_QUIC_SAME_DESTINATION;
    } else {
      type = NOT_QUIC_DIFFERENT_DESTINATION;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Net.AlternativeServiceTypeForRequest", type,
                            MAX_ALTERNATIVE_SERVICE_TYPE);
  return alternative_service_info;
}

HttpStreamFactory::JobController::AdvertisedAlternativeService
HttpStreamFactory::JobController::GetAdvertisedAltSvcInternal(
    const StreamRequestInfo& request_info,
    HttpStreamRequest::Delegate* delegate,
    HttpStreamRequest::StreamType stream_type) {
  if (!request_info.url.SchemeIs(url::kHttpsScheme)) {
    return AdvertisedAlternativeService();
  }

  HttpServerProperties& http_server_properties =
      *session_->http_server_properties();
  const AlternativeServiceInfoVector alternative_service_info_vector =
      http_server_properties.GetAlternativeServiceInfos(
          url::SchemeHostPort(request_info.url),
          request_info.network_anonymization_key);
  if (alternative_service_info_vector.empty()) {
    return AdvertisedAlternativeService();
  }

  bool quic_advertised = false;
  bool quic_all_broken = true;

  // First alternative service that is not marked as broken.
  AlternativeServiceInfo first_alternative_service_info;

  bool is_any_broken = false;
  for (const AlternativeServiceInfo& alternative_service_info :
       alternative_service_info_vector) {
    DCHECK(IsAlternateProtocolValid(alternative_service_info.protocol()));
    if (!quic_advertised &&
        alternative_service_info.protocol() == NextProto::kProtoQUIC) {
      quic_advertised = true;
    }
    const bool is_broken = http_server_properties.IsAlternativeServiceBroken(
        alternative_service_info.alternative_service(),
        request_info.network_anonymization_key);
    net_log_.AddEvent(
        NetLogEventType::HTTP_STREAM_JOB_CONTROLLER_ALT_SVC_FOUND, [&] {
          return NetLogAltSvcParams(&alternative_service_info, is_broken);
        });
    if (is_broken) {
      if (!is_any_broken) {
        // Only log the broken alternative service once per request.
        is_any_broken = true;
        HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_BROKEN,
                                        HasGoogleHost(request_info.url));
      }
      continue;
    }

    // Some shared unix systems may have user home directories (like
    // http://foo.com/~mike) which allow users to emit headers.  This is a bad
    // idea already, but with Alternate-Protocol, it provides the ability for a
    // single user on a multi-user system to hijack the alternate protocol.
    // These systems also enforce ports <1024 as restricted ports.  So don't
    // allow protocol upgrades to user-controllable ports.
    const int kUnrestrictedPort = 1024;
    if (!session_->params().enable_user_alternate_protocol_ports &&
        (alternative_service_info.alternative_service().port >=
             kUnrestrictedPort &&
         request_info.url.EffectiveIntPort() < kUnrestrictedPort)) {
      continue;
    }

    if (alternative_service_info.protocol() == NextProto::kProtoHTTP2) {
      if (!session_->params().enable_http2_alternative_service) {
        continue;
      }

      // Cache this entry if we don't have a non-broken Alt-Svc yet.
      if (first_alternative_service_info.protocol() ==
          NextProto::kProtoUnknown) {
        first_alternative_service_info = alternative_service_info;
      }
      continue;
    }

    DCHECK_EQ(NextProto::kProtoQUIC, alternative_service_info.protocol());
    quic_all_broken = false;
    if (!session_->IsQuicEnabled()) {
      continue;
    }

    if (!request_info.url.SchemeIs(url::kHttpsScheme)) {
      continue;
    }

    // If there is no QUIC version in the advertised versions that is
    // supported, ignore this entry.
    if (SelectQuicVersion(alternative_service_info.advertised_versions()) ==
        quic::ParsedQuicVersion::Unsupported()) {
      continue;
    }

    // Check whether there is an existing QUIC session to use for this origin.
    QuicSessionKey session_key(
        HostPortPair::FromURL(request_info.url), request_info.privacy_mode,
        proxy_info_.proxy_chain(), SessionUsage::kDestination,
        request_info.socket_tag, request_info.network_anonymization_key,
        request_info.secure_dns_policy, /*require_dns_https_alpn=*/false,
        disable_cert_verification_network_fetches());

    GURL destination = CreateAltSvcUrl(
        request_info.url, alternative_service_info.GetHostPortPair());
    if (session_key.host() != destination.host() &&
        !session_->context().quic_context->params()->allow_remote_alt_svc) {
      continue;
    }

    if (session_->quic_session_pool()->CanUseExistingSession(
            session_key, url::SchemeHostPort(destination))) {
      return {alternative_service_info, AdvertisedAltSvcState::kQuicNotBroken};
    }

    if (!IsQuicAllowedForHost(destination.GetHost())) {
      continue;
    }

    // Cache this entry if we don't have a non-broken Alt-Svc yet.
    if (first_alternative_service_info.protocol() == NextProto::kProtoUnknown) {
      first_alternative_service_info = alternative_service_info;
    }
  }

  // Ask delegate to mark QUIC as broken for the origin.
  if (quic_advertised && quic_all_broken && delegate != nullptr) {
    delegate->OnQuicBroken();
  }

  AdvertisedAltSvcState state = AdvertisedAltSvcState::kUnknown;
  if (quic_advertised) {
    if (quic_all_broken) {
      state = AdvertisedAltSvcState::kQuicBroken;
    } else {
      state = AdvertisedAltSvcState::kQuicNotBroken;
    }
  }

  return {first_alternative_service_info, state};
}

quic::ParsedQuicVersion HttpStreamFactory::JobController::SelectQuicVersion(
    const quic::ParsedQuicVersionVector& advertised_versions) {
  return session_->context().quic_context->SelectQuicVersion(
      advertised_versions);
}

void HttpStreamFactory::JobController::ReportAlternateProtocolUsage(
    AlternateProtocolUsage alternate_protocol_usage,
    bool is_google_host) const {
  DCHECK_LT(alternate_protocol_usage, ALTERNATE_PROTOCOL_USAGE_MAX);
  HistogramAlternateProtocolUsage(alternate_protocol_usage, is_google_host);
}

bool HttpStreamFactory::JobController::IsJobOrphaned(Job* job) const {
  return !request_ || (job_bound_ && bound_job_ != job);
}

AlternateProtocolUsage
HttpStreamFactory::JobController::CalculateAlternateProtocolUsage(
    Job* job) const {
  if ((main_job_ && alternative_job_) || dns_alpn_h3_job_) {
    if (job == main_job_.get()) {
      return ALTERNATE_PROTOCOL_USAGE_MAIN_JOB_WON_RACE;
    }
    if (job == alternative_job_.get()) {
      if (job->using_existing_quic_session()) {
        return ALTERNATE_PROTOCOL_USAGE_NO_RACE;
      }
      return ALTERNATE_PROTOCOL_USAGE_WON_RACE;
    }
    if (job == dns_alpn_h3_job_.get()) {
      if (job->using_existing_quic_session()) {
        return ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_WITHOUT_RACE;
      }
      return ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE;
    }
  }
  // TODO(crbug.com/40232167): Implement better logic to support uncovered
  // cases.
  return ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON;
}

int HttpStreamFactory::JobController::ReconsiderProxyAfterError(Job* job,
                                                                int error) {
  // ReconsiderProxyAfterError() should only be called when the last job fails.
  DCHECK_EQ(1, GetJobCount());
  DCHECK(!proxy_resolve_request_);

  if (!job->should_reconsider_proxy()) {
    return error;
  }

  if (request_info_.load_flags & LOAD_BYPASS_PROXY) {
    return error;
  }

  // Clear client certificates for all proxies in the chain.
  // TODO(crbug.com/40284947): client certificates for multi-proxy
  // chains are not yet supported, and this is only tested with single-proxy
  // chains.
  for (auto& proxy_server : proxy_info_.proxy_chain().proxy_servers()) {
    if (proxy_server.is_secure_http_like()) {
      session_->ssl_client_context()->ClearClientCertificate(
          proxy_server.host_port_pair());
    }
  }

  if (!proxy_info_.Fallback(error, net_log_)) {
    // If there is no more proxy to fallback to, fail the transaction
    // with the last connection error we got.
    return error;
  }

  // Abandon all Jobs and start over.
  job_bound_ = false;
  bound_job_ = nullptr;
  dns_alpn_h3_job_.reset();
  alternative_job_.reset();
  main_job_.reset();
  ResetErrorStatusForJobs();
  // Also resets states that related to the old main job. In particular,
  // cancels |resume_main_job_callback_| so there won't be any delayed
  // ResumeMainJob() left in the task queue.
  resume_main_job_callback_.Cancel();
  main_job_is_resumed_ = false;
  main_job_is_blocked_ = false;

  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;
  return OK;
}

bool HttpStreamFactory::JobController::IsQuicAllowedForHost(
    const std::string& host) {
  const base::flat_set<std::string>& host_allowlist =
      session_->params().quic_host_allowlist;
  if (host_allowlist.empty()) {
    return true;
  }

  std::string lowered_host = base::ToLowerASCII(host);
  return base::Contains(host_allowlist, lowered_host);
}

void HttpStreamFactory::JobController::SwitchToHttpStreamPool() {
  CHECK(request_info_.socket_tag == SocketTag());
  CHECK_EQ(stream_type_, HttpStreamRequest::HTTP_STREAM);
  CHECK(session_->host_resolver()->IsHappyEyeballsV3Enabled());

  switched_to_http_stream_pool_ = true;

  bool disable_cert_network_fetches =
      disable_cert_verification_network_fetches();
  NextProtoSet allowed_alpns =
      request_info_.is_http1_allowed
          ? NextProtoSet::All()
          : NextProtoSet{NextProto::kProtoHTTP2, NextProto::kProtoQUIC};
  url::SchemeHostPort destination(request_info_.url);
  session_->ApplyTestingFixedPort(destination);
  HttpStreamPoolRequestInfo pool_request_info(
      std::move(destination), request_info_.privacy_mode,
      request_info_.socket_tag, request_info_.network_anonymization_key,
      request_info_.secure_dns_policy, disable_cert_network_fetches,
      advertised_alt_svc_.info, advertised_alt_svc_.state, allowed_alpns,
      request_info_.load_flags, proxy_info_, net_log_);
  if (is_preconnect_) {
    auto split_callback = base::SplitOnceCallback(
        base::IgnoreArgs<int>(std::move(preconnect_callback_)));
    int rv = session_->http_stream_pool()->Preconnect(
        std::move(pool_request_info), num_streams_,
        std::move(split_callback.first));
    if (rv != ERR_IO_PENDING && split_callback.second) {
      std::move(split_callback.second).Run(rv);
    }
    return;
  }

  // Exchange `request_` and `delegate_` to prevent them from being dangling.
  session_->http_stream_pool()->HandleStreamRequest(
      std::exchange(request_, nullptr), std::exchange(delegate_, nullptr),
      std::move(pool_request_info), priority_, allowed_bad_certs_,
      enable_ip_based_pooling_for_h2_, enable_alternative_services_);
}

void HttpStreamFactory::JobController::NotifyOnStreamCreationAttempted(
    base::optional_ref<int> net_error) {
  auto* proxy_delegate = session_->context().proxy_delegate.get();
  if (!proxy_delegate || proxy_info_.is_empty()) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - stream_creation_attempt_start_time_;

  proxy_delegate->OnStreamCreationAttempted(proxy_info_.proxy_chain(), duration,
                                            net_error);
}

}  // namespace net
