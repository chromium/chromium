/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_LOCAL_FRAME_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_LOCAL_FRAME_CLIENT_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class WebDevToolsAgentImpl;
class WebLocalFrameImpl;
class WebSpellCheckPanelHostClient;
struct WebScrollIntoViewParams;

class LocalFrameClientImpl final : public LocalFrameClient {
 public:
  static LocalFrameClientImpl* Create(WebLocalFrameImpl*);

  ~LocalFrameClientImpl() override;

  void Trace(blink::Visitor*) override;

  WebLocalFrameImpl* GetWebFrame() const override;

  // LocalFrameClient ----------------------------------------------

  void DidCreateNewDocument() override;
  // Notifies the WebView delegate that the JS window object has been cleared,
  // giving it a chance to bind native objects to the window before script
  // parsing begins.
  void DispatchDidClearWindowObjectInMainWorld() override;
  void DocumentElementAvailable() override;
  void RunScriptsAtDocumentElementAvailable() override;
  void RunScriptsAtDocumentReady(bool document_is_empty) override;
  void RunScriptsAtDocumentIdle() override;

  void DidCreateScriptContext(v8::Local<v8::Context>, int world_id) override;
  void WillReleaseScriptContext(v8::Local<v8::Context>, int world_id) override;

  // Returns true if we should allow register V8 extensions to be added.
  bool AllowScriptExtensions() override;

  bool HasWebView() const override;
  bool InShadowTree() const override;
  Frame* Opener() const override;
  void SetOpener(Frame*) override;
  Frame* Parent() const override;
  Frame* Top() const override;
  Frame* NextSibling() const override;
  Frame* FirstChild() const override;
  void WillBeDetached() override;
  void Detached(FrameDetachType) override;
  void DispatchWillSendRequest(ResourceRequest&) override;
  void DispatchDidReceiveResponse(const ResourceResponse&) override;
  void DispatchDidLoadResourceFromMemoryCache(const ResourceRequest&,
                                              const ResourceResponse&) override;
  void DispatchDidHandleOnloadEvents() override;
  void DidFinishSameDocumentNavigation(HistoryItem*,
                                       WebHistoryCommitType,
                                       bool content_initiated) override;
  void DispatchWillCommitProvisionalLoad() override;
  void DispatchDidStartProvisionalLoad(
      DocumentLoader*,
      ResourceRequest&,
      mojo::ScopedMessagePipeHandle navigation_initiator_handle) override;
  void DispatchDidReceiveTitle(const String&) override;
  void DispatchDidChangeIcons(IconType) override;
  void DispatchDidCommitLoad(HistoryItem*,
                             WebHistoryCommitType,
                             WebGlobalObjectReusePolicy) override;
  void DispatchDidFailProvisionalLoad(const ResourceError&,
                                      WebHistoryCommitType) override;
  void DispatchDidFailLoad(const ResourceError&, WebHistoryCommitType) override;
  void DispatchDidFinishDocumentLoad() override;
  void DispatchDidFinishLoad() override;

  void DispatchDidChangeThemeColor() override;
  NavigationPolicy DecidePolicyForNavigation(
      const ResourceRequest&,
      Document* origin_document,
      DocumentLoader*,
      WebNavigationType,
      NavigationPolicy,
      bool has_transient_activation,
      bool should_replace_current_entry,
      bool is_client_redirect,
      WebTriggeringEventInfo,
      HTMLFormElement*,
      ContentSecurityPolicyDisposition should_bypass_main_world_csp,
      mojom::blink::BlobURLTokenPtr,
      base::TimeTicks input_start_time) override;
  void DispatchWillSendSubmitEvent(HTMLFormElement*) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void ProgressEstimateChanged(double progress_estimate) override;
  void ForwardResourceTimingToParent(const WebResourceTimingInfo&) override;
  void DownloadURL(const ResourceRequest&,
                   DownloadCrossOriginRedirects) override;
  void LoadErrorPage(int reason) override;
  bool NavigateBackForward(int offset) const override;
  void DidAccessInitialDocument() override;
  void DidDisplayInsecureContent() override;
  void DidContainInsecureFormAction() override;
  void DidRunInsecureContent(const SecurityOrigin*,
                             const KURL& insecure_url) override;
  void DidDispatchPingLoader(const KURL&) override;
  void DidDisplayContentWithCertificateErrors() override;
  void DidRunContentWithCertificateErrors() override;
  void ReportLegacySymantecCert(const KURL&, bool) override;
  void DidChangePerformanceTiming() override;
  void DidObserveLoadingBehavior(WebLoadingBehaviorFlag) override;
  void DidObserveNewFeatureUsage(mojom::WebFeature) override;
  void DidObserveNewCssPropertyUsage(int, bool) override;
  void DidObserveLayoutJank(double jank_fraction) override;
  bool ShouldTrackUseCounter(const KURL&) override;
  void SelectorMatchChanged(const Vector<String>& added_selectors,
                            const Vector<String>& removed_selectors) override;

  // Creates a WebDocumentLoaderImpl that is a DocumentLoader but also has:
  // - storage to store an extra data that can be used by the content layer
  // - wrapper methods to expose DocumentLoader's variables to the content
  //   layer
  DocumentLoader* CreateDocumentLoader(
      LocalFrame*,
      const ResourceRequest&,
      const SubstituteData&,
      ClientRedirectPolicy,
      const base::UnguessableToken& devtools_navigation_token,
      WebFrameLoadType,
      WebNavigationType,
      std::unique_ptr<WebNavigationParams> navigation_params,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) override;

  // Updates the underlying |WebDocumentLoaderImpl| of |DocumentLoader| with
  // extra_data.
  void UpdateDocumentLoader(
      DocumentLoader* document_loader,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) override;
  WTF::String UserAgent() override;
  WTF::String DoNotTrackValue() override;
  void TransitionToCommittedForNewPage() override;
  LocalFrame* CreateFrame(const WTF::AtomicString& name,
                          HTMLFrameOwnerElement*) override;
  WebPluginContainerImpl* CreatePlugin(HTMLPlugInElement&,
                                       const KURL&,
                                       const Vector<WTF::String>&,
                                       const Vector<WTF::String>&,
                                       const WTF::String&,
                                       bool load_manually) override;
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*,
      WebLayerTreeView*) override;
  WebRemotePlaybackClient* CreateWebRemotePlaybackClient(
      HTMLMediaElement&) override;
  void DidChangeScrollOffset() override;
  void DidUpdateCurrentHistoryItem() override;

  bool AllowContentInitiatedDataUrlNavigations(const KURL&) override;

  WebCookieJar* CookieJar() const override;
  void FrameFocused() const override;
  void DidChangeName(const String&) override;
  void DidEnforceInsecureRequestPolicy(WebInsecureRequestPolicy) override;
  void DidEnforceInsecureNavigationsSet(const std::vector<unsigned>&) override;
  void DidChangeFramePolicy(Frame* child_frame,
                            SandboxFlags,
                            const ParsedFeaturePolicy&) override;
  void DidSetFramePolicyHeaders(
      SandboxFlags,
      const ParsedFeaturePolicy& parsed_header) override;
  void DidAddContentSecurityPolicies(
      const blink::WebVector<WebContentSecurityPolicy>&) override;
  void DidChangeFrameOwnerProperties(HTMLFrameOwnerElement*) override;

  void DispatchWillStartUsingPeerConnectionHandler(
      WebRTCPeerConnectionHandler*) override;

  bool ShouldBlockWebGL() override;

  std::unique_ptr<WebServiceWorkerProvider> CreateServiceWorkerProvider()
      override;
  WebContentSettingsClient* GetContentSettingsClient() override;

  SharedWorkerRepositoryClient* GetSharedWorkerRepositoryClient() override;

  std::unique_ptr<WebApplicationCacheHost> CreateApplicationCacheHost(
      WebApplicationCacheHostClient*) override;

  void DispatchDidChangeManifest() override;

  unsigned BackForwardLength() override;

  void SuddenTerminationDisablerChanged(
      bool present,
      WebSuddenTerminationDisablerType) override;

  BlameContext* GetFrameBlameContext() override;

  WebEffectiveConnectionType GetEffectiveConnectionType() override;
  void SetEffectiveConnectionTypeForTesting(
      WebEffectiveConnectionType) override;

  WebURLRequest::PreviewsState GetPreviewsStateForFrame() const override;

  KURL OverrideFlashEmbedWithHTML(const KURL&) override;

  void NotifyUserActivation() override;
  void ConsumeUserActivation() override;

  void SetHasReceivedUserGestureBeforeNavigation(bool value) override;

  void AbortClientNavigation() override;

  WebSpellCheckPanelHostClient* SpellCheckPanelHostClient() const override;

  WebTextCheckClient* GetTextCheckerClient() const override;

  std::unique_ptr<WebURLLoaderFactory> CreateURLLoaderFactory() override;

  service_manager::InterfaceProvider* GetInterfaceProvider() override;
  AssociatedInterfaceProvider* GetRemoteNavigationAssociatedInterfaces()
      override;

  void AnnotatedRegionsChanged() override;

  void DidBlockFramebust(const KURL&) override;

  base::UnguessableToken GetDevToolsFrameToken() const override;

  void ScrollRectToVisibleInParentFrame(
      const WebRect&,
      const WebScrollIntoViewParams&) override;

  void BubbleLogicalScrollInParentFrame(ScrollDirection direction,
                                        ScrollGranularity granularity) override;

  String evaluateInInspectorOverlayForTesting(const String& script) override;

  bool HandleCurrentKeyboardEvent() override;

  void DidChangeSelection(bool is_selection_empty) override;

  void DidChangeContents() override;

  Frame* FindFrame(const AtomicString& name) const override;

  void FrameRectsChanged(const IntRect&) override;

  bool IsPluginHandledExternally(HTMLPlugInElement&,
                                 const KURL&,
                                 const String&) override;

  std::unique_ptr<WebWorkerFetchContext> CreateWorkerFetchContext() override;
  std::unique_ptr<WebContentSettingsClient> CreateWorkerContentSettingsClient()
      override;

  void SetMouseCapture(bool capture) override;

  bool UsePrintingLayout() const override;

 private:
  explicit LocalFrameClientImpl(WebLocalFrameImpl*);

  bool IsLocalFrameClientImpl() const override { return true; }
  WebDevToolsAgentImpl* DevToolsAgent();

  // The WebFrame that owns this object and manages its lifetime. Therefore,
  // the web frame object is guaranteed to exist.
  Member<WebLocalFrameImpl> web_frame_;

  String user_agent_;
};

DEFINE_TYPE_CASTS(LocalFrameClientImpl,
                  LocalFrameClient,
                  client,
                  client->IsLocalFrameClientImpl(),
                  client.IsLocalFrameClientImpl());

}  // namespace blink

#endif
