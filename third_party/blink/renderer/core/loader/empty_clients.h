/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
 * Copyright (C) 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_EMPTY_CLIENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_EMPTY_CLIENTS_H_

#include <memory>

#include "base/macros.h"
#include "cc/paint/paint_canvas.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_menu_source_type.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/public/platform/web_spell_check_panel_host_client.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "v8/include/v8.h"

/*
 This file holds empty Client stubs for use by WebCore.

 Viewless element needs to create a dummy Page->LocalFrame->FrameView tree for
 use in parsing or executing JavaScript. This tree depends heavily on Clients
 (usually provided by WebKit classes).

 This file was first created for SVGImage as it had no way to access the current
 Page (nor should it, since Images are not tied to a page). See
 http://bugs.webkit.org/show_bug.cgi?id=5971 for the original discussion about
 this file.

 Ideally, whenever you change a Client class, you should add a stub here.
 Brittle, yes. Unfortunate, yes. Hopefully temporary.
*/

namespace blink {

enum class GlobalObjectReusePolicy;

class CORE_EXPORT EmptyChromeClient : public ChromeClient {
 public:
  ~EmptyChromeClient() override = default;
  void ChromeDestroyed() override {}

  WebViewImpl* GetWebView() const override { return nullptr; }
  void SetWindowRect(const IntRect&, LocalFrame&) override {}
  IntRect RootWindowRect(LocalFrame&) override { return IntRect(); }

  void Focus(LocalFrame*) override {}

  bool CanTakeFocus(WebFocusType) override { return false; }
  void TakeFocus(WebFocusType) override {}

  void FocusedElementChanged(Element*, Element*) override {}
  void Show(NavigationPolicy) override {}

  void DidOverscroll(const FloatSize&,
                     const FloatSize&,
                     const FloatPoint&,
                     const FloatSize&) override {}
  void SetOverscrollBehavior(LocalFrame& frame,
                             const cc::OverscrollBehavior&) override {}

  void BeginLifecycleUpdates(LocalFrame& main_frame) override {}
  void StartDeferringCommits(LocalFrame& main_frame,
                             base::TimeDelta timeout) override {}
  void StopDeferringCommits(LocalFrame& main_frame,
                            cc::PaintHoldingCommitTrigger) override {}

  bool HadFormInteraction() const override { return false; }

  void StartDragging(LocalFrame*,
                     const WebDragData&,
                     WebDragOperationsMask,
                     const SkBitmap& drag_image,
                     const gfx::Point& drag_image_offset) override {}
  bool AcceptsLoadDrops() const override { return true; }

  bool ShouldReportDetailedMessageForSource(LocalFrame&,
                                            const String&) override {
    return false;
  }
  void AddMessageToConsole(LocalFrame*,
                           mojom::ConsoleMessageSource,
                           mojom::ConsoleMessageLevel,
                           const String&,
                           unsigned,
                           const String&,
                           const String&) override {}

  bool CanOpenBeforeUnloadConfirmPanel() override { return false; }
  bool OpenBeforeUnloadConfirmPanelDelegate(LocalFrame*, bool) override {
    return true;
  }

  void CloseWindowSoon() override {}

  Page* CreateWindowDelegate(LocalFrame*,
                             const FrameLoadRequest&,
                             const AtomicString&,
                             const WebWindowFeatures&,
                             WebSandboxFlags,
                             const FeaturePolicy::FeatureState&,
                             const SessionStorageNamespaceId&) override {
    return nullptr;
  }

  bool OpenJavaScriptAlertDelegate(LocalFrame*, const String&) override {
    return false;
  }
  bool OpenJavaScriptConfirmDelegate(LocalFrame*, const String&) override {
    return false;
  }
  bool OpenJavaScriptPromptDelegate(LocalFrame*,
                                    const String&,
                                    const String&,
                                    String&) override {
    return false;
  }

  bool HasOpenedPopup() const override { return false; }
  PopupMenu* OpenPopupMenu(LocalFrame&, HTMLSelectElement&) override;
  PagePopup* OpenPagePopup(PagePopupClient*) override { return nullptr; }
  void ClosePagePopup(PagePopup*) override {}
  DOMWindow* PagePopupWindowForTesting() const override { return nullptr; }

  bool TabsToLinks() override { return false; }

  void InvalidateRect(const IntRect&) override {}
  void ScheduleAnimation(const LocalFrameView*,
                         base::TimeDelta = base::TimeDelta()) override {}

  IntRect ViewportToScreen(const IntRect& r,
                           const LocalFrameView*) const override {
    return r;
  }
  float WindowToViewportScalar(LocalFrame*, const float s) const override {
    return s;
  }
  WebScreenInfo GetScreenInfo(LocalFrame&) const override {
    return WebScreenInfo();
  }
  void ContentsSizeChanged(LocalFrame*, const IntSize&) const override {}

  void ShowMouseOverURL(const HitTestResult&) override {}

  void SetToolTip(LocalFrame&, const String&, TextDirection) override {}

  void PrintDelegate(LocalFrame*) override {}

  ColorChooser* OpenColorChooser(LocalFrame*,
                                 ColorChooserClient*,
                                 const Color&) override;
  DateTimeChooser* OpenDateTimeChooser(
      LocalFrame* frame,
      DateTimeChooserClient*,
      const DateTimeChooserParameters&) override;
  void OpenTextDataListChooser(HTMLInputElement&) override;

  void OpenFileChooser(LocalFrame*, scoped_refptr<FileChooser>) override;

  void SetCursor(const Cursor&, LocalFrame* local_root) override {}
  void SetCursorOverridden(bool) override {}
  Cursor LastSetCursorForTesting() const override { return PointerCursor(); }

  void AttachRootLayer(scoped_refptr<cc::Layer>,
                       LocalFrame* local_root) override;

  void SetEventListenerProperties(LocalFrame*,
                                  cc::EventListenerClass,
                                  cc::EventListenerProperties) override {}
  cc::EventListenerProperties EventListenerProperties(
      LocalFrame*,
      cc::EventListenerClass event_class) const override {
    return cc::EventListenerProperties::kNone;
  }
  void SetHasScrollEventHandlers(LocalFrame*, bool) override {}
  void SetNeedsLowLatencyInput(LocalFrame*, bool) override {}
  void SetNeedsUnbufferedInputForDebugger(LocalFrame*, bool) override {}
  void RequestUnbufferedInputEvents(LocalFrame*) override {}
  void SetTouchAction(LocalFrame*, TouchAction) override {}

  void DidAssociateFormControlsAfterLoad(LocalFrame*) override {}

  String AcceptLanguages() override;

  void RegisterPopupOpeningObserver(PopupOpeningObserver*) override {}
  void UnregisterPopupOpeningObserver(PopupOpeningObserver*) override {}
  void NotifyPopupOpeningObservers() const override {}

  void FallbackCursorModeLockCursor(LocalFrame* frame,
                                    bool left,
                                    bool right,
                                    bool up,
                                    bool down) override {}

  void FallbackCursorModeSetCursorVisibility(LocalFrame* frame,
                                             bool visible) override {}

  void RequestBeginMainFrameNotExpected(LocalFrame& frame,
                                        bool request) override {}
  int GetLayerTreeId(LocalFrame& frame) override { return 0; }

  void SetCursorForPlugin(const WebCursorInfo&, LocalFrame*) override {}

  void InstallSupplements(LocalFrame&) override {}

  void MainFrameScrollOffsetChanged(LocalFrame& main_frame) const override {}
};

class CORE_EXPORT EmptyLocalFrameClient : public LocalFrameClient {
 public:
  EmptyLocalFrameClient() = default;
  ~EmptyLocalFrameClient() override = default;

  bool HasWebView() const override { return true; }  // mainly for assertions

  bool InShadowTree() const override { return false; }

  Frame* Opener() const override { return nullptr; }
  void SetOpener(Frame*) override {}

  Frame* Parent() const override { return nullptr; }
  Frame* Top() const override { return nullptr; }
  Frame* NextSibling() const override { return nullptr; }
  Frame* FirstChild() const override { return nullptr; }
  void WillBeDetached() override {}
  void Detached(FrameDetachType) override {}

  void DispatchWillSendRequest(ResourceRequest&) override {}
  void DispatchDidLoadResourceFromMemoryCache(
      const ResourceRequest&,
      const ResourceResponse&) override {}

  void DispatchDidHandleOnloadEvents() override {}
  void DispatchDidStartProvisionalLoad(DocumentLoader*) override {}
  void DispatchDidReceiveTitle(const String&) override {}
  void DispatchDidChangeIcons(IconType) override {}
  void DispatchDidCommitLoad(HistoryItem*,
                             WebHistoryCommitType,
                             GlobalObjectReusePolicy) override {}
  void DispatchDidFailLoad(const ResourceError&,
                           WebHistoryCommitType) override {}
  void DispatchDidFinishDocumentLoad() override {}
  void DispatchDidFinishLoad() override {}

  void BeginNavigation(
      const ResourceRequest&,
      network::mojom::RequestContextFrameType,
      Document* origin_document,
      DocumentLoader*,
      WebNavigationType,
      NavigationPolicy,
      bool,
      WebFrameLoadType,
      bool,
      TriggeringEventInfo,
      HTMLFormElement*,
      ContentSecurityPolicyDisposition,
      mojo::PendingRemote<mojom::blink::BlobURLToken>,
      base::TimeTicks,
      const String&,
      WebContentSecurityPolicyList,
      network::mojom::IPAddressSpace,
      mojo::PendingRemote<mojom::blink::NavigationInitiator>) override;

  void DispatchWillSendSubmitEvent(HTMLFormElement*) override;

  void DidStartLoading() override {}
  void ProgressEstimateChanged(double) override {}
  void DidStopLoading() override {}

  void ForwardResourceTimingToParent(const WebResourceTimingInfo&) override {}

  void DownloadURL(const ResourceRequest&,
                   network::mojom::RedirectMode) override {}

  DocumentLoader* CreateDocumentLoader(
      LocalFrame*,
      WebNavigationType,
      std::unique_ptr<WebNavigationParams> navigation_params,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) override;
  void UpdateDocumentLoader(
      DocumentLoader* document_loader,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) override {}

  String UserAgent() override { return ""; }
  blink::UserAgentMetadata UserAgentMetadata() override {
    return blink::UserAgentMetadata();
  }

  String DoNotTrackValue() override { return String(); }

  void TransitionToCommittedForNewPage() override {}

  bool NavigateBackForward(int offset) const override { return false; }
  void DidRunInsecureContent(const SecurityOrigin*, const KURL&) override {}
  void DidDispatchPingLoader(const KURL&) override {}
  void DidDisplayContentWithCertificateErrors() override {}
  void DidRunContentWithCertificateErrors() override {}
  void SelectorMatchChanged(const Vector<String>&,
                            const Vector<String>&) override {}
  LocalFrame* CreateFrame(const AtomicString&, HTMLFrameOwnerElement*) override;
  std::pair<RemoteFrame*, base::UnguessableToken> CreatePortal(
      HTMLPortalElement*,
      mojo::PendingAssociatedReceiver<mojom::blink::Portal>,
      mojo::PendingAssociatedRemote<mojom::blink::PortalClient>) override;
  RemoteFrame* AdoptPortal(HTMLPortalElement*) override;
  WebPluginContainerImpl* CreatePlugin(HTMLPlugInElement&,
                                       const KURL&,
                                       const Vector<String>&,
                                       const Vector<String>&,
                                       const String&,
                                       bool) override;
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override;
  WebRemotePlaybackClient* CreateWebRemotePlaybackClient(
      HTMLMediaElement&) override;

  void DidCreateNewDocument() override {}
  void DispatchDidClearWindowObjectInMainWorld() override {}
  void DocumentElementAvailable() override {}
  void RunScriptsAtDocumentElementAvailable() override {}
  void RunScriptsAtDocumentReady(bool) override {}
  void RunScriptsAtDocumentIdle() override {}

  void DidCreateScriptContext(v8::Local<v8::Context>,
                              int32_t world_id) override {}
  void WillReleaseScriptContext(v8::Local<v8::Context>,
                                int32_t world_id) override {}
  bool AllowScriptExtensions() override { return false; }

  service_manager::InterfaceProvider* GetInterfaceProvider() override {
    return &interface_provider_;
  }

  BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() override {
    return GetEmptyBrowserInterfaceBroker();
  }

  AssociatedInterfaceProvider* GetRemoteNavigationAssociatedInterfaces()
      override;

  WebSpellCheckPanelHostClient* SpellCheckPanelHostClient() const override {
    return nullptr;
  }

  std::unique_ptr<WebServiceWorkerProvider> CreateServiceWorkerProvider()
      override;
  WebContentSettingsClient* GetContentSettingsClient() override {
    return nullptr;
  }

  void SetTextCheckerClientForTesting(WebTextCheckClient*);
  WebTextCheckClient* GetTextCheckerClient() const override;

  std::unique_ptr<WebURLLoaderFactory> CreateURLLoaderFactory() override {
    // Most consumers of EmptyLocalFrameClient should not make network requests.
    // If an exception needs to be made (e.g. in test code), then the consumer
    // should define their own subclass of LocalFrameClient or
    // EmptyLocalFrameClient and override the CreateURLLoaderFactory method.
    // See also https://crbug.com/891872.
    NOTREACHED();
    return nullptr;
  }

  void BubbleLogicalScrollInParentFrame(
      ScrollDirection direction,
      ScrollGranularity granularity) override {}

  void AnnotatedRegionsChanged() override {}
  base::UnguessableToken GetDevToolsFrameToken() const override {
    return base::UnguessableToken::Create();
  }
  String evaluateInInspectorOverlayForTesting(const String& script) override {
    return g_empty_string;
  }

  Frame* FindFrame(const AtomicString& name) const override;

 protected:
  // Not owned
  WebTextCheckClient* text_check_client_;

  service_manager::InterfaceProvider interface_provider_;

  DISALLOW_COPY_AND_ASSIGN(EmptyLocalFrameClient);
};

class EmptySpellCheckPanelHostClient : public WebSpellCheckPanelHostClient {
  USING_FAST_MALLOC(EmptySpellCheckPanelHostClient);

 public:
  EmptySpellCheckPanelHostClient() = default;

  void ShowSpellingUI(bool) override {}
  bool IsShowingSpellingUI() override { return false; }
  void UpdateSpellingUIWithMisspelledWord(const WebString&) override {}

  DISALLOW_COPY_AND_ASSIGN(EmptySpellCheckPanelHostClient);
};

class CORE_EXPORT EmptyRemoteFrameClient : public RemoteFrameClient {
 public:
  EmptyRemoteFrameClient();

  // RemoteFrameClient implementation.
  void Navigate(const ResourceRequest&,
                bool should_replace_current_entry,
                bool is_opener_navigation,
                bool prevent_sandboxed_download,
                bool initiator_frame_is_ad,
                mojo::PendingRemote<mojom::blink::BlobURLToken>) override {}
  unsigned BackForwardLength() override { return 0; }
  void CheckCompleted() override {}
  void ForwardPostMessage(MessageEvent*,
                          scoped_refptr<const SecurityOrigin> target,
                          LocalFrame* source_frame) const override {}
  void FrameRectsChanged(const IntRect& local_frame_rect,
                         const IntRect& transformed_frame_rect) override {}
  void UpdateRemoteViewportIntersection(
      const ViewportIntersectionState& intersection_state) override {}
  void AdvanceFocus(WebFocusType, LocalFrame* source) override {}
  void SetIsInert(bool) override {}
  void UpdateRenderThrottlingStatus(bool is_throttled,
                                    bool subtree_throttled) override {}
  uint32_t Print(const IntRect& rect, cc::PaintCanvas* canvas) const override {
    return 0;
  }

  // FrameClient implementation.
  bool InShadowTree() const override { return false; }
  void Detached(FrameDetachType) override {}
  Frame* Opener() const override { return nullptr; }
  void SetOpener(Frame*) override {}
  Frame* Parent() const override { return nullptr; }
  Frame* Top() const override { return nullptr; }
  Frame* NextSibling() const override { return nullptr; }
  Frame* FirstChild() const override { return nullptr; }
  base::UnguessableToken GetDevToolsFrameToken() const override {
    return base::UnguessableToken::Create();
  }

  DISALLOW_COPY_AND_ASSIGN(EmptyRemoteFrameClient);
};

CORE_EXPORT void FillWithEmptyClients(Page::PageClients&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_EMPTY_CLIENTS_H_
