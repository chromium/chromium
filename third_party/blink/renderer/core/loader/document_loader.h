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
#include "base/unguessable_token.h"
#include "third_party/blink/public/platform/web_loading_behavior_flag.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_global_object_reuse_policy.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/weak_identifier_map.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/parser/parser_synchronization_policy.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/loader/link_loader.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/substitute_data.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

#include <memory>

namespace blink {

class ApplicationCacheHost;
class Document;
class DocumentParser;
class FrameLoader;
class HistoryItem;
class LocalFrame;
class LocalFrameClient;
class ResourceFetcher;
class ResourceTimingInfo;
class SerializedScriptValue;
class SubresourceFilter;
class WebServiceWorkerNetworkProvider;
struct ViewportDescriptionWrapper;

// The DocumentLoader fetches a main resource and handles the result.
class CORE_EXPORT DocumentLoader
    : public GarbageCollectedFinalized<DocumentLoader>,
      private RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(DocumentLoader);

 public:
  DocumentLoader(LocalFrame*,
                 const ResourceRequest&,
                 const SubstituteData&,
                 ClientRedirectPolicy,
                 const base::UnguessableToken& devtools_navigation_token,
                 WebFrameLoadType load_type,
                 WebNavigationType navigation_type,
                 std::unique_ptr<WebNavigationParams> navigation_params);
  ~DocumentLoader() override;

  LocalFrame* GetFrame() const { return frame_; }

  ResourceTimingInfo* GetNavigationTimingInfo() const;

  virtual void DetachFromFrame(bool flush_microtask_queue);

  unsigned long MainResourceIdentifier() const;

  void ReplaceDocumentWhileExecutingJavaScriptURL(const KURL&,
                                                  Document* owner_document,
                                                  WebGlobalObjectReusePolicy,
                                                  const String& source);

  const AtomicString& MimeType() const;

  const ResourceRequest& OriginalRequest() const;

  const ResourceRequest& GetRequest() const;

  ResourceFetcher* Fetcher() const { return fetcher_.Get(); }

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

  const SubstituteData& GetSubstituteData() const { return substitute_data_; }

  const KURL& Url() const;
  const KURL& UnreachableURL() const;
  const KURL& UrlForHistory() const;

  void DidChangePerformanceTiming();
  void DidObserveLoadingBehavior(WebLoadingBehaviorFlag);
  void UpdateForSameDocumentNavigation(const KURL&,
                                       SameDocumentNavigationSource,
                                       scoped_refptr<SerializedScriptValue>,
                                       HistoryScrollRestorationType,
                                       WebFrameLoadType,
                                       Document*);
  const ResourceResponse& GetResponse() const { return response_; }
  bool IsClientRedirect() const { return is_client_redirect_; }
  void SetIsClientRedirect(bool is_client_redirect) {
    is_client_redirect_ = is_client_redirect;
  }
  bool ReplacesCurrentHistoryItem() const {
    return replaces_current_history_item_;
  }
  void SetReplacesCurrentHistoryItem(bool replaces_current_history_item) {
    replaces_current_history_item_ = replaces_current_history_item;
  }

  bool IsCommittedButEmpty() const {
    return state_ >= kCommitted && !data_received_;
  }

  // Without PlzNavigate, this is only false for a narrow window during
  // navigation start. For PlzNavigate, a navigation sent to the browser will
  // leave a dummy DocumentLoader in the NotStarted state until the navigation
  // is actually handled in the renderer.
  bool DidStart() const { return state_ != kNotStarted; }

  void MarkAsCommitted();
  void SetSentDidFinishLoad() { state_ = kSentDidFinishLoad; }
  bool SentDidFinishLoad() const { return state_ == kSentDidFinishLoad; }

  WebFrameLoadType LoadType() const { return load_type_; }
  void SetLoadType(WebFrameLoadType load_type) { load_type_ = load_type; }

  WebNavigationType GetNavigationType() const { return navigation_type_; }
  void SetNavigationType(WebNavigationType navigation_type) {
    navigation_type_ = navigation_type;
  }

  void SetItemForHistoryNavigation(HistoryItem* item) { history_item_ = item; }
  HistoryItem* GetHistoryItem() const { return history_item_; }

  void StartLoading();
  void StopLoading();

  DocumentLoadTiming& GetTiming() { return document_load_timing_; }
  const DocumentLoadTiming& GetTiming() const { return document_load_timing_; }

  ApplicationCacheHost* GetApplicationCacheHost() const {
    return application_cache_host_.Get();
  }

  void ClearRedirectChain();
  void AppendRedirect(const KURL&);

  ClientHintsPreferences& GetClientHintsPreferences() {
    return client_hints_preferences_;
  }

  struct InitialScrollState {
    DISALLOW_NEW();
    InitialScrollState()
        : was_scrolled_by_user(false), did_restore_from_history(false) {}

    bool was_scrolled_by_user;
    bool did_restore_from_history;
  };
  InitialScrollState& GetInitialScrollState() { return initial_scroll_state_; }

  void SetWasBlockedAfterCSP() { was_blocked_after_csp_ = true; }
  bool WasBlockedAfterCSP() { return was_blocked_after_csp_; }

  void DispatchLinkHeaderPreloads(ViewportDescriptionWrapper*,
                                  LinkLoader::MediaPreloadPolicy);

  Resource* StartPreload(ResourceType, FetchParameters&);

  void SetServiceWorkerNetworkProvider(
      std::unique_ptr<WebServiceWorkerNetworkProvider>);

  // May return null before the first HTML tag is inserted by the
  // parser (before didCreateDataSource is called), after the document
  // is detached from frame, or in tests.
  WebServiceWorkerNetworkProvider* GetServiceWorkerNetworkProvider() {
    return service_worker_network_provider_.get();
  }

  // Allows to specify the SourceLocation that triggered the navigation.
  void ResetSourceLocation();
  std::unique_ptr<SourceLocation> CopySourceLocation() const;

  void LoadFailed(const ResourceError&);

  void SetUserActivated();

  const AtomicString& RequiredCSP();

  void Trace(blink::Visitor*) override;

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

  // Returns the currently stored content security policy, if this is called
  // after the document has been installed it will return nullptr as the
  // CSP belongs to the document at that point.
  const ContentSecurityPolicy* GetContentSecurityPolicy() const {
    return content_security_policy_.Get();
  }

  UseCounter& GetUseCounter() { return use_counter_; }

 protected:
  static bool ShouldClearWindowName(
      const LocalFrame&,
      const SecurityOrigin* previous_security_origin,
      const Document& new_document);

  bool had_transient_activation() const { return had_transient_activation_; }

  Vector<KURL> redirect_chain_;

 private:
  // installNewDocument() does the work of creating a Document and
  // DocumentParser, as well as creating a new LocalDOMWindow if needed. It also
  // initalizes a bunch of state on the Document (e.g., the state based on
  // response headers).
  enum class InstallNewDocumentReason { kNavigation, kJavascriptURL };
  void InstallNewDocument(const KURL&,
                          Document* owner_document,
                          WebGlobalObjectReusePolicy,
                          const AtomicString& mime_type,
                          const AtomicString& encoding,
                          InstallNewDocumentReason,
                          ParserSynchronizationPolicy,
                          const KURL& overriding_url);
  void DidInstallNewDocument(Document*, const ContentSecurityPolicy*);
  void WillCommitNavigation();
  void DidCommitNavigation(WebGlobalObjectReusePolicy);

  void CommitNavigation(const AtomicString& mime_type,
                        const KURL& overriding_url = KURL());

  // Use these method only where it's guaranteed that |m_frame| hasn't been
  // cleared.
  FrameLoader& GetFrameLoader() const;
  LocalFrameClient& GetLocalFrameClient() const;

  void CommitData(const char* bytes, size_t length);

  bool MaybeCreateArchive();

  void FinishedLoading(TimeTicks finish_time);
  void CancelLoadAfterCSPDenied(const ResourceResponse&);

  enum class HistoryNavigationType {
    kDifferentDocument,
    kFragment,
    kHistoryApi
  };
  void SetHistoryItemStateForCommit(HistoryItem* old_item,
                                    WebFrameLoadType,
                                    HistoryNavigationType);

  // RawResourceClient implementation
  bool RedirectReceived(Resource*,
                        const ResourceRequest&,
                        const ResourceResponse&) final;
  void ResponseReceived(Resource*,
                        const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) final;
  void DataReceived(Resource*, const char* data, size_t length) final;

  // ResourceClient implementation
  void NotifyFinished(Resource*) final;
  String DebugName() const override { return "DocumentLoader"; }

  void ProcessData(const char* data, size_t length);

  bool MaybeLoadEmpty();

  bool IsRedirectAfterPost(const ResourceRequest&, const ResourceResponse&);

  bool ShouldContinueForResponse() const;

  // Processes the data stored in the data_buffer_, used to avoid appending data
  // to the parser in a nested message loop.
  void ProcessDataBuffer();

  Member<LocalFrame> frame_;
  Member<ResourceFetcher> fetcher_;

  Member<HistoryItem> history_item_;

  // The parser that was created when the current Document was installed.
  // document.open() may create a new parser at a later point, but this
  // will not be updated.
  Member<DocumentParser> parser_;

  Member<SubresourceFilter> subresource_filter_;

  // Stores the resource loading hints for this document.
  Member<PreviewsResourceLoadingHints> resource_loading_hints_;

  // A reference to actual request used to create the data source.
  // The only part of this request that should change is the url, and
  // that only in the case of a same-document navigation.
  ResourceRequest original_request_;

  SubstituteData substitute_data_;

  // The 'working' request. It may be mutated
  // several times from the original request to include additional
  // headers, cookie information, canonicalization and redirects.
  ResourceRequest request_;

  ResourceResponse response_;

  WebFrameLoadType load_type_;

  bool is_client_redirect_;
  bool replaces_current_history_item_;
  bool data_received_;

  WebNavigationType navigation_type_;

  DocumentLoadTiming document_load_timing_;

  TimeTicks time_of_last_data_received_;

  Member<ApplicationCacheHost> application_cache_host_;

  std::unique_ptr<WebServiceWorkerNetworkProvider>
      service_worker_network_provider_;

  Member<ContentSecurityPolicy> content_security_policy_;
  ClientHintsPreferences client_hints_preferences_;
  InitialScrollState initial_scroll_state_;

  bool was_blocked_after_csp_;

  // PlzNavigate: set when committing a navigation. The data has originally been
  // captured when the navigation was sent to the browser process, and it is
  // sent back at commit time.
  std::unique_ptr<SourceLocation> source_location_;

  enum State { kNotStarted, kProvisional, kCommitted, kSentDidFinishLoad };
  State state_;

  // Used to block the parser.
  int parser_blocked_count_ = 0;
  bool finished_loading_ = false;
  scoped_refptr<SharedBuffer> committed_data_buffer_;

  // Used to protect against reentrancy into dataReceived().
  bool in_data_received_;
  scoped_refptr<SharedBuffer> data_buffer_;
  base::UnguessableToken devtools_navigation_token_;

  // Whether this load request comes with a sitcky user activation.
  bool had_sticky_activation_;
  // Whether this load request had a user activation when created.
  bool had_transient_activation_;

  // This UseCounter tracks feature usage associated with the lifetime of the
  // document load. Features recorded prior to commit will be recorded locally.
  // Once commited, feature usage will be piped to the browser side page load
  // metrics that aggregates usage from frames to one page load and report
  // feature usage to UMA histograms per page load.
  UseCounter use_counter_;
};

DECLARE_WEAK_IDENTIFIER_MAP(DocumentLoader);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_DOCUMENT_LOADER_H_
