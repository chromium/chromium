/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/threadable_loader.h"

#include <memory>
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/cors.mojom-blink.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_cors.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/base_fetch_context.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/cors/cors_error_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

// Fetch API Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch-0
AtomicString CreateAccessControlRequestHeadersHeader(
    const HTTPHeaderMap& headers) {
  Vector<String> filtered_headers = CORS::CORSUnsafeRequestHeaderNames(headers);

  if (!filtered_headers.size())
    return g_null_atom;

  // Sort header names lexicographically.
  std::sort(filtered_headers.begin(), filtered_headers.end(),
            WTF::CodePointCompareLessThan);
  StringBuilder header_buffer;
  for (const String& header : filtered_headers) {
    if (!header_buffer.IsEmpty())
      header_buffer.Append(",");
    header_buffer.Append(header);
  }

  return header_buffer.ToAtomicString();
}

}  // namespace

// DetachedClient is a ThreadableLoaderClient for a "detached"
// ThreadableLoader. It's for fetch requests with keepalive set, so
// it keeps itself alive during loading.
class ThreadableLoader::DetachedClient final
    : public GarbageCollectedFinalized<DetachedClient>,
      public ThreadableLoaderClient {
 public:
  explicit DetachedClient(ThreadableLoader* loader)
      : self_keep_alive_(this), loader_(loader) {}
  ~DetachedClient() override {}

  void DidFinishLoading(unsigned long identifier) override {
    self_keep_alive_.Clear();
  }
  void DidFail(const ResourceError&) override { self_keep_alive_.Clear(); }
  void DidFailRedirectCheck() override { self_keep_alive_.Clear(); }
  void Trace(Visitor* visitor) { visitor->Trace(loader_); }

 private:
  SelfKeepAlive<DetachedClient> self_keep_alive_;
  // Keep it alive.
  const Member<ThreadableLoader> loader_;
};

class ThreadableLoader::AssignOnScopeExit final {
  STACK_ALLOCATED();

 public:
  AssignOnScopeExit(const KURL& from, KURL* to) : from_(from), to_(to) {}
  ~AssignOnScopeExit() { *to_ = from_; }

 private:
  const KURL& from_;
  KURL* to_;

  DISALLOW_COPY_AND_ASSIGN(AssignOnScopeExit);
};

// Max number of CORS redirects handled in ThreadableLoader.
// See https://fetch.spec.whatwg.org/#http-redirect-fetch.
// //net/url_request/url_request.cc and
// //services/network/cors/cors_url_loader.cc also implement the same logic
// separately.
static const int kMaxRedirects = 20;

// static
std::unique_ptr<ResourceRequest>
ThreadableLoader::CreateAccessControlPreflightRequest(
    const ResourceRequest& request,
    const SecurityOrigin* origin) {
  const KURL& request_url = request.Url();

  DCHECK(request_url.User().IsEmpty());
  DCHECK(request_url.Pass().IsEmpty());

  std::unique_ptr<ResourceRequest> preflight_request =
      std::make_unique<ResourceRequest>(request_url);
  preflight_request->SetHTTPMethod(HTTPNames::OPTIONS);
  preflight_request->SetHTTPHeaderField(
      HTTPNames::Access_Control_Request_Method, request.HttpMethod());
  preflight_request->SetPriority(request.Priority());
  preflight_request->SetRequestContext(request.GetRequestContext());
  preflight_request->SetFetchCredentialsMode(
      network::mojom::FetchCredentialsMode::kOmit);
  preflight_request->SetSkipServiceWorker(true);
  preflight_request->SetReferrerString(request.ReferrerString());
  preflight_request->SetReferrerPolicy(request.GetReferrerPolicy());

  if (request.IsExternalRequest()) {
    preflight_request->SetHTTPHeaderField(
        HTTPNames::Access_Control_Request_External, "true");
  }

  const AtomicString request_headers =
      CreateAccessControlRequestHeadersHeader(request.HttpHeaderFields());
  if (request_headers != g_null_atom) {
    preflight_request->SetHTTPHeaderField(
        HTTPNames::Access_Control_Request_Headers, request_headers);
  }

  if (origin)
    preflight_request->SetHTTPOrigin(origin);

  return preflight_request;
}

// static
std::unique_ptr<ResourceRequest>
ThreadableLoader::CreateAccessControlPreflightRequestForTesting(
    const ResourceRequest& request) {
  return CreateAccessControlPreflightRequest(request, nullptr);
}

ThreadableLoader::ThreadableLoader(
    ExecutionContext& execution_context,
    ThreadableLoaderClient* client,
    const ResourceLoaderOptions& resource_loader_options)
    : client_(client),
      execution_context_(execution_context),
      resource_loader_options_(resource_loader_options),
      out_of_blink_cors_(RuntimeEnabledFeatures::OutOfBlinkCORSEnabled()),
      is_using_data_consumer_handle_(false),
      async_(resource_loader_options.synchronous_policy ==
             kRequestAsynchronously),
      request_context_(mojom::RequestContextType::UNSPECIFIED),
      fetch_request_mode_(network::mojom::FetchRequestMode::kSameOrigin),
      fetch_credentials_mode_(network::mojom::FetchCredentialsMode::kOmit),
      timeout_timer_(execution_context_->GetTaskRunner(TaskType::kNetworking),
                     this,
                     &ThreadableLoader::DidTimeout),
      redirect_limit_(kMaxRedirects),
      redirect_mode_(network::mojom::FetchRedirectMode::kFollow),
      override_referrer_(false) {
  DCHECK(client);
  if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context_))
    scope->EnsureFetcher();
}

void ThreadableLoader::Start(const ResourceRequest& request) {
  original_security_origin_ = security_origin_ = request.RequestorOrigin();
  // Setting an outgoing referer is only supported in the async code path.
  DCHECK(async_ || request.HttpReferrer().IsEmpty());

  bool cors_enabled =
      CORS::IsCORSEnabledRequestMode(request.GetFetchRequestMode());

  // kPreventPreflight can be used only when the CORS is enabled.
  DCHECK(request.CORSPreflightPolicy() ==
             network::mojom::CORSPreflightPolicy::kConsiderPreflight ||
         cors_enabled);

  initial_request_url_ = request.Url();
  last_request_url_ = initial_request_url_;
  request_context_ = request.GetRequestContext();
  fetch_request_mode_ = request.GetFetchRequestMode();
  fetch_credentials_mode_ = request.GetFetchCredentialsMode();
  redirect_mode_ = request.GetFetchRedirectMode();

  if (request.GetFetchRequestMode() ==
      network::mojom::FetchRequestMode::kNoCORS) {
    SECURITY_CHECK(WebCORS::IsNoCORSAllowedContext(request_context_));
  }
  cors_flag_ = CORS::CalculateCORSFlag(request.Url(), GetSecurityOrigin(),
                                       request.GetFetchRequestMode());

  // The CORS flag variable is not yet used at the step in the spec that
  // corresponds to this line, but divert |cors_flag_| here for convenience.
  if (cors_flag_ && request.GetFetchRequestMode() ==
                        network::mojom::FetchRequestMode::kSameOrigin) {
    ThreadableLoaderClient* client = client_;
    Clear();
    client->DidFail(ResourceError(
        request.Url(), network::CORSErrorStatus(
                           network::mojom::CORSError::kDisallowedByMode)));
    return;
  }

  request_started_ = CurrentTimeTicks();

  // Save any headers on the request here. If this request redirects
  // cross-origin, we cancel the old request create a new one, and copy these
  // headers.
  request_headers_ = request.HttpHeaderFields();
  report_upload_progress_ = request.ReportUploadProgress();

  ResourceRequest new_request(request);

  // Set the service worker mode to none if "bypass for network" in DevTools is
  // enabled.
  bool should_bypass_service_worker = false;
  probe::shouldBypassServiceWorker(execution_context_,
                                   &should_bypass_service_worker);
  if (should_bypass_service_worker)
    new_request.SetSkipServiceWorker(true);

  // In S13nServiceWorker, if the controller service worker has no fetch event
  // handler, it's skipped entirely, so we should treat that case the same as
  // having no controller. In non-S13nServiceWorker, we can't do that since we
  // don't know which service worker will handle the request since it's
  // determined on the browser process and skipWaiting() can happen in the
  // meantime.
  //
  // TODO(crbug.com/715640): When non-S13nServiceWorker is removed,
  // is_controlled_by_service_worker is the same as
  // ControllerServiceWorkerMode::kControlled, so this code can be simplified.
  bool is_controlled_by_service_worker = false;
  switch (execution_context_->Fetcher()->IsControlledByServiceWorker()) {
    case blink::mojom::ControllerServiceWorkerMode::kControlled:
      is_controlled_by_service_worker = true;
      break;
    case blink::mojom::ControllerServiceWorkerMode::kNoFetchEventHandler:
      if (ServiceWorkerUtils::IsServicificationEnabled())
        is_controlled_by_service_worker = false;
      else
        is_controlled_by_service_worker = true;
      break;
    case blink::mojom::ControllerServiceWorkerMode::kNoController:
      is_controlled_by_service_worker = false;
      break;
  }

  // Process the CORS protocol inside the ThreadableLoader for the
  // following cases:
  //
  // - When the request is sync or the protocol is unsupported since we can
  //   assume that any service worker (SW) is skipped for such requests by
  //   content/ code.
  // - When |skip_service_worker| is true, any SW will be skipped.
  // - If we're not yet controlled by a SW, then we're sure that this
  //   request won't be intercepted by a SW. In case we end up with
  //   sending a CORS preflight request, the actual request to be sent later
  //   may be intercepted. This is taken care of in LoadPreflightRequest() by
  //   setting |skip_service_worker| to true.
  //
  // From the above analysis, you can see that the request can never be
  // intercepted by a SW inside this if-block. It's because:
  // - |skip_service_worker| needs to be false, and
  // - we're controlled by a SW at this point
  // to allow a SW to intercept the request. Even when the request gets issued
  // asynchronously after performing the CORS preflight, it doesn't get
  // intercepted since LoadPreflightRequest() sets the flag to kNone in advance.
  if (!async_ || new_request.GetSkipServiceWorker() ||
      !SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          new_request.Url().Protocol()) ||
      !is_controlled_by_service_worker) {
    DispatchInitialRequest(new_request);
    return;
  }

  if (CORS::IsCORSEnabledRequestMode(request.GetFetchRequestMode())) {
    // Save the request to fallback_request_for_service_worker to use when the
    // service worker doesn't handle (call respondWith()) a CORS enabled
    // request.
    fallback_request_for_service_worker_ = ResourceRequest(request);
    // Skip the service worker for the fallback request.
    fallback_request_for_service_worker_.SetSkipServiceWorker(true);
  }

  LoadRequest(new_request, resource_loader_options_);
}

void ThreadableLoader::DispatchInitialRequest(ResourceRequest& request) {
  if (out_of_blink_cors_ || (!request.IsExternalRequest() && !cors_flag_)) {
    LoadRequest(request, resource_loader_options_);
    return;
  }

  DCHECK(CORS::IsCORSEnabledRequestMode(request.GetFetchRequestMode()) ||
         request.IsExternalRequest());

  MakeCrossOriginAccessRequest(request);
}

void ThreadableLoader::PrepareCrossOriginRequest(
    ResourceRequest& request) const {
  if (GetSecurityOrigin())
    request.SetHTTPOrigin(GetSecurityOrigin());

  // TODO(domfarolino): Stop setting the HTTPReferrer header, and instead use
  // ResourceRequest::referrer_. See https://crbug.com/850813.
  if (override_referrer_)
    request.SetHTTPReferrer(referrer_after_redirect_);
}

void ThreadableLoader::LoadPreflightRequest(
    const ResourceRequest& actual_request,
    const ResourceLoaderOptions& actual_options) {
  std::unique_ptr<ResourceRequest> preflight_request =
      CreateAccessControlPreflightRequest(actual_request, GetSecurityOrigin());

  actual_request_ = actual_request;
  actual_options_ = actual_options;

  // Explicitly set |skip_service_worker| to true here. Although the page is
  // not controlled by a SW at this point, a new SW may be controlling the
  // page when this actual request gets sent later. We should not send the
  // actual request to the SW. See https://crbug.com/604583.
  actual_request_.SetSkipServiceWorker(true);

  // Create a ResourceLoaderOptions for preflight.
  ResourceLoaderOptions preflight_options = actual_options;

  LoadRequest(*preflight_request, preflight_options);
}

void ThreadableLoader::MakeCrossOriginAccessRequest(
    const ResourceRequest& request) {
  DCHECK(CORS::IsCORSEnabledRequestMode(request.GetFetchRequestMode()) ||
         request.IsExternalRequest());
  DCHECK(client_);
  DCHECK(!GetResource());

  // Cross-origin requests are only allowed certain registered schemes. We would
  // catch this when checking response headers later, but there is no reason to
  // send a request, preflighted or not, that's guaranteed to be denied.
  if (!SchemeRegistry::ShouldTreatURLSchemeAsCORSEnabled(
          request.Url().Protocol())) {
    DispatchDidFail(ResourceError(
        request.Url(), network::CORSErrorStatus(
                           network::mojom::CORSError::kCORSDisabledScheme)));
    return;
  }

  // Non-secure origins may not make "external requests":
  // https://wicg.github.io/cors-rfc1918/#integration-fetch
  String error_message;
  // TODO(yhirano): Consider moving this branch elsewhere.
  if (!execution_context_->IsSecureContext(error_message) &&
      request.IsExternalRequest()) {
    // TODO(yhirano): Fix the link.
    DispatchDidFail(ResourceError::CancelledDueToAccessCheckError(
        request.Url(), ResourceRequestBlockedReason::kOrigin,
        "Requests to internal network resources are not allowed "
        "from non-secure contexts (see https://goo.gl/Y0ZkNV). "
        "This is an experimental restriction which is part of "
        "'https://mikewest.github.io/cors-rfc1918/'."));
    return;
  }

  ResourceRequest cross_origin_request(request);
  ResourceLoaderOptions cross_origin_options(resource_loader_options_);

  cross_origin_request.RemoveUserAndPassFromURL();

  // Enforce the CORS preflight for checking the Access-Control-Allow-External
  // header. The CORS preflight cache doesn't help for this purpose.
  if (request.IsExternalRequest()) {
    LoadPreflightRequest(cross_origin_request, cross_origin_options);
    return;
  }

  if (request.GetFetchRequestMode() !=
      network::mojom::FetchRequestMode::kCORSWithForcedPreflight) {
    if (request.CORSPreflightPolicy() ==
        network::mojom::CORSPreflightPolicy::kPreventPreflight) {
      PrepareCrossOriginRequest(cross_origin_request);
      LoadRequest(cross_origin_request, cross_origin_options);
      return;
    }

    DCHECK_EQ(request.CORSPreflightPolicy(),
              network::mojom::CORSPreflightPolicy::kConsiderPreflight);

    // We use ContainsOnlyCORSSafelistedOrForbiddenHeaders() here since
    // |request| may have been modified in the process of loading (not from
    // the user's input). For example, referrer. We need to accept them. For
    // security, we must reject forbidden headers/methods at the point we
    // accept user's input. Not here.
    if (CORS::IsCORSSafelistedMethod(request.HttpMethod()) &&
        CORS::ContainsOnlyCORSSafelistedOrForbiddenHeaders(
            request.HttpHeaderFields())) {
      PrepareCrossOriginRequest(cross_origin_request);
      LoadRequest(cross_origin_request, cross_origin_options);
      return;
    }
  }

  // Now, we need to check that the request passes the CORS preflight either by
  // issuing a CORS preflight or based on an entry in the CORS preflight cache.

  bool should_ignore_preflight_cache = false;
  // Prevent use of the CORS preflight cache when instructed by the DevTools
  // not to use caches.
  probe::shouldForceCORSPreflight(execution_context_,
                                  &should_ignore_preflight_cache);
  if (should_ignore_preflight_cache ||
      !CORS::CheckIfRequestCanSkipPreflight(
          GetSecurityOrigin()->ToString(), cross_origin_request.Url(),
          cross_origin_request.GetFetchCredentialsMode(),
          cross_origin_request.HttpMethod(),
          cross_origin_request.HttpHeaderFields())) {
    LoadPreflightRequest(cross_origin_request, cross_origin_options);
    return;
  }

  // We don't want any requests that could involve a CORS preflight to get
  // intercepted by a foreign SW, even if we have the result of the preflight
  // cached already. See https://crbug.com/674370.
  cross_origin_request.SetSkipServiceWorker(true);

  PrepareCrossOriginRequest(cross_origin_request);
  LoadRequest(cross_origin_request, cross_origin_options);
}

ThreadableLoader::~ThreadableLoader() {
  // |client_| is a raw pointer and having a non-null |client_| here probably
  // means UaF.
  // In the detached case, |this| is held by DetachedClient defined above, but
  // SelfKeepAlive in DetachedClient is forcibly cancelled on worker thread
  // termination. We can safely ignore this case.
  CHECK(!client_ || detached_);
  DCHECK(!GetResource());
}

void ThreadableLoader::SetTimeout(const TimeDelta& timeout) {
  timeout_ = timeout;

  // |request_started_| <= TimeTicks() indicates loading is either not yet
  // started or is already finished, and thus we don't need to do anything with
  // timeout_timer_.
  if (request_started_ <= TimeTicks()) {
    DCHECK(!timeout_timer_.IsActive());
    return;
  }

  DCHECK(async_);
  timeout_timer_.Stop();

  // At the time of this method's implementation, it is only ever called for an
  // inflight request by XMLHttpRequest.
  //
  // The XHR request says to resolve the time relative to when the request
  // was initially sent, however other uses of this method may need to
  // behave differently, in which case this should be re-arranged somehow.
  if (!timeout_.is_zero()) {
    TimeDelta elapsed_time = CurrentTimeTicks() - request_started_;
    TimeDelta resolved_time = std::max(timeout_ - elapsed_time, TimeDelta());
    timeout_timer_.StartOneShot(resolved_time, FROM_HERE);
  }
}

void ThreadableLoader::Cancel() {
  // Cancel can re-enter, and therefore |resource()| might be null here as a
  // result.
  if (!client_ || !GetResource()) {
    Clear();
    return;
  }

  DispatchDidFail(ResourceError::CancelledError(GetResource()->Url()));
}

void ThreadableLoader::Detach() {
  Resource* resource = GetResource();
  if (!resource)
    return;
  detached_ = true;
  client_ = new DetachedClient(this);
}

void ThreadableLoader::SetDefersLoading(bool value) {
  if (GetResource() && GetResource()->Loader())
    GetResource()->Loader()->SetDefersLoading(value);
}

void ThreadableLoader::Clear() {
  client_ = nullptr;
  timeout_timer_.Stop();
  request_started_ = TimeTicks();
  if (GetResource())
    checker_.WillRemoveClient();
  ClearResource();
}

// In this method, we can clear |request| to tell content::WebURLLoaderImpl of
// Chromium not to follow the redirect. This works only when this method is
// called by RawResource::willSendRequest(). If called by
// RawResource::didAddClient(), clearing |request| won't be propagated to
// content::WebURLLoaderImpl. So, this loader must also get detached from the
// resource by calling clearResource().
// TODO(toyoshim): Implement OOR-CORS mode specific redirect code.
bool ThreadableLoader::RedirectReceived(
    Resource* resource,
    const ResourceRequest& new_request,
    const ResourceResponse& redirect_response) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());
  {
    AssignOnScopeExit assign_on_scope_exit(new_request.Url(),
                                           &last_request_url_);

    checker_.RedirectReceived();

    const KURL& new_url = new_request.Url();
    const KURL& original_url = redirect_response.Url();

    if (out_of_blink_cors_)
      return client_->WillFollowRedirect(new_url, redirect_response);

    if (!actual_request_.IsNull()) {
      ReportResponseReceived(resource->Identifier(), redirect_response);

      HandlePreflightFailure(
          original_url,
          network::CORSErrorStatus(
              network::mojom::CORSError::kPreflightDisallowedRedirect));
      return false;
    }

    if (cors_flag_) {
      if (const auto error_status = CORS::CheckAccess(
              original_url, redirect_response.HttpStatusCode(),
              redirect_response.HttpHeaderFields(),
              new_request.GetFetchCredentialsMode(), *GetSecurityOrigin())) {
        DispatchDidFail(ResourceError(original_url, *error_status));
        return false;
      }
    }

    if (redirect_mode_ == network::mojom::FetchRedirectMode::kError) {
      bool follow = client_->WillFollowRedirect(new_url, redirect_response);
      DCHECK(!follow);
      return false;
    }

    if (redirect_mode_ == network::mojom::FetchRedirectMode::kManual) {
      auto redirect_response_to_pass = redirect_response;
      redirect_response_to_pass.SetType(
          network::mojom::FetchResponseType::kOpaqueRedirect);
      bool follow =
          client_->WillFollowRedirect(new_url, redirect_response_to_pass);
      DCHECK(!follow);
      return false;
    }

    DCHECK_EQ(redirect_mode_, network::mojom::FetchRedirectMode::kFollow);

    if (redirect_limit_ <= 0) {
      ThreadableLoaderClient* client = client_;
      Clear();
      ConsoleMessage* message = ConsoleMessage::Create(
          kNetworkMessageSource, kErrorMessageLevel,
          "Failed to load resource: net::ERR_TOO_MANY_REDIRECTS",
          SourceLocation::Capture(original_url, 0, 0));
      execution_context_->AddConsoleMessage(message);
      client->DidFailRedirectCheck();
      return false;
    }
    --redirect_limit_;

    auto redirect_response_to_pass = redirect_response;
    redirect_response_to_pass.SetType(response_tainting_);

    // Allow same origin requests to continue after allowing clients to audit
    // the redirect.
    if (!(cors_flag_ ||
          CORS::CalculateCORSFlag(new_url, GetSecurityOrigin(),
                                  new_request.GetFetchRequestMode()))) {
      bool follow =
          client_->WillFollowRedirect(new_url, redirect_response_to_pass);
      response_tainting_ = CORS::CalculateResponseTainting(
          new_url, new_request.GetFetchRequestMode(), GetSecurityOrigin(),
          CORSFlag::Unset);
      return follow;
    }

    probe::didReceiveCORSRedirectResponse(
        execution_context_, resource->Identifier(),
        GetDocument() && GetDocument()->GetFrame()
            ? GetDocument()->GetFrame()->Loader().GetDocumentLoader()
            : nullptr,
        redirect_response_to_pass, resource);

    if (auto error_status = CORS::CheckRedirectLocation(
            new_url, fetch_request_mode_, GetSecurityOrigin(),
            cors_flag_ ? CORSFlag::Set : CORSFlag::Unset)) {
      DispatchDidFail(ResourceError(original_url, *error_status));
      return false;
    }

    if (!client_->WillFollowRedirect(new_url, redirect_response_to_pass))
      return false;

    // FIXME: consider combining this with CORS redirect handling performed by
    // CrossOriginAccessControl::handleRedirect().
    if (GetResource())
      checker_.WillRemoveClient();
    ClearResource();

    // If
    // - CORS flag is set, and
    // - the origin of the redirect target URL is not same origin with the
    //   origin of the current request's URL
    // set the source origin to a unique opaque origin.
    //
    // See https://fetch.spec.whatwg.org/#http-redirect-fetch.
    if (cors_flag_) {
      scoped_refptr<const SecurityOrigin> original_origin =
          SecurityOrigin::Create(original_url);
      scoped_refptr<const SecurityOrigin> new_origin =
          SecurityOrigin::Create(new_url);
      if (!original_origin->IsSameSchemeHostPort(new_origin.get()))
        security_origin_ = SecurityOrigin::CreateUniqueOpaque();
    }

    // Set |cors_flag_| so that further logic (corresponds to the main fetch in
    // the spec) will be performed with CORS flag set.
    // See https://fetch.spec.whatwg.org/#http-redirect-fetch.
    cors_flag_ = true;

    // Save the referrer to use when following the redirect.
    override_referrer_ = true;
    // TODO(domfarolino): Use ReferrerString() once https://crbug.com/850813 is
    // closed and we stop storing the referrer string as a `Referer` header.
    referrer_after_redirect_ =
        Referrer(new_request.HttpReferrer(), new_request.GetReferrerPolicy());
  }
  // We're initiating a new request (for redirect), so update
  // |last_request_url_| by destroying |assign_on_scope_exit|.

  ResourceRequest cross_origin_request(new_request);

  // Remove any headers that may have been added by the network layer that cause
  // access control to fail.
  cross_origin_request.ClearHTTPReferrer();
  cross_origin_request.ClearHTTPOrigin();
  cross_origin_request.ClearHTTPUserAgent();
  // Add any request headers which we previously saved from the
  // original request.
  for (const auto& header : request_headers_)
    cross_origin_request.SetHTTPHeaderField(header.key, header.value);
  cross_origin_request.SetReportUploadProgress(report_upload_progress_);
  MakeCrossOriginAccessRequest(cross_origin_request);

  return false;
}

void ThreadableLoader::RedirectBlocked() {
  checker_.RedirectBlocked();

  // Tells the client that a redirect was received but not followed (for an
  // unknown reason).
  ThreadableLoaderClient* client = client_;
  Clear();
  client->DidFailRedirectCheck();
}

void ThreadableLoader::DataSent(Resource* resource,
                                unsigned long long bytes_sent,
                                unsigned long long total_bytes_to_be_sent) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());
  DCHECK(async_);

  checker_.DataSent();
  client_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

void ThreadableLoader::DataDownloaded(Resource* resource, int data_length) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());
  DCHECK(actual_request_.IsNull());

  checker_.DataDownloaded();
  client_->DidDownloadData(data_length);
}

void ThreadableLoader::DidReceiveResourceTiming(
    Resource* resource,
    const ResourceTimingInfo& info) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());

  client_->DidReceiveResourceTiming(info);
}

void ThreadableLoader::DidDownloadToBlob(Resource* resource,
                                         scoped_refptr<BlobDataHandle> blob) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());

  checker_.DidDownloadToBlob();
  client_->DidDownloadToBlob(std::move(blob));
}

void ThreadableLoader::HandlePreflightResponse(
    const ResourceResponse& response) {
  base::Optional<network::CORSErrorStatus> cors_error_status =
      CORS::CheckPreflightAccess(response.Url(), response.HttpStatusCode(),
                                 response.HttpHeaderFields(),
                                 actual_request_.GetFetchCredentialsMode(),
                                 *GetSecurityOrigin());
  if (cors_error_status) {
    HandlePreflightFailure(response.Url(), *cors_error_status);
    return;
  }

  base::Optional<network::mojom::CORSError> preflight_error =
      CORS::CheckPreflight(response.HttpStatusCode());
  if (preflight_error) {
    HandlePreflightFailure(response.Url(),
                           network::CORSErrorStatus(*preflight_error));
    return;
  }

  base::Optional<network::CORSErrorStatus> error_status;
  if (actual_request_.IsExternalRequest()) {
    error_status = CORS::CheckExternalPreflight(response.HttpHeaderFields());
    if (error_status) {
      HandlePreflightFailure(response.Url(), *error_status);
      return;
    }
  }

  String access_control_error_description;
  error_status = CORS::EnsurePreflightResultAndCacheOnSuccess(
      response.HttpHeaderFields(), GetSecurityOrigin()->ToString(),
      actual_request_.Url(), actual_request_.HttpMethod(),
      actual_request_.HttpHeaderFields(),
      actual_request_.GetFetchCredentialsMode());
  if (error_status)
    HandlePreflightFailure(response.Url(), *error_status);
}

void ThreadableLoader::ReportResponseReceived(
    unsigned long identifier,
    const ResourceResponse& response) {
  LocalFrame* frame = GetDocument() ? GetDocument()->GetFrame() : nullptr;
  if (!frame)
    return;
  DocumentLoader* loader = frame->Loader().GetDocumentLoader();
  probe::didReceiveResourceResponse(execution_context_, identifier, loader,
                                    response, GetResource());
  frame->Console().ReportResourceResponseReceived(loader, identifier, response);
}

void ThreadableLoader::ResponseReceived(
    Resource* resource,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK_EQ(resource, GetResource());
  DCHECK(client_);

  checker_.ResponseReceived();

  if (handle)
    is_using_data_consumer_handle_ = true;

  // TODO(toyoshim): Support OOR-CORS preflight and Service Worker case.
  // Note that CORS-preflight is usually handled in the Network Service side,
  // but still done in Blink side when it is needed on redirects.
  // https://crbug.com/736308.
  if (out_of_blink_cors_ && !response.WasFetchedViaServiceWorker()) {
    DCHECK(actual_request_.IsNull());
    fallback_request_for_service_worker_ = ResourceRequest();
    client_->DidReceiveResponse(resource->Identifier(), response,
                                std::move(handle));
    return;
  }

  // Code path for legacy Blink CORS.
  if (!actual_request_.IsNull()) {
    ReportResponseReceived(resource->Identifier(), response);
    HandlePreflightResponse(response);
    return;
  }

  if (response.WasFetchedViaServiceWorker()) {
    if (response.WasFallbackRequiredByServiceWorker()) {
      // At this point we must have m_fallbackRequestForServiceWorker. (For
      // SharedWorker the request won't be CORS or CORS-with-preflight,
      // therefore fallback-to-network is handled in the browser process when
      // the ServiceWorker does not call respondWith().)
      DCHECK(!fallback_request_for_service_worker_.IsNull());
      ReportResponseReceived(resource->Identifier(), response);
      LoadFallbackRequestForServiceWorker();
      return;
    }

    // It's possible that we issue a fetch with request with non "no-cors"
    // mode but get an opaque filtered response if a service worker is involved.
    // We dispatch a CORS failure for the case.
    // TODO(yhirano): This is probably not spec conformant. Fix it after
    // https://github.com/w3c/preload/issues/100 is addressed.
    if (fetch_request_mode_ != network::mojom::FetchRequestMode::kNoCORS &&
        response.GetType() == network::mojom::FetchResponseType::kOpaque) {
      DispatchDidFail(ResourceError(
          response.Url(), network::CORSErrorStatus(
                              network::mojom::CORSError::kInvalidResponse)));
      return;
    }

    fallback_request_for_service_worker_ = ResourceRequest();
    client_->DidReceiveResponse(resource->Identifier(), response,
                                std::move(handle));
    return;
  }

  // Even if the request met the conditions to get handled by a Service Worker
  // in the constructor of this class (and therefore
  // |m_fallbackRequestForServiceWorker| is set), the Service Worker may skip
  // processing the request. Only if the request is same origin, the skipped
  // response may come here (wasFetchedViaServiceWorker() returns false) since
  // such a request doesn't have to go through the CORS algorithm by calling
  // loadFallbackRequestForServiceWorker().
  DCHECK(fallback_request_for_service_worker_.IsNull() ||
         GetSecurityOrigin()->CanRequest(
             fallback_request_for_service_worker_.Url()));
  fallback_request_for_service_worker_ = ResourceRequest();

  if (cors_flag_) {
    base::Optional<network::CORSErrorStatus> access_error = CORS::CheckAccess(
        response.Url(), response.HttpStatusCode(), response.HttpHeaderFields(),
        fetch_credentials_mode_, *GetSecurityOrigin());
    if (access_error) {
      ReportResponseReceived(resource->Identifier(), response);
      DispatchDidFail(ResourceError(response.Url(), *access_error));
      return;
    }
  }

  DCHECK_EQ(&response, &resource->GetResponse());
  resource->SetResponseType(response_tainting_);
  DCHECK_EQ(response.GetType(), response_tainting_);
  client_->DidReceiveResponse(resource->Identifier(), response,
                              std::move(handle));
}

void ThreadableLoader::SetSerializedCachedMetadata(Resource*,
                                                   const char* data,
                                                   size_t size) {
  checker_.SetSerializedCachedMetadata();

  if (!actual_request_.IsNull())
    return;
  client_->DidReceiveCachedMetadata(data, size);
}

void ThreadableLoader::DataReceived(Resource* resource,
                                    const char* data,
                                    size_t data_length) {
  DCHECK_EQ(resource, GetResource());
  DCHECK(client_);

  checker_.DataReceived();

  if (is_using_data_consumer_handle_)
    return;

  // Preflight data should be invisible to clients.
  if (!actual_request_.IsNull())
    return;

  DCHECK(fallback_request_for_service_worker_.IsNull());

  // TODO(junov): Fix the ThreadableLoader ecosystem to use size_t. Until then,
  // we use safeCast to trap potential overflows.
  client_->DidReceiveData(data, SafeCast<unsigned>(data_length));
}

void ThreadableLoader::NotifyFinished(Resource* resource) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());

  checker_.NotifyFinished(resource);

  // Don't throw an exception for failed sync local file loads.
  // TODO(japhet): This logic has been moved around but unchanged since 2007.
  // Tested by fast/xmlhttprequest/xmlhttprequest-missing-file-exception.html
  // Do we still need this?
  bool is_sync_to_local_file = resource->Url().IsLocalFile() && !async_;

  if (resource->ErrorOccurred() && !is_sync_to_local_file) {
    DispatchDidFail(resource->GetResourceError());
    return;
  }

  DCHECK(fallback_request_for_service_worker_.IsNull());

  if (!actual_request_.IsNull()) {
    DCHECK(actual_request_.IsExternalRequest() || cors_flag_);
    LoadActualRequest();
    return;
  }

  ThreadableLoaderClient* client = client_;
  // Protect the resource in |didFinishLoading| in order not to release the
  // downloaded file.
  Persistent<Resource> protect = GetResource();
  Clear();
  client->DidFinishLoading(resource->Identifier());
}

void ThreadableLoader::DidTimeout(TimerBase* timer) {
  DCHECK(async_);
  DCHECK_EQ(timer, &timeout_timer_);
  // clearResource() may be called in clear() and some other places. clear()
  // calls stop() on |m_timeoutTimer|. In the other places, the resource is set
  // again. If the creation fails, clear() is called. So, here, resource() is
  // always non-nullptr.
  DCHECK(GetResource());
  // When |m_client| is set to nullptr only in clear() where |m_timeoutTimer|
  // is stopped. So, |m_client| is always non-nullptr here.
  DCHECK(client_);

  DispatchDidFail(ResourceError::TimeoutError(GetResource()->Url()));
}

void ThreadableLoader::LoadFallbackRequestForServiceWorker() {
  if (GetResource())
    checker_.WillRemoveClient();
  ClearResource();
  ResourceRequest fallback_request(fallback_request_for_service_worker_);
  fallback_request_for_service_worker_ = ResourceRequest();
  DispatchInitialRequest(fallback_request);
}

void ThreadableLoader::LoadActualRequest() {
  ResourceRequest actual_request = actual_request_;
  ResourceLoaderOptions actual_options = actual_options_;
  actual_request_ = ResourceRequest();
  actual_options_ = ResourceLoaderOptions();

  if (GetResource())
    checker_.WillRemoveClient();
  ClearResource();

  PrepareCrossOriginRequest(actual_request);
  LoadRequest(actual_request, actual_options);
}

void ThreadableLoader::HandlePreflightFailure(
    const KURL& url,
    const network::CORSErrorStatus& error_status) {
  // Prevent NotifyFinished() from bypassing access check.
  actual_request_ = ResourceRequest();

  DispatchDidFail(ResourceError(url, error_status));
}

void ThreadableLoader::DispatchDidFail(const ResourceError& error) {
  if (!out_of_blink_cors_ && error.CORSErrorStatus()) {
    String message = CORS::GetErrorString(
        *error.CORSErrorStatus(), initial_request_url_, last_request_url_,
        *GetSecurityOrigin(), ResourceType::kRaw,
        resource_loader_options_.initiator_info.name);
    execution_context_->AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kErrorMessageLevel, std::move(message)));
  }
  Resource* resource = GetResource();
  if (resource)
    resource->SetResponseType(network::mojom::FetchResponseType::kError);
  ThreadableLoaderClient* client = client_;
  Clear();
  client->DidFail(error);
}

void ThreadableLoader::LoadRequest(
    ResourceRequest& request,
    ResourceLoaderOptions resource_loader_options) {
  resource_loader_options.cors_handling_by_resource_fetcher =
      kDisableCORSHandlingByResourceFetcher;

  if (out_of_blink_cors_) {
    if (request.GetFetchCredentialsMode() ==
        network::mojom::FetchCredentialsMode::kOmit) {
      // See comments at network::ResourceRequest::fetch_credentials_mode.
      request.SetAllowStoredCredentials(false);
    }
  } else {
    if (actual_request_.IsNull()) {
      response_tainting_ = CORS::CalculateResponseTainting(
          request.Url(), request.GetFetchRequestMode(), GetSecurityOrigin(),
          cors_flag_ ? CORSFlag::Set : CORSFlag::Unset);
      request.SetAllowStoredCredentials(CORS::CalculateCredentialsFlag(
          request.GetFetchCredentialsMode(), response_tainting_));
    } else {
      request.SetAllowStoredCredentials(false);
    }
  }

  request.SetRequestorOrigin(original_security_origin_);

  if (!actual_request_.IsNull())
    resource_loader_options.data_buffering_policy = kBufferData;

  if (!timeout_.is_zero()) {
    if (!async_) {
      request.SetTimeoutInterval(timeout_);
    } else if (!timeout_timer_.IsActive()) {
      // The timer can be active if this is the actual request of a
      // CORS-with-preflight request.
      timeout_timer_.StartOneShot(timeout_, FROM_HERE);
    }
  }

  FetchParameters new_params(request, resource_loader_options);
  DCHECK(!GetResource());

  checker_.WillAddClient();
  ResourceFetcher* fetcher = execution_context_->Fetcher();
  if (request.GetRequestContext() == mojom::RequestContextType::VIDEO ||
      request.GetRequestContext() == mojom::RequestContextType::AUDIO) {
    DCHECK(async_);
    RawResource::FetchMedia(new_params, fetcher, this);
  } else if (request.GetRequestContext() ==
             mojom::RequestContextType::MANIFEST) {
    DCHECK(async_);
    RawResource::FetchManifest(new_params, fetcher, this);
  } else if (async_) {
    RawResource::Fetch(new_params, fetcher, this);
  } else {
    RawResource::FetchSynchronously(new_params, fetcher, this);
  }
}

const SecurityOrigin* ThreadableLoader::GetSecurityOrigin() const {
  return security_origin_ ? security_origin_.get()
                          : execution_context_->GetSecurityOrigin();
}

Document* ThreadableLoader::GetDocument() const {
  return DynamicTo<Document>(execution_context_.Get());
}

void ThreadableLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  RawResourceClient::Trace(visitor);
}

}  // namespace blink
