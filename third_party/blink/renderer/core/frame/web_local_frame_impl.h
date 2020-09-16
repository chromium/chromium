/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_LOCAL_FRAME_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_LOCAL_FRAME_IMPL_H_

#include <memory>
#include <set>
#include <utility>

#include "base/single_thread_task_runner.h"
#include "base/util/type_safety/pass_key.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/mojom/ad_tagging/ad_frame.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom-blink.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_file_system_type.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/exported/web_input_method_controller_impl.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ChromePrintContext;
class FindInPage;
class HTMLPortalElement;
class IntSize;
class LocalFrameClientImpl;
class ResourceError;
class ScrollableArea;
class TextFinder;
class WebAssociatedURLLoader;
struct WebAssociatedURLLoaderOptions;
class WebAutofillClient;
class WebContentSettingsClient;
class WebDevToolsAgentImpl;
class WebLocalFrameClient;
class WebFrameWidgetBase;
class WebNode;
class WebPerformance;
class WebRemoteFrameImpl;
class WebScriptExecutionCallback;
class WebSpellCheckPanelHostClient;
class WebView;
class WebViewImpl;
enum class WebFrameLoadType;
struct WebPrintParams;
class WindowAgentFactory;

template <typename T>
class WebVector;

// Implementation of WebFrame, note that this is a reference counted object.
class CORE_EXPORT WebLocalFrameImpl final
    : public GarbageCollected<WebLocalFrameImpl>,
      public WebNavigationControl {
 public:
  // WebFrame overrides:
  void Close() override;
  WebView* View() const override;
  v8::Local<v8::Object> GlobalProxy() const override;
  bool IsLoading() const override;

  // WebLocalFrame overrides:
  WebLocalFrameImpl* CreateLocalChild(
      mojom::blink::TreeScopeType,
      WebLocalFrameClient*,
      blink::InterfaceRegistry*,
      const base::UnguessableToken& frame_token) override;
  WebLocalFrameClient* Client() const override { return client_; }
  void SetAutofillClient(WebAutofillClient*) override;
  WebAutofillClient* AutofillClient() override;
  void SetContentCaptureClient(WebContentCaptureClient*) override;
  WebContentCaptureClient* ContentCaptureClient() const override;
  WebDocument GetDocument() const override;
  WebString AssignedName() const override;
  ui::AXTreeID GetAXTreeID() const override;
  void SetName(const WebString&) override;
  bool IsProvisional() const override;
  WebLocalFrameImpl* LocalRoot() override;
  WebFrameWidget* FrameWidget() const override;
  WebFrame* FindFrameByName(const WebString& name) override;
  void SetEmbeddingToken(
      const base::UnguessableToken& embedding_token) override;
  const base::Optional<base::UnguessableToken>& GetEmbeddingToken()
      const override;
  void SendPings(const WebURL& destination_url) override;
  void StartReload(WebFrameLoadType) override;
  void StartNavigation(const WebURLRequest&) override;
  void EnableViewSourceMode(bool enable) override;
  bool IsViewSourceModeEnabled() const override;
  WebDocumentLoader* GetDocumentLoader() const override;
  void SetReferrerForRequest(WebURLRequest&, const WebURL& referrer) override;
  bool IsNavigationScheduledWithin(base::TimeDelta interval) const override;
  void BlinkFeatureUsageReport(blink::mojom::WebFeature feature) override;
  void MixedContentFound(const WebURL& main_resource_url,
                         const WebURL& mixed_content_url,
                         mojom::RequestContextType,
                         bool was_allowed,
                         const WebURL& url_before_redirects,
                         bool had_redirect,
                         const WebSourceLocation&) override;
  void SendOrientationChangeEvent() override;
  PageSizeType GetPageSizeType(uint32_t page_index) override;
  void GetPageDescription(uint32_t page_index,
                          WebPrintPageDescription*) override;
  void ExecuteScript(const WebScriptSource&) override;
  void ExecuteScriptInIsolatedWorld(int32_t world_id,
                                    const WebScriptSource&) override;
  WARN_UNUSED_RESULT v8::Local<v8::Value>
  ExecuteScriptInIsolatedWorldAndReturnValue(int32_t world_id,
                                             const WebScriptSource&) override;
  void ClearIsolatedWorldCSPForTesting(int32_t world_id) override;
  v8::Local<v8::Value> ExecuteScriptAndReturnValue(
      const WebScriptSource&) override;
  v8::MaybeLocal<v8::Value> CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>,
      v8::Local<v8::Value>,
      int argc,
      v8::Local<v8::Value> argv[]) override;
  v8::Local<v8::Context> MainWorldScriptContext() const override;
  int32_t GetScriptContextWorldId(
      v8::Local<v8::Context> script_context) const override;
  void RequestExecuteScriptAndReturnValue(const WebScriptSource&,
                                          bool user_gesture,
                                          WebScriptExecutionCallback*) override;
  void RequestExecuteV8Function(v8::Local<v8::Context>,
                                v8::Local<v8::Function>,
                                v8::Local<v8::Value> receiver,
                                int argc,
                                v8::Local<v8::Value> argv[],
                                WebScriptExecutionCallback*) override;
  void RequestExecuteScriptInIsolatedWorld(
      int32_t world_id,
      const WebScriptSource* source_in,
      unsigned num_sources,
      bool user_gesture,
      ScriptExecutionType,
      WebScriptExecutionCallback*) override;
  void Alert(const WebString& message) override;
  bool Confirm(const WebString& message) override;
  WebString Prompt(const WebString& message,
                   const WebString& default_value) override;
  void BindDevToolsAgent(CrossVariantMojoAssociatedRemote<
                             mojom::blink::DevToolsAgentHostInterfaceBase>
                             devtools_agent_host_remote,
                         CrossVariantMojoAssociatedReceiver<
                             mojom::blink::DevToolsAgentInterfaceBase>
                             devtools_agent_receiver) override;
  void UnmarkText() override;
  bool HasMarkedText() const override;
  WebRange MarkedRange() const override;
  bool FirstRectForCharacterRange(unsigned location,
                                  unsigned length,
                                  WebRect&) const override;
  bool ExecuteCommand(const WebString&) override;
  bool ExecuteCommand(const WebString&, const WebString& value) override;
  bool IsCommandEnabled(const WebString&) const override;
  bool SelectionTextDirection(base::i18n::TextDirection& start,
                              base::i18n::TextDirection& end) const override;
  bool IsSelectionAnchorFirst() const override;
  void SetTextDirectionForTesting(base::i18n::TextDirection direction) override;
  bool HasSelection() const override;
  WebRange SelectionRange() const override;
  WebString SelectionAsText() const override;
  WebString SelectionAsMarkup() const override;
  void TextSelectionChanged(const WebString& selection_text,
                            uint32_t offset,
                            const gfx::Range& range) override;
  bool SelectWordAroundCaret() override;
  void SelectRange(const gfx::Point& base, const gfx::Point& extent) override;
  void SelectRange(const WebRange&,
                   HandleVisibilityBehavior,
                   blink::mojom::SelectionMenuBehavior) override;
  WebString RangeAsText(const WebRange&) override;
  void MoveRangeSelection(
      const gfx::Point& base,
      const gfx::Point& extent,
      WebFrame::TextGranularity = kCharacterGranularity) override;
  void MoveCaretSelection(const gfx::Point&) override;
  bool SetEditableSelectionOffsets(int start, int end) override;
  bool AddImeTextSpansToExistingText(
      const WebVector<ui::ImeTextSpan>& ime_text_spans,
      unsigned text_start,
      unsigned text_end) override;
  bool ClearImeTextSpansByType(ui::ImeTextSpan::Type type,
                               unsigned text_start,
                               unsigned text_end) override;
  bool SetCompositionFromExistingText(
      int composition_start,
      int composition_end,
      const WebVector<ui::ImeTextSpan>& ime_text_spans) override;
  void ExtendSelectionAndDelete(int before, int after) override;
  void MoveRangeSelectionExtent(const gfx::Point&) override;
  void ReplaceSelection(const WebString&) override;
  void DeleteSurroundingText(int before, int after) override;
  void DeleteSurroundingTextInCodePoints(int before, int after) override;
  void ExtractSmartClipData(WebRect rect_in_viewport,
                            WebString& clip_text,
                            WebString& clip_html,
                            WebRect& clip_rect) override;
  void SetTextCheckClient(WebTextCheckClient*) override;
  void SetSpellCheckPanelHostClient(WebSpellCheckPanelHostClient*) override;
  WebSpellCheckPanelHostClient* SpellCheckPanelHostClient() const override {
    return spell_check_panel_host_client_;
  }
  void ReplaceMisspelledRange(const WebString&) override;
  void RemoveSpellingMarkers() override;
  void RemoveSpellingMarkersUnderWords(
      const WebVector<WebString>& words) override;
  void SetContentSettingsClient(WebContentSettingsClient*) override;
  void ReloadImage(const WebNode&) override;
  bool IsAllowedToDownload() const override;
  bool FindForTesting(int identifier,
                      const WebString& search_text,
                      bool match_case,
                      bool forward,
                      bool force,
                      bool new_session,
                      bool wrap_within_frame,
                      bool async) override;
  void SetTickmarks(const WebVector<WebRect>&) override;
  WebNode ContextMenuNode() const override;
  void CopyImageAtForTesting(const gfx::Point&) override;
  void UsageCountChromeLoadTimes(const WebString& metric) override;
  bool DispatchedPagehideAndStillHidden() const override;
  FrameScheduler* Scheduler() const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override;
  WebInputMethodController* GetInputMethodController() override;
  WebAssociatedURLLoader* CreateAssociatedURLLoader(
      const WebAssociatedURLLoaderOptions&) override;
  void StopLoading() override;
  WebSize GetScrollOffset() const override;
  void SetScrollOffset(const WebSize&) override;
  WebSize DocumentSize() const override;
  bool HasVisibleContent() const override;
  WebRect VisibleContentRect() const override;
  void DispatchBeforePrintEvent(
      base::WeakPtr<WebPrintClient> print_client) override;
  WebPlugin* GetPluginToPrint(const WebNode& constrain_to_node) override;
  uint32_t PrintBegin(const WebPrintParams&,
                      const WebNode& constrain_to_node) override;
  float GetPrintPageShrink(uint32_t page) override;
  float PrintPage(uint32_t page_to_print, cc::PaintCanvas*) override;
  void PrintEnd() override;
  void DispatchAfterPrintEvent() override;
  bool GetPrintPresetOptionsForPlugin(const WebNode&,
                                      WebPrintPresetOptions*) override;
  bool CapturePaintPreview(const WebRect& bounds,
                           cc::PaintCanvas* canvas,
                           bool include_linked_destinations) override;
  bool ShouldSuppressKeyboardForFocusedElement() override;
  WebPerformance Performance() const override;
  bool IsAdSubframe() const override;
  void SetIsAdSubframe(blink::mojom::AdFrameType ad_frame_type) override;
  WebSize SpoolSizeInPixelsForTesting(const WebSize& page_size_in_pixels,
                                      uint32_t page_count) override;
  void PrintPagesForTesting(cc::PaintCanvas*,
                            const WebSize& page_size_in_pixels,
                            const WebSize& spool_size_in_pixels) override;
  WebRect GetSelectionBoundsRectForTesting() const override;
  gfx::Point GetPositionInViewportForTesting() const override;
  void WasHidden() override;
  void WasShown() override;
  void SetAllowsCrossBrowsingInstanceFrameLookup() override;
  void NotifyUserActivation(
      mojom::blink::UserActivationNotificationType notification_type) override;
  bool HasStickyUserActivation() override;
  bool HasTransientUserActivation() override;
  bool ConsumeTransientUserActivation(UserActivationUpdateSource) override;
  void SetOptimizationGuideHints(const WebOptimizationGuideHints&) override;

  // WebNavigationControl overrides:
  bool DispatchBeforeUnloadEvent(bool) override;
  void CommitNavigation(
      std::unique_ptr<WebNavigationParams> navigation_params,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) override;
  blink::mojom::CommitResult CommitSameDocumentNavigation(
      const WebURL&,
      WebFrameLoadType,
      const WebHistoryItem&,
      bool is_client_redirect,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) override;
  void LoadJavaScriptURL(const WebURL&) override;
  FallbackContentResult MaybeRenderFallbackContent(
      const WebURLError&) const override;
  void SetCommittedFirstRealLoad() override;
  bool HasCommittedFirstRealLoad() override;
  bool WillStartNavigation(const WebNavigationInfo&) override;
  void DidDropNavigation() override;
  void DownloadURL(
      const WebURLRequest& request,
      network::mojom::blink::RedirectMode cross_origin_redirect_behavior,
      CrossVariantMojoRemote<mojom::blink::BlobURLTokenInterfaceBase>
          blob_url_token) override;

  void InitializeCoreFrame(
      Page&,
      FrameOwner*,
      WebFrame* parent,
      WebFrame* previous_sibling,
      FrameInsertType,
      const AtomicString& name,
      WindowAgentFactory*,
      WebFrame* opener,
      network::mojom::blink::WebSandboxFlags sandbox_flags =
          network::mojom::blink::WebSandboxFlags::kNone,
      const FeaturePolicyFeatureState& opener_feature_state =
          FeaturePolicyFeatureState());
  LocalFrame* GetFrame() const { return frame_.Get(); }

  void WillBeDetached();
  void WillDetachParent();
  void CollectGarbageForTesting();

  static WebLocalFrameImpl* CreateMainFrame(
      WebView*,
      WebLocalFrameClient*,
      InterfaceRegistry*,
      const base::UnguessableToken& frame_token,
      WebFrame* opener,
      const WebString& name,
      network::mojom::blink::WebSandboxFlags,
      const FeaturePolicyFeatureState&);
  static WebLocalFrameImpl* CreateProvisional(
      WebLocalFrameClient*,
      InterfaceRegistry*,
      const base::UnguessableToken& frame_token,
      WebFrame*,
      const FramePolicy&,
      const WebString& name);

  WebLocalFrameImpl(util::PassKey<WebLocalFrameImpl>,
                    mojom::blink::TreeScopeType,
                    WebLocalFrameClient*,
                    blink::InterfaceRegistry*,
                    const base::UnguessableToken& frame_token);
  WebLocalFrameImpl(util::PassKey<WebRemoteFrameImpl>,
                    mojom::blink::TreeScopeType,
                    WebLocalFrameClient*,
                    blink::InterfaceRegistry*,
                    const base::UnguessableToken& frame_token);
  ~WebLocalFrameImpl() override;

  LocalFrame* CreateChildFrame(const AtomicString& name,
                               HTMLFrameOwnerElement*);
  std::pair<RemoteFrame*, PortalToken> CreatePortal(
      HTMLPortalElement*,
      mojo::PendingAssociatedReceiver<mojom::blink::Portal>,
      mojo::PendingAssociatedRemote<mojom::blink::PortalClient>);
  RemoteFrame* AdoptPortal(HTMLPortalElement*);

  void DidChangeContentsSize(const IntSize&);

  bool HasDevToolsOverlays() const;
  void UpdateDevToolsOverlaysPrePaint();
  void PaintDevToolsOverlays(GraphicsContext&);  // For CompositeAfterPaint.

  void CreateFrameView();

  // Sometimes Blink makes Page/Frame for internal purposes like for SVGImage
  // (see comments in third_party/blink/renderer/core/page/page.h). In that
  // case, such frames are not associated with a WebLocalFrame(Impl).
  // So note that FromFrame may return nullptr even for non-null frames.
  static WebLocalFrameImpl* FromFrame(LocalFrame*);
  static WebLocalFrameImpl* FromFrame(LocalFrame&);

  WebViewImpl* ViewImpl() const;

  LocalFrameView* GetFrameView() const {
    return GetFrame() ? GetFrame()->View() : nullptr;
  }

  void SetDevToolsAgentImpl(WebDevToolsAgentImpl*);
  WebDevToolsAgentImpl* DevToolsAgentImpl();

  // Instructs devtools to pause loading of the frame as soon as it's shown
  // until explicit command from the devtools client. May only be called on a
  // local root.
  void WaitForDebuggerWhenShown();

  // When a Find operation ends, we want to set the selection to what was active
  // and set focus to the first focusable node we find (starting with the first
  // node in the matched range and going up the inheritance chain). If we find
  // nothing to focus we focus the first focusable node in the range. This
  // allows us to set focus to a link (when we find text inside a link), which
  // allows us to navigate by pressing Enter after closing the Find box.
  void SetFindEndstateFocusAndSelection();

  void DidFailLoad(const ResourceError&, WebHistoryCommitType);
  void DidFinish();

  void SetClient(WebLocalFrameClient* client) { client_ = client; }

  WebFrameWidgetBase* FrameWidgetImpl() { return frame_widget_; }

  WebContentSettingsClient* GetContentSettingsClient() {
    return content_settings_client_;
  }

  WebTextCheckClient* GetTextCheckerClient() const {
    return text_check_client_;
  }

  FindInPage* GetFindInPage() const { return find_in_page_; }

  TextFinder* GetTextFinder() const;
  // Returns the text finder object if it already exists.
  // Otherwise creates it and then returns.
  TextFinder& EnsureTextFinder();

  void SetFrameWidget(WebFrameWidgetBase*);

  // TODO(dcheng): Remove this and make |FrameWidget()| always return something
  // useful.
  WebFrameWidgetBase* LocalRootFrameWidget();

  // Returns true if the frame is focused.
  bool IsFocused() const;

  // Returns true if our print context suggests using printing layout.
  bool UsePrintingLayout() const;

  // Copy the current selection to the pboard.
  void CopyToFindPboard();

  virtual void Trace(Visitor*) const;

 protected:
  // WebLocalFrame protected overrides:
  void AddMessageToConsoleImpl(const WebConsoleMessage&,
                               bool discard_duplicates) override;

  void AddInspectorIssueImpl(mojom::blink::InspectorIssueCode code) override;

 private:
  friend LocalFrameClientImpl;

  // Sets the local core frame and registers destruction observers.
  void SetCoreFrame(LocalFrame*);

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to call these on a WebLocalFrameImpl.
  bool IsWebLocalFrame() const override;
  WebLocalFrame* ToWebLocalFrame() override;
  bool IsWebRemoteFrame() const override;
  WebRemoteFrame* ToWebRemoteFrame() override;

  HitTestResult HitTestResultForVisualViewportPos(const IntPoint&);

  WebPlugin* FocusedPluginIfInputMethodSupported();
  ScrollableArea* LayoutViewport() const;

  // A helper for DispatchBeforePrintEvent() and DispatchAfterPrintEvent().
  void DispatchPrintEventRecursively(const AtomicString& event_type);

  WebPluginContainerImpl* GetPluginToPrintHelper(
      const WebNode& constrain_to_node);

  Node* ContextMenuNodeInner() const;

  WebLocalFrameClient* client_;

  // TODO(dcheng): Inline this field directly rather than going through Member.
  const Member<LocalFrameClientImpl> local_frame_client_;

  // The embedder retains a reference to the WebCore LocalFrame while it is
  // active in the DOM. This reference is released when the frame is removed
  // from the DOM or the entire page is closed.
  Member<LocalFrame> frame_;

  // This is set if the frame is the root of a local frame tree, and requires a
  // widget for layout.
  Member<WebFrameWidgetBase> frame_widget_;

  Member<WebDevToolsAgentImpl> dev_tools_agent_;

  WebAutofillClient* autofill_client_;

  WebContentCaptureClient* content_capture_client_ = nullptr;

  WebContentSettingsClient* content_settings_client_ = nullptr;

  Member<FindInPage> find_in_page_;

  // Optional weak pointer to the WebPrintClient that initiated printing. Only
  // valid when |is_in_printing_| is true.
  base::WeakPtr<WebPrintClient> print_client_;

  // Valid between calls to BeginPrint() and EndPrint(). Containts the print
  // information. Is used by PrintPage().
  Member<ChromePrintContext> print_context_;

  // Borrowed pointers to Mojo objects.
  blink::InterfaceRegistry* interface_registry_;

  WebInputMethodControllerImpl input_method_controller_;

  WebTextCheckClient* text_check_client_;

  WebSpellCheckPanelHostClient* spell_check_panel_host_client_;

  // Oilpan: WebLocalFrameImpl must remain alive until close() is called.
  // Accomplish that by keeping a self-referential Persistent<>. It is
  // cleared upon close().
  SelfKeepAlive<WebLocalFrameImpl> self_keep_alive_;

  // True if DispatchBeforePrintEvent() was called, and
  // DispatchAfterPrintEvent() is not called yet.
  // TODO(crbug.com/1121077) After fixing the bug, make this member variable
  // only available when DCHECK_IS_ON().
  bool is_in_printing_ = false;
};

template <>
struct DowncastTraits<WebLocalFrameImpl> {
  static bool AllowFrom(const WebFrame& frame) {
    return frame.IsWebLocalFrame();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_LOCAL_FRAME_IMPL_H_
