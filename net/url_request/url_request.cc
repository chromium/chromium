// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_delegate.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/socket/next_proto.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_manager.h"
#include "net/url_request/url_request_netlog_params.h"
#include "net/url_request/url_request_redirect_job.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

using base::Time;
using std::string;

namespace net {

namespace {

// TODO(battre): Delete this, see http://crbug.com/89321:
// This counter keeps track of the identifiers used for URL requests so far.
// 0 is reserved to represent an invalid ID.
uint64_t g_next_url_request_identifier = 1;

// This lock protects g_next_url_request_identifier.
base::LazyInstance<base::Lock>::Leaky
    g_next_url_request_identifier_lock = LAZY_INSTANCE_INITIALIZER;

// Returns an prior unused identifier for URL requests.
uint64_t GenerateURLRequestIdentifier() {
  base::AutoLock lock(g_next_url_request_identifier_lock.Get());
  return g_next_url_request_identifier++;
}

// True once the first URLRequest was started.
bool g_url_requests_started = false;

// True if cookies are accepted by default.
bool g_default_can_use_cookies = true;

// When the URLRequest first assempts load timing information, it has the times
// at which each event occurred.  The API requires the time which the request
// was blocked on each phase.  This function handles the conversion.
//
// In the case of reusing a SPDY session, old proxy results may have been
// reused, so proxy resolution times may be before the request was started.
//
// Due to preconnect and late binding, it is also possible for the connection
// attempt to start before a request has been started, or proxy resolution
// completed.
//
// This functions fixes both those cases.
void ConvertRealLoadTimesToBlockingTimes(LoadTimingInfo* load_timing_info) {
  DCHECK(!load_timing_info->request_start.is_null());

  // Earliest time possible for the request to be blocking on connect events.
  base::TimeTicks block_on_connect = load_timing_info->request_start;

  if (!load_timing_info->proxy_resolve_start.is_null()) {
    DCHECK(!load_timing_info->proxy_resolve_end.is_null());

    // Make sure the proxy times are after request start.
    if (load_timing_info->proxy_resolve_start < load_timing_info->request_start)
      load_timing_info->proxy_resolve_start = load_timing_info->request_start;
    if (load_timing_info->proxy_resolve_end < load_timing_info->request_start)
      load_timing_info->proxy_resolve_end = load_timing_info->request_start;

    // Connect times must also be after the proxy times.
    block_on_connect = load_timing_info->proxy_resolve_end;
  }

  // Make sure connection times are after start and proxy times.

  LoadTimingInfo::ConnectTiming* connect_timing =
      &load_timing_info->connect_timing;
  if (!connect_timing->dns_start.is_null()) {
    DCHECK(!connect_timing->dns_end.is_null());
    if (connect_timing->dns_start < block_on_connect)
      connect_timing->dns_start = block_on_connect;
    if (connect_timing->dns_end < block_on_connect)
      connect_timing->dns_end = block_on_connect;
  }

  if (!connect_timing->connect_start.is_null()) {
    DCHECK(!connect_timing->connect_end.is_null());
    if (connect_timing->connect_start < block_on_connect)
      connect_timing->connect_start = block_on_connect;
    if (connect_timing->connect_end < block_on_connect)
      connect_timing->connect_end = block_on_connect;
  }

  if (!connect_timing->ssl_start.is_null()) {
    DCHECK(!connect_timing->ssl_end.is_null());
    if (connect_timing->ssl_start < block_on_connect)
      connect_timing->ssl_start = block_on_connect;
    if (connect_timing->ssl_end < block_on_connect)
      connect_timing->ssl_end = block_on_connect;
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// URLRequest::Delegate

void URLRequest::Delegate::OnReceivedRedirect(URLRequest* request,
                                              const RedirectInfo& redirect_info,
                                              bool* defer_redirect) {
}

void URLRequest::Delegate::OnAuthRequired(URLRequest* request,
                                          AuthChallengeInfo* auth_info) {
  request->CancelAuth();
}

void URLRequest::Delegate::OnCertificateRequested(
    URLRequest* request,
    SSLCertRequestInfo* cert_request_info) {
  request->CancelWithError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void URLRequest::Delegate::OnSSLCertificateError(URLRequest* request,
                                                 const SSLInfo& ssl_info,
                                                 bool is_hsts_ok) {
  request->Cancel();
}

void URLRequest::Delegate::OnResponseStarted(URLRequest* request,
                                             int net_error) {
  NOTREACHED();
}

///////////////////////////////////////////////////////////////////////////////
// URLRequest

URLRequest::~URLRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Log the redirect count during destruction, to ensure that it is only
  // recorded at the end of following all redirect chains.
  UMA_HISTOGRAM_EXACT_LINEAR("Net.RedirectChainLength",
                             kMaxRedirects - redirect_limit_,
                             kMaxRedirects + 1);

  Cancel();

  if (network_delegate_) {
    network_delegate_->NotifyURLRequestDestroyed(this);
    if (job_.get())
      job_->NotifyURLRequestDestroyed();
  }

  // Delete job before |this|, since subclasses may do weird things, like depend
  // on UserData associated with |this| and poke at it during teardown.
  job_.reset();

  DCHECK_EQ(1u, context_->url_requests()->count(this));
  context_->url_requests()->erase(this);

  int net_error = OK;
  // Log error only on failure, not cancellation, as even successful requests
  // are "cancelled" on destruction.
  if (status_.status() == URLRequestStatus::FAILED)
    net_error = status_.error();
  net_log_.EndEventWithNetErrorCode(NetLogEventType::REQUEST_ALIVE, net_error);
}

void URLRequest::set_upload(std::unique_ptr<UploadDataStream> upload) {
  upload_data_stream_ = std::move(upload);
}

const UploadDataStream* URLRequest::get_upload() const {
  return upload_data_stream_.get();
}

bool URLRequest::has_upload() const {
  return upload_data_stream_.get() != NULL;
}

void URLRequest::SetExtraRequestHeaderByName(const string& name,
                                             const string& value,
                                             bool overwrite) {
  DCHECK(!is_pending_ || is_redirecting_);
  if (overwrite) {
    extra_request_headers_.SetHeader(name, value);
  } else {
    extra_request_headers_.SetHeaderIfMissing(name, value);
  }
}

void URLRequest::RemoveRequestHeaderByName(const string& name) {
  DCHECK(!is_pending_ || is_redirecting_);
  extra_request_headers_.RemoveHeader(name);
}

void URLRequest::SetExtraRequestHeaders(
    const HttpRequestHeaders& headers) {
  DCHECK(!is_pending_);
  extra_request_headers_ = headers;

  // NOTE: This method will likely become non-trivial once the other setters
  // for request headers are implemented.
}

bool URLRequest::GetFullRequestHeaders(HttpRequestHeaders* headers) const {
  if (!job_.get())
    return false;

  return job_->GetFullRequestHeaders(headers);
}

int64_t URLRequest::GetTotalReceivedBytes() const {
  if (!job_.get())
    return 0;

  return job_->GetTotalReceivedBytes();
}

int64_t URLRequest::GetTotalSentBytes() const {
  if (!job_.get())
    return 0;

  return job_->GetTotalSentBytes();
}

int64_t URLRequest::GetRawBodyBytes() const {
  if (!job_.get())
    return 0;

  return job_->prefilter_bytes_read();
}

LoadStateWithParam URLRequest::GetLoadState() const {
  // The !blocked_by_.empty() check allows |this| to report it's blocked on a
  // delegate before it has been started.
  if (calling_delegate_ || !blocked_by_.empty()) {
    return LoadStateWithParam(
        LOAD_STATE_WAITING_FOR_DELEGATE,
        use_blocked_by_as_load_param_ ? base::UTF8ToUTF16(blocked_by_) :
                                        base::string16());
  }
  return LoadStateWithParam(job_.get() ? job_->GetLoadState() : LOAD_STATE_IDLE,
                            base::string16());
}

std::unique_ptr<base::Value> URLRequest::GetStateAsValue() const {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("url", original_url().possibly_invalid_spec());

  if (url_chain_.size() > 1) {
    std::unique_ptr<base::ListValue> list(new base::ListValue());
    for (const GURL& url : url_chain_) {
      list->AppendString(url.possibly_invalid_spec());
    }
    dict->Set("url_chain", std::move(list));
  }

  dict->SetInteger("load_flags", load_flags_);

  LoadStateWithParam load_state = GetLoadState();
  dict->SetInteger("load_state", load_state.state);
  if (!load_state.param.empty())
    dict->SetString("load_state_param", load_state.param);
  if (!blocked_by_.empty())
    dict->SetString("delegate_blocked_by", blocked_by_);

  dict->SetString("method", method_);
  dict->SetBoolean("has_upload", has_upload());
  dict->SetBoolean("is_pending", is_pending_);

  // Add the status of the request.  The status should always be IO_PENDING, and
  // the error should always be OK, unless something is holding onto a request
  // that has finished or a request was leaked.  Neither of these should happen.
  switch (status_.status()) {
    case URLRequestStatus::SUCCESS:
      dict->SetString("status", "SUCCESS");
      break;
    case URLRequestStatus::IO_PENDING:
      dict->SetString("status", "IO_PENDING");
      break;
    case URLRequestStatus::CANCELED:
      dict->SetString("status", "CANCELED");
      break;
    case URLRequestStatus::FAILED:
      dict->SetString("status", "FAILED");
      break;
  }
  if (status_.error() != OK)
    dict->SetInteger("net_error", status_.error());
  return std::move(dict);
}

void URLRequest::LogBlockedBy(const char* blocked_by) {
  DCHECK(blocked_by);
  DCHECK_GT(strlen(blocked_by), 0u);

  // Only log information to NetLog during startup and certain deferring calls
  // to delegates.  For all reads but the first, do nothing.
  if (!calling_delegate_ && !response_info_.request_time.is_null())
    return;

  LogUnblocked();
  blocked_by_ = blocked_by;
  use_blocked_by_as_load_param_ = false;

  net_log_.BeginEvent(
      NetLogEventType::DELEGATE_INFO,
      NetLog::StringCallback("delegate_blocked_by", &blocked_by_));
}

void URLRequest::LogAndReportBlockedBy(const char* source) {
  LogBlockedBy(source);
  use_blocked_by_as_load_param_ = true;
}

void URLRequest::LogUnblocked() {
  if (blocked_by_.empty())
    return;

  net_log_.EndEvent(NetLogEventType::DELEGATE_INFO);
  blocked_by_.clear();
}

UploadProgress URLRequest::GetUploadProgress() const {
  if (!job_.get()) {
    // We haven't started or the request was cancelled
    return UploadProgress();
  }

  if (final_upload_progress_.position()) {
    // The first job completed and none of the subsequent series of
    // GETs when following redirects will upload anything, so we return the
    // cached results from the initial job, the POST.
    return final_upload_progress_;
  }

  if (upload_data_stream_)
    return upload_data_stream_->GetUploadProgress();

  return UploadProgress();
}

void URLRequest::GetResponseHeaderByName(const string& name,
                                         string* value) const {
  DCHECK(value);
  if (response_info_.headers.get()) {
    response_info_.headers->GetNormalizedHeader(name, value);
  } else {
    value->clear();
  }
}

HostPortPair URLRequest::GetSocketAddress() const {
  DCHECK(job_.get());
  return job_->GetSocketAddress();
}

HttpResponseHeaders* URLRequest::response_headers() const {
  return response_info_.headers.get();
}

void URLRequest::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  *load_timing_info = load_timing_info_;
}

void URLRequest::PopulateNetErrorDetails(NetErrorDetails* details) const {
  if (!job_)
    return;
  return job_->PopulateNetErrorDetails(details);
}

bool URLRequest::GetRemoteEndpoint(IPEndPoint* endpoint) const {
  if (!job_)
    return false;

  return job_->GetRemoteEndpoint(endpoint);
}

void URLRequest::GetMimeType(string* mime_type) const {
  DCHECK(job_.get());
  job_->GetMimeType(mime_type);
}

void URLRequest::GetCharset(string* charset) const {
  DCHECK(job_.get());
  job_->GetCharset(charset);
}

int URLRequest::GetResponseCode() const {
  DCHECK(job_.get());
  return job_->GetResponseCode();
}

void URLRequest::SetLoadFlags(int flags) {
  if ((load_flags_ & LOAD_IGNORE_LIMITS) != (flags & LOAD_IGNORE_LIMITS)) {
    DCHECK(!job_.get());
    DCHECK(flags & LOAD_IGNORE_LIMITS);
    DCHECK_EQ(priority_, MAXIMUM_PRIORITY);
  }
  load_flags_ = flags;

  // This should be a no-op given the above DCHECKs, but do this
  // anyway for release mode.
  if ((load_flags_ & LOAD_IGNORE_LIMITS) != 0)
    SetPriority(MAXIMUM_PRIORITY);
}

// static
void URLRequest::SetDefaultCookiePolicyToBlock() {
  CHECK(!g_url_requests_started);
  g_default_can_use_cookies = false;
}

// static
bool URLRequest::IsHandledProtocol(const std::string& scheme) {
  return URLRequestJobManager::SupportsScheme(scheme);
}

// static
bool URLRequest::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  return IsHandledProtocol(url.scheme());
}

void URLRequest::set_site_for_cookies(const GURL& site_for_cookies) {
  DCHECK(!is_pending_);
  site_for_cookies_ = site_for_cookies;
}

void URLRequest::set_first_party_url_policy(
    FirstPartyURLPolicy first_party_url_policy) {
  DCHECK(!is_pending_);
  first_party_url_policy_ = first_party_url_policy;
}

void URLRequest::set_initiator(const base::Optional<url::Origin>& initiator) {
  DCHECK(!is_pending_);
  DCHECK(!initiator.has_value() || initiator.value().opaque() ||
         initiator.value().GetURL().is_valid());
  initiator_ = initiator;
}

void URLRequest::set_method(const std::string& method) {
  DCHECK(!is_pending_);
  method_ = method;
}

void URLRequest::SetReferrer(const std::string& referrer) {
  DCHECK(!is_pending_);
  GURL referrer_url(referrer);
  if (referrer_url.is_valid()) {
    referrer_ = referrer_url.GetAsReferrer().spec();
  } else {
    referrer_ = referrer;
  }
}

void URLRequest::set_referrer_policy(ReferrerPolicy referrer_policy) {
  DCHECK(!is_pending_);
  referrer_policy_ = referrer_policy;
}

void URLRequest::set_delegate(Delegate* delegate) {
  DCHECK(!delegate_);
  DCHECK(delegate);
  delegate_ = delegate;
}

void URLRequest::set_allow_credentials(bool allow_credentials) {
  if (allow_credentials) {
    load_flags_ &= ~(LOAD_DO_NOT_SAVE_COOKIES | LOAD_DO_NOT_SEND_COOKIES |
                     LOAD_DO_NOT_SEND_AUTH_DATA);
  } else {
    load_flags_ |= (LOAD_DO_NOT_SAVE_COOKIES | LOAD_DO_NOT_SEND_COOKIES |
                    LOAD_DO_NOT_SEND_AUTH_DATA);
  }
}

void URLRequest::Start() {
  DCHECK(delegate_);

  if (!status_.is_success())
    return;

  // Some values can be NULL, but the job factory must not be.
  DCHECK(context_->job_factory());

  // Anything that sets |blocked_by_| before start should have cleaned up after
  // itself.
  DCHECK(blocked_by_.empty());

  g_url_requests_started = true;
  response_info_.request_time = base::Time::Now();

  load_timing_info_ = LoadTimingInfo();
  load_timing_info_.request_start_time = response_info_.request_time;
  load_timing_info_.request_start = base::TimeTicks::Now();

  if (network_delegate_) {
    OnCallToDelegate(NetLogEventType::NETWORK_DELEGATE_BEFORE_URL_REQUEST);
    int error = network_delegate_->NotifyBeforeURLRequest(
        this,
        base::BindOnce(&URLRequest::BeforeRequestComplete,
                       base::Unretained(this)),
        &delegate_redirect_url_);
    // If ERR_IO_PENDING is returned, the delegate will invoke
    // |BeforeRequestComplete| later.
    if (error != ERR_IO_PENDING)
      BeforeRequestComplete(error);
    return;
  }

  StartJob(URLRequestJobManager::GetInstance()->CreateJob(
      this, network_delegate_));
}

///////////////////////////////////////////////////////////////////////////////

URLRequest::URLRequest(const GURL& url,
                       RequestPriority priority,
                       Delegate* delegate,
                       const URLRequestContext* context,
                       NetworkDelegate* network_delegate,
                       NetworkTrafficAnnotationTag traffic_annotation)
    : context_(context),
      network_delegate_(network_delegate ? network_delegate
                                         : context->network_delegate()),
      net_log_(NetLogWithSource::Make(context->net_log(),
                                      NetLogSourceType::URL_REQUEST)),
      url_chain_(1, url),
      attach_same_site_cookies_(false),
      method_("GET"),
      referrer_policy_(CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE),
      first_party_url_policy_(NEVER_CHANGE_FIRST_PARTY_URL),
      load_flags_(LOAD_NORMAL),
      delegate_(delegate),
      status_(URLRequestStatus::FromError(OK)),
      is_pending_(false),
      is_redirecting_(false),
      redirect_limit_(kMaxRedirects),
      priority_(priority),
      identifier_(GenerateURLRequestIdentifier()),
      delegate_event_type_(NetLogEventType::FAILED),
      calling_delegate_(false),
      use_blocked_by_as_load_param_(false),
      has_notified_completion_(false),
      received_response_content_length_(0),
      creation_time_(base::TimeTicks::Now()),
      raw_header_size_(0),
      is_pac_request_(false),
      traffic_annotation_(traffic_annotation),
      upgrade_if_insecure_(false),
      weak_factory_(this) {
  // Sanity check out environment.
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  context->url_requests()->insert(this);
  net_log_.BeginEvent(
      NetLogEventType::REQUEST_ALIVE,
      base::Bind(&NetLogURLRequestConstructorCallback, &url, priority_));
}

void URLRequest::BeforeRequestComplete(int error) {
  DCHECK(!job_.get());
  DCHECK_NE(ERR_IO_PENDING, error);

  // Check that there are no callbacks to already canceled requests.
  DCHECK_NE(URLRequestStatus::CANCELED, status_.status());

  OnCallToDelegateComplete();

  if (error != OK) {
    std::string source("delegate");
    net_log_.AddEvent(NetLogEventType::CANCELLED,
                      NetLog::StringCallback("source", &source));
    StartJob(new URLRequestErrorJob(this, network_delegate_, error));
  } else if (!delegate_redirect_url_.is_empty()) {
    GURL new_url;
    new_url.Swap(&delegate_redirect_url_);

    URLRequestRedirectJob* job = new URLRequestRedirectJob(
        this, network_delegate_, new_url,
        // Use status code 307 to preserve the method, so POST requests work.
        URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT, "Delegate");
    StartJob(job);
  } else {
    StartJob(URLRequestJobManager::GetInstance()->CreateJob(
        this, network_delegate_));
  }
}

void URLRequest::StartJob(URLRequestJob* job) {
  DCHECK(!is_pending_);
  DCHECK(!job_.get());

  net_log_.BeginEvent(
      NetLogEventType::URL_REQUEST_START_JOB,
      base::Bind(&NetLogURLRequestStartCallback, &url(), &method_, load_flags_,
                 upload_data_stream_ ? upload_data_stream_->identifier() : -1));

  job_.reset(job);
  job_->SetExtraRequestHeaders(extra_request_headers_);
  job_->SetPriority(priority_);
  job_->SetRequestHeadersCallback(request_headers_callback_);
  job_->SetResponseHeadersCallback(response_headers_callback_);

  if (upload_data_stream_.get())
    job_->SetUpload(upload_data_stream_.get());

  is_pending_ = true;
  is_redirecting_ = false;

  response_info_.was_cached = false;

  GURL referrer_url(referrer_);
  if (referrer_url != URLRequestJob::ComputeReferrerForPolicy(
                          referrer_policy_, referrer_url, url())) {
    if (!network_delegate_ ||
        !network_delegate_->CancelURLRequestWithPolicyViolatingReferrerHeader(
            *this, url(), referrer_url)) {
      referrer_.clear();
    } else {
      // We need to clear the referrer anyway to avoid an infinite recursion
      // when starting the error job.
      referrer_.clear();
      std::string source("delegate");
      net_log_.AddEvent(NetLogEventType::CANCELLED,
                        NetLog::StringCallback("source", &source));
      RestartWithJob(new URLRequestErrorJob(
          this, network_delegate_, ERR_BLOCKED_BY_CLIENT));
      return;
    }
  }

  // Start() always completes asynchronously.
  //
  // Status is generally set by URLRequestJob itself, but Start() calls
  // directly into the URLRequestJob subclass, so URLRequestJob can't set it
  // here.
  // TODO(mmenke):  Make the URLRequest manage its own status.
  status_ = URLRequestStatus::FromError(ERR_IO_PENDING);
  job_->Start();
}

void URLRequest::Restart() {
  // Should only be called if the original job didn't make any progress.
  DCHECK(job_.get() && !job_->has_response_started());
  RestartWithJob(
      URLRequestJobManager::GetInstance()->CreateJob(this, network_delegate_));
}

void URLRequest::RestartWithJob(URLRequestJob *job) {
  DCHECK(job->request() == this);
  PrepareToRestart();
  StartJob(job);
}

int URLRequest::Cancel() {
  return DoCancel(ERR_ABORTED, SSLInfo());
}

int URLRequest::CancelWithError(int error) {
  return DoCancel(error, SSLInfo());
}

void URLRequest::CancelWithSSLError(int error, const SSLInfo& ssl_info) {
  // This should only be called on a started request.
  if (!is_pending_ || !job_.get() || job_->has_response_started()) {
    NOTREACHED();
    return;
  }
  DoCancel(error, ssl_info);
}

int URLRequest::DoCancel(int error, const SSLInfo& ssl_info) {
  DCHECK_LT(error, 0);
  // If cancelled while calling a delegate, clear delegate info.
  if (calling_delegate_) {
    LogUnblocked();
    OnCallToDelegateComplete();
  }

  // If the URL request already has an error status, then canceling is a no-op.
  // Plus, we don't want to change the error status once it has been set.
  if (status_.is_success()) {
    status_ = URLRequestStatus(URLRequestStatus::CANCELED, error);
    response_info_.ssl_info = ssl_info;

    // If the request hasn't already been completed, log a cancellation event.
    if (!has_notified_completion_) {
      // Don't log an error code on ERR_ABORTED, since that's redundant.
      net_log_.AddEventWithNetErrorCode(NetLogEventType::CANCELLED,
                                        error == ERR_ABORTED ? OK : error);
    }
  }

  if (is_pending_ && job_.get())
    job_->Kill();

  // We need to notify about the end of this job here synchronously. The
  // Job sends an asynchronous notification but by the time this is processed,
  // our |context_| is NULL.
  NotifyRequestCompleted();

  // The Job will call our NotifyDone method asynchronously.  This is done so
  // that the Delegate implementation can call Cancel without having to worry
  // about being called recursively.

  return status_.error();
}

int URLRequest::Read(IOBuffer* dest, int dest_size) {
  DCHECK(job_.get());

  // If this is the first read, end the delegate call that may have started in
  // OnResponseStarted.
  OnCallToDelegateComplete();

  // If the request has failed, Read() will return actual network error code.
  if (!status_.is_success())
    return status_.error();

  // This handles reads after the request already completed successfully.
  // TODO(ahendrickson): DCHECK() that it is not done after
  // http://crbug.com/115705 is fixed.
  if (job_->is_done())
    return status_.error();

  if (dest_size == 0) {
    // Caller is not too bright.  I guess we've done what they asked.
    return OK;
  }

  int rv = job_->Read(dest, dest_size);
  if (rv == ERR_IO_PENDING) {
    set_status(URLRequestStatus::FromError(ERR_IO_PENDING));
  } else if (rv <= 0) {
    NotifyRequestCompleted();
  }

  // If rv is not 0 or actual bytes read, the status cannot be success.
  DCHECK(rv >= 0 || status_.status() != URLRequestStatus::SUCCESS);
  return rv;
}

// Deprecated.
bool URLRequest::Read(IOBuffer* dest, int dest_size, int* bytes_read) {
  int result = Read(dest, dest_size);
  if (result >= 0) {
    *bytes_read = result;
    return true;
  }

  if (result == ERR_IO_PENDING) {
    *bytes_read = 0;
  } else {
    *bytes_read = -1;
  }

  return false;
}

void URLRequest::StopCaching() {
  DCHECK(job_.get());
  job_->StopCaching();
}

void URLRequest::NotifyReceivedRedirect(const RedirectInfo& redirect_info,
                                        bool* defer_redirect) {
  is_redirecting_ = true;

  URLRequestJob* job =
      URLRequestJobManager::GetInstance()->MaybeInterceptRedirect(
          this, network_delegate_, redirect_info.new_url);
  if (job) {
    RestartWithJob(job);
  } else {
    OnCallToDelegate(NetLogEventType::URL_REQUEST_DELEGATE_RECEIVED_REDIRECT);
    delegate_->OnReceivedRedirect(this, redirect_info, defer_redirect);
    // |this| may be have been destroyed here.
  }
}

void URLRequest::NotifyResponseStarted(const URLRequestStatus& status) {
  // Change status if there was an error.
  if (status.status() != URLRequestStatus::SUCCESS)
    set_status(status);

  int net_error = OK;
  if (!status_.is_success())
    net_error = status_.error();
  net_log_.EndEventWithNetErrorCode(NetLogEventType::URL_REQUEST_START_JOB,
                                    net_error);

  URLRequestJob* job =
      URLRequestJobManager::GetInstance()->MaybeInterceptResponse(
          this, network_delegate_);
  if (job) {
    RestartWithJob(job);
  } else {
    // In some cases (e.g. an event was canceled), we might have sent the
    // completion event and receive a NotifyResponseStarted() later.
    if (!has_notified_completion_ && status_.is_success()) {
      if (network_delegate_)
        network_delegate_->NotifyResponseStarted(this, net_error);
    }

    // Notify in case the entire URL Request has been finished.
    if (!has_notified_completion_ && !status_.is_success())
      NotifyRequestCompleted();

    OnCallToDelegate(NetLogEventType::URL_REQUEST_DELEGATE_RESPONSE_STARTED);
    delegate_->OnResponseStarted(this, net_error);
    // Nothing may appear below this line as OnResponseStarted may delete
    // |this|.
  }
}

void URLRequest::FollowDeferredRedirect(
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  DCHECK(job_.get());
  DCHECK(status_.is_success());

  status_ = URLRequestStatus::FromError(ERR_IO_PENDING);
  job_->FollowDeferredRedirect(modified_request_headers);
}

void URLRequest::SetAuth(const AuthCredentials& credentials) {
  DCHECK(job_.get());
  DCHECK(job_->NeedsAuth());

  status_ = URLRequestStatus::FromError(ERR_IO_PENDING);
  job_->SetAuth(credentials);
}

void URLRequest::CancelAuth() {
  DCHECK(job_.get());
  DCHECK(job_->NeedsAuth());

  status_ = URLRequestStatus::FromError(ERR_IO_PENDING);
  job_->CancelAuth();
}

void URLRequest::ContinueWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key) {
  DCHECK(job_.get());

  // Matches the call in NotifyCertificateRequested.
  OnCallToDelegateComplete();

  status_ = URLRequestStatus::FromError(ERR_IO_PENDING);
  job_->ContinueWithCertificate(std::move(client_cert),
                                std::move(client_private_key));
}

void URLRequest::ContinueDespiteLastError() {
  DCHECK(job_.get());

  // Matches the call in NotifySSLCertificateError.
  OnCallToDelegateComplete();

  status_ = URLRequestStatus::FromError(ERR_IO_PENDING);
  job_->ContinueDespiteLastError();
}

void URLRequest::PrepareToRestart() {
  DCHECK(job_.get());

  // Close the current URL_REQUEST_START_JOB, since we will be starting a new
  // one.
  net_log_.EndEvent(NetLogEventType::URL_REQUEST_START_JOB);

  job_.reset();

  response_info_ = HttpResponseInfo();
  response_info_.request_time = base::Time::Now();

  load_timing_info_ = LoadTimingInfo();
  load_timing_info_.request_start_time = response_info_.request_time;
  load_timing_info_.request_start = base::TimeTicks::Now();

  status_ = URLRequestStatus();
  is_pending_ = false;
  proxy_server_ = ProxyServer();
}

void URLRequest::Redirect(
    const RedirectInfo& redirect_info,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  // This method always succeeds. Whether |job_| is allowed to redirect to
  // |redirect_info| is checked in URLRequestJob::CanFollowRedirect, before
  // NotifyReceivedRedirect. This means the delegate can assume that, if it
  // accepted the redirect, future calls to OnResponseStarted correspond to
  // |redirect_info.new_url|.
  OnCallToDelegateComplete();
  if (net_log_.IsCapturing()) {
    net_log_.AddEvent(
        NetLogEventType::URL_REQUEST_REDIRECTED,
        NetLog::StringCallback("location",
                               &redirect_info.new_url.possibly_invalid_spec()));
  }

  if (network_delegate_)
    network_delegate_->NotifyBeforeRedirect(this, redirect_info.new_url);

  if (!final_upload_progress_.position() && upload_data_stream_)
    final_upload_progress_ = upload_data_stream_->GetUploadProgress();
  PrepareToRestart();

  bool clear_body = false;
  net::RedirectUtil::UpdateHttpRequest(url(), method_, redirect_info,
                                       modified_request_headers,
                                       &extra_request_headers_, &clear_body);
  if (clear_body)
    upload_data_stream_.reset();

  method_ = redirect_info.new_method;
  referrer_ = redirect_info.new_referrer;
  referrer_policy_ = redirect_info.new_referrer_policy;
  site_for_cookies_ = redirect_info.new_site_for_cookies;

  url_chain_.push_back(redirect_info.new_url);
  --redirect_limit_;

  Start();
}

const URLRequestContext* URLRequest::context() const {
  return context_;
}

int64_t URLRequest::GetExpectedContentSize() const {
  int64_t expected_content_size = -1;
  if (job_.get())
    expected_content_size = job_->expected_content_size();

  return expected_content_size;
}

void URLRequest::SetPriority(RequestPriority priority) {
  DCHECK_GE(priority, MINIMUM_PRIORITY);
  DCHECK_LE(priority, MAXIMUM_PRIORITY);

  if ((load_flags_ & LOAD_IGNORE_LIMITS) && (priority != MAXIMUM_PRIORITY)) {
    NOTREACHED();
    // Maintain the invariant that requests with IGNORE_LIMITS set
    // have MAXIMUM_PRIORITY for release mode.
    return;
  }

  if (priority_ == priority)
    return;

  priority_ = priority;
  net_log_.AddEvent(
      NetLogEventType::URL_REQUEST_SET_PRIORITY,
      NetLog::StringCallback("priority", RequestPriorityToString(priority_)));
  if (job_.get())
    job_->SetPriority(priority_);
}

void URLRequest::NotifyAuthRequired(AuthChallengeInfo* auth_info) {
  NetworkDelegate::AuthRequiredResponse rv =
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  auth_info_ = auth_info;
  if (network_delegate_) {
    OnCallToDelegate(NetLogEventType::NETWORK_DELEGATE_AUTH_REQUIRED);
    rv = network_delegate_->NotifyAuthRequired(
        this, *auth_info,
        base::BindOnce(&URLRequest::NotifyAuthRequiredComplete,
                       base::Unretained(this)),
        &auth_credentials_);
    if (rv == NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING)
      return;
  }

  NotifyAuthRequiredComplete(rv);
}

void URLRequest::NotifyAuthRequiredComplete(
    NetworkDelegate::AuthRequiredResponse result) {
  OnCallToDelegateComplete();

  // Check that there are no callbacks to already canceled requests.
  DCHECK_NE(URLRequestStatus::CANCELED, status_.status());

  // NotifyAuthRequired may be called multiple times, such as
  // when an authentication attempt fails. Clear out the data
  // so it can be reset on another round.
  AuthCredentials credentials = auth_credentials_;
  auth_credentials_ = AuthCredentials();
  scoped_refptr<AuthChallengeInfo> auth_info;
  auth_info.swap(auth_info_);

  switch (result) {
    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION:
      // Defer to the URLRequest::Delegate, since the NetworkDelegate
      // didn't take an action.
      delegate_->OnAuthRequired(this, auth_info.get());
      break;

    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH:
      SetAuth(credentials);
      break;

    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH:
      CancelAuth();
      break;

    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING:
      NOTREACHED();
      break;
  }
}

void URLRequest::NotifyCertificateRequested(
    SSLCertRequestInfo* cert_request_info) {
  status_ = URLRequestStatus();
  OnCallToDelegate(NetLogEventType::URL_REQUEST_DELEGATE_CERTIFICATE_REQUESTED);
  delegate_->OnCertificateRequested(this, cert_request_info);
}

void URLRequest::NotifySSLCertificateError(const SSLInfo& ssl_info,
                                           bool fatal) {
  status_ = URLRequestStatus();
  OnCallToDelegate(NetLogEventType::URL_REQUEST_DELEGATE_SSL_CERTIFICATE_ERROR);
  delegate_->OnSSLCertificateError(this, ssl_info, fatal);
}

bool URLRequest::CanGetCookies(const CookieList& cookie_list) const {
  DCHECK(!(load_flags_ & LOAD_DO_NOT_SEND_COOKIES));
  bool can_get_cookies = g_default_can_use_cookies;
  if (network_delegate_) {
    can_get_cookies =
        network_delegate_->CanGetCookies(*this, cookie_list,
                                         /*allowed_from_caller=*/true);
  }

  if (!can_get_cookies)
    net_log_.AddEvent(NetLogEventType::COOKIE_GET_BLOCKED_BY_NETWORK_DELEGATE);
  return can_get_cookies;
}

bool URLRequest::CanSetCookie(const net::CanonicalCookie& cookie,
                              CookieOptions* options) const {
  DCHECK(!(load_flags_ & LOAD_DO_NOT_SAVE_COOKIES));
  bool can_set_cookies = g_default_can_use_cookies;
  if (network_delegate_) {
    can_set_cookies =
        network_delegate_->CanSetCookie(*this, cookie, options,
                                        /*allowed_from_caller=*/true);
  }
  if (!can_set_cookies)
    net_log_.AddEvent(NetLogEventType::COOKIE_SET_BLOCKED_BY_NETWORK_DELEGATE);
  return can_set_cookies;
}

bool URLRequest::CanEnablePrivacyMode() const {
  if (network_delegate_) {
    return network_delegate_->CanEnablePrivacyMode(url(), site_for_cookies_);
  }
  return !g_default_can_use_cookies;
}


void URLRequest::NotifyReadCompleted(int bytes_read) {
  if (bytes_read > 0)
    set_status(URLRequestStatus());
  // Notify in case the entire URL Request has been finished.
  if (bytes_read <= 0)
    NotifyRequestCompleted();

  // When URLRequestJob notices there was an error in URLRequest's |status_|,
  // it calls this method with |bytes_read| set to -1. Set it to a real error
  // here.
  // TODO(maksims): NotifyReadCompleted take the error code as an argument on
  // failure, rather than -1.
  if (bytes_read == -1)
    bytes_read = status_.error();

  delegate_->OnReadCompleted(this, bytes_read);

  // Nothing below this line as OnReadCompleted may delete |this|.
}

void URLRequest::OnHeadersComplete() {
  set_status(URLRequestStatus());
  // Cache load timing information now, as information will be lost once the
  // socket is closed and the ClientSocketHandle is Reset, which will happen
  // once the body is complete.  The start times should already be populated.
  if (job_.get()) {
    // Keep a copy of the two times the URLRequest sets.
    base::TimeTicks request_start = load_timing_info_.request_start;
    base::Time request_start_time = load_timing_info_.request_start_time;

    // Clear load times.  Shouldn't be neded, but gives the GetLoadTimingInfo a
    // consistent place to start from.
    load_timing_info_ = LoadTimingInfo();
    job_->GetLoadTimingInfo(&load_timing_info_);

    load_timing_info_.request_start = request_start;
    load_timing_info_.request_start_time = request_start_time;
    raw_header_size_ = GetTotalReceivedBytes();

    ConvertRealLoadTimesToBlockingTimes(&load_timing_info_);
  }
}

void URLRequest::NotifyRequestCompleted() {
  // TODO(battre): Get rid of this check, according to willchan it should
  // not be needed.
  if (has_notified_completion_)
    return;

  is_pending_ = false;
  is_redirecting_ = false;
  has_notified_completion_ = true;
  if (network_delegate_)
    network_delegate_->NotifyCompleted(this, job_.get() != NULL,
                                       status_.error());

#if BUILDFLAG(ENABLE_REPORTING)
  MaybeGenerateNetworkErrorLoggingReport();
#endif  // BUILDFLAG(ENABLE_REPORTING)
}

void URLRequest::OnCallToDelegate(NetLogEventType type) {
  DCHECK(!calling_delegate_);
  DCHECK(blocked_by_.empty());
  calling_delegate_ = true;
  delegate_event_type_ = type;
  net_log_.BeginEvent(type);
}

void URLRequest::OnCallToDelegateComplete() {
  // This should have been cleared before resuming the request.
  DCHECK(blocked_by_.empty());
  if (!calling_delegate_)
    return;
  calling_delegate_ = false;
  net_log_.EndEvent(delegate_event_type_);
  delegate_event_type_ = NetLogEventType::FAILED;
}

#if BUILDFLAG(ENABLE_REPORTING)
std::string URLRequest::GetUserAgent() const {
  // This should be kept in sync with the corresponding code in
  // URLRequestHttpJob::Start.
  // TODO(dcreager): Consider whether we can share code instead of copy-pasting
  std::string user_agent;
  if (extra_request_headers_.GetHeader(net::HttpRequestHeaders::kUserAgent,
                                       &user_agent))
    return user_agent;
  if (context()->http_user_agent_settings())
    return context()->http_user_agent_settings()->GetUserAgent();
  return std::string();
}

void URLRequest::MaybeGenerateNetworkErrorLoggingReport() {
  NetworkErrorLoggingService* service =
      context()->network_error_logging_service();
  if (!service) {
    NetworkErrorLoggingService::
        RecordRequestDiscardedForNoNetworkErrorLoggingService();
    return;
  }

  // TODO(juliatuttle): Figure out whether we should be ignoring errors from
  // non-HTTPS origins.

  NetworkErrorLoggingService::RequestDetails details;

  details.uri = url();
  details.referrer = GURL(referrer());
  details.user_agent = GetUserAgent();
  IPEndPoint endpoint;
  if (GetRemoteEndpoint(&endpoint))
    details.server_ip = endpoint.address();
  if (response_headers()) {
    // HttpResponseHeaders::response_code() returns 0 if response code couldn't
    // be parsed, which is also how NEL represents the same.
    details.status_code = response_headers()->response_code();
    // If we got response headers, assume that the connection used HTTP/1.1
    // unless ALPN negotation tells us otherwise (handled below).
    details.protocol = "http/1.1";
  } else {
    details.status_code = 0;
  }
  if (response_info().was_alpn_negotiated)
    details.protocol = response_info().alpn_negotiated_protocol;
  details.method = method();
  details.elapsed_time =
      base::TimeTicks::Now() - load_timing_info_.request_start;
  details.type = status().ToNetError();

  if (context()->reporting_service()) {
    details.reporting_upload_depth =
        context()->reporting_service()->GetUploadDepth(*this);
  } else {
    details.reporting_upload_depth = 0;
  }

  service->OnRequest(std::move(details));
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

void URLRequest::GetConnectionAttempts(ConnectionAttempts* out) const {
  if (job_)
    job_->GetConnectionAttempts(out);
  else
    out->clear();
}

void URLRequest::SetRequestHeadersCallback(RequestHeadersCallback callback) {
  DCHECK(!job_.get());
  DCHECK(request_headers_callback_.is_null());
  request_headers_callback_ = std::move(callback);
}

void URLRequest::SetResponseHeadersCallback(ResponseHeadersCallback callback) {
  DCHECK(!job_.get());
  DCHECK(response_headers_callback_.is_null());
  response_headers_callback_ = std::move(callback);
}

void URLRequest::set_socket_tag(const SocketTag& socket_tag) {
  DCHECK(!is_pending_);
  DCHECK(url().SchemeIsHTTPOrHTTPS());
  socket_tag_ = socket_tag;
}

void URLRequest::set_status(URLRequestStatus status) {
  DCHECK(status_.is_io_pending() || status_.is_success() ||
         (!status.is_success() && !status.is_io_pending()));
  status_ = status;
}

base::WeakPtr<URLRequest> URLRequest::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace net
