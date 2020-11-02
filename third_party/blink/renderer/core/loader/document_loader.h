/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_DOCUMENT_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_DOCUMENT_LOADER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/feature_policy/document_policy.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/mojom/loader/content_security_notifier.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/mhtml_load_result.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/page_state/page_state.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/worker_timing_container.mojom-blink-forward.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_origin_policy.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/weak_identifier_map.h"
#include "third_party/blink/renderer/core/feature_policy/policy_helper.h"
#include "third_party/blink/renderer/core/frame/dactyloscoper.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/use_counter_helper.h"
#include "third_party/blink/renderer/core/html/parser/parser_synchronization_policy.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/source_keyed_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
namespace base {
class TickClock;
}

namespace blink {

class ApplicationCacheHostForFrame;
class ContentSecurityPolicy;
class Document;
class DocumentParser;
class FrameLoader;
class HistoryItem;
class LocalDOMWindow;
class LocalFrame;
class LocalFrameClient;
class MHTMLArchive;
class PrefetchedSignedExchangeManager;
class ResourceTimingInfo;
class SerializedScriptValue;
class SubresourceFilter;
class WebServiceWorkerNetworkProvider;

namespace mojom {
enum class CommitResult : int32_t;
}

// The DocumentLoader fetches a main resource and handles the result.
// TODO(https://crbug.com/855189). This was originally structured to have a
// provisional load, then commit but that is no longer necessary and this class
// can be simplified.
class CORE_EXPORT DocumentLoader : public GarbageCollected<DocumentLoader>,
                                   public UseCounter,
                                   public WebNavigationBodyLoader::Client {
 public:
  DocumentLoader(LocalFrame*,
                 WebNavigationType navigation_type,
                 ContentSecurityPolicy* content_security_policy,
                 std::unique_ptr<WebNavigationParams> navigation_params);
  ~DocumentLoader() override;

  static bool WillLoadUrlAsEmpty(const KURL&);

  LocalFrame* GetFrame() const { return frame_; }

  ResourceTimingInfo* GetNavigationTimingInfo() const;

  virtual void DetachFromFrame(bool flush_microtask_queue);

  uint64_t MainResourceIdentifier() const;

  const AtomicString& MimeType() const;

  const KURL& OriginalUrl() const;
  const Referrer& OriginalReferrer() const;

  MHTMLArchive* Archive() const { return archive_.Get(); }

  void SetSubresourceFilter(SubresourceFilter*);
  SubresourceFilter* GetSubresourceFilter() const {
    return subresource_filter_.Get();
  }
  void SetPreviewsResourceLoadingHints(
      PreviewsResourceLoadingHints* resource_loading_hints) {
    resource_loading_hints_ = resource_loading_hints;
  }
  PreviewsResourceLoadingHints* GetPreviewsResourceLoadingHints() const {
    return resource_loading_hints_;
  }

  const KURL& Url() const;
  const KURL& UrlForHistory() const;
  const AtomicString& HttpMethod() const;
  const Referrer& GetReferrer() const;
  const KURL& UnreachableURL() const;
  const base::Optional<blink::mojom::FetchCacheMode>& ForceFetchCacheMode()
      const;

  void DidChangePerformanceTiming();
  void DidObserveInputDelay(base::TimeDelta input_delay);
  void DidObserveLoadingBehavior(LoadingBehaviorFlag);
  void UpdateForSameDocumentNavigation(const KURL&,
                                       SameDocumentNavigationSource,
                                       scoped_refptr<SerializedScriptValue>,
                                       mojom::blink::ScrollRestorationType,
                                       WebFrameLoadType,
                                       bool is_content_initiated);
  const ResourceResponse& GetResponse() const { return response_; }
  bool IsClientRedirect() const { return is_client_redirect_; }
  bool ReplacesCurrentHistoryItem() const {
    return replaces_current_history_item_;
  }

  bool IsCommittedButEmpty() const {
    return state_ >= kCommitted && !data_received_;
  }

  void SetSentDidFinishLoad() { state_ = kSentDidFinishLoad; }
  bool SentDidFinishLoad() const { return state_ == kSentDidFinishLoad; }

  WebFrameLoadType LoadType() const { return load_type_; }
  void SetLoadType(WebFrameLoadType load_type) { load_type_ = load_type; }

  WebNavigationType GetNavigationType() const { return navigation_type_; }
  void SetNavigationType(WebNavigationType navigation_type) {
    navigation_type_ = navigation_type;
  }

  HistoryItem* GetHistoryItem() const { return history_item_; }

  void StartLoading();
  void StopLoading();

  // CommitNavigation() does the work of creating a Document and
  // DocumentParser, as well as creating a new LocalDOMWindow if needed. It also
  // initializes a bunch of state on the Document (e.g., the state based on
  // response headers).
  void CommitNavigation();

  // Called when the browser process has asked this renderer process to commit a
  // same document navigation in that frame. Returns false if the navigation
  // cannot commit, true otherwise.
  mojom::CommitResult CommitSameDocumentNavigation(
      const KURL&,
      WebFrameLoadType,
      HistoryItem*,
      ClientRedirectPolicy,
      LocalDOMWindow* origin_window,
      bool has_event,
      std::unique_ptr<WebDocumentLoader::ExtraData>);

  void SetDefersLoading(bool defers);

  DocumentLoadTiming& GetTiming() { return document_load_timing_; }

  ApplicationCacheHostForFrame* GetApplicationCacheHost() const {
    return application_cache_host_.Get();
  }

  PreviewsState GetPreviewsState() const { return previews_state_; }

  struct InitialScrollState {
    DISALLOW_NEW();
    InitialScrollState()
        : was_scrolled_by_user(false), did_restore_from_history(false) {}

    bool was_scrolled_by_user;
    bool did_restore_from_history;
  };
  InitialScrollState& GetInitialScrollState() { return initial_scroll_state_; }

  void DispatchLinkHeaderPreloads(const ViewportDescription*,
                                  PreloadHelper::MediaPreloadPolicy);

  void SetServiceWorkerNetworkProvider(
      std::unique_ptr<WebServiceWorkerNetworkProvider>);

  // May return null before the first HTML tag is inserted by the
  // parser (before didCreateDataSource is called), after the document
  // is detached from frame, or in tests.
  WebServiceWorkerNetworkProvider* GetServiceWorkerNetworkProvider() {
    return service_worker_network_provider_.get();
  }

  void LoadFailed(const ResourceError&);

  void Trace(Visitor*) const override;

  // For automation driver-initiated navigations over the devtools protocol,
  // |devtools_navigation_token_| is used to tag the navigation. This navigation
  // token is then sent into the renderer and lands on the DocumentLoader. That
  // way subsequent Blink-level frame lifecycle events can be associated with
  // the concrete navigation.
  // - The value should not be sent back to the browser.
  // - The value on DocumentLoader may be generated in the renderer in some
  // cases, and thus shouldn't be trusted.
  // TODO(crbug.com/783506): Replace devtools navigation token with the generic
  // navigation token that can be passed from renderer to the browser.
  const base::UnguessableToken& GetDevToolsNavigationToken() {
    return devtools_navigation_token_;
  }

  // Can be used to temporarily suspend feeding the parser with new data. The
  // parser will be allowed to read new data when ResumeParser() is called the
  // same number of time than BlockParser().
  void BlockParser();
  void ResumeParser();

  bool IsListingFtpDirectory() const { return listing_ftp_directory_; }

  UseCounterHelper& GetUseCounterHelper() { return use_counter_; }
  Dactyloscoper& GetDactyloscoper() { return dactyloscoper_; }

  PrefetchedSignedExchangeManager* GetPrefetchedSignedExchangeManager() const;

  // UseCounter
  void CountUse(mojom::WebFeature) override;

  void SetApplicationCacheHostForTesting(ApplicationCacheHostForFrame* host) {
    application_cache_host_ = host;
  }

  void SetCommitReason(CommitReason reason) { commit_reason_ = reason; }

  bool HadTransientActivation() const { return had_transient_activation_; }

  // Whether the navigation originated from the browser process. Note: history
  // navigation is always considered to be browser initiated, even if the
  // navigation was started using the history API in the renderer.
  bool IsBrowserInitiated() const { return is_browser_initiated_; }

  bool IsSameOriginNavigation() const { return is_same_origin_navigation_; }

  // TODO(dcheng, japhet): Some day, Document::Url() will always match
  // DocumentLoader::Url(), and one of them will be removed. Today is not that
  // day though.
  void UpdateUrlForDocumentOpen(const KURL& url) { url_ = url; }

  enum class HistoryNavigationType {
    kDifferentDocument,
    kFragment,
    kHistoryApi
  };

  void SetHistoryItemStateForCommit(HistoryItem* old_item,
                                    WebFrameLoadType,
                                    HistoryNavigationType);

  mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
  TakePendingWorkerTimingReceiver(int request_id);

  const KURL& WebBundlePhysicalUrl() const { return web_bundle_physical_url_; }

  bool NavigationScrollAllowed() const { return navigation_scroll_allowed_; }

  // We want to make sure that the largest content is painted before the "LCP
  // limit", so that we get a good LCP value. This returns the remaining time to
  // the LCP limit. See crbug.com/1065508 for details.
  base::TimeDelta RemainingTimeToLCPLimit() const;

  mojom::blink::ContentSecurityNotifier& GetContentSecurityNotifier();

  // Returns the value of the text fragment token and then resets it to false
  // to ensure the token can only be used to invoke a single text fragment.
  bool ConsumeTextFragmentToken();

 protected:
  Vector<KURL> redirect_chain_;

  // Based on its MIME type, if the main document's response corresponds to an
  // MHTML archive, then every resources will be loaded from this archive.
  //
  // This includes:
  // - The main document.
  // - Every nested document.
  // - Every subresource.
  //
  // This excludes:
  // - data-URLs documents and subresources.
  // - about:srcdoc documents.
  // - Error pages.
  //
  // Whether about:blank and derivative should be loaded from the archive is
  // weird edge case: Please refer to the tests:
  // - NavigationMhtmlBrowserTest.IframeAboutBlankNotFound
  // - NavigationMhtmlBrowserTest.IframeAboutBlankFound
  //
  // Nested documents are loaded in the same process and grab a reference to the
  // same `archive_` as their parent.
  //
  // Resources:
  // - https://tools.ietf.org/html/rfc822
  // - https://tools.ietf.org/html/rfc2387
  Member<MHTMLArchive> archive_;

 private:
  network::mojom::blink::WebSandboxFlags CalculateSandboxFlags();
  scoped_refptr<SecurityOrigin> CalculateOrigin(
      Document* owner_document,
      network::mojom::blink::WebSandboxFlags);
  void InitializeWindow(Document* owner_document);
  void DidInstallNewDocument(Document*);
  void WillCommitNavigation();
  void DidCommitNavigation();
  void RecordUseCountersForCommit();
  void RecordConsoleMessagesForCommit();

  void CreateParserPostCommit();

  void CommitSameDocumentNavigationInternal(
      const KURL&,
      WebFrameLoadType,
      HistoryItem*,
      ClientRedirectPolicy,
      bool is_content_initiated,
      bool has_event,
      std::unique_ptr<WebDocumentLoader::ExtraData>);

  // Use these method only where it's guaranteed that |m_frame| hasn't been
  // cleared.
  FrameLoader& GetFrameLoader() const;
  LocalFrameClient& GetLocalFrameClient() const;

  void ConsoleError(const String& message);

  // Replace the current document with a empty one and the URL with a unique
  // opaque origin.
  void ReplaceWithEmptyDocument();

  DocumentPolicy::ParsedDocumentPolicy CreateDocumentPolicy();

  void StartLoadingInternal();
  void StartLoadingResponse();
  void FinishedLoading(base::TimeTicks finish_time);
  void CancelLoadAfterCSPDenied(const ResourceResponse&);

  void HandleRedirect(const KURL& current_request_url);
  void HandleResponse();

  void InitializeEmptyResponse();

  bool ShouldReportTimingInfoToParent();

  void CommitData(const char* bytes, size_t length);
  // Processes the data stored in the data_buffer_, used to avoid appending data
  // to the parser in a nested message loop.
  void ProcessDataBuffer(const char* bytes = nullptr, size_t length = 0);

  // Sends an intervention report if the page is being served as a preview.
  void ReportPreviewsIntervention() const;

  // WebNavigationBodyLoader::Client
  void BodyCodeCacheReceived(mojo_base::BigBuffer data) override;
  void BodyDataReceived(base::span<const char> data) override;
  void BodyLoadingFinished(base::TimeTicks completion_time,
                           int64_t total_encoded_data_length,
                           int64_t total_encoded_body_length,
                           int64_t total_decoded_body_length,
                           bool should_report_corb_blocking,
                           const base::Optional<WebURLError>& error) override;

  void ApplyClientHintsConfig(
      const WebVector<network::mojom::WebClientHintsType>&
          enabled_client_hints);

  // For SignedExchangeSubresourcePrefetch feature. If the page was loaded from
  // a signed exchage which has "allowed-alt-sxg" link headers in the inner
  // response and PrefetchedSignedExchanges were passed from the previous page,
  // initializes a PrefetchedSignedExchangeManager which will hold the
  // subresource signed exchange related headers ("alternate" link header in the
  // outer response and "allowed-alt-sxg" link header in the inner response of
  // the page's signed exchange), and the passed PrefetchedSignedExchanges.
  // The created PrefetchedSignedExchangeManager will be used to load the
  // prefetched signed exchanges for matching requests.
  void InitializePrefetchedSignedExchangeManager();

  bool IsJavaScriptURLOrXSLTCommit() const {
    return commit_reason_ == CommitReason::kJavascriptUrl ||
           commit_reason_ == CommitReason::kXSLT;
  }

  // Params are saved in constructor and are cleared after StartLoading().
  // TODO(dgozman): remove once StartLoading is merged with constructor.
  std::unique_ptr<WebNavigationParams> params_;

  // These fields are copied from WebNavigationParams, see there for definition.
  KURL url_;
  AtomicString http_method_;
  Referrer referrer_;
  scoped_refptr<EncodedFormData> http_body_;
  AtomicString http_content_type_;
  PreviewsState previews_state_;
  base::Optional<WebOriginPolicy> origin_policy_;
  const scoped_refptr<const SecurityOrigin> requestor_origin_;
  const KURL unreachable_url_;
  std::unique_ptr<WebNavigationBodyLoader> body_loader_;
  const network::mojom::IPAddressSpace ip_address_space_ =
      network::mojom::IPAddressSpace::kUnknown;
  const bool grant_load_local_resources_ = false;
  const base::Optional<blink::mojom::FetchCacheMode> force_fetch_cache_mode_;
  const FramePolicy frame_policy_;

  Member<LocalFrame> frame_;

  Member<HistoryItem> history_item_;

  // The parser that was created when the current Document was installed.
  // document.open() may create a new parser at a later point, but this
  // will not be updated.
  Member<DocumentParser> parser_;

  Member<SubresourceFilter> subresource_filter_;

  // Stores the resource loading hints for this document.
  Member<PreviewsResourceLoadingHints> resource_loading_hints_;

  // A reference to actual request's url and referrer used to
  // inititate this load.
  KURL original_url_;
  const Referrer original_referrer_;

  ResourceResponse response_;

  WebFrameLoadType load_type_;

  bool is_client_redirect_;
  bool replaces_current_history_item_;
  bool data_received_;

  const Member<ContentSecurityPolicy> content_security_policy_;
  const bool was_blocked_by_csp_;
  mojo::Remote<mojom::blink::ContentSecurityNotifier>
      content_security_notifier_;

  const scoped_refptr<SecurityOrigin> origin_to_commit_;
  WebNavigationType navigation_type_;

  DocumentLoadTiming document_load_timing_;

  base::TimeTicks time_of_last_data_received_;

  Member<ApplicationCacheHostForFrame> application_cache_host_;

  std::unique_ptr<WebServiceWorkerNetworkProvider>
      service_worker_network_provider_;

  DocumentPolicy::ParsedDocumentPolicy document_policy_;
  bool was_blocked_by_document_policy_;
  Vector<PolicyParserMessageBuffer::Message> document_policy_parsing_messages_;

  ClientHintsPreferences client_hints_preferences_;
  InitialScrollState initial_scroll_state_;

  enum State { kNotStarted, kProvisional, kCommitted, kSentDidFinishLoad };
  State state_;

  // Used to block the parser.
  int parser_blocked_count_ = 0;
  bool finish_loading_when_parser_resumed_ = false;

  // Used to protect against reentrancy into CommitData().
  bool in_commit_data_;
  scoped_refptr<SharedBuffer> data_buffer_;
  const base::UnguessableToken devtools_navigation_token_;

  bool defers_loading_ = false;

  // Whether this load request comes with a sitcky user activation.
  const bool had_sticky_activation_ = false;
  // Whether this load request had a user activation when created.
  const bool had_transient_activation_ = false;

  // Whether this load request was initiated by the browser.
  const bool is_browser_initiated_ = false;

  // Whether this load request was initiated by the same origin.
  bool is_same_origin_navigation_ = false;

  // If true, the navigation loading this document should allow a text fragment
  // to invoke. This token may be instead consumed to pass this permission
  // through a redirect.
  bool has_text_fragment_token_ = false;

  // See WebNavigationParams for definition.
  const bool was_discarded_ = false;

  bool listing_ftp_directory_ = false;

  // True when loading the main document from the MHTML archive. It implies an
  // |archive_| to be created. Nested documents will also inherit from the same
  // |archive_|, but won't have |loading_main_document_from_mhtml_archive_| set.
  bool loading_main_document_from_mhtml_archive_ = false;
  const bool loading_srcdoc_ = false;
  const bool loading_url_as_empty_document_ = false;
  CommitReason commit_reason_ = CommitReason::kRegular;
  uint64_t main_resource_identifier_ = 0;
  scoped_refptr<ResourceTimingInfo> navigation_timing_info_;
  bool report_timing_info_to_parent_ = false;
  WebScopedVirtualTimePauser virtual_time_pauser_;
  Member<SourceKeyedCachedMetadataHandler> cached_metadata_handler_;
  Member<PrefetchedSignedExchangeManager> prefetched_signed_exchange_manager_;
  const KURL web_bundle_physical_url_;
  const KURL web_bundle_claimed_url_;
  ukm::SourceId ukm_source_id_;

  // This UseCounterHelper tracks feature usage associated with the lifetime of
  // the document load. Features recorded prior to commit will be recorded
  // locally. Once committed, feature usage will be piped to the browser side
  // page load metrics that aggregates usage from frames to one page load and
  // report feature usage to UMA histograms per page load.
  UseCounterHelper use_counter_;

  Dactyloscoper dactyloscoper_;

  const base::TickClock* clock_;

  const Vector<OriginTrialFeature> initiator_origin_trial_features_;

  const Vector<String> force_enabled_origin_trials_;

  // Whether the document can be scrolled on load
  bool navigation_scroll_allowed_ = true;

  bool origin_isolated_ = false;

  // Whether this load request is cross browsing context group.
  bool is_cross_browsing_context_group_navigation_ = false;
};

DECLARE_WEAK_IDENTIFIER_MAP(DocumentLoader);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_DOCUMENT_LOADER_H_
