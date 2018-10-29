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

#include "base/macros.h"
#include "third_party/blink/public/platform/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/loader/frame_loader_state_machine.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

#include <memory>

namespace base {
class UnguessableToken;
}

namespace blink {

class Document;
class DocumentLoader;
class ExecutionContext;
class LocalFrame;
class Frame;
class LocalFrameClient;
class ProgressTracker;
class ResourceError;
class ResourceRequest;
class SerializedScriptValue;
class SubstituteData;
class TracedValue;
struct FrameLoadRequest;
struct WebNavigationParams;

namespace mojom {
enum class CommitResult : int32_t;
}

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
                       WebFrameLoadType = WebFrameLoadType::kStandard,
                       NavigationPolicy = kNavigationPolicyCurrentTab);

  // Called when the browser process has asked this renderer process to commit
  // a navigation in this frame. This method skips most of the checks assuming
  // that browser process has already performed any checks necessary.
  // For history navigations, a history item should be provided and
  // an appropriate WebFrameLoadType should be given.
  // See DocumentLoader::devtools_navigation_token_ for documentation on
  // the token.
  void CommitNavigation(
      const ResourceRequest&,
      const SubstituteData&,
      ClientRedirectPolicy,
      const base::UnguessableToken& devtools_navigation_token,
      WebFrameLoadType = WebFrameLoadType::kStandard,
      HistoryItem* = nullptr,
      std::unique_ptr<WebNavigationParams> navigation_params = nullptr,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data = nullptr);

  // Called when the browser process has asked this renderer process to commit a
  // same document navigation in that frame. Returns false if the navigation
  // cannot commit, true otherwise.
  mojom::CommitResult CommitSameDocumentNavigation(
      const KURL&,
      WebFrameLoadType,
      HistoryItem*,
      ClientRedirectPolicy,
      Document* origin_document,
      bool has_event,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data = nullptr);

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

  void ReplaceDocumentWhileExecutingJavaScriptURL(const String& source,
                                                  Document* owner_document);

  // Notifies the client that the initial empty document has been accessed, and
  // thus it is no longer safe to show a provisional URL above the document
  // without risking a URL spoof. The client must not call back into JavaScript.
  void DidAccessInitialDocument();

  DocumentLoader* GetDocumentLoader() const { return document_loader_.Get(); }
  DocumentLoader* GetProvisionalDocumentLoader() const {
    return provisional_document_loader_.Get();
  }

  void LoadFailed(DocumentLoader*, const ResourceError&);

  bool IsLoadingMainFrame() const;

  bool ShouldTreatURLAsSameAsCurrent(const KURL&) const;
  bool ShouldTreatURLAsSrcdocDocument(const KURL&) const;

  void SetDefersLoading(bool);

  void DidExplicitOpen();

  String UserAgent() const;

  void DispatchDidClearWindowObjectInMainWorld();
  void DispatchDidClearDocumentOfWindowObject();
  void DispatchDocumentElementAvailable();
  void RunScriptsAtDocumentElementAvailable();

  // The following sandbox flags will be forced, regardless of changes to the
  // sandbox attribute of any parent frames.
  void ForceSandboxFlags(SandboxFlags flags) { forced_sandbox_flags_ |= flags; }
  SandboxFlags EffectiveSandboxFlags() const;

  void ModifyRequestForCSP(ResourceRequest&, Document*) const;

  Frame* Opener();
  void SetOpener(LocalFrame*);

  const AtomicString& RequiredCSP() const { return required_csp_; }
  void RecordLatestRequiredCSP();

  void Detach();

  void FinishedParsing();
  void DidFinishNavigation();

  // This prepares the FrameLoader for the next commit. It will dispatch unload
  // events, abort XHR requests and detach the document. Returns true if the
  // frame is ready to receive the next commit, or false otherwise.
  bool PrepareForCommit();

  void CommitProvisionalLoad();

  FrameLoaderStateMachine* StateMachine() const { return &state_machine_; }

  bool AllAncestorsAreComplete() const;  // including this

  bool ShouldClose(bool is_reload = false);
  void DispatchUnloadEvent();

  bool AllowPlugins(ReasonForCallingAllowPlugins);

  void UpdateForSameDocumentNavigation(const KURL&,
                                       SameDocumentNavigationSource,
                                       scoped_refptr<SerializedScriptValue>,
                                       HistoryScrollRestorationType,
                                       WebFrameLoadType,
                                       Document*);

  bool ShouldSerializeScrollAnchor();
  void SaveScrollAnchor();
  void SaveScrollState();
  void RestoreScrollPositionAndViewState();

  // Note: When a PlzNavigtate navigation is handled by the client, we will
  // have created a dummy provisional DocumentLoader, so this will return true
  // while the client handles the navigation.
  bool HasProvisionalNavigation() const {
    return GetProvisionalDocumentLoader();
  }

  void DetachProvisionalDocumentLoader(DocumentLoader*);

  void Trace(blink::Visitor*);

  static void SetReferrerForFrameRequest(FrameLoadRequest&);
  static void UpgradeInsecureRequest(ResourceRequest&, ExecutionContext*);

  void ClientDroppedNavigation();
  void MarkAsLoading();

 private:
  bool PrepareRequestForThisFrame(FrameLoadRequest&);
  WebFrameLoadType DetermineFrameLoadType(
      const ResourceRequest& resource_request,
      Document* origin_document,
      const KURL& failing_url,
      WebFrameLoadType);

  SubstituteData DefaultSubstituteDataForURL(const KURL&);

  bool ShouldPerformFragmentNavigation(bool is_form_submission,
                                       const String& http_method,
                                       WebFrameLoadType,
                                       const KURL&);
  void ProcessFragment(const KURL&, WebFrameLoadType, LoadStartType);

  // Returns whether we should continue with new navigation.
  bool CancelProvisionalLoaderForNewNavigation(
      bool cancel_scheduled_navigations);

  void ClearInitialScrollState();

  void LoadInSameDocument(const KURL&,
                          scoped_refptr<SerializedScriptValue> state_object,
                          WebFrameLoadType,
                          HistoryItem*,
                          ClientRedirectPolicy,
                          Document*,
                          std::unique_ptr<WebDocumentLoader::ExtraData>);
  void RestoreScrollPositionAndViewState(WebFrameLoadType,
                                         bool is_same_document,
                                         HistoryItem::ViewState*,
                                         HistoryScrollRestorationType);

  void ScheduleCheckCompleted();

  void DetachDocumentLoader(Member<DocumentLoader>&,
                            bool flush_microtask_queue = false);

  std::unique_ptr<TracedValue> ToTracedValue() const;
  void TakeObjectSnapshot() const;

  DocumentLoader* CreateDocumentLoader(
      const ResourceRequest&,
      const SubstituteData&,
      ClientRedirectPolicy,
      const base::UnguessableToken& devtools_navigation_token,
      WebFrameLoadType,
      WebNavigationType,
      std::unique_ptr<WebNavigationParams>,
      std::unique_ptr<WebDocumentLoader::ExtraData>);

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

  bool in_stop_all_loaders_;
  bool in_restore_scroll_;

  SandboxFlags forced_sandbox_flags_;

  bool dispatching_did_clear_window_object_in_main_world_;
  bool protect_provisional_loader_;
  bool detached_;

  WebScopedVirtualTimePauser virtual_time_pauser_;

  DISALLOW_COPY_AND_ASSIGN(FrameLoader);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_LOADER_H_
