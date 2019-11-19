/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_CHROME_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_CHROME_CLIENT_IMPL_H_

#include <memory>

#include "cc/input/overscroll_behavior.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"

namespace blink {

class PagePopup;
class PagePopupClient;
class WebAutofillClient;
class WebViewImpl;
struct WebCursorInfo;

// Handles window-level notifications from core on behalf of a WebView.
class CORE_EXPORT ChromeClientImpl final : public ChromeClient {
 public:
  static ChromeClientImpl* Create(WebViewImpl*);
  explicit ChromeClientImpl(WebViewImpl*);
  ~ChromeClientImpl() override;
  void Trace(Visitor* visitor) override;

  WebViewImpl* GetWebView() const override;

  // ChromeClient methods:
  void ChromeDestroyed() override;
  void SetWindowRect(const IntRect&, LocalFrame&) override;
  IntRect RootWindowRect(LocalFrame&) override;
  void Focus(LocalFrame*) override;
  bool CanTakeFocus(WebFocusType) override;
  void TakeFocus(WebFocusType) override;
  void FocusedElementChanged(Element* from_node, Element* to_node) override;
  void BeginLifecycleUpdates(LocalFrame& main_frame) override;
  void StartDeferringCommits(LocalFrame& main_frame,
                             base::TimeDelta timeout) override;
  void StopDeferringCommits(LocalFrame& main_frame,
                            cc::PaintHoldingCommitTrigger) override;
  bool HadFormInteraction() const override;
  void StartDragging(LocalFrame*,
                     const WebDragData&,
                     WebDragOperationsMask,
                     const SkBitmap& drag_image,
                     const gfx::Point& drag_image_offset) override;
  bool AcceptsLoadDrops() const override;
  Page* CreateWindowDelegate(LocalFrame*,
                             const FrameLoadRequest&,
                             const AtomicString& name,
                             const WebWindowFeatures&,
                             WebSandboxFlags,
                             const FeaturePolicy::FeatureState&,
                             const SessionStorageNamespaceId&) override;
  void Show(NavigationPolicy) override;
  void DidOverscroll(const FloatSize& overscroll_delta,
                     const FloatSize& accumulated_overscroll,
                     const FloatPoint& position_in_viewport,
                     const FloatSize& velocity_in_viewport) override;
  void SetOverscrollBehavior(LocalFrame& main_frame,
                             const cc::OverscrollBehavior&) override;
  void InjectGestureScrollEvent(LocalFrame& local_frame,
                                WebGestureDevice device,
                                const WebFloatSize& delta,
                                ScrollGranularity granularity,
                                CompositorElementId scrollable_area_element_id,
                                WebInputEvent::Type injected_type) override;
  bool ShouldReportDetailedMessageForSource(LocalFrame&,
                                            const String&) override;
  void AddMessageToConsole(LocalFrame*,
                           mojom::ConsoleMessageSource,
                           mojom::ConsoleMessageLevel,
                           const String& message,
                           unsigned line_number,
                           const String& source_id,
                           const String& stack_trace) override;
  bool CanOpenBeforeUnloadConfirmPanel() override;
  bool OpenBeforeUnloadConfirmPanelDelegate(LocalFrame*,
                                            bool is_reload) override;
  void CloseWindowSoon() override;
  bool OpenJavaScriptAlertDelegate(LocalFrame*, const String&) override;
  bool OpenJavaScriptConfirmDelegate(LocalFrame*, const String&) override;
  bool OpenJavaScriptPromptDelegate(LocalFrame*,
                                    const String& message,
                                    const String& default_value,
                                    String& result) override;
  bool TabsToLinks() override;
  void InvalidateRect(const IntRect&) override;
  void ScheduleAnimation(const LocalFrameView*,
                         base::TimeDelta = base::TimeDelta()) override;
  IntRect ViewportToScreen(const IntRect&,
                           const LocalFrameView*) const override;
  float WindowToViewportScalar(LocalFrame*, const float) const override;
  WebScreenInfo GetScreenInfo(LocalFrame&) const override;
  void OverrideVisibleRectForMainFrame(LocalFrame& frame,
                                       IntRect* paint_rect) const override;
  float InputEventsScaleForEmulation() const override;
  void ContentsSizeChanged(LocalFrame*, const IntSize&) const override;
  bool DoubleTapToZoomEnabled() const override;
  void PageScaleFactorChanged() const override;
  float ClampPageScaleFactorToLimits(float scale) const override;
  void MainFrameScrollOffsetChanged(LocalFrame& main_frame) const override;
  void ResizeAfterLayout() const override;
  void MainFrameLayoutUpdated() const override;
  void ShowMouseOverURL(const HitTestResult&) override;
  void SetToolTip(LocalFrame&, const String&, TextDirection) override;
  void DispatchViewportPropertiesDidChange(
      const ViewportDescription&) const override;
  void PrintDelegate(LocalFrame*) override;
  ColorChooser* OpenColorChooser(LocalFrame*,
                                 ColorChooserClient*,
                                 const Color&) override;
  DateTimeChooser* OpenDateTimeChooser(
      LocalFrame* frame,
      DateTimeChooserClient*,
      const DateTimeChooserParameters&) override;
  ExternalDateTimeChooser* GetExternalDateTimeChooserForTesting() override;
  void OpenFileChooser(LocalFrame*, scoped_refptr<FileChooser>) override;
  void SetCursor(const Cursor&, LocalFrame*) override;
  void SetCursorOverridden(bool) override;
  Cursor LastSetCursorForTesting() const override;
  void SetEventListenerProperties(LocalFrame*,
                                  cc::EventListenerClass,
                                  cc::EventListenerProperties) override;
  cc::EventListenerProperties EventListenerProperties(
      LocalFrame*,
      cc::EventListenerClass) const override;
  // Informs client about the existence of handlers for scroll events so
  // appropriate scroll optimizations can be chosen.
  void SetHasScrollEventHandlers(LocalFrame*, bool has_event_handlers) override;
  void SetNeedsLowLatencyInput(LocalFrame*, bool needs_low_latency) override;
  void SetNeedsUnbufferedInputForDebugger(LocalFrame*, bool immediate) override;
  void RequestUnbufferedInputEvents(LocalFrame*) override;
  void SetTouchAction(LocalFrame*, TouchAction) override;

  void AttachRootLayer(scoped_refptr<cc::Layer>,
                       LocalFrame* local_root) override;

  void AttachCompositorAnimationTimeline(CompositorAnimationTimeline*,
                                         LocalFrame*) override;
  void DetachCompositorAnimationTimeline(CompositorAnimationTimeline*,
                                         LocalFrame*) override;

  void EnterFullscreen(LocalFrame&, const FullscreenOptions*) override;
  void ExitFullscreen(LocalFrame&) override;
  void FullscreenElementChanged(Element* old_element,
                                Element* new_element) override;

  void ClearLayerSelection(LocalFrame*) override;
  void UpdateLayerSelection(LocalFrame*, const cc::LayerSelection&) override;

  // ChromeClient methods:
  String AcceptLanguages() override;
  void SetCursorForPlugin(const WebCursorInfo&, LocalFrame*) override;

  // ChromeClientImpl:
  void SetNewWindowNavigationPolicy(WebNavigationPolicy);

  // FileChooser calls this function to kick pending file chooser
  // requests.
  void DidCompleteFileChooser(FileChooser& file_chooser);

  void AutoscrollStart(WebFloatPoint viewport_point, LocalFrame*) override;
  void AutoscrollFling(WebFloatSize velocity, LocalFrame*) override;
  void AutoscrollEnd(LocalFrame*) override;

  bool HasOpenedPopup() const override;
  PopupMenu* OpenPopupMenu(LocalFrame&, HTMLSelectElement&) override;
  PagePopup* OpenPagePopup(PagePopupClient*) override;
  void ClosePagePopup(PagePopup*) override;
  DOMWindow* PagePopupWindowForTesting() const override;

  void SetBrowserControlsState(float top_height,
                               float bottom_height,
                               bool shrinks_layout) override;
  void SetBrowserControlsShownRatio(float top_ratio,
                                    float bottom_ratio) override;

  bool ShouldOpenUIElementDuringPageDismissal(
      LocalFrame&,
      UIElementType,
      const String& dialog_message,
      Document::PageDismissalType) const override;

  bool RequestPointerLock(LocalFrame*, bool) override;
  void RequestPointerUnlock(LocalFrame*) override;

  // AutofillClient pass throughs:
  void DidAssociateFormControlsAfterLoad(LocalFrame*) override;
  void HandleKeyboardEventOnTextField(HTMLInputElement&,
                                      KeyboardEvent&) override;
  void DidChangeValueInTextField(HTMLFormControlElement&) override;
  void DidEndEditingOnTextField(HTMLInputElement&) override;
  void OpenTextDataListChooser(HTMLInputElement&) override;
  void TextFieldDataListChanged(HTMLInputElement&) override;
  void DidChangeSelectionInSelectControl(HTMLFormControlElement&) override;
  void SelectFieldOptionsChanged(HTMLFormControlElement&) override;
  void AjaxSucceeded(LocalFrame*) override;

  void ShowVirtualKeyboardOnElementFocus(LocalFrame&) override;

  TransformationMatrix GetDeviceEmulationTransform() const override;

  void OnMouseDown(Node&) override;
  void DidUpdateBrowserControls() const override;

  FloatSize ElasticOverscroll() const override;

  void RegisterPopupOpeningObserver(PopupOpeningObserver*) override;
  void UnregisterPopupOpeningObserver(PopupOpeningObserver*) override;
  void NotifyPopupOpeningObservers() const override;

  viz::FrameSinkId GetFrameSinkId(LocalFrame*) override;

  void RequestDecode(LocalFrame*,
                     const PaintImage&,
                     base::OnceCallback<void(bool)>) override;

  void NotifySwapTime(LocalFrame& frame, ReportTimeCallback callback) override;

  void FallbackCursorModeLockCursor(LocalFrame* frame,
                                    bool left,
                                    bool right,
                                    bool up,
                                    bool down) override;

  void FallbackCursorModeSetCursorVisibility(LocalFrame* frame,
                                             bool visible) override;

  void RequestBeginMainFrameNotExpected(LocalFrame& frame,
                                        bool request) override;

  int GetLayerTreeId(LocalFrame& frame) override;

  void DidUpdateTextAutosizerPageInfo(const WebTextAutosizerPageInfo&) override;

  void DocumentDetached(Document&) override;

 private:
  bool IsChromeClientImpl() const override { return true; }

  void SetCursor(const WebCursorInfo&, LocalFrame*);

  // Returns WebAutofillClient associated with the WebLocalFrame. This takes and
  // returns nullable.
  WebAutofillClient* AutofillClientFromFrame(LocalFrame*);

  WebViewImpl* web_view_;  // Weak pointer.
  HeapHashSet<WeakMember<PopupOpeningObserver>> popup_opening_observers_;
  Vector<scoped_refptr<FileChooser>> file_chooser_queue_;
  Cursor last_set_mouse_cursor_for_testing_;
  bool cursor_overridden_;
  Member<ExternalDateTimeChooser> external_date_time_chooser_;
  bool did_request_non_empty_tool_tip_;

  FRIEND_TEST_ALL_PREFIXES(FileChooserQueueTest, DerefQueuedChooser);
};

DEFINE_TYPE_CASTS(ChromeClientImpl,
                  ChromeClient,
                  client,
                  client->IsChromeClientImpl(),
                  client.IsChromeClientImpl());

}  // namespace blink

#endif
