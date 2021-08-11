/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_LOADER_H_

#include <memory>

#include "base/callback_helpers.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/page_state/page_state.mojom-blink-forward.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_origin_policy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/loader_freeze_mode.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class DocumentLoader;
class FetchClientSettingsObject;
class Frame;
class LocalFrame;
class LocalFrameClient;
class ProgressTracker;
class ResourceRequest;
class TracedValue;
struct FrameLoadRequest;
struct WebNavigationInfo;
struct WebNavigationParams;

CORE_EXPORT bool IsBackForwardLoadType(WebFrameLoadType);
CORE_EXPORT bool IsReloadLoadType(WebFrameLoadType);

class CORE_EXPORT FrameLoader final {
  DISALLOW_NEW();

 public:
  explicit FrameLoader(LocalFrame*);
  FrameLoader(const FrameLoader&) = delete;
  FrameLoader& operator=(const FrameLoader&) = delete;
  ~FrameLoader();

  void Init(std::unique_ptr<PolicyContainer> policy_container);

  ResourceRequest ResourceRequestForReload(
      WebFrameLoadType,
      ClientRedirectPolicy = ClientRedirectPolicy::kNotClientRedirect);

  ProgressTracker& Progress() const { return *progress_tracker_; }

  // Starts a navigation. It will eventually send the navigation to the
  // browser process, or call LoadInSameDocument for same-document navigation.
  // For reloads, an appropriate WebFrameLoadType should be given. Otherwise,
  // kStandard should be used (and the final WebFrameLoadType
  // will be computed).
  void StartNavigation(FrameLoadRequest&,
                       WebFrameLoadType = WebFrameLoadType::kStandard);

  // Called when the browser process has asked this renderer process to commit
  // a navigation in this frame. This method skips most of the checks assuming
  // that browser process has already performed any checks necessary.
  // See WebNavigationParams for details.
  void CommitNavigation(
      std::unique_ptr<WebNavigationParams> navigation_params,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data,
      CommitReason = CommitReason::kRegular);

  // Called before the browser process is asked to navigate this frame, to mark
  // the frame as loading and save some navigation information for later use.
  bool WillStartNavigation(const WebNavigationInfo& info);

  // This runs the "stop document loading" algorithm in HTML:
  // https://html.spec.whatwg.org/C/browsing-the-web.html#stop-document-loading
  // Note, this function only cancels ongoing navigation handled through
  // FrameLoader.
  //
  // If |abort_client| is true, then the frame's client will have
  // AbortClientNavigation() called if a navigation was aborted. Normally this
  // should be passed as true, unless the navigation has been migrated to a
  // provisional frame, while this frame is going away, so the navigation isn't
  // actually being aborted.
  //
  // Warning: StopAllLoaders() may detach the LocalFrame to which this
  // FrameLoader belongs. Callers need to be careful about checking the
  // existence of the frame after StopAllLoaders() returns.
  void StopAllLoaders(bool abort_client);

  // Notifies the client that the initial empty document has been accessed, and
  // thus it is no longer safe to show a provisional URL above the document
  // without risking a URL spoof. The client must not call back into JavaScript.
  void DidAccessInitialDocument();

  DocumentLoader* GetDocumentLoader() const { return document_loader_.Get(); }

  void SetDefersLoading(LoaderFreezeMode mode);

  void DidExplicitOpen();

  String UserAgent() const;
  String ReducedUserAgent() const;
  absl::optional<blink::UserAgentMetadata> UserAgentMetadata() const;

  void DispatchDidClearWindowObjectInMainWorld();
  void DispatchDidClearDocumentOfWindowObject();
  void DispatchDocumentElementAvailable();
  void RunScriptsAtDocumentElementAvailable();

  // See content/browser/renderer_host/sandbox_flags.md
  // This contains the sandbox flags to commit for new documents.
  // - For main documents, it contains the sandbox inherited from the opener.
  // - For nested documents, it contains the sandbox flags inherited from the
  //   parent and the one defined in the <iframe>'s sandbox attribute.
  network::mojom::blink::WebSandboxFlags PendingEffectiveSandboxFlags() const;

  // Modifying itself is done based on |fetch_client_settings_object|.
  // |document_for_logging| is used only for logging, use counters,
  // UKM-related things.
  void ModifyRequestForCSP(
      ResourceRequest&,
      const FetchClientSettingsObject* fetch_client_settings_object,
      LocalDOMWindow* window_for_logging,
      mojom::RequestContextFrameType) const;
  void ReportLegacyTLSVersion(const KURL& url,
                              bool is_subresource,
                              bool is_ad_resource);

  Frame* Opener();
  void SetOpener(LocalFrame*);

  void Detach();

  void FinishedParsing();
  enum class NavigationFinishState { kSuccess, kFailure };
  void DidFinishNavigation(NavigationFinishState);

  void DidFinishSameDocumentNavigation(const KURL&,
                                       WebFrameLoadType,
                                       HistoryItem*,
                                       bool may_restore_scroll_offset);

  // This will attempt to detach the current document. It will dispatch unload
  // events and abort XHR requests. Returns true if the frame is ready to
  // receive the next document commit, or false otherwise.
  bool DetachDocument(SecurityOrigin* committing_origin,
                      absl::optional<Document::UnloadEventTiming>*);

  bool ShouldClose(bool is_reload = false);

  // Dispatches the Unload event for the current document. If this is due to the
  // commit of a navigation, both |committing_origin| and the
  // Optional<Document::UnloadEventTiming>* should be non null.
  // |committing_origin| is the origin of the document that is being committed.
  // If it is allowed to access the unload timings of the current document, the
  // Document::UnloadEventTiming will be created and populated.
  // If the dispatch of the unload event is not due to a commit, both parameters
  // should be null.
  void DispatchUnloadEvent(SecurityOrigin* committing_origin,
                           absl::optional<Document::UnloadEventTiming>*);

  bool AllowPlugins();

  void SaveScrollAnchor();
  void SaveScrollState();
  void RestoreScrollPositionAndViewState();

  bool HasProvisionalNavigation() const {
    return committing_navigation_ || client_navigation_.get();
  }

  // Like ClearClientNavigation, but also notifies the client to actually cancel
  // the navigation.
  void CancelClientNavigation();

  void Trace(Visitor*) const;

  void DidDropNavigation();

  bool HasAccessedInitialDocument() { return has_accessed_initial_document_; }

  void SetDidLoadNonEmptyDocument() {
    empty_document_status_ = EmptyDocumentStatus::kNonEmpty;
  }
  bool HasLoadedNonEmptyDocument() {
    return empty_document_status_ == EmptyDocumentStatus::kNonEmpty;
  }

  static bool NeedsHistoryItemRestore(WebFrameLoadType type);

  void WriteIntoTrace(perfetto::TracedValue context) const;

 private:
  bool AllowRequestForThisFrame(const FrameLoadRequest&);
  WebFrameLoadType HandleInitialEmptyDocumentReplacementIfNeeded(
      const KURL& url,
      WebFrameLoadType);

  bool ShouldPerformFragmentNavigation(bool is_form_submission,
                                       const String& http_method,
                                       WebFrameLoadType,
                                       const KURL&);
  void ProcessFragment(const KURL&, WebFrameLoadType, LoadStartType);

  // Returns whether we should continue with new navigation.
  bool CancelProvisionalLoaderForNewNavigation();

  // Clears any information about client navigation, see client_navigation_.
  void ClearClientNavigation();

  void RestoreScrollPositionAndViewState(WebFrameLoadType,
                                         const HistoryItem::ViewState&,
                                         mojom::blink::ScrollRestorationType);

  void DetachDocumentLoader(Member<DocumentLoader>&,
                            bool flush_microtask_queue = false);

  void TakeObjectSnapshot() const;

  // Commits the given |document_loader|.
  void CommitDocumentLoader(DocumentLoader* document_loader,
                            const absl::optional<Document::UnloadEventTiming>&,
                            HistoryItem* previous_history_item,
                            CommitReason);

  LocalFrameClient* Client() const;

  Member<LocalFrame> frame_;

  Member<ProgressTracker> progress_tracker_;

  // Document loader for frame loading.
  Member<DocumentLoader> document_loader_;

  // This struct holds information about a navigation, which is being
  // initiated by the client through the browser process, until the navigation
  // is either committed or cancelled.
  struct ClientNavigationState {
    KURL url;
  };
  std::unique_ptr<ClientNavigationState> client_navigation_;

  // The state is set to kInitialized when Init() completes, and kDetached
  // during teardown in Detach().
  enum class State { kUninitialized, kInitialized, kDetached };
  State state_ = State::kUninitialized;

  bool dispatching_did_clear_window_object_in_main_world_;
  bool committing_navigation_ = false;
  bool has_accessed_initial_document_ = false;

  enum class EmptyDocumentStatus {
    kOnlyEmpty,
    kOnlyEmptyButExplicitlyOpened,
    kNonEmpty
  };
  EmptyDocumentStatus empty_document_status_ = EmptyDocumentStatus::kOnlyEmpty;

  WebScopedVirtualTimePauser virtual_time_pauser_;

  // The origins for which a legacy TLS version warning has been printed. The
  // size of this set is capped, after which no more warnings are printed.
  HashSet<String> tls_version_warning_origins_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_LOADER_H_
