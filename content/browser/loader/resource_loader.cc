// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_loader.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/appcache/appcache_interceptor.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_handler.h"
#include "content/browser/loader/resource_loader_delegate.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/service_worker/service_worker_response_info.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/common/appcache_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_type.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/cert/symantec_certs.h"
#include "net/http/http_response_headers.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/ssl/client_cert_store.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/throttling/scoped_throttling_token.h"
#include "url/url_constants.h"

using base::TimeDelta;
using base::TimeTicks;

namespace content {
namespace {

// Copies selected fields from |in| to the returned SSLInfo. To avoid sending
// unnecessary data into the renderer, this only copies the fields that the
// renderer cares about.
net::SSLInfo SelectSSLInfoFields(const net::SSLInfo& in) {
  net::SSLInfo out;
  out.connection_status = in.connection_status;
  out.key_exchange_group = in.key_exchange_group;
  out.peer_signature_algorithm = in.peer_signature_algorithm;
  out.signed_certificate_timestamps = in.signed_certificate_timestamps;
  out.cert = in.cert;
  return out;
}

void PopulateResourceResponse(
    ResourceRequestInfoImpl* info,
    net::URLRequest* request,
    network::ResourceResponse* response,
    const net::HttpRawRequestHeaders& raw_request_headers,
    const net::HttpResponseHeaders* raw_response_headers) {
  response->head.request_time = request->request_time();
  response->head.response_time = request->response_time();
  response->head.headers = request->response_headers();
  request->GetCharset(&response->head.charset);
  response->head.content_length = request->GetExpectedContentSize();
  request->GetMimeType(&response->head.mime_type);
  net::HttpResponseInfo response_info = request->response_info();
  response->head.was_fetched_via_spdy = response_info.was_fetched_via_spdy;
  response->head.was_alpn_negotiated = response_info.was_alpn_negotiated;
  response->head.alpn_negotiated_protocol =
      response_info.alpn_negotiated_protocol;
  response->head.connection_info = response_info.connection_info;
  response->head.socket_address = response_info.socket_address;
  response->head.was_fetched_via_proxy = request->was_fetched_via_proxy();
  response->head.network_accessed = response_info.network_accessed;
  response->head.async_revalidation_requested =
      response_info.async_revalidation_requested;
  if (info->ShouldReportRawHeaders()) {
    response->head.raw_request_response_info =
        network::BuildRawRequestResponseInfo(*request, raw_request_headers,
                                             raw_response_headers);
  }

  response->head.effective_connection_type =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  if (info->GetResourceType() == RESOURCE_TYPE_MAIN_FRAME) {
    DCHECK(info->IsMainFrame());
    net::NetworkQualityEstimator* estimator =
        request->context()->network_quality_estimator();
    if (estimator) {
      response->head.effective_connection_type =
          estimator->GetEffectiveConnectionType();
    }
  }

  const ServiceWorkerResponseInfo* service_worker_info =
      ServiceWorkerResponseInfo::ForRequest(request);
  if (service_worker_info)
    service_worker_info->GetExtraResponseInfo(&response->head);
  response->head.appcache_id = kAppCacheNoCacheId;
  AppCacheInterceptor::GetExtraResponseInfo(
      request, &response->head.appcache_id,
      &response->head.appcache_manifest_url);
  if (info->is_load_timing_enabled())
    request->GetLoadTimingInfo(&response->head.load_timing);

  if (request->ssl_info().cert.get()) {
    response->head.cert_status = request->ssl_info().cert_status;
    response->head.ct_policy_compliance =
        request->ssl_info().ct_policy_compliance;
    response->head.is_legacy_symantec_cert =
        (!net::IsCertStatusError(response->head.cert_status) ||
         net::IsCertStatusMinorError(response->head.cert_status)) &&
        net::IsLegacySymantecCert(request->ssl_info().public_key_hashes);

    if (info->ShouldReportSecurityInfo())
      response->head.ssl_info = SelectSSLInfoFields(request->ssl_info());
  } else {
    // We should not have any SSL state.
    DCHECK(!request->ssl_info().cert_status);
    DCHECK_EQ(request->ssl_info().security_bits, -1);
    DCHECK_EQ(request->ssl_info().key_exchange_group, 0);
    DCHECK_EQ(request->ssl_info().peer_signature_algorithm, 0);
    DCHECK_EQ(request->ssl_info().connection_status, 0);
  }
}

}  // namespace

class ResourceLoader::Controller : public ResourceController {
 public:
  explicit Controller(ResourceLoader* resource_loader)
      : resource_loader_(resource_loader){};

  ~Controller() override {}

  // ResourceController implementation:
  void Resume() override {
    MarkAsUsed();
    resource_loader_->Resume(true /* called_from_resource_controller */,
                             base::nullopt);
  }

  void ResumeForRedirect(const base::Optional<net::HttpRequestHeaders>&
                             modified_request_headers) override {
    MarkAsUsed();
    resource_loader_->Resume(true /* called_from_resource_controller */,
                             modified_request_headers);
  }

  void Cancel() override {
    MarkAsUsed();
    resource_loader_->Cancel();
  }

  void CancelWithError(int error_code) override {
    MarkAsUsed();
    resource_loader_->CancelWithError(error_code);
  }

 private:
  void MarkAsUsed() {
#if DCHECK_IS_ON()
    DCHECK(!used_);
    used_ = true;
#endif
  }

  ResourceLoader* const resource_loader_;

#if DCHECK_IS_ON()
  // Set to true once one of the ResourceContoller methods has been invoked.
  bool used_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

// Helper class.  Sets the stage of a ResourceLoader to DEFERRED_SYNC on
// construction, and on destruction does one of the following:
// 1) If the ResourceLoader has a deferred stage of DEFERRED_NONE, sets the
// ResourceLoader's stage to the stage specified on construction and resumes it.
// 2) If the ResourceLoader still has a deferred stage of DEFERRED_SYNC, sets
// the ResourceLoader's stage to the stage specified on construction.  The
// ResourceLoader will be resumed at some point in the future.
class ResourceLoader::ScopedDeferral {
 public:
  ScopedDeferral(ResourceLoader* resource_loader,
                 ResourceLoader::DeferredStage deferred_stage)
      : resource_loader_(resource_loader), deferred_stage_(deferred_stage) {
    resource_loader_->deferred_stage_ = DEFERRED_SYNC;
  }

  ~ScopedDeferral() {
    DeferredStage old_deferred_stage = resource_loader_->deferred_stage_;
    // On destruction, either the stage is still DEFERRED_SYNC, or Resume() was
    // called once, and it advanced to DEFERRED_NONE.
    DCHECK(old_deferred_stage == DEFERRED_NONE ||
           old_deferred_stage == DEFERRED_SYNC)
        << old_deferred_stage;
    resource_loader_->deferred_stage_ = deferred_stage_;
    // If Resume() was called, it just advanced the state without doing
    // anything. Go ahead and resume the request now.
    if (old_deferred_stage == DEFERRED_NONE)
      resource_loader_->Resume(false /* called_from_resource_controller */,
                               base::nullopt);
  }

 private:
  ResourceLoader* const resource_loader_;
  const DeferredStage deferred_stage_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDeferral);
};

ResourceLoader::ResourceLoader(
    std::unique_ptr<net::URLRequest> request,
    std::unique_ptr<ResourceHandler> handler,
    ResourceLoaderDelegate* delegate,
    ResourceContext* resource_context,
    std::unique_ptr<network::ScopedThrottlingToken> throttling_token)
    : deferred_stage_(DEFERRED_NONE),
      request_(std::move(request)),
      handler_(std::move(handler)),
      delegate_(delegate),
      times_cancelled_before_request_start_(0),
      started_request_(false),
      times_cancelled_after_request_start_(0),
      resource_context_(resource_context),
      throttling_token_(std::move(throttling_token)),
      weak_ptr_factory_(this) {
  request_->set_delegate(this);
  handler_->SetDelegate(this);
}

ResourceLoader::~ResourceLoader() {
  if (update_body_read_before_paused_)
    body_read_before_paused_ = request_->GetRawBodyBytes();
  if (body_read_before_paused_ != -1) {
    // Only record histograms for web schemes.
    bool should_record_scheme = request_->url().SchemeIs(url::kHttpScheme) ||
                                request_->url().SchemeIs(url::kHttpsScheme) ||
                                request_->url().SchemeIs(url::kFtpScheme);
    if (!request_->was_cached() && should_record_scheme) {
      UMA_HISTOGRAM_COUNTS_1M("Network.URLLoader.BodyReadFromNetBeforePaused",
                              body_read_before_paused_);
    } else {
      DVLOG(1) << "The request has been paused, but "
               << "Network.URLLoader.BodyReadFromNetBeforePaused is not "
               << "reported because the response body may not be from the "
               << "network, or may be from cache. body_read_before_paused_: "
               << body_read_before_paused_;
    }
  }

  if (login_delegate_.get())
    login_delegate_->OnRequestCancelled();
  ssl_client_auth_handler_.reset();

  // Run ResourceHandler destructor before we tear-down the rest of our state
  // as the ResourceHandler may want to inspect the URLRequest and other state.
  handler_.reset();
}

void ResourceLoader::StartRequest() {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::StartRequest", this,
                         TRACE_EVENT_FLAG_FLOW_OUT);

  ScopedDeferral scoped_deferral(this, DEFERRED_START);
  handler_->OnWillStart(request_->url(), std::make_unique<Controller>(this));
}

void ResourceLoader::CancelRequest(bool from_renderer) {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::CancelRequest", this,
                         TRACE_EVENT_FLAG_FLOW_IN);
  CancelRequestInternal(net::ERR_ABORTED, from_renderer);
}

void ResourceLoader::CancelWithError(int error_code) {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::CancelWithError", this,
                         TRACE_EVENT_FLAG_FLOW_IN);
  CancelRequestInternal(error_code, false);
}

ResourceRequestInfoImpl* ResourceLoader::GetRequestInfo() {
  return ResourceRequestInfoImpl::ForRequest(request_.get());
}

void ResourceLoader::ClearLoginDelegate() {
  login_delegate_ = nullptr;
}

void ResourceLoader::OutOfBandCancel(int error_code, bool tell_renderer) {
  CancelRequestInternal(error_code, !tell_renderer);
}

void ResourceLoader::PauseReadingBodyFromNet() {
  DVLOG(1) << "ResourceLoader pauses fetching response body for "
           << request_->original_url().spec();

  // Please note that reading the body is paused in all cases. Even if the URL
  // request indicates that the response was cached, there could still be
  // network activity involved. For example, the response was only partially
  // cached. This also pauses things that don't come from the network (chrome
  // URLs, file URLs, data URLs, etc.).
  //
  // On the other hand, BodyReadFromNetBeforePaused histogram is only reported
  // when it is certain that the response body is read from the network and
  // wasn't cached. This avoids polluting the histogram data.
  should_pause_reading_body_ = true;

  if (pending_read_) {
    update_body_read_before_paused_ = true;
  } else {
    body_read_before_paused_ = request_->GetRawBodyBytes();
  }
}

void ResourceLoader::ResumeReadingBodyFromNet() {
  DVLOG(1) << "ResourceLoader resumes fetching response body for "
           << request_->original_url().spec();

  should_pause_reading_body_ = false;

  if (read_more_body_supressed_) {
    DCHECK(!is_deferred());
    read_more_body_supressed_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ResourceLoader::ReadMore,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  false /* handle_result_asynchronously */));
  }
}

void ResourceLoader::OnReceivedRedirect(net::URLRequest* unused,
                                        const net::RedirectInfo& redirect_info,
                                        bool* defer) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "ResourceLoader::OnReceivedRedirect");
  DCHECK_EQ(request_.get(), unused);

  DVLOG(1) << "OnReceivedRedirect: " << request_->url().spec();
  DCHECK(request_->status().is_success());

  ResourceRequestInfoImpl* info = GetRequestInfo();

  // With PlzNavigate for frame navigations this check is done in the
  // NavigationRequest::OnReceivedRedirect() function.
  bool check_handled_elsewhere = IsBrowserSideNavigationEnabled() &&
      IsResourceTypeFrame(info->GetResourceType());

  if (!check_handled_elsewhere) {
    if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
            info->GetChildID(), redirect_info.new_url)) {
      DVLOG(1) << "Denied unauthorized request for "
               << redirect_info.new_url.possibly_invalid_spec();

      // Tell the renderer that this request was disallowed.
      Cancel();
      return;
    }
  }

  scoped_refptr<network::ResourceResponse> response =
      new network::ResourceResponse();
  PopulateResourceResponse(info, request_.get(), response.get(),
                           raw_request_headers_, raw_response_headers_.get());
  raw_request_headers_ = net::HttpRawRequestHeaders();
  raw_response_headers_ = nullptr;

  delegate_->DidReceiveRedirect(this, redirect_info.new_url, response.get());

  // Can't used ScopedDeferral here, because on sync completion, need to set
  // |defer| to false instead of calling back into the URLRequest.
  deferred_stage_ = DEFERRED_SYNC;
  handler_->OnRequestRedirected(redirect_info, response.get(),
                                std::make_unique<Controller>(this));
  if (is_deferred()) {
    *defer = true;
    deferred_redirect_url_ = redirect_info.new_url;
    deferred_stage_ = DEFERRED_REDIRECT;
  } else {
    *defer = false;
    if (delegate_->HandleExternalProtocol(this, redirect_info.new_url))
      Cancel();
  }
}

void ResourceLoader::OnAuthRequired(net::URLRequest* unused,
                                    net::AuthChallengeInfo* auth_info) {
  DCHECK_EQ(request_.get(), unused);

  ResourceRequestInfoImpl* info = GetRequestInfo();
  if (info->do_not_prompt_for_login()) {
    request_->CancelAuth();
    return;
  }

  // Create a login dialog on the UI thread to get authentication data, or pull
  // from cache and continue on the IO thread.

  DCHECK(!login_delegate_.get())
      << "OnAuthRequired called with login_delegate pending";
  login_delegate_ = delegate_->CreateLoginDelegate(this, auth_info);
  if (!login_delegate_.get())
    request_->CancelAuth();
}

void ResourceLoader::OnCertificateRequested(
    net::URLRequest* unused,
    net::SSLCertRequestInfo* cert_info) {
  DCHECK_EQ(request_.get(), unused);

  if (request_->load_flags() & net::LOAD_PREFETCH) {
    request_->Cancel();
    return;
  }

  DCHECK(!ssl_client_auth_handler_)
      << "OnCertificateRequested called with ssl_client_auth_handler pending";
  ResourceRequestInfo::WebContentsGetter web_contents_getter =
      ResourceRequestInfo::ForRequest(request_.get())
          ->GetWebContentsGetterForRequest();

  std::unique_ptr<net::ClientCertStore> client_cert_store =
      GetContentClient()->browser()->CreateClientCertStore(resource_context_);
  ssl_client_auth_handler_.reset(new SSLClientAuthHandler(
      std::move(client_cert_store), std::move(web_contents_getter), cert_info,
      this));
  ssl_client_auth_handler_->SelectCertificate();
}

void ResourceLoader::OnSSLCertificateError(net::URLRequest* request,
                                           const net::SSLInfo& ssl_info,
                                           bool fatal) {
  ResourceRequestInfoImpl* info = GetRequestInfo();

  SSLManager::OnSSLCertificateError(
      weak_ptr_factory_.GetWeakPtr(), info->GetResourceType(), request_->url(),
      info->GetWebContentsGetterForRequest(), ssl_info, fatal);
}

void ResourceLoader::OnResponseStarted(net::URLRequest* unused, int net_error) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "ResourceLoader::OnResponseStarted");
  DCHECK_EQ(request_.get(), unused);

  DVLOG(1) << "OnResponseStarted: " << request_->url().spec();

  if (net_error != net::OK) {
    ResponseCompleted();
    return;
  }

  CompleteResponseStarted();
}

void ResourceLoader::OnReadCompleted(net::URLRequest* unused, int bytes_read) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "ResourceLoader::OnReadCompleted");
  DCHECK_EQ(request_.get(), unused);
  DVLOG(1) << "OnReadCompleted: \"" << request_->url().spec() << "\""
           << " bytes_read = " << bytes_read;

  pending_read_ = false;

  // bytes_read == -1 always implies an error.
  if (bytes_read == -1 || !request_->status().is_success()) {
    ResponseCompleted();
    return;
  }

  CompleteRead(bytes_read);
}

void ResourceLoader::CancelSSLRequest(int error,
                                      const net::SSLInfo* ssl_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The request can be NULL if it was cancelled by the renderer (as the
  // request of the user navigating to a new page from the location bar).
  if (!request_->is_pending())
    return;
  DVLOG(1) << "CancelSSLRequest() url: " << request_->url().spec();

  if (ssl_info) {
    request_->CancelWithSSLError(error, *ssl_info);
  } else {
    request_->CancelWithError(error);
  }
}

void ResourceLoader::ContinueSSLRequest() {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(1) << "ContinueSSLRequest() url: " << request_->url().spec();

  request_->ContinueDespiteLastError();
}

void ResourceLoader::ContinueWithCertificate(
    scoped_refptr<net::X509Certificate> cert,
    scoped_refptr<net::SSLPrivateKey> private_key) {
  DCHECK(ssl_client_auth_handler_);
  ssl_client_auth_handler_.reset();
  request_->ContinueWithCertificate(std::move(cert), std::move(private_key));
}

void ResourceLoader::CancelCertificateSelection() {
  DCHECK(ssl_client_auth_handler_);
  ssl_client_auth_handler_.reset();
  request_->CancelWithError(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void ResourceLoader::Resume(
    bool called_from_resource_controller,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  DeferredStage stage = deferred_stage_;
  deferred_stage_ = DEFERRED_NONE;
  DCHECK(!modified_request_headers.has_value() || stage == DEFERRED_REDIRECT)
      << "modified_request_headers can only be used with redirects";
  switch (stage) {
    case DEFERRED_NONE:
      NOTREACHED();
      break;
    case DEFERRED_SYNC:
      DCHECK(called_from_resource_controller);
      // Request will be resumed when the stack unwinds.
      break;
    case DEFERRED_START:
      // URLRequest::Start completes asynchronously, so starting the request now
      // won't result in synchronously calling into a ResourceHandler, if this
      // was called from Resume().
      StartRequestInternal();
      break;
    case DEFERRED_REDIRECT:
      // URLRequest::Start completes asynchronously, so starting the request now
      // won't result in synchronously calling into a ResourceHandler, if this
      // was called from Resume().
      FollowDeferredRedirectInternal(modified_request_headers);
      break;
    case DEFERRED_ON_WILL_READ:
      // Always post a task, as synchronous resumes don't go through this
      // method.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&ResourceLoader::ReadMore,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    false /* handle_result_asynchronously */));
      break;
    case DEFERRED_READ:
      if (called_from_resource_controller) {
        // TODO(mmenke):  Call PrepareToReadMore instead?  Strange that this is
        // the only case which calls different methods, depending on the path.
        // ResumeReading does check for cancellation. Should other paths do that
        // as well?
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&ResourceLoader::ResumeReading,
                                      weak_ptr_factory_.GetWeakPtr()));
      } else {
        // If this was called as a result of a handler succeeding synchronously,
        // force the result of the next read to be handled asynchronously, to
        // avoid blocking the IO thread.
        PrepareToReadMore(true /* handle_result_asynchronously */);
      }
      break;
    case DEFERRED_RESPONSE_COMPLETE:
      if (called_from_resource_controller) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&ResourceLoader::ResponseCompleted,
                                      weak_ptr_factory_.GetWeakPtr()));
      } else {
        ResponseCompleted();
      }
      break;
    case DEFERRED_FINISH:
      if (called_from_resource_controller) {
        // Delay self-destruction since we don't know how we were reached.
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&ResourceLoader::CallDidFinishLoading,
                                      weak_ptr_factory_.GetWeakPtr()));
      } else {
        CallDidFinishLoading();
      }
      break;
  }
}

void ResourceLoader::Cancel() {
  CancelRequest(false);
}

void ResourceLoader::StartRequestInternal() {
  DCHECK(!request_->is_pending());

  // Note: at this point any possible deferred start actions are already over.

  if (!request_->status().is_success()) {
    return;
  }

  if (delegate_->HandleExternalProtocol(this, request_->url())) {
    Cancel();
    return;
  }

  started_request_ = true;

  if (GetRequestInfo()->ShouldReportRawHeaders()) {
    request_->SetRequestHeadersCallback(
        base::Bind(&net::HttpRawRequestHeaders::Assign,
                   base::Unretained(&raw_request_headers_)));
    request_->SetResponseHeadersCallback(base::Bind(
        &ResourceLoader::SetRawResponseHeaders, base::Unretained(this)));
  }
  request_->Start();

  delegate_->DidStartRequest(this);
}

void ResourceLoader::CancelRequestInternal(int error, bool from_renderer) {
  DVLOG(1) << "CancelRequestInternal: " << request_->url().spec();

  ResourceRequestInfoImpl* info = GetRequestInfo();

  // WebKit will send us a cancel for downloads since it no longer handles
  // them.  In this case, ignore the cancel since we handle downloads in the
  // browser.
  if (from_renderer && (info->IsDownload() || info->is_stream()))
    return;

  if (from_renderer && info->detachable_handler()) {
    // TODO(davidben): Fix Blink handling of prefetches so they are not
    // cancelled on navigate away and end up in the local cache.
    info->detachable_handler()->Detach();
    return;
  }

  // TODO(darin): Perhaps we should really be looking to see if the status is
  // IO_PENDING?
  bool was_pending = request_->is_pending();

  if (login_delegate_.get()) {
    login_delegate_->OnRequestCancelled();
    login_delegate_ = nullptr;
  }
  ssl_client_auth_handler_.reset();

  if (!started_request_) {
    times_cancelled_before_request_start_++;
  } else {
    times_cancelled_after_request_start_++;
  }

  request_->CancelWithError(error);

  if (!was_pending) {
    // If the request isn't in flight, then we won't get an asynchronous
    // notification from the request, so we have to signal ourselves to finish
    // this request.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ResourceLoader::ResponseCompleted,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void ResourceLoader::FollowDeferredRedirectInternal(
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  DCHECK(!deferred_redirect_url_.is_empty());
  GURL redirect_url = deferred_redirect_url_;
  deferred_redirect_url_ = GURL();
  if (delegate_->HandleExternalProtocol(this, redirect_url)) {
    DCHECK(!modified_request_headers.has_value())
        << "ResourceLoaderDelegate::HandleExternalProtocol() with modified "
           "headers was not supported yet. crbug.com/845683";
    Cancel();
  } else {
    request_->FollowDeferredRedirect(modified_request_headers);
  }
}

void ResourceLoader::CompleteResponseStarted() {
  ResourceRequestInfoImpl* info = GetRequestInfo();
  scoped_refptr<network::ResourceResponse> response =
      new network::ResourceResponse();
  PopulateResourceResponse(info, request_.get(), response.get(),
                           raw_request_headers_, raw_response_headers_.get());
  raw_request_headers_ = net::HttpRawRequestHeaders();
  raw_response_headers_ = nullptr;

  delegate_->DidReceiveResponse(this, response.get());

  // For back-forward navigations, record metrics.
  // TODO(clamy): Remove once we understand the root cause behind the regression
  // of PLT for b/f navigations in PlzNavigate.
  if ((info->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK) &&
      IsResourceTypeFrame(info->GetResourceType()) &&
      request_->url().SchemeIsHTTPOrHTTPS()) {
    UMA_HISTOGRAM_BOOLEAN("Navigation.BackForward.WasCached",
                          request_->was_cached());
  }

  read_deferral_start_time_ = base::TimeTicks::Now();
  // Using a ScopedDeferral here would result in calling ReadMore(true) on sync
  // success. Calling PrepareToReadMore(false) here instead allows small
  // responses to be handled completely synchronously, if no ResourceHandler
  // defers handling of the response.
  deferred_stage_ = DEFERRED_SYNC;
  handler_->OnResponseStarted(response.get(),
                              std::make_unique<Controller>(this));
  if (is_deferred()) {
    deferred_stage_ = DEFERRED_READ;
  } else {
    PrepareToReadMore(false);
  }
}

void ResourceLoader::PrepareToReadMore(bool handle_result_async) {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::PrepareToReadMore", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(!is_deferred());

  deferred_stage_ = DEFERRED_SYNC;

  handler_->OnWillRead(&read_buffer_, &read_buffer_size_,
                       std::make_unique<Controller>(this));

  if (is_deferred()) {
    deferred_stage_ = DEFERRED_ON_WILL_READ;
  } else {
    ReadMore(handle_result_async);
  }
}

void ResourceLoader::ReadMore(bool handle_result_async) {
  DCHECK(read_buffer_.get());
  DCHECK_GT(read_buffer_size_, 0);

  if (should_pause_reading_body_) {
    read_more_body_supressed_ = true;
    return;
  }

  pending_read_ = true;

  int result = request_->Read(read_buffer_.get(), read_buffer_size_);
  // Have to do this after the Read call, to ensure it still has an outstanding
  // reference.
  read_buffer_ = nullptr;
  read_buffer_size_ = 0;

  if (result == net::ERR_IO_PENDING)
    return;

  if (!handle_result_async || result <= 0) {
    OnReadCompleted(request_.get(), result);
  } else {
    // Else, trigger OnReadCompleted asynchronously to avoid starving the IO
    // thread in case the URLRequest can provide data synchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ResourceLoader::OnReadCompleted,
                       weak_ptr_factory_.GetWeakPtr(), request_.get(), result));
  }
}

void ResourceLoader::ResumeReading() {
  DCHECK(!is_deferred());

  if (!read_deferral_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("Net.ResourceLoader.ReadDeferral",
                        base::TimeTicks::Now() - read_deferral_start_time_);
    read_deferral_start_time_ = base::TimeTicks();
  }
  if (request_->status().is_success()) {
    PrepareToReadMore(false /* handle_result_asynchronously */);
  } else {
    ResponseCompleted();
  }
}

void ResourceLoader::CompleteRead(int bytes_read) {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::CompleteRead", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  DCHECK(bytes_read >= 0);
  DCHECK(request_->status().is_success());

  if (update_body_read_before_paused_) {
    update_body_read_before_paused_ = false;
    body_read_before_paused_ = request_->GetRawBodyBytes();
  }

  ScopedDeferral scoped_deferral(
      this, bytes_read > 0 ? DEFERRED_READ : DEFERRED_RESPONSE_COMPLETE);
  handler_->OnReadCompleted(bytes_read, std::make_unique<Controller>(this));
}

void ResourceLoader::ResponseCompleted() {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::ResponseCompleted", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  DVLOG(1) << "ResponseCompleted: " << request_->url().spec();

  ScopedDeferral scoped_deferral(this, DEFERRED_FINISH);
  handler_->OnResponseCompleted(request_->status(),
                                std::make_unique<Controller>(this));
}

void ResourceLoader::CallDidFinishLoading() {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::CallDidFinishLoading",
                         this, TRACE_EVENT_FLAG_FLOW_IN);
  delegate_->DidFinishLoading(this);
}

void ResourceLoader::SetRawResponseHeaders(
    scoped_refptr<const net::HttpResponseHeaders> headers) {
  raw_response_headers_ = headers;
}

}  // namespace content
