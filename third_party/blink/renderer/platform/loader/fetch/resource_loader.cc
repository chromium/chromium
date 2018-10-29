/*
 * Copyright (C) 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/platform/code_cache_loader.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_cors.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/cors/cors_error_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/mixed_content_autoupgrade_status.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/network/network_instrumentation.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

namespace {

bool IsThrottlableRequestContext(mojom::RequestContextType context) {
  // Requests that could run long should not be throttled as they
  // may stay there forever and avoid other requests from making
  // progress.
  // See https://crbug.com/837771 for the sample breakages.
  return context != mojom::RequestContextType::EVENT_SOURCE &&
         context != mojom::RequestContextType::FETCH &&
         context != mojom::RequestContextType::XML_HTTP_REQUEST &&
         context != mojom::RequestContextType::VIDEO &&
         context != mojom::RequestContextType::AUDIO;
}

void LogMixedAutoupgradeStatus(blink::MixedContentAutoupgradeStatus status) {
  UMA_HISTOGRAM_ENUMERATION("MixedAutoupgrade.ResourceRequest.Status", status);
}

void LogMixedAutoupgradeResponseOrError(int response_or_error_code) {
  base::UmaHistogramSparse(
      "MixedAutoupgrade.ResourceRequest.ErrorOrResponseCode",
      response_or_error_code);
}

}  // namespace

// CodeCacheRequest handles the requests to fetch data from code cache.
// This owns CodeCacheLoader that actually loads the data from the
// code cache. This class performs the necessary checks of matching the
// resource response time and the code cache response time before sending
// the data to the resource. It caches the data returned from the code cache
// if the response wasn't received.  One CodeCacheRequest handles only one
// request. On a restart new CodeCacheRequest is created.
class ResourceLoader::CodeCacheRequest {
 public:
  CodeCacheRequest(std::unique_ptr<CodeCacheLoader> code_cache_loader,
                   const KURL& url,
                   bool defers_loading)
      : status_(kNoRequestSent),
        code_cache_loader_(std::move(code_cache_loader)),
        gurl_(url),
        defers_loading_(defers_loading),
        weak_ptr_factory_(this) {
    DCHECK(RuntimeEnabledFeatures::IsolatedCodeCacheEnabled());
  }

  ~CodeCacheRequest() = default;

  // Request data from code cache.
  bool FetchFromCodeCache(WebURLLoader* url_loader,
                          ResourceLoader* resource_loader);
  bool FetchFromCodeCacheSynchronously(ResourceLoader* resource_loader);

  // Notifies about the response from webURLLoader. Stores the
  // resource_response_time that is used to validate responses from
  // code cache. Might send cached code if available.
  void DidReceiveResponse(const base::Time& resource_response_time,
                          bool use_isolated_code_cache,
                          ResourceLoader* resource_loader);

  // Stores the value of defers that is needed to restore the state
  // once fetching from code cache is finished. Returns true if the
  // request is handled here and hence need not be handled by the loader.
  // Returns false otherwise.
  bool SetDefersLoading(bool defers);

 private:
  enum CodeCacheRequestStatus {
    kNoRequestSent,
    kPendingResponse,
    kReceivedResponse
  };

  // Callback to receive data from CodeCacheLoader.
  void DidReceiveCachedCode(ResourceLoader* loader,
                            const base::Time& response_time,
                            const std::vector<uint8_t>& data);

  // Process the response from code cache.
  void ProcessCodeCacheResponse(const base::Time& response_time,
                                const std::vector<uint8_t>& data,
                                ResourceLoader* resource_loader);

  // Send |cache_code| if we got a response from code_cache_loader and the
  // web_url_loader.
  void MaybeSendCachedCode(const std::vector<uint8_t>& cached_code,
                           ResourceLoader* resource_loader);

  CodeCacheRequestStatus status_;
  std::unique_ptr<CodeCacheLoader> code_cache_loader_;
  const GURL gurl_;
  bool defers_loading_ = false;
  std::vector<uint8_t> cached_code_;
  base::Time cached_code_response_time_;
  base::Time resource_response_time_;
  bool use_isolated_code_cache_ = false;
  base::WeakPtrFactory<CodeCacheRequest> weak_ptr_factory_;
};

bool ResourceLoader::CodeCacheRequest::FetchFromCodeCache(
    WebURLLoader* url_loader,
    ResourceLoader* resource_loader) {
  if (!code_cache_loader_)
    return false;
  DCHECK_EQ(status_, kNoRequestSent);
  status_ = kPendingResponse;

  // Set defers loading before fetching data from code cache. This is to
  // ensure that the resource receives cached code before the response data.
  // This directly calls the WebURLLoader's SetDefersLoading without going
  // through ResourceLoader.
  url_loader->SetDefersLoading(true);

  CodeCacheLoader::FetchCodeCacheCallback callback =
      base::BindOnce(&ResourceLoader::CodeCacheRequest::DidReceiveCachedCode,
                     weak_ptr_factory_.GetWeakPtr(), resource_loader);
  auto cache_type = resource_loader->GetCodeCacheType();
  code_cache_loader_->FetchFromCodeCache(cache_type, gurl_,
                                         std::move(callback));
  return true;
}

bool ResourceLoader::CodeCacheRequest::FetchFromCodeCacheSynchronously(
    ResourceLoader* resource_loader) {
  if (!code_cache_loader_)
    return false;
  DCHECK_EQ(status_, kNoRequestSent);
  status_ = kPendingResponse;

  base::Time response_time;
  std::vector<uint8_t> data;
  code_cache_loader_->FetchFromCodeCacheSynchronously(gurl_, &response_time,
                                                      &data);
  ProcessCodeCacheResponse(response_time, data, resource_loader);
  return true;
}

// This is called when a response is received from the WebURLLoader. We buffer
// the response_time if the response from code cache is not available yet.
void ResourceLoader::CodeCacheRequest::DidReceiveResponse(
    const base::Time& resource_response_time,
    bool use_isolated_code_cache,
    ResourceLoader* resource_loader) {
  resource_response_time_ = resource_response_time;
  use_isolated_code_cache_ = use_isolated_code_cache;
  MaybeSendCachedCode(cached_code_, resource_loader);
}

// Returns true if |this| handles |defers| and therefore the callsite, i.e. the
// loader, doesn't need to take care of it). Returns false otherwise.
bool ResourceLoader::CodeCacheRequest::SetDefersLoading(bool defers) {
  defers_loading_ = defers;
  if (status_ == kPendingResponse) {
    // The flag doesn't need to be handled by the loader. The value is stored
    // in |defers_loading_| and set once the response from the code cache is
    // received.
    return true;
  }
  return false;
}

void ResourceLoader::CodeCacheRequest::DidReceiveCachedCode(
    ResourceLoader* resource_loader,
    const base::Time& response_time,
    const std::vector<uint8_t>& data) {
  ProcessCodeCacheResponse(response_time, data, resource_loader);
  // Reset the deferred value to its original state.
  DCHECK(resource_loader);
  resource_loader->SetDefersLoading(defers_loading_);
}

// This is called when a response is received from code cache. If the
// resource response time is not available the response is buffered and
// will be processed when the response is received from the URLLoader.
void ResourceLoader::CodeCacheRequest::ProcessCodeCacheResponse(
    const base::Time& response_time,
    const std::vector<uint8_t>& data,
    ResourceLoader* resource_loader) {
  status_ = kReceivedResponse;
  cached_code_response_time_ = response_time;

  if (resource_response_time_.is_null()) {
    // Wait for the response before we can send the cached code.
    // TODO(crbug.com/866889): Pass this as a handle to avoid the overhead of
    // copying this data.
    cached_code_ = data;
    return;
  }

  MaybeSendCachedCode(data, resource_loader);
}

void ResourceLoader::CodeCacheRequest::MaybeSendCachedCode(
    const std::vector<uint8_t>& cached_code,
    ResourceLoader* resource_loader) {
  if (status_ != kReceivedResponse || cached_code_response_time_.is_null() ||
      resource_response_time_.is_null()) {
    return;
  }

  // If the resource was fetched for service worker script or was served from
  // CacheStorage via service worker then they maintain their own code cache.
  // We should not use the isolated cache.
  if (!use_isolated_code_cache_ ||
      resource_response_time_ != cached_code_response_time_) {
    resource_loader->ClearCachedCode();
    return;
  }

  if (cached_code.size() != 0) {
    resource_loader->SendCachedCodeToResource(
        reinterpret_cast<const char*>(cached_code.data()), cached_code.size());
  }
}

ResourceLoader* ResourceLoader::Create(ResourceFetcher* fetcher,
                                       ResourceLoadScheduler* scheduler,
                                       Resource* resource,
                                       uint32_t inflight_keepalive_bytes) {
  return new ResourceLoader(fetcher, scheduler, resource,
                            inflight_keepalive_bytes);
}

ResourceLoader::ResourceLoader(ResourceFetcher* fetcher,
                               ResourceLoadScheduler* scheduler,
                               Resource* resource,
                               uint32_t inflight_keepalive_bytes)
    : scheduler_client_id_(ResourceLoadScheduler::kInvalidClientId),
      fetcher_(fetcher),
      scheduler_(scheduler),
      resource_(resource),
      inflight_keepalive_bytes_(inflight_keepalive_bytes),
      is_cache_aware_loading_activated_(false),
      progress_binding_(this),
      cancel_timer_(Context().GetLoadingTaskRunner(),
                    this,
                    &ResourceLoader::CancelTimerFired) {
  DCHECK(resource_);
  DCHECK(fetcher_);

  resource_->SetLoader(this);
}

ResourceLoader::~ResourceLoader() = default;

void ResourceLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetcher_);
  visitor->Trace(scheduler_);
  visitor->Trace(resource_);
  ResourceLoadSchedulerClient::Trace(visitor);
}

bool ResourceLoader::ShouldFetchCodeCache() {
  if (!RuntimeEnabledFeatures::IsolatedCodeCacheEnabled())
    return false;

  // TODO(crbug.com/867347): Enable fetching of code caches on non-main threads
  // once code cache has its own mojo interface. Currently it is using
  // RenderMessageFilter that is not available on non-main threads.
  if (!IsMainThread())
    return false;

  const ResourceRequest& request = resource_->GetResourceRequest();
  if (!request.Url().ProtocolIsInHTTPFamily())
    return false;
  // When loading the service worker scripts, we don't need to check the
  // GeneratedCodeCache. The code cache corresponding to these scripts is in
  // the service worker's "installed script storage" and would be fetched along
  // with the resource from the cache storage.
  if (request.GetRequestContext() == mojom::RequestContextType::SERVICE_WORKER)
    return false;
  // These requests are serviced by the service worker. It is possible that the
  // service worker may not service the request in which case it is serviced
  // by the network. Assuming those fallback cases are not frequent, we don't
  // fetch from code cache. We may want to have some actual data, to make an
  // informed decision.
  // TODO(crbug.com/895850): Get UMA data to see if this check is necessary.
  if (ResourceLoader::Context().IsControlledByServiceWorker() ==
      mojom::ControllerServiceWorkerMode::kControlled)
    return false;
  if (request.DownloadToBlob())
    return false;
  // Javascript resources have type kScript or kMainResource (for inline
  // scripts). WebAssembly module resources have type kRaw. Note that since we
  // can't easily distinguish WebAssembly modules from other raw resources, we
  // perform a code fetch for all raw resources. These fetches should be cheap,
  // however, requiring one additional IPC and no browser process disk IO since
  // the cache index is in memory and the resource key should not be present.
  return resource_->GetType() == ResourceType::kScript ||
         resource_->GetType() == ResourceType::kMainResource ||
         resource_->GetType() == ResourceType::kRaw;
}

void ResourceLoader::Start() {
  const ResourceRequest& request = resource_->GetResourceRequest();
  ActivateCacheAwareLoadingIfNeeded(request);
  loader_ = Context().CreateURLLoader(request, resource_->Options());
  DCHECK_EQ(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
  auto throttle_option = ResourceLoadScheduler::ThrottleOption::kThrottleable;

  // Synchronous requests should not work with throttling or stopping. Also,
  // disables throttling for the case that can be used for aka long-polling
  // requests, but allows stopping for long-polling requests.
  // Top level frame main resource loads are also not throttleable or
  // stoppable. We also disable throttling and stopping for non-http[s]
  // requests.
  if (resource_->Options().synchronous_policy == kRequestSynchronously ||
      (request.GetFrameType() ==
           network::mojom::RequestContextFrameType::kTopLevel &&
       resource_->GetType() == ResourceType::kMainResource) ||
      !request.Url().ProtocolIsInHTTPFamily()) {
    throttle_option =
        ResourceLoadScheduler::ThrottleOption::kCanNotBeStoppedOrThrottled;
  } else if (!IsThrottlableRequestContext(request.GetRequestContext())) {
    throttle_option = ResourceLoadScheduler::ThrottleOption::kStoppable;
  }

  if (ShouldCheckCORSInResourceLoader()) {
    const auto origin = resource_->GetOrigin();
    response_tainting_ = CORS::CalculateResponseTainting(
        request.Url(), request.GetFetchRequestMode(), origin.get(),
        GetCORSFlag() ? CORSFlag::Set : CORSFlag::Unset);
  }

  if (request.IsAutomaticUpgrade()) {
    LogMixedAutoupgradeStatus(MixedContentAutoupgradeStatus::kStarted);
  }
  if (resource_->GetResourceRequest().IsDownloadToNetworkCacheOnly()) {
    // The download-to-cache requests are throttled in net/, they are fire-and
    // forget, and cannot unregister properly from the scheduler once they are
    // finished.
    throttle_option =
        ResourceLoadScheduler::ThrottleOption::kCanNotBeStoppedOrThrottled;
  }
  scheduler_->Request(this, throttle_option, request.Priority(),
                      request.IntraPriorityValue(), &scheduler_client_id_);
}

void ResourceLoader::Run() {
  StartWith(resource_->GetResourceRequest());
}

void ResourceLoader::StartWith(const ResourceRequest& request) {
  DCHECK_NE(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
  DCHECK(loader_);

  if (resource_->Options().synchronous_policy == kRequestSynchronously &&
      Context().DefersLoading()) {
    Cancel();
    return;
  }

  is_downloading_to_blob_ = request.DownloadToBlob();

  SetDefersLoading(Context().DefersLoading());

  if (ShouldFetchCodeCache()) {
    code_cache_request_ = std::make_unique<CodeCacheRequest>(
        Context().CreateCodeCacheLoader(), request.Url(),
        Context().DefersLoading());
  }

  if (is_cache_aware_loading_activated_) {
    // Override cache policy for cache-aware loading. If this request fails, a
    // reload with original request will be triggered in DidFail().
    ResourceRequest cache_aware_request(request);
    cache_aware_request.SetCacheMode(
        mojom::FetchCacheMode::kUnspecifiedOnlyIfCachedStrict);
    loader_->LoadAsynchronously(WrappedResourceRequest(cache_aware_request),
                                this);
    if (code_cache_request_) {
      // Sets defers loading and initiates a fetch from code cache.
      code_cache_request_->FetchFromCodeCache(loader_.get(), this);
    }
    return;
  }

  if (resource_->Options().synchronous_policy == kRequestSynchronously) {
    RequestSynchronously(request);
  } else {
    loader_->LoadAsynchronously(WrappedResourceRequest(request), this);

    if (code_cache_request_) {
      // Sets defers loading and initiates a fetch from code cache.
      code_cache_request_->FetchFromCodeCache(loader_.get(), this);
    }
  }
}

void ResourceLoader::Release(
    ResourceLoadScheduler::ReleaseOption option,
    const ResourceLoadScheduler::TrafficReportHints& hints) {
  DCHECK_NE(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
  bool released = scheduler_->Release(scheduler_client_id_, option, hints);
  DCHECK(released);
  scheduler_client_id_ = ResourceLoadScheduler::kInvalidClientId;
}

void ResourceLoader::Restart(const ResourceRequest& request) {
  CHECK_EQ(resource_->Options().synchronous_policy, kRequestAsynchronously);
  loader_ = Context().CreateURLLoader(request, resource_->Options());
  StartWith(request);
}

void ResourceLoader::SetDefersLoading(bool defers) {
  DCHECK(loader_);
  // If CodeCacheRequest handles this, then no need to handle here.
  if (code_cache_request_ && code_cache_request_->SetDefersLoading(defers))
    return;

  loader_->SetDefersLoading(defers);
  if (defers) {
    resource_->VirtualTimePauser().UnpauseVirtualTime();
  } else {
    resource_->VirtualTimePauser().PauseVirtualTime();
  }
}

void ResourceLoader::DidChangePriority(ResourceLoadPriority load_priority,
                                       int intra_priority_value) {
  if (scheduler_->IsRunning(scheduler_client_id_)) {
    DCHECK(loader_);
    DCHECK_NE(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
    loader_->DidChangePriority(
        static_cast<WebURLRequest::Priority>(load_priority),
        intra_priority_value);
  } else {
    scheduler_->SetPriority(scheduler_client_id_, load_priority,
                            intra_priority_value);
  }
}

void ResourceLoader::ScheduleCancel() {
  if (!cancel_timer_.IsActive())
    cancel_timer_.StartOneShot(TimeDelta(), FROM_HERE);
}

void ResourceLoader::CancelTimerFired(TimerBase*) {
  if (loader_ && !resource_->HasClientsOrObservers())
    Cancel();
}

void ResourceLoader::Cancel() {
  HandleError(
      ResourceError::CancelledError(resource_->LastResourceRequest().Url()));
}

void ResourceLoader::CancelForRedirectAccessCheckError(
    const KURL& new_url,
    ResourceRequestBlockedReason blocked_reason) {
  resource_->WillNotFollowRedirect();

  if (loader_) {
    HandleError(
        ResourceError::CancelledDueToAccessCheckError(new_url, blocked_reason));
  }
}

static bool IsManualRedirectFetchRequest(const ResourceRequest& request) {
  return request.GetFetchRedirectMode() ==
             network::mojom::FetchRedirectMode::kManual &&
         request.GetRequestContext() == mojom::RequestContextType::FETCH;
}

bool ResourceLoader::WillFollowRedirect(
    const WebURL& new_url,
    const WebURL& new_site_for_cookies,
    const WebString& new_referrer,
    network::mojom::ReferrerPolicy new_referrer_policy,
    const WebString& new_method,
    const WebURLResponse& passed_redirect_response,
    bool& report_raw_headers) {
  DCHECK(!passed_redirect_response.IsNull());

  if (is_cache_aware_loading_activated_) {
    // Fail as cache miss if cached response is a redirect.
    HandleError(
        ResourceError::CacheMissError(resource_->LastResourceRequest().Url()));
    return false;
  }

  std::unique_ptr<ResourceRequest> new_request =
      resource_->LastResourceRequest().CreateRedirectRequest(
          new_url, new_method, new_site_for_cookies, new_referrer,
          static_cast<ReferrerPolicy>(new_referrer_policy),
          !passed_redirect_response.WasFetchedViaServiceWorker());

  ResourceType resource_type = resource_->GetType();

  const ResourceRequest& initial_request = resource_->GetResourceRequest();
  // The following parameters never change during the lifetime of a request.
  mojom::RequestContextType request_context =
      initial_request.GetRequestContext();
  network::mojom::RequestContextFrameType frame_type =
      initial_request.GetFrameType();
  network::mojom::FetchRequestMode fetch_request_mode =
      initial_request.GetFetchRequestMode();
  network::mojom::FetchCredentialsMode fetch_credentials_mode =
      initial_request.GetFetchCredentialsMode();

  const ResourceLoaderOptions& options = resource_->Options();

  const ResourceResponse& redirect_response(
      passed_redirect_response.ToResourceResponse());

  if (!IsManualRedirectFetchRequest(initial_request)) {
    bool unused_preload = resource_->IsUnusedPreload();

    // Don't send security violation reports for unused preloads.
    SecurityViolationReportingPolicy reporting_policy =
        unused_preload ? SecurityViolationReportingPolicy::kSuppressReporting
                       : SecurityViolationReportingPolicy::kReport;

    // CanRequest() checks only enforced CSP, so check report-only here to
    // ensure that violations are sent.
    Context().CheckCSPForRequest(
        request_context, new_url, options, reporting_policy,
        ResourceRequest::RedirectStatus::kFollowedRedirect);

    base::Optional<ResourceRequestBlockedReason> blocked_reason =
        Context().CanRequest(
            resource_type, *new_request, new_url, options, reporting_policy,
            ResourceRequest::RedirectStatus::kFollowedRedirect);

    if (Context().IsAdResource(new_url, resource_type,
                               new_request->GetRequestContext())) {
      new_request->SetIsAdResource();
    }

    if (blocked_reason) {
      CancelForRedirectAccessCheckError(new_url, blocked_reason.value());
      return false;
    }

    if (ShouldCheckCORSInResourceLoader()) {
      scoped_refptr<const SecurityOrigin> origin = resource_->GetOrigin();
      base::Optional<network::CORSErrorStatus> cors_error =
          CORS::CheckRedirectLocation(
              new_url, fetch_request_mode, origin.get(),
              GetCORSFlag() ? CORSFlag::Set : CORSFlag::Unset);
      if (!cors_error && GetCORSFlag()) {
        cors_error =
            CORS::CheckAccess(new_url, redirect_response.HttpStatusCode(),
                              redirect_response.HttpHeaderFields(),
                              fetch_credentials_mode, *origin);
      }
      if (cors_error) {
        HandleError(ResourceError(redirect_response.Url(), *cors_error));
        return false;
      }
      // If |actualResponse|’s location URL’s origin is not same origin with
      // |request|’s current url’s origin and |request|’s origin is not same
      // origin with |request|’s current url’s origin, then set |request|’s
      // tainted origin flag.
      if (origin &&
          !SecurityOrigin::AreSameSchemeHostPort(new_url,
                                                 redirect_response.Url()) &&
          !origin->CanRequest(redirect_response.Url())) {
        origin = SecurityOrigin::CreateUniqueOpaque();
        new_request->SetRequestorOrigin(origin);
      }
    }
    if (resource_type == ResourceType::kImage &&
        fetcher_->ShouldDeferImageLoad(new_url)) {
      CancelForRedirectAccessCheckError(new_url,
                                        ResourceRequestBlockedReason::kOther);
      return false;
    }
  }

  bool cross_origin =
      !SecurityOrigin::AreSameSchemeHostPort(redirect_response.Url(), new_url);
  fetcher_->RecordResourceTimingOnRedirect(resource_.Get(), redirect_response,
                                           cross_origin);

  base::Optional<ResourceResponse> redirect_response_with_type;
  if (ShouldCheckCORSInResourceLoader()) {
    new_request->SetAllowStoredCredentials(CORS::CalculateCredentialsFlag(
        fetch_credentials_mode, response_tainting_));
    if (!redirect_response.WasFetchedViaServiceWorker()) {
      auto response_type = response_tainting_;
      if (initial_request.GetFetchRedirectMode() ==
          network::mojom::FetchRedirectMode::kManual) {
        response_type = network::mojom::FetchResponseType::kOpaqueRedirect;
      }
      if (response_type != redirect_response.GetType()) {
        redirect_response_with_type = redirect_response;
        redirect_response_with_type->SetType(response_type);
      }
    }
  }
  // TODO(yhirano): Remove this once out-of-blink CORS is enabled.
  const ResourceResponse& redirect_response_to_pass =
      redirect_response_with_type ? *redirect_response_with_type
                                  : redirect_response;

  // The following two calls may rewrite the new_request.Url() to
  // something else not for rejecting redirect but for other reasons.
  // E.g. WebFrameTestClient::WillSendRequest() and
  // RenderFrameImpl::WillSendRequest(). We should reflect the
  // rewriting but currently we cannot. So, compare new_request.Url() and
  // new_url after calling them, and return false to make the redirect fail on
  // mismatch.

  Context().PrepareRequest(*new_request,
                           FetchContext::RedirectType::kForRedirect);
  if (Context().GetFrameScheduler()) {
    WebScopedVirtualTimePauser virtual_time_pauser =
        Context().GetFrameScheduler()->CreateWebScopedVirtualTimePauser(
            resource_->Url().GetString(),
            WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);
    virtual_time_pauser.PauseVirtualTime();
    resource_->VirtualTimePauser() = std::move(virtual_time_pauser);
  }
  Context().DispatchWillSendRequest(
      resource_->Identifier(), *new_request, redirect_response_to_pass,
      resource_->GetType(), options.initiator_info);

  // First-party cookie logic moved from DocumentLoader in Blink to
  // net::URLRequest in the browser. Assert that Blink didn't try to change it
  // to something else.
  DCHECK(KURL(new_site_for_cookies) == new_request->SiteForCookies());

  // The following parameters never change during the lifetime of a request.
  DCHECK_EQ(new_request->GetRequestContext(), request_context);
  DCHECK_EQ(new_request->GetFrameType(), frame_type);
  DCHECK_EQ(new_request->GetFetchRequestMode(), fetch_request_mode);
  DCHECK_EQ(new_request->GetFetchCredentialsMode(), fetch_credentials_mode);

  if (new_request->Url() != KURL(new_url)) {
    CancelForRedirectAccessCheckError(new_request->Url(),
                                      ResourceRequestBlockedReason::kOther);
    return false;
  }

  if (!resource_->WillFollowRedirect(*new_request, redirect_response_to_pass)) {
    CancelForRedirectAccessCheckError(new_request->Url(),
                                      ResourceRequestBlockedReason::kOther);
    return false;
  }

  if (ShouldCheckCORSInResourceLoader()) {
    bool new_cors_flag =
        GetCORSFlag() || CORS::CalculateCORSFlag(new_request->Url(),
                                                 resource_->GetOrigin().get(),
                                                 fetch_request_mode);
    resource_->MutableOptions().cors_flag = new_cors_flag;
    // Cross-origin requests are only allowed certain registered schemes.
    if (GetCORSFlag() && !SchemeRegistry::ShouldTreatURLSchemeAsCORSEnabled(
                             new_request->Url().Protocol())) {
      HandleError(
          ResourceError(new_request->Url(),
                        network::CORSErrorStatus(
                            network::mojom::CORSError::kCORSDisabledScheme)));
      return false;
    }
    response_tainting_ = CORS::CalculateResponseTainting(
        new_request->Url(), fetch_request_mode, resource_->GetOrigin().get(),
        GetCORSFlag() ? CORSFlag::Set : CORSFlag::Unset);
  }

  report_raw_headers = new_request->ReportRawHeaders();
  return true;
}

void ResourceLoader::DidReceiveCachedMetadata(const char* data, int length) {
  DCHECK(!should_use_isolated_code_cache_);
  resource_->SetSerializedCachedMetadata(data, length);
}

blink::mojom::CodeCacheType ResourceLoader::GetCodeCacheType() const {
  return Resource::ResourceTypeToCodeCacheType(resource_->GetType());
}

void ResourceLoader::SendCachedCodeToResource(const char* data, int length) {
  resource_->SetSerializedCachedMetadata(data, length);
}

void ResourceLoader::ClearCachedCode() {
  auto cache_type = GetCodeCacheType();
  Platform::Current()->ClearCodeCacheEntry(cache_type, resource_->Url());
}

void ResourceLoader::DidSendData(unsigned long long bytes_sent,
                                 unsigned long long total_bytes_to_be_sent) {
  resource_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

FetchContext& ResourceLoader::Context() const {
  return fetcher_->Context();
}

void ResourceLoader::DidReceiveResponse(
    const WebURLResponse& web_url_response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!web_url_response.IsNull());

  if (resource_->GetResourceRequest().IsAutomaticUpgrade()) {
    LogMixedAutoupgradeStatus(MixedContentAutoupgradeStatus::kResponseReceived);
    LogMixedAutoupgradeResponseOrError(web_url_response.HttpStatusCode());
  }

  if (Context().IsDetached()) {
    // If the fetch context is already detached, we don't need further signals,
    // so let's cancel the request.
    HandleError(ResourceError::CancelledError(web_url_response.Url()));
    return;
  }

  ResourceType resource_type = resource_->GetType();

  const ResourceRequest& initial_request = resource_->GetResourceRequest();
  // The following parameters never change during the lifetime of a request.
  mojom::RequestContextType request_context =
      initial_request.GetRequestContext();
  network::mojom::FetchRequestMode fetch_request_mode =
      initial_request.GetFetchRequestMode();

  const ResourceLoaderOptions& options = resource_->Options();

  const ResourceResponse& response = web_url_response.ToResourceResponse();
  // Service worker script has its own code cache. And also, resources which
  // are served from CacheStorage via service workers have its own code cache.
  // We should not use cached code from site isolated GeneratedCodeCache in such
  // cases.
  should_use_isolated_code_cache_ =
      RuntimeEnabledFeatures::IsolatedCodeCacheEnabled() &&
      !(request_context == mojom::RequestContextType::SERVICE_WORKER ||
        response.WasFetchedViaServiceWorker());

  // Perform 'nosniff' checks against the original response instead of the 304
  // response for a successful revalidation.
  const ResourceResponse& nosniffed_response =
      (resource_->IsCacheValidator() && response.HttpStatusCode() == 304)
          ? resource_->GetResponse()
          : response;
  base::Optional<ResourceRequestBlockedReason> blocked_reason =
      CheckResponseNosniff(request_context, nosniffed_response);
  if (blocked_reason) {
    HandleError(ResourceError::CancelledDueToAccessCheckError(
        response.Url(), blocked_reason.value()));
    return;
  }

  if (response.WasFetchedViaServiceWorker()) {
    if (options.cors_handling_by_resource_fetcher ==
            kEnableCORSHandlingByResourceFetcher &&
        fetch_request_mode == network::mojom::FetchRequestMode::kCORS &&
        response.WasFallbackRequiredByServiceWorker()) {
      ResourceRequest last_request = resource_->LastResourceRequest();
      DCHECK(!last_request.GetSkipServiceWorker());
      // This code handles the case when a controlling service worker doesn't
      // handle a cross origin request.
      if (!Context().ShouldLoadNewResource(resource_type)) {
        // Cancel the request if we should not trigger a reload now.
        HandleError(ResourceError::CancelledError(response.Url()));
        return;
      }
      last_request.SetSkipServiceWorker(true);
      Restart(last_request);
      return;
    }

    // If the response is fetched via ServiceWorker, the original URL of the
    // response could be different from the URL of the request. We check the URL
    // not to load the resources which are forbidden by the page CSP.
    // https://w3c.github.io/webappsec-csp/#should-block-response
    const KURL& original_url = response.OriginalURLViaServiceWorker();
    if (!original_url.IsEmpty()) {
      // CanRequest() below only checks enforced policies: check report-only
      // here to ensure violations are sent.
      Context().CheckCSPForRequest(
          request_context, original_url, options,
          SecurityViolationReportingPolicy::kReport,
          ResourceRequest::RedirectStatus::kFollowedRedirect);

      base::Optional<ResourceRequestBlockedReason> blocked_reason =
          Context().CanRequest(
              resource_type, initial_request, original_url, options,
              SecurityViolationReportingPolicy::kReport,
              ResourceRequest::RedirectStatus::kFollowedRedirect);
      if (blocked_reason) {
        HandleError(ResourceError::CancelledDueToAccessCheckError(
            original_url, blocked_reason.value()));
        return;
      }
    }
  }

  base::Optional<ResourceResponse> response_with_type;
  if (ShouldCheckCORSInResourceLoader() &&
      !response.WasFetchedViaServiceWorker() &&
      !(resource_->IsCacheValidator() && response.HttpStatusCode() == 304)) {
    if (GetCORSFlag()) {
      base::Optional<network::CORSErrorStatus> cors_error = CORS::CheckAccess(
          response.Url(), response.HttpStatusCode(),
          response.HttpHeaderFields(),
          initial_request.GetFetchCredentialsMode(), *resource_->GetOrigin());
      if (cors_error) {
        HandleError(ResourceError(response.Url(), *cors_error));
        return;
      }
    }
    if (response_tainting_ != response.GetType()) {
      response_with_type = response;
      response_with_type->SetType(response_tainting_);
    }
  }
  // TODO(yhirano): Remove this once out-of-blink CORS is enabled.
  const ResourceResponse& response_to_pass =
      response_with_type ? *response_with_type : response;

  // FrameType never changes during the lifetime of a request.
  Context().DispatchDidReceiveResponse(
      resource_->Identifier(), response_to_pass, initial_request.GetFrameType(),
      request_context, resource_,
      FetchContext::ResourceResponseType::kNotFromMemoryCache);

  resource_->ResponseReceived(response_to_pass, std::move(handle));

  // Send the cached code after we notify that the response is received.
  // Resource expects that we receive the response first before the
  // corresponding cached code.
  if (code_cache_request_) {
    code_cache_request_->DidReceiveResponse(
        response.ResponseTime(), should_use_isolated_code_cache_, this);
  }

  if (!resource_->Loader())
    return;

  if (response_to_pass.HttpStatusCode() >= 400 &&
      !resource_->ShouldIgnoreHTTPStatusCodeErrors())
    HandleError(ResourceError::CancelledError(response_to_pass.Url()));
}

void ResourceLoader::DidReceiveResponse(const WebURLResponse& response) {
  DidReceiveResponse(response, nullptr);
}

void ResourceLoader::DidStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(is_downloading_to_blob_);
  DCHECK(!blob_response_started_);
  blob_response_started_ = true;

  const ResourceResponse& response = resource_->GetResponse();
  AtomicString mime_type = response.MimeType();

  mojom::blink::ProgressClientAssociatedPtrInfo progress_client_ptr;
  progress_binding_.Bind(MakeRequest(&progress_client_ptr));

  // Callback is bound to a WeakPersistent, as ResourceLoader is kept alive by
  // ResourceFetcher as long as we still care about the result of the load.
  mojom::blink::BlobRegistry* blob_registry = BlobDataHandle::GetBlobRegistry();
  blob_registry->RegisterFromStream(
      mime_type.IsNull() ? g_empty_string : mime_type.LowerASCII(), "",
      std::max(0ll, response.ExpectedContentLength()), std::move(body),
      std::move(progress_client_ptr),
      WTF::Bind(&ResourceLoader::FinishedCreatingBlob,
                WrapWeakPersistent(this)));
}

void ResourceLoader::DidReceiveData(const char* data, int length) {
  CHECK_GE(length, 0);

  Context().DispatchDidReceiveData(resource_->Identifier(), data, length);
  resource_->AppendData(data, length);
}

void ResourceLoader::DidReceiveTransferSizeUpdate(int transfer_size_diff) {
  DCHECK_GT(transfer_size_diff, 0);
  Context().DispatchDidReceiveEncodedData(resource_->Identifier(),
                                          transfer_size_diff);
}

void ResourceLoader::DidFinishLoadingFirstPartInMultipart() {
  network_instrumentation::EndResourceLoad(
      resource_->Identifier(),
      network_instrumentation::RequestOutcome::kSuccess);

  fetcher_->HandleLoaderFinish(
      resource_.Get(), TimeTicks(),
      ResourceFetcher::kDidFinishFirstPartInMultipart, 0, false,
      std::vector<network::cors::PreflightTimingInfo>());
}

void ResourceLoader::DidFinishLoading(
    TimeTicks finish_time,
    int64_t encoded_data_length,
    int64_t encoded_body_length,
    int64_t decoded_body_length,
    bool should_report_corb_blocking,
    const std::vector<network::cors::PreflightTimingInfo>&
        cors_preflight_timing_info) {
  resource_->SetEncodedDataLength(encoded_data_length);
  resource_->SetEncodedBodyLength(encoded_body_length);
  resource_->SetDecodedBodyLength(decoded_body_length);

  if (is_downloading_to_blob_ && !blob_finished_ && blob_response_started_) {
    load_did_finish_before_blob_ =
        DeferedFinishLoadingInfo{finish_time, should_report_corb_blocking};
    return;
  }

  Release(ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
          ResourceLoadScheduler::TrafficReportHints(encoded_data_length,
                                                    decoded_body_length));
  loader_.reset();
  code_cache_request_.reset();

  network_instrumentation::EndResourceLoad(
      resource_->Identifier(),
      network_instrumentation::RequestOutcome::kSuccess);

  fetcher_->HandleLoaderFinish(
      resource_.Get(), finish_time, ResourceFetcher::kDidFinishLoading,
      inflight_keepalive_bytes_, should_report_corb_blocking,
      cors_preflight_timing_info);
}

void ResourceLoader::DidFail(const WebURLError& error,
                             int64_t encoded_data_length,
                             int64_t encoded_body_length,
                             int64_t decoded_body_length) {
  if (resource_->GetResourceRequest().IsAutomaticUpgrade()) {
    LogMixedAutoupgradeStatus(MixedContentAutoupgradeStatus::kFailed);
    LogMixedAutoupgradeResponseOrError(error.reason());
  }
  resource_->SetEncodedDataLength(encoded_data_length);
  resource_->SetEncodedBodyLength(encoded_body_length);
  resource_->SetDecodedBodyLength(decoded_body_length);
  HandleError(error);
}

void ResourceLoader::HandleError(const ResourceError& error) {
  if (is_cache_aware_loading_activated_ && error.IsCacheMiss() &&
      Context().ShouldLoadNewResource(resource_->GetType())) {
    resource_->WillReloadAfterDiskCacheMiss();
    is_cache_aware_loading_activated_ = false;
    Restart(resource_->GetResourceRequest());
    return;
  }
  if (error.CORSErrorStatus()) {
    Context().AddErrorConsoleMessage(
        CORS::GetErrorString(
            *error.CORSErrorStatus(), resource_->GetResourceRequest().Url(),
            resource_->LastResourceRequest().Url(), *resource_->GetOrigin(),
            resource_->GetType(), resource_->Options().initiator_info.name),
        FetchContext::kJSSource);
  }

  Release(ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
          ResourceLoadScheduler::TrafficReportHints::InvalidInstance());
  loader_.reset();
  code_cache_request_.reset();

  network_instrumentation::EndResourceLoad(
      resource_->Identifier(), network_instrumentation::RequestOutcome::kFail);

  fetcher_->HandleLoaderError(resource_.Get(), error,
                              inflight_keepalive_bytes_);
}

void ResourceLoader::RequestSynchronously(const ResourceRequest& request) {
  DCHECK(loader_);
  DCHECK_EQ(request.Priority(), ResourceLoadPriority::kHighest);

  WrappedResourceRequest request_in(request);
  WebURLResponse response_out;
  base::Optional<WebURLError> error_out;
  WebData data_out;
  int64_t encoded_data_length = WebURLLoaderClient::kUnknownEncodedDataLength;
  int64_t encoded_body_length = 0;
  WebBlobInfo downloaded_blob;
  loader_->LoadSynchronously(request_in, this, response_out, error_out,
                             data_out, encoded_data_length, encoded_body_length,
                             downloaded_blob);

  // A message dispatched while synchronously fetching the resource
  // can bring about the cancellation of this load.
  if (!loader_)
    return;
  int64_t decoded_body_length = data_out.size();
  if (error_out) {
    DidFail(*error_out, encoded_data_length, encoded_body_length,
            decoded_body_length);
    return;
  }
  DidReceiveResponse(response_out);
  if (!loader_)
    return;
  DCHECK_GE(response_out.ToResourceResponse().EncodedBodyLength(), 0);

  // TODO(crbug.com/867347): Enable fetching of code caches synchronously
  // once code cache has its own mojo interface. Currently it is using
  // RenderMessageFilter that is not available on non-main threads.

  // Follow the async case convention of not calling DidReceiveData or
  // appending data to m_resource if the response body is empty. Copying the
  // empty buffer is a noop in most cases, but is destructive in the case of
  // a 304, where it will overwrite the cached data we should be reusing.
  if (data_out.size()) {
    data_out.ForEachSegment([this](const char* segment, size_t segment_size,
                                   size_t segment_offset) {
      DidReceiveData(segment, segment_size);
      return true;
    });
  }

  if (request.DownloadToBlob()) {
    auto blob = downloaded_blob.GetBlobHandle();
    if (blob)
      OnProgress(blob->size());
    FinishedCreatingBlob(blob);
  }
  DidFinishLoading(CurrentTimeTicks(), encoded_data_length, encoded_body_length,
                   decoded_body_length, false,
                   std::vector<network::cors::PreflightTimingInfo>());
}

void ResourceLoader::Dispose() {
  loader_ = nullptr;
  progress_binding_.Close();
  code_cache_request_.reset();

  // Release() should be called to release |scheduler_client_id_| beforehand in
  // DidFinishLoading() or DidFail(), but when a timer to call Cancel() is
  // ignored due to GC, this case happens. We just release here because we can
  // not schedule another request safely. See crbug.com/675947.
  if (scheduler_client_id_ != ResourceLoadScheduler::kInvalidClientId) {
    Release(ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
            ResourceLoadScheduler::TrafficReportHints::InvalidInstance());
  }
}

void ResourceLoader::ActivateCacheAwareLoadingIfNeeded(
    const ResourceRequest& request) {
  DCHECK(!is_cache_aware_loading_activated_);

  if (resource_->Options().cache_aware_loading_enabled !=
      kIsCacheAwareLoadingEnabled)
    return;

  // Synchronous requests are not supported.
  if (resource_->Options().synchronous_policy == kRequestSynchronously)
    return;

  // Don't activate on Resource revalidation.
  if (resource_->IsCacheValidator())
    return;

  // Don't activate if cache policy is explicitly set.
  if (request.GetCacheMode() != mojom::FetchCacheMode::kDefault)
    return;

  // Don't activate if the page is controlled by service worker.
  if (fetcher_->IsControlledByServiceWorker() !=
      blink::mojom::ControllerServiceWorkerMode::kNoController) {
    return;
  }

  is_cache_aware_loading_activated_ = true;
}

bool ResourceLoader::ShouldBeKeptAliveWhenDetached() const {
  return resource_->GetResourceRequest().GetKeepalive() &&
         resource_->GetResponse().IsNull();
}

scoped_refptr<base::SingleThreadTaskRunner>
ResourceLoader::GetLoadingTaskRunner() {
  return Context().GetLoadingTaskRunner();
}

void ResourceLoader::OnProgress(uint64_t delta) {
  DCHECK(!blob_finished_);

  if (scheduler_client_id_ == ResourceLoadScheduler::kInvalidClientId)
    return;

  Context().DispatchDidReceiveData(resource_->Identifier(), nullptr, delta);
  resource_->DidDownloadData(delta);
}

void ResourceLoader::FinishedCreatingBlob(
    const scoped_refptr<BlobDataHandle>& blob) {
  DCHECK(!blob_finished_);

  if (scheduler_client_id_ == ResourceLoadScheduler::kInvalidClientId)
    return;

  Context().DispatchDidDownloadToBlob(resource_->Identifier(), blob.get());
  resource_->DidDownloadToBlob(blob);

  blob_finished_ = true;
  if (load_did_finish_before_blob_) {
    const ResourceResponse& response = resource_->GetResponse();
    DidFinishLoading(load_did_finish_before_blob_->finish_time,
                     response.EncodedDataLength(), response.EncodedBodyLength(),
                     response.DecodedBodyLength(),
                     load_did_finish_before_blob_->should_report_corb_blocking,
                     std::vector<network::cors::PreflightTimingInfo>());
  }
}

base::Optional<ResourceRequestBlockedReason>
ResourceLoader::CheckResponseNosniff(mojom::RequestContextType request_context,
                                     const ResourceResponse& response) const {
  bool sniffing_allowed =
      ParseContentTypeOptionsHeader(response.HttpHeaderField(
          HTTPNames::X_Content_Type_Options)) != kContentTypeOptionsNosniff;
  if (sniffing_allowed)
    return base::nullopt;

  String mime_type = response.HttpContentType();
  if (request_context == mojom::RequestContextType::STYLE &&
      !MIMETypeRegistry::IsSupportedStyleSheetMIMEType(mime_type)) {
    Context().AddErrorConsoleMessage(
        "Refused to apply style from '" + response.Url().ElidedString() +
            "' because its MIME type ('" + mime_type + "') " +
            "is not a supported stylesheet MIME type, and strict MIME checking "
            "is enabled.",
        FetchContext::kSecuritySource);
    return ResourceRequestBlockedReason::kContentType;
  }
  // TODO(mkwst): Move the 'nosniff' bit of 'AllowedByNosniff::MimeTypeAsScript'
  // here alongside the style checks, and put its use counters somewhere else.

  return base::nullopt;
}

bool ResourceLoader::ShouldCheckCORSInResourceLoader() const {
  return !RuntimeEnabledFeatures::OutOfBlinkCORSEnabled() &&
         resource_->Options().cors_handling_by_resource_fetcher ==
             kEnableCORSHandlingByResourceFetcher;
}

}  // namespace blink
