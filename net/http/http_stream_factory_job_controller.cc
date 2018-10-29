// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_job_controller.h"

#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "net/base/host_mapping_rules.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/spdy/spdy_session.h"
#include "url/url_constants.h"

namespace net {

namespace {

// Returns parameters associated with the proxy resolution.
std::unique_ptr<base::Value> NetLogHttpStreamJobProxyServerResolved(
    const ProxyServer& proxy_server,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("proxy_server", proxy_server.is_valid()
                                      ? proxy_server.ToPacString()
                                      : std::string());
  return std::move(dict);
}

}  // namespace

// The maximum time to wait for the alternate job to complete before resuming
// the main job.
const int kMaxDelayTimeForMainJobSecs = 3;

std::unique_ptr<base::Value> NetLogJobControllerCallback(
    const GURL* url,
    bool is_preconnect,
    NetLogCaptureMode /* capture_mode */) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString("url", url->possibly_invalid_spec());
  dict->SetBoolean("is_preconnect", is_preconnect);
  return std::move(dict);
}

HttpStreamFactory::JobController::JobController(
    HttpStreamFactory* factory,
    HttpStreamRequest::Delegate* delegate,
    HttpNetworkSession* session,
    JobFactory* job_factory,
    const HttpRequestInfo& request_info,
    bool is_preconnect,
    bool is_websocket,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config)
    : factory_(factory),
      session_(session),
      job_factory_(job_factory),
      request_(nullptr),
      delegate_(delegate),
      is_preconnect_(is_preconnect),
      is_websocket_(is_websocket),
      enable_ip_based_pooling_(enable_ip_based_pooling),
      enable_alternative_services_(enable_alternative_services),
      main_job_net_error_(OK),
      alternative_job_net_error_(OK),
      alternative_job_failed_on_default_network_(false),
      job_bound_(false),
      main_job_is_blocked_(false),
      main_job_is_resumed_(false),
      bound_job_(nullptr),
      can_start_alternative_proxy_job_(true),
      next_state_(STATE_RESOLVE_PROXY),
      proxy_resolve_request_(nullptr),
      request_info_(request_info),
      server_ssl_config_(server_ssl_config),
      proxy_ssl_config_(proxy_ssl_config),
      num_streams_(0),
      priority_(IDLE),
      net_log_(
          NetLogWithSource::Make(session->net_log(),
                                 NetLogSourceType::HTTP_STREAM_JOB_CONTROLLER)),
      ptr_factory_(this) {
  DCHECK(factory);
  net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_JOB_CONTROLLER,
                      base::Bind(&NetLogJobControllerCallback,
                                 &request_info.url, is_preconnect));
}

HttpStreamFactory::JobController::~JobController() {
  main_job_.reset();
  alternative_job_.reset();
  bound_job_ = nullptr;
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
  DCHECK(factory_);
  DCHECK(!request_);

  stream_type_ = stream_type;
  priority_ = priority;

  auto request = std::make_unique<HttpStreamRequest>(
      request_info_.url, this, delegate,
      websocket_handshake_stream_create_helper, source_net_log, stream_type);
  // Keep a raw pointer but release ownership of HttpStreamRequest instance.
  request_ = request.get();

  // Associates |net_log_| with |source_net_log|.
  source_net_log.AddEvent(NetLogEventType::HTTP_STREAM_JOB_CONTROLLER_BOUND,
                          net_log_.source().ToEventParametersCallback());
  net_log_.AddEvent(NetLogEventType::HTTP_STREAM_JOB_CONTROLLER_BOUND,
                    source_net_log.source().ToEventParametersCallback());

  RunLoop(OK);
  return request;
}

void HttpStreamFactory::JobController::Preconnect(int num_streams) {
  DCHECK(!main_job_);
  DCHECK(!alternative_job_);
  DCHECK(is_preconnect_);

  stream_type_ = HttpStreamRequest::HTTP_STREAM;
  num_streams_ = num_streams;

  RunLoop(OK);
}

LoadState HttpStreamFactory::JobController::GetLoadState() const {
  DCHECK(request_);
  if (next_state_ == STATE_RESOLVE_PROXY_COMPLETE)
    return proxy_resolve_request_->GetLoadState();
  if (bound_job_)
    return bound_job_->GetLoadState();
  if (main_job_)
    return main_job_->GetLoadState();
  if (alternative_job_)
    return alternative_job_->GetLoadState();
  // When proxy resolution fails, there is no job created and
  // NotifyRequestFailed() is executed one message loop iteration later.
  return LOAD_STATE_IDLE;
}

void HttpStreamFactory::JobController::OnRequestComplete() {
  DCHECK(request_);

  RemoveRequestFromSpdySessionRequestMap();
  CancelJobs();
  request_ = nullptr;
  if (bound_job_) {
    if (bound_job_->job_type() == MAIN) {
      main_job_.reset();
    } else {
      DCHECK(bound_job_->job_type() == ALTERNATIVE);
      alternative_job_.reset();
    }
    bound_job_ = nullptr;
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
}

void HttpStreamFactory::JobController::OnStreamReadyOnPooledConnection(
    const SSLConfig& used_ssl_config,
    const ProxyInfo& proxy_info,
    std::unique_ptr<HttpStream> stream) {
  DCHECK(request_->completed());
  DCHECK(!is_websocket_);
  DCHECK_EQ(HttpStreamRequest::HTTP_STREAM, request_->stream_type());

  main_job_.reset();
  alternative_job_.reset();
  ResetErrorStatusForJobs();

  factory_->OnStreamReady(proxy_info, request_info_.privacy_mode);

  delegate_->OnStreamReady(used_ssl_config, proxy_info, std::move(stream));
}

void HttpStreamFactory::JobController::
    OnBidirectionalStreamImplReadyOnPooledConnection(
        const SSLConfig& used_ssl_config,
        const ProxyInfo& used_proxy_info,
        std::unique_ptr<BidirectionalStreamImpl> stream) {
  DCHECK(request_->completed());
  DCHECK(!is_websocket_);
  DCHECK_EQ(HttpStreamRequest::BIDIRECTIONAL_STREAM, request_->stream_type());

  main_job_.reset();
  alternative_job_.reset();
  ResetErrorStatusForJobs();

  delegate_->OnBidirectionalStreamImplReady(used_ssl_config, used_proxy_info,
                                            std::move(stream));
}

void HttpStreamFactory::JobController::OnStreamReady(
    Job* job,
    const SSLConfig& used_ssl_config) {
  DCHECK(job);

  factory_->OnStreamReady(job->proxy_info(), request_info_.privacy_mode);

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }
  std::unique_ptr<HttpStream> stream = job->ReleaseStream();
  DCHECK(stream);

  MarkRequestComplete(job->was_alpn_negotiated(), job->negotiated_protocol(),
                      job->using_spdy());

  if (!request_)
    return;
  DCHECK(!is_websocket_);
  DCHECK_EQ(HttpStreamRequest::HTTP_STREAM, request_->stream_type());
  OnJobSucceeded(job);
  DCHECK(request_->completed());
  delegate_->OnStreamReady(used_ssl_config, job->proxy_info(),
                           std::move(stream));
}

void HttpStreamFactory::JobController::OnBidirectionalStreamImplReady(
    Job* job,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info) {
  DCHECK(job);

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }

  MarkRequestComplete(job->was_alpn_negotiated(), job->negotiated_protocol(),
                      job->using_spdy());

  if (!request_)
    return;
  std::unique_ptr<BidirectionalStreamImpl> stream =
      job->ReleaseBidirectionalStream();
  DCHECK(stream);
  DCHECK(!is_websocket_);
  DCHECK_EQ(HttpStreamRequest::BIDIRECTIONAL_STREAM, request_->stream_type());

  OnJobSucceeded(job);
  DCHECK(request_->completed());
  delegate_->OnBidirectionalStreamImplReady(used_ssl_config, used_proxy_info,
                                            std::move(stream));
}

void HttpStreamFactory::JobController::OnWebSocketHandshakeStreamReady(
    Job* job,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    std::unique_ptr<WebSocketHandshakeStreamBase> stream) {
  DCHECK(job);
  MarkRequestComplete(job->was_alpn_negotiated(), job->negotiated_protocol(),
                      job->using_spdy());

  if (!request_)
    return;
  DCHECK(is_websocket_);
  DCHECK_EQ(HttpStreamRequest::HTTP_STREAM, request_->stream_type());
  DCHECK(stream);

  OnJobSucceeded(job);
  DCHECK(request_->completed());
  delegate_->OnWebSocketHandshakeStreamReady(used_ssl_config, used_proxy_info,
                                             std::move(stream));
}

void HttpStreamFactory::JobController::OnStreamFailed(
    Job* job,
    int status,
    const SSLConfig& used_ssl_config) {
  if (job->job_type() == ALTERNATIVE) {
    DCHECK_EQ(alternative_job_.get(), job);
    if (alternative_job_->alternative_proxy_server().is_valid()) {
      OnAlternativeProxyJobFailed(status);
    } else {
      OnAlternativeServiceJobFailed(status);
    }
  } else {
    DCHECK_EQ(main_job_.get(), job);
    main_job_net_error_ = status;
  }

  MaybeResumeMainJob(job, base::TimeDelta());

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }

  if (!request_)
    return;
  DCHECK_NE(OK, status);
  DCHECK(job);

  if (!bound_job_) {
    if (main_job_ && alternative_job_) {
      // Hey, we've got other jobs! Maybe one of them will succeed, let's just
      // ignore this failure.
      if (job->job_type() == MAIN) {
        main_job_.reset();
      } else {
        DCHECK(job->job_type() == ALTERNATIVE);
        alternative_job_.reset();
      }
      return;
    } else {
      BindJob(job);
    }
  }

  status = ReconsiderProxyAfterError(job, status);
  if (next_state_ == STATE_RESOLVE_PROXY_COMPLETE) {
    if (status == ERR_IO_PENDING)
      return;
    DCHECK_EQ(OK, status);
    RunLoop(status);
    return;
  }
  delegate_->OnStreamFailed(status, *job->net_error_details(), used_ssl_config);
}

void HttpStreamFactory::JobController::OnFailedOnDefaultNetwork(Job* job) {
  DCHECK_EQ(job->job_type(), ALTERNATIVE);
  alternative_job_failed_on_default_network_ = true;
}

void HttpStreamFactory::JobController::OnCertificateError(
    Job* job,
    int status,
    const SSLConfig& used_ssl_config,
    const SSLInfo& ssl_info) {
  MaybeResumeMainJob(job, base::TimeDelta());

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }

  if (!request_)
    return;
  DCHECK_NE(OK, status);
  if (!bound_job_)
    BindJob(job);

  delegate_->OnCertificateError(status, used_ssl_config, ssl_info);
}

void HttpStreamFactory::JobController::OnHttpsProxyTunnelResponse(
    Job* job,
    const HttpResponseInfo& response_info,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    std::unique_ptr<HttpStream> stream) {
  MaybeResumeMainJob(job, base::TimeDelta());

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }

  if (!bound_job_)
    BindJob(job);
  if (!request_)
    return;
  delegate_->OnHttpsProxyTunnelResponse(response_info, used_ssl_config,
                                        used_proxy_info, std::move(stream));
}

void HttpStreamFactory::JobController::OnNeedsClientAuth(
    Job* job,
    const SSLConfig& used_ssl_config,
    SSLCertRequestInfo* cert_info) {
  MaybeResumeMainJob(job, base::TimeDelta());

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }
  if (!request_)
    return;
  if (!bound_job_)
    BindJob(job);

  delegate_->OnNeedsClientAuth(used_ssl_config, cert_info);
}

void HttpStreamFactory::JobController::OnNeedsProxyAuth(
    Job* job,
    const HttpResponseInfo& proxy_response,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpAuthController* auth_controller) {
  MaybeResumeMainJob(job, base::TimeDelta());

  if (IsJobOrphaned(job)) {
    // We have bound a job to the associated HttpStreamRequest, |job| has been
    // orphaned.
    OnOrphanedJobComplete(job);
    return;
  }

  if (!request_)
    return;
  if (!bound_job_)
    BindJob(job);
  delegate_->OnNeedsProxyAuth(proxy_response, used_ssl_config, used_proxy_info,
                              auth_controller);
}

bool HttpStreamFactory::JobController::OnInitConnection(
    const ProxyInfo& proxy_info) {
  return factory_->OnInitConnection(*this, proxy_info,
                                    request_info_.privacy_mode);
}

void HttpStreamFactory::JobController::OnNewSpdySessionReady(
    Job* job,
    const base::WeakPtr<SpdySession>& spdy_session) {
  DCHECK(job);
  DCHECK(job->using_spdy());
  DCHECK(!is_preconnect_);

  bool is_job_orphaned = IsJobOrphaned(job);

  // Cache these values in case the job gets deleted.
  const SSLConfig used_ssl_config = job->server_ssl_config();
  const ProxyInfo used_proxy_info = job->proxy_info();
  const bool was_alpn_negotiated = job->was_alpn_negotiated();
  const NextProto negotiated_protocol = job->negotiated_protocol();
  const bool using_spdy = job->using_spdy();
  const NetLogSource source_dependency = job->net_log().source();

  // Cache this so we can still use it if the JobController is deleted.
  SpdySessionPool* spdy_session_pool = session_->spdy_session_pool();

  // Notify |request_|.
  if (!is_preconnect_ && !is_job_orphaned) {

    DCHECK(request_);

    // The first case is the usual case.
    if (!job_bound_) {
      BindJob(job);
    }

    MarkRequestComplete(was_alpn_negotiated, negotiated_protocol, using_spdy);

    if (is_websocket_) {
      // TODO(bnc): Re-instate this code when WebSockets over HTTP/2 is
      // implemented.  https://crbug.com/801564.
      NOTREACHED();
    } else if (job->stream_type() == HttpStreamRequest::BIDIRECTIONAL_STREAM) {
      std::unique_ptr<BidirectionalStreamImpl> bidirectional_stream_impl =
          job->ReleaseBidirectionalStream();
      DCHECK(bidirectional_stream_impl);
      delegate_->OnBidirectionalStreamImplReady(
          used_ssl_config, used_proxy_info,
          std::move(bidirectional_stream_impl));
    } else {
      std::unique_ptr<HttpStream> stream = job->ReleaseStream();
      DCHECK(stream);
      delegate_->OnStreamReady(used_ssl_config, used_proxy_info,
                               std::move(stream));
    }
  }

  // Notify other requests that have the same SpdySessionKey.
  // |request_| and |bound_job_| might be deleted already.
  if (spdy_session && spdy_session->IsAvailable()) {
    spdy_session_pool->OnNewSpdySessionReady(
        spdy_session, used_ssl_config, used_proxy_info, was_alpn_negotiated,
        negotiated_protocol, using_spdy, source_dependency);
  }
  if (is_job_orphaned) {
    OnOrphanedJobComplete(job);
  }
}

void HttpStreamFactory::JobController::OnPreconnectsComplete(Job* job) {
  DCHECK_EQ(main_job_.get(), job);
  main_job_.reset();
  ResetErrorStatusForJobs();
  factory_->OnPreconnectsCompleteInternal();
  MaybeNotifyFactoryOfCompletion();
}

void HttpStreamFactory::JobController::OnOrphanedJobComplete(const Job* job) {
  if (job->job_type() == MAIN) {
    DCHECK_EQ(main_job_.get(), job);
    main_job_.reset();
  } else {
    DCHECK_EQ(alternative_job_.get(), job);
    alternative_job_.reset();
  }

  MaybeNotifyFactoryOfCompletion();
}

void HttpStreamFactory::JobController::AddConnectionAttemptsToRequest(
    Job* job,
    const ConnectionAttempts& attempts) {
  if (is_preconnect_ || IsJobOrphaned(job))
    return;

  request_->AddConnectionAttempts(attempts);
}

void HttpStreamFactory::JobController::ResumeMainJobLater(
    const base::TimeDelta& delay) {
  net_log_.AddEvent(NetLogEventType::HTTP_STREAM_JOB_DELAYED,
                    NetLog::Int64Callback("delay", delay.InMilliseconds()));
  resume_main_job_callback_.Reset(
      base::BindOnce(&HttpStreamFactory::JobController::ResumeMainJob,
                     ptr_factory_.GetWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, resume_main_job_callback_.callback(), delay);
}

void HttpStreamFactory::JobController::ResumeMainJob() {
  DCHECK(main_job_);

  if (main_job_is_resumed_) {
    return;
  }
  main_job_is_resumed_ = true;
  main_job_->net_log().AddEvent(
      NetLogEventType::HTTP_STREAM_JOB_RESUMED,
      NetLog::Int64Callback("delay", main_job_wait_time_.InMilliseconds()));

  main_job_->Resume();
  main_job_wait_time_ = base::TimeDelta();
}

void HttpStreamFactory::JobController::ResetErrorStatusForJobs() {
  main_job_net_error_ = OK;
  alternative_job_net_error_ = OK;
  alternative_job_failed_on_default_network_ = false;
}

void HttpStreamFactory::JobController::MaybeResumeMainJob(
    Job* job,
    const base::TimeDelta& delay) {
  DCHECK(delay == base::TimeDelta() || delay == main_job_wait_time_);
  DCHECK(job == main_job_.get() || job == alternative_job_.get());

  if (job != alternative_job_.get() || !main_job_)
    return;

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
  if (job == alternative_job_.get())
    return false;

  if (main_job_is_blocked_)
    return true;

  if (main_job_wait_time_.is_zero())
    return false;

  ResumeMainJobLater(main_job_wait_time_);
  return true;
}

void HttpStreamFactory::JobController::SetSpdySessionKey(
    Job* job,
    const SpdySessionKey& spdy_session_key) {
  DCHECK(!job->using_quic());

  if (is_preconnect_ || IsJobOrphaned(job))
    return;

  session_->spdy_session_pool()->AddRequestToSpdySessionRequestMap(
      spdy_session_key, request_);
}

void HttpStreamFactory::JobController::
    RemoveRequestFromSpdySessionRequestMapForJob(Job* job) {
  DCHECK(!job->using_quic());

  if (is_preconnect_ || IsJobOrphaned(job))
    return;

  RemoveRequestFromSpdySessionRequestMap();
}

void HttpStreamFactory::JobController::
    RemoveRequestFromSpdySessionRequestMap() {
  DCHECK(request_);
  session_->spdy_session_pool()->RemoveRequestFromSpdySessionRequestMap(
      request_);
}

const NetLogWithSource* HttpStreamFactory::JobController::GetNetLog() const {
  return &net_log_;
}

void HttpStreamFactory::JobController::MaybeSetWaitTimeForMainJob(
    const base::TimeDelta& delay) {
  if (main_job_is_blocked_) {
    main_job_wait_time_ = std::min(
        delay, base::TimeDelta::FromSeconds(kMaxDelayTimeForMainJobSecs));
  }
}

bool HttpStreamFactory::JobController::HasPendingMainJob() const {
  return main_job_.get() != nullptr;
}

bool HttpStreamFactory::JobController::HasPendingAltJob() const {
  return alternative_job_.get() != nullptr;
}

size_t HttpStreamFactory::JobController::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(main_job_) +
         base::trace_event::EstimateMemoryUsage(alternative_job_);
}

WebSocketHandshakeStreamBase::CreateHelper*
HttpStreamFactory::JobController::websocket_handshake_stream_create_helper() {
  DCHECK(request_);
  return request_->websocket_handshake_stream_create_helper();
}

void HttpStreamFactory::JobController::OnIOComplete(int result) {
  RunLoop(result);
}

void HttpStreamFactory::JobController::RunLoop(int result) {
  int rv = DoLoop(result);
  if (rv == ERR_IO_PENDING)
    return;
  if (rv != OK) {
    // DoLoop can only fail during proxy resolution step which happens before
    // any jobs are created. Notify |request_| of the failure one message loop
    // iteration later to avoid re-entrancy.
    DCHECK(!main_job_);
    DCHECK(!alternative_job_);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&HttpStreamFactory::JobController::NotifyRequestFailed,
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
        break;
    }
  } while (next_state_ != STATE_NONE && rv != ERR_IO_PENDING);
  return rv;
}

int HttpStreamFactory::JobController::DoResolveProxy() {
  DCHECK(!proxy_resolve_request_);
  DCHECK(session_);

  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;

  if (request_info_.load_flags & LOAD_BYPASS_PROXY) {
    proxy_info_.UseDirect();
    return OK;
  }

  HostPortPair destination(HostPortPair::FromURL(request_info_.url));
  GURL origin_url = ApplyHostMappingRules(request_info_.url, &destination);

  CompletionOnceCallback io_callback =
      base::Bind(&JobController::OnIOComplete, base::Unretained(this));
  return session_->proxy_resolution_service()->ResolveProxy(
      origin_url, request_info_.method, &proxy_info_, std::move(io_callback),
      &proxy_resolve_request_, net_log_);
}

int HttpStreamFactory::JobController::DoResolveProxyComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);

  proxy_resolve_request_ = nullptr;
  net_log_.AddEvent(
      NetLogEventType::HTTP_STREAM_JOB_CONTROLLER_PROXY_SERVER_RESOLVED,
      base::Bind(
          &NetLogHttpStreamJobProxyServerResolved,
          proxy_info_.is_empty() ? ProxyServer() : proxy_info_.proxy_server()));

  if (rv != OK)
    return rv;
  // Remove unsupported proxies from the list.
  int supported_proxies = ProxyServer::SCHEME_DIRECT |
                          ProxyServer::SCHEME_HTTP | ProxyServer::SCHEME_HTTPS |
                          ProxyServer::SCHEME_SOCKS4 |
                          ProxyServer::SCHEME_SOCKS5;
  if (session_->IsQuicEnabled())
    supported_proxies |= ProxyServer::SCHEME_QUIC;
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

  HostPortPair destination(HostPortPair::FromURL(request_info_.url));
  GURL origin_url = ApplyHostMappingRules(request_info_.url, &destination);

  // Create an alternative job if alternative service is set up for this domain.
  alternative_service_info_ =
      GetAlternativeServiceInfoFor(request_info_, delegate_, stream_type_);
  quic::QuicTransportVersion quic_version = quic::QUIC_VERSION_UNSUPPORTED;
  if (alternative_service_info_.protocol() == kProtoQUIC) {
    quic_version =
        SelectQuicVersion(alternative_service_info_.advertised_versions());
    DCHECK_NE(quic_version, quic::QUIC_VERSION_UNSUPPORTED);
  }

  if (is_preconnect_) {
    // Due to how the socket pools handle priorities and idle sockets, only IDLE
    // priority currently makes sense for preconnects. The priority for
    // preconnects is currently ignored (see RequestSocketsForPool()), but could
    // be used at some point for proxy resolution or something.
    if (alternative_service_info_.protocol() != kProtoUnknown) {
      HostPortPair alternative_destination(
          alternative_service_info_.host_port_pair());
      ignore_result(
          ApplyHostMappingRules(request_info_.url, &alternative_destination));
      main_job_ = job_factory_->CreateAltSvcJob(
          this, PRECONNECT, session_, request_info_, IDLE, proxy_info_,
          server_ssl_config_, proxy_ssl_config_, alternative_destination,
          origin_url, alternative_service_info_.protocol(), quic_version,
          is_websocket_, enable_ip_based_pooling_, session_->net_log());
    } else {
      main_job_ = job_factory_->CreateMainJob(
          this, PRECONNECT, session_, request_info_, IDLE, proxy_info_,
          server_ssl_config_, proxy_ssl_config_, destination, origin_url,
          is_websocket_, enable_ip_based_pooling_, session_->net_log());
    }
    main_job_->Preconnect(num_streams_);
    return OK;
  }
  main_job_ = job_factory_->CreateMainJob(
      this, MAIN, session_, request_info_, priority_, proxy_info_,
      server_ssl_config_, proxy_ssl_config_, destination, origin_url,
      is_websocket_, enable_ip_based_pooling_, net_log_.net_log());
  // Alternative Service can only be set for HTTPS requests while Alternative
  // Proxy is set for HTTP requests.
  if (alternative_service_info_.protocol() != kProtoUnknown) {
    DCHECK(request_info_.url.SchemeIs(url::kHttpsScheme));
    DVLOG(1) << "Selected alternative service (host: "
             << alternative_service_info_.host_port_pair().host()
             << " port: " << alternative_service_info_.host_port_pair().port()
             << " version: " << quic_version << ")";

    HostPortPair alternative_destination(
        alternative_service_info_.host_port_pair());
    ignore_result(
        ApplyHostMappingRules(request_info_.url, &alternative_destination));
    alternative_job_ = job_factory_->CreateAltSvcJob(
        this, ALTERNATIVE, session_, request_info_, priority_, proxy_info_,
        server_ssl_config_, proxy_ssl_config_, alternative_destination,
        origin_url, alternative_service_info_.protocol(), quic_version,
        is_websocket_, enable_ip_based_pooling_, net_log_.net_log());

    main_job_is_blocked_ = true;
    alternative_job_->Start(request_->stream_type());
  } else {
    ProxyInfo alternative_proxy_info;
    if (ShouldCreateAlternativeProxyServerJob(proxy_info_, request_info_.url,
                                              &alternative_proxy_info)) {
      DCHECK(!main_job_is_blocked_);

      alternative_job_ = job_factory_->CreateAltProxyJob(
          this, ALTERNATIVE, session_, request_info_, priority_,
          alternative_proxy_info, server_ssl_config_, proxy_ssl_config_,
          destination, origin_url, alternative_proxy_info.proxy_server(),
          is_websocket_, enable_ip_based_pooling_, net_log_.net_log());

      can_start_alternative_proxy_job_ = false;
      main_job_is_blocked_ = true;
      alternative_job_->Start(request_->stream_type());
    }
  }
  // Even if |alternative_job| has already finished, it will not have notified
  // the request yet, since we defer that to the next iteration of the
  // MessageLoop, so starting |main_job_| is always safe.
  main_job_->Start(request_->stream_type());
  return OK;
}

void HttpStreamFactory::JobController::BindJob(Job* job) {
  DCHECK(request_);
  DCHECK(job);
  DCHECK(job == alternative_job_.get() || job == main_job_.get());
  DCHECK(!job_bound_);
  DCHECK(!bound_job_);

  job_bound_ = true;
  bound_job_ = job;

  request_->net_log().AddEvent(
      NetLogEventType::HTTP_STREAM_REQUEST_BOUND_TO_JOB,
      job->net_log().source().ToEventParametersCallback());
  job->net_log().AddEvent(
      NetLogEventType::HTTP_STREAM_JOB_BOUND_TO_REQUEST,
      request_->net_log().source().ToEventParametersCallback());

  OrphanUnboundJob();
}

void HttpStreamFactory::JobController::CancelJobs() {
  DCHECK(request_);
  if (job_bound_)
    return;
  if (alternative_job_)
    alternative_job_.reset();
  if (main_job_)
    main_job_.reset();
}

void HttpStreamFactory::JobController::OrphanUnboundJob() {
  DCHECK(request_);
  DCHECK(bound_job_);
  RemoveRequestFromSpdySessionRequestMap();

  if (bound_job_->job_type() == MAIN && alternative_job_) {
    DCHECK(!is_websocket_);
    // Allow |alternative_job_| to run to completion, rather than resetting it
    // to check if there is any broken alternative service to report.
    // OnOrphanedJobComplete() will clean up |this| when the job completes.
    alternative_job_->Orphan();
    return;
  }

  if (bound_job_->job_type() == ALTERNATIVE && main_job_ &&
      !alternative_job_failed_on_default_network_) {
    // |request_| is bound to the alternative job and the alternative job
    // succeeds on the default network. This means that the main job
    // is no longer needed, so cancel it now. Pending ConnectJobs will return
    // established sockets to socket pools if applicable.
    // https://crbug.com/757548.
    // The main job still needs to run if the alternative job succeeds on the
    // alternate network in order to figure out whether QUIC should be marked as
    // broken until the default network changes.
    DCHECK_EQ(OK, alternative_job_net_error_);
    main_job_.reset();
  }
}

void HttpStreamFactory::JobController::OnJobSucceeded(Job* job) {
  DCHECK(job);

  if (!bound_job_) {
    if (main_job_ && alternative_job_)
      ReportAlternateProtocolUsage(job);
    BindJob(job);
    return;
  }
  DCHECK(bound_job_);
}

void HttpStreamFactory::JobController::MarkRequestComplete(
    bool was_alpn_negotiated,
    NextProto negotiated_protocol,
    bool using_spdy) {
  if (request_)
    request_->Complete(was_alpn_negotiated, negotiated_protocol, using_spdy);
}

void HttpStreamFactory::JobController::OnAlternativeServiceJobFailed(
    int net_error) {
  DCHECK_EQ(alternative_job_->job_type(), ALTERNATIVE);
  DCHECK_NE(OK, net_error);
  DCHECK_NE(kProtoUnknown, alternative_service_info_.protocol());

  alternative_job_net_error_ = net_error;
}

void HttpStreamFactory::JobController::OnAlternativeProxyJobFailed(
    int net_error) {
  DCHECK_EQ(alternative_job_->job_type(), ALTERNATIVE);
  DCHECK_NE(OK, net_error);
  DCHECK(alternative_job_->alternative_proxy_server().is_valid());
  DCHECK(alternative_job_->alternative_proxy_server() ==
         alternative_job_->proxy_info().proxy_server());

  base::UmaHistogramSparse("Net.AlternativeProxyFailed", -net_error);

  // Need to mark alt proxy as broken regardless of whether the job is bound.
  // The proxy will be marked bad until the proxy retry information is cleared
  // by an event such as a network change.
  if (net_error != ERR_NETWORK_CHANGED &&
      net_error != ERR_INTERNET_DISCONNECTED) {
    session_->proxy_resolution_service()->MarkProxiesAsBadUntil(
        alternative_job_->proxy_info(), base::TimeDelta::Max(), {}, net_log_);
  }
}

void HttpStreamFactory::JobController::MaybeReportBrokenAlternativeService() {
  // If alternative job succeeds on the default network, no brokenness to
  // report.
  if (alternative_job_net_error_ == OK &&
      !alternative_job_failed_on_default_network_)
    return;

  // No brokenness to report if the main job fails.
  if (main_job_net_error_ != OK)
    return;

  DCHECK(alternative_service_info_.protocol() != kProtoUnknown);

  if (alternative_job_failed_on_default_network_ &&
      alternative_job_net_error_ == OK) {
    // Alternative job failed on the default network but succeeds on the
    // non-default network, mark alternative service broken until the default
    // network changes.
    session_->http_server_properties()
        ->MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
            alternative_service_info_.alternative_service());
    // Reset error status for Jobs after reporting brokenness.
    ResetErrorStatusForJobs();
    return;
  }

  // Report brokenness if alternative job failed.
  base::UmaHistogramSparse("Net.AlternateServiceFailed",
                           -alternative_job_net_error_);

  if (alternative_job_net_error_ == ERR_NETWORK_CHANGED ||
      alternative_job_net_error_ == ERR_INTERNET_DISCONNECTED) {
    // No need to mark alternative service as broken.
    // Reset error status for Jobs.
    ResetErrorStatusForJobs();
    return;
  }

  HistogramBrokenAlternateProtocolLocation(
      BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_JOB_ALT);
  session_->http_server_properties()->MarkAlternativeServiceBroken(
      alternative_service_info_.alternative_service());
  // Reset error status for Jobs after reporting brokenness.
  ResetErrorStatusForJobs();
}

void HttpStreamFactory::JobController::MaybeNotifyFactoryOfCompletion() {
  if (!main_job_ && !alternative_job_) {
    // Both jobs are gone, report brokenness if apply. Error status for Jobs
    // will be reset after reporting to avoid redundant reporting.
    MaybeReportBrokenAlternativeService();
  }

  if (!request_ && !main_job_ && !alternative_job_) {
    DCHECK(!bound_job_);
    factory_->OnJobControllerComplete(this);
  }
}

void HttpStreamFactory::JobController::NotifyRequestFailed(int rv) {
  if (!request_)
    return;
  delegate_->OnStreamFailed(rv, NetErrorDetails(), server_ssl_config_);
}

GURL HttpStreamFactory::JobController::ApplyHostMappingRules(
    const GURL& url,
    HostPortPair* endpoint) {
  if (session_->params().host_mapping_rules.RewriteHost(endpoint)) {
    url::Replacements<char> replacements;
    const std::string port_str = base::UintToString(endpoint->port());
    replacements.SetPort(port_str.c_str(), url::Component(0, port_str.size()));
    replacements.SetHost(endpoint->host().c_str(),
                         url::Component(0, endpoint->host().size()));
    return url.ReplaceComponents(replacements);
  }
  return url;
}

AlternativeServiceInfo
HttpStreamFactory::JobController::GetAlternativeServiceInfoFor(
    const HttpRequestInfo& request_info,
    HttpStreamRequest::Delegate* delegate,
    HttpStreamRequest::StreamType stream_type) {
  if (!enable_alternative_services_)
    return AlternativeServiceInfo();

  AlternativeServiceInfo alternative_service_info =
      GetAlternativeServiceInfoInternal(request_info, delegate, stream_type);
  AlternativeServiceType type;
  if (alternative_service_info.protocol() == kProtoUnknown) {
    type = NO_ALTERNATIVE_SERVICE;
  } else if (alternative_service_info.protocol() == kProtoQUIC) {
    if (request_info.url.host_piece() ==
        alternative_service_info.alternative_service().host) {
      type = QUIC_SAME_DESTINATION;
    } else {
      type = QUIC_DIFFERENT_DESTINATION;
    }
  } else {
    if (request_info.url.host_piece() ==
        alternative_service_info.alternative_service().host) {
      type = NOT_QUIC_SAME_DESTINATION;
    } else {
      type = NOT_QUIC_DIFFERENT_DESTINATION;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Net.AlternativeServiceTypeForRequest", type,
                            MAX_ALTERNATIVE_SERVICE_TYPE);
  return alternative_service_info;
}

AlternativeServiceInfo
HttpStreamFactory::JobController::GetAlternativeServiceInfoInternal(
    const HttpRequestInfo& request_info,
    HttpStreamRequest::Delegate* delegate,
    HttpStreamRequest::StreamType stream_type) {
  GURL original_url = request_info.url;

  if (!original_url.SchemeIs(url::kHttpsScheme))
    return AlternativeServiceInfo();

  url::SchemeHostPort origin(original_url);
  HttpServerProperties& http_server_properties =
      *session_->http_server_properties();
  const AlternativeServiceInfoVector alternative_service_info_vector =
      http_server_properties.GetAlternativeServiceInfos(origin);
  if (alternative_service_info_vector.empty())
    return AlternativeServiceInfo();

  bool quic_advertised = false;
  bool quic_all_broken = true;

  // First alternative service that is not marked as broken.
  AlternativeServiceInfo first_alternative_service_info;

  for (const AlternativeServiceInfo& alternative_service_info :
       alternative_service_info_vector) {
    DCHECK(IsAlternateProtocolValid(alternative_service_info.protocol()));
    if (!quic_advertised && alternative_service_info.protocol() == kProtoQUIC)
      quic_advertised = true;
    if (http_server_properties.IsAlternativeServiceBroken(
            alternative_service_info.alternative_service())) {
      HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_BROKEN, false);
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
         origin.port() < kUnrestrictedPort))
      continue;

    if (alternative_service_info.protocol() == kProtoHTTP2) {
      if (!session_->params().enable_http2_alternative_service)
        continue;

      // Cache this entry if we don't have a non-broken Alt-Svc yet.
      if (first_alternative_service_info.protocol() == kProtoUnknown)
        first_alternative_service_info = alternative_service_info;
      continue;
    }

    DCHECK_EQ(kProtoQUIC, alternative_service_info.protocol());
    quic_all_broken = false;
    if (!session_->IsQuicEnabled())
      continue;

    if (stream_type == HttpStreamRequest::BIDIRECTIONAL_STREAM &&
        session_->params().quic_disable_bidirectional_streams) {
      continue;
    }

    if (!original_url.SchemeIs(url::kHttpsScheme))
      continue;

    // If there is no QUIC version in the advertised versions that is
    // supported, ignore this entry.
    if (SelectQuicVersion(alternative_service_info.advertised_versions()) ==
        quic::QUIC_VERSION_UNSUPPORTED)
      continue;

    // Check whether there is an existing QUIC session to use for this origin.
    HostPortPair mapped_origin(origin.host(), origin.port());
    ignore_result(ApplyHostMappingRules(original_url, &mapped_origin));
    QuicSessionKey session_key(mapped_origin, request_info.privacy_mode,
                               request_info.socket_tag);

    HostPortPair destination(alternative_service_info.host_port_pair());
    if (session_key.host() != destination.host() &&
        !session_->params().quic_allow_remote_alt_svc) {
      continue;
    }
    ignore_result(ApplyHostMappingRules(original_url, &destination));

    if (session_->quic_stream_factory()->CanUseExistingSession(session_key,
                                                               destination))
      return alternative_service_info;

    if (!IsQuicWhitelistedForHost(destination.host()))
      continue;

    // Cache this entry if we don't have a non-broken Alt-Svc yet.
    if (first_alternative_service_info.protocol() == kProtoUnknown)
      first_alternative_service_info = alternative_service_info;
  }

  // Ask delegate to mark QUIC as broken for the origin.
  if (quic_advertised && quic_all_broken && delegate != nullptr)
    delegate->OnQuicBroken();

  return first_alternative_service_info;
}

quic::QuicTransportVersion HttpStreamFactory::JobController::SelectQuicVersion(
    const quic::QuicTransportVersionVector& advertised_versions) {
  const quic::QuicTransportVersionVector& supported_versions =
      session_->params().quic_supported_versions;
  if (advertised_versions.empty())
    return supported_versions[0];

  for (const quic::QuicTransportVersion& supported : supported_versions) {
    for (const quic::QuicTransportVersion& advertised : advertised_versions) {
      if (supported == advertised) {
        DCHECK_NE(quic::QUIC_VERSION_UNSUPPORTED, supported);
        return supported;
      }
    }
  }

  return quic::QUIC_VERSION_UNSUPPORTED;
}

bool HttpStreamFactory::JobController::ShouldCreateAlternativeProxyServerJob(
    const ProxyInfo& proxy_info,
    const GURL& url,
    ProxyInfo* alternative_proxy_info) const {
  DCHECK(alternative_proxy_info->is_empty());

  if (!enable_alternative_services_)
    return false;

  if (!can_start_alternative_proxy_job_) {
    // Either an alternative service job or an alternative proxy server job has
    // already been started.
    return false;
  }

  if (proxy_info.is_empty() || proxy_info.is_direct() || proxy_info.is_quic()) {
    // Alternative proxy server job can be created only if |job| fetches the
    // |request_| through a non-QUIC proxy.
    return false;
  }

  if (!url.SchemeIs(url::kHttpScheme)) {
    // Only HTTP URLs can be fetched through alternative proxy server, since the
    // alternative proxy server may not support fetching of URLs with other
    // schemes.
    return false;
  }

  alternative_proxy_info->UseProxyServer(proxy_info.alternative_proxy());
  if (alternative_proxy_info->is_empty())
    return false;

  DCHECK(alternative_proxy_info->proxy_server() != proxy_info.proxy_server());

  if (!alternative_proxy_info->is_https() &&
      !alternative_proxy_info->is_quic()) {
    // Alternative proxy server should be a secure server.
    return false;
  }

  if (alternative_proxy_info->is_quic()) {
    // Check that QUIC is enabled globally.
    if (!session_->IsQuicEnabled())
      return false;
  }

  return true;
}

void HttpStreamFactory::JobController::ReportAlternateProtocolUsage(
    Job* job) const {
  DCHECK(main_job_ && alternative_job_);

  bool proxy_server_used =
      alternative_job_->alternative_proxy_server().is_quic();

  if (job == main_job_.get()) {
    HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_LOST_RACE,
                                    proxy_server_used);
    return;
  }

  DCHECK_EQ(alternative_job_.get(), job);
  if (job->using_existing_quic_session()) {
    HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_NO_RACE,
                                    proxy_server_used);
    return;
  }

  HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_WON_RACE,
                                  proxy_server_used);
}

bool HttpStreamFactory::JobController::IsJobOrphaned(Job* job) const {
  return !request_ || (job_bound_ && bound_job_ != job);
}

int HttpStreamFactory::JobController::ReconsiderProxyAfterError(Job* job,
                                                                int error) {
  // ReconsiderProxyAfterError() should only be called when the last job fails.
  DCHECK(!(alternative_job_ && main_job_));
  DCHECK(!proxy_resolve_request_);
  DCHECK(session_);

  if (!job->should_reconsider_proxy())
    return error;

  DCHECK(!job->alternative_proxy_server().is_valid());

  if (request_info_.load_flags & LOAD_BYPASS_PROXY)
    return error;

  if (proxy_info_.is_https() && proxy_ssl_config_.send_client_cert) {
    session_->ssl_client_auth_cache()->Remove(
        proxy_info_.proxy_server().host_port_pair());
  }

  if (!proxy_info_.Fallback(error, net_log_)) {
    // If there is no more proxy to fallback to, fail the transaction
    // with the last connection error we got.
    return error;
  }

  if (!job->using_quic())
    RemoveRequestFromSpdySessionRequestMap();
  // Abandon all Jobs and start over.
  job_bound_ = false;
  bound_job_ = nullptr;
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

bool HttpStreamFactory::JobController::IsQuicWhitelistedForHost(
    const std::string& host) {
  const base::flat_set<std::string>& host_whitelist =
      session_->params().quic_host_whitelist;
  if (host_whitelist.empty())
    return true;

  std::string lowered_host = base::ToLowerASCII(host);
  return base::ContainsKey(host_whitelist, lowered_host);
}

}  // namespace net
