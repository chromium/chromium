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

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink-forward.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/loader/frame_loader_state_machine.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ContentSecurityPolicy;
class DocumentLoader;
class LocalFrame;
class Frame;
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
  ~FrameLoader();

  void Init();

  ResourceRequest ResourceRequestForReload(
      WebFrameLoadType,
      ClientRedirectPolicy = ClientRedirectPolicy::kNotClientRedirect);

  ProgressTracker& Progress() const { return *progress_tracker_; }

  // Starts a navigation. It will eventually send the navigation to the
  // browser process, or call LoadInSameDocument for same-document navigation.
  // For reloads, an appropriate WebFrameLoadType should be given. Otherwise,
  // kStandard should be used (and the final WebFrameLoadType
  // will be computed).
  void StartNavigation(const FrameLoadRequest&,
                       WebFrameLoadType = WebFrameLoadType::kStandard);

  // Called when the browser process has asked this renderer process to commit
  // a navigation in this frame. This method skips most of the checks assuming
  // that browser process has already performed any checks necessary.
  // See WebNavigationParams for details.
  void CommitNavigation(
      std::unique_ptr<WebNavigationParams> navigation_params,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data,
      base::OnceClosure call_before_attaching_new_document =
          base::DoNothing::Once(),
      bool is_javascript_url = false);

  // Called before the browser process is asked to navigate this frame, to mark
  // the frame as loading and save some navigation information for later use.
  bool WillStartNavigation(const WebNavigationInfo& info,
                           bool is_history_navigation_in_new_frame);

  // This runs the "stop document loading" algorithm in HTML:
  // https://html.spec.whatwg.org/C/browsing-the-web.html#stop-document-loading
  // Note, this function only cancels ongoing navigation handled through
  // FrameLoader. You might also want to call
  // LocalFrameClient::AbortClientNavigation() if appropriate.
  //
  // Warning: StopAllLoaders() may detach the LocalFrame to which this
  // FrameLoader belongs. Callers need to be careful about checking the
  // existence of the frame after StopAllLoaders() returns.
  void StopAllLoaders();

  // Notifies the client that the initial empty document has been accessed, and
  // thus it is no longer safe to show a provisional URL above the document
  // without risking a URL spoof. The client must not call back into JavaScript.
  void DidAccessInitialDocument();

  DocumentLoader* GetDocumentLoader() const { return document_loader_.Get(); }

  void SetDefersLoading(bool);

  void DidExplicitOpen();

  String UserAgent() const;
  blink::UserAgentMetadata UserAgentMetadata() const;

  void DispatchDidClearWindowObjectInMainWorld();
  void DispatchDidClearDocumentOfWindowObject();
  void DispatchDocumentElementAvailable();
  void RunScriptsAtDocumentElementAvailable();

  // The following sandbox flags will be forced, regardless of changes to the
  // sandbox attribute of any parent frames.
  void ForceSandboxFlags(WebSandboxFlags flags) {
    forced_sandbox_flags_ |= flags;
  }
  // Includes the collection of forced, inherited, and FrameOwner's sandbox
  // flags. Note: with FeaturePolicyForSandbox the frame owner's sandbox flags
  // only includes the flags which are *not* implemented as feature policies
  // already present in the FrameOwner's ContainerPolicy.
  WebSandboxFlags EffectiveSandboxFlags() const;

  // Modifying itself is done based on |fetch_client_settings_object|.
  // |document_for_logging| is used only for logging, use counters,
  // UKM-related things.
  void ModifyRequestForCSP(
      ResourceRequest&,
      const FetchClientSettingsObject* fetch_client_settings_object,
      Document* document_for_logging,
      network::mojom::RequestContextFrameType) const;
  void ReportLegacyTLSVersion(const KURL& url,
                              bool is_subresource,
                              bool is_ad_resource);

  Frame* Opener();
  void SetOpener(LocalFrame*);

  const AtomicString& RequiredCSP() const { return required_csp_; }
  void RecordLatestRequiredCSP();

  void Detach();

  void FinishedParsing();
  enum class NavigationFinishState { kSuccess, kFailure };
  void DidFinishNavigation(NavigationFinishState);

  void DidFinishSameDocumentNavigation(const KURL&,
                                       WebFrameLoadType,
                                       HistoryItem*);

  // This will attempt to detach the current document. It will dispatch unload
  // events and abort XHR requests. Returns true if the frame is ready to
  // receive the next document commit, or false otherwise.
  bool DetachDocument(SecurityOrigin* committing_origin,
                      base::Optional<Document::UnloadEventTiming>*);

  FrameLoaderStateMachine* StateMachine() const { return &state_machine_; }

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
                           base::Optional<Document::UnloadEventTiming>*);

  bool AllowPlugins(ReasonForCallingAllowPlugins);

  void SaveScrollAnchor();
  void SaveScrollState();
  void RestoreScrollPositionAndViewState();

  bool HasProvisionalNavigation() const {
    return committing_navigation_ || client_navigation_.get();
  }

  bool MaybeRenderFallbackContent();

  // Like ClearClientNavigation, but also notifies the client to actually cancel
  // the navigation.
  void CancelClientNavigation();
  void DetachProvisionalDocumentLoader();

  void Trace(blink::Visitor*);

  void DidDropNavigation();
  void MarkAsLoading();

  ContentSecurityPolicy* GetLastOriginDocumentCSP() {
    return last_origin_document_csp_.Get();
  }
  bool ShouldReuseDefaultView(const scoped_refptr<const SecurityOrigin>&,
                              const ContentSecurityPolicy*);

  bool IsClientNavigationInitialHistoryLoad();

 private:
  bool AllowRequestForThisFrame(const FrameLoadRequest&);
  WebFrameLoadType DetermineFrameLoadType(const KURL& url,
                                          const AtomicString& http_method,
                                          Document* origin_document,
                                          const KURL& failing_url,
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
                                         bool is_same_document,
                                         const HistoryItem::ViewState&,
                                         HistoryScrollRestorationType);

  void DetachDocumentLoader(Member<DocumentLoader>&,
                            bool flush_microtask_queue = false);

  std::unique_ptr<TracedValue> ToTracedValue() const;
  void TakeObjectSnapshot() const;

  // Commits the given |document_loader|.
  void CommitDocumentLoader(
      DocumentLoader* document_loader,
      const base::Optional<Document::UnloadEventTiming>&,
      bool dispatch_did_start,
      base::OnceClosure call_before_attaching_new_document,
      bool dispatch_did_commit);

  LocalFrameClient* Client() const;

  Member<LocalFrame> frame_;
  AtomicString required_csp_;

  // FIXME: These should be std::unique_ptr<T> to reduce build times and
  // simplify header dependencies unless performance testing proves otherwise.
  // Some of these could be lazily created for memory savings on devices.
  mutable FrameLoaderStateMachine state_machine_;

  Member<ProgressTracker> progress_tracker_;

  // Document loaders for the three phases of frame loading. Note that while a
  // new request is being loaded, the old document loader may still be
  // referenced. E.g. while a new request is in the "policy" state, the old
  // document loader may be consulted in particular as it makes sense to imply
  // certain settings on the new loader.
  Member<DocumentLoader> document_loader_;
  Member<DocumentLoader> provisional_document_loader_;

  // This struct holds information about a navigation, which is being
  // initiated by the client through the browser process, until the navigation
  // is either committed or cancelled.
  struct ClientNavigationState {
    KURL url;
    bool is_history_navigation_in_new_frame = false;
  };
  std::unique_ptr<ClientNavigationState> client_navigation_;

  bool in_restore_scroll_;

  WebSandboxFlags forced_sandbox_flags_;

  bool dispatching_did_clear_window_object_in_main_world_;
  bool detached_;
  bool committing_navigation_ = false;

  WebScopedVirtualTimePauser virtual_time_pauser_;

  Member<ContentSecurityPolicy> last_origin_document_csp_;

  // The origins for which a legacy TLS version warning has been printed. The
  // size of this set is capped, after which no more warnings are printed.
  HashSet<String> tls_version_warning_origins_;

  DISALLOW_COPY_AND_ASSIGN(FrameLoader);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_LOADER_H_
