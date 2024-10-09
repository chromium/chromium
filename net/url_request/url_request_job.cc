// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "net/base/auth.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/base/proxy_chain.h"
#include "net/base/schemeful_site.h"
#include "net/cert/x509_certificate.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/ssl/ssl_private_key.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request_context.h"

namespace net {

namespace {

// Callback for TYPE_URL_REQUEST_FILTERS_SET net-internals event.
base::Value::Dict SourceStreamSetParams(SourceStream* source_stream) {
  base::Value::Dict event_params;
  event_params.Set("filters", source_stream->Description());
  return event_params;
}

}  // namespace

// Each SourceStreams own the previous SourceStream in the chain, but the
// ultimate source is URLRequestJob, which has other ownership semantics, so
// this class is a proxy for URLRequestJob that is owned by the first stream
// (in dataflow order).
class URLRequestJob::URLRequestJobSourceStream : public SourceStream {
 public:
  explicit URLRequestJobSourceStream(URLRequestJob* job)
      : SourceStream(SourceStream::TYPE_NONE), job_(job) {
    DCHECK(job_);
  }

  URLRequestJobSourceStream(const URLRequestJobSourceStream&) = delete;
  URLRequestJobSourceStream& operator=(const URLRequestJobSourceStream&) =
      delete;

  ~URLRequestJobSourceStream() override = default;

  // SourceStream implementation:
  int Read(IOBuffer* dest_buffer,
           int buffer_size,
           CompletionOnceCallback callback) override {
    DCHECK(job_);
    return job_->ReadRawDataHelper(dest_buffer, buffer_size,
                                   std::move(callback));
  }

  std::string Description() const override { return std::string(); }

  bool MayHaveMoreBytes() const override { return true; }

 private:
  // It is safe to keep a raw pointer because |job_| owns the last stream which
  // indirectly owns |this|. Therefore, |job_| will not be destroyed when |this|
  // is alive.
  const raw_ptr<URLRequestJob> job_;
};

URLRequestJob::URLRequestJob(URLRequest* request) : request_(request) {}

URLRequestJob::~URLRequestJob() = default;

void URLRequestJob::SetUpload(UploadDataStream* upload) {
}

void URLRequestJob::SetExtraRequestHeaders(const HttpRequestHeaders& headers) {
}

void URLRequestJob::SetPriority(RequestPriority priority) {
}

void URLRequestJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  // Make sure the URLRequest is notified that the job is done.  This assumes
  // that the URLRequest took care of setting its error status before calling
  // Kill().
  // TODO(mmenke):  The URLRequest is currently deleted before this method
  // invokes its async callback whenever this is called by the URLRequest.
  // Try to simplify how cancellation works.
  NotifyCanceled();
}

// This method passes reads down the filter chain, where they eventually end up
// at URLRequestJobSourceStream::Read, which calls back into
// URLRequestJob::ReadRawData.
int URLRequestJob::Read(IOBuffer* buf, int buf_size) {
  DCHECK(buf);

  pending_read_buffer_ = buf;
  int result = source_stream_->Read(
      buf, buf_size,
      base::BindOnce(&URLRequestJob::SourceStreamReadComplete,
                     weak_factory_.GetWeakPtr(), false));
  if (result == ERR_IO_PENDING)
    return ERR_IO_PENDING;

  SourceStreamReadComplete(true, result);
  return result;
}

int64_t URLRequestJob::GetTotalReceivedBytes() const {
  return 0;
}

int64_t URLRequestJob::GetTotalSentBytes() const {
  return 0;
}

int64_t URLRequestJob::GetReceivedBodyBytes() const {
  return 0;
}

LoadState URLRequestJob::GetLoadState() const {
  return LOAD_STATE_IDLE;
}

bool URLRequestJob::GetCharset(std::string* charset) {
  return false;
}

void URLRequestJob::GetResponseInfo(HttpResponseInfo* info) {
}

void URLRequestJob::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  // Only certain request types return more than just request start times.
}

bool URLRequestJob::GetTransactionRemoteEndpoint(IPEndPoint* endpoint) const {
  return false;
}

void URLRequestJob::PopulateNetErrorDetails(NetErrorDetails* details) const {
  return;
}

bool URLRequestJob::IsRedirectResponse(GURL* location,
                                       int* http_status_code,
                                       bool* insecure_scheme_was_upgraded) {
  // For non-HTTP jobs, headers will be null.
  HttpResponseHeaders* headers = request_->response_headers();
  if (!headers)
    return false;

  std::string value;
  if (!headers->IsRedirect(&value))
    return false;
  *insecure_scheme_was_upgraded = false;
  *location = request_->url().Resolve(value);
  // If this a redirect to HTTP of a request that had the
  // 'upgrade-insecure-requests' policy set, upgrade it to HTTPS.
  if (request_->upgrade_if_insecure()) {
    if (location->SchemeIs("http")) {
      *insecure_scheme_was_upgraded = true;
      GURL::Replacements replacements;
      replacements.SetSchemeStr("https");
      *location = location->ReplaceComponents(replacements);
    }
  }
  *http_status_code = headers->response_code();
  return true;
}

bool URLRequestJob::CopyFragmentOnRedirect(const GURL& location) const {
  return true;
}

bool URLRequestJob::IsSafeRedirect(const GURL& location) {
  return true;
}

bool URLRequestJob::NeedsAuth() {
  return false;
}

std::unique_ptr<AuthChallengeInfo> URLRequestJob::GetAuthChallengeInfo() {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void URLRequestJob::SetAuth(const AuthCredentials& credentials) {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED_IN_MIGRATION();
}

void URLRequestJob::CancelAuth() {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED_IN_MIGRATION();
}

void URLRequestJob::ContinueWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key) {
  // The derived class should implement this!
  NOTREACHED_IN_MIGRATION();
}

void URLRequestJob::ContinueDespiteLastError() {
  // Implementations should know how to recover from errors they generate.
  // If this code was reached, we are trying to recover from an error that
  // we don't know how to recover from.
  NOTREACHED_IN_MIGRATION();
}

void URLRequestJob::FollowDeferredRedirect(
    const std::optional<std::vector<std::string>>& removed_headers,
    const std::optional<net::HttpRequestHeaders>& modified_headers) {
  // OnReceivedRedirect must have been called.
  DCHECK(deferred_redirect_info_);

  // It is possible that FollowRedirect will delete |this|, so it is not safe to
  // pass along a reference to |deferred_redirect_info_|.
  std::optional<RedirectInfo> redirect_info =
      std::move(deferred_redirect_info_);
  FollowRedirect(*redirect_info, removed_headers, modified_headers);
}

int64_t URLRequestJob::prefilter_bytes_read() const {
  return prefilter_bytes_read_;
}

bool URLRequestJob::GetMimeType(std::string* mime_type) const {
  return false;
}

int URLRequestJob::GetResponseCode() const {
  HttpResponseHeaders* headers = request_->response_headers();
  if (!headers)
    return -1;
  return headers->response_code();
}

IPEndPoint URLRequestJob::GetResponseRemoteEndpoint() const {
  return IPEndPoint();
}

void URLRequestJob::NotifyURLRequestDestroyed() {
}

ConnectionAttempts URLRequestJob::GetConnectionAttempts() const {
  return {};
}

void URLRequestJob::CloseConnectionOnDestruction() {}

bool URLRequestJob::NeedsRetryWithStorageAccess() {
  return false;
}

namespace {

// Assuming |url| has already been stripped for use as a referrer, if
// |should_strip_to_origin| is true, this method returns the output of the
// "Strip `url` for use as a referrer" algorithm from the Referrer Policy spec
// with its "origin-only" flag set to true:
// https://w3c.github.io/webappsec-referrer-policy/#strip-url
GURL MaybeStripToOrigin(GURL url, bool should_strip_to_origin) {
  if (!should_strip_to_origin)
    return url;

  return url.DeprecatedGetOriginAsURL();
}

}  // namespace

// static
GURL URLRequestJob::ComputeReferrerForPolicy(
    ReferrerPolicy policy,
    const GURL& original_referrer,
    const GURL& destination,
    bool* same_origin_out_for_metrics) {
  // Here and below, numbered lines are from the Referrer Policy spec's
  // "Determine request's referrer" algorithm:
  // https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
  //
  // 4. Let referrerURL be the result of stripping referrerSource for use as a
  // referrer.
  GURL stripped_referrer = original_referrer.GetAsReferrer();

  // 5. Let referrerOrigin be the result of stripping referrerSource for use as
  // a referrer, with the origin-only flag set to true.
  //
  // (We use a boolean instead of computing the URL right away in order to avoid
  // constructing a new GURL when it's not necessary.)
  bool should_strip_to_origin = false;

  // 6. If the result of serializing referrerURL is a string whose length is
  // greater than 4096, set referrerURL to referrerOrigin.
  if (stripped_referrer.spec().size() > 4096)
    should_strip_to_origin = true;

  bool same_origin = url::IsSameOriginWith(original_referrer, destination);

  if (same_origin_out_for_metrics)
    *same_origin_out_for_metrics = same_origin;

  // 7. The user agent MAY alter referrerURL or referrerOrigin at this point to
  // enforce arbitrary policy considerations in the interests of minimizing data
  // leakage. For example, the user agent could strip the URL down to an origin,
  // modify its host, replace it with an empty string, etc.
  if (base::FeatureList::IsEnabled(
          features::kCapReferrerToOriginOnCrossOrigin) &&
      !same_origin) {
    should_strip_to_origin = true;
  }

  bool secure_referrer_but_insecure_destination =
      original_referrer.SchemeIsCryptographic() &&
      !destination.SchemeIsCryptographic();

  switch (policy) {
    case ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      if (secure_referrer_but_insecure_destination)
        return GURL();
      return MaybeStripToOrigin(std::move(stripped_referrer),
                                should_strip_to_origin);

    case ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      if (secure_referrer_but_insecure_destination)
        return GURL();
      if (!same_origin)
        should_strip_to_origin = true;
      return MaybeStripToOrigin(std::move(stripped_referrer),
                                should_strip_to_origin);

    case ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      if (!same_origin)
        should_strip_to_origin = true;
      return MaybeStripToOrigin(std::move(stripped_referrer),
                                should_strip_to_origin);

    case ReferrerPolicy::NEVER_CLEAR:
      return MaybeStripToOrigin(std::move(stripped_referrer),
                                should_strip_to_origin);

    case ReferrerPolicy::ORIGIN:
      should_strip_to_origin = true;
      return MaybeStripToOrigin(std::move(stripped_referrer),
                                should_strip_to_origin);

    case ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN:
      if (!same_origin)
        return GURL();
      return MaybeStripToOrigin(std::move(stripped_referrer),
                                should_strip_to_origin);

    case ReferrerPolicy::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      if (secure_referrer_but_insecure_destination)
        return GURL();
      should_strip_to_origin = true;
      return MaybeStripToOrigin(std::move(stripped_referrer),
                                should_strip_to_origin);

    case ReferrerPolicy::NO_REFERRER:
      return GURL();
  }

  NOTREACHED_IN_MIGRATION();
  return GURL();
}

int URLRequestJob::NotifyConnected(const TransportInfo& info,
                                   CompletionOnceCallback callback) {
  return request_->NotifyConnected(info, std::move(callback));
}

void URLRequestJob::NotifyCertificateRequested(
    SSLCertRequestInfo* cert_request_info) {
  request_->NotifyCertificateRequested(cert_request_info);
}

void URLRequestJob::NotifySSLCertificateError(int net_error,
                                              const SSLInfo& ssl_info,
                                              bool fatal) {
  request_->NotifySSLCertificateError(net_error, ssl_info, fatal);
}

bool URLRequestJob::CanSetCookie(
    const net::CanonicalCookie& cookie,
    CookieOptions* options,
    const net::FirstPartySetMetadata& first_party_set_metadata,
    CookieInclusionStatus* inclusion_status) const {
  return request_->CanSetCookie(cookie, options, first_party_set_metadata,
                                inclusion_status);
}

void URLRequestJob::NotifyHeadersComplete() {
  if (has_handled_response_)
    return;

  // Initialize to the current time, and let the subclass optionally override
  // the time stamps if it has that information.  The default request_time is
  // set by URLRequest before it calls our Start method.
  request_->response_info_.response_time = base::Time::Now();
  GetResponseInfo(&request_->response_info_);

  request_->OnHeadersComplete();

  GURL new_location;
  int http_status_code;
  bool insecure_scheme_was_upgraded;

  if (NeedsAuth()) {
    CHECK(!IsRedirectResponse(&new_location, &http_status_code,
                              &insecure_scheme_was_upgraded));
    std::unique_ptr<AuthChallengeInfo> auth_info = GetAuthChallengeInfo();
    // Need to check for a NULL auth_info because the server may have failed
    // to send a challenge with the 401 response.
    if (auth_info) {
      request_->NotifyAuthRequired(std::move(auth_info));
      // Wait for SetAuth or CancelAuth to be called.
      return;
    }
  }

  if (NeedsRetryWithStorageAccess()) {
    DoneReadingRetryResponse();
    request_->RetryWithStorageAccess();
    return;
  }

  if (IsRedirectResponse(&new_location, &http_status_code,
                         &insecure_scheme_was_upgraded)) {
    CHECK(!NeedsAuth());
    // Redirect response bodies are not read. Notify the transaction
    // so it does not treat being stopped as an error.
    DoneReadingRedirectResponse();

    // Invalid redirect targets are failed early before
    // NotifyReceivedRedirect. This means the delegate can assume that, if it
    // accepts the redirect, future calls to OnResponseStarted correspond to
    // |redirect_info.new_url|.
    int redirect_check_result = CanFollowRedirect(new_location);
    if (redirect_check_result != OK) {
      OnDone(redirect_check_result, true /* notify_done */);
      return;
    }

    // When notifying the URLRequest::Delegate, it can destroy the request,
    // which will destroy |this|.  After calling to the URLRequest::Delegate,
    // pointer must be checked to see if |this| still exists, and if not, the
    // code must return immediately.
    base::WeakPtr<URLRequestJob> weak_this(weak_factory_.GetWeakPtr());

    RedirectInfo redirect_info = RedirectInfo::ComputeRedirectInfo(
        request_->method(), request_->url(), request_->site_for_cookies(),
        request_->first_party_url_policy(), request_->referrer_policy(),
        request_->referrer(), http_status_code, new_location,
        net::RedirectUtil::GetReferrerPolicyHeader(
            request_->response_headers()),
        insecure_scheme_was_upgraded, CopyFragmentOnRedirect(new_location));
    bool defer_redirect = false;
    request_->NotifyReceivedRedirect(redirect_info, &defer_redirect);

    // Ensure that the request wasn't detached, destroyed, or canceled in
    // NotifyReceivedRedirect.
    if (!weak_this || request_->failed())
      return;

    if (defer_redirect) {
      deferred_redirect_info_ = std::move(redirect_info);
    } else {
      FollowRedirect(redirect_info, std::nullopt, /*  removed_headers */
                     std::nullopt /* modified_headers */);
    }
    return;
  }

  NotifyFinalHeadersReceived();
  // |this| may be destroyed at this point.
}

void URLRequestJob::NotifyFinalHeadersReceived() {
  DCHECK(!NeedsAuth() || !GetAuthChallengeInfo());

  if (has_handled_response_)
    return;

  // While the request's status is normally updated in NotifyHeadersComplete(),
  // URLRequestHttpJob::CancelAuth() posts a task to invoke this method
  // directly, which bypasses that logic.
  if (request_->status() == ERR_IO_PENDING)
    request_->set_status(OK);

  has_handled_response_ = true;
  if (request_->status() == OK) {
    DCHECK(!source_stream_);
    source_stream_ = SetUpSourceStream();

    if (!source_stream_) {
      OnDone(ERR_CONTENT_DECODING_INIT_FAILED, true /* notify_done */);
      return;
    }
    if (source_stream_->type() == SourceStream::TYPE_NONE) {
      // If the subclass didn't set |expected_content_size|, and there are
      // headers, and the response body is not compressed, try to get the
      // expected content size from the headers.
      if (expected_content_size_ == -1 && request_->response_headers()) {
        // This sets |expected_content_size_| to its previous value of -1 if
        // there's no Content-Length header.
        expected_content_size_ =
            request_->response_headers()->GetContentLength();
      }
    } else {
      request_->net_log().AddEvent(
          NetLogEventType::URL_REQUEST_FILTERS_SET,
          [&] { return SourceStreamSetParams(source_stream_.get()); });
    }
  }

  request_->NotifyResponseStarted(OK);
  // |this| may be destroyed at this point.
}

void URLRequestJob::ConvertResultToError(int result, Error* error, int* count) {
  if (result >= 0) {
    *error = OK;
    *count = result;
  } else {
    *error = static_cast<Error>(result);
    *count = 0;
  }
}

void URLRequestJob::ReadRawDataComplete(int result) {
  DCHECK_EQ(ERR_IO_PENDING, request_->status());
  DCHECK_NE(ERR_IO_PENDING, result);

  // The headers should be complete before reads complete
  DCHECK(has_handled_response_);

  GatherRawReadStats(result);

  // Notify SourceStream.
  DCHECK(!read_raw_callback_.is_null());

  std::move(read_raw_callback_).Run(result);
  // |this| may be destroyed at this point.
}

void URLRequestJob::NotifyStartError(int net_error) {
  DCHECK(!has_handled_response_);
  DCHECK_EQ(ERR_IO_PENDING, request_->status());

  has_handled_response_ = true;
  // There may be relevant information in the response info even in the
  // error case.
  GetResponseInfo(&request_->response_info_);

  request_->NotifyResponseStarted(net_error);
  // |this| may have been deleted here.
}

void URLRequestJob::OnDone(int net_error, bool notify_done) {
  DCHECK_NE(ERR_IO_PENDING, net_error);
  DCHECK(!done_) << "Job sending done notification twice";
  if (done_)
    return;
  done_ = true;

  // Unless there was an error, we should have at least tried to handle
  // the response before getting here.
  DCHECK(has_handled_response_ || net_error != OK);

  request_->set_is_pending(false);
  // With async IO, it's quite possible to have a few outstanding
  // requests.  We could receive a request to Cancel, followed shortly
  // by a successful IO.  For tracking the status(), once there is
  // an error, we do not change the status back to success.  To
  // enforce this, only set the status if the job is so far
  // successful.
  if (!request_->failed()) {
    if (net_error != net::OK && net_error != ERR_ABORTED) {
      request_->net_log().AddEventWithNetErrorCode(NetLogEventType::FAILED,
                                                   net_error);
    }
    request_->set_status(net_error);
  }

  if (notify_done) {
    // Complete this notification later.  This prevents us from re-entering the
    // delegate if we're done because of a synchronous call.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&URLRequestJob::NotifyDone, weak_factory_.GetWeakPtr()));
  }
}

void URLRequestJob::NotifyDone() {
  // Check if we should notify the URLRequest that we're done because of an
  // error.
  if (request_->failed()) {
    // We report the error differently depending on whether we've called
    // OnResponseStarted yet.
    if (has_handled_response_) {
      // We signal the error by calling OnReadComplete with a bytes_read of -1.
      request_->NotifyReadCompleted(-1);
    } else {
      has_handled_response_ = true;
      // Error code doesn't actually matter here, since the status has already
      // been updated.
      request_->NotifyResponseStarted(request_->status());
    }
  }
}

void URLRequestJob::NotifyCanceled() {
  if (!done_)
    OnDone(ERR_ABORTED, true /* notify_done */);
}

void URLRequestJob::OnCallToDelegate(NetLogEventType type) {
  request_->OnCallToDelegate(type);
}

void URLRequestJob::OnCallToDelegateComplete() {
  request_->OnCallToDelegateComplete();
}

int URLRequestJob::ReadRawData(IOBuffer* buf, int buf_size) {
  return 0;
}

void URLRequestJob::DoneReading() {
  // Do nothing.
}

void URLRequestJob::DoneReadingRedirectResponse() {
}

void URLRequestJob::DoneReadingRetryResponse() {}

std::unique_ptr<SourceStream> URLRequestJob::SetUpSourceStream() {
  return std::make_unique<URLRequestJobSourceStream>(this);
}

void URLRequestJob::SetProxyChain(const ProxyChain& proxy_chain) {
  request_->proxy_chain_ = proxy_chain;
}

void URLRequestJob::SourceStreamReadComplete(bool synchronous, int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  if (result > 0 && request()->net_log().IsCapturing()) {
    request()->net_log().AddByteTransferEvent(
        NetLogEventType::URL_REQUEST_JOB_FILTERED_BYTES_READ, result,
        pending_read_buffer_->data());
  }
  pending_read_buffer_ = nullptr;

  if (result < 0) {
    OnDone(result, !synchronous /* notify_done */);
    return;
  }

  if (result > 0) {
    postfilter_bytes_read_ += result;
  } else {
    DCHECK_EQ(0, result);
    DoneReading();
    // In the synchronous case, the caller will notify the URLRequest of
    // completion. In the async case, the NotifyReadCompleted call will.
    // TODO(mmenke): Can this be combined with the error case?
    OnDone(OK, false /* notify_done */);
  }

  if (!synchronous)
    request_->NotifyReadCompleted(result);
}

int URLRequestJob::ReadRawDataHelper(IOBuffer* buf,
                                     int buf_size,
                                     CompletionOnceCallback callback) {
  DCHECK(!raw_read_buffer_);

  // Keep a pointer to the read buffer, so URLRequestJob::GatherRawReadStats()
  // has access to it to log stats.
  raw_read_buffer_ = buf;

  // TODO(xunjieli): Make ReadRawData take in a callback rather than requiring
  // subclass to call ReadRawDataComplete upon asynchronous completion.
  int result = ReadRawData(buf, buf_size);

  if (result != ERR_IO_PENDING) {
    // If the read completes synchronously, either success or failure, invoke
    // GatherRawReadStats so we can account for the completed read.
    GatherRawReadStats(result);
  } else {
    read_raw_callback_ = std::move(callback);
  }
  return result;
}

int URLRequestJob::CanFollowRedirect(const GURL& new_url) {
  if (request_->redirect_limit_ <= 0) {
    DVLOG(1) << "disallowing redirect: exceeds limit";
    return ERR_TOO_MANY_REDIRECTS;
  }

  if (!new_url.is_valid())
    return ERR_INVALID_REDIRECT;

  if (!IsSafeRedirect(new_url)) {
    DVLOG(1) << "disallowing redirect: unsafe protocol";
    return ERR_UNSAFE_REDIRECT;
  }

  return OK;
}

void URLRequestJob::FollowRedirect(
    const RedirectInfo& redirect_info,
    const std::optional<std::vector<std::string>>& removed_headers,
    const std::optional<net::HttpRequestHeaders>& modified_headers) {
  request_->Redirect(redirect_info, removed_headers, modified_headers);
}

void URLRequestJob::GatherRawReadStats(int bytes_read) {
  DCHECK(raw_read_buffer_ || bytes_read == 0);
  DCHECK_NE(ERR_IO_PENDING, bytes_read);

  if (bytes_read > 0) {
    // If there is a filter, bytes will be logged after the filter is applied.
    if (source_stream_->type() != SourceStream::TYPE_NONE &&
        request()->net_log().IsCapturing()) {
      request()->net_log().AddByteTransferEvent(
          NetLogEventType::URL_REQUEST_JOB_BYTES_READ, bytes_read,
          raw_read_buffer_->data());
    }
    RecordBytesRead(bytes_read);
  }
  raw_read_buffer_ = nullptr;
}

void URLRequestJob::RecordBytesRead(int bytes_read) {
  DCHECK_GT(bytes_read, 0);
  prefilter_bytes_read_ += base::checked_cast<size_t>(bytes_read);

  // On first read, notify NetworkQualityEstimator that response headers have
  // been received.
  // TODO(tbansal): Move this to url_request_http_job.cc. This may catch
  // Service Worker jobs twice.
  // If prefilter_bytes_read_ is equal to bytes_read, it indicates this is the
  // first raw read of the response body. This is used as the signal that
  // response headers have been received.
  if (request_->context()->network_quality_estimator()) {
    if (prefilter_bytes_read() == bytes_read) {
      request_->context()->network_quality_estimator()->NotifyHeadersReceived(
          *request_, prefilter_bytes_read());
    } else {
      request_->context()->network_quality_estimator()->NotifyBytesRead(
          *request_, prefilter_bytes_read());
    }
  }

  DVLOG(2) << __FUNCTION__ << "() "
           << "\"" << request_->url().spec() << "\""
           << " pre bytes read = " << bytes_read
           << " pre total = " << prefilter_bytes_read()
           << " post total = " << postfilter_bytes_read();
}

}  // namespace net
