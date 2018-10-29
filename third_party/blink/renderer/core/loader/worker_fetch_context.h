// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_FETCH_CONTEXT_H_

#include <memory>
#include "base/single_thread_task_runner.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/base_fetch_context.h"
#include "third_party/blink/renderer/core/script/fetch_client_settings_object_impl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class Resource;
class SubresourceFilter;
class WebURLLoader;
class WebURLLoaderFactory;
class WebWorkerFetchContext;
class WorkerClients;
class WorkerContentSettingsClient;
class WorkerSettings;
class WorkerOrWorkletGlobalScope;
enum class ResourceType : uint8_t;

CORE_EXPORT void ProvideWorkerFetchContextToWorker(
    WorkerClients*,
    std::unique_ptr<WebWorkerFetchContext>);

// The WorkerFetchContext is a FetchContext for workers (dedicated, shared and
// service workers) and threaded worklets (animation and audio worklets).
class WorkerFetchContext final : public BaseFetchContext {
 public:
  static WorkerFetchContext* Create(WorkerOrWorkletGlobalScope&);
  ~WorkerFetchContext() override;

  // BaseFetchContext implementation:
  const FetchClientSettingsObjectImpl* GetFetchClientSettingsObject()
      const override;
  KURL GetSiteForCookies() const override;
  SubresourceFilter* GetSubresourceFilter() const override;
  PreviewsResourceLoadingHints* GetPreviewsResourceLoadingHints()
      const override;
  bool AllowScriptFromSource(const KURL&) const override;
  bool ShouldBlockRequestByInspector(const KURL&) const override;
  void DispatchDidBlockRequest(const ResourceRequest&,
                               const FetchInitiatorInfo&,
                               ResourceRequestBlockedReason,
                               ResourceType) const override;
  bool ShouldBypassMainWorldCSP() const override;
  bool IsSVGImageChromeClient() const override;
  void CountUsage(WebFeature) const override;
  void CountDeprecation(WebFeature) const override;
  bool ShouldBlockWebSocketByMixedContentCheck(const KURL&) const override;
  std::unique_ptr<WebSocketHandshakeThrottle> CreateWebSocketHandshakeThrottle()
      override;
  bool ShouldBlockFetchByMixedContentCheck(
      mojom::RequestContextType,
      network::mojom::RequestContextFrameType,
      ResourceRequest::RedirectStatus,
      const KURL&,
      SecurityViolationReportingPolicy) const override;
  bool ShouldBlockFetchAsCredentialedSubresource(const ResourceRequest&,
                                                 const KURL&) const override;
  bool ShouldLoadNewResource(ResourceType) const override { return true; }
  const KURL& Url() const override;
  const SecurityOrigin* GetParentSecurityOrigin() const override;
  base::Optional<mojom::IPAddressSpace> GetAddressSpace() const override;
  const ContentSecurityPolicy* GetContentSecurityPolicy() const override;
  void AddConsoleMessage(ConsoleMessage*) const override;

  // FetchContext implementation:
  const SecurityOrigin* GetSecurityOrigin() const override;
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest&,
      const ResourceLoaderOptions&) override;
  std::unique_ptr<CodeCacheLoader> CreateCodeCacheLoader() override;
  void PrepareRequest(ResourceRequest&, RedirectType) override;
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker()
      const override;
  int ApplicationCacheHostID() const override;
  void AddAdditionalRequestHeaders(ResourceRequest&,
                                   FetchResourceType) override;
  void DispatchWillSendRequest(unsigned long,
                               ResourceRequest&,
                               const ResourceResponse&,
                               ResourceType,
                               const FetchInitiatorInfo&) override;
  void DispatchDidReceiveResponse(unsigned long identifier,
                                  const ResourceResponse&,
                                  network::mojom::RequestContextFrameType,
                                  mojom::RequestContextType,
                                  Resource*,
                                  ResourceResponseType) override;
  void DispatchDidReceiveData(unsigned long identifier,
                              const char* data,
                              int dataLength) override;
  void DispatchDidReceiveEncodedData(unsigned long identifier,
                                     int encoded_data_length) override;
  void DispatchDidFinishLoading(unsigned long identifier,
                                TimeTicks finish_time,
                                int64_t encoded_data_length,
                                int64_t decoded_body_length,
                                bool should_report_corb_blocking) override;
  void DispatchDidFail(const KURL&,
                       unsigned long identifier,
                       const ResourceError&,
                       int64_t encoded_data_length,
                       bool isInternalRequest) override;
  void AddResourceTiming(const ResourceTimingInfo&) override;
  void PopulateResourceRequest(ResourceType,
                               const ClientHintsPreferences&,
                               const FetchParameters::ResourceWidth&,
                               ResourceRequest&) override;
  bool DefersLoading() const override;

  std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
  CreateResourceLoadingTaskRunnerHandle() override;

  SecurityContext& GetSecurityContext() const;
  WorkerSettings* GetWorkerSettings() const;
  WorkerContentSettingsClient* GetWorkerContentSettingsClient() const;
  WebWorkerFetchContext* GetWebWorkerFetchContext() const {
    return web_context_.get();
  }

  void Trace(blink::Visitor*) override;

 private:
  WorkerFetchContext(WorkerOrWorkletGlobalScope&,
                     std::unique_ptr<WebWorkerFetchContext>);

  void SetFirstPartyCookie(ResourceRequest&);

  const Member<WorkerOrWorkletGlobalScope> global_scope_;
  const std::unique_ptr<WebWorkerFetchContext> web_context_;

  // Responsible for regular loads from the worker (i.e., Fetch API).
  std::unique_ptr<WebURLLoaderFactory> url_loader_factory_;

  // Responsible for handling script loads in certian situations (i.e.,
  // script import from service workers, which invole special processing
  // to persist the script in storage). May be null, fallback to
  // url_loader_factory_ in that case.
  std::unique_ptr<WebURLLoaderFactory> script_loader_factory_;

  Member<SubresourceFilter> subresource_filter_;

  const Member<FetchClientSettingsObjectImpl> fetch_client_settings_object_;

  // The value of |save_data_enabled_| is read once per frame from
  // NetworkStateNotifier, which is guarded by a mutex lock, and cached locally
  // here for performance.
  const bool save_data_enabled_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_FETCH_CONTEXT_H_
