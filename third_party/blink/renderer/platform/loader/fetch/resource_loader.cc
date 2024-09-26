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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_flags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/back_forward_cache_utils.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/cors/cors_error_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/detachable_use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/shared_buffer_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/background_response_processor.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/request_conversion.h"
#include "third_party/blink/renderer/platform/loader/mixed_content_autoupgrade_status.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "url/url_constants.h"

namespace blink {

namespace {

enum class RequestOutcome { kSuccess, kFail };

const char* RequestOutcomeToString(RequestOutcome outcome) {
  switch (outcome) {
    case RequestOutcome::kSuccess:
      return "Success";
    case RequestOutcome::kFail:
      return "Fail";
  }
}

// The sampling rate for UKM recording. A value of 0.1 corresponds to a
// sampling rate of 10%.
constexpr double kUkmSamplingRate = 0.1;

bool IsThrottlableRequestContext(mojom::blink::RequestContextType context) {
  // Requests that could run long should not be throttled as they
  // may stay there forever and avoid other requests from making
  // progress.
  // See https://crbug.com/837771 for the sample breakages.
  return context != mojom::blink::RequestContextType::EVENT_SOURCE &&
         context != mojom::blink::RequestContextType::FETCH &&
         context != mojom::blink::RequestContextType::XML_HTTP_REQUEST &&
         context != mojom::blink::RequestContextType::VIDEO &&
         context != mojom::blink::RequestContextType::AUDIO;
}

void LogMixedAutoupgradeMetrics(blink::MixedContentAutoupgradeStatus status,
                                std::optional<int> response_or_error_code,
                                ukm::SourceId source_id,
                                ukm::UkmRecorder* recorder,
                                Resource* resource) {
  UMA_HISTOGRAM_ENUMERATION("MixedAutoupgrade.ResourceRequest.Status", status);
  switch (status) {
    case MixedContentAutoupgradeStatus::kStarted:
      UMA_HISTOGRAM_ENUMERATION("MixedAutoupgrade.ResourceRequest.Start.Type",
                                resource->GetType());
      break;
    case MixedContentAutoupgradeStatus::kFailed:
      UMA_HISTOGRAM_ENUMERATION("MixedAutoupgrade.ResourceRequest.Failure.Type",
                                resource->GetType());
      UMA_HISTOGRAM_BOOLEAN("MixedAutoupgrade.ResourceRequest.Failure.IsAd",
                            resource->GetResourceRequest().IsAdResource());
      break;
    case MixedContentAutoupgradeStatus::kResponseReceived:
      UMA_HISTOGRAM_ENUMERATION(
          "MixedAutoupgrade.ResourceRequest.Response.Type",
          resource->GetType());
  };
  ukm::builders::MixedContentAutoupgrade_ResourceRequest builder(source_id);
  builder.SetStatus(static_cast<int64_t>(status));
  if (response_or_error_code.has_value()) {
    base::UmaHistogramSparse(
        "MixedAutoupgrade.ResourceRequest.ErrorOrResponseCode",
        response_or_error_code.value());
    builder.SetCode(response_or_error_code.value());
  }
  builder.Record(recorder);
}

bool RequestContextObserveResponse(mojom::blink::RequestContextType type) {
  switch (type) {
    case mojom::blink::RequestContextType::PING:
    case mojom::blink::RequestContextType::BEACON:
    case mojom::blink::RequestContextType::CSP_REPORT:
      return true;

    default:
      return false;
  }
}

SchedulingPolicy::Feature GetFeatureFromRequestContextType(
    mojom::blink::RequestContextType type) {
  switch (type) {
    case mojom::blink::RequestContextType::FETCH:
      return SchedulingPolicy::Feature::kOutstandingNetworkRequestFetch;
    case mojom::blink::RequestContextType::XML_HTTP_REQUEST:
      return SchedulingPolicy::Feature::kOutstandingNetworkRequestXHR;
    default:
      return SchedulingPolicy::Feature::kOutstandingNetworkRequestOthers;
  }
}

std::optional<mojom::WebFeature> PreflightResultToWebFeature(
    network::mojom::PrivateNetworkAccessPreflightResult result) {
  using Result = network::mojom::PrivateNetworkAccessPreflightResult;

  switch (result) {
    case Result::kNone:
      return std::nullopt;
    case Result::kError:
      return mojom::WebFeature::kPrivateNetworkAccessPreflightError;
    case Result::kSuccess:
      return mojom::WebFeature::kPrivateNetworkAccessPreflightSuccess;
    case Result::kWarning:
      return mojom::WebFeature::kPrivateNetworkAccessPreflightWarning;
  }
}

bool ShouldActivateCacheAwareLoading(const ResourceFetcher* fetcher,
                                     const Resource* resource) {
  if (resource->Options().cache_aware_loading_enabled !=
      kIsCacheAwareLoadingEnabled) {
    return false;
  }

  // Synchronous requests are not supported.
  if (resource->Options().synchronous_policy == kRequestSynchronously) {
    return false;
  }

  // Don't activate on Resource revalidation.
  if (resource->IsCacheValidator()) {
    return false;
  }

  // Don't activate if cache policy is explicitly set.
  if (resource->GetResourceRequest().GetCacheMode() !=
      mojom::blink::FetchCacheMode::kDefault) {
    return false;
  }

  // Don't activate if the page is controlled by service worker.
  if (fetcher->IsControlledByServiceWorker() !=
      mojom::blink::ControllerServiceWorkerMode::kNoController) {
    return false;
  }
  return true;
}

std::unique_ptr<network::ResourceRequest> CreateNetworkRequest(
    const ResourceRequestHead& request_head,
    ResourceRequestBody& request_body) {
  auto network_resource_request = std::make_unique<network::ResourceRequest>();
  scoped_refptr<EncodedFormData> form_body = request_body.FormBody();
  PopulateResourceRequest(request_head, std::move(request_body),
                          network_resource_request.get());
  if (form_body) {
    request_body = ResourceRequestBody(std::move(form_body));
  }
  return network_resource_request;
}

}  // namespace

ResourceLoader::ResourceLoader(ResourceFetcher* fetcher,
                               ResourceLoadScheduler* scheduler,
                               Resource* resource,
                               ContextLifecycleNotifier* context,
                               ResourceRequestBody request_body,
                               uint32_t inflight_keepalive_bytes)
    : scheduler_client_id_(ResourceLoadScheduler::kInvalidClientId),
      fetcher_(fetcher),
      scheduler_(scheduler),
      resource_(resource),
      request_body_(std::move(request_body)),
      inflight_keepalive_bytes_(inflight_keepalive_bytes),
      is_cache_aware_loading_activated_(
          ShouldActivateCacheAwareLoading(fetcher, resource)),
      progress_receiver_(this, context),
      cancel_timer_(fetcher_->GetTaskRunner(),
                    this,
                    &ResourceLoader::CancelTimerFired) {
  DCHECK(resource_);
  DCHECK(fetcher_);

  // Some requests should not block the page from entering the BackForwardCache.
  // If they are keepalive request && their responses are not observable to web
  // content, we can have them survive without breaking web content when the
  // page is put into BackForwardCache.
  const auto& request = resource_->GetResourceRequest();
  auto request_context = request.GetRequestContext();
  if (auto* frame_or_worker_scheduler = fetcher->GetFrameOrWorkerScheduler()) {
    if (!base::FeatureList::IsEnabled(
            features::kBackForwardCacheWithKeepaliveRequest) &&
        request.GetKeepalive()) {
      frame_or_worker_scheduler->RegisterStickyFeature(
          SchedulingPolicy::Feature::kKeepaliveRequest,
          {SchedulingPolicy::DisableBackForwardCache()});
    } else if (!RequestContextObserveResponse(request_context)) {
      // Only when this feature is turned on and the loading tasks keep being
      // processed and the data is queued up on the renderer, a page can stay in
      // BackForwardCache with network requests.
      if (!IsInflightNetworkRequestBackForwardCacheSupportEnabled()) {
        feature_handle_for_scheduler_ =
            frame_or_worker_scheduler->RegisterFeature(
                GetFeatureFromRequestContextType(request_context),
                {SchedulingPolicy::DisableBackForwardCache()});
      }
    }
  }

  resource_->SetLoader(this);
}

ResourceLoader::~ResourceLoader() = default;

void ResourceLoader::Trace(Visitor* visitor) const {
  visitor->Trace(fetcher_);
  visitor->Trace(scheduler_);
  visitor->Trace(resource_);
  visitor->Trace(response_body_loader_);
  visitor->Trace(data_pipe_completion_notifier_);
  visitor->Trace(cancel_timer_);
  visitor->Trace(progress_receiver_);
  ResourceLoadSchedulerClient::Trace(visitor);
}

void ResourceLoader::Start() {
  const ResourceRequestHead& request = resource_->GetResourceRequest();

  if (request.GetKeepalive()) {
    FetchUtils::LogFetchKeepAliveRequestMetric(
        request.GetRequestContext(),
        FetchUtils::FetchKeepAliveRequestState::kStarted,
        fetcher_->GetProperties().IsDetached());
  }

  if (!resource_->Url().ProtocolIsData()) {
    network_resource_request_ = CreateNetworkRequest(request, request_body_);
    if (is_cache_aware_loading_activated_) {
      // Override cache policy for cache-aware loading. If this request fails, a
      // reload with original request will be triggered in DidFail().
      network_resource_request_->load_flags |= net::LOAD_ONLY_FROM_CACHE;
    }
    loader_ = fetcher_->CreateURLLoader(
        *network_resource_request_, resource_->Options(),
        resource_->GetResourceRequest().GetRequestContext(),
        resource_->GetResourceRequest().GetRenderBlockingBehavior(),
        resource_->GetResourceRequest()
            .GetServiceWorkerRaceNetworkRequestToken(),
        resource_->GetResourceRequest().IsFromOriginDirtyStyleSheet());
    task_runner_for_body_loader_ = loader_->GetTaskRunnerForBodyLoader();
  } else {
    // ResourceLoader doesn't support DownloadToBlob option for data URL. This
    // logic is implemented inside XMLHttpRequest.
    CHECK(!resource_->GetResourceRequest().DownloadToBlob());
    task_runner_for_body_loader_ = fetcher_->GetTaskRunner();
  }

  DCHECK_EQ(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
  auto throttle_option = ResourceLoadScheduler::ThrottleOption::kThrottleable;

  // Synchronous requests should not work with throttling or stopping. Also,
  // disables throttling for the case that can be used for aka long-polling
  // requests, but allows stopping for long-polling requests. We don't want
  // to throttle a request with keepalive set because such a request is
  // expected to work even when a frame is freezed/detached.
  // Top level frame main resource loads are also not throttleable or
  // stoppable. We also disable throttling and stopping for non-http[s]
  // requests.
  if (resource_->Options().synchronous_policy == kRequestSynchronously ||
      request.GetKeepalive() || !request.Url().ProtocolIsInHTTPFamily()) {
    throttle_option =
        ResourceLoadScheduler::ThrottleOption::kCanNotBeStoppedOrThrottled;
  } else if (!IsThrottlableRequestContext(request.GetRequestContext())) {
    throttle_option = ResourceLoadScheduler::ThrottleOption::kStoppable;
  }

  if (request.IsAutomaticUpgrade()) {
    LogMixedAutoupgradeMetrics(MixedContentAutoupgradeStatus::kStarted,
                               std::nullopt, request.GetUkmSourceId(),
                               fetcher_->UkmRecorder(), resource_);
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

void ResourceLoader::DidStartLoadingResponseBodyInternal(
    BytesConsumer& bytes_consumer) {
  DCHECK(!response_body_loader_);
  ResponseBodyLoaderClient& response_body_loader_client = *this;
  response_body_loader_ = MakeGarbageCollected<ResponseBodyLoader>(
      bytes_consumer, response_body_loader_client, task_runner_for_body_loader_,
      fetcher_->GetBackForwardCacheLoaderHelper());
  resource_->ResponseBodyReceived(*response_body_loader_,
                                  task_runner_for_body_loader_);
  if (response_body_loader_->IsDrained()) {
    // When streaming, unpause virtual time early to prevent deadlocking
    // against stream consumer in case stream has backpressure enabled.
    resource_->VirtualTimePauser().UnpauseVirtualTime();
  } else {
    response_body_loader_->Start();
  }
}

void ResourceLoader::Run() {
  // TODO(crbug.com/1169032): Manage cookies' capability control here for the
  // Prerender2.
  StartFetch();
}

void ResourceLoader::DidReceiveDecodedData(
    const String& data,
    std::unique_ptr<ParkableStringImpl::SecureDigest> digest) {
  resource_->DidReceiveDecodedData(data, std::move(digest));
}

void ResourceLoader::DidFinishLoadingBody() {
  has_seen_end_of_body_ = true;

  const ResourceResponse& response = resource_->GetResponse();
  if (deferred_finish_loading_info_) {
    DidFinishLoading(deferred_finish_loading_info_->response_end_time,
                     response.EncodedDataLength(), response.EncodedBodyLength(),
                     response.DecodedBodyLength());
  }
}

void ResourceLoader::DidFailLoadingBody() {
  DidFail(WebURLError(ResourceError::Failure(resource_->Url())),
          base::TimeTicks::Now(), 0, 0, 0);
}

void ResourceLoader::DidCancelLoadingBody() {
  Cancel();
}

void ResourceLoader::StartFetch() {
  DCHECK_NE(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
  if (resource_->Options().synchronous_policy == kRequestSynchronously &&
      fetcher_->GetProperties().FreezeMode() != LoaderFreezeMode::kNone) {
    // TODO(yuzus): Evict bfcache if necessary.
    Cancel();
    return;
  }

  SetDefersLoading(fetcher_->GetProperties().FreezeMode());

  if (resource_->Options().synchronous_policy == kRequestSynchronously) {
    RequestSynchronously();
  } else {
    RequestAsynchronously();
  }
}

void ResourceLoader::Release(
    ResourceLoadScheduler::ReleaseOption option,
    const ResourceLoadScheduler::TrafficReportHints& hints) {
  DCHECK_NE(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
  bool released = scheduler_->Release(scheduler_client_id_, option, hints);
  DCHECK(released);
  scheduler_client_id_ = ResourceLoadScheduler::kInvalidClientId;
  feature_handle_for_scheduler_.reset();
}

void ResourceLoader::Restart() {
  const ResourceRequestHead& request = resource_->GetResourceRequest();
  CHECK_EQ(resource_->Options().synchronous_policy, kRequestAsynchronously);
  CHECK(!network_resource_request_);
  CHECK(!resource_->Url().ProtocolIsData());
  network_resource_request_ = CreateNetworkRequest(request, request_body_);
  loader_ = fetcher_->CreateURLLoader(
      *network_resource_request_, resource_->Options(),
      resource_->GetResourceRequest().GetRequestContext(),
      resource_->GetResourceRequest().GetRenderBlockingBehavior(),
      resource_->GetResourceRequest().GetServiceWorkerRaceNetworkRequestToken(),
      resource_->GetResourceRequest().IsFromOriginDirtyStyleSheet());
  task_runner_for_body_loader_ = loader_->GetTaskRunnerForBodyLoader();
  StartFetch();
}

void ResourceLoader::SetDefersLoading(LoaderFreezeMode mode) {
  freeze_mode_ = mode;

  if (response_body_loader_) {
    if (mode != LoaderFreezeMode::kNone &&
        !response_body_loader_->IsSuspended()) {
      response_body_loader_->Suspend(mode);
      if (mode == LoaderFreezeMode::kBufferIncoming) {
        response_body_loader_
            ->EvictFromBackForwardCacheIfDrainedAsBytesConsumer();
      }
    }
    if (mode == LoaderFreezeMode::kNone &&
        response_body_loader_->IsSuspended()) {
      response_body_loader_->Resume();
    }
  }

  if (defers_handling_data_url_) {
    if (freeze_mode_ == LoaderFreezeMode::kNone) {
      defers_handling_data_url_ = false;
      GetLoadingTaskRunner()->PostTask(
          FROM_HERE, WTF::BindOnce(&ResourceLoader::HandleDataUrl,
                                   WrapWeakPersistent(this)));
    }
  }

  if (loader_) {
    loader_->Freeze(mode);
  }
  if (mode != LoaderFreezeMode::kNone) {
    resource_->VirtualTimePauser().UnpauseVirtualTime();
  } else {
    resource_->VirtualTimePauser().PauseVirtualTime();
  }
}

void ResourceLoader::DidChangePriority(ResourceLoadPriority load_priority,
                                       int intra_priority_value) {
  if (scheduler_->IsRunning(scheduler_client_id_)) {
    DCHECK_NE(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
    if (loader_) {
      loader_->DidChangePriority(
          static_cast<WebURLRequest::Priority>(load_priority),
          intra_priority_value);
    }
  } else {
    scheduler_->SetPriority(scheduler_client_id_, load_priority,
                            intra_priority_value);
  }
}

void ResourceLoader::ScheduleCancel() {
  if (!cancel_timer_.IsActive()) {
    cancel_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
  }
}

void ResourceLoader::CancelTimerFired(TimerBase*) {
  if (IsLoading() && !resource_->HasClientsOrObservers()) {
    Cancel();
  }
}

void ResourceLoader::Cancel() {
  HandleError(
      ResourceError::CancelledError(resource_->LastResourceRequest().Url()));
}

bool ResourceLoader::IsLoading() const {
  return !finished_;
}

void ResourceLoader::CancelForRedirectAccessCheckError(
    const KURL& new_url,
    ResourceRequestBlockedReason blocked_reason) {
  resource_->WillNotFollowRedirect();

  if (IsLoading()) {
    HandleError(
        ResourceError::CancelledDueToAccessCheckError(new_url, blocked_reason));
  }
}

static bool IsManualRedirectFetchRequest(const ResourceRequestHead& request) {
  return request.GetRedirectMode() == network::mojom::RedirectMode::kManual &&
         request.GetRequestContext() == mojom::blink::RequestContextType::FETCH;
}

bool ResourceLoader::WillFollowRedirect(
    const WebURL& new_url,
    const net::SiteForCookies& new_site_for_cookies,
    const WebString& new_referrer,
    network::mojom::ReferrerPolicy new_referrer_policy,
    const WebString& new_method,
    const WebURLResponse& passed_redirect_response,
    bool& has_devtools_request_id,
    std::vector<std::string>* removed_headers,
    net::HttpRequestHeaders& modified_headers,
    bool insecure_scheme_was_upgraded) {
  DCHECK(!passed_redirect_response.IsNull());

  if (passed_redirect_response.HasAuthorizationCoveredByWildcardOnPreflight()) {
    fetcher_->GetUseCounter().CountDeprecation(
        mojom::WebFeature::kAuthorizationCoveredByWildcard);
  }

  CountPrivateNetworkAccessPreflightResult(
      passed_redirect_response.PrivateNetworkAccessPreflightResult());

  if (resource_->GetResourceRequest().HttpHeaderFields().Contains(
          http_names::kAuthorization) &&
      !SecurityOrigin::AreSameOrigin(resource_->LastResourceRequest().Url(),
                                     new_url)) {
    fetcher_->GetUseCounter().CountUse(
        mojom::WebFeature::kAuthorizationCrossOrigin);
  }

  // TODO(https://crbug.com/471397, https://crbug.com/1406737): Reconsider
  // the placement of this code, together with the //net counterpart.
  if (removed_headers) {
    // Step 13 of https://fetch.spec.whatwg.org/#http-redirect-fetch
    if (base::FeatureList::IsEnabled(
            features::kRemoveAuthroizationOnCrossOriginRedirect) &&
        !SecurityOrigin::AreSameOrigin(resource_->LastResourceRequest().Url(),
                                       new_url)) {
      removed_headers->push_back(net::HttpRequestHeaders::kAuthorization);
    }
    FindClientHintsToRemove(Context().GetPermissionsPolicy(),
                            GURL(new_url.GetString().Utf8()), removed_headers);
  }

  if (is_cache_aware_loading_activated_) {
    // Fail as cache miss if cached response is a redirect.
    HandleError(
        ResourceError::CacheMissError(resource_->LastResourceRequest().Url()));
    return false;
  }

  const ResourceRequestHead& initial_request = resource_->GetResourceRequest();
  if (initial_request.GetRedirectMode() ==
      network::mojom::RedirectMode::kError) {
    // The network::cors::CorsURLLoader would reject the redirect in any case,
    // but we reject the redirect here because otherwise we would see confusing
    // errors such as MixedContent errors in the console during redirect
    // handling.
    HandleError(ResourceError::Failure(new_url));
    return false;
  }

  std::unique_ptr<ResourceRequest> new_request =
      resource_->LastResourceRequest().CreateRedirectRequest(
          new_url, new_method, new_site_for_cookies, new_referrer,
          new_referrer_policy,
          !passed_redirect_response.WasFetchedViaServiceWorker());

  ResourceType resource_type = resource_->GetType();

  // The following parameters never change during the lifetime of a request.
  mojom::blink::RequestContextType request_context =
      initial_request.GetRequestContext();
  network::mojom::RequestDestination request_destination =
      initial_request.GetRequestDestination();
  network::mojom::RequestMode request_mode = initial_request.GetMode();
  network::mojom::CredentialsMode credentials_mode =
      initial_request.GetCredentialsMode();

  const ResourceLoaderOptions& options = resource_->Options();

  const ResourceResponse& redirect_response(
      passed_redirect_response.ToResourceResponse());

  const KURL& url_before_redirects = initial_request.Url();

  if (!IsManualRedirectFetchRequest(initial_request)) {
    bool unused_preload = resource_->IsUnusedPreload();

    // Don't send security violation reports for unused preloads.
    ReportingDisposition reporting_disposition =
        unused_preload ? ReportingDisposition::kSuppressReporting
                       : ReportingDisposition::kReport;

    // The network stack might have upgraded to https an http URL. Report-only
    // CSP must be checked with the url prior to that upgrade.
    KURL new_url_prior_upgrade = new_url;
    if (insecure_scheme_was_upgraded && new_url.ProtocolIs(url::kHttpsScheme)) {
      new_url_prior_upgrade.SetProtocol(url::kHttpScheme);
    }

    // CanRequest() checks only enforced CSP, so check report-only here to
    // ensure that violations are sent.
    Context().CheckCSPForRequest(
        request_context, request_destination, new_url_prior_upgrade, options,
        reporting_disposition, url_before_redirects,
        ResourceRequest::RedirectStatus::kFollowedRedirect);

    std::optional<ResourceRequestBlockedReason> blocked_reason =
        Context().CanRequest(resource_type, *new_request, new_url, options,
                             reporting_disposition,
                             new_request->GetRedirectInfo());

    if (Context().CalculateIfAdSubresource(
            *new_request, std::nullopt /* alias_url */, resource_type,
            options.initiator_info)) {
      new_request->SetIsAdResource();
    }

    if (blocked_reason) {
      CancelForRedirectAccessCheckError(new_url, blocked_reason.value());
      return false;
    }

    if (resource_type == ResourceType::kImage &&
        fetcher_->ShouldDeferImageLoad(new_url)) {
      CancelForRedirectAccessCheckError(new_url,
                                        ResourceRequestBlockedReason::kOther);
      return false;
    }
  }

  fetcher_->RecordResourceTimingOnRedirect(resource_.Get(), redirect_response,
                                           new_url);

  // The following two calls may rewrite the new_request->Url() to
  // something else not for rejecting redirect but for other reasons.
  // E.g. WebFrameTestClient::WillSendRequest() and
  // RenderFrameImpl::WillSendRequest(). We should reflect the
  // rewriting but currently we cannot. So, compare new_request->Url() and
  // new_url after calling them, and return false to make the redirect fail on
  // mismatch.

  WebScopedVirtualTimePauser unused_virtual_time_pauser;
  // TODO(yoichio): Have PrepareRequest use ResourceRequestHead.
  Context().PrepareRequest(*new_request, resource_->MutableOptions(),
                           unused_virtual_time_pauser, resource_->GetType());
  DCHECK(!new_request->HttpBody());
  if (auto* observer = fetcher_->GetResourceLoadObserver()) {
    observer->WillSendRequest(
        *new_request, redirect_response, resource_->GetType(), options,
        initial_request.GetRenderBlockingBehavior(), resource_);
  }

  // First-party cookie logic moved from DocumentLoader in Blink to
  // net::URLRequest in the browser. Assert that Blink didn't try to change it
  // to something else.
  DCHECK(new_request->SiteForCookies().IsEquivalent(new_site_for_cookies));

  // The following parameters never change during the lifetime of a request.
  DCHECK_EQ(new_request->GetRequestContext(), request_context);
  DCHECK_EQ(new_request->GetMode(), request_mode);
  DCHECK_EQ(new_request->GetCredentialsMode(), credentials_mode);

  // If `Shared-Storage-Writable` eligibity has changed, update the headers.
  bool previous_shared_storage_writable_eligible =
      resource_->LastResourceRequest().GetSharedStorageWritableEligible();
  bool new_shared_storage_writable_eligible =
      new_request->GetSharedStorageWritableEligible();
  if (new_shared_storage_writable_eligible !=
      previous_shared_storage_writable_eligible) {
    if (new_shared_storage_writable_eligible) {
      CHECK(new_request->GetSharedStorageWritableOptedIn());
      modified_headers.SetHeader(http_names::kSecSharedStorageWritable.Ascii(),
                                 "?1");
    } else if (removed_headers) {
      removed_headers->push_back(http_names::kSecSharedStorageWritable.Ascii());
    }
  }

  if (new_request->Url() != KURL(new_url)) {
    CancelForRedirectAccessCheckError(new_request->Url(),
                                      ResourceRequestBlockedReason::kOther);
    return false;
  }

  if (!resource_->WillFollowRedirect(*new_request, redirect_response)) {
    CancelForRedirectAccessCheckError(new_request->Url(),
                                      ResourceRequestBlockedReason::kOther);
    return false;
  }

  has_devtools_request_id = !new_request->GetDevToolsId().IsNull();
  return true;
}

void ResourceLoader::DidSendData(uint64_t bytes_sent,
                                 uint64_t total_bytes_to_be_sent) {
  resource_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

FetchContext& ResourceLoader::Context() const {
  return fetcher_->Context();
}

void ResourceLoader::DidReceiveResponse(
    const WebURLResponse& response,
    absl::variant<mojo::ScopedDataPipeConsumerHandle, SegmentedBuffer> body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK(!response.IsNull());

  if (resource_->GetResourceRequest().GetKeepalive()) {
    // Logs when a keepalive request succeeds. It does not matter whether the
    // response is a multipart resource or not.
    FetchUtils::LogFetchKeepAliveRequestMetric(
        resource_->GetResourceRequest().GetRequestContext(),
        FetchUtils::FetchKeepAliveRequestState::kSucceeded,
        fetcher_->GetProperties().IsDetached());
  }

  DidReceiveResponseInternal(response.ToResourceResponse(),
                             std::move(cached_metadata));
  if (!IsLoading()) {
    return;
  }
  if (resource_->HasSuccessfulRevalidation()) {
    // When we succeeded the revalidation, the response is a 304 Not Modified.
    // The body of the 304 Not Modified response must be empty.
    //   RFC9110: https://www.rfc-editor.org/rfc/rfc9110.html#section-6.4.1-8
    //     All 1xx (Informational), 204 (No Content), and 304 (Not Modified)
    //     responses do not include content.
    // net::HttpStreamParser::CalculateResponseBodySize() is skipping loading
    // the body of 304 Not Modified response. And Blink don't fetch the
    // revalidating request when the page is controlled by a service worker.
    // So, We don't need to handle the body for 304 Not Modified responses.
    if (absl::holds_alternative<SegmentedBuffer>(body)) {
      CHECK(absl::get<SegmentedBuffer>(body).empty());
    } else {
      CHECK(absl::holds_alternative<mojo::ScopedDataPipeConsumerHandle>(body));
      // If the `body` is released here, the network service will treat the
      // disconnection of the `body` handle as if the request was cancelled. So
      // we keeps the `body` handle.
      empty_body_handle_for_revalidation_ =
          std::move(absl::get<mojo::ScopedDataPipeConsumerHandle>(body));
    }
    return;
  }
  if (absl::holds_alternative<SegmentedBuffer>(body)) {
    DidReceiveDataImpl(std::move(absl::get<SegmentedBuffer>(body)));
    return;
  }
  mojo::ScopedDataPipeConsumerHandle body_handle =
      std::move(absl::get<mojo::ScopedDataPipeConsumerHandle>(body));
  if (!body_handle) {
    return;
  }
  if (resource_->GetResourceRequest().DownloadToBlob()) {
    DCHECK(!blob_response_started_);
    blob_response_started_ = true;

    AtomicString mime_type = response.MimeType();

    // Callback is bound to a WeakPersistent, as ResourceLoader is kept alive by
    // ResourceFetcher as long as we still care about the result of the load.
    fetcher_->GetBlobRegistry()->RegisterFromStream(
        mime_type.IsNull() ? g_empty_string : mime_type.LowerASCII(), "",
        std::max(static_cast<int64_t>(0), response.ExpectedContentLength()),
        std::move(body_handle),
        progress_receiver_.BindNewEndpointAndPassRemote(GetLoadingTaskRunner()),
        WTF::BindOnce(&ResourceLoader::FinishedCreatingBlob,
                      WrapWeakPersistent(this)));
    return;
  }

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;
  DidStartLoadingResponseBodyInternal(
      *MakeGarbageCollected<DataPipeBytesConsumer>(task_runner_for_body_loader_,
                                                   std::move(body_handle),
                                                   &completion_notifier));
  data_pipe_completion_notifier_ = completion_notifier;
}

void ResourceLoader::DidReceiveDataForTesting(base::span<const char> data) {
  DidReceiveDataImpl(data);
}

void ResourceLoader::DidReceiveResponseInternal(
    const ResourceResponse& response,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  const ResourceRequestHead& request = resource_->GetResourceRequest();

  AtomicString content_encoding =
      response.HttpHeaderField(http_names::kContentEncoding);
  bool used_zstd = false;
  if (EqualIgnoringASCIICase(content_encoding, "zstd")) {
    fetcher_->GetUseCounter().CountUse(WebFeature::kZstdContentEncoding);
    fetcher_->GetUseCounter().CountUse(
        WebFeature::kZstdContentEncodingForSubresource);
    used_zstd = true;
  }

  // Sample the UKM recorded events. Also, a current default task runner is
  // needed to obtain a UKM recorder, so if there is not one, do not record
  // UKMs.
  if ((base::RandDouble() <= kUkmSamplingRate) &&
      base::SequencedTaskRunner::HasCurrentDefault()) {
    ukm::builders::SubresourceLoad_ZstdContentEncoding builder(
        request.GetUkmSourceId());
    builder.SetUsedZstd(used_zstd);
    builder.Record(fetcher_->UkmRecorder());
  }

  if (response.DidUseSharedDictionary()) {
    fetcher_->GetUseCounter().CountUse(WebFeature::kSharedDictionaryUsed);
    fetcher_->GetUseCounter().CountUse(
        WebFeature::kSharedDictionaryUsedForSubresource);
    if (EqualIgnoringASCIICase(content_encoding, "dcb")) {
      fetcher_->GetUseCounter().CountUse(
          WebFeature::kSharedDictionaryUsedWithSharedBrotli);
    } else if (EqualIgnoringASCIICase(content_encoding, "dcz")) {
      fetcher_->GetUseCounter().CountUse(
          WebFeature::kSharedDictionaryUsedWithSharedZstd);
    }
  }

  if (response.HasAuthorizationCoveredByWildcardOnPreflight()) {
    fetcher_->GetUseCounter().CountDeprecation(
        mojom::WebFeature::kAuthorizationCoveredByWildcard);
  }

  CountPrivateNetworkAccessPreflightResult(
      response.PrivateNetworkAccessPreflightResult());

  if (request.IsAutomaticUpgrade()) {
    LogMixedAutoupgradeMetrics(MixedContentAutoupgradeStatus::kResponseReceived,
                               response.HttpStatusCode(),
                               request.GetUkmSourceId(),
                               fetcher_->UkmRecorder(), resource_);
  }

  ResourceType resource_type = resource_->GetType();

  const ResourceRequestHead& initial_request = resource_->GetResourceRequest();
  // The following parameters never change during the lifetime of a request.
  mojom::blink::RequestContextType request_context =
      initial_request.GetRequestContext();
  network::mojom::RequestDestination request_destination =
      initial_request.GetRequestDestination();

  const ResourceLoaderOptions& options = resource_->Options();

  // Perform 'nosniff' checks against the original response instead of the 304
  // response for a successful revalidation.
  const ResourceResponse& nosniffed_response =
      (resource_->IsCacheValidator() && response.HttpStatusCode() == 304)
          ? resource_->GetResponse()
          : response;

  if (std::optional<ResourceRequestBlockedReason> blocked_reason =
          CheckResponseNosniff(request_context, nosniffed_response)) {
    HandleError(ResourceError::CancelledDueToAccessCheckError(
        response.CurrentRequestUrl(), blocked_reason.value()));
    return;
  }

  // https://wicg.github.io/cross-origin-embedder-policy/#integration-html
  // TODO(crbug.com/1064920): Remove this once PlzDedicatedWorker ships.
  if (options.reject_coep_unsafe_none &&
      !network::CompatibleWithCrossOriginIsolated(
          response.GetCrossOriginEmbedderPolicy()) &&
      !response.CurrentRequestUrl().ProtocolIsData() &&
      !response.CurrentRequestUrl().ProtocolIs("blob")) {
    DCHECK(!base::FeatureList::IsEnabled(features::kPlzDedicatedWorker));
    HandleError(ResourceError::BlockedByResponse(
        response.CurrentRequestUrl(), network::mojom::BlockedByResponseReason::
                                          kCoepFrameResourceNeedsCoepHeader));
    return;
  }

  // Redirect information for possible post-request checks below.
  const std::optional<ResourceRequest::RedirectInfo>& previous_redirect_info =
      request.GetRedirectInfo();
  const KURL& original_url = previous_redirect_info
                                 ? previous_redirect_info->original_url
                                 : request.Url();
  const ResourceRequest::RedirectInfo redirect_info(original_url,
                                                    request.Url());

  if (response.WasFetchedViaServiceWorker()) {
    // Run post-request CSP checks. This is the "Should response to request be
    // blocked by Content Security Policy?" algorithm in the CSP specification:
    // https://w3c.github.io/webappsec-csp/#should-block-response
    //
    // In particular, the connect-src directive's post-request check:
    // https://w3c.github.io/webappsec-csp/#connect-src-post-request)
    //
    // We only run post-request checks when the response was fetched via service
    // worker, because that is the only case where the response URL can differ
    // from the current request URL, allowing the result of the check to differ
    // from the pre-request check. The pre-request check is implemented in
    // ResourceFetcher::PrepareRequest() and
    // ResourceFetcher::WillFollowRedirect().
    //
    // TODO(falken): To align with the CSP specification, implement post-request
    // checks as a first-class concept instead of just reusing the functions for
    // pre-request checks, and consider running the checks regardless of service
    // worker interception.
    //
    // CanRequest() below only checks enforced policies: check report-only
    // here to ensure violations are sent.
    const KURL& response_url = response.ResponseUrl();
    Context().CheckCSPForRequest(
        request_context, request_destination, response_url, options,
        ReportingDisposition::kReport, original_url,
        ResourceRequest::RedirectStatus::kFollowedRedirect);

    std::optional<ResourceRequestBlockedReason> blocked_reason =
        Context().CanRequest(resource_type, ResourceRequest(initial_request),
                             response_url, options,
                             ReportingDisposition::kReport, redirect_info);
    if (blocked_reason) {
      HandleError(ResourceError::CancelledDueToAccessCheckError(
          response_url, blocked_reason.value()));
      return;
    }
  }

  if (base::FeatureList::IsEnabled(
          features::kSendCnameAliasesToSubresourceFilterFromRenderer)) {
    bool should_block = ShouldBlockRequestBasedOnSubresourceFilterDnsAliasCheck(
        response.DnsAliases(), request.Url(), original_url, resource_type,
        initial_request, options, redirect_info);

    if (should_block) {
      return;
    }
  }

  scheduler_->SetConnectionInfo(scheduler_client_id_,
                                response.ConnectionInfo());

  // A response should not serve partial content if it was not requested via a
  // Range header: https://fetch.spec.whatwg.org/#main-fetch
  if (response.GetType() == network::mojom::FetchResponseType::kOpaque &&
      response.HttpStatusCode() == 206 && response.HasRangeRequested() &&
      !initial_request.HttpHeaderFields().Contains(http_names::kRange)) {
    HandleError(ResourceError::CancelledDueToAccessCheckError(
        response.CurrentRequestUrl(), ResourceRequestBlockedReason::kOther));
    return;
  }

  fetcher_->MarkEarlyHintConsumedIfNeeded(resource_->InspectorId(), resource_,
                                          response);
  // FrameType never changes during the lifetime of a request.
  if (auto* observer = fetcher_->GetResourceLoadObserver()) {
    ResourceRequest request_for_obserber(initial_request);
    // TODO(yoichio): Have DidReceiveResponse take a ResourceResponseHead, not
    // ResourceRequest.
    observer->DidReceiveResponse(
        resource_->InspectorId(), request_for_obserber, response, resource_,
        ResourceLoadObserver::ResponseSource::kNotFromMemoryCache);
  }

  resource_->ResponseReceived(response);

  if (resource_->Loader() && fetcher_->GetProperties().IsDetached()) {
    // If the fetch context is already detached, we don't need further signals,
    // so let's cancel the request.
    HandleError(ResourceError::CancelledError(response.CurrentRequestUrl()));
    return;
  }

  if (!resource_->Loader()) {
    return;
  }

  // Not SetSerializedCachedMetadata in a successful revalidation
  // because resource content would not expect to be changed.
  if (!resource_->HasSuccessfulRevalidation() && cached_metadata &&
      cached_metadata->size()) {
    resource_->SetSerializedCachedMetadata(std::move(*cached_metadata));
  }

  if (auto* frame_or_worker_scheduler = fetcher_->GetFrameOrWorkerScheduler()) {
    if (response.CacheControlContainsNoCache()) {
      frame_or_worker_scheduler->RegisterStickyFeature(
          SchedulingPolicy::Feature::kSubresourceHasCacheControlNoCache,
          {SchedulingPolicy::DisableBackForwardCache()});
    }
    if (response.CacheControlContainsNoStore()) {
      frame_or_worker_scheduler->RegisterStickyFeature(
          SchedulingPolicy::Feature::kSubresourceHasCacheControlNoStore,
          {SchedulingPolicy::DisableBackForwardCache()});
    }
  }

  if (!resource_->Loader()) {
    return;
  }

  if (response.HttpStatusCode() >= 400 &&
      !resource_->ShouldIgnoreHTTPStatusCodeErrors()) {
    HandleError(ResourceError::HttpError(response.CurrentRequestUrl()));
    return;
  }
}

void ResourceLoader::DidReceiveData(base::span<const char> data) {
  DidReceiveDataImpl(data);
}

void ResourceLoader::DidReceiveDataImpl(
    absl::variant<SegmentedBuffer, base::span<const char>> data) {
  size_t data_size = 0;
  // If a BackgroundResponseProcessor consumed the body data on the background
  // thread, this method is called with a SegmentedBuffer data. Otherwise, it is
  // called with a span<const char> data several times.
  if (absl::holds_alternative<SegmentedBuffer>(data)) {
    data_size = absl::get<SegmentedBuffer>(data).size();
    if (auto* observer = fetcher_->GetResourceLoadObserver()) {
      for (const auto& span : absl::get<SegmentedBuffer>(data)) {
        observer->DidReceiveData(resource_->InspectorId(),
                                 base::SpanOrSize(span));
      }
    }
  } else {
    CHECK(absl::holds_alternative<base::span<const char>>(data));
    base::span<const char> span = absl::get<base::span<const char>>(data);
    data_size = span.size();
    if (auto* observer = fetcher_->GetResourceLoadObserver()) {
      observer->DidReceiveData(resource_->InspectorId(),
                               base::SpanOrSize(span));
    }
  }
  resource_->AppendData(std::move(data));

  // This value should not be exposed for opaque responses.
  if (resource_->response_.WasFetchedViaServiceWorker() &&
      resource_->response_.GetType() !=
          network::mojom::FetchResponseType::kOpaque) {
    // `received_body_length_from_service_worker_` needs to fit into both a
    // uint64_t and an int64_t so must be >= 0 and also <=
    // std::numeric_limits<int64_t>::max(); Since `length` is guaranteed never
    // to be negative, the value must always increase, giving assurance that it
    // will always be >= 0, but the CheckAdd is used to enforce the second
    // constraint.
    received_body_length_from_service_worker_ =
        base::CheckAdd(received_body_length_from_service_worker_, data_size)
            .ValueOrDie<int64_t>();
  }
}

void ResourceLoader::DidReceiveTransferSizeUpdate(int transfer_size_diff) {
  if (auto* observer = fetcher_->GetResourceLoadObserver()) {
    observer->DidReceiveTransferSizeUpdate(resource_->InspectorId(),
                                           transfer_size_diff);
  }
}

void ResourceLoader::DidFinishLoadingFirstPartInMultipart() {
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      TRACE_DISABLED_BY_DEFAULT("network"), "ResourceLoad",
      TRACE_ID_WITH_SCOPE("BlinkResourceID",
                          TRACE_ID_LOCAL(resource_->InspectorId())),
      "outcome", RequestOutcomeToString(RequestOutcome::kSuccess));

  fetcher_->HandleLoaderFinish(resource_.Get(), base::TimeTicks(),
                               ResourceFetcher::kDidFinishFirstPartInMultipart,
                               0);
}

void ResourceLoader::DidFinishLoading(base::TimeTicks response_end_time,
                                      int64_t encoded_data_length,
                                      uint64_t encoded_body_length,
                                      int64_t decoded_body_length) {
  if (resource_->response_.WasFetchedViaServiceWorker()) {
    encoded_body_length = received_body_length_from_service_worker_;
    decoded_body_length = received_body_length_from_service_worker_;
  }

  resource_->SetEncodedDataLength(encoded_data_length);
  resource_->SetEncodedBodyLength(encoded_body_length);
  resource_->SetDecodedBodyLength(decoded_body_length);

  response_end_time_for_error_cases_ = response_end_time;

  if ((response_body_loader_ && !has_seen_end_of_body_ &&
       !response_body_loader_->IsAborted()) ||
      (resource_->GetResourceRequest().DownloadToBlob() && !blob_finished_ &&
       blob_response_started_)) {
    // If the body is still being loaded, we defer the completion until all the
    // body is received.
    deferred_finish_loading_info_ =
        DeferredFinishLoadingInfo{response_end_time};

    if (data_pipe_completion_notifier_) {
      data_pipe_completion_notifier_->SignalComplete();
    }
    return;
  }

  Release(ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
          ResourceLoadScheduler::TrafficReportHints(encoded_data_length,
                                                    decoded_body_length));
  loader_.reset();
  response_body_loader_ = nullptr;
  has_seen_end_of_body_ = false;
  deferred_finish_loading_info_ = std::nullopt;
  finished_ = true;

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      TRACE_DISABLED_BY_DEFAULT("network"), "ResourceLoad",
      TRACE_ID_WITH_SCOPE("BlinkResourceID",
                          TRACE_ID_LOCAL(resource_->InspectorId())),
      "outcome", RequestOutcomeToString(RequestOutcome::kSuccess));

  fetcher_->HandleLoaderFinish(resource_.Get(), response_end_time,
                               ResourceFetcher::kDidFinishLoading,
                               inflight_keepalive_bytes_);
}

void ResourceLoader::DidFail(const WebURLError& error,
                             base::TimeTicks response_end_time,
                             int64_t encoded_data_length,
                             uint64_t encoded_body_length,
                             int64_t decoded_body_length) {
  const ResourceRequestHead& request = resource_->GetResourceRequest();
  response_end_time_for_error_cases_ = response_end_time;

  if (request.IsAutomaticUpgrade()) {
    LogMixedAutoupgradeMetrics(MixedContentAutoupgradeStatus::kFailed,
                               error.reason(), request.GetUkmSourceId(),
                               fetcher_->UkmRecorder(), resource_);
  }

  CountPrivateNetworkAccessPreflightResult(
      error.private_network_access_preflight_result());

  resource_->SetEncodedDataLength(encoded_data_length);
  resource_->SetEncodedBodyLength(encoded_body_length);
  resource_->SetDecodedBodyLength(decoded_body_length);
  HandleError(ResourceError(error));
}

void ResourceLoader::CountFeature(blink::mojom::WebFeature feature) {
  fetcher_->GetUseCounter().CountUse(feature);
}

void ResourceLoader::HandleError(const ResourceError& error) {
  if (resource_->GetResourceRequest().GetKeepalive()) {
    FetchUtils::LogFetchKeepAliveRequestMetric(
        resource_->GetResourceRequest().GetRequestContext(),
        FetchUtils::FetchKeepAliveRequestState::kFailed);
  }

  if (error.CorsErrorStatus() &&
      error.CorsErrorStatus()
          ->has_authorization_covered_by_wildcard_on_preflight) {
    fetcher_->GetUseCounter().CountUse(
        mojom::WebFeature::kAuthorizationCoveredByWildcard);
  }

  if (response_body_loader_) {
    response_body_loader_->Abort();
  }

  if (data_pipe_completion_notifier_) {
    data_pipe_completion_notifier_->SignalError(BytesConsumer::Error());
  }

  if (is_cache_aware_loading_activated_ && error.IsCacheMiss() &&
      !fetcher_->GetProperties().ShouldBlockLoadingSubResource()) {
    resource_->WillReloadAfterDiskCacheMiss();
    is_cache_aware_loading_activated_ = false;
    Restart();
    return;
  }
  if (error.CorsErrorStatus()) {
    // CORS issues are reported via network service instrumentation.
    fetcher_->GetConsoleLogger().AddConsoleMessage(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        cors::GetErrorString(
            *error.CorsErrorStatus(), resource_->GetResourceRequest().Url(),
            resource_->LastResourceRequest().Url(), *resource_->GetOrigin(),
            resource_->GetType(), resource_->Options().initiator_info.name),
        false /* discard_duplicates */, mojom::ConsoleMessageCategory::Cors);
  }

  Release(ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
          ResourceLoadScheduler::TrafficReportHints::InvalidInstance());
  loader_.reset();
  response_body_loader_ = nullptr;
  has_seen_end_of_body_ = false;
  deferred_finish_loading_info_ = std::nullopt;
  finished_ = true;

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      TRACE_DISABLED_BY_DEFAULT("network"), "ResourceLoad",
      TRACE_ID_WITH_SCOPE("BlinkResourceID",
                          TRACE_ID_LOCAL(resource_->InspectorId())),
      "outcome", RequestOutcomeToString(RequestOutcome::kFail));

  // Set Now() as the response time, in case a more accurate one wasn't set in
  // DidFinishLoading or DidFail. This is important for error cases that don't
  // go through those methods.
  if (response_end_time_for_error_cases_.is_null()) {
    response_end_time_for_error_cases_ = base::TimeTicks::Now();
  }
  fetcher_->HandleLoaderError(resource_.Get(),
                              response_end_time_for_error_cases_, error,
                              inflight_keepalive_bytes_);
}

void ResourceLoader::RequestSynchronously() {
  DCHECK(IsLoading());
  DCHECK_EQ(resource_->GetResourceRequest().Priority(),
            ResourceLoadPriority::kHighest);

  WebURLResponse response_out;
  std::optional<WebURLError> error_out;
  scoped_refptr<SharedBuffer> data_out;
  int64_t encoded_data_length = URLLoaderClient::kUnknownEncodedDataLength;
  uint64_t encoded_body_length = 0;
  scoped_refptr<BlobDataHandle> downloaded_blob;
  const ResourceRequestHead& request = resource_->GetResourceRequest();

  if (resource_->Url().ProtocolIsData()) {
    CHECK(!network_resource_request_);
    CHECK(!loader_);
    // We don't have to verify mime type again since it's allowed to handle
    // the data url with invalid mime type in some cases.
    // CanHandleDataURLRequestLocally() has already checked if the data url can
    // be handled here.
    auto [result, response, data] = network_utils::ParseDataURL(
        resource_->Url(), request.HttpMethod(), request.GetUkmSourceId(),
        fetcher_->UkmRecorder());
    if (result != net::OK) {
      error_out = WebURLError(result, resource_->Url());
    } else {
      response_out = WrappedResourceResponse(response);
      data_out = std::move(data);
    }
  } else {
    CHECK(network_resource_request_);
    CHECK(loader_);
    // Don't do mime sniffing for fetch (crbug.com/2016)
    bool no_mime_sniffing = request.GetRequestContext() ==
                            blink::mojom::blink::RequestContextType::FETCH;
    loader_->LoadSynchronously(
        std::move(network_resource_request_), Context().GetTopFrameOrigin(),
        request.DownloadToBlob(), no_mime_sniffing, request.TimeoutInterval(),
        this, response_out, error_out, data_out, encoded_data_length,
        encoded_body_length, downloaded_blob,
        Context().CreateResourceLoadInfoNotifierWrapper());
  }
  // A message dispatched while synchronously fetching the resource
  // can bring about the cancellation of this load.
  if (!IsLoading()) {
    return;
  }
  int64_t decoded_body_length = data_out ? data_out->size() : 0;
  if (error_out) {
    DidFail(*error_out, base::TimeTicks::Now(), encoded_data_length,
            encoded_body_length, decoded_body_length);
    return;
  }

  DidReceiveResponseInternal(response_out.ToResourceResponse(),
                             /*cached_metadata=*/std::nullopt);
  if (!IsLoading()) {
    return;
  }
  DCHECK_GE(response_out.ToResourceResponse().EncodedBodyLength(), 0);

  // Follow the async case convention of not calling DidReceiveData or
  // appending data to m_resource if the response body is empty. Copying the
  // empty buffer is a noop in most cases, but is destructive in the case of
  // a 304, where it will overwrite the cached data we should be reusing.
  if (data_out && data_out->size()) {
    for (const auto& span : *data_out) {
      DidReceiveData(span);
    }
  }

  if (request.DownloadToBlob()) {
    if (downloaded_blob) {
      OnProgress(downloaded_blob->size());
    }
    FinishedCreatingBlob(std::move(downloaded_blob));
  }
  DidFinishLoading(base::TimeTicks::Now(), encoded_data_length,
                   encoded_body_length, decoded_body_length);
}

void ResourceLoader::RequestAsynchronously() {
  if (resource_->Url().ProtocolIsData()) {
    CHECK(!network_resource_request_);
    CHECK(!loader_);
    // Handle DataURL in another task instead of using |loader_|.
    GetLoadingTaskRunner()->PostTask(
        FROM_HERE, WTF::BindOnce(&ResourceLoader::HandleDataUrl,
                                 WrapWeakPersistent(this)));
    return;
  }
  CHECK(loader_);
  CHECK(network_resource_request_);

  // When `loader_` is a BackgroundURLLoader and
  // kBackgroundResponseProcessorBackground feature param is enabled, creates a
  // BackgroundResponseProcessor for the `resource_`, and set it to the
  // `loader_`.
  if (loader_->CanHandleResponseOnBackground()) {
    if (auto factory =
            resource_->MaybeCreateBackgroundResponseProcessorFactory()) {
      loader_->SetBackgroundResponseProcessorFactory(std::move(factory));
    }
  }

  // Don't do mime sniffing for fetch (crbug.com/2016)
  bool no_mime_sniffing = resource_->GetResourceRequest().GetRequestContext() ==
                          blink::mojom::blink::RequestContextType::FETCH;

  // Don't pass a CodeCacheHost when DownloadToBlob is true. The detailed
  // decision logic for whether or not to fetch code cache from the isolated
  // code cache is implemented in ResourceRequestSender::CodeCacheFetcher. We
  // only check the DownloadToBlob flag here, which ResourceRequestSender cannot
  // know.
  loader_->LoadAsynchronously(std::move(network_resource_request_),
                              Context().GetTopFrameOrigin(), no_mime_sniffing,
                              Context().CreateResourceLoadInfoNotifierWrapper(),
                              !resource_->GetResourceRequest().DownloadToBlob()
                                  ? fetcher_->GetCodeCacheHost()
                                  : nullptr,
                              this);
}

void ResourceLoader::Dispose() {
  loader_ = nullptr;
  progress_receiver_.reset();

  // Release() should be called to release |scheduler_client_id_| beforehand in
  // DidFinishLoading() or DidFail(), but when a timer to call Cancel() is
  // ignored due to GC, this case happens. We just release here because we can
  // not schedule another request safely. See crbug.com/675947.
  if (scheduler_client_id_ != ResourceLoadScheduler::kInvalidClientId) {
    Release(ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
            ResourceLoadScheduler::TrafficReportHints::InvalidInstance());
  }
}

bool ResourceLoader::ShouldBeKeptAliveWhenDetached() const {
  if (base::FeatureList::IsEnabled(
          blink::features::kKeepAliveInBrowserMigration) &&
      resource_->GetResourceRequest().GetKeepalive()) {
    if (resource_->GetResourceRequest().GetAttributionReportingEligibility() ==
        network::mojom::AttributionReportingEligibility::kUnset) {
      // When enabled, non-attribution reporting Fetch keepalive requests should
      // not be kept alive by renderer.
      return false;
    }
    if (base::FeatureList::IsEnabled(
            blink::features::kAttributionReportingInBrowserMigration)) {
      // Attribution reporting keepalive requests with its owned migration
      // enabled should not be kept alive by renderer.
      return false;
    }
  }

  return resource_->GetResourceRequest().GetKeepalive() &&
         resource_->GetResponse().IsNull();
}

void ResourceLoader::AbortResponseBodyLoading() {
  if (response_body_loader_) {
    response_body_loader_->Abort();
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
ResourceLoader::GetLoadingTaskRunner() {
  return fetcher_->GetTaskRunner();
}

void ResourceLoader::OnProgress(uint64_t delta) {
  DCHECK(!blob_finished_);

  if (scheduler_client_id_ == ResourceLoadScheduler::kInvalidClientId) {
    return;
  }

  if (auto* observer = fetcher_->GetResourceLoadObserver()) {
    observer->DidReceiveData(
        resource_->InspectorId(),
        base::SpanOrSize<const char>(base::checked_cast<size_t>(delta)));
  }
  resource_->DidDownloadData(delta);
}

void ResourceLoader::FinishedCreatingBlob(
    const scoped_refptr<BlobDataHandle>& blob) {
  DCHECK(!blob_finished_);

  if (scheduler_client_id_ == ResourceLoadScheduler::kInvalidClientId) {
    return;
  }

  if (auto* observer = fetcher_->GetResourceLoadObserver()) {
    observer->DidDownloadToBlob(resource_->InspectorId(), blob.get());
  }
  resource_->DidDownloadToBlob(blob);

  blob_finished_ = true;
  if (deferred_finish_loading_info_) {
    const ResourceResponse& response = resource_->GetResponse();
    DidFinishLoading(deferred_finish_loading_info_->response_end_time,
                     response.EncodedDataLength(), response.EncodedBodyLength(),
                     response.DecodedBodyLength());
  }
}

std::optional<ResourceRequestBlockedReason>
ResourceLoader::CheckResponseNosniff(
    mojom::blink::RequestContextType request_context,
    const ResourceResponse& response) {
  bool sniffing_allowed =
      ParseContentTypeOptionsHeader(response.HttpHeaderField(
          http_names::kXContentTypeOptions)) != kContentTypeOptionsNosniff;
  if (sniffing_allowed) {
    return std::nullopt;
  }

  String mime_type = response.HttpContentType();
  if (request_context == mojom::blink::RequestContextType::STYLE &&
      !MIMETypeRegistry::IsSupportedStyleSheetMIMEType(mime_type)) {
    fetcher_->GetConsoleLogger().AddConsoleMessage(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Refused to apply style from '" +
            response.CurrentRequestUrl().ElidedString() +
            "' because its MIME type ('" + mime_type + "') " +
            "is not a supported stylesheet MIME type, and strict MIME checking "
            "is enabled.");
    return ResourceRequestBlockedReason::kContentType;
  }
  // TODO(mkwst): Move the 'nosniff' bit of 'AllowedByNosniff::MimeTypeAsScript'
  // here alongside the style checks, and put its use counters somewhere else.

  return std::nullopt;
}

void ResourceLoader::HandleDataUrl() {
  if (!IsLoading()) {
    return;
  }
  if (freeze_mode_ != LoaderFreezeMode::kNone) {
    defers_handling_data_url_ = true;
    return;
  }

  // Extract a ResourceResponse from the data url.
  // We don't have to verify mime type again since it's allowed to handle the
  // data url with invalid mime type in some cases.
  // CanHandleDataURLRequestLocally() has already checked if the data url can be
  // handled here.
  auto [result, response, data] = network_utils::ParseDataURL(
      resource_->Url(), resource_->GetResourceRequest().HttpMethod(),
      resource_->GetResourceRequest().GetUkmSourceId(),
      fetcher_->UkmRecorder());
  if (result != net::OK) {
    HandleError(ResourceError(result, resource_->Url(), std::nullopt));
    return;
  }
  DCHECK(data);
  const size_t data_size = data->size();

  DidReceiveResponseInternal(response, /*cached_metadata=*/std::nullopt);
  if (!IsLoading()) {
    return;
  }

  auto* bytes_consumer =
      MakeGarbageCollected<SharedBufferBytesConsumer>(std::move(data));
  DidStartLoadingResponseBodyInternal(*bytes_consumer);
  if (!IsLoading()) {
    return;
  }

  // DidFinishLoading() may deferred until the response body loader reaches to
  // end.
  DidFinishLoading(base::TimeTicks::Now(), data_size, data_size, data_size);
}

bool ResourceLoader::ShouldBlockRequestBasedOnSubresourceFilterDnsAliasCheck(
    const Vector<String>& dns_aliases,
    const KURL& request_url,
    const KURL& original_url,
    ResourceType resource_type,
    const ResourceRequestHead& initial_request,
    const ResourceLoaderOptions& options,
    const ResourceRequest::RedirectInfo redirect_info) {
  // Look for CNAME aliases, and if any are found, run SubresourceFilter
  // checks on them to perform resource-blocking and ad-tagging based on the
  // aliases: if any one of the aliases is on the denylist, then the
  // request will be deemed on the denylist and treated accordingly (blocked
  // and/or tagged).
  cname_alias_info_for_testing_.has_aliases = !dns_aliases.empty();
  cname_alias_info_for_testing_.list_length = dns_aliases.size();

  // If there are no aliases, we have no reason to block based on them.
  if (!cname_alias_info_for_testing_.has_aliases) {
    return false;
  }

  // CNAME aliases were found, and so the SubresourceFilter must be
  // consulted for each one.
  // Create a copy of the request URL. We will swap out the host below.
  KURL alias_url = request_url;

  for (const String& alias : dns_aliases) {
    alias_url.SetHost(alias);

    // The SubresourceFilter only performs nontrivial matches for
    // valid URLs. Skip sending this alias if it's invalid.
    if (!alias_url.IsValid()) {
      cname_alias_info_for_testing_.invalid_count++;
      continue;
    }

    // Do not perform a SubresourceFilter check on an `alias_url` that matches
    // the requested URL (or, inclusively, the original URL in the case of
    // redirects).
    if (alias_url == original_url || alias_url == request_url) {
      cname_alias_info_for_testing_.redundant_count++;
      continue;
    }

    std::optional<ResourceRequestBlockedReason> blocked_reason =
        Context().CanRequestBasedOnSubresourceFilterOnly(
            resource_type, ResourceRequest(initial_request), alias_url, options,
            ReportingDisposition::kReport, redirect_info);
    if (blocked_reason) {
      HandleError(ResourceError::CancelledDueToAccessCheckError(
          alias_url, blocked_reason.value()));
      cname_alias_info_for_testing_.was_blocked_based_on_alias = true;
      return true;
    }

    if (!resource_->GetResourceRequest().IsAdResource() &&
        Context().CalculateIfAdSubresource(resource_->GetResourceRequest(),
                                           alias_url, resource_type,
                                           options.initiator_info)) {
      resource_->SetIsAdResource();
      cname_alias_info_for_testing_.was_ad_tagged_based_on_alias = true;
    }
  }

  return false;
}

void ResourceLoader::CountPrivateNetworkAccessPreflightResult(
    network::mojom::PrivateNetworkAccessPreflightResult result) {
  std::optional<mojom::WebFeature> feature =
      PreflightResultToWebFeature(result);
  if (!feature.has_value()) {
    return;
  }

  // We do not call `CountDeprecation()` because sending a deprecation report
  // would leak cross-origin information about the target of the fetch. Already,
  // the presence of this information in the renderer process is suboptimal, but
  // as of writing this is the best way to count a feature use detected in the
  // network service.
  fetcher_->GetUseCounter().CountUse(*feature);
}

void ResourceLoader::CancelIfWebBundleTokenMatches(
    const base::UnguessableToken& web_bundle_token) {
  if (resource_->GetResourceRequest().GetWebBundleTokenParams().has_value() &&
      resource_->GetResourceRequest().GetWebBundleTokenParams().value().token ==
          web_bundle_token) {
    Cancel();
  }
}

}  // namespace blink
