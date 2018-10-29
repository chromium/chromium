/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
n * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_FETCH_CONTEXT_H_

#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/loader/base_fetch_context.h"
#include "third_party/blink/renderer/core/script/fetch_client_settings_object_impl.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ClientHintsPreferences;
class Document;
class DocumentLoader;
class LocalFrame;
class LocalFrameClient;
class ResourceError;
class ResourceResponse;
class Settings;
class WebContentSettingsClient;
struct WebEnabledClientHints;

class CORE_EXPORT FrameFetchContext final : public BaseFetchContext {
 public:
  static ResourceFetcher* CreateFetcherFromDocumentLoader(
      DocumentLoader* loader) {
    return CreateFetcher(loader, nullptr);
  }
  // Used for creating a FrameFetchContext for an imported Document.
  // |document_loader_| will be set to nullptr.
  static ResourceFetcher* CreateFetcherFromDocument(Document* document) {
    return CreateFetcher(nullptr, document);
  }

  static void ProvideDocumentToContext(FetchContext&, Document*);

  ~FrameFetchContext() override;

  bool IsFrameFetchContext() override { return true; }

  void RecordDataUriWithOctothorpe() override;

  void AddAdditionalRequestHeaders(ResourceRequest&,
                                   FetchResourceType) override;
  base::Optional<ResourceRequestBlockedReason> CanRequest(
      ResourceType type,
      const ResourceRequest& resource_request,
      const KURL& url,
      const ResourceLoaderOptions& options,
      SecurityViolationReportingPolicy reporting_policy,
      ResourceRequest::RedirectStatus redirect_status) const override;
  mojom::FetchCacheMode ResourceRequestCachePolicy(
      const ResourceRequest&,
      ResourceType,
      FetchParameters::DeferOption) const override;
  void DispatchDidChangeResourcePriority(unsigned long identifier,
                                         ResourceLoadPriority,
                                         int intra_priority_value) override;
  void PrepareRequest(ResourceRequest&, RedirectType) override;
  void DispatchWillSendRequest(
      unsigned long identifier,
      ResourceRequest&,
      const ResourceResponse& redirect_response,
      ResourceType,
      const FetchInitiatorInfo& = FetchInitiatorInfo()) override;
  void DispatchDidLoadResourceFromMemoryCache(unsigned long identifier,
                                              const ResourceRequest&,
                                              const ResourceResponse&) override;
  void DispatchDidReceiveResponse(unsigned long identifier,
                                  const ResourceResponse&,
                                  network::mojom::RequestContextFrameType,
                                  mojom::RequestContextType,
                                  Resource*,
                                  ResourceResponseType) override;
  void DispatchDidReceiveData(unsigned long identifier,
                              const char* data,
                              int data_length) override;
  void DispatchDidReceiveEncodedData(unsigned long identifier,
                                     int encoded_data_length) override;
  void DispatchDidDownloadToBlob(unsigned long identifier,
                                 BlobDataHandle*) override;
  void DispatchDidFinishLoading(unsigned long identifier,
                                TimeTicks finish_time,
                                int64_t encoded_data_length,
                                int64_t decoded_body_length,
                                bool should_report_corb_blocking) override;
  void DispatchDidFail(const KURL&,
                       unsigned long identifier,
                       const ResourceError&,
                       int64_t encoded_data_length,
                       bool is_internal_request) override;

  bool ShouldLoadNewResource(ResourceType) const override;
  void RecordLoadingActivity(const ResourceRequest&,
                             ResourceType,
                             const AtomicString& fetch_initiator_name) override;
  void DidLoadResource(Resource*) override;

  void AddResourceTiming(const ResourceTimingInfo&) override;
  bool AllowImage(bool images_enabled, const KURL&) const override;
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker()
      const override;
  int64_t ServiceWorkerID() const override;
  int ApplicationCacheHostID() const override;

  bool IsMainFrame() const override;
  bool DefersLoading() const override;
  bool IsLoadComplete() const override;
  bool UpdateTimingInfoForIFrameNavigation(ResourceTimingInfo*) override;

  const SecurityOrigin* GetSecurityOrigin() const override;

  void PopulateResourceRequest(ResourceType,
                               const ClientHintsPreferences&,
                               const FetchParameters::ResourceWidth&,
                               ResourceRequest&) override;

  // Exposed for testing.
  void ModifyRequestForCSP(ResourceRequest&);
  void AddClientHintsIfNecessary(const ClientHintsPreferences&,
                                 const FetchParameters::ResourceWidth&,
                                 ResourceRequest&);

  MHTMLArchive* Archive() const override;

  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest&,
      const ResourceLoaderOptions&) override;

  ResourceLoadScheduler::ThrottlingPolicy InitialLoadThrottlingPolicy()
      const override {
    // Frame loading should normally start with |kTight| throttling, as the
    // frame will be in layout-blocking state until the <body> tag is inserted.
    return ResourceLoadScheduler::ThrottlingPolicy::kTight;
  }

  bool IsDetached() const override { return frozen_state_; }

  FetchContext* Detach() override;

  void Trace(blink::Visitor*) override;

  ResourceLoadPriority ModifyPriorityForExperiments(
      ResourceLoadPriority) const override;
  void DispatchNetworkQuiet() override;

 private:
  friend class FrameFetchContextTest;

  struct FrozenState;

  static ResourceFetcher* CreateFetcher(DocumentLoader*, Document*);

  FrameFetchContext(DocumentLoader*, Document*);

  // Convenient accessors below can be used to transparently access the
  // relevant document loader or frame in either cases without null-checks.
  //
  // TODO(kinuko): Remove constness, these return non-const members.
  DocumentLoader* MasterDocumentLoader() const;
  LocalFrame* GetFrame() const;
  LocalFrameClient* GetLocalFrameClient() const;
  LocalFrame* FrameOfImportsController() const;

  // FetchContext overrides:
  FrameScheduler* GetFrameScheduler() const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetLoadingTaskRunner() override;
  std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
  CreateResourceLoadingTaskRunnerHandle() override;

  // BaseFetchContext overrides:
  const FetchClientSettingsObject* GetFetchClientSettingsObject()
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

  const KURL& Url() const override;
  const SecurityOrigin* GetParentSecurityOrigin() const override;
  base::Optional<mojom::IPAddressSpace> GetAddressSpace() const override;
  const ContentSecurityPolicy* GetContentSecurityPolicy() const override;
  void AddConsoleMessage(ConsoleMessage*) const override;

  WebContentSettingsClient* GetContentSettingsClient() const;
  Settings* GetSettings() const;
  String GetUserAgent() const;
  const ClientHintsPreferences GetClientHintsPreferences() const;
  float GetDevicePixelRatio() const;
  bool ShouldSendClientHint(mojom::WebClientHintsType,
                            const ClientHintsPreferences&,
                            const WebEnabledClientHints&) const;
  // Checks if the origin requested persisting the client hints, and notifies
  // the |WebContentSettingsClient| with the list of client hints and the
  // persistence duration.
  void ParseAndPersistClientHints(const ResourceResponse&);
  void SetFirstPartyCookie(ResourceRequest&);

  // Returns true if execution of scripts from the url are allowed. Compared to
  // AllowScriptFromSource(), this method does not generate any
  // notification to the |WebContentSettingsClient| that the execution of the
  // script was blocked. This method should be called only when there is a need
  // to check the settings, and where blocked setting doesn't really imply that
  // JavaScript was blocked from being executed.
  bool AllowScriptFromSourceWithoutNotifying(const KURL&) const;

  // Returns true if the origin of |url| is same as the origin of the top level
  // frame's main resource.
  bool IsFirstPartyOrigin(const KURL& url) const;

  Member<DocumentLoader> document_loader_;
  Member<Document> document_;

  // The value of |save_data_enabled_| is read once per frame from
  // NetworkStateNotifier, which is guarded by a mutex lock, and cached locally
  // here for performance.
  const bool save_data_enabled_;

  // Non-null only when detached.
  Member<const FrozenState> frozen_state_;

  Member<FetchClientSettingsObject> fetch_client_settings_object_;
};

}  // namespace blink

#endif
