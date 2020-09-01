/*
 * Copyright (C) 2005, 2006, 2011 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_LOADER_H_

#include <memory>
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/network/public/mojom/fetch_api.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader_client.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class FetchContext;
class ResourceError;
class ResourceFetcher;
class ResponseBodyLoader;

// A ResourceLoader is created for each Resource by the ResourceFetcher when it
// needs to load the specified resource. A ResourceLoader creates a
// WebURLLoader and loads the resource using it. Any per-load logic should be
// implemented in this class basically.
class PLATFORM_EXPORT ResourceLoader final
    : public GarbageCollected<ResourceLoader>,
      public ResourceLoadSchedulerClient,
      protected WebURLLoaderClient,
      protected mojom::blink::ProgressClient,
      private ResponseBodyLoaderClient {
  USING_PRE_FINALIZER(ResourceLoader, Dispose);

 public:
  // Assumes ResourceFetcher and Resource are non-null.
  ResourceLoader(ResourceFetcher*,
                 ResourceLoadScheduler*,
                 Resource*,
                 ResourceRequestBody request_body = ResourceRequestBody(),
                 uint32_t inflight_keepalive_bytes = 0);
  ~ResourceLoader() override;
  void Trace(Visitor*) const override;

  void Start();

  void ScheduleCancel();
  void Cancel();

  void SetDefersLoading(bool);

  void DidChangePriority(ResourceLoadPriority, int intra_priority_value);

  // Called before start() to activate cache-aware loading if enabled in
  // |m_resource->options()| and applicable.
  void ActivateCacheAwareLoadingIfNeeded(const ResourceRequestHead&);

  bool IsCacheAwareLoadingActivated() const {
    return is_cache_aware_loading_activated_;
  }

  ResourceFetcher* Fetcher() { return fetcher_; }
  bool ShouldBeKeptAliveWhenDetached() const;

  void AbortResponseBodyLoading();

  // WebURLLoaderClient
  //
  // A succesful load will consist of:
  // 0+  WillFollowRedirect()
  // 0+  DidSendData()
  // 1   DidReceiveResponse()
  // 0-1 DidReceiveCachedMetadata()
  // 0+  DidReceiveData() or DidDownloadData(), but never both
  // 1   DidFinishLoading()
  // A failed load is indicated by 1 DidFail(), which can occur at any time
  // before DidFinishLoading(), including synchronous inside one of the other
  // callbacks via ResourceLoader::cancel()
  bool WillFollowRedirect(const WebURL& new_url,
                          const net::SiteForCookies& new_site_for_cookies,
                          const WebString& new_referrer,
                          network::mojom::ReferrerPolicy new_referrer_policy,
                          const WebString& new_method,
                          const WebURLResponse& passed_redirect_response,
                          bool& report_raw_headers,
                          std::vector<std::string>* removed_headers) override;
  void DidSendData(uint64_t bytes_sent,
                   uint64_t total_bytes_to_be_sent) override;
  void DidReceiveResponse(const WebURLResponse&) override;
  void DidReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void DidReceiveData(const char*, int) override;
  void DidReceiveTransferSizeUpdate(int transfer_size_diff) override;
  void DidStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void DidFinishLoading(base::TimeTicks response_end,
                        int64_t encoded_data_length,
                        int64_t encoded_body_length,
                        int64_t decoded_body_length,
                        bool should_report_corb_blocking) override;
  void DidFail(const WebURLError&,
               int64_t encoded_data_length,
               int64_t encoded_body_length,
               int64_t decoded_body_length) override;

  blink::mojom::CodeCacheType GetCodeCacheType() const;
  void SendCachedCodeToResource(mojo_base::BigBuffer data);
  void ClearCachedCode();

  void HandleError(const ResourceError&);

  void DidFinishLoadingFirstPartInMultipart();

  scoped_refptr<base::SingleThreadTaskRunner> GetLoadingTaskRunner();

 private:
  friend class SubresourceIntegrityTest;
  friend class ResourceLoaderIsolatedCodeCacheTest;
  class CodeCacheRequest;

  void DidStartLoadingResponseBodyInternal(BytesConsumer& bytes_consumer);

  // ResourceLoadSchedulerClient.
  void Run() override;

  // ResponseBodyLoaderClient implementation.
  void DidReceiveData(base::span<const char> data) override;
  void DidFinishLoadingBody() override;
  void DidFailLoadingBody() override;
  void DidCancelLoadingBody() override;

  bool ShouldFetchCodeCache();
  void StartWith(const ResourceRequestHead&);

  void Release(ResourceLoadScheduler::ReleaseOption,
               const ResourceLoadScheduler::TrafficReportHints&);

  // This method is currently only used for service worker fallback request and
  // cache-aware loading, other users should be careful not to break
  // ResourceLoader state.
  void Restart(const ResourceRequestHead&);

  FetchContext& Context() const;

  // Returns true during resource load is happening. Methods as
  // a WebURLLoaderClient should not be invoked if this returns false.
  bool IsLoading() const;

  void CancelForRedirectAccessCheckError(const KURL&,
                                         ResourceRequestBlockedReason);
  void RequestSynchronously(const ResourceRequestHead&);
  void RequestAsynchronously(const ResourceRequestHead&);
  void Dispose();

  void DidReceiveResponseInternal(const ResourceResponse&);

  void CancelTimerFired(TimerBase*);

  void OnProgress(uint64_t delta) override;
  void FinishedCreatingBlob(const scoped_refptr<BlobDataHandle>&);

  base::Optional<ResourceRequestBlockedReason> CheckResponseNosniff(
      mojom::RequestContextType,
      const ResourceResponse&);

  // Processes Data URL in ResourceLoader instead of using |loader_|.
  void HandleDataUrl();

  std::unique_ptr<WebURLLoader> loader_;
  ResourceLoadScheduler::ClientId scheduler_client_id_;
  Member<ResourceFetcher> fetcher_;
  Member<ResourceLoadScheduler> scheduler_;
  Member<Resource> resource_;
  ResourceRequestBody request_body_;
  Member<ResponseBodyLoader> response_body_loader_;
  Member<DataPipeBytesConsumer::CompletionNotifier>
      data_pipe_completion_notifier_;
  // code_cache_request_ is created only if required. It is required to check
  // if it is valid before using it.
  std::unique_ptr<CodeCacheRequest> code_cache_request_;

  // https://fetch.spec.whatwg.org/#concept-request-response-tainting
  network::mojom::FetchResponseType response_tainting_ =
      network::mojom::FetchResponseType::kBasic;
  uint32_t inflight_keepalive_bytes_;
  bool is_cache_aware_loading_activated_;

  bool should_use_isolated_code_cache_ = false;
  bool is_downloading_to_blob_ = false;
  mojo::AssociatedReceiver<mojom::blink::ProgressClient> progress_receiver_{
      this};
  bool blob_finished_ = false;
  bool blob_response_started_ = false;
  bool has_seen_end_of_body_ = false;
  // If DidFinishLoading is called while downloading to a blob before the blob
  // is finished, we might have to defer actually handling the event. This
  // struct is used to store the information needed to refire DidFinishLoading
  // when the blob is finished too.
  struct DeferredFinishLoadingInfo {
    base::TimeTicks response_end;
    bool should_report_corb_blocking;
  };
  base::Optional<DeferredFinishLoadingInfo> deferred_finish_loading_info_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_body_loader_;

  // True if loading is deferred.
  bool defers_ = false;
  // True if the next call of SetDefersLoading(false) needs to invoke
  // HandleDataURL().
  bool defers_handling_data_url_ = false;

  TaskRunnerTimer<ResourceLoader> cancel_timer_;

  FrameScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;
};

}  // namespace blink

#endif
