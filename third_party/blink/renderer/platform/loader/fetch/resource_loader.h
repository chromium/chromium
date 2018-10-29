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
#include "base/gtest_prod_util.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class FetchContext;
class ResourceError;
class ResourceFetcher;

// A ResourceLoader is created for each Resource by the ResourceFetcher when it
// needs to load the specified resource. A ResourceLoader creates a
// WebURLLoader and loads the resource using it. Any per-load logic should be
// implemented in this class basically.
class PLATFORM_EXPORT ResourceLoader final
    : public GarbageCollectedFinalized<ResourceLoader>,
      public ResourceLoadSchedulerClient,
      protected WebURLLoaderClient,
      protected mojom::blink::ProgressClient {
  USING_GARBAGE_COLLECTED_MIXIN(ResourceLoader);
  USING_PRE_FINALIZER(ResourceLoader, Dispose);

 public:
  static ResourceLoader* Create(ResourceFetcher*,
                                ResourceLoadScheduler*,
                                Resource*,
                                uint32_t inflight_keepalive_bytes = 0);
  ~ResourceLoader() override;
  void Trace(blink::Visitor*) override;

  void Start();

  void ScheduleCancel();
  void Cancel();

  void SetDefersLoading(bool);

  void DidChangePriority(ResourceLoadPriority, int intra_priority_value);

  // Called before start() to activate cache-aware loading if enabled in
  // |m_resource->options()| and applicable.
  void ActivateCacheAwareLoadingIfNeeded(const ResourceRequest&);

  bool IsCacheAwareLoadingActivated() const {
    return is_cache_aware_loading_activated_;
  }

  ResourceFetcher* Fetcher() { return fetcher_; }
  bool ShouldBeKeptAliveWhenDetached() const;

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
                          const WebURL& new_site_for_cookies,
                          const WebString& new_referrer,
                          network::mojom::ReferrerPolicy new_referrer_policy,
                          const WebString& new_method,
                          const WebURLResponse& passed_redirect_response,
                          bool& report_raw_headers) override;
  void DidSendData(unsigned long long bytes_sent,
                   unsigned long long total_bytes_to_be_sent) override;
  void DidReceiveResponse(const WebURLResponse&) override;
  void DidReceiveResponse(const WebURLResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void DidReceiveCachedMetadata(const char* data, int length) override;
  void DidReceiveData(const char*, int) override;
  void DidReceiveTransferSizeUpdate(int transfer_size_diff) override;
  void DidStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void DidFinishLoading(
      TimeTicks finish_time,
      int64_t encoded_data_length,
      int64_t encoded_body_length,
      int64_t decoded_body_length,
      bool should_report_corb_blocking,
      const std::vector<network::cors::PreflightTimingInfo>&) override;
  void DidFail(const WebURLError&,
               int64_t encoded_data_length,
               int64_t encoded_body_length,
               int64_t decoded_body_length) override;

  blink::mojom::CodeCacheType GetCodeCacheType() const;
  void SendCachedCodeToResource(const char* data, int size);
  void ClearCachedCode();

  void HandleError(const ResourceError&);

  void DidFinishLoadingFirstPartInMultipart();

  // ResourceLoadSchedulerClient.
  void Run() override;

  scoped_refptr<base::SingleThreadTaskRunner> GetLoadingTaskRunner();

 private:
  friend class SubresourceIntegrityTest;
  class CodeCacheRequest;

  // Assumes ResourceFetcher and Resource are non-null.
  ResourceLoader(ResourceFetcher*,
                 ResourceLoadScheduler*,
                 Resource*,
                 uint32_t inflight_keepalive_bytes);

  bool ShouldFetchCodeCache();
  void StartWith(const ResourceRequest&);

  void Release(ResourceLoadScheduler::ReleaseOption,
               const ResourceLoadScheduler::TrafficReportHints&);

  // This method is currently only used for service worker fallback request and
  // cache-aware loading, other users should be careful not to break
  // ResourceLoader state.
  void Restart(const ResourceRequest&);

  FetchContext& Context() const;

  void CancelForRedirectAccessCheckError(const KURL&,
                                         ResourceRequestBlockedReason);
  void RequestSynchronously(const ResourceRequest&);
  void Dispose();

  void CancelTimerFired(TimerBase*);

  void OnProgress(uint64_t delta) override;
  void FinishedCreatingBlob(const scoped_refptr<BlobDataHandle>&);

  bool GetCORSFlag() const { return resource_->Options().cors_flag; }

  base::Optional<ResourceRequestBlockedReason> CheckResponseNosniff(
      mojom::RequestContextType,
      const ResourceResponse&) const;

  bool ShouldCheckCORSInResourceLoader() const;

  std::unique_ptr<WebURLLoader> loader_;
  ResourceLoadScheduler::ClientId scheduler_client_id_;
  Member<ResourceFetcher> fetcher_;
  Member<ResourceLoadScheduler> scheduler_;
  Member<Resource> resource_;
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
  mojo::AssociatedBinding<mojom::blink::ProgressClient> progress_binding_;
  bool blob_finished_ = false;
  bool blob_response_started_ = false;
  // If DidFinishLoading is called while downloading to a blob before the blob
  // is finished, we might have to defer actually handling the event. This
  // struct is used to store the information needed to refire DidFinishLoading
  // when the blob is finished too.
  struct DeferedFinishLoadingInfo {
    TimeTicks finish_time;
    bool should_report_corb_blocking;
  };
  base::Optional<DeferedFinishLoadingInfo> load_did_finish_before_blob_;

  TaskRunnerTimer<ResourceLoader> cancel_timer_;
};

}  // namespace blink

#endif
