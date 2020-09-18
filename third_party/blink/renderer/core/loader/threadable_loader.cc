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
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
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
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/cors/cors_error_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// Fetch API Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch-0
AtomicString CreateAccessControlRequestHeadersHeader(
    const HTTPHeaderMap& headers) {
  Vector<String> filtered_headers = cors::CorsUnsafeRequestHeaderNames(headers);

  if (!filtered_headers.size())
    return g_null_atom;

  // Sort header names lexicographically.
  std::sort(filtered_headers.begin(), filtered_headers.end(),
            WTF::CodeUnitCompareLessThan);
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
    : public GarbageCollected<DetachedClient>,
      public ThreadableLoaderClient {
 public:
  explicit DetachedClient(ThreadableLoader* loader)
      : self_keep_alive_(PERSISTENT_FROM_HERE, this), loader_(loader) {}
  ~DetachedClient() override {}

  void DidFinishLoading(uint64_t identifier) override {
    self_keep_alive_.Clear();
  }
  void DidFail(const ResourceError&) override { self_keep_alive_.Clear(); }
  void DidFailRedirectCheck() override { self_keep_alive_.Clear(); }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(loader_);
    ThreadableLoaderClient::Trace(visitor);
  }

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
  preflight_request->SetHttpMethod(http_names::kOPTIONS);
  preflight_request->SetHttpHeaderField(http_names::kAccessControlRequestMethod,
                                        request.HttpMethod());
  preflight_request->SetMode(network::mojom::RequestMode::kCors);
  preflight_request->SetPriority(request.Priority());
  preflight_request->SetRequestContext(request.GetRequestContext());
  preflight_request->SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
  preflight_request->SetSkipServiceWorker(true);
  preflight_request->SetReferrerString(request.ReferrerString());
  preflight_request->SetReferrerPolicy(request.GetReferrerPolicy());

  if (request.IsExternalRequest()) {
    preflight_request->SetHttpHeaderField(
        http_names::kAccessControlRequestExternal, "true");
  }

  const AtomicString request_headers =
      CreateAccessControlRequestHeadersHeader(request.HttpHeaderFields());
  if (request_headers != g_null_atom) {
    preflight_request->SetHttpHeaderField(
        http_names::kAccessControlRequestHeaders, request_headers);
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
    const ResourceLoaderOptions& resource_loader_options,
    ResourceFetcher* resource_fetcher)
    : client_(client),
      execution_context_(execution_context),
      resource_fetcher_(resource_fetcher),
      resource_loader_options_(resource_loader_options),
      out_of_blink_cors_(true),
      async_(resource_loader_options.synchronous_policy ==
             kRequestAsynchronously),
      request_context_(mojom::RequestContextType::UNSPECIFIED),
      request_mode_(network::mojom::RequestMode::kSameOrigin),
      credentials_mode_(network::mojom::CredentialsMode::kOmit),
      timeout_timer_(execution_context_->GetTaskRunner(TaskType::kNetworking),
                     this,
                     &ThreadableLoader::DidTimeout),
      redirect_limit_(kMaxRedirects),
      redirect_mode_(network::mojom::RedirectMode::kFollow),
      override_referrer_(false) {
  DCHECK(client);
  if (!resource_fetcher_) {
    if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context_))
      scope->EnsureFetcher();
    resource_fetcher_ = execution_context_->Fetcher();
  }
}

void ThreadableLoader::Start(ResourceRequest request) {
  original_security_origin_ = security_origin_ = request.RequestorOrigin();
  // Setting an outgoing referer is only supported in the async code path.
  DCHECK(async_ ||
         request.ReferrerString() == Referrer::ClientReferrerString());

  bool cors_enabled = cors::IsCorsEnabledRequestMode(request.GetMode());

  // kPreventPreflight can be used only when the CORS is enabled.
  DCHECK(request.CorsPreflightPolicy() ==
             network::mojom::CorsPreflightPolicy::kConsiderPreflight ||
         cors_enabled);

  initial_request_url_ = request.Url();
  last_request_url_ = initial_request_url_;
  request_context_ = request.GetRequestContext();
  request_mode_ = request.GetMode();
  credentials_mode_ = request.GetCredentialsMode();
  redirect_mode_ = request.GetRedirectMode();

  if (request.GetMode() == network::mojom::RequestMode::kNoCors) {
    SECURITY_CHECK(cors::IsNoCorsAllowedContext(request_context_));
  }
  cors_flag_ = cors::CalculateCorsFlag(request.Url(), GetSecurityOrigin(),
                                       request.IsolatedWorldOrigin().get(),
                                       request.GetMode());

  // The CORS flag variable is not yet used at the step in the spec that
  // corresponds to this line, but divert |cors_flag_| here for convenience.
  if (cors_flag_ &&
      request.GetMode() == network::mojom::RequestMode::kSameOrigin) {
    ThreadableLoaderClient* client = client_;
    Clear();
    client->DidFail(ResourceError(
        request.Url(), network::CorsErrorStatus(
                           network::mojom::CorsError::kDisallowedByMode)));
    return;
  }

  request_started_ = base::TimeTicks::Now();

  // Save any headers on the request here. If this request redirects
  // cross-origin, we cancel the old request create a new one, and copy these
  // headers.
  request_headers_ = request.HttpHeaderFields();
  report_upload_progress_ = request.ReportUploadProgress();

  // Set the service worker mode to none if "bypass for network" in DevTools is
  // enabled.
  bool should_bypass_service_worker = false;
  probe::ShouldBypassServiceWorker(execution_context_,
                                   &should_bypass_service_worker);
  if (should_bypass_service_worker)
    request.SetSkipServiceWorker(true);

  // Process the CORS protocol inside the ThreadableLoader for the
  // following cases:
  //
  // - When the request is sync or the protocol is unsupported since we can
  //   assume that any service worker (SW) is skipped for such requests by
  //   content/ code.
  // - When |GetSkipServiceWorker()| is true, any SW will be skipped.
  // - If we're not yet controlled by a SW, then we're sure that this
  //   request won't be intercepted by a SW. In case we end up with
  //   sending a CORS preflight request, the actual request to be sent later
  //   may be intercepted. This is taken care of in LoadPreflightRequest() by
  //   setting |GetSkipServiceWorker()| to true.
  //
  // From the above analysis, you can see that the request can never be
  // intercepted by a SW inside this if-block. It's because:
  // - |GetSkipServiceWorker()| needs to be false, and
  // - we're controlled by a SW at this point
  // to allow a SW to intercept the request. Even when the request gets issued
  // asynchronously after performing the CORS preflight, it doesn't get
  // intercepted since LoadPreflightRequest() sets the flag to kNone in advance.
  const bool is_controlled_by_service_worker =
      resource_fetcher_->IsControlledByServiceWorker() ==
      blink::mojom::ControllerServiceWorkerMode::kControlled;
  if (!async_ || request.GetSkipServiceWorker() ||
      !SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          request.Url().Protocol()) ||
      !is_controlled_by_service_worker) {
    DispatchInitialRequest(request);
    return;
  }

  if (!out_of_blink_cors_ &&
      cors::IsCorsEnabledRequestMode(request.GetMode())) {
    // Save the request to fallback_request_for_service_worker to use when the
    // service worker doesn't handle (call respondWith()) a CORS enabled
    // request.
    fallback_request_for_service_worker_.CopyFrom(request);
    // Skip the service worker for the fallback request.
    fallback_request_for_service_worker_.SetSkipServiceWorker(true);
  }

  LoadRequest(request, resource_loader_options_);
}

void ThreadableLoader::DispatchInitialRequest(ResourceRequest& request) {
  if (out_of_blink_cors_ || (!request.IsExternalRequest() && !cors_flag_)) {
    LoadRequest(request, resource_loader_options_);
    return;
  }

  DCHECK(cors::IsCorsEnabledRequestMode(request.GetMode()) ||
         request.IsExternalRequest());

  MakeCrossOriginAccessRequest(request);
}

void ThreadableLoader::PrepareCrossOriginRequest(
    ResourceRequest& request) const {
  if (GetSecurityOrigin())
    request.SetHTTPOrigin(GetSecurityOrigin());

  if (override_referrer_) {
    request.SetReferrerString(referrer_after_redirect_.referrer);
    request.SetReferrerPolicy(referrer_after_redirect_.referrer_policy);
  }
}

void ThreadableLoader::LoadPreflightRequest(
    const ResourceRequest& actual_request,
    const ResourceLoaderOptions& actual_options) {
  std::unique_ptr<ResourceRequest> preflight_request =
      CreateAccessControlPreflightRequest(actual_request, GetSecurityOrigin());

  actual_request_.CopyFrom(actual_request);
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
  DCHECK(cors::IsCorsEnabledRequestMode(request.GetMode()) ||
         request.IsExternalRequest());
  DCHECK(client_);
  DCHECK(!GetResource());

  // Cross-origin requests are only allowed certain registered schemes. We would
  // catch this when checking response headers later, but there is no reason to
  // send a request, preflighted or not, that's guaranteed to be denied.
  if (!SchemeRegistry::ShouldTreatURLSchemeAsCorsEnabled(
          request.Url().Protocol())) {
    DispatchDidFail(ResourceError(
        request.Url(), network::CorsErrorStatus(
                           network::mojom::CorsError::kCorsDisabledScheme)));
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

  ResourceRequest cross_origin_request;
  cross_origin_request.CopyFrom(request);
  ResourceLoaderOptions cross_origin_options(resource_loader_options_);

  cross_origin_request.RemoveUserAndPassFromURL();

  // Enforce the CORS preflight for checking the Access-Control-Allow-External
  // header. The CORS preflight cache doesn't help for this purpose.
  if (request.IsExternalRequest()) {
    LoadPreflightRequest(cross_origin_request, cross_origin_options);
    return;
  }

  if (request.GetMode() !=
      network::mojom::RequestMode::kCorsWithForcedPreflight) {
    if (request.CorsPreflightPolicy() ==
        network::mojom::CorsPreflightPolicy::kPreventPreflight) {
      PrepareCrossOriginRequest(cross_origin_request);
      LoadRequest(cross_origin_request, cross_origin_options);
      return;
    }

    DCHECK_EQ(request.CorsPreflightPolicy(),
              network::mojom::CorsPreflightPolicy::kConsiderPreflight);

    // We use ContainsOnlyCorsSafelistedOrForbiddenHeaders() here since
    // |request| may have been modified in the process of loading (not from
    // the user's input). For example, referrer. We need to accept them. For
    // security, we must reject forbidden headers/methods at the point we
    // accept user's input. Not here.
    if (cors::IsCorsSafelistedMethod(request.HttpMethod()) &&
        cors::ContainsOnlyCorsSafelistedOrForbiddenHeaders(
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
  probe::ShouldForceCorsPreflight(execution_context_,
                                  &should_ignore_preflight_cache);
  if (should_ignore_preflight_cache ||
      !cors::CheckIfRequestCanSkipPreflight(
          GetSecurityOrigin()->ToString(), cross_origin_request.Url(),
          cross_origin_request.GetCredentialsMode(),
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

ThreadableLoader::~ThreadableLoader() {}

void ThreadableLoader::SetTimeout(const base::TimeDelta& timeout) {
  timeout_ = timeout;

  // |request_started_| <= base::TimeTicks() indicates loading is either not yet
  // started or is already finished, and thus we don't need to do anything with
  // timeout_timer_.
  if (request_started_ <= base::TimeTicks()) {
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
    base::TimeDelta elapsed_time = base::TimeTicks::Now() - request_started_;
    base::TimeDelta resolved_time =
        std::max(timeout_ - elapsed_time, base::TimeDelta());
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
  client_ = MakeGarbageCollected<DetachedClient>(this);
}

void ThreadableLoader::SetDefersLoading(bool value) {
  if (GetResource() && GetResource()->Loader())
    GetResource()->Loader()->SetDefersLoading(value);
}

void ThreadableLoader::Clear() {
  client_ = nullptr;
  timeout_timer_.Stop();
  request_started_ = base::TimeTicks();
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
    const KURL& original_url = redirect_response.CurrentRequestUrl();

    if (out_of_blink_cors_)
      return client_->WillFollowRedirect(new_url, redirect_response);

    if (!actual_request_.IsNull()) {
      ReportResponseReceived(resource->InspectorId(), redirect_response);

      HandlePreflightFailure(
          original_url,
          network::CorsErrorStatus(
              network::mojom::CorsError::kPreflightDisallowedRedirect));
      return false;
    }

    if (cors_flag_) {
      if (const auto error_status = cors::CheckAccess(
              original_url, redirect_response.HttpHeaderFields(),
              new_request.GetCredentialsMode(), *GetSecurityOrigin())) {
        DispatchDidFail(ResourceError(original_url, *error_status));
        return false;
      }
    }

    if (redirect_mode_ == network::mojom::RedirectMode::kError) {
      bool follow = client_->WillFollowRedirect(new_url, redirect_response);
      DCHECK(!follow);
      return false;
    }

    if (redirect_mode_ == network::mojom::RedirectMode::kManual) {
      auto redirect_response_to_pass = redirect_response;
      redirect_response_to_pass.SetType(
          network::mojom::FetchResponseType::kOpaqueRedirect);
      bool follow =
          client_->WillFollowRedirect(new_url, redirect_response_to_pass);
      DCHECK(!follow);
      return false;
    }

    DCHECK_EQ(redirect_mode_, network::mojom::RedirectMode::kFollow);

    // TODO(crbug.com/1053866): Dead code as the redirect limit is checked in
    // the network service with OOR-CORS today. This will be removed very soon.
    // Consider if it's possible to show similar console messages, and to
    // notify |client|.
    if (redirect_limit_ <= 0) {
      ThreadableLoaderClient* client = client_;
      Clear();
      auto* message = MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kNetwork,
          mojom::ConsoleMessageLevel::kError,
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
          cors::CalculateCorsFlag(new_url, GetSecurityOrigin(),
                                  new_request.IsolatedWorldOrigin().get(),
                                  new_request.GetMode()))) {
      bool follow =
          client_->WillFollowRedirect(new_url, redirect_response_to_pass);
      response_tainting_ = cors::CalculateResponseTainting(
          new_url, new_request.GetMode(), GetSecurityOrigin(),
          new_request.IsolatedWorldOrigin().get(), CorsFlag::Unset);
      return follow;
    }

    probe::DidReceiveCorsRedirectResponse(
        execution_context_, resource->InspectorId(),
        GetFrame() ? GetFrame()->Loader().GetDocumentLoader() : nullptr,
        redirect_response_to_pass, resource);

    if (auto error_status = cors::CheckRedirectLocation(
            new_url, request_mode_, GetSecurityOrigin(),
            cors_flag_ ? CorsFlag::Set : CorsFlag::Unset)) {
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
      if (!original_origin->IsSameOriginWith(new_origin.get()))
        security_origin_ = SecurityOrigin::CreateUniqueOpaque();
    }

    // Set |cors_flag_| so that further logic (corresponds to the main fetch in
    // the spec) will be performed with CORS flag set.
    // See https://fetch.spec.whatwg.org/#http-redirect-fetch.
    cors_flag_ = true;

    // Save the referrer to use when following the redirect.
    override_referrer_ = true;
    referrer_after_redirect_ =
        Referrer(new_request.ReferrerString(), new_request.GetReferrerPolicy());
  }
  // We're initiating a new request (for redirect), so update
  // |last_request_url_| by destroying |assign_on_scope_exit|.

  ResourceRequest cross_origin_request;
  cross_origin_request.CopyFrom(new_request);

  // Remove any headers that may have been added by the network layer that cause
  // access control to fail.
  cross_origin_request.SetReferrerString(Referrer::NoReferrer());
  cross_origin_request.SetReferrerPolicy(
      network::mojom::ReferrerPolicy::kDefault);
  cross_origin_request.ClearHTTPOrigin();
  cross_origin_request.ClearHTTPUserAgent();
  // Add any request headers which we previously saved from the
  // original request.
  for (const auto& header : request_headers_)
    cross_origin_request.SetHttpHeaderField(header.key, header.value);
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
                                uint64_t bytes_sent,
                                uint64_t total_bytes_to_be_sent) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());
  DCHECK(async_);

  checker_.DataSent();
  client_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

void ThreadableLoader::DataDownloaded(Resource* resource,
                                      uint64_t data_length) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());
  DCHECK(actual_request_.IsNull());

  checker_.DataDownloaded();
  client_->DidDownloadData(data_length);
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
  base::Optional<network::CorsErrorStatus> cors_error_status =
      cors::CheckPreflightAccess(
          response.CurrentRequestUrl(), response.HttpStatusCode(),
          response.HttpHeaderFields(), actual_request_.GetCredentialsMode(),
          *GetSecurityOrigin());
  if (cors_error_status) {
    HandlePreflightFailure(response.CurrentRequestUrl(), *cors_error_status);
    return;
  }

  base::Optional<network::CorsErrorStatus> error_status;
  if (actual_request_.IsExternalRequest()) {
    error_status = cors::CheckExternalPreflight(response.HttpHeaderFields());
    if (error_status) {
      HandlePreflightFailure(response.CurrentRequestUrl(), *error_status);
      return;
    }
  }

  String access_control_error_description;
  error_status = cors::EnsurePreflightResultAndCacheOnSuccess(
      response.HttpHeaderFields(), GetSecurityOrigin()->ToString(),
      actual_request_.Url(), actual_request_.HttpMethod(),
      actual_request_.HttpHeaderFields(), actual_request_.GetCredentialsMode());
  if (error_status)
    HandlePreflightFailure(response.CurrentRequestUrl(), *error_status);
}

void ThreadableLoader::ReportResponseReceived(
    uint64_t identifier,
    const ResourceResponse& response) {
  LocalFrame* frame = GetFrame();
  if (!frame)
    return;
  DocumentLoader* loader = frame->Loader().GetDocumentLoader();
  probe::DidReceiveResourceResponse(probe::ToCoreProbeSink(execution_context_),
                                    identifier, loader, response,
                                    GetResource());
  frame->Console().ReportResourceResponseReceived(loader, identifier, response);
}

void ThreadableLoader::ResponseReceived(Resource* resource,
                                        const ResourceResponse& response) {
  DCHECK_EQ(resource, GetResource());
  DCHECK(client_);

  checker_.ResponseReceived();

  // TODO(toyoshim): Support OOR-CORS preflight and Service Worker case.
  // Note that CORS-preflight is usually handled in the Network Service side,
  // but still done in Blink side when it is needed on redirects.
  // https://crbug.com/736308.
  if (out_of_blink_cors_ && !response.WasFetchedViaServiceWorker()) {
    DCHECK(actual_request_.IsNull());
    fallback_request_for_service_worker_ = ResourceRequest();
    client_->DidReceiveResponse(resource->InspectorId(), response);
    return;
  }

  // Code path for legacy Blink CORS.
  if (!actual_request_.IsNull()) {
    ReportResponseReceived(resource->InspectorId(), response);
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
      ReportResponseReceived(resource->InspectorId(), response);
      LoadFallbackRequestForServiceWorker();
      return;
    }

    // It's possible that we issue a fetch with request with non "no-cors"
    // mode but get an opaque filtered response if a service worker is involved.
    // We dispatch a CORS failure for the case.
    // TODO(yhirano): This is probably not spec conformant. Fix it after
    // https://github.com/w3c/preload/issues/100 is addressed.
    if (request_mode_ != network::mojom::RequestMode::kNoCors &&
        response.GetType() == network::mojom::FetchResponseType::kOpaque) {
      DispatchDidFail(
          ResourceError(response.CurrentRequestUrl(),
                        network::CorsErrorStatus(
                            network::mojom::CorsError::kInvalidResponse)));
      return;
    }

    fallback_request_for_service_worker_ = ResourceRequest();
    client_->DidReceiveResponse(resource->InspectorId(), response);
    return;
  }

  // Even if the request met the conditions to get handled by a Service Worker
  // in the constructor of this class (and therefore
  // |fallback_request_for_service_worker_| is set), the Service Worker may skip
  // processing the request. Only if the request is same origin, the skipped
  // response may come here (wasFetchedViaServiceWorker() returns false) since
  // such a request doesn't have to go through the CORS algorithm by calling
  // loadFallbackRequestForServiceWorker().
  DCHECK(fallback_request_for_service_worker_.IsNull() ||
         GetSecurityOrigin()->CanRequest(
             fallback_request_for_service_worker_.Url()));
  fallback_request_for_service_worker_ = ResourceRequest();

  if (cors_flag_) {
    base::Optional<network::CorsErrorStatus> access_error = cors::CheckAccess(
        response.CurrentRequestUrl(), response.HttpHeaderFields(),
        credentials_mode_, *GetSecurityOrigin());
    if (access_error) {
      ReportResponseReceived(resource->InspectorId(), response);
      DispatchDidFail(
          ResourceError(response.CurrentRequestUrl(), *access_error));
      return;
    }
  }

  DCHECK_EQ(&response, &resource->GetResponse());
  resource->SetResponseType(response_tainting_);
  DCHECK_EQ(response.GetType(), response_tainting_);
  client_->DidReceiveResponse(resource->InspectorId(), response);
}

void ThreadableLoader::ResponseBodyReceived(Resource*, BytesConsumer& body) {
  checker_.ResponseBodyReceived();

  client_->DidStartLoadingResponseBody(body);
}

void ThreadableLoader::SetSerializedCachedMetadata(Resource*,
                                                   const uint8_t* data,
                                                   size_t size) {
  checker_.SetSerializedCachedMetadata();

  if (!actual_request_.IsNull())
    return;
  client_->DidReceiveCachedMetadata(reinterpret_cast<const char*>(data),
                                    SafeCast<int>(size));
}

void ThreadableLoader::DataReceived(Resource* resource,
                                    const char* data,
                                    size_t data_length) {
  DCHECK_EQ(resource, GetResource());
  DCHECK(client_);

  checker_.DataReceived();

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

  if (resource->ErrorOccurred()) {
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
  client->DidFinishLoading(resource->InspectorId());
}

void ThreadableLoader::DidTimeout(TimerBase* timer) {
  DCHECK(async_);
  DCHECK_EQ(timer, &timeout_timer_);
  // clearResource() may be called in clear() and some other places. clear()
  // calls stop() on |timeout_|. In the other places, the resource is set
  // again. If the creation fails, clear() is called. So, here, resource() is
  // always non-nullptr.
  DCHECK(GetResource());
  // When |client_| is set to nullptr only in clear() where |timeout_|
  // is stopped. So, |client_| is always non-nullptr here.
  DCHECK(client_);

  DispatchDidFail(ResourceError::TimeoutError(GetResource()->Url()));
}

void ThreadableLoader::LoadFallbackRequestForServiceWorker() {
  if (GetResource())
    checker_.WillRemoveClient();
  ClearResource();
  ResourceRequest fallback_request;
  fallback_request.CopyFrom(fallback_request_for_service_worker_);
  fallback_request_for_service_worker_.CopyFrom(ResourceRequest());
  DispatchInitialRequest(fallback_request);
}

void ThreadableLoader::LoadActualRequest() {
  ResourceRequest actual_request;
  actual_request.CopyFrom(actual_request_);
  ResourceLoaderOptions actual_options = actual_options_;
  actual_request_.CopyFrom(ResourceRequest());
  actual_options_ = ResourceLoaderOptions(nullptr /* world */);

  if (GetResource())
    checker_.WillRemoveClient();
  ClearResource();

  PrepareCrossOriginRequest(actual_request);
  LoadRequest(actual_request, actual_options);
}

void ThreadableLoader::HandlePreflightFailure(
    const KURL& url,
    const network::CorsErrorStatus& error_status) {
  // Prevent NotifyFinished() from bypassing access check.
  actual_request_ = ResourceRequest();

  DispatchDidFail(ResourceError(url, error_status));
}

void ThreadableLoader::DispatchDidFail(const ResourceError& error) {
  if (!out_of_blink_cors_ && error.CorsErrorStatus()) {
    String message = cors::GetErrorString(
        *error.CorsErrorStatus(), initial_request_url_, last_request_url_,
        *GetSecurityOrigin(), ResourceType::kRaw,
        resource_loader_options_.initiator_info.name);
    execution_context_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError, std::move(message)));
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
  if (out_of_blink_cors_) {
    if (request.GetCredentialsMode() ==
        network::mojom::CredentialsMode::kOmit) {
      // See comments at network::ResourceRequest::credentials_mode.
      request.SetAllowStoredCredentials(false);
    }
  } else {
    if (actual_request_.IsNull()) {
      response_tainting_ = cors::CalculateResponseTainting(
          request.Url(), request.GetMode(), GetSecurityOrigin(),
          request.IsolatedWorldOrigin().get(),
          cors_flag_ ? CorsFlag::Set : CorsFlag::Unset);
      request.SetAllowStoredCredentials(cors::CalculateCredentialsFlag(
          request.GetCredentialsMode(), response_tainting_));
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

  const mojom::RequestContextType request_context = request.GetRequestContext();
  FetchParameters new_params(std::move(request), resource_loader_options);
  DCHECK(!GetResource());

  checker_.WillAddClient();
  if (request_context == mojom::RequestContextType::VIDEO ||
      request_context == mojom::RequestContextType::AUDIO) {
    DCHECK(async_);
    RawResource::FetchMedia(new_params, resource_fetcher_, this);
  } else if (request_context == mojom::RequestContextType::MANIFEST) {
    DCHECK(async_);
    RawResource::FetchManifest(new_params, resource_fetcher_, this);
  } else if (async_) {
    RawResource::Fetch(new_params, resource_fetcher_, this);
  } else {
    RawResource::FetchSynchronously(new_params, resource_fetcher_, this);
  }
}

const SecurityOrigin* ThreadableLoader::GetSecurityOrigin() const {
  return security_origin_ ? security_origin_.get()
                          : resource_fetcher_->GetProperties()
                                .GetFetchClientSettingsObject()
                                .GetSecurityOrigin();
}

LocalFrame* ThreadableLoader::GetFrame() const {
  auto* window = DynamicTo<LocalDOMWindow>(execution_context_.Get());
  return window ? window->GetFrame() : nullptr;
}

void ThreadableLoader::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(client_);
  visitor->Trace(resource_fetcher_);
  RawResourceClient::Trace(visitor);
}

}  // namespace blink
