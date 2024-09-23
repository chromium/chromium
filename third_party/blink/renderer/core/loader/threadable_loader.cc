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

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/cors.mojom-blink.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/loader_freeze_mode.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

// DetachedClient is a ThreadableLoaderClient for a "detached"
// ThreadableLoader. It's for fetch requests with keepalive set, so
// it keeps itself alive during loading.
class DetachedClient final : public GarbageCollected<DetachedClient>,
                             public ThreadableLoaderClient {
 public:
  explicit DetachedClient(ThreadableLoader* loader)
      : loader_(loader), detached_time_(base::TimeTicks::Now()) {}
  ~DetachedClient() override = default;

  void DidFinishLoading(uint64_t identifier) override {
    LogKeepAliveDuration();
    self_keep_alive_.Clear();
  }
  void DidFail(uint64_t identifier, const ResourceError&) override {
    LogKeepAliveDuration();
    self_keep_alive_.Clear();
  }
  void DidFailRedirectCheck(uint64_t identifier) override {
    LogKeepAliveDuration();
    self_keep_alive_.Clear();
  }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(loader_);
    ThreadableLoaderClient::Trace(visitor);
  }

 private:
  void LogKeepAliveDuration() {
    base::TimeDelta duration_after_detached =
        base::TimeTicks::Now() - detached_time_;
    // kKeepaliveLoadersTimeout > 10 sec, so UmaHistogramTimes can't be used.
    base::UmaHistogramMediumTimes("FetchKeepAlive.RequestOutliveDuration",
                                  duration_after_detached);
  }

  SelfKeepAlive<DetachedClient> self_keep_alive_{this};
  // Keep it alive.
  const Member<ThreadableLoader> loader_;
  base::TimeTicks detached_time_;
};

}  // namespace

ThreadableLoader::ThreadableLoader(
    ExecutionContext& execution_context,
    ThreadableLoaderClient* client,
    const ResourceLoaderOptions& resource_loader_options,
    ResourceFetcher* resource_fetcher)
    : resource_loader_options_(resource_loader_options),
      client_(client),
      execution_context_(execution_context),
      resource_fetcher_(resource_fetcher),
      request_mode_(network::mojom::RequestMode::kSameOrigin),
      timeout_timer_(execution_context_->GetTaskRunner(TaskType::kNetworking),
                     this,
                     &ThreadableLoader::DidTimeout) {
  DCHECK(client);
  if (!resource_fetcher_) {
    resource_fetcher_ = execution_context_->Fetcher();
  }
}

void ThreadableLoader::Start(ResourceRequest request) {
  const auto request_context = request.GetRequestContext();
  if (request.GetMode() == network::mojom::RequestMode::kNoCors) {
    SECURITY_CHECK(cors::IsNoCorsAllowedContext(request_context));
  }

  // Setting an outgoing referer is only supported in the async code path.
  DCHECK(resource_loader_options_.synchronous_policy ==
             kRequestAsynchronously ||
         request.ReferrerString() == Referrer::ClientReferrerString());

  // kPreventPreflight can be used only when the CORS is enabled.
  DCHECK(request.CorsPreflightPolicy() ==
             network::mojom::CorsPreflightPolicy::kConsiderPreflight ||
         cors::IsCorsEnabledRequestMode(request.GetMode()));

  request_started_ = base::TimeTicks::Now();
  request_mode_ = request.GetMode();

  // Set the service worker mode to none if "bypass for network" in DevTools is
  // enabled.
  bool should_bypass_service_worker = false;
  probe::ShouldBypassServiceWorker(execution_context_,
                                   &should_bypass_service_worker);
  if (should_bypass_service_worker)
    request.SetSkipServiceWorker(true);

  const bool async =
      resource_loader_options_.synchronous_policy == kRequestAsynchronously;
  if (!timeout_.is_zero()) {
    if (!async) {
      request.SetTimeoutInterval(timeout_);
    } else if (!timeout_timer_.IsActive()) {
      timeout_timer_.StartOneShot(timeout_, FROM_HERE);
    }
  }

  FetchParameters params(std::move(request), resource_loader_options_);
  DCHECK(!GetResource());

  checker_.WillAddClient();
  if (request_context == mojom::blink::RequestContextType::VIDEO ||
      request_context == mojom::blink::RequestContextType::AUDIO) {
    DCHECK(async);
    RawResource::FetchMedia(params, resource_fetcher_, this);
  } else if (request_context == mojom::blink::RequestContextType::MANIFEST) {
    DCHECK(async);
    RawResource::FetchManifest(params, resource_fetcher_, this);
  } else if (async) {
    RawResource::Fetch(params, resource_fetcher_, this);
  } else {
    RawResource::FetchSynchronously(params, resource_fetcher_, this);
  }
}

ThreadableLoader::~ThreadableLoader() = default;

void ThreadableLoader::SetTimeout(const base::TimeDelta& timeout) {
  timeout_ = timeout;

  // |request_started_| <= base::TimeTicks() indicates loading is either not yet
  // started or is already finished, and thus we don't need to do anything with
  // timeout_timer_.
  if (request_started_ <= base::TimeTicks()) {
    DCHECK(!timeout_timer_.IsActive());
    return;
  }
  DCHECK_EQ(kRequestAsynchronously,
            resource_loader_options_.synchronous_policy);
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
  client_ = MakeGarbageCollected<DetachedClient>(this);
}

void ThreadableLoader::SetDefersLoading(bool value) {
  if (GetResource() && GetResource()->Loader()) {
    GetResource()->Loader()->SetDefersLoading(value ? LoaderFreezeMode::kStrict
                                                    : LoaderFreezeMode::kNone);
  }
}

void ThreadableLoader::Clear() {
  client_ = nullptr;
  timeout_timer_.Stop();
  request_started_ = base::TimeTicks();
  if (GetResource())
    checker_.WillRemoveClient();
  ClearResource();
}

bool ThreadableLoader::RedirectReceived(
    Resource* resource,
    const ResourceRequest& new_request,
    const ResourceResponse& redirect_response) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());
  checker_.RedirectReceived();

  return client_->WillFollowRedirect(resource->InspectorId(), new_request.Url(),
                                     redirect_response);
}

void ThreadableLoader::RedirectBlocked() {
  DCHECK(client_);
  checker_.RedirectBlocked();

  // Tells the client that a redirect was received but not followed (for an
  // unknown reason).
  ThreadableLoaderClient* client = client_;
  Clear();
  uint64_t identifier = 0;  // We don't have an inspector id here.
  client->DidFailRedirectCheck(identifier);
}

void ThreadableLoader::DataSent(Resource* resource,
                                uint64_t bytes_sent,
                                uint64_t total_bytes_to_be_sent) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());
  DCHECK_EQ(kRequestAsynchronously,
            resource_loader_options_.synchronous_policy);

  checker_.DataSent();
  client_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

void ThreadableLoader::DataDownloaded(Resource* resource,
                                      uint64_t data_length) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());

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

void ThreadableLoader::ResponseReceived(Resource* resource,
                                        const ResourceResponse& response) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());

  checker_.ResponseReceived();

  // If "Cache-Control: no-store" header exists in the XHR response,
  // Back/Forward cache will be disabled for the page if the main resource has
  // "Cache-Control: no-store" as well.
  if (response.CacheControlContainsNoStore()) {
    execution_context_->GetScheduler()->RegisterStickyFeature(
        SchedulingPolicy::Feature::
            kJsNetworkRequestReceivedCacheControlNoStoreResource,
        {SchedulingPolicy::DisableBackForwardCache()});
  }

  client_->DidReceiveResponse(resource->InspectorId(), response);
}

void ThreadableLoader::ResponseBodyReceived(Resource* resource,
                                            BytesConsumer& body) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());

  checker_.ResponseBodyReceived();
  client_->DidStartLoadingResponseBody(body);
}

void ThreadableLoader::CachedMetadataReceived(
    Resource* resource,
    mojo_base::BigBuffer cached_metadata) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());

  checker_.SetSerializedCachedMetadata();

  client_->DidReceiveCachedMetadata(std::move(cached_metadata));
}

void ThreadableLoader::DataReceived(Resource* resource,
                                    base::span<const char> data) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());

  checker_.DataReceived();

  client_->DidReceiveData(data);
}

void ThreadableLoader::NotifyFinished(Resource* resource) {
  DCHECK(client_);
  DCHECK_EQ(resource, GetResource());

  checker_.NotifyFinished(resource);

  if (resource->ErrorOccurred()) {
    DispatchDidFail(resource->GetResourceError());
    return;
  }

  ThreadableLoaderClient* client = client_;
  // Protect the resource in |DidFinishLoading| in order not to release the
  // downloaded file.
  Persistent<Resource> protect = GetResource();
  Clear();
  client->DidFinishLoading(resource->InspectorId());
}

void ThreadableLoader::DidTimeout(TimerBase* timer) {
  DCHECK_EQ(kRequestAsynchronously,
            resource_loader_options_.synchronous_policy);
  DCHECK_EQ(timer, &timeout_timer_);
  // ClearResource() may be called in Clear() and some other places. Clear()
  // calls Stop() on |timeout_|. In the other places, the resource is set
  // again. If the creation fails, Clear() is called. So, here, GetResource() is
  // always non-nullptr.
  DCHECK(GetResource());
  // When |client_| is set to nullptr only in Clear() where |timeout_|
  // is stopped. So, |client_| is always non-nullptr here.
  DCHECK(client_);

  DispatchDidFail(ResourceError::TimeoutError(GetResource()->Url()));
}

void ThreadableLoader::DispatchDidFail(const ResourceError& error) {
  Resource* resource = GetResource();
  if (resource)
    resource->SetResponseType(network::mojom::FetchResponseType::kError);
  ThreadableLoaderClient* client = client_;
  Clear();
  client->DidFail(resource->InspectorId(), error);
}

void ThreadableLoader::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(client_);
  visitor->Trace(resource_fetcher_);
  visitor->Trace(timeout_timer_);
  visitor->Trace(resource_loader_options_);
  RawResourceClient::Trace(visitor);
}

scoped_refptr<base::SingleThreadTaskRunner> ThreadableLoader::GetTaskRunner() {
  return execution_context_->GetTaskRunner(TaskType::kNetworking);
}

}  // namespace blink
