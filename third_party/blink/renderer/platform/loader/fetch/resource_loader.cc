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

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_code_cache_loader.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/cors/cors_error_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/shared_buffer_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/request_conversion.h"
#include "third_party/blink/renderer/platform/loader/mixed_content_autoupgrade_status.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

enum RequestOutcome { kSuccess, kFail };

std::unique_ptr<TracedValue> EndResourceLoadData(RequestOutcome outcome) {
  auto value = std::make_unique<TracedValue>();
  switch (outcome) {
    case RequestOutcome::kSuccess:
      value->SetString("outcome", "Success");
      break;
    case RequestOutcome::kFail:
      value->SetString("outcome", "Fail");
      break;
  }

  return value;
}

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
                                base::Optional<int> response_or_error_code,
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

bool CanHandleDataURLRequestLocally(const ResourceRequestHead& request) {
  if (!request.Url().ProtocolIsData())
    return false;

  // The fast paths for data URL, Start() and HandleDataURL(), don't support
  // the DownloadToBlob option.
  if (request.DownloadToBlob())
    return false;

  // Main resources are handled in the browser, so we can handle data url
  // subresources locally.
  return true;
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

}  // namespace

// CodeCacheRequest handles the requests to fetch data from code cache.
// This owns WebCodeCacheLoader that actually loads the data from the
// code cache. This class performs the necessary checks of matching the
// resource response time and the code cache response time before sending the
// data to the resource (see https://crbug.com/1099587). It caches the data
// returned from the code cache if the response wasn't received. One
// CodeCacheRequest handles only one request. On a restart new CodeCacheRequest
// is created.
class ResourceLoader::CodeCacheRequest {
  USING_FAST_MALLOC(ResourceLoader::CodeCacheRequest);

 public:
  CodeCacheRequest(std::unique_ptr<WebCodeCacheLoader> code_cache_loader,
                   const KURL& url,
                   WebURLLoader::DeferType defers_loading)
      : status_(kNoRequestSent),
        code_cache_loader_(std::move(code_cache_loader)),
        url_(url),
        defers_loading_(defers_loading) {
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
  bool SetDefersLoading(WebURLLoader::DeferType defers);

 private:
  enum CodeCacheRequestStatus {
    kNoRequestSent,
    kPendingResponse,
    kReceivedResponse
  };

  // Callback to receive data from WebCodeCacheLoader.
  void DidReceiveCachedCode(ResourceLoader* loader,
                            base::Time response_time,
                            mojo_base::BigBuffer data);

  // Process the response from code cache.
  void ProcessCodeCacheResponse(const base::Time& response_time,
                                mojo_base::BigBuffer data,
                                ResourceLoader* resource_loader);

  // Send |cache_code| if we got a response from code_cache_loader and the
  // web_url_loader.
  void MaybeSendCachedCode(mojo_base::BigBuffer data,
                           ResourceLoader* resource_loader);

  CodeCacheRequestStatus status_;
  std::unique_ptr<WebCodeCacheLoader> code_cache_loader_;
  const WebURL url_;
  WebURLLoader::DeferType defers_loading_ =
      WebURLLoader::DeferType::kNotDeferred;
  mojo_base::BigBuffer cached_code_;
  base::Time cached_code_response_time_;
  base::Time resource_response_time_;
  bool use_isolated_code_cache_ = false;
  base::WeakPtrFactory<CodeCacheRequest> weak_ptr_factory_{this};
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
  url_loader->SetDefersLoading(WebURLLoader::DeferType::kDeferred);

  WebCodeCacheLoader::FetchCodeCacheCallback callback =
      base::BindOnce(&ResourceLoader::CodeCacheRequest::DidReceiveCachedCode,
                     weak_ptr_factory_.GetWeakPtr(), resource_loader);
  auto cache_type = resource_loader->GetCodeCacheType();
  code_cache_loader_->FetchFromCodeCache(cache_type, url_, std::move(callback));
  return true;
}

bool ResourceLoader::CodeCacheRequest::FetchFromCodeCacheSynchronously(
    ResourceLoader* resource_loader) {
  if (!code_cache_loader_)
    return false;
  DCHECK_EQ(status_, kNoRequestSent);
  status_ = kPendingResponse;

  base::Time response_time;
  mojo_base::BigBuffer data;
  code_cache_loader_->FetchFromCodeCacheSynchronously(url_, &response_time,
                                                      &data);
  ProcessCodeCacheResponse(response_time, std::move(data), resource_loader);
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
  MaybeSendCachedCode(std::move(cached_code_), resource_loader);
}

// Returns true if |this| handles |defers| and therefore the callsite, i.e. the
// loader, doesn't need to take care of it). Returns false otherwise.
bool ResourceLoader::CodeCacheRequest::SetDefersLoading(
    WebURLLoader::DeferType defers) {
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
    base::Time response_time,
    mojo_base::BigBuffer data) {
  ProcessCodeCacheResponse(response_time, std::move(data), resource_loader);
  // Reset the deferred value to its original state.
  DCHECK(resource_loader);
  resource_loader->SetDefersLoading(defers_loading_);
}

// This is called when a response is received from code cache. If the
// resource response time is not available the response is buffered and
// will be processed when the response is received from the URLLoader.
void ResourceLoader::CodeCacheRequest::ProcessCodeCacheResponse(
    const base::Time& response_time,
    mojo_base::BigBuffer data,
    ResourceLoader* resource_loader) {
  status_ = kReceivedResponse;
  cached_code_response_time_ = response_time;

  if (resource_response_time_.is_null()) {
    // Wait for the response before we can send the cached code.
    // TODO(crbug.com/866889): Pass this as a handle to avoid the overhead of
    // copying this data.
    cached_code_ = std::move(data);
    return;
  }

  MaybeSendCachedCode(std::move(data), resource_loader);
}

void ResourceLoader::CodeCacheRequest::MaybeSendCachedCode(
    mojo_base::BigBuffer data,
    ResourceLoader* resource_loader) {
  if (status_ != kReceivedResponse || cached_code_response_time_.is_null() ||
      resource_response_time_.is_null()) {
    return;
  }

  // If the resource was fetched for service worker script or was served from
  // CacheStorage via service worker then they maintain their own code cache.
  // We should not use the isolated cache.
  if (!use_isolated_code_cache_) {
    resource_loader->ClearCachedCode();
    return;
  }

  // If the timestamps don't match, the code cache data may be for a different
  // response. See https://crbug.com/1099587.
  if (resource_response_time_ != cached_code_response_time_) {
    resource_loader->ClearCachedCode();
    return;
  }

  if (data.size() > 0) {
    resource_loader->SendCachedCodeToResource(std::move(data));
  }
}

ResourceLoader::ResourceLoader(ResourceFetcher* fetcher,
                               ResourceLoadScheduler* scheduler,
                               Resource* resource,
                               ResourceRequestBody request_body,
                               uint32_t inflight_keepalive_bytes)
    : scheduler_client_id_(ResourceLoadScheduler::kInvalidClientId),
      fetcher_(fetcher),
      scheduler_(scheduler),
      resource_(resource),
      request_body_(std::move(request_body)),
      inflight_keepalive_bytes_(inflight_keepalive_bytes),
      is_cache_aware_loading_activated_(false),
      cancel_timer_(fetcher_->GetTaskRunner(),
                    this,
                    &ResourceLoader::CancelTimerFired) {
  DCHECK(resource_);
  DCHECK(fetcher_);

  // Some requests should not block the page from entering the BackForwardCache.
  // If they are keepalive request && their responses are not observable to web
  // content, we can have them survive without breaking web content when the
  // page is put into BackForwardCache.
  auto& request = resource_->GetResourceRequest();
  auto request_context = request.GetRequestContext();
  if (!RequestContextObserveResponse(request_context)) {
    if (auto* frame_or_worker_scheduler =
            fetcher->GetFrameOrWorkerScheduler()) {
      // Only when this feature is turned on and the loading tasks keep being
      // processed and the data is queued up on the renderer, a page can stay in
      // BackForwardCache with network requests.
      if (!base::FeatureList::IsEnabled(features::kLoadingTasksUnfreezable)) {
        feature_handle_for_scheduler_ =
            frame_or_worker_scheduler->RegisterFeature(
                GetFeatureFromRequestContextType(request_context),
                {SchedulingPolicy::RecordMetricsForBackForwardCache()});
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
  ResourceLoadSchedulerClient::Trace(visitor);
}

bool ResourceLoader::ShouldFetchCodeCache() {
  if (!RuntimeEnabledFeatures::IsolatedCodeCacheEnabled())
    return false;

  const ResourceRequestHead& request = resource_->GetResourceRequest();
  if (!request.Url().ProtocolIsInHTTPFamily())
    return false;
  // When loading the service worker scripts, we don't need to check the
  // GeneratedCodeCache. The code cache corresponding to these scripts is in
  // the service worker's "installed script storage" and would be fetched along
  // with the resource from the cache storage.
  if (request.GetRequestContext() ==
      mojom::blink::RequestContextType::SERVICE_WORKER)
    return false;
  if (request.DownloadToBlob())
    return false;
  // Javascript resources have type kScript. WebAssembly module resources
  // have type kRaw. Note that we always perform a code fetch for all of
  // these resources because:
  //
  // * It is not easy to distinguish WebAssembly modules from other raw
  //   resources
  // * The fetch might be handled by Service Workers, but we can't still know
  //   if the response comes from the CacheStorage (in such cases its own
  //   code cache will be used) or not.
  //
  // These fetches should be cheap, however, requiring one additional IPC and
  // no browser process disk IO since the cache index is in memory and the
  // resource key should not be present.
  return resource_->GetType() == ResourceType::kScript ||
         resource_->GetType() == ResourceType::kRaw;
}

void ResourceLoader::Start() {
  const ResourceRequestHead& request = resource_->GetResourceRequest();
  ActivateCacheAwareLoadingIfNeeded(request);
  loader_ = fetcher_->CreateURLLoader(request, resource_->Options());
  task_runner_for_body_loader_ = loader_->GetTaskRunnerForBodyLoader();
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
    mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> pending_recorder;
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        pending_recorder.InitWithNewPipeAndPassReceiver());
    auto recorder =
        std::make_unique<ukm::MojoUkmRecorder>(std::move(pending_recorder));
    LogMixedAutoupgradeMetrics(MixedContentAutoupgradeStatus::kStarted,
                               base::nullopt, request.GetUkmSourceId(),
                               recorder.get(), resource_);
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
      bytes_consumer, response_body_loader_client,
      task_runner_for_body_loader_);
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
  StartWith(resource_->GetResourceRequest());
}

void ResourceLoader::DidReceiveData(base::span<const char> data) {
  DidReceiveData(data.data(), data.size());
}

void ResourceLoader::DidFinishLoadingBody() {
  has_seen_end_of_body_ = true;

  const ResourceResponse& response = resource_->GetResponse();
  if (deferred_finish_loading_info_) {
    DidFinishLoading(
        deferred_finish_loading_info_->response_end,
        response.EncodedDataLength(), response.EncodedBodyLength(),
        response.DecodedBodyLength(),
        deferred_finish_loading_info_->should_report_corb_blocking);
  }
}

void ResourceLoader::DidFailLoadingBody() {
  DidFail(WebURLError(ResourceError::Failure(resource_->Url())), 0, 0, 0);
}

void ResourceLoader::DidCancelLoadingBody() {
  Cancel();
}

void ResourceLoader::StartWith(const ResourceRequestHead& request) {
  DCHECK_NE(ResourceLoadScheduler::kInvalidClientId, scheduler_client_id_);
  DCHECK(loader_);

  if (resource_->Options().synchronous_policy == kRequestSynchronously &&
      fetcher_->GetProperties().IsLoadDeferred()) {
    // TODO(yuzus): Evict bfcache if necessary.
    Cancel();
    return;
  }

  is_downloading_to_blob_ = request.DownloadToBlob();

  SetDefersLoading(fetcher_->GetProperties().DeferType());

  if (ShouldFetchCodeCache()) {
    code_cache_request_ = std::make_unique<CodeCacheRequest>(
        fetcher_->CreateCodeCacheLoader(), request.Url(),
        fetcher_->GetProperties().DeferType());
  }

  if (is_cache_aware_loading_activated_) {
    // Override cache policy for cache-aware loading. If this request fails, a
    // reload with original request will be triggered in DidFail().
    ResourceRequestHead cache_aware_request(request);
    cache_aware_request.SetCacheMode(
        mojom::FetchCacheMode::kUnspecifiedOnlyIfCachedStrict);
    RequestAsynchronously(cache_aware_request);
    return;
  }

  if (resource_->Options().synchronous_policy == kRequestSynchronously) {
    RequestSynchronously(request);
  } else {
    RequestAsynchronously(request);
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

void ResourceLoader::Restart(const ResourceRequestHead& request) {
  CHECK_EQ(resource_->Options().synchronous_policy, kRequestAsynchronously);
  loader_ = fetcher_->CreateURLLoader(request, resource_->Options());
  task_runner_for_body_loader_ = loader_->GetTaskRunnerForBodyLoader();
  StartWith(request);
}

void ResourceLoader::SetDefersLoading(WebURLLoader::DeferType defers) {
  DCHECK(loader_);
  defers_ = defers;
  // If CodeCacheRequest handles this, then no need to handle here.
  if (code_cache_request_ && code_cache_request_->SetDefersLoading(defers))
    return;

  if (response_body_loader_) {
    if (defers != WebURLLoader::DeferType::kNotDeferred &&
        !response_body_loader_->IsSuspended()) {
      response_body_loader_->Suspend();
      if (defers == WebURLLoader::DeferType::kDeferredWithBackForwardCache) {
        response_body_loader_->EvictFromBackForwardCacheIfDrained();
      }
    }
    if (defers == WebURLLoader::DeferType::kNotDeferred &&
        response_body_loader_->IsSuspended()) {
      response_body_loader_->Resume();
    }
  }

  if (defers_handling_data_url_) {
    if (defers_ == WebURLLoader::DeferType::kNotDeferred) {
      defers_handling_data_url_ = false;
      GetLoadingTaskRunner()->PostTask(
          FROM_HERE,
          WTF::Bind(&ResourceLoader::HandleDataUrl, WrapWeakPersistent(this)));
    }
  }

  loader_->SetDefersLoading(defers);
  if (defers != WebURLLoader::DeferType::kNotDeferred) {
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
    cancel_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void ResourceLoader::CancelTimerFired(TimerBase*) {
  if (loader_ && !resource_->HasClientsOrObservers())
    Cancel();
}

void ResourceLoader::Cancel() {
  HandleError(
      ResourceError::CancelledError(resource_->LastResourceRequest().Url()));
}

bool ResourceLoader::IsLoading() const {
  return !!loader_;
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
    bool& report_raw_headers,
    std::vector<std::string>* removed_headers) {
  DCHECK(!passed_redirect_response.IsNull());
  if (removed_headers) {
    FindClientHintsToRemove(Context().GetFeaturePolicy(),
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

    // CanRequest() checks only enforced CSP, so check report-only here to
    // ensure that violations are sent.
    Context().CheckCSPForRequest(
        request_context, request_destination, new_url, options,
        reporting_disposition, url_before_redirects,
        ResourceRequest::RedirectStatus::kFollowedRedirect);

    base::Optional<ResourceRequestBlockedReason> blocked_reason =
        Context().CanRequest(resource_type, *new_request, new_url, options,
                             reporting_disposition,
                             new_request->GetRedirectInfo());

    if (Context().CalculateIfAdSubresource(*new_request, resource_type,
                                           options.initiator_info))
      new_request->SetIsAdResource();

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
    observer->WillSendRequest(resource_->InspectorId(), *new_request,
                              redirect_response, resource_->GetType(),
                              options.initiator_info);
  }

  // First-party cookie logic moved from DocumentLoader in Blink to
  // net::URLRequest in the browser. Assert that Blink didn't try to change it
  // to something else.
  DCHECK(new_request->SiteForCookies().IsEquivalent(new_site_for_cookies));

  // The following parameters never change during the lifetime of a request.
  DCHECK_EQ(new_request->GetRequestContext(), request_context);
  DCHECK_EQ(new_request->GetMode(), request_mode);
  DCHECK_EQ(new_request->GetCredentialsMode(), credentials_mode);

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

  report_raw_headers = new_request->ReportRawHeaders();
  return true;
}

void ResourceLoader::DidReceiveCachedMetadata(mojo_base::BigBuffer data) {
  DCHECK(!should_use_isolated_code_cache_);
  resource_->SetSerializedCachedMetadata(std::move(data));
}

blink::mojom::CodeCacheType ResourceLoader::GetCodeCacheType() const {
  const auto& request = resource_->GetResourceRequest();
  if (request.GetRequestDestination() ==
      network::mojom::RequestDestination::kEmpty) {
    // For requests initiated by the fetch function, we use code cache for
    // WASM compiled code.
    return mojom::blink::CodeCacheType::kWebAssembly;
  } else {
    // Otherwise, we use code cache for scripting.
    return mojom::blink::CodeCacheType::kJavascript;
  }
}

void ResourceLoader::SendCachedCodeToResource(mojo_base::BigBuffer data) {
  resource_->SetSerializedCachedMetadata(std::move(data));
}

void ResourceLoader::ClearCachedCode() {
  auto cache_type = GetCodeCacheType();
  Platform::Current()->ClearCodeCacheEntry(cache_type, resource_->Url());
}

void ResourceLoader::DidSendData(uint64_t bytes_sent,
                                 uint64_t total_bytes_to_be_sent) {
  resource_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

FetchContext& ResourceLoader::Context() const {
  return fetcher_->Context();
}

void ResourceLoader::DidReceiveResponse(const WebURLResponse& response) {
  DCHECK(!response.IsNull());
  DidReceiveResponseInternal(response.ToResourceResponse());
}

void ResourceLoader::DidReceiveResponseInternal(
    const ResourceResponse& response) {
  const ResourceRequestHead& request = resource_->GetResourceRequest();

  if (request.IsAutomaticUpgrade()) {
    mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> pending_recorder;
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        pending_recorder.InitWithNewPipeAndPassReceiver());
    auto recorder =
        std::make_unique<ukm::MojoUkmRecorder>(std::move(pending_recorder));
    LogMixedAutoupgradeMetrics(MixedContentAutoupgradeStatus::kResponseReceived,
                               response.HttpStatusCode(),
                               request.GetUkmSourceId(), recorder.get(),
                               resource_);
  }

  if (fetcher_->GetProperties().IsDetached()) {
    // If the fetch context is already detached, we don't need further signals,
    // so let's cancel the request.
    HandleError(ResourceError::CancelledError(response.CurrentRequestUrl()));
    return;
  }

  ResourceType resource_type = resource_->GetType();

  const ResourceRequestHead& initial_request = resource_->GetResourceRequest();
  // The following parameters never change during the lifetime of a request.
  mojom::blink::RequestContextType request_context =
      initial_request.GetRequestContext();
  network::mojom::RequestDestination request_destination =
      initial_request.GetRequestDestination();

  const ResourceLoaderOptions& options = resource_->Options();

  should_use_isolated_code_cache_ =
      ShouldUseIsolatedCodeCache(request_context, response);

  // Perform 'nosniff' checks against the original response instead of the 304
  // response for a successful revalidation.
  const ResourceResponse& nosniffed_response =
      (resource_->IsCacheValidator() && response.HttpStatusCode() == 304)
          ? resource_->GetResponse()
          : response;

  if (base::Optional<ResourceRequestBlockedReason> blocked_reason =
          CheckResponseNosniff(request_context, nosniffed_response)) {
    HandleError(ResourceError::CancelledDueToAccessCheckError(
        response.CurrentRequestUrl(), blocked_reason.value()));
    return;
  }

  // https://wicg.github.io/cross-origin-embedder-policy/#integration-html
  // TODO(crbug.com/1064920): Remove this once PlzDedicatedWorker ships.
  if (options.reject_coep_unsafe_none &&
      response.GetCrossOriginEmbedderPolicy() !=
          network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp &&
      !response.CurrentRequestUrl().ProtocolIsData() &&
      !response.CurrentRequestUrl().ProtocolIs("blob")) {
    DCHECK(!base::FeatureList::IsEnabled(features::kPlzDedicatedWorker));
    HandleError(ResourceError::BlockedByResponse(
        response.CurrentRequestUrl(), network::mojom::BlockedByResponseReason::
                                          kCoepFrameResourceNeedsCoepHeader));
    return;
  }

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
    const KURL& response_url = response.ResponseUrl();
    const base::Optional<ResourceRequest::RedirectInfo>&
        previous_redirect_info = request.GetRedirectInfo();
    const KURL& original_url = previous_redirect_info
                                   ? previous_redirect_info->original_url
                                   : request.Url();
    const ResourceRequest::RedirectInfo redirect_info(original_url,
                                                      request.Url());
    // CanRequest() below only checks enforced policies: check report-only
    // here to ensure violations are sent.
    Context().CheckCSPForRequest(
        request_context, request_destination, response_url, options,
        ReportingDisposition::kReport, original_url,
        ResourceRequest::RedirectStatus::kFollowedRedirect);

    base::Optional<ResourceRequestBlockedReason> blocked_reason =
        Context().CanRequest(resource_type, ResourceRequest(initial_request),
                             response_url, options,
                             ReportingDisposition::kReport, redirect_info);
    if (blocked_reason) {
      HandleError(ResourceError::CancelledDueToAccessCheckError(
          response_url, blocked_reason.value()));
      return;
    }
  }

  // A response should not serve partial content if it was not requested via a
  // Range header: https://fetch.spec.whatwg.org/#main-fetch
  if (response.GetType() == network::mojom::FetchResponseType::kOpaque &&
      response.HttpStatusCode() == 206 && response.HasRangeRequested() &&
      !initial_request.HttpHeaderFields().Contains(
          net::HttpRequestHeaders::kRange)) {
    HandleError(ResourceError::CancelledDueToAccessCheckError(
        response.CurrentRequestUrl(), ResourceRequestBlockedReason::kOther));
    return;
  }

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

  // Send the cached code after we notify that the response is received.
  // Resource expects that we receive the response first before the
  // corresponding cached code.
  if (code_cache_request_) {
    code_cache_request_->DidReceiveResponse(
        response.ResponseTime(), should_use_isolated_code_cache_, this);
  }

  if (auto* frame_or_worker_scheduler = fetcher_->GetFrameOrWorkerScheduler()) {
    if (response.CacheControlContainsNoCache()) {
      frame_or_worker_scheduler->RegisterStickyFeature(
          SchedulingPolicy::Feature::kSubresourceHasCacheControlNoCache,
          {SchedulingPolicy::RecordMetricsForBackForwardCache()});
    }
    if (response.CacheControlContainsNoStore()) {
      frame_or_worker_scheduler->RegisterStickyFeature(
          SchedulingPolicy::Feature::kSubresourceHasCacheControlNoStore,
          {SchedulingPolicy::RecordMetricsForBackForwardCache()});
    }
  }

  if (!resource_->Loader())
    return;

  if (response.HttpStatusCode() >= 400 &&
      !resource_->ShouldIgnoreHTTPStatusCodeErrors()) {
    HandleError(ResourceError::CancelledError(response.CurrentRequestUrl()));
    return;
  }
}

void ResourceLoader::DidStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  if (is_downloading_to_blob_) {
    DCHECK(!blob_response_started_);
    blob_response_started_ = true;

    const ResourceResponse& response = resource_->GetResponse();
    AtomicString mime_type = response.MimeType();

    // Callback is bound to a WeakPersistent, as ResourceLoader is kept alive by
    // ResourceFetcher as long as we still care about the result of the load.
    fetcher_->GetBlobRegistry()->RegisterFromStream(
        mime_type.IsNull() ? g_empty_string : mime_type.LowerASCII(), "",
        std::max(static_cast<int64_t>(0), response.ExpectedContentLength()),
        std::move(body),
        progress_receiver_.BindNewEndpointAndPassRemote(GetLoadingTaskRunner()),
        WTF::Bind(&ResourceLoader::FinishedCreatingBlob,
                  WrapWeakPersistent(this)));
    return;
  }

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;
  DidStartLoadingResponseBodyInternal(
      *MakeGarbageCollected<DataPipeBytesConsumer>(
          GetLoadingTaskRunner(), std::move(body), &completion_notifier));
  data_pipe_completion_notifier_ = completion_notifier;
}

void ResourceLoader::DidReceiveData(const char* data, int length) {
  CHECK_GE(length, 0);

  if (auto* observer = fetcher_->GetResourceLoadObserver()) {
    observer->DidReceiveData(resource_->InspectorId(),
                             base::make_span(data, length));
  }
  resource_->AppendData(data, length);
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
      "endData", EndResourceLoadData(RequestOutcome::kSuccess));

  fetcher_->HandleLoaderFinish(resource_.Get(), base::TimeTicks(),
                               ResourceFetcher::kDidFinishFirstPartInMultipart,
                               0, false);
}

void ResourceLoader::DidFinishLoading(base::TimeTicks response_end,
                                      int64_t encoded_data_length,
                                      int64_t encoded_body_length,
                                      int64_t decoded_body_length,
                                      bool should_report_corb_blocking) {
  resource_->SetEncodedDataLength(encoded_data_length);
  resource_->SetEncodedBodyLength(encoded_body_length);
  resource_->SetDecodedBodyLength(decoded_body_length);

  if ((response_body_loader_ && !has_seen_end_of_body_ &&
       !response_body_loader_->IsAborted()) ||
      (is_downloading_to_blob_ && !blob_finished_ && blob_response_started_)) {
    // If the body is still being loaded, we defer the completion until all the
    // body is received.
    deferred_finish_loading_info_ =
        DeferredFinishLoadingInfo{response_end, should_report_corb_blocking};

    if (data_pipe_completion_notifier_)
      data_pipe_completion_notifier_->SignalComplete();
    return;
  }

  Release(ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
          ResourceLoadScheduler::TrafficReportHints(encoded_data_length,
                                                    decoded_body_length));
  loader_.reset();
  code_cache_request_.reset();
  response_body_loader_ = nullptr;
  has_seen_end_of_body_ = false;
  deferred_finish_loading_info_ = base::nullopt;

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      TRACE_DISABLED_BY_DEFAULT("network"), "ResourceLoad",
      TRACE_ID_WITH_SCOPE("BlinkResourceID",
                          TRACE_ID_LOCAL(resource_->InspectorId())),
      "endData", EndResourceLoadData(RequestOutcome::kSuccess));

  fetcher_->HandleLoaderFinish(
      resource_.Get(), response_end, ResourceFetcher::kDidFinishLoading,
      inflight_keepalive_bytes_, should_report_corb_blocking);
}

void ResourceLoader::DidFail(const WebURLError& error,
                             int64_t encoded_data_length,
                             int64_t encoded_body_length,
                             int64_t decoded_body_length) {
  const ResourceRequestHead& request = resource_->GetResourceRequest();

  if (request.IsAutomaticUpgrade()) {
    mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> pending_recorder;
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        pending_recorder.InitWithNewPipeAndPassReceiver());
    auto recorder =
        std::make_unique<ukm::MojoUkmRecorder>(std::move(pending_recorder));
    LogMixedAutoupgradeMetrics(MixedContentAutoupgradeStatus::kFailed,
                               error.reason(), request.GetUkmSourceId(),
                               recorder.get(), resource_);
  }
  resource_->SetEncodedDataLength(encoded_data_length);
  resource_->SetEncodedBodyLength(encoded_body_length);
  resource_->SetDecodedBodyLength(decoded_body_length);
  HandleError(ResourceError(error));
}

void ResourceLoader::HandleError(const ResourceError& error) {
  if (response_body_loader_)
    response_body_loader_->Abort();

  if (data_pipe_completion_notifier_)
    data_pipe_completion_notifier_->SignalError(BytesConsumer::Error());

  if (is_cache_aware_loading_activated_ && error.IsCacheMiss() &&
      !fetcher_->GetProperties().ShouldBlockLoadingSubResource()) {
    resource_->WillReloadAfterDiskCacheMiss();
    is_cache_aware_loading_activated_ = false;
    Restart(resource_->GetResourceRequest());
    return;
  }
  if (error.CorsErrorStatus()) {
    fetcher_->GetConsoleLogger().AddConsoleMessage(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        cors::GetErrorString(
            *error.CorsErrorStatus(), resource_->GetResourceRequest().Url(),
            resource_->LastResourceRequest().Url(), *resource_->GetOrigin(),
            resource_->GetType(), resource_->Options().initiator_info.name));
  }

  Release(ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
          ResourceLoadScheduler::TrafficReportHints::InvalidInstance());
  loader_.reset();
  code_cache_request_.reset();
  response_body_loader_ = nullptr;
  has_seen_end_of_body_ = false;
  deferred_finish_loading_info_ = base::nullopt;

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      TRACE_DISABLED_BY_DEFAULT("network"), "ResourceLoad",
      TRACE_ID_WITH_SCOPE("BlinkResourceID",
                          TRACE_ID_LOCAL(resource_->InspectorId())),
      "endData", EndResourceLoadData(RequestOutcome::kFail));

  fetcher_->HandleLoaderError(resource_.Get(), error,
                              inflight_keepalive_bytes_);
}

void ResourceLoader::EvictFromBackForwardCache() {
  fetcher_->EvictFromBackForwardCache();
}

void ResourceLoader::RequestSynchronously(const ResourceRequestHead& request) {
  DCHECK(loader_);
  DCHECK_EQ(request.Priority(), ResourceLoadPriority::kHighest);

  auto network_resource_request = std::make_unique<network::ResourceRequest>();
  scoped_refptr<EncodedFormData> form_body = request_body_.FormBody();
  PopulateResourceRequest(request, std::move(request_body_),
                          network_resource_request.get());
  if (form_body)
    request_body_ = ResourceRequestBody(std::move(form_body));
  WebURLResponse response_out;
  base::Optional<WebURLError> error_out;
  WebData data_out;
  int64_t encoded_data_length = WebURLLoaderClient::kUnknownEncodedDataLength;
  int64_t encoded_body_length = 0;
  WebBlobInfo downloaded_blob;

  if (CanHandleDataURLRequestLocally(request)) {
    ResourceResponse response;
    scoped_refptr<SharedBuffer> data;
    int result;
    // It doesn't have to verify mime type again since it's allowed to handle
    // the data url with invalid mime type in some cases.
    // CanHandleDataURLRequestLocally() has already checked if the data url can
    // be handled here.
    std::tie(result, response, data) =
        network_utils::ParseDataURL(resource_->Url(), request.HttpMethod());
    if (result != net::OK) {
      error_out = WebURLError(result, resource_->Url());
    } else {
      response_out = WrappedResourceResponse(response);
      data_out = WebData(std::move(data));
    }
  } else {
    // Don't do mime sniffing for fetch (crbug.com/2016)
    bool no_mime_sniffing = request.GetRequestContext() ==
                            blink::mojom::blink::RequestContextType::FETCH;
    loader_->LoadSynchronously(
        std::move(network_resource_request), request.GetURLRequestExtraData(),
        request.RequestorID(), request.DownloadToBlob(), no_mime_sniffing,
        request.TimeoutInterval(), this, response_out, error_out, data_out,
        encoded_data_length, encoded_body_length, downloaded_blob,
        Context().CreateResourceLoadInfoNotifierWrapper());
  }
  // A message dispatched while synchronously fetching the resource
  // can bring about the cancellation of this load.
  if (!IsLoading())
    return;
  int64_t decoded_body_length = data_out.size();
  if (error_out) {
    DidFail(*error_out, encoded_data_length, encoded_body_length,
            decoded_body_length);
    return;
  }
  DidReceiveResponse(response_out);
  if (!IsLoading())
    return;
  DCHECK_GE(response_out.ToResourceResponse().EncodedBodyLength(), 0);

  // Follow the async case convention of not calling DidReceiveData or
  // appending data to m_resource if the response body is empty. Copying the
  // empty buffer is a noop in most cases, but is destructive in the case of
  // a 304, where it will overwrite the cached data we should be reusing.
  if (data_out.size()) {
    data_out.ForEachSegment([this](const char* segment, size_t segment_size,
                                   size_t segment_offset) {
      DidReceiveData(segment, SafeCast<int>(segment_size));
      return true;
    });
  }

  if (request.DownloadToBlob()) {
    auto blob = downloaded_blob.GetBlobHandle();
    if (blob)
      OnProgress(blob->size());
    FinishedCreatingBlob(blob);
  }
  DidFinishLoading(base::TimeTicks::Now(), encoded_data_length,
                   encoded_body_length, decoded_body_length, false);
}

void ResourceLoader::RequestAsynchronously(const ResourceRequestHead& request) {
  DCHECK(loader_);
  if (CanHandleDataURLRequestLocally(request)) {
    DCHECK(!code_cache_request_);
    // Handle DataURL in another task instead of using |loader_|.
    GetLoadingTaskRunner()->PostTask(
        FROM_HERE,
        WTF::Bind(&ResourceLoader::HandleDataUrl, WrapWeakPersistent(this)));
    return;
  }

  auto network_resource_request = std::make_unique<network::ResourceRequest>();
  // Don't do mime sniffing for fetch (crbug.com/2016)
  bool no_mime_sniffing = request.GetRequestContext() ==
                          blink::mojom::blink::RequestContextType::FETCH;
  scoped_refptr<EncodedFormData> form_body = request_body_.FormBody();
  PopulateResourceRequest(request, std::move(request_body_),
                          network_resource_request.get());
  if (form_body)
    request_body_ = ResourceRequestBody(std::move(form_body));
  loader_->LoadAsynchronously(
      std::move(network_resource_request), request.GetURLRequestExtraData(),
      request.RequestorID(), no_mime_sniffing,
      Context().CreateResourceLoadInfoNotifierWrapper(), this);
  if (code_cache_request_) {
    // Sets defers loading and initiates a fetch from code cache.
    code_cache_request_->FetchFromCodeCache(loader_.get(), this);
  }
}

void ResourceLoader::Dispose() {
  loader_ = nullptr;
  progress_receiver_.reset();
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
    const ResourceRequestHead& request) {
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
         resource_->GetResponse().IsNull() &&
         !base::FeatureList::IsEnabled(
             network::features::kDisableKeepaliveFetch);
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

  if (scheduler_client_id_ == ResourceLoadScheduler::kInvalidClientId)
    return;

  if (auto* observer = fetcher_->GetResourceLoadObserver()) {
    observer->DidReceiveData(resource_->InspectorId(),
                             base::make_span(static_cast<const char*>(nullptr),
                                             static_cast<size_t>(delta)));
  }
  resource_->DidDownloadData(delta);
}

void ResourceLoader::FinishedCreatingBlob(
    const scoped_refptr<BlobDataHandle>& blob) {
  DCHECK(!blob_finished_);

  if (scheduler_client_id_ == ResourceLoadScheduler::kInvalidClientId)
    return;

  if (auto* observer = fetcher_->GetResourceLoadObserver()) {
    observer->DidDownloadToBlob(resource_->InspectorId(), blob.get());
  }
  resource_->DidDownloadToBlob(blob);

  blob_finished_ = true;
  if (deferred_finish_loading_info_) {
    const ResourceResponse& response = resource_->GetResponse();
    DidFinishLoading(
        deferred_finish_loading_info_->response_end,
        response.EncodedDataLength(), response.EncodedBodyLength(),
        response.DecodedBodyLength(),
        deferred_finish_loading_info_->should_report_corb_blocking);
  }
}

base::Optional<ResourceRequestBlockedReason>
ResourceLoader::CheckResponseNosniff(
    mojom::blink::RequestContextType request_context,
    const ResourceResponse& response) {
  bool sniffing_allowed =
      ParseContentTypeOptionsHeader(response.HttpHeaderField(
          http_names::kXContentTypeOptions)) != kContentTypeOptionsNosniff;
  if (sniffing_allowed)
    return base::nullopt;

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

  return base::nullopt;
}

void ResourceLoader::HandleDataUrl() {
  if (!IsLoading())
    return;
  if (defers_ != WebURLLoader::DeferType::kNotDeferred) {
    defers_handling_data_url_ = true;
    return;
  }

  // Extract a ResourceResponse from the data url.
  ResourceResponse response;
  scoped_refptr<SharedBuffer> data;
  int result;
  // We doesn't have to verify mime type again since it's allowed to handle the
  // data url with invalid mime type in some cases.
  // CanHandleDataURLRequestLocally() has already checked if the data url can be
  // handled here.
  std::tie(result, response, data) = network_utils::ParseDataURL(
      resource_->Url(), resource_->GetResourceRequest().HttpMethod());
  if (result != net::OK) {
    HandleError(ResourceError(result, resource_->Url(), base::nullopt));
    return;
  }
  DCHECK(data);
  const size_t data_size = data->size();

  DidReceiveResponseInternal(response);
  if (!IsLoading())
    return;

  auto* bytes_consumer =
      MakeGarbageCollected<SharedBufferBytesConsumer>(std::move(data));
  DidStartLoadingResponseBodyInternal(*bytes_consumer);
  if (!IsLoading())
    return;

  // DidFinishLoading() may deferred until the response body loader reaches to
  // end.
  DidFinishLoading(base::TimeTicks::Now(), data_size, data_size, data_size,
                   false /* should_report_corb_blocking */);
}

}  // namespace blink
