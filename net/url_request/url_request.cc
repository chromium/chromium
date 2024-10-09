// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions_internal_overloads.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_util.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "net/base/auth.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_delegate.h"
#include "net/base/upload_data_stream.h"
#include "net/cert/x509_certificate.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_log_util.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/socket/next_proto.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/storage_access_api/status.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_netlog_params.h"
#include "net/url_request/url_request_redirect_job.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

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

  if (!load_timing_info->receive_headers_start.is_null() &&
      load_timing_info->receive_headers_start < block_on_connect) {
    load_timing_info->receive_headers_start = block_on_connect;
  }
  if (!load_timing_info->receive_non_informational_headers_start.is_null() &&
      load_timing_info->receive_non_informational_headers_start <
          block_on_connect) {
    load_timing_info->receive_non_informational_headers_start =
        block_on_connect;
  }

  // Make sure connection times are after start and proxy times.

  LoadTimingInfo::ConnectTiming* connect_timing =
      &load_timing_info->connect_timing;
  if (!connect_timing->domain_lookup_start.is_null()) {
    DCHECK(!connect_timing->domain_lookup_end.is_null());
    if (connect_timing->domain_lookup_start < block_on_connect)
      connect_timing->domain_lookup_start = block_on_connect;
    if (connect_timing->domain_lookup_end < block_on_connect)
      connect_timing->domain_lookup_end = block_on_connect;
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

NetLogWithSource CreateNetLogWithSource(
    NetLog* net_log,
    std::optional<net::NetLogSource> net_log_source) {
  if (net_log_source) {
    return NetLogWithSource::Make(net_log, net_log_source.value());
  }
  return NetLogWithSource::Make(net_log, NetLogSourceType::URL_REQUEST);
}

// TODO(https://crbug.com/366284840): remove this, once the "retry" header is
// handled in URLLoader.
net::cookie_util::SecFetchStorageAccessValueOutcome
ConvertSecFetchStorageAccessHeaderValueToOutcome(
    net::cookie_util::StorageAccessStatus storage_access_status) {
  using enum net::cookie_util::SecFetchStorageAccessValueOutcome;
  switch (storage_access_status) {
    case net::cookie_util::StorageAccessStatus::kInactive:
      return kValueInactive;
    case net::cookie_util::StorageAccessStatus::kActive:
      return kValueActive;
    case net::cookie_util::StorageAccessStatus::kNone:
      return kValueNone;
  }
  NOTREACHED();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// URLRequest::Delegate

int URLRequest::Delegate::OnConnected(URLRequest* request,
                                      const TransportInfo& info,
                                      CompletionOnceCallback callback) {
  return OK;
}

void URLRequest::Delegate::OnReceivedRedirect(URLRequest* request,
                                              const RedirectInfo& redirect_info,
                                              bool* defer_redirect) {}

void URLRequest::Delegate::OnAuthRequired(URLRequest* request,
                                          const AuthChallengeInfo& auth_info) {
  request->CancelAuth();
}

void URLRequest::Delegate::OnCertificateRequested(
    URLRequest* request,
    SSLCertRequestInfo* cert_request_info) {
  request->CancelWithError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void URLRequest::Delegate::OnSSLCertificateError(URLRequest* request,
                                                 int net_error,
                                                 const SSLInfo& ssl_info,
                                                 bool is_hsts_ok) {
  request->Cancel();
}

void URLRequest::Delegate::OnResponseStarted(URLRequest* request,
                                             int net_error) {
  NOTREACHED_IN_MIGRATION();
}

///////////////////////////////////////////////////////////////////////////////
// URLRequest

URLRequest::~URLRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Cancel();

  if (network_delegate()) {
    network_delegate()->NotifyURLRequestDestroyed(this);
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
  if (status_ != ERR_ABORTED)
    net_error = status_;
  net_log_.EndEventWithNetErrorCode(NetLogEventType::REQUEST_ALIVE, net_error);
}

void URLRequest::set_upload(std::unique_ptr<UploadDataStream> upload) {
  upload_data_stream_ = std::move(upload);
}

const UploadDataStream* URLRequest::get_upload_for_testing() const {
  return upload_data_stream_.get();
}

bool URLRequest::has_upload() const {
  return upload_data_stream_.get() != nullptr;
}

void URLRequest::SetExtraRequestHeaderByName(std::string_view name,
                                             std::string_view value,
                                             bool overwrite) {
  DCHECK(!is_pending_ || is_redirecting_);
  if (overwrite) {
    extra_request_headers_.SetHeader(name, value);
  } else {
    extra_request_headers_.SetHeaderIfMissing(name, value);
  }
}

void URLRequest::RemoveRequestHeaderByName(std::string_view name) {
  DCHECK(!is_pending_ || is_redirecting_);
  extra_request_headers_.RemoveHeader(name);
}

void URLRequest::SetExtraRequestHeaders(const HttpRequestHeaders& headers) {
  DCHECK(!is_pending_);
  extra_request_headers_ = headers;

  // NOTE: This method will likely become non-trivial once the other setters
  // for request headers are implemented.
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
  if (!job_.get()) {
    return 0;
  }

  if (int64_t bytes = job_->GetReceivedBodyBytes()) {
    return bytes;
  }

  // GetReceivedBodyBytes() is available only when the body was received from
  // the network. Otherwise, returns prefilter_bytes_read() instead.
  return job_->prefilter_bytes_read();
}

LoadStateWithParam URLRequest::GetLoadState() const {
  // The !blocked_by_.empty() check allows |this| to report it's blocked on a
  // delegate before it has been started.
  if (calling_delegate_ || !blocked_by_.empty()) {
    return LoadStateWithParam(LOAD_STATE_WAITING_FOR_DELEGATE,
                              use_blocked_by_as_load_param_
                                  ? base::UTF8ToUTF16(blocked_by_)
                                  : std::u16string());
  }
  return LoadStateWithParam(job_.get() ? job_->GetLoadState() : LOAD_STATE_IDLE,
                            std::u16string());
}

base::Value::Dict URLRequest::GetStateAsValue() const {
  base::Value::Dict dict;
  dict.Set("url", original_url().possibly_invalid_spec());

  if (url_chain_.size() > 1) {
    base::Value::List list;
    for (const GURL& url : url_chain_) {
      list.Append(url.possibly_invalid_spec());
    }
    dict.Set("url_chain", std::move(list));
  }

  dict.Set("load_flags", load_flags());

  LoadStateWithParam load_state = GetLoadState();
  dict.Set("load_state", load_state.state);
  if (!load_state.param.empty())
    dict.Set("load_state_param", load_state.param);
  if (!blocked_by_.empty())
    dict.Set("delegate_blocked_by", blocked_by_);

  dict.Set("method", method_);
  dict.Set("network_anonymization_key",
           isolation_info_.network_anonymization_key().ToDebugString());
  dict.Set("network_isolation_key",
           isolation_info_.network_isolation_key().ToDebugString());
  dict.Set("has_upload", has_upload());
  dict.Set("is_pending", is_pending_);

  dict.Set("traffic_annotation", traffic_annotation_.unique_id_hash_code);

  if (status_ != OK)
    dict.Set("net_error", status_);
  return dict;
}

void URLRequest::LogBlockedBy(std::string_view blocked_by) {
  DCHECK(!blocked_by.empty());

  // Only log information to NetLog during startup and certain deferring calls
  // to delegates.  For all reads but the first, do nothing.
  if (!calling_delegate_ && !response_info_.request_time.is_null())
    return;

  LogUnblocked();
  blocked_by_ = std::string(blocked_by);
  use_blocked_by_as_load_param_ = false;

  net_log_.BeginEventWithStringParams(NetLogEventType::DELEGATE_INFO,
                                      "delegate_blocked_by", blocked_by_);
}

void URLRequest::LogAndReportBlockedBy(std::string_view source) {
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

void URLRequest::GetResponseHeaderByName(std::string_view name,
                                         std::string* value) const {
  DCHECK(value);
  if (response_info_.headers.get()) {
    response_info_.headers->GetNormalizedHeader(name, value);
  } else {
    value->clear();
  }
}

IPEndPoint URLRequest::GetResponseRemoteEndpoint() const {
  DCHECK(job_.get());
  return job_->GetResponseRemoteEndpoint();
}

HttpResponseHeaders* URLRequest::response_headers() const {
  return response_info_.headers.get();
}

const std::optional<AuthChallengeInfo>& URLRequest::auth_challenge_info()
    const {
  return response_info_.auth_challenge;
}

void URLRequest::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  *load_timing_info = load_timing_info_;
}

void URLRequest::PopulateNetErrorDetails(NetErrorDetails* details) const {
  if (!job_)
    return;
  return job_->PopulateNetErrorDetails(details);
}

bool URLRequest::GetTransactionRemoteEndpoint(IPEndPoint* endpoint) const {
  if (!job_)
    return false;

  return job_->GetTransactionRemoteEndpoint(endpoint);
}

void URLRequest::GetMimeType(std::string* mime_type) const {
  DCHECK(job_.get());
  job_->GetMimeType(mime_type);
}

void URLRequest::GetCharset(std::string* charset) const {
  DCHECK(job_.get());
  job_->GetCharset(charset);
}

int URLRequest::GetResponseCode() const {
  DCHECK(job_.get());
  return job_->GetResponseCode();
}

void URLRequest::set_maybe_sent_cookies(CookieAccessResultList cookies) {
  maybe_sent_cookies_ = std::move(cookies);
}

void URLRequest::set_maybe_stored_cookies(
    CookieAndLineAccessResultList cookies) {
  maybe_stored_cookies_ = std::move(cookies);
}

void URLRequest::SetLoadFlags(int flags) {
  if ((load_flags() & LOAD_IGNORE_LIMITS) != (flags & LOAD_IGNORE_LIMITS)) {
    DCHECK(!job_.get());
    DCHECK(flags & LOAD_IGNORE_LIMITS);
    DCHECK_EQ(priority_, MAXIMUM_PRIORITY);
  }
  partial_load_flags_ = flags;

  // This should be a no-op given the above DCHECKs, but do this
  // anyway for release mode.
  if ((load_flags() & LOAD_IGNORE_LIMITS) != 0) {
    SetPriority(MAXIMUM_PRIORITY);
  }
}

void URLRequest::SetSecureDnsPolicy(SecureDnsPolicy secure_dns_policy) {
  secure_dns_policy_ = secure_dns_policy;
}

// static
void URLRequest::SetDefaultCookiePolicyToBlock() {
  CHECK(!g_url_requests_started);
  g_default_can_use_cookies = false;
}

void URLRequest::SetURLChain(const std::vector<GURL>& url_chain) {
  DCHECK(!job_);
  DCHECK(!is_pending_);
  DCHECK_EQ(url_chain_.size(), 1u);

  if (url_chain.size() < 2)
    return;

  // In most cases the current request URL will match the last URL in the
  // explicitly set URL chain.  In some cases, however, a throttle will modify
  // the request URL resulting in a different request URL.  We handle this by
  // using previous values from the explicitly set URL chain, but with the
  // request URL as the final entry in the chain.
  url_chain_.insert(url_chain_.begin(), url_chain.begin(),
                    url_chain.begin() + url_chain.size() - 1);
}

void URLRequest::set_site_for_cookies(const SiteForCookies& site_for_cookies) {
  DCHECK(!is_pending_);
  site_for_cookies_ = site_for_cookies;
}

void URLRequest::set_isolation_info(const IsolationInfo& isolation_info,
                                    std::optional<GURL> redirect_info_new_url) {
  isolation_info_ = isolation_info;

  bool is_main_frame_navigation = isolation_info.IsMainFrameRequest() ||
                                  force_main_frame_for_same_site_cookies();

  cookie_partition_key_ = CookiePartitionKey::FromNetworkIsolationKey(
      isolation_info.network_isolation_key(), isolation_info.site_for_cookies(),
      net::SchemefulSite(redirect_info_new_url.has_value()
                             ? redirect_info_new_url.value()
                             : url_chain_.back()),
      is_main_frame_navigation);
}

void URLRequest::set_isolation_info_from_network_anonymization_key(
    const NetworkAnonymizationKey& network_anonymization_key) {
  set_isolation_info(URLRequest::CreateIsolationInfoFromNetworkAnonymizationKey(
      network_anonymization_key));

  is_created_from_network_anonymization_key_ = true;
}

void URLRequest::set_first_party_url_policy(
    RedirectInfo::FirstPartyURLPolicy first_party_url_policy) {
  DCHECK(!is_pending_);
  first_party_url_policy_ = first_party_url_policy;
}

void URLRequest::set_initiator(const std::optional<url::Origin>& initiator) {
  DCHECK(!is_pending_);
  DCHECK(!initiator.has_value() || initiator.value().opaque() ||
         initiator.value().GetURL().is_valid());
  initiator_ = initiator;
}

void URLRequest::set_method(std::string_view method) {
  DCHECK(!is_pending_);
  method_ = std::string(method);
}

#if BUILDFLAG(ENABLE_REPORTING)
void URLRequest::set_reporting_upload_depth(int reporting_upload_depth) {
  DCHECK(!is_pending_);
  reporting_upload_depth_ = reporting_upload_depth;
}
#endif

void URLRequest::SetReferrer(std::string_view referrer) {
  DCHECK(!is_pending_);
  GURL referrer_url(referrer);
  if (referrer_url.is_valid()) {
    referrer_ = referrer_url.GetAsReferrer().spec();
  } else {
    referrer_ = std::string(referrer);
  }
}

void URLRequest::set_referrer_policy(ReferrerPolicy referrer_policy) {
  DCHECK(!is_pending_);
  referrer_policy_ = referrer_policy;
}

void URLRequest::set_allow_credentials(bool allow_credentials) {
  allow_credentials_ = allow_credentials;
  if (allow_credentials) {
    partial_load_flags_ &= ~LOAD_DO_NOT_SAVE_COOKIES;
  } else {
    partial_load_flags_ |= LOAD_DO_NOT_SAVE_COOKIES;
  }
}

void URLRequest::Start() {
  DCHECK(delegate_);

  if (status_ != OK)
    return;

  if (context_->require_network_anonymization_key()) {
    DCHECK(!isolation_info_.IsEmpty());
  }

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

  if (network_delegate()) {
    OnCallToDelegate(NetLogEventType::NETWORK_DELEGATE_BEFORE_URL_REQUEST);
    int error = network_delegate()->NotifyBeforeURLRequest(
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

  StartJob(context_->job_factory()->CreateJob(this));
}

///////////////////////////////////////////////////////////////////////////////

URLRequest::URLRequest(base::PassKey<URLRequestContext> pass_key,
                       const GURL& url,
                       RequestPriority priority,
                       Delegate* delegate,
                       const URLRequestContext* context,
                       NetworkTrafficAnnotationTag traffic_annotation,
                       bool is_for_websockets,
                       std::optional<net::NetLogSource> net_log_source)
    : context_(context),
      net_log_(CreateNetLogWithSource(context->net_log(), net_log_source)),
      url_chain_(1, url),
      method_("GET"),
      delegate_(delegate),
      is_for_websockets_(is_for_websockets),
      redirect_limit_(kMaxRedirects),
      priority_(priority),
      creation_time_(base::TimeTicks::Now()),
      traffic_annotation_(traffic_annotation) {
  // Sanity check out environment.
  DCHECK(base::SingleThreadTaskRunner::HasCurrentDefault());

  context->url_requests()->insert(this);
  net_log_.BeginEvent(NetLogEventType::REQUEST_ALIVE, [&] {
    return NetLogURLRequestConstructorParams(url, priority_,
                                             traffic_annotation_);
  });
}

void URLRequest::BeforeRequestComplete(int error) {
  DCHECK(!job_.get());
  DCHECK_NE(ERR_IO_PENDING, error);

  // Check that there are no callbacks to already failed or canceled requests.
  DCHECK(!failed());

  OnCallToDelegateComplete();

  if (error != OK) {
    net_log_.AddEventWithStringParams(NetLogEventType::CANCELLED, "source",
                                      "delegate");
    StartJob(std::make_unique<URLRequestErrorJob>(this, error));
  } else if (!delegate_redirect_url_.is_empty()) {
    GURL new_url;
    new_url.Swap(&delegate_redirect_url_);

    StartJob(std::make_unique<URLRequestRedirectJob>(
        this, new_url,
        // Use status code 307 to preserve the method, so POST requests work.
        RedirectUtil::ResponseCode::REDIRECT_307_TEMPORARY_REDIRECT,
        "Delegate"));
  } else {
    StartJob(context_->job_factory()->CreateJob(this));
  }
}

void URLRequest::StartJob(std::unique_ptr<URLRequestJob> job) {
  DCHECK(!is_pending_);
  DCHECK(!job_);
  if (is_created_from_network_anonymization_key_) {
    DCHECK(load_flags() & LOAD_DISABLE_CACHE);
    DCHECK(!allow_credentials_);
  }

  net_log_.BeginEvent(NetLogEventType::URL_REQUEST_START_JOB, [&] {
    return NetLogURLRequestStartParams(
        url(), method_, load_flags(), isolation_info_, site_for_cookies_,
        initiator_,
        upload_data_stream_ ? upload_data_stream_->identifier() : -1);
  });

  job_ = std::move(job);
  job_->SetExtraRequestHeaders(extra_request_headers_);
  job_->SetPriority(priority_);
  job_->SetRequestHeadersCallback(request_headers_callback_);
  job_->SetEarlyResponseHeadersCallback(early_response_headers_callback_);
  if (is_shared_dictionary_read_allowed_callback_) {
    job_->SetIsSharedDictionaryReadAllowedCallback(
        is_shared_dictionary_read_allowed_callback_);
  }
  job_->SetResponseHeadersCallback(response_headers_callback_);
  if (shared_dictionary_getter_) {
    job_->SetSharedDictionaryGetter(shared_dictionary_getter_);
  }

  if (upload_data_stream_.get())
    job_->SetUpload(upload_data_stream_.get());

  is_pending_ = true;
  is_redirecting_ = false;

  response_info_.was_cached = false;

  maybe_sent_cookies_.clear();
  maybe_stored_cookies_.clear();

  GURL referrer_url(referrer_);
  bool same_origin_for_metrics;

  if (referrer_url !=
      URLRequestJob::ComputeReferrerForPolicy(
          referrer_policy_, referrer_url, url(), &same_origin_for_metrics)) {
    if (!network_delegate() ||
        !network_delegate()->CancelURLRequestWithPolicyViolatingReferrerHeader(
            *this, url(), referrer_url)) {
      referrer_.clear();
    } else {
      // We need to clear the referrer anyway to avoid an infinite recursion
      // when starting the error job.
      referrer_.clear();
      net_log_.AddEventWithStringParams(NetLogEventType::CANCELLED, "source",
                                        "delegate");
      RestartWithJob(
          std::make_unique<URLRequestErrorJob>(this, ERR_BLOCKED_BY_CLIENT));
      return;
    }
  }

  RecordReferrerGranularityMetrics(same_origin_for_metrics);

  // Start() always completes asynchronously.
  //
  // Status is generally set by URLRequestJob itself, but Start() calls
  // directly into the URLRequestJob subclass, so URLRequestJob can't set it
  // here.
  // TODO(mmenke):  Make the URLRequest manage its own status.
  status_ = ERR_IO_PENDING;
  job_->Start();
}

void URLRequest::RestartWithJob(std::unique_ptr<URLRequestJob> job) {
  DCHECK(job->request() == this);
  PrepareToRestart();
  StartJob(std::move(job));
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
    NOTREACHED_IN_MIGRATION();
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
  if (!failed()) {
    status_ = error;
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

  return status_;
}

int URLRequest::Read(IOBuffer* dest, int dest_size) {
  DCHECK(job_.get());
  DCHECK_NE(ERR_IO_PENDING, status_);

  // If this is the first read, end the delegate call that may have started in
  // OnResponseStarted.
  OnCallToDelegateComplete();

  // If the request has failed, Read() will return actual network error code.
  if (status_ != OK)
    return status_;

  // This handles reads after the request already completed successfully.
  // TODO(ahendrickson): DCHECK() that it is not done after
  // http://crbug.com/115705 is fixed.
  if (job_->is_done())
    return status_;

  if (dest_size == 0) {
    // Caller is not too bright.  I guess we've done what they asked.
    return OK;
  }

  // Caller should provide a buffer.
  DCHECK(dest && dest->data());

  int rv = job_->Read(dest, dest_size);
  if (rv == ERR_IO_PENDING) {
    set_status(ERR_IO_PENDING);
  } else if (rv <= 0) {
    NotifyRequestCompleted();
  }

  // If rv is not 0 or actual bytes read, the status cannot be success.
  DCHECK(rv >= 0 || status_ != OK);
  return rv;
}

void URLRequest::set_status(int status) {
  DCHECK_LE(status, 0);
  DCHECK(!failed() || (status != OK && status != ERR_IO_PENDING));
  status_ = status;
}

bool URLRequest::failed() const {
  return (status_ != OK && status_ != ERR_IO_PENDING);
}

int URLRequest::NotifyConnected(const TransportInfo& info,
                                CompletionOnceCallback callback) {
  OnCallToDelegate(NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED);
  int result = delegate_->OnConnected(
      this, info,
      base::BindOnce(
          [](URLRequest* request, CompletionOnceCallback callback, int result) {
            request->OnCallToDelegateComplete(result);
            std::move(callback).Run(result);
          },
          this, std::move(callback)));
  if (result != ERR_IO_PENDING)
    OnCallToDelegateComplete(result);
  return result;
}

void URLRequest::NotifyReceivedRedirect(const RedirectInfo& redirect_info,
                                        bool* defer_redirect) {
  DCHECK_EQ(OK, status_);
  is_redirecting_ = true;
  OnCallToDelegate(NetLogEventType::URL_REQUEST_DELEGATE_RECEIVED_REDIRECT);
  delegate_->OnReceivedRedirect(this, redirect_info, defer_redirect);
  // |this| may be have been destroyed here.
}

void URLRequest::NotifyResponseStarted(int net_error) {
  DCHECK_LE(net_error, 0);

  // Change status if there was an error.
  if (net_error != OK)
    set_status(net_error);

  // |status_| should not be ERR_IO_PENDING when calling into the
  // URLRequest::Delegate().
  DCHECK_NE(ERR_IO_PENDING, status_);

  net_log_.EndEventWithNetErrorCode(NetLogEventType::URL_REQUEST_START_JOB,
                                    net_error);

  // In some cases (e.g. an event was canceled), we might have sent the
  // completion event and receive a NotifyResponseStarted() later.
  if (!has_notified_completion_ && net_error == OK) {
    if (network_delegate())
      network_delegate()->NotifyResponseStarted(this, net_error);
  }

  // Notify in case the entire URL Request has been finished.
  if (!has_notified_completion_ && net_error != OK)
    NotifyRequestCompleted();

  OnCallToDelegate(NetLogEventType::URL_REQUEST_DELEGATE_RESPONSE_STARTED);
  delegate_->OnResponseStarted(this, net_error);
  // Nothing may appear below this line as OnResponseStarted may delete
  // |this|.
}

void URLRequest::FollowDeferredRedirect(
    const std::optional<std::vector<std::string>>& removed_headers,
    const std::optional<net::HttpRequestHeaders>& modified_headers) {
  DCHECK(job_.get());
  DCHECK_EQ(OK, status_);

  maybe_sent_cookies_.clear();
  maybe_stored_cookies_.clear();

  status_ = ERR_IO_PENDING;
  job_->FollowDeferredRedirect(removed_headers, modified_headers);
}

void URLRequest::SetAuth(const AuthCredentials& credentials) {
  DCHECK(job_.get());
  DCHECK(job_->NeedsAuth());

  maybe_sent_cookies_.clear();
  maybe_stored_cookies_.clear();

  status_ = ERR_IO_PENDING;
  job_->SetAuth(credentials);
}

void URLRequest::CancelAuth() {
  DCHECK(job_.get());
  DCHECK(job_->NeedsAuth());

  status_ = ERR_IO_PENDING;
  job_->CancelAuth();
}

void URLRequest::ContinueWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key) {
  DCHECK(job_.get());

  // Matches the call in NotifyCertificateRequested.
  OnCallToDelegateComplete();

  status_ = ERR_IO_PENDING;
  job_->ContinueWithCertificate(std::move(client_cert),
                                std::move(client_private_key));
}

void URLRequest::ContinueDespiteLastError() {
  DCHECK(job_.get());

  // Matches the call in NotifySSLCertificateError.
  OnCallToDelegateComplete();

  status_ = ERR_IO_PENDING;
  job_->ContinueDespiteLastError();
}

void URLRequest::AbortAndCloseConnection() {
  DCHECK_EQ(OK, status_);
  DCHECK(!has_notified_completion_);
  DCHECK(job_);
  job_->CloseConnectionOnDestruction();
  job_.reset();
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

  status_ = OK;
  is_pending_ = false;
  proxy_chain_ = ProxyChain();
}

void URLRequest::Redirect(
    const RedirectInfo& redirect_info,
    const std::optional<std::vector<std::string>>& removed_headers,
    const std::optional<net::HttpRequestHeaders>& modified_headers) {
  // This method always succeeds. Whether |job_| is allowed to redirect to
  // |redirect_info| is checked in URLRequestJob::CanFollowRedirect, before
  // NotifyReceivedRedirect. This means the delegate can assume that, if it
  // accepted the redirect, future calls to OnResponseStarted correspond to
  // |redirect_info.new_url|.
  OnCallToDelegateComplete();
  if (net_log_.IsCapturing()) {
    net_log_.AddEventWithStringParams(
        NetLogEventType::URL_REQUEST_REDIRECTED, "location",
        redirect_info.new_url.possibly_invalid_spec());
  }

  if (network_delegate())
    network_delegate()->NotifyBeforeRedirect(this, redirect_info.new_url);

  if (!final_upload_progress_.position() && upload_data_stream_)
    final_upload_progress_ = upload_data_stream_->GetUploadProgress();
  PrepareToRestart();

  bool clear_body = false;
  net::RedirectUtil::UpdateHttpRequest(url(), method_, redirect_info,
                                       removed_headers, modified_headers,
                                       &extra_request_headers_, &clear_body);
  if (clear_body)
    upload_data_stream_.reset();

  method_ = redirect_info.new_method;
  referrer_ = redirect_info.new_referrer;
  referrer_policy_ = redirect_info.new_referrer_policy;
  site_for_cookies_ = redirect_info.new_site_for_cookies;
  set_isolation_info(isolation_info_.CreateForRedirect(
                         url::Origin::Create(redirect_info.new_url)),
                     redirect_info.new_url);

  cookie_setting_overrides().Remove(
      CookieSettingOverride::kStorageAccessGrantEligibleViaHeader);

  if ((load_flags() & LOAD_CAN_USE_SHARED_DICTIONARY) &&
      (load_flags() &
       LOAD_DISABLE_SHARED_DICTIONARY_AFTER_CROSS_ORIGIN_REDIRECT) &&
      !url::Origin::Create(url()).IsSameOriginWith(redirect_info.new_url)) {
    partial_load_flags_ &= ~LOAD_CAN_USE_SHARED_DICTIONARY;
  }

  url_chain_.push_back(redirect_info.new_url);
  --redirect_limit_;

  Start();
}

void URLRequest::RetryWithStorageAccess() {
  CHECK(!cookie_setting_overrides().Has(
      CookieSettingOverride::kStorageAccessGrantEligibleViaHeader));
  CHECK(!cookie_setting_overrides().Has(
      CookieSettingOverride::kStorageAccessGrantEligible));

  net_log_.AddEvent(NetLogEventType::URL_REQUEST_RETRY_WITH_STORAGE_ACCESS);
  if (network_delegate()) {
    network_delegate()->NotifyBeforeRetry(this);
  }

  cookie_setting_overrides().Put(
      CookieSettingOverride::kStorageAccessGrantEligibleViaHeader);
  set_storage_access_status(CalculateStorageAccessStatus());

  if (!final_upload_progress_.position() && upload_data_stream_) {
    final_upload_progress_ = upload_data_stream_->GetUploadProgress();
  }
  PrepareToRestart();

  // This isn't really a proper redirect, but we add to the `url_chain_` and
  // count it against the redirect limit anyway, to avoid unbounded retries.
  url_chain_.push_back(url());
  --redirect_limit_;

  Start();
}

// static
bool URLRequest::DefaultCanUseCookies() {
  return g_default_can_use_cookies;
}

const URLRequestContext* URLRequest::context() const {
  return context_;
}

NetworkDelegate* URLRequest::network_delegate() const {
  return context_->network_delegate();
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

  if ((load_flags() & LOAD_IGNORE_LIMITS) && (priority != MAXIMUM_PRIORITY)) {
    NOTREACHED_IN_MIGRATION();
    // Maintain the invariant that requests with IGNORE_LIMITS set
    // have MAXIMUM_PRIORITY for release mode.
    return;
  }

  if (priority_ == priority)
    return;

  priority_ = priority;
  net_log_.AddEventWithStringParams(NetLogEventType::URL_REQUEST_SET_PRIORITY,
                                    "priority",
                                    RequestPriorityToString(priority_));
  if (job_.get())
    job_->SetPriority(priority_);
}

void URLRequest::SetPriorityIncremental(bool priority_incremental) {
  priority_incremental_ = priority_incremental;
}

void URLRequest::NotifyAuthRequired(
    std::unique_ptr<AuthChallengeInfo> auth_info) {
  DCHECK_EQ(OK, status_);
  DCHECK(auth_info);
  // Check that there are no callbacks to already failed or cancelled requests.
  DCHECK(!failed());

  delegate_->OnAuthRequired(this, *auth_info.get());
}

void URLRequest::NotifyCertificateRequested(
    SSLCertRequestInfo* cert_request_info) {
  status_ = OK;

  OnCallToDelegate(NetLogEventType::URL_REQUEST_DELEGATE_CERTIFICATE_REQUESTED);
  delegate_->OnCertificateRequested(this, cert_request_info);
}

void URLRequest::NotifySSLCertificateError(int net_error,
                                           const SSLInfo& ssl_info,
                                           bool fatal) {
  status_ = OK;
  OnCallToDelegate(NetLogEventType::URL_REQUEST_DELEGATE_SSL_CERTIFICATE_ERROR);
  delegate_->OnSSLCertificateError(this, net_error, ssl_info, fatal);
}

bool URLRequest::CanSetCookie(
    const net::CanonicalCookie& cookie,
    CookieOptions* options,
    const net::FirstPartySetMetadata& first_party_set_metadata,
    CookieInclusionStatus* inclusion_status) const {
  DCHECK(!(load_flags() & LOAD_DO_NOT_SAVE_COOKIES));
  bool can_set_cookies = g_default_can_use_cookies;
  if (network_delegate()) {
    can_set_cookies = network_delegate()->CanSetCookie(
        *this, cookie, options, first_party_set_metadata, inclusion_status);
  }
  if (!can_set_cookies)
    net_log_.AddEvent(NetLogEventType::COOKIE_SET_BLOCKED_BY_NETWORK_DELEGATE);
  return can_set_cookies;
}

void URLRequest::NotifyReadCompleted(int bytes_read) {
  if (bytes_read > 0)
    set_status(OK);
  // Notify in case the entire URL Request has been finished.
  if (bytes_read <= 0)
    NotifyRequestCompleted();

  // When URLRequestJob notices there was an error in URLRequest's |status_|,
  // it calls this method with |bytes_read| set to -1. Set it to a real error
  // here.
  // TODO(maksims): NotifyReadCompleted take the error code as an argument on
  // failure, rather than -1.
  if (bytes_read == -1) {
    // |status_| should indicate an error.
    DCHECK(failed());
    bytes_read = status_;
  }

  delegate_->OnReadCompleted(this, bytes_read);

  // Nothing below this line as OnReadCompleted may delete |this|.
}

void URLRequest::OnHeadersComplete() {
  // The URLRequest status should still be IO_PENDING, which it was set to
  // before the URLRequestJob was started.  On error or cancellation, this
  // method should not be called.
  DCHECK_EQ(ERR_IO_PENDING, status_);
  set_status(OK);
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
  if (network_delegate())
    network_delegate()->NotifyCompleted(this, job_.get() != nullptr, status_);
}

void URLRequest::OnCallToDelegate(NetLogEventType type) {
  DCHECK(!calling_delegate_);
  DCHECK(blocked_by_.empty());
  calling_delegate_ = true;
  delegate_event_type_ = type;
  net_log_.BeginEvent(type);
}

void URLRequest::OnCallToDelegateComplete(int error) {
  // This should have been cleared before resuming the request.
  DCHECK(blocked_by_.empty());
  if (!calling_delegate_)
    return;
  calling_delegate_ = false;
  net_log_.EndEventWithNetErrorCode(delegate_event_type_, error);
  delegate_event_type_ = NetLogEventType::FAILED;
}

void URLRequest::RecordReferrerGranularityMetrics(
    bool request_is_same_origin) const {
  GURL referrer_url(referrer_);
  bool referrer_more_descriptive_than_its_origin =
      referrer_url.is_valid() && referrer_url.PathForRequestPiece().size() > 1;

  // To avoid renaming the existing enum, we have to use the three-argument
  // histogram macro.
  if (request_is_same_origin) {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.URLRequest.ReferrerPolicyForRequest.SameOrigin", referrer_policy_,
        static_cast<int>(ReferrerPolicy::MAX) + 1);
    UMA_HISTOGRAM_BOOLEAN(
        "Net.URLRequest.ReferrerHasInformativePath.SameOrigin",
        referrer_more_descriptive_than_its_origin);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.URLRequest.ReferrerPolicyForRequest.CrossOrigin", referrer_policy_,
        static_cast<int>(ReferrerPolicy::MAX) + 1);
    UMA_HISTOGRAM_BOOLEAN(
        "Net.URLRequest.ReferrerHasInformativePath.CrossOrigin",
        referrer_more_descriptive_than_its_origin);
  }
}

IsolationInfo URLRequest::CreateIsolationInfoFromNetworkAnonymizationKey(
    const NetworkAnonymizationKey& network_anonymization_key) {
  if (!network_anonymization_key.IsFullyPopulated()) {
    return IsolationInfo();
  }

  url::Origin top_frame_origin =
      network_anonymization_key.GetTopFrameSite()->site_as_origin_;

  std::optional<url::Origin> frame_origin;
  if (network_anonymization_key.IsCrossSite()) {
    // If we know that the origin is cross site to the top level site, create an
    // empty origin to use as the frame origin for the isolation info. This
    // should be cross site with the top level origin.
    frame_origin = url::Origin();
  } else {
    // If we don't know that it's cross site to the top level site, use the top
    // frame site to set the frame origin.
    frame_origin = top_frame_origin;
  }

  auto isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kOther, top_frame_origin,
      frame_origin.value(), SiteForCookies(),
      network_anonymization_key.GetNonce());
  // TODO(crbug.com/40852603): DCHECK isolation info is fully populated.
  return isolation_info;
}

ConnectionAttempts URLRequest::GetConnectionAttempts() const {
  if (job_)
    return job_->GetConnectionAttempts();
  return {};
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

void URLRequest::SetEarlyResponseHeadersCallback(
    ResponseHeadersCallback callback) {
  DCHECK(!job_.get());
  DCHECK(early_response_headers_callback_.is_null());
  early_response_headers_callback_ = std::move(callback);
}

void URLRequest::SetIsSharedDictionaryReadAllowedCallback(
    base::RepeatingCallback<bool()> callback) {
  DCHECK(!job_.get());
  DCHECK(is_shared_dictionary_read_allowed_callback_.is_null());
  is_shared_dictionary_read_allowed_callback_ = std::move(callback);
}

void URLRequest::set_socket_tag(const SocketTag& socket_tag) {
  DCHECK(!is_pending_);
  DCHECK(url().SchemeIsHTTPOrHTTPS());
  socket_tag_ = socket_tag;
}
std::optional<net::cookie_util::StorageAccessStatus>
URLRequest::CalculateStorageAccessStatus() const {
  std::optional<net::cookie_util::StorageAccessStatus> storage_access_status =
      network_delegate()->GetStorageAccessStatus(*this);

  auto get_storage_access_value_outcome_if_omitted = [&]()
      -> std::optional<net::cookie_util::SecFetchStorageAccessValueOutcome> {
    if (!network_delegate()->IsStorageAccessHeaderEnabled(
            base::OptionalToPtr(isolation_info().top_frame_origin()), url())) {
      return net::cookie_util::SecFetchStorageAccessValueOutcome::
          kOmittedFeatureDisabled;
    }
    // Avoid attaching the header in cases where credentials are not included in
    // the request.
    if (!allow_credentials_) {
      return net::cookie_util::SecFetchStorageAccessValueOutcome::
          kOmittedRequestOmitsCredentials;
    }
    if (!storage_access_status) {
      return net::cookie_util::SecFetchStorageAccessValueOutcome::
          kOmittedSameSite;
    }
    return std::nullopt;
  };

  auto storage_access_value_outcome =
      get_storage_access_value_outcome_if_omitted();
  if (storage_access_value_outcome) {
    storage_access_status = std::nullopt;
  } else {
    storage_access_value_outcome =
        ConvertSecFetchStorageAccessHeaderValueToOutcome(
            storage_access_status.value());
  }

  base::UmaHistogramEnumeration(
      "API.StorageAccessHeader.SecFetchStorageAccessValueOutcome",
      storage_access_value_outcome.value());

  return storage_access_status;
}

void URLRequest::SetSharedDictionaryGetter(
    SharedDictionaryGetter shared_dictionary_getter) {
  CHECK(!job_.get());
  CHECK(shared_dictionary_getter_.is_null());
  shared_dictionary_getter_ = std::move(shared_dictionary_getter);
}

base::WeakPtr<URLRequest> URLRequest::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace net
