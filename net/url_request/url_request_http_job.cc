// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_version_info.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/base/network_isolation_key.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/trace_constants.h"
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/known_roots.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/filter/brotli_source_stream.h"
#include "net/filter/filter_source_stream.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/source_stream.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_redirect_job.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "net/url_request/websocket_handshake_userdata_key.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace {

base::Value CookieExcludedNetLogParams(const std::string& operation,
                                       const std::string& cookie_name,
                                       const std::string& exclusion_reason,
                                       net::NetLogCaptureMode capture_mode) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("operation", operation);
  dict.SetStringKey("exclusion_reason", exclusion_reason);
  if (net::NetLogCaptureIncludesSensitive(capture_mode) &&
      !cookie_name.empty()) {
    dict.SetStringKey("name", cookie_name);
  }
  return dict;
}

// Records details about the most-specific trust anchor in |spki_hashes|,
// which is expected to be ordered with the leaf cert first and the root cert
// last. This complements the per-verification histogram
// Net.Certificate.TrustAnchor.Verify
void LogTrustAnchor(const net::HashValueVector& spki_hashes) {
  // Don't record metrics if there are no hashes; this is true if the HTTP
  // load did not come from an active network connection, such as the disk
  // cache or a synthesized response.
  if (spki_hashes.empty())
    return;

  int32_t id = 0;
  for (const auto& hash : spki_hashes) {
    id = net::GetNetTrustAnchorHistogramIdForSPKI(hash);
    if (id != 0)
      break;
  }
  base::UmaHistogramSparse("Net.Certificate.TrustAnchor.Request", id);
}

// Records per-request histograms relating to Certificate Transparency
// compliance.
void RecordCTHistograms(const net::SSLInfo& ssl_info) {
  if (ssl_info.ct_policy_compliance ==
      net::ct::CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE) {
    return;
  }
  if (!ssl_info.is_issued_by_known_root)
    return;

  // Connections with major errors other than CERTIFICATE_TRANSPARENCY_REQUIRED
  // would have failed anyway, so do not record these histograms for such
  // requests.
  net::CertStatus other_errors =
      ssl_info.cert_status &
      ~net::CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED;
  if (net::IsCertStatusError(other_errors))
    return;

  // Record the CT compliance of each request, to give a picture of the
  // percentage of overall requests that are CT-compliant.
  UMA_HISTOGRAM_ENUMERATION(
      "Net.CertificateTransparency.RequestComplianceStatus",
      ssl_info.ct_policy_compliance,
      net::ct::CTPolicyCompliance::CT_POLICY_COUNT);
  // Record the CT compliance of each request which was required to be CT
  // compliant. This gives a picture of the sites that are supposed to be
  // compliant and how well they do at actually being compliant.
  if (ssl_info.ct_policy_compliance_required) {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.CertificateTransparency.CTRequiredRequestComplianceStatus",
        ssl_info.ct_policy_compliance,
        net::ct::CTPolicyCompliance::CT_POLICY_COUNT);
  }
}

}  // namespace

namespace net {

// TODO(darin): make sure the port blocking code is not lost
// static
URLRequestJob* URLRequestHttpJob::Factory(URLRequest* request,
                                          NetworkDelegate* network_delegate,
                                          const std::string& scheme) {
  DCHECK(scheme == "http" || scheme == "https" || scheme == "ws" ||
         scheme == "wss");

  if (!request->context()->http_transaction_factory()) {
    NOTREACHED() << "requires a valid context";
    return new URLRequestErrorJob(
        request, network_delegate, ERR_INVALID_ARGUMENT);
  }

  const GURL& url = request->url();

  // Check for reasons not to return a URLRequestHttpJob. These don't apply to
  // https and wss requests.
  if (!url.SchemeIsCryptographic()) {
    // Check for HSTS upgrade.
    TransportSecurityState* hsts =
        request->context()->transport_security_state();
    if (hsts && hsts->ShouldUpgradeToSSL(url.host())) {
      GURL::Replacements replacements;
      replacements.SetSchemeStr(

          url.SchemeIs(url::kHttpScheme) ? url::kHttpsScheme : url::kWssScheme);
      return new URLRequestRedirectJob(
          request, network_delegate, url.ReplaceComponents(replacements),
          // Use status code 307 to preserve the method, so POST requests work.
          URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT, "HSTS");
    }

#if defined(OS_ANDROID)
    // Check whether the app allows cleartext traffic to this host, and return
    // ERR_CLEARTEXT_NOT_PERMITTED if not.
    if (request->context()->check_cleartext_permitted() &&
        !android::IsCleartextPermitted(url.host())) {
      return new URLRequestErrorJob(request, network_delegate,
                                    ERR_CLEARTEXT_NOT_PERMITTED);
    }
#endif
  }

  return new URLRequestHttpJob(request,
                               network_delegate,
                               request->context()->http_user_agent_settings());
}

URLRequestHttpJob::URLRequestHttpJob(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    const HttpUserAgentSettings* http_user_agent_settings)
    : URLRequestJob(request, network_delegate),
      num_cookie_lines_left_(0),
      priority_(DEFAULT_PRIORITY),
      response_info_(nullptr),
      proxy_auth_state_(AUTH_STATE_DONT_NEED_AUTH),
      server_auth_state_(AUTH_STATE_DONT_NEED_AUTH),
      read_in_progress_(false),
      throttling_entry_(nullptr),
      done_(false),
      awaiting_callback_(false),
      http_user_agent_settings_(http_user_agent_settings),
      total_received_bytes_from_previous_transactions_(0),
      total_sent_bytes_from_previous_transactions_(0) {
  URLRequestThrottlerManager* manager = request->context()->throttler_manager();
  if (manager)
    throttling_entry_ = manager->RegisterRequestUrl(request->url());

  ResetTimer();
}

URLRequestHttpJob::~URLRequestHttpJob() {
  CHECK(!awaiting_callback_);

  DoneWithRequest(ABORTED);
}

void URLRequestHttpJob::SetPriority(RequestPriority priority) {
  priority_ = priority;
  if (transaction_)
    transaction_->SetPriority(priority_);
}

void URLRequestHttpJob::Start() {
  DCHECK(!transaction_.get());

  // URLRequest::SetReferrer ensures that we do not send username and password
  // fields in the referrer.
  GURL referrer(request_->referrer());

  request_info_.url = request_->url();
  request_info_.method = request_->method();

  request_info_.network_isolation_key = request_->network_isolation_key();
  request_info_.load_flags = request_->load_flags();
  request_info_.disable_secure_dns = request_->disable_secure_dns();
  request_info_.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(request_->traffic_annotation());
  request_info_.socket_tag = request_->socket_tag();
#if BUILDFLAG(ENABLE_REPORTING)
  request_info_.reporting_upload_depth = request_->reporting_upload_depth();
#endif

  // Privacy mode could still be disabled in SetCookieHeaderAndStart if we are
  // going to send previously saved cookies.
  request_info_.privacy_mode = privacy_mode();

  // Strip Referer from request_info_.extra_headers to prevent, e.g., plugins
  // from overriding headers that are controlled using other means. Otherwise a
  // plugin could set a referrer although sending the referrer is inhibited.
  request_info_.extra_headers.RemoveHeader(HttpRequestHeaders::kReferer);

  // Our consumer should have made sure that this is a safe referrer. See for
  // instance WebCore::FrameLoader::HideReferrer.
  if (referrer.is_valid()) {
    std::string referer_value = referrer.spec();
    UMA_HISTOGRAM_COUNTS_10000("Referrer.HeaderLength", referer_value.length());
    if (base::FeatureList::IsEnabled(features::kCapRefererHeaderLength) &&
        base::saturated_cast<int>(referer_value.length()) >
            features::kMaxRefererHeaderLength.Get()) {
      // Strip the referrer down to its origin, but ensure that it's serialized
      // as a URL (e.g. retaining a trailing `/` character).
      referer_value = url::Origin::Create(referrer).GetURL().spec();
    }
    request_info_.extra_headers.SetHeader(HttpRequestHeaders::kReferer,
                                          referer_value);
  }

  request_info_.extra_headers.SetHeaderIfMissing(
      HttpRequestHeaders::kUserAgent,
      http_user_agent_settings_ ?
          http_user_agent_settings_->GetUserAgent() : std::string());

  AddExtraHeaders();
  AddCookieHeaderAndStart();
}

void URLRequestHttpJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  if (transaction_)
    DestroyTransaction();
  URLRequestJob::Kill();
}

void URLRequestHttpJob::GetConnectionAttempts(ConnectionAttempts* out) const {
  if (transaction_)
    transaction_->GetConnectionAttempts(out);
  else
    out->clear();
}

void URLRequestHttpJob::NotifyBeforeSendHeadersCallback(
    const ProxyInfo& proxy_info,
    HttpRequestHeaders* request_headers) {
  DCHECK(request_headers);
  DCHECK_NE(URLRequestStatus::CANCELED, GetStatus().status());
  if (proxy_info.is_empty()) {
    SetProxyServer(ProxyServer::Direct());
  } else {
    SetProxyServer(proxy_info.proxy_server());
  }
  if (network_delegate()) {
    network_delegate()->NotifyBeforeSendHeaders(
        request_, proxy_info,
        request_->context()->proxy_resolution_service()->proxy_retry_info(),
        request_headers);
  }
}

void URLRequestHttpJob::NotifyHeadersComplete() {
  DCHECK(!response_info_);
  DCHECK_EQ(0, num_cookie_lines_left_);
  DCHECK(request_->maybe_stored_cookies().empty());

  response_info_ = transaction_->GetResponseInfo();

  if (!response_info_->was_cached && throttling_entry_.get())
    throttling_entry_->UpdateWithResponse(GetResponseCode());

  // The ordering of these calls is not important.
  ProcessStrictTransportSecurityHeader();
  ProcessExpectCTHeader();

  // Clear |set_cookie_status_list_| after any processing in case
  // SaveCookiesAndNotifyHeadersComplete is called again.
  request_->set_maybe_stored_cookies(std::move(set_cookie_status_list_));

  // The HTTP transaction may be restarted several times for the purposes
  // of sending authorization information. Each time it restarts, we get
  // notified of the headers completion so that we can update the cookie store.
  if (transaction_->IsReadyToRestartForAuth()) {
    // TODO(battre): This breaks the webrequest API for
    // URLRequestTestHTTP.BasicAuthWithCookies
    // where OnBeforeStartTransaction -> OnStartTransaction ->
    // OnBeforeStartTransaction occurs.
    RestartTransactionWithAuth(AuthCredentials());
    return;
  }

  URLRequestJob::NotifyHeadersComplete();
}

void URLRequestHttpJob::DestroyTransaction() {
  DCHECK(transaction_.get());

  DoneWithRequest(ABORTED);

  total_received_bytes_from_previous_transactions_ +=
      transaction_->GetTotalReceivedBytes();
  total_sent_bytes_from_previous_transactions_ +=
      transaction_->GetTotalSentBytes();
  transaction_.reset();
  response_info_ = nullptr;
  override_response_headers_ = nullptr;
  receive_headers_end_ = base::TimeTicks();
}

void URLRequestHttpJob::StartTransaction() {
  if (network_delegate()) {
    OnCallToDelegate(
        NetLogEventType::NETWORK_DELEGATE_BEFORE_START_TRANSACTION);
    // The NetworkDelegate must watch for OnRequestDestroyed and not modify
    // |extra_headers| after it's called.
    // TODO(mattm): change the API to remove the out-params and take the
    // results as params of the callback.
    int rv = network_delegate()->NotifyBeforeStartTransaction(
        request_,
        base::BindOnce(&URLRequestHttpJob::NotifyBeforeStartTransactionCallback,
                       weak_factory_.GetWeakPtr()),
        &request_info_.extra_headers);
    // If an extension blocks the request, we rely on the callback to
    // MaybeStartTransactionInternal().
    if (rv == ERR_IO_PENDING)
      return;
    MaybeStartTransactionInternal(rv);
    return;
  }
  StartTransactionInternal();
}

void URLRequestHttpJob::NotifyBeforeStartTransactionCallback(int result) {
  // Check that there are no callbacks to already canceled requests.
  DCHECK_NE(URLRequestStatus::CANCELED, GetStatus().status());

  MaybeStartTransactionInternal(result);
}

void URLRequestHttpJob::MaybeStartTransactionInternal(int result) {
  OnCallToDelegateComplete();
  if (result == OK) {
    StartTransactionInternal();
  } else {
    request_->net_log().AddEventWithStringParams(NetLogEventType::CANCELLED,
                                                 "source", "delegate");
    // Don't call back synchronously to the delegate.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&URLRequestHttpJob::NotifyStartError,
                       weak_factory_.GetWeakPtr(),
                       URLRequestStatus(URLRequestStatus::FAILED, result)));
  }
}

void URLRequestHttpJob::StartTransactionInternal() {
  // This should only be called while the request's status is IO_PENDING.
  DCHECK_EQ(URLRequestStatus::IO_PENDING, request_->status().status());
  DCHECK(!override_response_headers_);

  // NOTE: This method assumes that request_info_ is already setup properly.

  // If we already have a transaction, then we should restart the transaction
  // with auth provided by auth_credentials_.

  int rv;

  // Notify NetworkQualityEstimator.
  NetworkQualityEstimator* network_quality_estimator =
      request()->context()->network_quality_estimator();
  if (network_quality_estimator)
    network_quality_estimator->NotifyStartTransaction(*request_);

  if (transaction_.get()) {
    rv = transaction_->RestartWithAuth(
        auth_credentials_, base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                                          base::Unretained(this)));
    auth_credentials_ = AuthCredentials();
  } else {
    DCHECK(request_->context()->http_transaction_factory());

    rv = request_->context()->http_transaction_factory()->CreateTransaction(
        priority_, &transaction_);

    if (rv == OK && request_info_.url.SchemeIsWSOrWSS()) {
      base::SupportsUserData::Data* data =
          request_->GetUserData(kWebSocketHandshakeUserDataKey);
      if (data) {
        transaction_->SetWebSocketHandshakeStreamCreateHelper(
            static_cast<WebSocketHandshakeStreamBase::CreateHelper*>(data));
      } else {
        rv = ERR_DISALLOWED_URL_SCHEME;
      }
    }

    if (rv == OK) {
      transaction_->SetBeforeHeadersSentCallback(
          base::Bind(&URLRequestHttpJob::NotifyBeforeSendHeadersCallback,
                     base::Unretained(this)));
      transaction_->SetRequestHeadersCallback(request_headers_callback_);
      transaction_->SetResponseHeadersCallback(response_headers_callback_);

      if (!throttling_entry_.get() ||
          !throttling_entry_->ShouldRejectRequest(*request_)) {
        rv = transaction_->Start(
            &request_info_,
            base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                           base::Unretained(this)),
            request_->net_log());
        start_time_ = base::TimeTicks::Now();
      } else {
        // Special error code for the exponential back-off module.
        rv = ERR_TEMPORARILY_THROTTLED;
      }
    }
  }

  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                                weak_factory_.GetWeakPtr(), rv));
}

void URLRequestHttpJob::AddExtraHeaders() {
  if (!request_info_.extra_headers.HasHeader(
          HttpRequestHeaders::kAcceptEncoding)) {
    // If a range is specifically requested, set the "Accepted Encoding" header
    // to "identity"
    if (request_info_.extra_headers.HasHeader(HttpRequestHeaders::kRange)) {
      request_info_.extra_headers.SetHeader(HttpRequestHeaders::kAcceptEncoding,
                                            "identity");
    } else {
      // Advertise "br" encoding only if transferred data is opaque to proxy.
      bool advertise_brotli = false;
      if (request()->context()->enable_brotli()) {
        if (request()->url().SchemeIsCryptographic() ||
            IsLocalhost(request()->url())) {
          advertise_brotli = true;
        }
      }

      // Supply Accept-Encoding headers first so that it is more likely that
      // they will be in the first transmitted packet. This can sometimes make
      // it easier to filter and analyze the streams to assure that a proxy has
      // not damaged these headers. Some proxies deliberately corrupt
      // Accept-Encoding headers.
      std::string advertised_encodings = "gzip, deflate";
      if (advertise_brotli)
        advertised_encodings += ", br";
      // Tell the server what compression formats are supported.
      request_info_.extra_headers.SetHeader(HttpRequestHeaders::kAcceptEncoding,
                                            advertised_encodings);
    }
  }

  if (http_user_agent_settings_) {
    // Only add default Accept-Language if the request didn't have it
    // specified.
    std::string accept_language =
        http_user_agent_settings_->GetAcceptLanguage();
    if (base::FeatureList::IsEnabled(features::kAcceptLanguageHeader) &&
        !accept_language.empty()) {
      request_info_.extra_headers.SetHeaderIfMissing(
          HttpRequestHeaders::kAcceptLanguage,
          accept_language);
    }
  }
}

void URLRequestHttpJob::AddCookieHeaderAndStart() {
  CookieStore* cookie_store = request_->context()->cookie_store();
  if (cookie_store && !(request_info_.load_flags & LOAD_DO_NOT_SEND_COOKIES)) {
    CookieOptions options;
    options.set_return_excluded_cookies();
    options.set_include_httponly();
    bool attach_same_site_cookies = request_->attach_same_site_cookies();
    if (cookie_store->cookie_access_delegate() &&
        cookie_store->cookie_access_delegate()
            ->ShouldIgnoreSameSiteRestrictions(request_->url(),
                                               request_->site_for_cookies())) {
      attach_same_site_cookies = true;
    }
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForRequest(
            request_->method(), request_->url(), request_->site_for_cookies(),
            request_->initiator(), attach_same_site_cookies));
    cookie_store->GetCookieListWithOptionsAsync(
        request_->url(), options,
        base::BindOnce(&URLRequestHttpJob::SetCookieHeaderAndStart,
                       weak_factory_.GetWeakPtr(), options));
  } else {
    StartTransaction();
  }
}

void URLRequestHttpJob::SetCookieHeaderAndStart(
    const CookieOptions& options,
    const CookieStatusList& cookies_with_status_list,
    const CookieStatusList& excluded_list) {
  DCHECK(request_->maybe_sent_cookies().empty());

  // TODO(chlily): This is just for passing to CanGetCookies(), however the
  // CookieList parameter of CanGetCookies(), which eventually gets passed to
  // the NetworkDelegate, never actually gets used anywhere except in tests. The
  // parameter should be removed.
  CookieList cookie_list =
      net::cookie_util::StripStatuses(cookies_with_status_list);

  bool can_get_cookies = CanGetCookies(cookie_list);
  if (!cookies_with_status_list.empty() && can_get_cookies) {
    std::string cookie_line =
        CanonicalCookie::BuildCookieLine(cookies_with_status_list);
    UMA_HISTOGRAM_COUNTS_10000("Cookie.HeaderLength", cookie_line.length());
    request_info_.extra_headers.SetHeader(HttpRequestHeaders::kCookie,
                                          cookie_line);

    // Disable privacy mode as we are sending cookies anyway.
    request_info_.privacy_mode = PRIVACY_MODE_DISABLED;
  }

  // Report status for things in |excluded_list| and |cookies_with_status_list|
  // after the delegate got a chance to block them.
  CookieStatusList maybe_sent_cookies = excluded_list;
  // CanGetCookies only looks at the fields of the URLRequest, not the cookies
  // it is passed, so if CanGetCookies(cookie_list) is false, then
  // CanGetCookies(excluded_list) would also be false, so tag also the
  // excluded cookies as having been blocked by user preferences.
  if (!can_get_cookies) {
    for (CookieStatusList::iterator it = maybe_sent_cookies.begin();
         it != maybe_sent_cookies.end(); ++it) {
      it->status.AddExclusionReason(
          CanonicalCookie::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
    }
  }
  for (const auto& cookie_with_status : cookies_with_status_list) {
    CanonicalCookie::CookieInclusionStatus status = cookie_with_status.status;
    if (!can_get_cookies) {
      status.AddExclusionReason(
          CanonicalCookie::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
    }
    maybe_sent_cookies.push_back({cookie_with_status.cookie, status});
  }

  if (request_->net_log().IsCapturing()) {
    for (const auto& cookie_and_status : maybe_sent_cookies) {
      if (!cookie_and_status.status.IsInclude()) {
        request_->net_log().AddEvent(
            NetLogEventType::COOKIE_INCLUSION_STATUS,
            [&](NetLogCaptureMode capture_mode) {
              return CookieExcludedNetLogParams(
                  "send", cookie_and_status.cookie.Name(),
                  cookie_and_status.status.GetDebugString(), capture_mode);
            });
      }
    }
  }

  request_->set_maybe_sent_cookies(std::move(maybe_sent_cookies));

  StartTransaction();
}

void URLRequestHttpJob::SaveCookiesAndNotifyHeadersComplete(int result) {
  DCHECK(set_cookie_status_list_.empty());
  DCHECK_EQ(0, num_cookie_lines_left_);

  // End of the call started in OnStartCompleted.
  OnCallToDelegateComplete();

  if (result != OK) {
    request_->net_log().AddEventWithStringParams(NetLogEventType::CANCELLED,
                                                 "source", "delegate");
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
    return;
  }

  CookieStore* cookie_store = request_->context()->cookie_store();

  if ((request_info_.load_flags & LOAD_DO_NOT_SAVE_COOKIES) || !cookie_store) {
    NotifyHeadersComplete();
    return;
  }

  base::Time response_date;
  base::Optional<base::Time> server_time = base::nullopt;
  if (GetResponseHeaders()->GetDateValue(&response_date))
    server_time = base::make_optional(response_date);

  CookieOptions options;
  options.set_include_httponly();
  bool attach_same_site_cookies = request_->attach_same_site_cookies();
  if (cookie_store->cookie_access_delegate() &&
      cookie_store->cookie_access_delegate()->ShouldIgnoreSameSiteRestrictions(
          request_->url(), request_->site_for_cookies())) {
    attach_same_site_cookies = true;
  }
  options.set_same_site_cookie_context(
      net::cookie_util::ComputeSameSiteContextForResponse(
          request_->url(), request_->site_for_cookies(), request_->initiator(),
          attach_same_site_cookies));

  options.set_return_excluded_cookies();

  // Set all cookies, without waiting for them to be set. Any subsequent read
  // will see the combined result of all cookie operation.
  const base::StringPiece name("Set-Cookie");
  std::string cookie_string;
  size_t iter = 0;
  HttpResponseHeaders* headers = GetResponseHeaders();

  // NotifyHeadersComplete needs to be called once and only once after the
  // list has been fully processed, and it can either be called in the
  // callback or after the loop is called, depending on how the last element
  // was handled. |num_cookie_lines_left_| keeps track of how many async
  // callbacks are currently out (starting from 1 to make sure the loop runs all
  // the way through before trying to exit). If there are any callbacks still
  // waiting when the loop ends, then NotifyHeadersComplete will be called when
  // it reaches 0 in the callback itself.
  num_cookie_lines_left_ = 1;
  while (headers->EnumerateHeader(&iter, name, &cookie_string)) {
    CanonicalCookie::CookieInclusionStatus returned_status;

    num_cookie_lines_left_++;

    std::unique_ptr<CanonicalCookie> cookie = net::CanonicalCookie::Create(
        request_->url(), cookie_string, base::Time::Now(), server_time,
        &returned_status);

    base::Optional<CanonicalCookie> cookie_to_return = base::nullopt;
    if (returned_status.IsInclude()) {
      DCHECK(cookie);
      // Make a copy of the cookie if we successfully made one.
      cookie_to_return = *cookie;
    }
    if (cookie && !CanSetCookie(*cookie, &options)) {
      returned_status.AddExclusionReason(
          CanonicalCookie::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
    }
    if (!returned_status.IsInclude()) {
      OnSetCookieResult(options, cookie_to_return, std::move(cookie_string),
                        returned_status);
      continue;
    }

    request_->context()->cookie_store()->SetCanonicalCookieAsync(
        std::move(cookie), request_->url().scheme(), options,
        base::BindOnce(&URLRequestHttpJob::OnSetCookieResult,
                       weak_factory_.GetWeakPtr(), options, cookie_to_return,
                       cookie_string));
  }
  // Removing the 1 that |num_cookie_lines_left| started with, signifing that
  // loop has been exited.
  num_cookie_lines_left_--;

  if (num_cookie_lines_left_ == 0)
    NotifyHeadersComplete();
}

void URLRequestHttpJob::OnSetCookieResult(
    const CookieOptions& options,
    base::Optional<CanonicalCookie> cookie,
    std::string cookie_string,
    CanonicalCookie::CookieInclusionStatus status) {
  if (!status.IsInclude() && request_->net_log().IsCapturing()) {
    request_->net_log().AddEvent(NetLogEventType::COOKIE_INCLUSION_STATUS,
                                 [&](NetLogCaptureMode capture_mode) {
                                   return CookieExcludedNetLogParams(
                                       "store",
                                       cookie ? cookie.value().Name() : "",
                                       status.GetDebugString(), capture_mode);
                                 });
  }
  set_cookie_status_list_.emplace_back(std::move(cookie),
                                       std::move(cookie_string), status);

  num_cookie_lines_left_--;

  // If all the cookie lines have been handled, |set_cookie_status_list_| now
  // reflects the result of all Set-Cookie lines, and the request can be
  // continued.
  if (num_cookie_lines_left_ == 0)
    NotifyHeadersComplete();
}

void URLRequestHttpJob::ProcessStrictTransportSecurityHeader() {
  DCHECK(response_info_);
  TransportSecurityState* security_state =
      request_->context()->transport_security_state();
  const SSLInfo& ssl_info = response_info_->ssl_info;

  // Only accept HSTS headers on HTTPS connections that have no
  // certificate errors.
  if (!ssl_info.is_valid() || IsCertStatusError(ssl_info.cert_status) ||
      !security_state) {
    return;
  }

  // Don't accept HSTS headers when the hostname is an IP address.
  if (request_info_.url.HostIsIPAddress())
    return;

  // http://tools.ietf.org/html/draft-ietf-websec-strict-transport-sec:
  //
  //   If a UA receives more than one STS header field in a HTTP response
  //   message over secure transport, then the UA MUST process only the
  //   first such header field.
  HttpResponseHeaders* headers = GetResponseHeaders();
  std::string value;
  if (headers->EnumerateHeader(nullptr, "Strict-Transport-Security", &value))
    security_state->AddHSTSHeader(request_info_.url.host(), value);
}

void URLRequestHttpJob::ProcessExpectCTHeader() {
  DCHECK(response_info_);
  TransportSecurityState* security_state =
      request_->context()->transport_security_state();
  const SSLInfo& ssl_info = response_info_->ssl_info;

  // Only accept Expect CT headers on HTTPS connections that have no
  // certificate errors.
  if (!ssl_info.is_valid() || IsCertStatusError(ssl_info.cert_status) ||
      !security_state) {
    return;
  }

  HttpResponseHeaders* headers = GetResponseHeaders();
  std::string value;
  if (headers->GetNormalizedHeader("Expect-CT", &value)) {
    security_state->ProcessExpectCTHeader(
        value, HostPortPair::FromURL(request_info_.url), ssl_info);
  }
}

void URLRequestHttpJob::OnStartCompleted(int result) {
  TRACE_EVENT0(NetTracingCategory(), "URLRequestHttpJob::OnStartCompleted");
  RecordTimer();

  // If the job is done (due to cancellation), can just ignore this
  // notification.
  if (done_)
    return;

  receive_headers_end_ = base::TimeTicks::Now();

  const URLRequestContext* context = request_->context();

  if (transaction_ && transaction_->GetResponseInfo()) {
    const SSLInfo& ssl_info = transaction_->GetResponseInfo()->ssl_info;
    if (!IsCertificateError(result)) {
      LogTrustAnchor(ssl_info.public_key_hashes);
    }

    RecordCTHistograms(ssl_info);
  }

  if (transaction_ && transaction_->GetResponseInfo()) {
    SetProxyServer(transaction_->GetResponseInfo()->proxy_server);
  }

  if (result == OK) {
    scoped_refptr<HttpResponseHeaders> headers = GetResponseHeaders();

    if (network_delegate()) {
      // Note that |this| may not be deleted until
      // |URLRequestHttpJob::OnHeadersReceivedCallback()| or
      // |NetworkDelegate::URLRequestDestroyed()| has been called.
      OnCallToDelegate(NetLogEventType::NETWORK_DELEGATE_HEADERS_RECEIVED);
      preserve_fragment_on_redirect_url_ = base::nullopt;
      IPEndPoint endpoint;
      if (transaction_)
        transaction_->GetRemoteEndpoint(&endpoint);
      // The NetworkDelegate must watch for OnRequestDestroyed and not modify
      // any of the arguments after it's called.
      // TODO(mattm): change the API to remove the out-params and take the
      // results as params of the callback.
      int error = network_delegate()->NotifyHeadersReceived(
          request_,
          base::BindOnce(&URLRequestHttpJob::OnHeadersReceivedCallback,
                         weak_factory_.GetWeakPtr()),
          headers.get(), &override_response_headers_, endpoint,
          &preserve_fragment_on_redirect_url_);
      if (error != OK) {
        if (error == ERR_IO_PENDING) {
          awaiting_callback_ = true;
        } else {
          request_->net_log().AddEventWithStringParams(
              NetLogEventType::CANCELLED, "source", "delegate");
          OnCallToDelegateComplete();
          NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, error));
        }
        return;
      }
    }

    SaveCookiesAndNotifyHeadersComplete(OK);
  } else if (IsCertificateError(result)) {
    // We encountered an SSL certificate error.
    // Maybe overridable, maybe not. Ask the delegate to decide.
    TransportSecurityState* state = context->transport_security_state();
    NotifySSLCertificateError(
        result, transaction_->GetResponseInfo()->ssl_info,
        state->ShouldSSLErrorsBeFatal(request_info_.url.host()));
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    NotifyCertificateRequested(
        transaction_->GetResponseInfo()->cert_request_info.get());
  } else {
    // Even on an error, there may be useful information in the response
    // info (e.g. whether there's a cached copy).
    if (transaction_.get())
      response_info_ = transaction_->GetResponseInfo();
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
}

void URLRequestHttpJob::OnHeadersReceivedCallback(int result) {
  awaiting_callback_ = false;

  // Check that there are no callbacks to already canceled requests.
  DCHECK_NE(URLRequestStatus::CANCELED, GetStatus().status());

  SaveCookiesAndNotifyHeadersComplete(result);
}

void URLRequestHttpJob::OnReadCompleted(int result) {
  TRACE_EVENT0(NetTracingCategory(), "URLRequestHttpJob::OnReadCompleted");
  read_in_progress_ = false;

  DCHECK_NE(ERR_IO_PENDING, result);

  if (ShouldFixMismatchedContentLength(result))
    result = OK;

  // EOF or error, done with this job.
  if (result <= 0)
    DoneWithRequest(FINISHED);

  ReadRawDataComplete(result);
}

void URLRequestHttpJob::RestartTransactionWithAuth(
    const AuthCredentials& credentials) {
  auth_credentials_ = credentials;

  // These will be reset in OnStartCompleted.
  response_info_ = nullptr;
  override_response_headers_ = nullptr;  // See https://crbug.com/801237.
  receive_headers_end_ = base::TimeTicks();

  ResetTimer();

  // Update the cookies, since the cookie store may have been updated from the
  // headers in the 401/407. Since cookies were already appended to
  // extra_headers, we need to strip them out before adding them again.
  request_info_.extra_headers.RemoveHeader(HttpRequestHeaders::kCookie);

  // TODO(https://crbug.com/968327/): This is weird, as all other clearing is at
  // the URLRequest layer. Should this call into URLRequest so it can share
  // logic at that layer with SetAuth()?
  request_->set_maybe_sent_cookies({});
  request_->set_maybe_stored_cookies({});

  AddCookieHeaderAndStart();
}

void URLRequestHttpJob::SetUpload(UploadDataStream* upload) {
  DCHECK(!transaction_.get()) << "cannot change once started";
  request_info_.upload_data_stream = upload;
}

void URLRequestHttpJob::SetExtraRequestHeaders(
    const HttpRequestHeaders& headers) {
  DCHECK(!transaction_.get()) << "cannot change once started";
  request_info_.extra_headers.CopyFrom(headers);
}

LoadState URLRequestHttpJob::GetLoadState() const {
  return transaction_.get() ?
      transaction_->GetLoadState() : LOAD_STATE_IDLE;
}

bool URLRequestHttpJob::GetMimeType(std::string* mime_type) const {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  HttpResponseHeaders* headers = GetResponseHeaders();
  if (!headers)
    return false;
  return headers->GetMimeType(mime_type);
}

bool URLRequestHttpJob::GetCharset(std::string* charset) {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  return GetResponseHeaders()->GetCharset(charset);
}

void URLRequestHttpJob::GetResponseInfo(HttpResponseInfo* info) {
  if (response_info_) {
    DCHECK(transaction_.get());

    *info = *response_info_;
    if (override_response_headers_.get())
      info->headers = override_response_headers_;
  }
}

void URLRequestHttpJob::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  // If haven't made it far enough to receive any headers, don't return
  // anything. This makes for more consistent behavior in the case of errors.
  if (!transaction_ || receive_headers_end_.is_null())
    return;
  if (transaction_->GetLoadTimingInfo(load_timing_info))
    load_timing_info->receive_headers_end = receive_headers_end_;
}

bool URLRequestHttpJob::GetTransactionRemoteEndpoint(
    IPEndPoint* endpoint) const {
  if (!transaction_)
    return false;

  return transaction_->GetRemoteEndpoint(endpoint);
}

int URLRequestHttpJob::GetResponseCode() const {
  DCHECK(transaction_.get());

  if (!response_info_)
    return -1;

  return GetResponseHeaders()->response_code();
}

void URLRequestHttpJob::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (!transaction_)
    return;
  return transaction_->PopulateNetErrorDetails(details);
}

std::unique_ptr<SourceStream> URLRequestHttpJob::SetUpSourceStream() {
  DCHECK(transaction_.get());
  if (!response_info_)
    return nullptr;

  std::unique_ptr<SourceStream> upstream = URLRequestJob::SetUpSourceStream();
  HttpResponseHeaders* headers = GetResponseHeaders();
  std::string type;
  std::vector<SourceStream::SourceType> types;
  size_t iter = 0;
  while (headers->EnumerateHeader(&iter, "Content-Encoding", &type)) {
    SourceStream::SourceType source_type =
        FilterSourceStream::ParseEncodingType(type);
    switch (source_type) {
      case SourceStream::TYPE_BROTLI:
      case SourceStream::TYPE_DEFLATE:
      case SourceStream::TYPE_GZIP:
        types.push_back(source_type);
        break;
      case SourceStream::TYPE_NONE:
        // Identity encoding type. Pass through raw response body.
        return upstream;
      case SourceStream::TYPE_UNKNOWN:
        // Unknown encoding type. Pass through raw response body.
        // Request will not be canceled; though
        // it is expected that user will see malformed / garbage response.
        return upstream;
      case SourceStream::TYPE_GZIP_FALLBACK_DEPRECATED:
      case SourceStream::TYPE_SDCH_DEPRECATED:
      case SourceStream::TYPE_SDCH_POSSIBLE_DEPRECATED:
      case SourceStream::TYPE_REJECTED:
      case SourceStream::TYPE_INVALID:
      case SourceStream::TYPE_MAX:
        NOTREACHED();
        return nullptr;
    }
  }

  for (auto r_iter = types.rbegin(); r_iter != types.rend(); ++r_iter) {
    std::unique_ptr<FilterSourceStream> downstream;
    SourceStream::SourceType type = *r_iter;
    switch (type) {
      case SourceStream::TYPE_BROTLI:
        downstream = CreateBrotliSourceStream(std::move(upstream));
        break;
      case SourceStream::TYPE_GZIP:
      case SourceStream::TYPE_DEFLATE:
        downstream = GzipSourceStream::Create(std::move(upstream), type);
        break;
      case SourceStream::TYPE_GZIP_FALLBACK_DEPRECATED:
      case SourceStream::TYPE_SDCH_DEPRECATED:
      case SourceStream::TYPE_SDCH_POSSIBLE_DEPRECATED:
      case SourceStream::TYPE_NONE:
      case SourceStream::TYPE_INVALID:
      case SourceStream::TYPE_REJECTED:
      case SourceStream::TYPE_UNKNOWN:
      case SourceStream::TYPE_MAX:
        NOTREACHED();
        return nullptr;
    }
    if (downstream == nullptr)
      return nullptr;
    upstream = std::move(downstream);
  }

  return upstream;
}

bool URLRequestHttpJob::CopyFragmentOnRedirect(const GURL& location) const {
  // Allow modification of reference fragments by default, unless
  // |preserve_fragment_on_redirect_url_| is set and equal to the redirect URL.
  return !preserve_fragment_on_redirect_url_.has_value() ||
         preserve_fragment_on_redirect_url_ != location;
}

bool URLRequestHttpJob::IsSafeRedirect(const GURL& location) {
  // HTTP is always safe.
  // TODO(pauljensen): Remove once crbug.com/146591 is fixed.
  if (location.is_valid() &&
      (location.scheme() == "http" || location.scheme() == "https")) {
    return true;
  }
  // Query URLRequestJobFactory as to whether |location| would be safe to
  // redirect to.
  return request_->context()->job_factory() &&
      request_->context()->job_factory()->IsSafeRedirectTarget(location);
}

bool URLRequestHttpJob::NeedsAuth() {
  int code = GetResponseCode();
  if (code == -1)
    return false;

  // Check if we need either Proxy or WWW Authentication. This could happen
  // because we either provided no auth info, or provided incorrect info.
  switch (code) {
    case 407:
      if (proxy_auth_state_ == AUTH_STATE_CANCELED)
        return false;
      proxy_auth_state_ = AUTH_STATE_NEED_AUTH;
      return true;
    case 401:
      if (server_auth_state_ == AUTH_STATE_CANCELED)
        return false;
      server_auth_state_ = AUTH_STATE_NEED_AUTH;
      return true;
  }
  return false;
}

std::unique_ptr<AuthChallengeInfo> URLRequestHttpJob::GetAuthChallengeInfo() {
  DCHECK(transaction_.get());
  DCHECK(response_info_);

  // sanity checks:
  DCHECK(proxy_auth_state_ == AUTH_STATE_NEED_AUTH ||
         server_auth_state_ == AUTH_STATE_NEED_AUTH);
  DCHECK((GetResponseHeaders()->response_code() == HTTP_UNAUTHORIZED) ||
         (GetResponseHeaders()->response_code() ==
          HTTP_PROXY_AUTHENTICATION_REQUIRED));

  if (!response_info_->auth_challenge.has_value())
    return nullptr;
  return std::make_unique<AuthChallengeInfo>(
      response_info_->auth_challenge.value());
}

void URLRequestHttpJob::SetAuth(const AuthCredentials& credentials) {
  DCHECK(transaction_.get());

  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = AUTH_STATE_HAVE_AUTH;
  } else {
    DCHECK_EQ(server_auth_state_, AUTH_STATE_NEED_AUTH);
    server_auth_state_ = AUTH_STATE_HAVE_AUTH;
  }

  RestartTransactionWithAuth(credentials);
}

void URLRequestHttpJob::CancelAuth() {
  if (proxy_auth_state_ == AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = AUTH_STATE_CANCELED;
  } else {
    DCHECK_EQ(server_auth_state_, AUTH_STATE_NEED_AUTH);
    server_auth_state_ = AUTH_STATE_CANCELED;
  }

  // The above lines should ensure this is the case.
  DCHECK(!NeedsAuth());

  // Let the consumer read the HTTP error page. NeedsAuth() should now return
  // false, so NotifyHeadersComplete() should not request auth from the client
  // again.
  //
  // Have to do this via PostTask to avoid re-entrantly calling into the
  // consumer.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestHttpJob::NotifyFinalHeadersReceived,
                                weak_factory_.GetWeakPtr()));
}

void URLRequestHttpJob::ContinueWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key) {
  DCHECK(transaction_);

  DCHECK(!response_info_) << "should not have a response yet";
  DCHECK(!override_response_headers_);
  receive_headers_end_ = base::TimeTicks();

  ResetTimer();

  int rv = transaction_->RestartWithCertificate(
      std::move(client_cert), std::move(client_private_key),
      base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                     base::Unretained(this)));
  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                                weak_factory_.GetWeakPtr(), rv));
}

void URLRequestHttpJob::ContinueDespiteLastError() {
  // If the transaction was destroyed, then the job was cancelled.
  if (!transaction_.get())
    return;

  DCHECK(!response_info_) << "should not have a response yet";
  DCHECK(!override_response_headers_);
  receive_headers_end_ = base::TimeTicks();

  ResetTimer();

  int rv = transaction_->RestartIgnoringLastError(base::BindOnce(
      &URLRequestHttpJob::OnStartCompleted, base::Unretained(this)));
  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestHttpJob::OnStartCompleted,
                                weak_factory_.GetWeakPtr(), rv));
}

bool URLRequestHttpJob::ShouldFixMismatchedContentLength(int rv) const {
  // Some servers send the body compressed, but specify the content length as
  // the uncompressed size. Although this violates the HTTP spec we want to
  // support it (as IE and FireFox do), but *only* for an exact match.
  // See http://crbug.com/79694.
  if (rv == ERR_CONTENT_LENGTH_MISMATCH ||
      rv == ERR_INCOMPLETE_CHUNKED_ENCODING) {
    if (request_->response_headers()) {
      int64_t expected_length =
          request_->response_headers()->GetContentLength();
      VLOG(1) << __func__ << "() \"" << request_->url().spec() << "\""
              << " content-length = " << expected_length
              << " pre total = " << prefilter_bytes_read()
              << " post total = " << postfilter_bytes_read();
      if (postfilter_bytes_read() == expected_length) {
        // Clear the error.
        return true;
      }
    }
  }
  return false;
}

int URLRequestHttpJob::ReadRawData(IOBuffer* buf, int buf_size) {
  DCHECK_NE(buf_size, 0);
  DCHECK(!read_in_progress_);

  int rv =
      transaction_->Read(buf, buf_size,
                         base::BindOnce(&URLRequestHttpJob::OnReadCompleted,
                                        base::Unretained(this)));

  if (ShouldFixMismatchedContentLength(rv))
    rv = OK;

  if (rv == 0 || (rv < 0 && rv != ERR_IO_PENDING))
    DoneWithRequest(FINISHED);

  if (rv == ERR_IO_PENDING)
    read_in_progress_ = true;

  return rv;
}

int64_t URLRequestHttpJob::GetTotalReceivedBytes() const {
  int64_t total_received_bytes =
      total_received_bytes_from_previous_transactions_;
  if (transaction_)
    total_received_bytes += transaction_->GetTotalReceivedBytes();
  return total_received_bytes;
}

int64_t URLRequestHttpJob::GetTotalSentBytes() const {
  int64_t total_sent_bytes = total_sent_bytes_from_previous_transactions_;
  if (transaction_)
    total_sent_bytes += transaction_->GetTotalSentBytes();
  return total_sent_bytes;
}

void URLRequestHttpJob::DoneReading() {
  if (transaction_) {
    transaction_->DoneReading();
  }
  DoneWithRequest(FINISHED);
}

void URLRequestHttpJob::DoneReadingRedirectResponse() {
  if (transaction_) {
    if (transaction_->GetResponseInfo()->headers->IsRedirect(nullptr)) {
      // If the original headers indicate a redirect, go ahead and cache the
      // response, even if the |override_response_headers_| are a redirect to
      // another location.
      transaction_->DoneReading();
    } else {
      // Otherwise, |override_response_headers_| must be non-NULL and contain
      // bogus headers indicating a redirect.
      DCHECK(override_response_headers_.get());
      DCHECK(override_response_headers_->IsRedirect(nullptr));
      transaction_->StopCaching();
    }
  }
  DoneWithRequest(FINISHED);
}

IPEndPoint URLRequestHttpJob::GetResponseRemoteEndpoint() const {
  return response_info_ ? response_info_->remote_endpoint : IPEndPoint();
}

void URLRequestHttpJob::RecordTimer() {
  if (request_creation_time_.is_null()) {
    NOTREACHED()
        << "The same transaction shouldn't start twice without new timing.";
    return;
  }

  base::TimeDelta to_start = base::Time::Now() - request_creation_time_;
  request_creation_time_ = base::Time();

  UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpTimeToFirstByte", to_start);
}

void URLRequestHttpJob::ResetTimer() {
  if (!request_creation_time_.is_null()) {
    NOTREACHED()
        << "The timer was reset before it was recorded.";
    return;
  }
  request_creation_time_ = base::Time::Now();
}

void URLRequestHttpJob::SetRequestHeadersCallback(
    RequestHeadersCallback callback) {
  DCHECK(!transaction_);
  DCHECK(!request_headers_callback_);
  request_headers_callback_ = std::move(callback);
}

void URLRequestHttpJob::SetResponseHeadersCallback(
    ResponseHeadersCallback callback) {
  DCHECK(!transaction_);
  DCHECK(!response_headers_callback_);
  response_headers_callback_ = std::move(callback);
}

void URLRequestHttpJob::RecordPerfHistograms(CompletionCause reason) {
  if (start_time_.is_null())
    return;

  base::TimeDelta total_time = base::TimeTicks::Now() - start_time_;
  UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTime", total_time);

  if (reason == FINISHED) {
    UmaHistogramTimes(
        base::StringPrintf("Net.HttpJob.TotalTimeSuccess.Priority%d",
                           request()->priority()),
        total_time);
    UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeSuccess", total_time);
  } else {
    UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeCancel", total_time);
  }

  if (response_info_) {
    // QUIC (by default) supports https scheme only, thus track https URLs only
    // for QUIC.
    bool is_https_google = request() && request()->url().SchemeIs("https") &&
                           HasGoogleHost(request()->url());
    bool used_quic = response_info_->DidUseQuic();
    if (is_https_google) {
      if (used_quic) {
        UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpJob.TotalTime.Secure.Quic",
                                   total_time);
      }
    }

    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.HttpJob.PrefilterBytesRead",
                                prefilter_bytes_read(), 1, 50000000, 50);
    if (response_info_->was_cached) {
      UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeCached", total_time);
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.HttpJob.PrefilterBytesRead.Cache",
                                  prefilter_bytes_read(), 1, 50000000, 50);

      if (response_info_->unused_since_prefetch)
        UMA_HISTOGRAM_COUNTS_1M("Net.Prefetch.HitBytes",
                                prefilter_bytes_read());
    } else {
      UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeNotCached", total_time);
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.HttpJob.PrefilterBytesRead.Net",
                                  prefilter_bytes_read(), 1, 50000000, 50);

      if (request_info_.load_flags & LOAD_PREFETCH) {
        UMA_HISTOGRAM_COUNTS_1M("Net.Prefetch.PrefilterBytesReadFromNetwork",
                                prefilter_bytes_read());
      }
      if (is_https_google) {
        if (used_quic) {
          UMA_HISTOGRAM_MEDIUM_TIMES(
              "Net.HttpJob.TotalTimeNotCached.Secure.Quic", total_time);
        } else {
          UMA_HISTOGRAM_MEDIUM_TIMES(
              "Net.HttpJob.TotalTimeNotCached.Secure.NotQuic", total_time);
        }
      }
    }
  }

  start_time_ = base::TimeTicks();
}

void URLRequestHttpJob::DoneWithRequest(CompletionCause reason) {
  if (done_)
    return;
  done_ = true;

  // Notify NetworkQualityEstimator.
  NetworkQualityEstimator* network_quality_estimator =
      request()->context()->network_quality_estimator();
  if (network_quality_estimator) {
    network_quality_estimator->NotifyRequestCompleted(
        *request(), request_->status().error());
  }

  RecordPerfHistograms(reason);
  request()->set_received_response_content_length(prefilter_bytes_read());
}

HttpResponseHeaders* URLRequestHttpJob::GetResponseHeaders() const {
  DCHECK(transaction_.get());
  DCHECK(transaction_->GetResponseInfo());
  return override_response_headers_.get() ?
             override_response_headers_.get() :
             transaction_->GetResponseInfo()->headers.get();
}

void URLRequestHttpJob::NotifyURLRequestDestroyed() {
  awaiting_callback_ = false;

  // Notify NetworkQualityEstimator.
  NetworkQualityEstimator* network_quality_estimator =
      request()->context()->network_quality_estimator();
  if (network_quality_estimator)
    network_quality_estimator->NotifyURLRequestDestroyed(*request());
}

}  // namespace net
