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

#include "base/single_thread_task_runner.h"

#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/public/platform/web_file_system_type.h"
#include "third_party/blink/public/web/devtools_agent.mojom-blink.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/exported/web_input_method_controller_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/wtf/compiler.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ChromePrintContext;
class FindInPage;
class IntSize;
class LocalFrameClientImpl;
class ScrollableArea;
class SharedWorkerRepositoryClientImpl;
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
class WebScriptExecutionCallback;
class WebSpellCheckPanelHostClient;
class WebView;
class WebViewImpl;
enum class WebFrameLoadType;
struct WebContentSecurityPolicyViolation;
struct WebPrintParams;

template <typename T>
class WebVector;

// Implementation of WebFrame, note that this is a reference counted object.
class CORE_EXPORT WebLocalFrameImpl final
    : public GarbageCollectedFinalized<WebLocalFrameImpl>,
      public WebLocalFrame {
 public:
  // WebFrame methods:
  // TODO(dcheng): Fix sorting here; a number of method have been moved to
  // WebLocalFrame but not correctly updated here.
  void Close() override;
  WebString AssignedName() const override;
  void SetName(const WebString&) override;
  WebVector<WebIconURL> IconURLs(int icon_types_mask) const override;
  void SetSharedWorkerRepositoryClient(
      WebSharedWorkerRepositoryClient*) override;
  WebSize GetScrollOffset() const override;
  void SetScrollOffset(const WebSize&) override;
  WebSize DocumentSize() const override;
  bool HasVisibleContent() const override;
  WebRect VisibleContentRect() const override;
  WebView* View() const override;
  WebDocument GetDocument() const override;
  WebPerformance Performance() const override;
  bool IsAdSubframe() const override;
  void SetIsAdSubframe() override;
  void DispatchUnloadEvent() override;
  void ExecuteScript(const WebScriptSource&) override;
  void ExecuteScriptInIsolatedWorld(int world_id,
                                    const WebScriptSource&) override;
  WARN_UNUSED_RESULT v8::Local<v8::Value>
  ExecuteScriptInIsolatedWorldAndReturnValue(int world_id,
                                             const WebScriptSource&) override;
  void SetIsolatedWorldSecurityOrigin(int world_id,
                                      const WebSecurityOrigin&) override;
  void SetIsolatedWorldContentSecurityPolicy(int world_id,
                                             const WebString&) override;
  void SetIsolatedWorldHumanReadableName(int world_id,
                                         const WebString&) override;
  void AddMessageToConsole(const WebConsoleMessage&) override;
  void Alert(const WebString& message) override;
  bool Confirm(const WebString& message) override;
  WebString Prompt(const WebString& message,
                   const WebString& default_value) override;

  void CollectGarbageForTesting();
  v8::Local<v8::Value> ExecuteScriptAndReturnValue(
      const WebScriptSource&) override;
  void RequestExecuteScriptAndReturnValue(const WebScriptSource&,
                                          bool user_gesture,
                                          WebScriptExecutionCallback*) override;
  void RequestExecuteV8Function(v8::Local<v8::Context>,
                                v8::Local<v8::Function>,
                                v8::Local<v8::Value> receiver,
                                int argc,
                                v8::Local<v8::Value> argv[],
                                WebScriptExecutionCallback*) override;
  void PostPausableTask(PausableTaskCallback) override;
  void RequestExecuteScriptInIsolatedWorld(
      int world_id,
      const WebScriptSource* source_in,
      unsigned num_sources,
      bool user_gesture,
      ScriptExecutionType,
      WebScriptExecutionCallback*) override;
  v8::MaybeLocal<v8::Value> CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>,
      v8::Local<v8::Value>,
      int argc,
      v8::Local<v8::Value> argv[]) override;
  v8::Local<v8::Context> MainWorldScriptContext() const override;
  v8::Local<v8::Object> GlobalProxy() const override;
  void StartReload(WebFrameLoadType) override;
  void ReloadImage(const WebNode&) override;
  void ReloadLoFiImages() override;
  void StartNavigation(const WebURLRequest&) override;
  void CheckCompleted() override;
  void LoadHTMLString(const WebData& html,
                      const WebURL& base_url,
                      const WebURL& unreachable_url) override;
  void StopLoading() override;
  WebDocumentLoader* GetProvisionalDocumentLoader() const override;
  WebDocumentLoader* GetDocumentLoader() const override;
  void EnableViewSourceMode(bool enable) override;
  bool IsViewSourceModeEnabled() const override;
  void SetReferrerForRequest(WebURLRequest&, const WebURL& referrer) override;
  WebAssociatedURLLoader* CreateAssociatedURLLoader(
      const WebAssociatedURLLoaderOptions&) override;
  void BindDevToolsAgent(
      mojo::ScopedInterfaceEndpointHandle devtools_agent_host_ptr_info,
      mojo::ScopedInterfaceEndpointHandle devtools_agent_request) override;
  void SetMarkedText(const WebString&,
                     unsigned location,
                     unsigned length) override;
  void UnmarkText() override;
  bool HasMarkedText() const override;
  WebRange MarkedRange() const override;
  bool FirstRectForCharacterRange(unsigned location,
                                  unsigned length,
                                  WebRect&) const override;
  size_t CharacterIndexForPoint(const WebPoint&) const override;
  bool ExecuteCommand(const WebString&) override;
  bool ExecuteCommand(const WebString&, const WebString& value) override;
  bool IsCommandEnabled(const WebString&) const override;
  bool SelectionTextDirection(WebTextDirection& start,
                              WebTextDirection& end) const override;
  bool IsSelectionAnchorFirst() const override;
  void SetTextDirection(WebTextDirection) override;
  void SetTextCheckClient(WebTextCheckClient*) override;
  void SetSpellCheckPanelHostClient(WebSpellCheckPanelHostClient*) override;
  void ReplaceMisspelledRange(const WebString&) override;
  void RemoveSpellingMarkers() override;
  void RemoveSpellingMarkersUnderWords(
      const WebVector<WebString>& words) override;
  void SetContentSettingsClient(WebContentSettingsClient*) override;
  bool HasSelection() const override;
  WebRange SelectionRange() const override;
  WebString SelectionAsText() const override;
  WebString SelectionAsMarkup() const override;
  bool SelectWordAroundCaret() override;
  void SelectRange(const WebPoint& base, const WebPoint& extent) override;
  void SelectRange(const WebRange&,
                   HandleVisibilityBehavior,
                   blink::mojom::SelectionMenuBehavior) override;
  WebString RangeAsText(const WebRange&) override;
  void MoveRangeSelectionExtent(const WebPoint&) override;
  void MoveRangeSelection(
      const WebPoint& base,
      const WebPoint& extent,
      WebFrame::TextGranularity = kCharacterGranularity) override;
  void MoveCaretSelection(const WebPoint&) override;
  bool SetEditableSelectionOffsets(int start, int end) override;
  bool SetCompositionFromExistingText(
      int composition_start,
      int composition_end,
      const WebVector<WebImeTextSpan>& ime_text_spans) override;
  void ExtendSelectionAndDelete(int before, int after) override;
  void DeleteSurroundingText(int before, int after) override;
  void DeleteSurroundingTextInCodePoints(int before, int after) override;
  void SetCaretVisible(bool) override;
  void DispatchBeforePrintEvent() override;
  int PrintBegin(const WebPrintParams&,
                 const WebNode& constrain_to_node) override;
  float PrintPage(int page_to_print, cc::PaintCanvas*) override;
  float GetPrintPageShrink(int page) override;
  void PrintEnd() override;
  void DispatchAfterPrintEvent() override;
  bool IsPrintScalingDisabledForPlugin(const WebNode&) override;
  bool GetPrintPresetOptionsForPlugin(const WebNode&,
                                      WebPrintPresetOptions*) override;
  bool HasCustomPageSizeStyle(int page_index) override;
  bool IsPageBoxVisible(int page_index) override;
  void PageSizeAndMarginsInPixels(int page_index,
                                  WebDoubleSize& page_size,
                                  int& margin_top,
                                  int& margin_right,
                                  int& margin_bottom,
                                  int& margin_left) override;
  WebString PageProperty(const WebString& property_name,
                         int page_index) override;
  void PrintPagesForTesting(cc::PaintCanvas*, const WebSize&) override;

  void DispatchMessageEventWithOriginCheck(
      const WebSecurityOrigin& intended_target_origin,
      const WebDOMEvent&,
      bool has_user_gesture) override;

  WebRect GetSelectionBoundsRectForTesting() const override;

  WebString GetLayerTreeAsTextForTesting(
      bool show_debug_info = false) const override;

  WebLocalFrameClient* Client() const override { return client_; }

  // WebLocalFrame methods:
  WebLocalFrameImpl* CreateLocalChild(WebTreeScopeType,
                                      WebLocalFrameClient*,
                                      blink::InterfaceRegistry*) override;
  void SetAutofillClient(WebAutofillClient*) override;
  WebAutofillClient* AutofillClient() override;
  bool IsLocalRoot() const override;
  bool IsProvisional() const override;
  WebLocalFrameImpl* LocalRoot() override;
  WebFrame* FindFrameByName(const WebString& name) override;
  void SendPings(const WebURL& destination_url) override;
  bool DispatchBeforeUnloadEvent(bool) override;
  void CommitNavigation(
      const WebURLRequest&,
      WebFrameLoadType,
      const WebHistoryItem&,
      bool is_client_redirect,
      const base::UnguessableToken& devtools_navigation_token,
      std::unique_ptr<WebNavigationParams> navigation_params,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) override;
  blink::mojom::CommitResult CommitSameDocumentNavigation(
      const WebURL&,
      WebFrameLoadType,
      const WebHistoryItem&,
      bool is_client_redirect,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) override;
  void LoadJavaScriptURL(const WebURL&) override;
  void CommitDataNavigation(
      const WebURLRequest&,
      const WebData&,
      const WebString& mime_type,
      const WebString& text_encoding,
      const WebURL& unreachable_url,
      WebFrameLoadType,
      const WebHistoryItem&,
      bool is_client_redirect,
      std::unique_ptr<WebNavigationParams> navigation_params,
      std::unique_ptr<WebDocumentLoader::ExtraData> navigation_data) override;
  FallbackContentResult MaybeRenderFallbackContent(
      const WebURLError&) const override;
  void RenderFallbackContent() const override;
  void ReportContentSecurityPolicyViolation(
      const blink::WebContentSecurityPolicyViolation&) override;
  bool IsLoading() const override;
  bool IsNavigationScheduledWithin(double interval) const override;
  void SetCommittedFirstRealLoad() override;
  void NotifyUserActivation() override;
  void BlinkFeatureUsageReport(const std::set<int>& features) override;
  void MixedContentFound(const WebURL& main_resource_url,
                         const WebURL& mixed_content_url,
                         mojom::RequestContextType,
                         bool was_allowed,
                         bool had_redirect,
                         const WebSourceLocation&) override;
  void ClientDroppedNavigation() override;
  void MarkAsLoading() override;
  void SendOrientationChangeEvent() override;
  WebSandboxFlags EffectiveSandboxFlags() const override;
  void DidCallAddSearchProvider() override;
  void DidCallIsSearchProviderInstalled() override;
  void ReplaceSelection(const WebString&) override;
  bool FindForTesting(int identifier,
                      const WebString& search_text,
                      bool match_case,
                      bool forward,
                      bool force,
                      bool find_next,
                      bool wrap_within_frame) override;
  void SetTickmarks(const WebVector<WebRect>&) override;
  WebNode ContextMenuNode() const override;
  WebFrameWidget* FrameWidget() const override;
  void CopyImageAt(const WebPoint&) override;
  void SaveImageAt(const WebPoint&) override;
  void UsageCountChromeLoadTimes(const WebString& metric) override;
  FrameScheduler* Scheduler() const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override;
  WebInputMethodController* GetInputMethodController() override;
  void ExtractSmartClipData(WebRect rect_in_viewport,
                            WebString& clip_text,
                            WebString& clip_html,
                            WebRect& clip_rect) override;
  void AdvanceFocusInForm(WebFocusType) override;
  void PerformMediaPlayerAction(const WebPoint&,
                                const WebMediaPlayerAction&) override;

  void InitializeCoreFrame(Page&, FrameOwner*, const AtomicString& name);
  LocalFrame* GetFrame() const { return frame_.Get(); }

  void WillBeDetached();
  void WillDetachParent();

  static WebLocalFrameImpl* Create(WebTreeScopeType,
                                   WebLocalFrameClient*,
                                   InterfaceRegistry*,
                                   WebFrame* opener);
  static WebLocalFrameImpl* CreateMainFrame(WebView*,
                                            WebLocalFrameClient*,
                                            InterfaceRegistry*,
                                            WebFrame* opener,
                                            const WebString& name,
                                            WebSandboxFlags);
  static WebLocalFrameImpl* CreateProvisional(WebLocalFrameClient*,
                                              InterfaceRegistry*,
                                              WebRemoteFrame*,
                                              WebSandboxFlags,
                                              ParsedFeaturePolicy);

  ~WebLocalFrameImpl() override;

  LocalFrame* CreateChildFrame(const AtomicString& name,
                               HTMLFrameOwnerElement*);

  void DidChangeContentsSize(const IntSize&);

  void UpdateDevToolsOverlays();

  void CreateFrameView();

  static WebLocalFrameImpl* FromFrame(LocalFrame*);
  static WebLocalFrameImpl* FromFrame(LocalFrame&);
  static WebLocalFrameImpl* FromFrameOwnerElement(Element*);

  WebViewImpl* ViewImpl() const;

  LocalFrameView* GetFrameView() const {
    return GetFrame() ? GetFrame()->View() : nullptr;
  }

  void SetDevToolsAgentImpl(WebDevToolsAgentImpl*);
  WebDevToolsAgentImpl* DevToolsAgentImpl();

  // When a Find operation ends, we want to set the selection to what was active
  // and set focus to the first focusable node we find (starting with the first
  // node in the matched range and going up the inheritance chain). If we find
  // nothing to focus we focus the first focusable node in the range. This
  // allows us to set focus to a link (when we find text inside a link), which
  // allows us to navigate by pressing Enter after closing the Find box.
  void SetFindEndstateFocusAndSelection();

  void DidFail(const ResourceError&,
               bool was_provisional,
               WebHistoryCommitType);
  void DidFinish();

  void SetClient(WebLocalFrameClient* client) { client_ = client; }

  WebFrameWidgetBase* FrameWidgetImpl() { return frame_widget_; }

  WebContentSettingsClient* GetContentSettingsClient() {
    return content_settings_client_;
  }

  SharedWorkerRepositoryClientImpl* SharedWorkerRepositoryClient() const {
    return shared_worker_repository_client_.get();
  }

  void SetInputEventsScaleForEmulation(float);

  WebTextCheckClient* GetTextCheckerClient() const {
    return text_check_client_;
  }

  WebSpellCheckPanelHostClient* SpellCheckPanelHostClient() const override {
    return spell_check_panel_host_client_;
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

  virtual void Trace(blink::Visitor*);

 private:
  friend LocalFrameClientImpl;

  WebLocalFrameImpl(WebTreeScopeType,
                    WebLocalFrameClient*,
                    blink::InterfaceRegistry*);
  WebLocalFrameImpl(WebRemoteFrame*,
                    WebLocalFrameClient*,
                    blink::InterfaceRegistry*);

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
  WebContentSettingsClient* content_settings_client_ = nullptr;
  std::unique_ptr<SharedWorkerRepositoryClientImpl>
      shared_worker_repository_client_;

  Member<FindInPage> find_in_page_;

  // Valid between calls to BeginPrint() and EndPrint(). Containts the print
  // information. Is used by PrintPage().
  Member<ChromePrintContext> print_context_;

  // Stores the additional input events scale when device metrics
  // emulation is enabled.
  float input_events_scale_factor_for_emulation_;

  // Borrowed pointers to Mojo objects.
  blink::InterfaceRegistry* interface_registry_;

  WebInputMethodControllerImpl input_method_controller_;

  WebTextCheckClient* text_check_client_;

  WebSpellCheckPanelHostClient* spell_check_panel_host_client_;

  // Oilpan: WebLocalFrameImpl must remain alive until close() is called.
  // Accomplish that by keeping a self-referential Persistent<>. It is
  // cleared upon close().
  SelfKeepAlive<WebLocalFrameImpl> self_keep_alive_;

#if DCHECK_IS_ON()
  // True if DispatchBeforePrintEvent() was called, and
  // DispatchAfterPrintEvent() is not called yet.
  bool is_in_printing_ = false;
#endif
};

DEFINE_TYPE_CASTS(WebLocalFrameImpl,
                  WebFrame,
                  frame,
                  frame->IsWebLocalFrame(),
                  frame.IsWebLocalFrame());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_LOCAL_FRAME_IMPL_H_
