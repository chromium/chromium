/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_VIEW_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_VIEW_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/platform/web_float_size.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_page_importance_signals.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/exported/web_page_popup_impl.h"
#include "third_party/blink/renderer/core/frame/resize_viewport_anchor.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/context_menu_provider.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"
#include "third_party/blink/renderer/core/page/page_widget_delegate.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/apply_viewport_changes.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cc {
class Layer;
struct BeginMainFrameMetrics;
class ScopedDeferMainFrameUpdate;
}

namespace blink {

namespace frame_test_helpers {
class WebViewHelper;
}

class BrowserControls;
class DevToolsEmulator;
class Frame;
class FullscreenController;
class HTMLPlugInElement;
class PageScaleConstraintsSet;
class WebDevToolsAgentImpl;
class WebElement;
class WebInputMethodController;
class WebLocalFrame;
class WebLocalFrameImpl;
class WebRemoteFrame;
class WebSettingsImpl;
class WebViewClient;

struct WebTextAutosizerPageInfo;

using PaintHoldingCommitTrigger = cc::PaintHoldingCommitTrigger;

class CORE_EXPORT WebViewImpl final : public WebView,
                                      public RefCounted<WebViewImpl>,
                                      public PageWidgetEventHandler {
 public:
  static WebViewImpl* Create(WebViewClient*,
                             bool is_hidden,
                             bool compositing_enabled,
                             WebViewImpl* opener);

  // All calls to Create() should be balanced with a call to Close(). This
  // synchronously destroys the WebViewImpl.
  void Close() override;

  static HashSet<WebViewImpl*>& AllInstances();
  // Returns true if popup menus should be rendered by the browser, false if
  // they should be rendered by WebKit (which is the default).
  static bool UseExternalPopupMenus();

  // Returns whether frames under this WebView are backed by a compositor.
  bool does_composite() const { return does_composite_; }

  // WebView methods:
  void DidAttachLocalMainFrame() override;
  void SetPrerendererClient(WebPrerendererClient*) override;
  WebSettings* GetSettings() override;
  WebString PageEncoding() const override;
  bool TabsToLinks() const override;
  void SetTabsToLinks(bool value) override;
  bool TabKeyCyclesThroughElements() const override;
  void SetTabKeyCyclesThroughElements(bool value) override;
  bool IsActive() const override;
  void SetIsActive(bool value) override;
  void SetDomainRelaxationForbidden(bool, const WebString& scheme) override;
  void SetWindowFeatures(const WebWindowFeatures&) override;
  void SetOpenedByDOM() override;
  void ResizeWithBrowserControls(const WebSize&,
                                 float top_controls_height,
                                 float bottom_controls_height,
                                 bool browser_controls_shrink_layout) override;
  WebFrame* MainFrame() override;
  WebLocalFrame* FocusedFrame() override;
  void SetFocusedFrame(WebFrame*) override;
  void FocusDocumentView(WebFrame*) override;
  void SetInitialFocus(bool reverse) override;
  void ClearFocusedElement() override;
  void SmoothScroll(int target_x,
                    int target_y,
                    base::TimeDelta duration) override;
  void AdvanceFocus(bool reverse) override;
  void AdvanceFocusAcrossFrames(WebFocusType,
                                WebRemoteFrame* from,
                                WebLocalFrame* to) override;
  double ZoomLevel() override;
  double SetZoomLevel(double) override;
  float TextZoomFactor() override;
  float SetTextZoomFactor(float) override;
  float PageScaleFactor() const override;
  float MinimumPageScaleFactor() const override;
  float MaximumPageScaleFactor() const override;
  void SetDefaultPageScaleLimits(float min_scale, float max_scale) override;
  void SetInitialPageScaleOverride(float) override;
  void SetMaximumLegibleScale(float) override;
  void SetPageScaleFactor(float) override;
  void SetVisualViewportOffset(const WebFloatPoint&) override;
  WebFloatPoint VisualViewportOffset() const override;
  WebFloatSize VisualViewportSize() const override;
  void ResizeVisualViewport(const WebSize&) override;
  void Resize(const WebSize&) override;
  WebSize GetSize() override;
  void ResetScrollAndScaleState() override;
  void SetIgnoreViewportTagScaleLimits(bool) override;
  WebSize ContentsPreferredMinimumSize() override;
  void SetDisplayMode(blink::mojom::DisplayMode) override;
  void AnimateDoubleTapZoom(const gfx::Point&,
                            const WebRect& block_bounds) override;
  void ZoomToFindInPageRect(const WebRect&) override;
  void SetDeviceScaleFactor(float) override;
  void SetZoomFactorForDeviceScaleFactor(float) override;
  float ZoomFactorForDeviceScaleFactor() override {
    return zoom_factor_for_device_scale_factor_;
  }
  void EnableAutoResizeMode(const WebSize& min_size,
                            const WebSize& max_size) override;
  void DisableAutoResizeMode() override;
  void PerformPluginAction(const PluginAction&, const gfx::Point&) override;
  void AudioStateChanged(bool is_audio_playing) override;
  WebHitTestResult HitTestResultAt(const gfx::Point&);
  WebHitTestResult HitTestResultForTap(const gfx::Point&,
                                       const WebSize&) override;
  uint64_t CreateUniqueIdentifierForRequest() override;
  void EnableDeviceEmulation(const WebDeviceEmulationParams&) override;
  void DisableDeviceEmulation() override;
  void PerformCustomContextMenuAction(unsigned action) override;
  void DidCloseContextMenu() override;
  void CancelPagePopup() override;
  WebPagePopupImpl* GetPagePopup() const override { return page_popup_.get(); }
  void SetMainFrameOverlayColor(SkColor) override;
  WebPageImportanceSignals* PageImportanceSignals() override;
  void AcceptLanguagesChanged() override;
  void SetPageFrozen(bool frozen) override;
  void PutPageIntoBackForwardCache() override;
  void RestorePageFromBackForwardCache(
      base::TimeTicks navigation_start) override;
  WebWidget* MainFrameWidget() override;
  void SetBaseBackgroundColor(SkColor) override;
  void SetBackgroundColorOverride(SkColor) override;
  void ClearBackgroundColorOverride() override;
  void SetBaseBackgroundColorOverride(SkColor) override;
  void ClearBaseBackgroundColorOverride() override;
  void SetInsidePortal(bool inside_portal) override;
  void PaintContent(cc::PaintCanvas*, const gfx::Rect&) override;
  void SetTextAutosizePageInfo(const WebTextAutosizerPageInfo&) override;

  float DefaultMinimumPageScaleFactor() const;
  float DefaultMaximumPageScaleFactor() const;
  float ClampPageScaleFactorToLimits(float) const;
  void ResetScaleStateImmediately();

  HitTestResult CoreHitTestResultAt(const gfx::Point&);
  void InvalidateRect(const IntRect&);

  void SetZoomFactorOverride(float);
  void SetCompositorDeviceScaleFactorOverride(float);
  TransformationMatrix GetDeviceEmulationTransform() const;

  SkColor BackgroundColor() const;
  Color BaseBackgroundColor() const;
  bool BackgroundColorOverrideEnabled() const {
    return background_color_override_enabled_;
  }
  SkColor BackgroundColorOverride() const { return background_color_override_; }

  Frame* FocusedCoreFrame() const;

  // Returns the currently focused Element or null if no element has focus.
  Element* FocusedElement() const;

  WebViewClient* Client() { return AsView().client; }

  // Returns the page object associated with this view. This may be null when
  // the page is shutting down, but will be valid at all other times.
  Page* GetPage() const { return AsView().page.Get(); }

  WebDevToolsAgentImpl* MainFrameDevToolsAgentImpl();

  DevToolsEmulator* GetDevToolsEmulator() const {
    return dev_tools_emulator_.Get();
  }

  // Returns the main frame associated with this view. This will be null when
  // the main frame is remote.
  // Internally during startup/shutdown this can be null when no main frame
  // (local or remote) is attached, but this should not generally matter to code
  // outside this class.
  WebLocalFrameImpl* MainFrameImpl() const;

  // Event related methods:
  void MouseContextMenu(const WebMouseEvent&);
  void MouseDoubleClick(const WebMouseEvent&);

  bool StartPageScaleAnimation(const IntPoint& target_position,
                               bool use_anchor,
                               float new_scale,
                               base::TimeDelta duration);

  // Handles context menu events orignated via the the keyboard. These
  // include the VK_APPS virtual key and the Shift+F10 combine. Code is
  // based on the Webkit function bool WebView::handleContextMenuEvent(WPARAM
  // wParam, LPARAM lParam) in webkit\webkit\win\WebView.cpp. The only
  // significant change in this function is the code to convert from a
  // Keyboard event to the Right Mouse button down event.
  WebInputEventResult SendContextMenuEvent();

  void ShowContextMenuForElement(WebElement);

  // Notifies the WebView that a load has been committed. isNewNavigation
  // will be true if a new session history item should be created for that
  // load. isNavigationWithinPage will be true if the navigation does
  // not take the user away from the current page.
  void DidCommitLoad(bool is_new_navigation, bool is_navigation_within_page);

  // Indicates two things:
  //   1) This view may have a new layout now.
  //   2) Layout is up-to-date.
  // After calling WebWidget::updateAllLifecyclePhases(), expect to get this
  // notification unless the view did not need a layout.
  void MainFrameLayoutUpdated();
  void ResizeAfterLayout();

  void DidChangeContentsSize();
  void PageScaleFactorChanged();
  void MainFrameScrollOffsetChanged();

  bool ShouldAutoResize() const { return should_auto_resize_; }

  IntSize MinAutoSize() const { return min_auto_size_; }

  IntSize MaxAutoSize() const { return max_auto_size_; }

  void UpdateMainFrameLayoutSize();
  void UpdatePageDefinedViewportConstraints(const ViewportDescription&);

  WebPagePopupImpl* OpenPagePopup(PagePopupClient*);
  bool HasOpenedPopup() const { return page_popup_.get(); }
  void ClosePagePopup(PagePopup*);
  // Callback from PagePopup when it is closed, which it can be done directly
  // without coming through WebViewImpl.
  void CleanupPagePopup();
  LocalDOMWindow* PagePopupWindow() const;

  PageScheduler* Scheduler() const override;
  void SetVisibilityState(PageVisibilityState visibility_state,
                          bool is_initial_state) override;
  PageVisibilityState GetVisibilityState() override;

  // Called by a full frame plugin inside this view to inform it that its
  // zoom level has been updated.  The plugin should only call this function
  // if the zoom change was triggered by the browser, it's only needed in case
  // a plugin can update its own zoom, say because of its own UI.
  void FullFramePluginZoomLevelChanged(double zoom_level);

  void ComputeScaleAndScrollForBlockRect(
      const gfx::Point& hit_point,
      const WebRect& block_rect,
      float padding,
      float default_scale_when_already_legible,
      float& scale,
      IntPoint& scroll);
  Node* BestTapNode(const GestureEventWithHitTestResults& targeted_tap_event);
  void EnableTapHighlightAtPoint(
      const GestureEventWithHitTestResults& targeted_tap_event);

  void EnableFakePageScaleAnimationForTesting(bool);
  bool FakeDoubleTapAnimationPendingForTesting() const {
    return double_tap_zoom_pending_;
  }
  IntPoint FakePageScaleAnimationTargetPositionForTesting() const {
    return fake_page_scale_animation_target_position_;
  }
  float FakePageScaleAnimationPageScaleForTesting() const {
    return fake_page_scale_animation_page_scale_factor_;
  }
  bool FakePageScaleAnimationUseAnchorForTesting() const {
    return fake_page_scale_animation_use_anchor_;
  }

  void EnterFullscreen(LocalFrame&, const FullscreenOptions*);
  void ExitFullscreen(LocalFrame&);
  void FullscreenElementChanged(Element* old_element, Element* new_element);

  // Sends a request to the main frame's view to resize, and updates the page
  // scale limits if needed.
  void SendResizeEventForMainFrame();

  // Exposed for testing purposes.
  bool HasHorizontalScrollbar();
  bool HasVerticalScrollbar();

  WebSettingsImpl* SettingsImpl();

  cc::AnimationHost* AnimationHost() const { return animation_host_; }

  BrowserControls& GetBrowserControls();
  // Called anytime browser controls layout height or content offset have
  // changed.
  void DidUpdateBrowserControls();

  void ForceNextWebGLContextCreationToFail() override;
  void ForceNextDrawingBufferCreationToFail() override;

  void AddAutoplayFlags(int32_t) override;
  void ClearAutoplayFlags() override;
  int32_t AutoplayFlagsForTest() override;

  WebSize Size();
  IntSize MainFrameSize();
  blink::mojom::DisplayMode DisplayMode() const { return display_mode_; }

  PageScaleConstraintsSet& GetPageScaleConstraintsSet() const;

  FloatSize ElasticOverscroll() const { return elastic_overscroll_; }

  class ChromeClient& GetChromeClient() const {
    return *chrome_client_.Get();
  }

  // Returns the currently active WebInputMethodController which is the one
  // corresponding to the focused frame. It will return nullptr if there is no
  // focused frame, or if the there is one but it belongs to a different local
  // root.
  WebInputMethodController* GetActiveWebInputMethodController() const;

  bool ShouldZoomToLegibleScale(const Element&);
  void ZoomAndScrollToFocusedEditableElementRect(
      const IntRect& element_bounds_in_document,
      const IntRect& caret_bounds_in_document,
      bool zoom_into_legible_scale);

  // Allows main frame updates to occur if they were previously blocked. They
  // are blocked during loading a navigation, to allow Blink to proceed without
  // being interrupted by useless work until enough progress is made that it
  // desires composited output to be generated.
  void StopDeferringMainFrameUpdate();

  // This function checks the element ids of ScrollableAreas only and returns
  // the equivalent DOM Node if such exists.
  Node* FindNodeFromScrollableCompositorElementId(
      cc::ElementId element_id) const;

  void DidEnterFullscreen();
  void DidExitFullscreen();

  void SetWebWidget(WebWidget* widget);

 private:
  FRIEND_TEST_ALL_PREFIXES(WebFrameTest, DivScrollIntoEditableTest);
  FRIEND_TEST_ALL_PREFIXES(WebFrameTest,
                           DivScrollIntoEditablePreservePageScaleTest);
  FRIEND_TEST_ALL_PREFIXES(WebFrameTest,
                           DivScrollIntoEditableTestZoomToLegibleScaleDisabled);
  FRIEND_TEST_ALL_PREFIXES(WebFrameTest,
                           DivScrollIntoEditableTestWithDeviceScaleFactor);
  FRIEND_TEST_ALL_PREFIXES(WebViewTest, SetBaseBackgroundColorBeforeMainFrame);
  FRIEND_TEST_ALL_PREFIXES(WebViewTest, LongPressImage);
  FRIEND_TEST_ALL_PREFIXES(WebViewTest, LongPressImageAndThenLongTapImage);
  friend class frame_test_helpers::WebViewHelper;
  friend class SimCompositor;
  friend class WebView;  // So WebView::Create can call our constructor
  friend class WebViewFrameWidget;
  friend class WTF::RefCounted<WebViewImpl>;

  // TODO(danakj): DCHECK in these that we're not inside a wrong API stackframe.
  struct ViewData;
  ViewData& AsView() { return as_view_; }
  const ViewData& AsView() const { return as_view_; }

  // Called while the main LocalFrame is being detached. The MainFrameImpl() is
  // still valid until after this method is called.
  void DidDetachLocalMainFrame();

  // These are temporary methods to allow WebViewFrameWidget to delegate to
  // WebViewImpl. We expect to eventually move these out.
  void SetAnimationHost(cc::AnimationHost*);
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool);
  void BeginFrame(base::TimeTicks last_frame_time,
                  bool record_main_frame_metrics);
  void DidBeginFrame();
  void BeginRafAlignedInput();
  void EndRafAlignedInput();
  void BeginUpdateLayers();
  void EndUpdateLayers();
  void BeginCommitCompositorFrame();
  void EndCommitCompositorFrame();
  void RecordStartOfFrameMetrics();
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time);
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics();
  void UpdateLifecycle(WebWidget::LifecycleUpdate requested_update,
                       WebWidget::LifecycleUpdateReason reason);
  void ThemeChanged();
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&);
  WebInputEventResult DispatchBufferedTouchEvents();
  void SetCursorVisibilityState(bool is_visible);
  void OnFallbackCursorModeToggled(bool is_on);
  void ApplyViewportChanges(const ApplyViewportChangesArgs& args);
  void RecordManipulationTypeCounts(cc::ManipulationInfo info);
  void SendOverscrollEventFromImplSide(const gfx::Vector2dF& overscroll_delta,
                                       cc::ElementId scroll_latched_element_id);
  void SendScrollEndEventFromImplSide(cc::ElementId scroll_latched_element_id);
  void MouseCaptureLost();
  void SetFocus(bool enable) override;
  bool SelectionBounds(WebRect& anchor, WebRect& focus) const;
  bool IsAcceleratedCompositingActive() const;
  void DidAcquirePointerLock();
  void DidNotAcquirePointerLock();
  void DidLosePointerLock();
  void ShowContextMenu(WebMenuSourceType);
  WebURL GetURLForDebugTrace();

  void SetPageScaleFactorAndLocation(float scale,
                                     bool is_pinch_gesture_active,
                                     const FloatPoint&);
  void PropagateZoomFactorToLocalFrameRoots(Frame*, float);

  float MaximumLegiblePageScale() const;
  void RefreshPageScaleFactor();
  IntSize ContentsSize() const;

  void UpdateBrowserControlsConstraint(cc::BrowserControlsState constraint);
  void UpdateICBAndResizeViewport();
  void ResizeViewWhileAnchored(float top_controls_height,
                               float bottom_controls_height,
                               bool browser_controls_shrink_layout);

  void UpdateBaseBackgroundColor();

  WebViewImpl(WebViewClient*,
              bool is_hidden,
              bool does_composite,
              WebViewImpl* opener);
  ~WebViewImpl() override;

  HitTestResult HitTestResultForRootFramePos(const PhysicalOffset&);

  void ConfigureAutoResizeMode();

  void SetIsAcceleratedCompositingActive(bool);
  void DoComposite();
  void ReallocateRenderer();

  void SetDeviceEmulationTransform(const TransformationMatrix&);
  void UpdateDeviceEmulationTransform();

  // Helper function: Widens the width of |source| by the specified margins
  // while keeping it smaller than page width.
  //
  // This method can only be called if the main frame is local.
  WebRect WidenRectWithinPageBounds(const WebRect& source,
                                    int target_margin,
                                    int minimum_margin);

  // PageWidgetEventHandler functions
  void HandleMouseLeave(LocalFrame&, const WebMouseEvent&) override;
  void HandleMouseDown(LocalFrame&, const WebMouseEvent&) override;
  WebInputEventResult HandleMouseUp(LocalFrame&, const WebMouseEvent&) override;
  WebInputEventResult HandleMouseWheel(LocalFrame&,
                                       const WebMouseWheelEvent&) override;
  WebInputEventResult HandleGestureEvent(const WebGestureEvent&) override;
  WebInputEventResult HandleKeyEvent(const WebKeyboardEvent&) override;
  WebInputEventResult HandleCharEvent(const WebKeyboardEvent&) override;

  WebInputEventResult HandleCapturedMouseEvent(const WebCoalescedInputEvent&);

  void EnablePopupMouseWheelEventListener(WebLocalFrameImpl* local_root);
  void DisablePopupMouseWheelEventListener();

  float DeviceScaleFactor() const;

  void SetRootLayer(scoped_refptr<cc::Layer>);

  LocalFrame* FocusedLocalFrameInWidget() const;
  LocalFrame* FocusedLocalFrameAvailableForIme() const;

  bool ScrollFocusedEditableElementIntoView();
  // Finds the zoom and scroll parameters for zooming into an editable element
  // with bounds |element_bounds_in_document| and caret bounds
  // |caret_bounds_in_document|. If the original element belongs to the local
  // root of MainFrameImpl(), then the bounds are exactly those of the element
  // and caret. Otherwise (when the editable element is inside an OOPIF), the
  // bounds are projection of the original element's bounds in the main frame
  // which is inside the layout area of some remote frame in this frame tree.
  void ComputeScaleAndScrollForEditableElementRects(
      const IntRect& element_bounds_in_document,
      const IntRect& caret_bounds_in_document,
      bool zoom_into_legible_scale,
      float& scale,
      IntPoint& scroll,
      bool& need_animation);

  // These member variables should not be accessed within calls to WebWidget
  // APIs. They can be called from within WebView APIs, and internal methods,
  // though these need to be sorted as being for the view or the widget also.
  struct ViewData {
    ViewData(WebViewClient* client) : client(client) {}

    // Can be null (e.g. unittests, shared workers, etc).
    WebViewClient* client;
    Persistent<Page> page;
  } as_view_;

  Persistent<ChromeClient> chrome_client_;

  // This is the size of the page that the web contents will render into. This
  // is usually, but not necessarily the same as the VisualViewport size. The
  // VisualViewport is the 'inner' viewport, and can be smaller than the size of
  // the page. This allows the browser to shrink the size of the displayed
  // contents [e.g. to accomodate a keyboard] without forcing the web page to
  // relayout. For more details, see the header for the VisualViewport class.
  WebSize size_;
  // If true, automatically resize the layout view around its content.
  bool should_auto_resize_ = false;
  // The lower bound on the size when auto-resizing.
  IntSize min_auto_size_;
  // The upper bound on the size when auto-resizing.
  IntSize max_auto_size_;

  // An object that can be used to manipulate m_page->settings() without linking
  // against WebCore. This is lazily allocated the first time GetWebSettings()
  // is called.
  std::unique_ptr<WebSettingsImpl> web_settings_;

  // Keeps track of the current zoom level. 0 means no zoom, positive numbers
  // mean zoom in, negative numbers mean zoom out.
  double zoom_level_ = 0.;

  const double minimum_zoom_level_;
  const double maximum_zoom_level_;

  // Additional zoom factor used to scale the content by device scale factor.
  double zoom_factor_for_device_scale_factor_ = 0.;

  // This value, when multiplied by the font scale factor, gives the maximum
  // page scale that can result from automatic zooms.
  float maximum_legible_scale_ = 1.f;

  // The scale moved to by the latest double tap zoom, if any.
  float double_tap_zoom_page_scale_factor_ = 0.f;
  // Have we sent a double-tap zoom and not yet heard back the scale?
  bool double_tap_zoom_pending_ = false;

  // Used for testing purposes.
  bool enable_fake_page_scale_animation_for_testing_ = false;
  IntPoint fake_page_scale_animation_target_position_;
  float fake_page_scale_animation_page_scale_factor_ = 0.f;
  bool fake_page_scale_animation_use_anchor_ = false;

  float compositor_device_scale_factor_override_ = 0.f;
  TransformationMatrix device_emulation_transform_;

  // Webkit expects keyPress events to be suppressed if the associated keyDown
  // event was handled. Safari implements this behavior by peeking out the
  // associated WM_CHAR event if the keydown was handled. We emulate
  // this behavior by setting this flag if the keyDown was handled.
  bool suppress_next_keypress_event_ = false;

  // TODO(ekaramad): Can we remove this and make sure IME events are not called
  // when there is no page focus?
  // Represents whether or not this object should process incoming IME events.
  bool ime_accept_events_ = true;

  // The popup associated with an input/select element. The popup is owned via
  // closership (self-owned-but-deleted-via-close) by RenderWidget. We also hold
  // a reference here because we can extend the lifetime of the popup while
  // handling input events in order to compare its popup client after it was
  // closed.
  scoped_refptr<WebPagePopupImpl> page_popup_;

  // This stores the last hidden page popup. If a GestureTap attempts to open
  // the popup that is closed by its previous GestureTapDown, the popup remains
  // closed.
  scoped_refptr<WebPagePopupImpl> last_hidden_page_popup_;

  Persistent<DevToolsEmulator> dev_tools_emulator_;

  // Whether the user can press tab to focus links.
  bool tabs_to_links_ = false;

  // If set, the (plugin) element which has mouse capture.
  Persistent<HTMLPlugInElement> mouse_capture_element_;

  // WebViews, and WebWidgets, are used to host a Page. The WidgetClient()
  // provides compositing support for the WebView.
  // In some cases, a WidgetClient() is not provided, or it informs us that
  // it won't be presenting content via a compositor.
  //
  // TODO(dcheng): All WebViewImpls should have an associated LayerTreeView,
  // but for various reasons, that's not the case... WebView plugin, printing,
  // workers, and tests don't use a compositor in their WebViews. Sometimes
  // they avoid the compositor by using a null client, and sometimes by having
  // the client return a null compositor. We should make things more consistent
  // and clear.
  const bool does_composite_;
  cc::AnimationHost* animation_host_ = nullptr;

  scoped_refptr<cc::Layer> root_layer_;
  bool matches_heuristics_for_gpu_rasterization_ = false;

  std::unique_ptr<FullscreenController> fullscreen_controller_;

  SkColor base_background_color_ = Color::kWhite;
  bool base_background_color_override_enabled_ = false;
  SkColor base_background_color_override_ = Color::kTransparent;
  bool background_color_override_enabled_ = false;
  SkColor background_color_override_ = Color::kTransparent;
  float zoom_factor_override_ = 0.f;

  bool should_dispatch_first_visually_non_empty_layout_ = false;
  bool should_dispatch_first_layout_after_finished_parsing_ = false;
  bool should_dispatch_first_layout_after_finished_loading_ = false;
  blink::mojom::DisplayMode display_mode_ = blink::mojom::DisplayMode::kBrowser;

  // TODO(bokan): Temporary debugging added to diagnose
  // https://crbug.com/992315. Somehow we're synchronously calling
  // WebViewImpl::Close while handling an input event.
  bool debug_inside_input_handling_ = false;

  FloatSize elastic_overscroll_;

  Persistent<EventListener> popup_mouse_wheel_event_listener_;

  // The local root whose document has |popup_mouse_wheel_event_listener_|
  // registered.
  WeakPersistent<WebLocalFrameImpl> local_root_with_empty_mouse_wheel_listener_;

  // The WebWidget for the main frame. This is expected to be unset when the
  // WebWidget destroys itself.
  WebWidget* web_widget_ = nullptr;

  WebPageImportanceSignals page_importance_signals_;

  // We defer commits when transitioning to a new page. ChromeClientImpl calls
  // StopDeferringCommits() to release this when a new page is loaded.
  std::unique_ptr<cc::ScopedDeferMainFrameUpdate>
      scoped_defer_main_frame_update_;

  Persistent<ResizeViewportAnchor> resize_viewport_anchor_;

  // Set when a measurement begins, reset when the measurement is taken.
  base::Optional<base::TimeTicks> raf_aligned_input_start_time_;
  base::Optional<base::TimeTicks> update_layers_start_time_;
  base::Optional<base::TimeTicks> commit_compositor_frame_start_time_;
};

// We have no ways to check if the specified WebView is an instance of
// WebViewImpl because WebViewImpl is the only implementation of WebView.
DEFINE_TYPE_CASTS(WebViewImpl, WebView, webView, true, true);

}  // namespace blink

#endif
