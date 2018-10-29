/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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
#include "third_party/blink/renderer/core/exported/web_view_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_image.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/public/platform/web_menu_source_type.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/public/platform/web_text_input_info.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/public/web/web_media_player_action.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_action.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_scoped_user_gesture.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/context_features_client_impl.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/html_interchange.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/ui_event_with_key_state.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/fullscreen_controller.h"
#include "third_party/blink/renderer/core/frame/link_highlights.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/resize_viewport_anchor.h"
#include "third_party/blink/renderer/core/frame/rotation_viewport_anchor.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/touch_action_util.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader_state_machine.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/loader/prerenderer_client.h"
#include "third_party/blink/renderer/core/page/chrome_client_impl.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/context_menu_provider.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_host.h"
#include "third_party/blink/renderer/platform/cursor.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

#if defined(WTF_USE_DEFAULT_RENDER_THEME)
#include "third_party/blink/renderer/core/layout/layout_theme_default.h"
#endif

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath>  // for std::pow

// The following constants control parameters for automated scaling of webpages
// (such as due to a double tap gesture or find in page etc.). These are
// experimentally determined.
static const int touchPointPadding = 32;
static const int nonUserInitiatedPointPadding = 11;
static const float minScaleDifference = 0.01f;
static const float doubleTapZoomContentDefaultMargin = 5;
static const float doubleTapZoomContentMinimumMargin = 2;
static const double doubleTapZoomAnimationDurationInSeconds = 0.25;
static const float doubleTapZoomAlreadyLegibleRatio = 1.2f;

static const double findInPageAnimationDurationInSeconds = 0;

// Constants for viewport anchoring on resize.
static const float viewportAnchorCoordX = 0.5f;
static const float viewportAnchorCoordY = 0;

// Constants for zooming in on a focused text field.
static const double scrollAndScaleAnimationDurationInSeconds = 0.2;
static const int minReadableCaretHeight = 16;
static const int minReadableCaretHeightForTextArea = 13;
static const float minScaleChangeToTriggerZoom = 1.5f;
static const float leftBoxRatio = 0.3f;
static const int caretPadding = 10;

namespace blink {

// Change the text zoom level by kTextSizeMultiplierRatio each time the user
// zooms text in or out (ie., change by 20%).  The min and max values limit
// text zoom to half and 3x the original text size.  These three values match
// those in Apple's port in WebKit/WebKit/WebView/WebView.mm
const double WebView::kTextSizeMultiplierRatio = 1.2;
const double WebView::kMinTextSizeMultiplier = 0.5;
const double WebView::kMaxTextSizeMultiplier = 3.0;

// Used to defer all page activity in cases where the embedder wishes to run
// a nested event loop. Using a stack enables nesting of message loop
// invocations.
static Vector<std::unique_ptr<ScopedPagePauser>>& PagePauserStack() {
  DEFINE_STATIC_LOCAL(Vector<std::unique_ptr<ScopedPagePauser>>, pauser_stack,
                      ());
  return pauser_stack;
}

void WebView::WillEnterModalLoop() {
  PagePauserStack().push_back(std::make_unique<ScopedPagePauser>());
}

void WebView::DidExitModalLoop() {
  DCHECK(PagePauserStack().size());
  PagePauserStack().pop_back();
}

// static
HashSet<WebViewImpl*>& WebViewImpl::AllInstances() {
  DEFINE_STATIC_LOCAL(HashSet<WebViewImpl*>, all_instances, ());
  return all_instances;
}

static bool g_should_use_external_popup_menus = false;

void WebView::SetUseExternalPopupMenus(bool use_external_popup_menus) {
  g_should_use_external_popup_menus = use_external_popup_menus;
}

bool WebViewImpl::UseExternalPopupMenus() {
  return g_should_use_external_popup_menus;
}

namespace {

class EmptyEventListener final : public EventListener {
 public:
  static EmptyEventListener* Create() { return new EmptyEventListener(); }

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

 private:
  EmptyEventListener() : EventListener(kCPPEventListenerType) {}

  void handleEvent(ExecutionContext* execution_context, Event*) override {}
};

}  // namespace

// WebView ----------------------------------------------------------------

WebView* WebView::Create(WebViewClient* client,
                         WebWidgetClient* widget_client,
                         mojom::PageVisibilityState visibility_state,
                         WebView* opener) {
  return WebViewImpl::Create(client, widget_client, visibility_state,
                             static_cast<WebViewImpl*>(opener));
}

WebViewImpl* WebViewImpl::Create(WebViewClient* client,
                                 WebWidgetClient* widget_client,
                                 mojom::PageVisibilityState visibility_state,
                                 WebViewImpl* opener) {
  // Pass the WebViewImpl's self-reference to the caller.
  auto web_view = base::AdoptRef(
      new WebViewImpl(client, widget_client, visibility_state, opener));
  web_view->AddRef();
  return web_view.get();
}

void WebView::UpdateVisitedLinkState(unsigned long long link_hash) {
  Page::VisitedStateChanged(link_hash);
}

void WebView::ResetVisitedLinkState(bool invalidate_visited_link_hashes) {
  Page::AllVisitedStateChanged(invalidate_visited_link_hashes);
}

void WebViewImpl::SetPrerendererClient(
    WebPrerendererClient* prerenderer_client) {
  DCHECK(page_);
  ProvidePrerendererClientTo(*page_,
                             new PrerendererClient(*page_, prerenderer_client));
}

WebViewImpl::WebViewImpl(WebViewClient* client,
                         WebWidgetClient* widget_client,
                         mojom::PageVisibilityState visibility_state,
                         WebViewImpl* opener)
    : client_(client),
      widget_client_(widget_client),
      chrome_client_(ChromeClientImpl::Create(this)),
      should_auto_resize_(false),
      zoom_level_(0),
      minimum_zoom_level_(ZoomFactorToZoomLevel(kMinTextSizeMultiplier)),
      maximum_zoom_level_(ZoomFactorToZoomLevel(kMaxTextSizeMultiplier)),
      zoom_factor_for_device_scale_factor_(0.f),
      maximum_legible_scale_(1),
      double_tap_zoom_page_scale_factor_(0),
      double_tap_zoom_pending_(false),
      enable_fake_page_scale_animation_for_testing_(false),
      fake_page_scale_animation_page_scale_factor_(0),
      fake_page_scale_animation_use_anchor_(false),
      compositor_device_scale_factor_override_(0),
      suppress_next_keypress_event_(false),
      ime_accept_events_(true),
      dev_tools_emulator_(nullptr),
      tabs_to_links_(false),
      layer_tree_view_(nullptr),
      root_layer_(nullptr),
      root_graphics_layer_(nullptr),
      visual_viewport_container_layer_(nullptr),
      matches_heuristics_for_gpu_rasterization_(false),
      fullscreen_controller_(FullscreenController::Create(this)),
      base_background_color_(Color::kWhite),
      base_background_color_override_enabled_(false),
      base_background_color_override_(Color::kTransparent),
      background_color_override_enabled_(false),
      background_color_override_(Color::kTransparent),
      zoom_factor_override_(0),
      should_dispatch_first_visually_non_empty_layout_(false),
      should_dispatch_first_layout_after_finished_parsing_(false),
      should_dispatch_first_layout_after_finished_loading_(false),
      display_mode_(kWebDisplayModeBrowser),
      elastic_overscroll_(FloatSize()),
      mutator_dispatcher_(nullptr),
      override_compositor_visibility_(false) {
  DCHECK_EQ(!!client_, !!widget_client_);
  Page::PageClients page_clients;
  page_clients.chrome_client = chrome_client_.Get();

  page_ =
      Page::CreateOrdinary(page_clients, opener ? opener->GetPage() : nullptr);
  CoreInitializer::GetInstance().ProvideModulesToPage(*page_, client_);
  SetVisibilityState(visibility_state, true);

  // WebViews, and WebWidgets, are used to host a Page and present it via a
  // WebLayerTreeView compositor. The WidgetClient() provides compositing
  // support for the WebView.
  // In some cases, a WidgetClient() is not provided, or it informs us that
  // it won't be presenting content via a compositor. In that case we keep the
  // Page in the loop so that it will paint all content into the root layer
  // as multiple layers can only be used when compositing them together later.
  //
  // TODO(dcheng): All WebViewImpls should have an associated LayerTreeView,
  // but for various reasons, that's not the case... WebView plugin, printing,
  // workers, and tests don't use a compositor in their WebViews. Sometimes
  // they avoid the compositor by using a null client, and sometimes by having
  // the client return a null compositor. We should make things more consistent
  // and clear.
  if (WidgetClient() && !WidgetClient()->AllowsBrokenNullLayerTreeView())
    page_->GetSettings().SetAcceleratedCompositingEnabled(true);

  dev_tools_emulator_ = DevToolsEmulator::Create(this);

  AllInstances().insert(this);

  page_importance_signals_.SetObserver(client);
  resize_viewport_anchor_ = new ResizeViewportAnchor(*page_);
}

WebViewImpl::~WebViewImpl() {
  DCHECK(!page_);
}

WebDevToolsAgentImpl* WebViewImpl::MainFrameDevToolsAgentImpl() {
  WebLocalFrameImpl* main_frame = MainFrameImpl();
  return main_frame ? main_frame->DevToolsAgentImpl() : nullptr;
}

WebLocalFrameImpl* WebViewImpl::MainFrameImpl() const {
  return page_ && page_->MainFrame() && page_->MainFrame()->IsLocalFrame()
             ? WebLocalFrameImpl::FromFrame(page_->DeprecatedLocalMainFrame())
             : nullptr;
}

bool WebViewImpl::TabKeyCyclesThroughElements() const {
  DCHECK(page_);
  return page_->TabKeyCyclesThroughElements();
}

void WebViewImpl::SetTabKeyCyclesThroughElements(bool value) {
  if (page_)
    page_->SetTabKeyCyclesThroughElements(value);
}

void WebViewImpl::HandleMouseLeave(LocalFrame& main_frame,
                                   const WebMouseEvent& event) {
  client_->SetMouseOverURL(WebURL());
  PageWidgetEventHandler::HandleMouseLeave(main_frame, event);
}

void WebViewImpl::HandleMouseDown(LocalFrame& main_frame,
                                  const WebMouseEvent& event) {
  // If there is a popup open, close it as the user is clicking on the page
  // (outside of the popup). We also save it so we can prevent a click on an
  // element from immediately reopening the same popup.
  scoped_refptr<WebPagePopupImpl> page_popup;
  if (event.button == WebMouseEvent::Button::kLeft) {
    page_popup = page_popup_;
    HidePopups();
    DCHECK(!page_popup_);
  }

  // Take capture on a mouse down on a plugin so we can send it mouse events.
  // If the hit node is a plugin but a scrollbar is over it don't start mouse
  // capture because it will interfere with the scrollbar receiving events.
  if (event.button == WebMouseEvent::Button::kLeft &&
      page_->MainFrame()->IsLocalFrame()) {
    HitTestLocation location(
        page_->DeprecatedLocalMainFrame()->View()->ConvertFromRootFrame(
            event.PositionInWidget()));
    HitTestResult result(page_->DeprecatedLocalMainFrame()
                             ->GetEventHandler()
                             .HitTestResultAtLocation(location));
    result.SetToShadowHostIfInRestrictedShadowRoot();
    Node* hit_node = result.InnerNodeOrImageMapImage();

    if (!result.GetScrollbar() && hit_node && hit_node->GetLayoutObject() &&
        hit_node->GetLayoutObject()->IsEmbeddedObject()) {
      mouse_capture_node_ = hit_node;
      page_->DeprecatedLocalMainFrame()->Client()->SetMouseCapture(true);
      TRACE_EVENT_ASYNC_BEGIN0("input", "capturing mouse", this);
    }
  }

  PageWidgetEventHandler::HandleMouseDown(main_frame, event);

  if (event.button == WebMouseEvent::Button::kLeft && mouse_capture_node_) {
    mouse_capture_gesture_token_ =
        main_frame.GetEventHandler().TakeLastMouseDownGestureToken();
  }

  if (page_popup_ && page_popup &&
      page_popup_->HasSamePopupClient(page_popup.get())) {
    // That click triggered a page popup that is the same as the one we just
    // closed.  It needs to be closed.
    CancelPagePopup();
  }

  // Dispatch the contextmenu event regardless of if the click was swallowed.
  if (!GetPage()->GetSettings().GetShowContextMenuOnMouseUp()) {
#if defined(OS_MACOSX)
    if (event.button == WebMouseEvent::Button::kRight ||
        (event.button == WebMouseEvent::Button::kLeft &&
         event.GetModifiers() & WebMouseEvent::kControlKey))
      MouseContextMenu(event);
#else
    if (event.button == WebMouseEvent::Button::kRight)
      MouseContextMenu(event);
#endif
  }
}

void WebViewImpl::SetDisplayMode(WebDisplayMode mode) {
  display_mode_ = mode;
  if (!MainFrameImpl() || !MainFrameImpl()->GetFrameView())
    return;

  MainFrameImpl()->GetFrameView()->SetDisplayMode(mode);
}

void WebViewImpl::MouseContextMenu(const WebMouseEvent& event) {
  if (!MainFrameImpl() || !MainFrameImpl()->GetFrameView())
    return;

  page_->GetContextMenuController().ClearContextMenu();

  WebMouseEvent transformed_event =
      TransformWebMouseEvent(MainFrameImpl()->GetFrameView(), event);
  transformed_event.menu_source_type = kMenuSourceMouse;
  LayoutPoint position_in_root_frame(transformed_event.PositionInRootFrame());

  // Find the right target frame. See issue 1186900.
  HitTestResult result = HitTestResultForRootFramePos(position_in_root_frame);
  Frame* target_frame;
  if (result.InnerNodeOrImageMapImage())
    target_frame = result.InnerNodeOrImageMapImage()->GetDocument().GetFrame();
  else
    target_frame = page_->GetFocusController().FocusedOrMainFrame();

  if (!target_frame->IsLocalFrame())
    return;

  LocalFrame* target_local_frame = ToLocalFrame(target_frame);
  {
    ContextMenuAllowedScope scope;
    target_local_frame->GetEventHandler().SendContextMenuEvent(
        transformed_event, nullptr);
  }
  // Actually showing the context menu is handled by the ContextMenuController
  // implementation...
}

void WebViewImpl::HandleMouseUp(LocalFrame& main_frame,
                                const WebMouseEvent& event) {
  PageWidgetEventHandler::HandleMouseUp(main_frame, event);

  if (GetPage()->GetSettings().GetShowContextMenuOnMouseUp()) {
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::Button::kRight)
      MouseContextMenu(event);
  }
}

WebInputEventResult WebViewImpl::HandleMouseWheel(
    LocalFrame& main_frame,
    const WebMouseWheelEvent& event) {
  HidePopups();
  return PageWidgetEventHandler::HandleMouseWheel(main_frame, event);
}

WebInputEventResult WebViewImpl::HandleGestureEvent(
    const WebGestureEvent& event) {
  if (!client_ || !WidgetClient() || !client_->CanHandleGestureEvent()) {
    return WebInputEventResult::kNotHandled;
  }

  WebInputEventResult event_result = WebInputEventResult::kNotHandled;
  bool event_cancelled = false;  // for disambiguation

  // Fling events are not sent to the renderer.
  CHECK(event.GetType() != WebInputEvent::kGestureFlingStart);
  CHECK(event.GetType() != WebInputEvent::kGestureFlingCancel);

  WebGestureEvent scaled_event =
      TransformWebGestureEvent(MainFrameImpl()->GetFrameView(), event);

  // Special handling for double tap and scroll events as we don't want to
  // hit test for them.
  switch (event.GetType()) {
    case WebInputEvent::kGestureDoubleTap:
      if (web_settings_->DoubleTapToZoomEnabled() &&
          MinimumPageScaleFactor() != MaximumPageScaleFactor()) {
        if (auto* main_frame = MainFrameImpl()) {
          IntPoint pos_in_root_frame =
              FlooredIntPoint(scaled_event.PositionInRootFrame());
          WebRect block_bounds =
              main_frame->FrameWidgetImpl()->ComputeBlockBound(
                  pos_in_root_frame, false);
          AnimateDoubleTapZoom(pos_in_root_frame, block_bounds);
        }
      }
      event_result = WebInputEventResult::kHandledSystem;
      WidgetClient()->DidHandleGestureEvent(event, event_cancelled);
      return event_result;
    case WebInputEvent::kGestureScrollBegin:
    case WebInputEvent::kGestureScrollEnd:
    case WebInputEvent::kGestureScrollUpdate:
      // Scrolling-related gesture events invoke EventHandler recursively for
      // each frame down the chain, doing a single-frame hit-test per frame.
      // This matches handleWheelEvent.  Perhaps we could simplify things by
      // rewriting scroll handling to work inner frame out, and then unify with
      // other gesture events.
      event_result = MainFrameImpl()
                         ->GetFrame()
                         ->GetEventHandler()
                         .HandleGestureScrollEvent(scaled_event);
      WidgetClient()->DidHandleGestureEvent(event, event_cancelled);
      return event_result;
    default:
      break;
  }

  // Hit test across all frames and do touch adjustment as necessary for the
  // event type.
  GestureEventWithHitTestResults targeted_event =
      page_->DeprecatedLocalMainFrame()->GetEventHandler().TargetGestureEvent(
          scaled_event);

  // Handle link highlighting outside the main switch to avoid getting lost in
  // the complicated set of cases handled below.
  switch (event.GetType()) {
    case WebInputEvent::kGestureShowPress:
      // Queue a highlight animation, then hand off to regular handler.
      EnableTapHighlightAtPoint(targeted_event);
      break;
    case WebInputEvent::kGestureTapCancel:
    case WebInputEvent::kGestureTap:
    case WebInputEvent::kGestureLongPress:
      GetPage()->GetLinkHighlights().StartHighlightAnimationIfNeeded();
      break;
    default:
      break;
  }

  switch (event.GetType()) {
    case WebInputEvent::kGestureTap: {
      {
        ContextMenuAllowedScope scope;
        event_result =
            MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
                targeted_event);
      }

      if (page_popup_ && last_hidden_page_popup_ &&
          page_popup_->HasSamePopupClient(last_hidden_page_popup_.get())) {
        // The tap triggered a page popup that is the same as the one we just
        // closed. It needs to be closed.
        CancelPagePopup();
      }
      last_hidden_page_popup_ = nullptr;
      break;
    }
    case WebInputEvent::kGestureTwoFingerTap:
    case WebInputEvent::kGestureLongPress:
    case WebInputEvent::kGestureLongTap: {
      if (!MainFrameImpl() || !MainFrameImpl()->GetFrameView())
        break;

      page_->GetContextMenuController().ClearContextMenu();
      {
        ContextMenuAllowedScope scope;
        event_result =
            MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
                targeted_event);
      }

      break;
    }
    case WebInputEvent::kGestureTapDown: {
      // Touch pinch zoom and scroll on the page (outside of a popup) must hide
      // the popup. In case of a touch scroll or pinch zoom, this function is
      // called with GestureTapDown rather than a GSB/GSU/GSE or GPB/GPU/GPE.
      // When we close a popup because of a GestureTapDown, we also save it so
      // we can prevent the following GestureTap from immediately reopening the
      // same popup.
      last_hidden_page_popup_ = page_popup_;
      HidePopups();
      DCHECK(!page_popup_);
      event_result =
          MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
              targeted_event);
      break;
    }
    case WebInputEvent::kGestureTapCancel: {
      last_hidden_page_popup_ = nullptr;
      event_result =
          MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
              targeted_event);
      break;
    }
    case WebInputEvent::kGestureShowPress: {
      event_result =
          MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
              targeted_event);
      break;
    }
    case WebInputEvent::kGestureTapUnconfirmed: {
      event_result =
          MainFrameImpl()->GetFrame()->GetEventHandler().HandleGestureEvent(
              targeted_event);
      break;
    }
    default: { NOTREACHED(); }
  }
  WidgetClient()->DidHandleGestureEvent(event, event_cancelled);
  return event_result;
}

bool WebViewImpl::StartPageScaleAnimation(const IntPoint& target_position,
                                          bool use_anchor,
                                          float new_scale,
                                          double duration_in_seconds) {
  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();
  WebPoint clamped_point = target_position;
  if (!use_anchor) {
    clamped_point =
        visual_viewport.ClampDocumentOffsetAtScale(target_position, new_scale);
    if (!duration_in_seconds) {
      SetPageScaleFactor(new_scale);

      LocalFrameView* view = MainFrameImpl()->GetFrameView();
      if (view && view->GetScrollableArea()) {
        view->GetScrollableArea()->SetScrollOffset(
            ScrollOffset(clamped_point.x, clamped_point.y),
            kProgrammaticScroll);
      }

      return false;
    }
  }
  if (use_anchor && new_scale == PageScaleFactor())
    return false;

  if (enable_fake_page_scale_animation_for_testing_) {
    fake_page_scale_animation_target_position_ = target_position;
    fake_page_scale_animation_use_anchor_ = use_anchor;
    fake_page_scale_animation_page_scale_factor_ = new_scale;
  } else {
    if (!layer_tree_view_)
      return false;
    layer_tree_view_->StartPageScaleAnimation(
        static_cast<gfx::Vector2d>(target_position), use_anchor, new_scale,
        duration_in_seconds);
  }
  return true;
}

void WebViewImpl::EnableFakePageScaleAnimationForTesting(bool enable) {
  enable_fake_page_scale_animation_for_testing_ = enable;
  fake_page_scale_animation_target_position_ = IntPoint();
  fake_page_scale_animation_use_anchor_ = false;
  fake_page_scale_animation_page_scale_factor_ = 0;
}

void WebViewImpl::SetShowFPSCounter(bool show) {
  if (layer_tree_view_) {
    TRACE_EVENT0("blink", "WebViewImpl::setShowFPSCounter");
    layer_tree_view_->SetShowFPSCounter(show);
  }
}

void WebViewImpl::SetShowPaintRects(bool show) {
  if (layer_tree_view_) {
    TRACE_EVENT0("blink", "WebViewImpl::setShowPaintRects");
    layer_tree_view_->SetShowPaintRects(show);
  }
}

void WebViewImpl::SetShowDebugBorders(bool show) {
  if (layer_tree_view_)
    layer_tree_view_->SetShowDebugBorders(show);
}

void WebViewImpl::SetShowScrollBottleneckRects(bool show) {
  if (layer_tree_view_)
    layer_tree_view_->SetShowScrollBottleneckRects(show);
}

void WebViewImpl::AcceptLanguagesChanged() {
  if (client_)
    FontCache::AcceptLanguagesChanged(client_->AcceptLanguages());

  if (!GetPage())
    return;

  GetPage()->AcceptLanguagesChanged();
}

void WebViewImpl::PausePageScheduledTasks(bool paused) {
  GetPage()->SetPaused(paused);
}

WebInputEventResult WebViewImpl::HandleKeyEvent(const WebKeyboardEvent& event) {
  DCHECK((event.GetType() == WebInputEvent::kRawKeyDown) ||
         (event.GetType() == WebInputEvent::kKeyDown) ||
         (event.GetType() == WebInputEvent::kKeyUp));
  TRACE_EVENT2("input", "WebViewImpl::handleKeyEvent", "type",
               WebInputEvent::GetName(event.GetType()), "text",
               String(event.text).Utf8());

  // Please refer to the comments explaining the m_suppressNextKeypressEvent
  // member.
  // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
  // Webkit. A keyDown event is typically associated with a keyPress(char)
  // event and a keyUp event. We reset this flag here as this is a new keyDown
  // event.
  suppress_next_keypress_event_ = false;

  // If there is a popup, it should be the one processing the event, not the
  // page.
  if (page_popup_) {
    page_popup_->HandleKeyEvent(event);
    // We need to ignore the next Char event after this otherwise pressing
    // enter when selecting an item in the popup will go to the page.
    if (WebInputEvent::kRawKeyDown == event.GetType())
      suppress_next_keypress_event_ = true;
    return WebInputEventResult::kHandledSystem;
  }

  Frame* focused_frame = FocusedCoreFrame();
  if (!focused_frame || !focused_frame->IsLocalFrame())
    return WebInputEventResult::kNotHandled;

  LocalFrame* frame = ToLocalFrame(focused_frame);

  WebInputEventResult result = frame->GetEventHandler().KeyEvent(event);
  if (result != WebInputEventResult::kNotHandled) {
    if (WebInputEvent::kRawKeyDown == event.GetType()) {
      // Suppress the next keypress event unless the focused node is a plugin
      // node.  (Flash needs these keypress events to handle non-US keyboards.)
      Element* element = FocusedElement();
      if (element && element->GetLayoutObject() &&
          element->GetLayoutObject()->IsEmbeddedObject()) {
        if (event.windows_key_code == VKEY_TAB) {
          // If the plugin supports keyboard focus then we should not send a tab
          // keypress event.
          WebPluginContainerImpl* plugin_view =
              ToLayoutEmbeddedContent(element->GetLayoutObject())->Plugin();
          if (plugin_view && plugin_view->SupportsKeyboardFocus()) {
            suppress_next_keypress_event_ = true;
          }
        }
      } else {
        suppress_next_keypress_event_ = true;
      }
    }
    return result;
  }

#if !defined(OS_MACOSX)
  const WebInputEvent::Type kContextMenuKeyTriggeringEventType =
#if defined(OS_WIN)
      WebInputEvent::kKeyUp;
#else
      WebInputEvent::kRawKeyDown;
#endif
  const WebInputEvent::Type kShiftF10TriggeringEventType =
      WebInputEvent::kRawKeyDown;

  bool is_unmodified_menu_key =
      !(event.GetModifiers() & WebInputEvent::kInputModifiers) &&
      event.windows_key_code == VKEY_APPS;
  bool is_shift_f10 = (event.GetModifiers() & WebInputEvent::kInputModifiers) ==
                          WebInputEvent::kShiftKey &&
                      event.windows_key_code == VKEY_F10;
  if ((is_unmodified_menu_key &&
       event.GetType() == kContextMenuKeyTriggeringEventType) ||
      (is_shift_f10 && event.GetType() == kShiftF10TriggeringEventType)) {
    SendContextMenuEvent();
    return WebInputEventResult::kHandledSystem;
  }
#endif  // !defined(OS_MACOSX)

  return WebInputEventResult::kNotHandled;
}

WebInputEventResult WebViewImpl::HandleCharEvent(
    const WebKeyboardEvent& event) {
  DCHECK_EQ(event.GetType(), WebInputEvent::kChar);
  TRACE_EVENT1("input", "WebViewImpl::handleCharEvent", "text",
               String(event.text).Utf8());

  // Please refer to the comments explaining the m_suppressNextKeypressEvent
  // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
  // handled by Webkit. A keyDown event is typically associated with a
  // keyPress(char) event and a keyUp event. We reset this flag here as it
  // only applies to the current keyPress event.
  bool suppress = suppress_next_keypress_event_;
  suppress_next_keypress_event_ = false;

  // If there is a popup, it should be the one processing the event, not the
  // page.
  if (page_popup_)
    return page_popup_->HandleKeyEvent(event);

  LocalFrame* frame = ToLocalFrame(FocusedCoreFrame());
  if (!frame) {
    return suppress ? WebInputEventResult::kHandledSuppressed
                    : WebInputEventResult::kNotHandled;
  }

  EventHandler& handler = frame->GetEventHandler();

  if (!event.IsCharacterKey())
    return WebInputEventResult::kHandledSuppressed;

  // Accesskeys are triggered by char events and can't be suppressed.
  if (handler.HandleAccessKey(event))
    return WebInputEventResult::kHandledSystem;

  // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
  // the eventHandler::keyEvent. We mimic this behavior on all platforms since
  // for now we are converting other platform's key events to windows key
  // events.
  if (event.is_system_key)
    return WebInputEventResult::kNotHandled;

  if (suppress)
    return WebInputEventResult::kHandledSuppressed;

  WebInputEventResult result = handler.KeyEvent(event);
  if (result != WebInputEventResult::kNotHandled)
    return result;

  return WebInputEventResult::kNotHandled;
}

WebRect WebViewImpl::WidenRectWithinPageBounds(const WebRect& source,
                                               int target_margin,
                                               int minimum_margin) {
  WebSize max_size;
  IntSize scroll_offset;
  if (MainFrame()) {
    // TODO(lukasza): https://crbug.com/734209: The DCHECK below holds now, but
    // only because all of the callers don't support OOPIFs and exit early if
    // the main frame is not local.
    DCHECK(MainFrame()->IsWebLocalFrame());
    max_size = MainFrame()->ToWebLocalFrame()->DocumentSize();
    scroll_offset = MainFrame()->ToWebLocalFrame()->GetScrollOffset();
  }
  int left_margin = target_margin;
  int right_margin = target_margin;

  const int absolute_source_x = source.x + scroll_offset.Width();
  if (left_margin > absolute_source_x) {
    left_margin = absolute_source_x;
    right_margin = std::max(left_margin, minimum_margin);
  }

  const int maximum_right_margin =
      max_size.width - (source.width + absolute_source_x);
  if (right_margin > maximum_right_margin) {
    right_margin = maximum_right_margin;
    left_margin = std::min(left_margin, std::max(right_margin, minimum_margin));
  }

  const int new_width = source.width + left_margin + right_margin;
  const int new_x = source.x - left_margin;

  DCHECK_GE(new_width, 0);
  DCHECK_LE(scroll_offset.Width() + new_x + new_width, max_size.width);

  return WebRect(new_x, source.y, new_width, source.height);
}

float WebViewImpl::MaximumLegiblePageScale() const {
  // Pages should be as legible as on desktop when at dpi scale, so no
  // need to zoom in further when automatically determining zoom level
  // (after double tap, find in page, etc), though the user should still
  // be allowed to manually pinch zoom in further if they desire.
  if (GetPage()) {
    return maximum_legible_scale_ *
           GetPage()->GetSettings().GetAccessibilityFontScaleFactor();
  }
  return maximum_legible_scale_;
}

void WebViewImpl::ComputeScaleAndScrollForBlockRect(
    const WebPoint& hit_point_in_root_frame,
    const WebRect& block_rect_in_root_frame,
    float padding,
    float default_scale_when_already_legible,
    float& scale,
    WebPoint& scroll) {
  scale = PageScaleFactor();
  scroll.x = scroll.y = 0;

  WebRect rect = block_rect_in_root_frame;

  if (!rect.IsEmpty()) {
    float default_margin = doubleTapZoomContentDefaultMargin;
    float minimum_margin = doubleTapZoomContentMinimumMargin;
    // We want the margins to have the same physical size, which means we
    // need to express them in post-scale size. To do that we'd need to know
    // the scale we're scaling to, but that depends on the margins. Instead
    // we express them as a fraction of the target rectangle: this will be
    // correct if we end up fully zooming to it, and won't matter if we
    // don't.
    rect = WidenRectWithinPageBounds(
        rect, static_cast<int>(default_margin * rect.width / size_.width),
        static_cast<int>(minimum_margin * rect.width / size_.width));
    // Fit block to screen, respecting limits.
    scale = static_cast<float>(size_.width) / rect.width;
    scale = std::min(scale, MaximumLegiblePageScale());
    if (PageScaleFactor() < default_scale_when_already_legible)
      scale = std::max(scale, default_scale_when_already_legible);
    scale = ClampPageScaleFactorToLimits(scale);
  }

  // FIXME: If this is being called for auto zoom during find in page,
  // then if the user manually zooms in it'd be nice to preserve the
  // relative increase in zoom they caused (if they zoom out then it's ok
  // to zoom them back in again). This isn't compatible with our current
  // double-tap zoom strategy (fitting the containing block to the screen)
  // though.

  float screen_width = size_.width / scale;
  float screen_height = size_.height / scale;

  // Scroll to vertically align the block.
  if (rect.height < screen_height) {
    // Vertically center short blocks.
    rect.y -= 0.5 * (screen_height - rect.height);
  } else {
    // Ensure position we're zooming to (+ padding) isn't off the bottom of
    // the screen.
    rect.y = std::max<float>(
        rect.y, hit_point_in_root_frame.y + padding - screen_height);
  }  // Otherwise top align the block.

  // Do the same thing for horizontal alignment.
  if (rect.width < screen_width) {
    rect.x -= 0.5 * (screen_width - rect.width);
  } else {
    rect.x = std::max<float>(
        rect.x, hit_point_in_root_frame.x + padding - screen_width);
  }
  scroll.x = rect.x;
  scroll.y = rect.y;

  scale = ClampPageScaleFactorToLimits(scale);
  scroll = MainFrameImpl()->GetFrameView()->RootFrameToDocument(scroll);
  scroll =
      GetPage()->GetVisualViewport().ClampDocumentOffsetAtScale(scroll, scale);
}

static Node* FindCursorDefiningAncestor(Node* node, LocalFrame* frame) {
  // Go up the tree to find the node that defines a mouse cursor style
  while (node) {
    if (node->GetLayoutObject()) {
      ECursor cursor = node->GetLayoutObject()->Style()->Cursor();
      if (cursor != ECursor::kAuto ||
          frame->GetEventHandler().UseHandCursor(node, node->IsLink()))
        break;
    }
    node = LayoutTreeBuilderTraversal::Parent(*node);
  }

  return node;
}

static bool ShowsHandCursor(Node* node, LocalFrame* frame) {
  if (!node || !node->GetLayoutObject())
    return false;

  ECursor cursor = node->GetLayoutObject()->Style()->Cursor();
  return cursor == ECursor::kPointer ||
         (cursor == ECursor::kAuto &&
          frame->GetEventHandler().UseHandCursor(node, node->IsLink()));
}

Node* WebViewImpl::BestTapNode(
    const GestureEventWithHitTestResults& targeted_tap_event) {
  TRACE_EVENT0("input", "WebViewImpl::bestTapNode");

  if (!page_ || !page_->MainFrame())
    return nullptr;

  Node* best_touch_node = targeted_tap_event.GetHitTestResult().InnerNode();
  if (!best_touch_node)
    return nullptr;

  // We might hit something like an image map that has no layoutObject on it
  // Walk up the tree until we have a node with an attached layoutObject
  while (!best_touch_node->GetLayoutObject()) {
    best_touch_node = LayoutTreeBuilderTraversal::Parent(*best_touch_node);
    if (!best_touch_node)
      return nullptr;
  }

  // Editable nodes should not be highlighted (e.g., <input>)
  if (HasEditableStyle(*best_touch_node))
    return nullptr;

  Node* cursor_defining_ancestor = FindCursorDefiningAncestor(
      best_touch_node, page_->DeprecatedLocalMainFrame());
  // We show a highlight on tap only when the current node shows a hand cursor
  if (!cursor_defining_ancestor ||
      !ShowsHandCursor(cursor_defining_ancestor,
                       page_->DeprecatedLocalMainFrame())) {
    return nullptr;
  }

  // We should pick the largest enclosing node with hand cursor set. We do this
  // by first jumping up to cursorDefiningAncestor (which is already known to
  // have hand cursor set). Then we locate the next cursor-defining ancestor up
  // in the the tree and repeat the jumps as long as the node has hand cursor
  // set.
  do {
    best_touch_node = cursor_defining_ancestor;
    cursor_defining_ancestor = FindCursorDefiningAncestor(
        LayoutTreeBuilderTraversal::Parent(*best_touch_node),
        page_->DeprecatedLocalMainFrame());
  } while (cursor_defining_ancestor &&
           ShowsHandCursor(cursor_defining_ancestor,
                           page_->DeprecatedLocalMainFrame()));

  return best_touch_node;
}

void WebViewImpl::EnableTapHighlightAtPoint(
    const GestureEventWithHitTestResults& targeted_tap_event) {
  Node* touch_node = BestTapNode(targeted_tap_event);

  HeapVector<Member<Node>> highlight_nodes;
  highlight_nodes.push_back(touch_node);

  EnableTapHighlights(highlight_nodes);
}

void WebViewImpl::EnableTapHighlights(
    HeapVector<Member<Node>>& highlight_nodes) {
  GetPage()->GetLinkHighlights().SetTapHighlights(highlight_nodes);
  UpdateAllLifecyclePhases();
}

void WebViewImpl::AnimateDoubleTapZoom(const IntPoint& point_in_root_frame,
                                       const WebRect& block_bounds) {
  DCHECK(MainFrameImpl());

  float scale;
  WebPoint scroll;

  ComputeScaleAndScrollForBlockRect(
      point_in_root_frame, block_bounds, touchPointPadding,
      MinimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio, scale,
      scroll);

  bool still_at_previous_double_tap_scale =
      (PageScaleFactor() == double_tap_zoom_page_scale_factor_ &&
       double_tap_zoom_page_scale_factor_ != MinimumPageScaleFactor()) ||
      double_tap_zoom_pending_;

  bool scale_unchanged = fabs(PageScaleFactor() - scale) < minScaleDifference;
  bool should_zoom_out = block_bounds.IsEmpty() || scale_unchanged ||
                         still_at_previous_double_tap_scale;

  bool is_animating;

  if (should_zoom_out) {
    scale = MinimumPageScaleFactor();
    IntPoint target_position =
        MainFrameImpl()->GetFrameView()->RootFrameToDocument(
            point_in_root_frame);
    is_animating = StartPageScaleAnimation(
        target_position, true, scale, doubleTapZoomAnimationDurationInSeconds);
  } else {
    is_animating = StartPageScaleAnimation(
        scroll, false, scale, doubleTapZoomAnimationDurationInSeconds);
  }

  // TODO(dglazkov): The only reason why we're using isAnimating and not just
  // checking for m_layerTreeView->hasPendingPageScaleAnimation() is because of
  // fake page scale animation plumbing for testing, which doesn't actually
  // initiate a page scale animation.
  if (is_animating) {
    double_tap_zoom_page_scale_factor_ = scale;
    double_tap_zoom_pending_ = true;
  }
}

void WebViewImpl::ZoomToFindInPageRect(const WebRect& rect_in_root_frame) {
  // TODO(lukasza): https://crbug.com/734209: Add OOPIF support.
  if (!MainFrameImpl())
    return;

  WebRect block_bounds = MainFrameImpl()->FrameWidgetImpl()->ComputeBlockBound(
      WebPoint(rect_in_root_frame.x + rect_in_root_frame.width / 2,
               rect_in_root_frame.y + rect_in_root_frame.height / 2),
      true);

  if (block_bounds.IsEmpty()) {
    // Keep current scale (no need to scroll as x,y will normally already
    // be visible). FIXME: Revisit this if it isn't always true.
    return;
  }

  float scale;
  WebPoint scroll;

  ComputeScaleAndScrollForBlockRect(
      WebPoint(rect_in_root_frame.x, rect_in_root_frame.y), block_bounds,
      nonUserInitiatedPointPadding, MinimumPageScaleFactor(), scale, scroll);

  StartPageScaleAnimation(scroll, false, scale,
                          findInPageAnimationDurationInSeconds);
}

#if !defined(OS_MACOSX)
// Mac has no way to open a context menu based on a keyboard event.
WebInputEventResult WebViewImpl::SendContextMenuEvent() {
  // The contextMenuController() holds onto the last context menu that was
  // popped up on the page until a new one is created. We need to clear
  // this menu before propagating the event through the DOM so that we can
  // detect if we create a new menu for this event, since we won't create
  // a new menu if the DOM swallows the event and the defaultEventHandler does
  // not run.
  GetPage()->GetContextMenuController().ClearContextMenu();

  {
    ContextMenuAllowedScope scope;
    Frame* focused_frame = GetPage()->GetFocusController().FocusedOrMainFrame();
    if (!focused_frame->IsLocalFrame())
      return WebInputEventResult::kNotHandled;
    // Firefox reveal focus based on "keydown" event but not "contextmenu"
    // event, we match FF.
    if (Element* focused_element =
            ToLocalFrame(focused_frame)->GetDocument()->FocusedElement())
      focused_element->scrollIntoViewIfNeeded();
    return ToLocalFrame(focused_frame)
        ->GetEventHandler()
        .ShowNonLocatedContextMenu(nullptr, kMenuSourceKeyboard);
  }
}
#else
WebInputEventResult WebViewImpl::SendContextMenuEvent() {
  return WebInputEventResult::kNotHandled;
}
#endif

void WebViewImpl::ShowContextMenuForElement(WebElement element) {
  if (!GetPage())
    return;

  GetPage()->GetContextMenuController().ClearContextMenu();
  {
    ContextMenuAllowedScope scope;
    if (LocalFrame* focused_frame = ToLocalFrame(
            GetPage()->GetFocusController().FocusedOrMainFrame())) {
      focused_frame->GetEventHandler().ShowNonLocatedContextMenu(
          element.Unwrap<Element>());
    }
  }
}

PagePopup* WebViewImpl::OpenPagePopup(PagePopupClient* client) {
  DCHECK(client);
  if (HasOpenedPopup())
    HidePopups();
  DCHECK(!page_popup_);

  WebLocalFrameImpl* frame = WebLocalFrameImpl::FromFrame(
      client->OwnerElement().GetDocument().GetFrame()->LocalFrameRoot());
  WebWidget* popup_widget = client_->CreatePopup(frame);
  // CreatePopup returns nullptr if this renderer process is about to die.
  if (!popup_widget)
    return nullptr;
  page_popup_ = ToWebPagePopupImpl(popup_widget);
  page_popup_->Initialize(this, client);
  EnablePopupMouseWheelEventListener(frame);
  return page_popup_.get();
}

void WebViewImpl::ClosePagePopup(PagePopup* popup) {
  DCHECK(popup);
  WebPagePopupImpl* popup_impl = ToWebPagePopupImpl(popup);
  DCHECK_EQ(page_popup_.get(), popup_impl);
  if (page_popup_.get() != popup_impl)
    return;
  page_popup_->ClosePopup();
}

void WebViewImpl::CleanupPagePopup() {
  page_popup_ = nullptr;
  DisablePopupMouseWheelEventListener();
}

void WebViewImpl::CancelPagePopup() {
  if (page_popup_)
    page_popup_->Cancel();
}

void WebViewImpl::EnablePopupMouseWheelEventListener(
    WebLocalFrameImpl* local_root) {
  DCHECK(!popup_mouse_wheel_event_listener_);
  Document* document = local_root->GetDocument();
  DCHECK(document);
  // We register an empty event listener, EmptyEventListener, so that mouse
  // wheel events get sent to the WebView.
  popup_mouse_wheel_event_listener_ = EmptyEventListener::Create();
  document->addEventListener(EventTypeNames::mousewheel,
                             popup_mouse_wheel_event_listener_, false);
  local_root_with_empty_mouse_wheel_listener_ = local_root;
}

void WebViewImpl::DisablePopupMouseWheelEventListener() {
  // TODO(kenrb): Concerns the same as in enablePopupMouseWheelEventListener.
  // See https://crbug.com/566130
  DCHECK(popup_mouse_wheel_event_listener_);
  Document* document =
      local_root_with_empty_mouse_wheel_listener_->GetDocument();
  DCHECK(document);
  // Document may have already removed the event listener, for instance, due
  // to a navigation, but remove it anyway.
  document->removeEventListener(EventTypeNames::mousewheel,
                                popup_mouse_wheel_event_listener_.Release(),
                                false);
  local_root_with_empty_mouse_wheel_listener_ = nullptr;
}

LocalDOMWindow* WebViewImpl::PagePopupWindow() const {
  return page_popup_ ? page_popup_->Window() : nullptr;
}

Frame* WebViewImpl::FocusedCoreFrame() const {
  return page_ ? page_->GetFocusController().FocusedOrMainFrame() : nullptr;
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::Close() {
  DCHECK(AllInstances().Contains(this));
  AllInstances().erase(this);

  if (page_) {
    // Initiate shutdown for the entire frameset.  This will cause a lot of
    // notifications to be sent.
    page_->WillBeDestroyed();
    page_.Clear();
  }

  // Reset the delegate to prevent notifications being sent as we're being
  // deleted.
  client_ = nullptr;

  Release();  // Balances a reference acquired in WebView::Create
}

WebSize WebViewImpl::Size() {
  return size_;
}

void WebViewImpl::ResizeVisualViewport(const WebSize& new_size) {
  GetPage()->GetVisualViewport().SetSize(new_size);
  GetPage()->GetVisualViewport().ClampToBoundaries();
}

void WebViewImpl::UpdateICBAndResizeViewport() {
  // We'll keep the initial containing block size from changing when the top
  // controls hide so that the ICB will always be the same size as the
  // viewport with the browser controls shown.
  IntSize icb_size = size_;
  if (GetBrowserControls().PermittedState() ==
          cc::BrowserControlsState::kBoth &&
      !GetBrowserControls().ShrinkViewport()) {
    icb_size.Expand(0, -GetBrowserControls().TotalHeight());
  }

  GetPageScaleConstraintsSet().DidChangeInitialContainingBlockSize(icb_size);

  UpdatePageDefinedViewportConstraints(MainFrameImpl()
                                           ->GetFrame()
                                           ->GetDocument()
                                           ->GetViewportData()
                                           .GetViewportDescription());
  UpdateMainFrameLayoutSize();

  GetPage()->GetVisualViewport().SetSize(size_);

  if (MainFrameImpl()->GetFrameView()) {
    MainFrameImpl()->GetFrameView()->SetInitialViewportSize(icb_size);
    if (!MainFrameImpl()->GetFrameView()->NeedsLayout())
      resize_viewport_anchor_->ResizeFrameView(MainFrameSize());
  }
}

void WebViewImpl::UpdateBrowserControlsConstraint(
    cc::BrowserControlsState constraint) {
  cc::BrowserControlsState old_permitted_state =
      GetBrowserControls().PermittedState();

  GetBrowserControls().UpdateConstraintsAndState(
      constraint, cc::BrowserControlsState::kBoth, false);

  // If the controls are going from a locked hidden to unlocked state, or vice
  // versa, the ICB size needs to change but we can't rely on getting a
  // WebViewImpl::resize since the top controls shown state may not have
  // changed.
  if ((old_permitted_state == cc::BrowserControlsState::kHidden &&
       constraint == cc::BrowserControlsState::kBoth) ||
      (old_permitted_state == cc::BrowserControlsState::kBoth &&
       constraint == cc::BrowserControlsState::kHidden)) {
    UpdateICBAndResizeViewport();
  }
}

void WebViewImpl::DidUpdateBrowserControls() {
  if (layer_tree_view_) {
    layer_tree_view_->SetBrowserControlsShownRatio(
        GetBrowserControls().ShownRatio());
    layer_tree_view_->SetBrowserControlsHeight(
        GetBrowserControls().TopHeight(), GetBrowserControls().BottomHeight(),
        GetBrowserControls().ShrinkViewport());
  }

  WebLocalFrameImpl* main_frame = MainFrameImpl();
  if (!main_frame)
    return;

  LocalFrameView* view = main_frame->GetFrameView();
  if (!view)
    return;

  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();

  {
    // This object will save the current visual viewport offset w.r.t. the
    // document and restore it when the object goes out of scope. It's
    // needed since the browser controls adjustment will change the maximum
    // scroll offset and we may need to reposition them to keep the user's
    // apparent position unchanged.
    ResizeViewportAnchor::ResizeScope resize_scope(*resize_viewport_anchor_);

    visual_viewport.SetBrowserControlsAdjustment(
        GetBrowserControls().UnreportedSizeAdjustment());
  }
}

void WebViewImpl::SetOverscrollBehavior(
    const cc::OverscrollBehavior& overscroll_behavior) {
  if (layer_tree_view_)
    layer_tree_view_->SetOverscrollBehavior(overscroll_behavior);
}

BrowserControls& WebViewImpl::GetBrowserControls() {
  return GetPage()->GetBrowserControls();
}

void WebViewImpl::ResizeViewWhileAnchored(float top_controls_height,
                                          float bottom_controls_height,
                                          bool browser_controls_shrink_layout) {
  DCHECK(MainFrameImpl());

  GetBrowserControls().SetHeight(top_controls_height, bottom_controls_height,
                                 browser_controls_shrink_layout);

  {
    // Avoids unnecessary invalidations while various bits of state in
    // TextAutosizer are updated.
    TextAutosizer::DeferUpdatePageInfo defer_update_page_info(GetPage());
    LocalFrameView* frame_view = MainFrameImpl()->GetFrameView();
    IntSize old_size = frame_view->Size();
    UpdateICBAndResizeViewport();
    IntSize new_size = frame_view->Size();
    frame_view->MarkViewportConstrainedObjectsForLayout(
        old_size.Width() != new_size.Width(),
        old_size.Height() != new_size.Height());
  }

  fullscreen_controller_->UpdateSize();

  // Update lifecyle phases immediately to recalculate the minimum scale limit
  // for rotation anchoring, and to make sure that no lifecycle states are
  // stale if this WebView is embedded in another one.
  UpdateAllLifecyclePhases();
}

void WebViewImpl::ResizeWithBrowserControls(
    const WebSize& new_size,
    float top_controls_height,
    float bottom_controls_height,
    bool browser_controls_shrink_layout) {
  if (should_auto_resize_)
    return;

  if (size_ == new_size &&
      GetBrowserControls().TopHeight() == top_controls_height &&
      GetBrowserControls().BottomHeight() == bottom_controls_height &&
      GetBrowserControls().ShrinkViewport() == browser_controls_shrink_layout)
    return;

  if (GetPage()->MainFrame() && !GetPage()->MainFrame()->IsLocalFrame()) {
    // Viewport resize for a remote main frame does not require any
    // particular action, but the state needs to reflect the correct size
    // so that it can be used for initalization if the main frame gets
    // swapped to a LocalFrame at a later time.
    size_ = new_size;
    GetPageScaleConstraintsSet().DidChangeInitialContainingBlockSize(size_);
    GetPage()->GetVisualViewport().SetSize(size_);
    GetPage()->GetBrowserControls().SetHeight(top_controls_height,
                                              bottom_controls_height,
                                              browser_controls_shrink_layout);
    return;
  }

  WebLocalFrameImpl* main_frame = MainFrameImpl();
  if (!main_frame)
    return;

  LocalFrameView* view = main_frame->GetFrameView();
  if (!view)
    return;

  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();

  bool is_rotation =
      GetPage()->GetSettings().GetMainFrameResizesAreOrientationChanges() &&
      size_.width && ContentsSize().Width() && new_size.width != size_.width &&
      !fullscreen_controller_->IsFullscreenOrTransitioning();
  size_ = new_size;

  FloatSize viewport_anchor_coords(viewportAnchorCoordX, viewportAnchorCoordY);
  if (is_rotation) {
    RotationViewportAnchor anchor(*view, visual_viewport,
                                  viewport_anchor_coords,
                                  GetPageScaleConstraintsSet());
    ResizeViewWhileAnchored(top_controls_height, bottom_controls_height,
                            browser_controls_shrink_layout);
  } else {
    ResizeViewportAnchor::ResizeScope resize_scope(*resize_viewport_anchor_);
    ResizeViewWhileAnchored(top_controls_height, bottom_controls_height,
                            browser_controls_shrink_layout);
  }
  SendResizeEventAndRepaint();
}

void WebViewImpl::Resize(const WebSize& new_size) {
  if (should_auto_resize_ || size_ == new_size)
    return;

  ResizeWithBrowserControls(new_size, GetBrowserControls().TopHeight(),
                            GetBrowserControls().BottomHeight(),
                            GetBrowserControls().ShrinkViewport());
}

void WebViewImpl::DidEnterFullscreen() {
  fullscreen_controller_->DidEnterFullscreen();
}

void WebViewImpl::DidExitFullscreen() {
  fullscreen_controller_->DidExitFullscreen();
}

void WebViewImpl::DidUpdateFullscreenSize() {
  fullscreen_controller_->UpdateSize();
}

void WebViewImpl::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  page_->Animator().SetSuppressFrameRequestsWorkaroundFor704763Only(
      suppress_frame_requests);
}
void WebViewImpl::BeginFrame(base::TimeTicks last_frame_time) {
  TRACE_EVENT1("blink", "WebViewImpl::beginFrame", "frameTime",
               last_frame_time);
  DCHECK(!last_frame_time.is_null());

  if (!MainFrameImpl())
    return;

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      MainFrameImpl()->GetFrame()->GetDocument()->Lifecycle());
  PageWidgetDelegate::Animate(*page_, last_frame_time);
}

void WebViewImpl::RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) {
  if (!MainFrameImpl())
    return;

  MainFrameImpl()->GetFrame()->View()->RecordEndOfFrameMetrics(
      frame_begin_time);
}

void WebViewImpl::UpdateLifecycle(LifecycleUpdate requested_update) {
  TRACE_EVENT0("blink", "WebViewImpl::updateAllLifecyclePhases");
  if (!MainFrameImpl())
    return;

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      MainFrameImpl()->GetFrame()->GetDocument()->Lifecycle());

  PageWidgetDelegate::UpdateLifecycle(*page_, *MainFrameImpl()->GetFrame(),
                                      requested_update);
  if (requested_update == LifecycleUpdate::kLayout)
    return;

  UpdateLayerTreeBackgroundColor();

  if (requested_update == LifecycleUpdate::kPrePaint)
    return;

  if (LocalFrameView* view = MainFrameImpl()->GetFrameView()) {
    LocalFrame* frame = MainFrameImpl()->GetFrame();
    WebWidgetClient* client =
        WebLocalFrameImpl::FromFrame(frame)->FrameWidgetImpl()->Client();

    if (should_dispatch_first_visually_non_empty_layout_ &&
        view->IsVisuallyNonEmpty()) {
      should_dispatch_first_visually_non_empty_layout_ = false;
      // TODO(esprehn): Move users of this callback to something
      // better, the heuristic for "visually non-empty" is bad.
      client->DidMeaningfulLayout(WebMeaningfulLayout::kVisuallyNonEmpty);
    }

    if (should_dispatch_first_layout_after_finished_parsing_ &&
        frame->GetDocument()->HasFinishedParsing()) {
      should_dispatch_first_layout_after_finished_parsing_ = false;
      client->DidMeaningfulLayout(WebMeaningfulLayout::kFinishedParsing);
    }

    if (should_dispatch_first_layout_after_finished_loading_ &&
        frame->GetDocument()->IsLoadCompleted()) {
      should_dispatch_first_layout_after_finished_loading_ = false;
      client->DidMeaningfulLayout(WebMeaningfulLayout::kFinishedLoading);
    }
  }
}

void WebViewImpl::UpdateAllLifecyclePhasesAndCompositeForTesting(
    bool do_raster) {
  if (layer_tree_view_) {
    layer_tree_view_->UpdateAllLifecyclePhasesAndCompositeForTesting(do_raster);
  }
}

void WebViewImpl::PaintContent(cc::PaintCanvas* canvas, const WebRect& rect) {
  // This should only be used when compositing is not being used for this
  // WebView, and it is painting into the recording of its parent.
  DCHECK(!IsAcceleratedCompositingActive());
  PageWidgetDelegate::PaintContent(*page_, canvas, rect,
                                   *page_->DeprecatedLocalMainFrame());
}

void WebViewImpl::PaintContentIgnoringCompositing(cc::PaintCanvas* canvas,
                                                  const WebRect& rect) {
  // This is called on a composited WebViewImpl, but we will ignore it,
  // producing all possible content of the WebViewImpl into the PaintCanvas.
  DCHECK(IsAcceleratedCompositingActive());
  PageWidgetDelegate::PaintContentIgnoringCompositing(
      *page_, canvas, rect, *page_->DeprecatedLocalMainFrame());
}

void WebViewImpl::LayoutAndPaintAsync(base::OnceClosure callback) {
  if (layer_tree_view_)
    layer_tree_view_->LayoutAndPaintAsync(std::move(callback));
}

void WebViewImpl::CompositeAndReadbackAsync(
    base::OnceCallback<void(const SkBitmap&)> callback) {
  if (layer_tree_view_)
    layer_tree_view_->CompositeAndReadbackAsync(std::move(callback));
}

void WebViewImpl::ThemeChanged() {
  if (!GetPage())
    return;
  if (!GetPage()->MainFrame()->IsLocalFrame())
    return;
  LocalFrameView* view = GetPage()->DeprecatedLocalMainFrame()->View();

  WebRect damaged_rect(0, 0, size_.width, size_.height);
  view->InvalidateRect(damaged_rect);
}

void WebViewImpl::EnterFullscreen(LocalFrame& frame,
                                  const FullscreenOptions& options) {
  fullscreen_controller_->EnterFullscreen(frame, options);
}

void WebViewImpl::ExitFullscreen(LocalFrame& frame) {
  fullscreen_controller_->ExitFullscreen(frame);
}

void WebViewImpl::FullscreenElementChanged(Element* old_element,
                                           Element* new_element) {
  fullscreen_controller_->FullscreenElementChanged(old_element, new_element);
}

bool WebViewImpl::HasHorizontalScrollbar() {
  return MainFrameImpl()
      ->GetFrameView()
      ->LayoutViewport()
      ->HorizontalScrollbar();
}

bool WebViewImpl::HasVerticalScrollbar() {
  return MainFrameImpl()->GetFrameView()->LayoutViewport()->VerticalScrollbar();
}

WebInputEventResult WebViewImpl::DispatchBufferedTouchEvents() {
  if (!MainFrameImpl())
    return WebInputEventResult::kNotHandled;
  if (WebDevToolsAgentImpl* devtools = MainFrameDevToolsAgentImpl())
    devtools->DispatchBufferedTouchEvents();
  return MainFrameImpl()
      ->GetFrame()
      ->GetEventHandler()
      .DispatchBufferedTouchEvents();
}

WebInputEventResult WebViewImpl::HandleInputEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  const WebInputEvent& input_event = coalesced_event.Event();
  // TODO(dcheng): The fact that this is getting called when there is no local
  // main frame is problematic and probably indicates a bug in the input event
  // routing code.
  if (!MainFrameImpl())
    return WebInputEventResult::kNotHandled;
  DCHECK(!WebInputEvent::IsTouchEventType(input_event.GetType()));

  GetPage()->GetVisualViewport().StartTrackingPinchStats();

  TRACE_EVENT1("input,rail", "WebViewImpl::handleInputEvent", "type",
               WebInputEvent::GetName(input_event.GetType()));

  // If a drag-and-drop operation is in progress, ignore input events except
  // PointerCancel.
  if (MainFrameImpl()->FrameWidgetImpl()->DoingDragAndDrop() &&
      input_event.GetType() != WebInputEvent::kPointerCancel)
    return WebInputEventResult::kHandledSuppressed;

  if (WebDevToolsAgentImpl* devtools = MainFrameDevToolsAgentImpl()) {
    if (devtools->HandleInputEvent(input_event))
      return WebInputEventResult::kHandledSuppressed;
  }

  // Report the event to be NOT processed by WebKit, so that the browser can
  // handle it appropriately.
  if (WebFrameWidgetBase::IgnoreInputEvents())
    return WebInputEventResult::kNotHandled;

  base::AutoReset<const WebInputEvent*> current_event_change(
      &CurrentInputEvent::current_input_event_, &input_event);
  UIEventWithKeyState::ClearNewTabModifierSetFromIsolatedWorld();

  bool is_pointer_locked = false;
  if (WebFrameWidgetBase* widget = MainFrameImpl()->FrameWidgetImpl()) {
    if (WebWidgetClient* client = widget->Client())
      is_pointer_locked = client->IsPointerLocked();
  }

  if (is_pointer_locked &&
      WebInputEvent::IsMouseEventType(input_event.GetType())) {
    MainFrameImpl()->FrameWidgetImpl()->PointerLockMouseEvent(coalesced_event);
    return WebInputEventResult::kHandledSystem;
  }

  Document& main_frame_document = *MainFrameImpl()->GetFrame()->GetDocument();

  if (input_event.GetType() != WebInputEvent::kMouseMove) {
    FirstMeaningfulPaintDetector::From(main_frame_document).NotifyInputEvent();
  }

  if (input_event.GetType() != WebInputEvent::kMouseMove &&
      input_event.GetType() != WebInputEvent::kMouseEnter &&
      input_event.GetType() != WebInputEvent::kMouseLeave) {
    InteractiveDetector* interactive_detector(
        InteractiveDetector::From(main_frame_document));
    if (interactive_detector) {
      interactive_detector->OnInvalidatingInputEvent(input_event.TimeStamp());
    }
  }

  // Skip the pointerrawmove for mouse capture case.
  if (mouse_capture_node_ &&
      input_event.GetType() == WebInputEvent::kPointerRawMove)
    return WebInputEventResult::kHandledSystem;

  if (mouse_capture_node_ &&
      WebInputEvent::IsMouseEventType(input_event.GetType()))
    return HandleCapturedMouseEvent(coalesced_event);

  // FIXME: This should take in the intended frame, not the local frame
  // root.
  return PageWidgetDelegate::HandleInputEvent(*this, coalesced_event,
                                              MainFrameImpl()->GetFrame());
}

WebInputEventResult WebViewImpl::HandleCapturedMouseEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  const WebInputEvent& input_event = coalesced_event.Event();
  TRACE_EVENT1("input", "captured mouse event", "type", input_event.GetType());
  // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
  Node* node = mouse_capture_node_;

  // Not all platforms call mouseCaptureLost() directly.
  if (input_event.GetType() == WebInputEvent::kMouseUp)
    MouseCaptureLost();

  std::unique_ptr<UserGestureIndicator> gesture_indicator;

  AtomicString event_type;
  switch (input_event.GetType()) {
    case WebInputEvent::kMouseEnter:
      event_type = EventTypeNames::mouseover;
      break;
    case WebInputEvent::kMouseMove:
      event_type = EventTypeNames::mousemove;
      break;
    case WebInputEvent::kPointerRawMove:
      // There will be no mouse event for raw move events.
      event_type = EventTypeNames::pointerrawmove;
      break;
    case WebInputEvent::kMouseLeave:
      event_type = EventTypeNames::mouseout;
      break;
    case WebInputEvent::kMouseDown:
      event_type = EventTypeNames::mousedown;
      gesture_indicator = LocalFrame::NotifyUserActivation(
          node->GetDocument().GetFrame(), UserGestureToken::kNewGesture);
      mouse_capture_gesture_token_ = gesture_indicator->CurrentToken();
      break;
    case WebInputEvent::kMouseUp:
      event_type = EventTypeNames::mouseup;
      gesture_indicator = std::make_unique<UserGestureIndicator>(
          std::move(mouse_capture_gesture_token_));
      break;
    default:
      NOTREACHED();
  }

  WebMouseEvent transformed_event =
      TransformWebMouseEvent(MainFrameImpl()->GetFrameView(),
                             static_cast<const WebMouseEvent&>(input_event));
  if (LocalFrame* frame = node->GetDocument().GetFrame()) {
    frame->GetEventHandler().HandleTargetedMouseEvent(
        node, transformed_event, event_type,
        TransformWebMouseEventVector(
            MainFrameImpl()->GetFrameView(),
            coalesced_event.GetCoalescedEventsPointers()),
        TransformWebMouseEventVector(
            MainFrameImpl()->GetFrameView(),
            coalesced_event.GetPredictedEventsPointers()));
  }
  return WebInputEventResult::kHandledSystem;
}

void WebViewImpl::SetCursorVisibilityState(bool is_visible) {
  if (page_)
    page_->SetIsCursorVisible(is_visible);
}

void WebViewImpl::MouseCaptureLost() {
  TRACE_EVENT_ASYNC_END0("input", "capturing mouse", this);
  mouse_capture_node_ = nullptr;
  if (page_->DeprecatedLocalMainFrame())
    page_->DeprecatedLocalMainFrame()->Client()->SetMouseCapture(false);
}

void WebViewImpl::SetFocus(bool enable) {
  if (enable)
    page_->GetFocusController().SetActive(true);
  page_->GetFocusController().SetFocused(enable);
  if (enable) {
    LocalFrame* focused_frame = page_->GetFocusController().FocusedFrame();
    if (focused_frame) {
      Element* element = focused_frame->GetDocument()->FocusedElement();
      if (element && focused_frame->Selection()
                         .ComputeVisibleSelectionInDOMTreeDeprecated()
                         .IsNone()) {
        // If the selection was cleared while the WebView was not
        // focused, then the focus element shows with a focus ring but
        // no caret and does respond to keyboard inputs.
        focused_frame->GetDocument()->UpdateStyleAndLayoutTree();
        if (element->IsTextControl()) {
          element->UpdateFocusAppearance(SelectionBehaviorOnFocus::kRestore);
        } else if (HasEditableStyle(*element)) {
          // updateFocusAppearance() selects all the text of
          // contentseditable DIVs. So we set the selection explicitly
          // instead. Note that this has the side effect of moving the
          // caret back to the beginning of the text.
          Position position(element, 0);
          focused_frame->Selection().SetSelectionAndEndTyping(
              SelectionInDOMTree::Builder().Collapse(position).Build());
        }
      }
    }
    ime_accept_events_ = true;
  } else {
    HidePopups();

    // Clear focus on the currently focused frame if any.
    if (!page_)
      return;

    LocalFrame* frame = page_->MainFrame() && page_->MainFrame()->IsLocalFrame()
                            ? page_->DeprecatedLocalMainFrame()
                            : nullptr;
    if (!frame)
      return;

    LocalFrame* focused_frame = FocusedLocalFrameInWidget();
    if (focused_frame) {
      // Finish an ongoing composition to delete the composition node.
      if (focused_frame->GetInputMethodController().HasComposition()) {
        // TODO(editing-dev): The use of
        // updateStyleAndLayoutIgnorePendingStylesheets needs to be audited.
        // See http://crbug.com/590369 for more details.
        focused_frame->GetDocument()
            ->UpdateStyleAndLayoutIgnorePendingStylesheets();

        focused_frame->GetInputMethodController().FinishComposingText(
            InputMethodController::kKeepSelection);
      }
      ime_accept_events_ = false;
    }
  }
}

bool WebViewImpl::SelectionBounds(WebRect& anchor_web,
                                  WebRect& focus_web) const {
  const Frame* frame = FocusedCoreFrame();
  if (!frame || !frame->IsLocalFrame())
    return false;
  const LocalFrame* local_frame = ToLocalFrame(frame);
  if (!local_frame)
    return false;

  LocalFrameView* frame_view = local_frame->View();
  if (!frame_view)
    return false;

  IntRect anchor;
  IntRect focus;
  if (!local_frame->Selection().ComputeAbsoluteBounds(anchor, focus))
    return false;

  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();
  anchor_web = visual_viewport.RootFrameToViewport(
      frame_view->ConvertToRootFrame(anchor));
  focus_web = visual_viewport.RootFrameToViewport(
      frame_view->ConvertToRootFrame(focus));
  return true;
}

SkColor WebViewImpl::BackgroundColor() const {
  if (background_color_override_enabled_)
    return background_color_override_;
  if (!page_)
    return BaseBackgroundColor().Rgb();
  if (!page_->MainFrame())
    return BaseBackgroundColor().Rgb();
  if (!page_->MainFrame()->IsLocalFrame())
    return BaseBackgroundColor().Rgb();
  LocalFrameView* view = page_->DeprecatedLocalMainFrame()->View();
  if (!view)
    return BaseBackgroundColor().Rgb();
  return view->DocumentBackgroundColor().Rgb();
}

WebPagePopupImpl* WebViewImpl::GetPagePopup() const {
  return page_popup_.get();
}

bool WebViewImpl::IsAcceleratedCompositingActive() const {
  return !!root_layer_;
}

void WebViewImpl::WillCloseLayerTreeView() {
  if (layer_tree_view_)
    GetPage()->WillCloseLayerTreeView(*layer_tree_view_, nullptr);

  SetRootLayer(nullptr);
  animation_host_ = nullptr;

  mutator_dispatcher_ = nullptr;
  layer_tree_view_ = nullptr;
}

void WebViewImpl::DidAcquirePointerLock() {
  if (MainFrameImpl())
    MainFrameImpl()->FrameWidget()->DidAcquirePointerLock();
}

void WebViewImpl::DidNotAcquirePointerLock() {
  if (MainFrameImpl())
    MainFrameImpl()->FrameWidget()->DidNotAcquirePointerLock();
}

void WebViewImpl::DidLosePointerLock() {
  // Make sure that the main frame wasn't swapped-out when the pointer lock is
  // lost. There's a race that can happen when a pointer lock is requested, but
  // the browser swaps out the main frame while the pointer lock request is in
  // progress. This won't be needed once the main frame is refactored to not use
  // the WebViewImpl as its WebWidget.
  if (MainFrameImpl())
    MainFrameImpl()->FrameWidget()->DidLosePointerLock();
}

// WebView --------------------------------------------------------------------

WebSettingsImpl* WebViewImpl::SettingsImpl() {
  if (!web_settings_) {
    web_settings_ = std::make_unique<WebSettingsImpl>(
        &page_->GetSettings(), dev_tools_emulator_.Get());
  }
  DCHECK(web_settings_);
  return web_settings_.get();
}

WebSettings* WebViewImpl::GetSettings() {
  return SettingsImpl();
}

WebString WebViewImpl::PageEncoding() const {
  if (!page_)
    return WebString();

  if (!page_->MainFrame()->IsLocalFrame())
    return WebString();

  // FIXME: Is this check needed?
  if (!page_->DeprecatedLocalMainFrame()->GetDocument()->Loader())
    return WebString();

  return page_->DeprecatedLocalMainFrame()->GetDocument()->EncodingName();
}

WebFrame* WebViewImpl::MainFrame() {
  return WebFrame::FromFrame(page_ ? page_->MainFrame() : nullptr);
}

WebLocalFrame* WebViewImpl::FocusedFrame() {
  Frame* frame = FocusedCoreFrame();
  // TODO(yabinh): focusedCoreFrame() should always return a local frame, and
  // the following check should be unnecessary.
  // See crbug.com/625068
  if (!frame || !frame->IsLocalFrame())
    return nullptr;
  return WebLocalFrameImpl::FromFrame(ToLocalFrame(frame));
}

void WebViewImpl::SetFocusedFrame(WebFrame* frame) {
  if (!frame) {
    // Clears the focused frame if any.
    Frame* focused_frame = FocusedCoreFrame();
    if (focused_frame && focused_frame->IsLocalFrame())
      ToLocalFrame(focused_frame)->Selection().SetFrameIsFocused(false);
    return;
  }
  LocalFrame* core_frame = ToWebLocalFrameImpl(frame)->GetFrame();
  core_frame->GetPage()->GetFocusController().SetFocusedFrame(core_frame);
}

void WebViewImpl::FocusDocumentView(WebFrame* frame) {
  // This is currently only used when replicating focus changes for
  // cross-process frames, and |notifyEmbedder| is disabled to avoid sending
  // duplicate frameFocused updates from FocusController to the browser
  // process, which already knows the latest focused frame.
  GetPage()->GetFocusController().FocusDocumentView(
      WebFrame::ToCoreFrame(*frame), false /* notifyEmbedder */);
}

void WebViewImpl::SetInitialFocus(bool reverse) {
  if (!page_)
    return;
  Frame* frame = GetPage()->GetFocusController().FocusedOrMainFrame();
  if (frame->IsLocalFrame()) {
    if (Document* document = ToLocalFrame(frame)->GetDocument())
      document->ClearFocusedElement();
  }
  GetPage()->GetFocusController().SetInitialFocus(
      reverse ? kWebFocusTypeBackward : kWebFocusTypeForward);
}

void WebViewImpl::ClearFocusedElement() {
  Frame* frame = FocusedCoreFrame();
  if (!frame || !frame->IsLocalFrame())
    return;

  LocalFrame* local_frame = ToLocalFrame(frame);

  Document* document = local_frame->GetDocument();
  if (!document)
    return;

  Element* old_focused_element = document->FocusedElement();
  document->ClearFocusedElement();
  if (!old_focused_element)
    return;

  // If a text field has focus, we need to make sure the selection controller
  // knows to remove selection from it. Otherwise, the text field is still
  // processing keyboard events even though focus has been moved to the page and
  // keystrokes get eaten as a result.
  document->UpdateStyleAndLayoutTree();
  if (HasEditableStyle(*old_focused_element) ||
      old_focused_element->IsTextControl())
    local_frame->Selection().Clear();
}

// TODO(dglazkov): Remove and replace with Node:hasEditableStyle.
// http://crbug.com/612560
static bool IsElementEditable(const Element* element) {
  element->GetDocument().UpdateStyleAndLayoutTree();
  if (HasEditableStyle(*element))
    return true;

  if (auto* text_control = ToTextControlOrNull(element)) {
    if (!text_control->IsDisabledOrReadOnly())
      return true;
  }

  return EqualIgnoringASCIICase(element->getAttribute(HTMLNames::roleAttr),
                                "textbox");
}

bool WebViewImpl::ScrollFocusedEditableElementIntoView() {
  DCHECK(MainFrameImpl());
  LocalFrameView* main_frame_view = MainFrameImpl()->GetFrame()->View();
  if (!main_frame_view)
    return false;

  Element* element = FocusedElement();
  if (!element || !IsElementEditable(element))
    return false;

  element->GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object)
    return false;

  // Since the page has been resized, the layout may have changed. The page
  // scale animation started by ZoomAndScrollToFocusedEditableRect will scroll
  // only the visual and layout viewports. We'll call ScrollRectToVisible with
  // the stop_at_main_frame_layout_viewport param to ensure the element is
  // actually visible in the page.
  WebScrollIntoViewParams params(ScrollAlignment::kAlignCenterIfNeeded,
                                 ScrollAlignment::kAlignCenterIfNeeded,
                                 kProgrammaticScroll, false,
                                 kScrollBehaviorInstant);
  params.stop_at_main_frame_layout_viewport = true;
  layout_object->ScrollRectToVisible(
      LayoutRect(layout_object->AbsoluteBoundingBoxRect()), params);

  ZoomAndScrollToFocusedEditableElementRect(
      main_frame_view->RootFrameToDocument(
          element->GetDocument().View()->ConvertToRootFrame(
              layout_object->AbsoluteBoundingBoxRect())),
      main_frame_view->RootFrameToDocument(
          element->GetDocument().View()->ConvertToRootFrame(
              element->GetDocument()
                  .GetFrame()
                  ->Selection()
                  .AbsoluteCaretBounds())),
      ShouldZoomToLegibleScale(*element));

  return true;
}

bool WebViewImpl::ShouldZoomToLegibleScale(const Element& element) {
  bool zoom_into_legible_scale =
      web_settings_->AutoZoomFocusedNodeToLegibleScale() &&
      !GetPage()->GetVisualViewport().ShouldDisableDesktopWorkarounds();

  if (zoom_into_legible_scale) {
    // When deciding whether to zoom in on a focused text box, we should
    // decide not to zoom in if the user won't be able to zoom out. e.g if the
    // textbox is within a touch-action: none container the user can't zoom
    // back out.
    TouchAction action =
        touch_action_util::ComputeEffectiveTouchAction(element);
    if (!(action & TouchAction::kTouchActionPinchZoom))
      zoom_into_legible_scale = false;
  }

  return zoom_into_legible_scale;
}

void WebViewImpl::ZoomAndScrollToFocusedEditableElementRect(
    const IntRect& element_bounds_in_document,
    const IntRect& caret_bounds_in_document,
    bool zoom_into_legible_scale) {
  float scale;
  IntPoint scroll;
  bool need_animation = false;
  ComputeScaleAndScrollForEditableElementRects(
      element_bounds_in_document, caret_bounds_in_document,
      zoom_into_legible_scale, scale, scroll, need_animation);
  if (need_animation) {
    StartPageScaleAnimation(scroll, false, scale,
                            scrollAndScaleAnimationDurationInSeconds);
  }
}

void WebViewImpl::SmoothScroll(int target_x, int target_y, long duration_ms) {
  IntPoint target_position(target_x, target_y);
  StartPageScaleAnimation(target_position, false, PageScaleFactor(),
                          (double)duration_ms / 1000);
}

void WebViewImpl::ComputeScaleAndScrollForEditableElementRects(
    const IntRect& element_bounds_in_document,
    const IntRect& caret_bounds_in_document,
    bool zoom_into_legible_scale,
    float& new_scale,
    IntPoint& new_scroll,
    bool& need_animation) {
  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();

  if (!zoom_into_legible_scale) {
    new_scale = PageScaleFactor();
  } else {
    // Pick a scale which is reasonably readable. This is the scale at which
    // the caret height will become minReadableCaretHeightForNode (adjusted
    // for dpi and font scale factor).
    const int min_readable_caret_height_for_node =
        (element_bounds_in_document.Height() >=
                 2 * caret_bounds_in_document.Height()
             ? minReadableCaretHeightForTextArea
             : minReadableCaretHeight) *
        MainFrameImpl()->GetFrame()->PageZoomFactor();
    new_scale = ClampPageScaleFactorToLimits(
        MaximumLegiblePageScale() * min_readable_caret_height_for_node /
        caret_bounds_in_document.Height());
    new_scale = std::max(new_scale, PageScaleFactor());
  }
  const float delta_scale = new_scale / PageScaleFactor();

  need_animation = false;

  // If we are at less than the target zoom level, zoom in.
  if (delta_scale > minScaleChangeToTriggerZoom)
    need_animation = true;
  else
    new_scale = PageScaleFactor();

  // If the caret is offscreen, then animate.
  if (!visual_viewport.VisibleRectInDocument().Contains(
          caret_bounds_in_document))
    need_animation = true;

  // If the box is partially offscreen and it's possible to bring it fully
  // onscreen, then animate.
  if (visual_viewport.VisibleRect().Width() >=
          element_bounds_in_document.Width() &&
      visual_viewport.VisibleRect().Height() >=
          element_bounds_in_document.Height() &&
      !visual_viewport.VisibleRectInDocument().Contains(
          element_bounds_in_document))
    need_animation = true;

  if (!need_animation)
    return;

  FloatSize target_viewport_size(visual_viewport.Size());
  target_viewport_size.Scale(1 / new_scale);

  if (element_bounds_in_document.Width() <= target_viewport_size.Width()) {
    // Field is narrower than screen. Try to leave padding on left so field's
    // label is visible, but it's more important to ensure entire field is
    // onscreen.
    int ideal_left_padding = target_viewport_size.Width() * leftBoxRatio;
    int max_left_padding_keeping_box_onscreen =
        target_viewport_size.Width() - element_bounds_in_document.Width();
    new_scroll.SetX(element_bounds_in_document.X() -
                    std::min<int>(ideal_left_padding,
                                  max_left_padding_keeping_box_onscreen));
  } else {
    // Field is wider than screen. Try to left-align field, unless caret would
    // be offscreen, in which case right-align the caret.
    new_scroll.SetX(std::max<int>(
        element_bounds_in_document.X(),
        caret_bounds_in_document.X() + caret_bounds_in_document.Width() +
            caretPadding - target_viewport_size.Width()));
  }
  if (element_bounds_in_document.Height() <= target_viewport_size.Height()) {
    // Field is shorter than screen. Vertically center it.
    new_scroll.SetY(
        element_bounds_in_document.Y() -
        (target_viewport_size.Height() - element_bounds_in_document.Height()) /
            2);
  } else {
    // Field is taller than screen. Try to top align field, unless caret would
    // be offscreen, in which case bottom-align the caret.
    new_scroll.SetY(std::max<int>(
        element_bounds_in_document.Y(),
        caret_bounds_in_document.Y() + caret_bounds_in_document.Height() +
            caretPadding - target_viewport_size.Height()));
  }
}

void WebViewImpl::AdvanceFocus(bool reverse) {
  GetPage()->GetFocusController().AdvanceFocus(reverse ? kWebFocusTypeBackward
                                                       : kWebFocusTypeForward);
}

void WebViewImpl::AdvanceFocusAcrossFrames(WebFocusType type,
                                           WebRemoteFrame* from,
                                           WebLocalFrame* to) {
  // TODO(alexmos): Pass in proper with sourceCapabilities.
  GetPage()->GetFocusController().AdvanceFocusAcrossFrames(
      type, ToWebRemoteFrameImpl(from)->GetFrame(),
      ToWebLocalFrameImpl(to)->GetFrame());
}

double WebViewImpl::ZoomLevel() {
  return zoom_level_;
}

void WebViewImpl::PropagateZoomFactorToLocalFrameRoots(Frame* frame,
                                                       float zoom_factor) {
  if (frame->IsLocalFrame() && ToLocalFrame(frame)->IsLocalRoot()) {
    LocalFrame* local_frame = ToLocalFrame(frame);
    if (Document* document = local_frame->GetDocument()) {
      if (!document->IsPluginDocument() ||
          !ToPluginDocument(document)->GetPluginView()) {
        local_frame->SetPageZoomFactor(zoom_factor);
      }
    }
  }

  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().NextSibling())
    PropagateZoomFactorToLocalFrameRoots(child, zoom_factor);
}

double WebViewImpl::SetZoomLevel(double zoom_level) {
  if (zoom_level < minimum_zoom_level_)
    zoom_level_ = minimum_zoom_level_;
  else if (zoom_level > maximum_zoom_level_)
    zoom_level_ = maximum_zoom_level_;
  else
    zoom_level_ = zoom_level;

  float zoom_factor =
      zoom_factor_override_
          ? zoom_factor_override_
          : static_cast<float>(ZoomLevelToZoomFactor(zoom_level_));
  if (zoom_factor_for_device_scale_factor_) {
    if (compositor_device_scale_factor_override_) {
      // Adjust the page's DSF so that DevicePixelRatio becomes
      // m_zoomFactorForDeviceScaleFactor.
      GetPage()->SetDeviceScaleFactorDeprecated(
          zoom_factor_for_device_scale_factor_ /
          compositor_device_scale_factor_override_);
      zoom_factor *= compositor_device_scale_factor_override_;
    } else {
      GetPage()->SetDeviceScaleFactorDeprecated(1.f);
      zoom_factor *= zoom_factor_for_device_scale_factor_;
    }
  }
  PropagateZoomFactorToLocalFrameRoots(page_->MainFrame(), zoom_factor);

  return zoom_level_;
}

void WebViewImpl::ZoomLimitsChanged(double minimum_zoom_level,
                                    double maximum_zoom_level) {
  minimum_zoom_level_ = minimum_zoom_level;
  maximum_zoom_level_ = maximum_zoom_level;
  client_->ZoomLimitsChanged(minimum_zoom_level_, maximum_zoom_level_);
}

float WebViewImpl::TextZoomFactor() {
  return MainFrameImpl()->GetFrame()->TextZoomFactor();
}

float WebViewImpl::SetTextZoomFactor(float text_zoom_factor) {
  LocalFrame* frame = MainFrameImpl()->GetFrame();
  if (frame->GetWebPluginContainer())
    return 1;

  frame->SetTextZoomFactor(text_zoom_factor);

  return text_zoom_factor;
}

double WebView::ZoomLevelToZoomFactor(double zoom_level) {
  return pow(kTextSizeMultiplierRatio, zoom_level);
}

double WebView::ZoomFactorToZoomLevel(double factor) {
  // Since factor = 1.2^level, level = log(factor) / log(1.2)
  return log(factor) / log(kTextSizeMultiplierRatio);
}

float WebViewImpl::PageScaleFactor() const {
  if (!GetPage())
    return 1;

  return GetPage()->GetVisualViewport().Scale();
}

float WebViewImpl::ClampPageScaleFactorToLimits(float scale_factor) const {
  return GetPageScaleConstraintsSet().FinalConstraints().ClampToConstraints(
      scale_factor);
}

void WebViewImpl::SetVisualViewportOffset(const WebFloatPoint& offset) {
  DCHECK(GetPage());
  GetPage()->GetVisualViewport().SetLocation(offset);
}

WebFloatPoint WebViewImpl::VisualViewportOffset() const {
  DCHECK(GetPage());
  return GetPage()->GetVisualViewport().VisibleRect().Location();
}

WebFloatSize WebViewImpl::VisualViewportSize() const {
  DCHECK(GetPage());
  return GetPage()->GetVisualViewport().VisibleRect().Size();
}

void WebViewImpl::SetPageScaleFactorAndLocation(float scale_factor,
                                                const FloatPoint& location) {
  DCHECK(GetPage());

  GetPage()->GetVisualViewport().SetScaleAndLocation(
      ClampPageScaleFactorToLimits(scale_factor), location);
}

void WebViewImpl::SetPageScaleFactor(float scale_factor) {
  DCHECK(GetPage());

  scale_factor = ClampPageScaleFactorToLimits(scale_factor);
  if (scale_factor == PageScaleFactor())
    return;

  GetPage()->GetVisualViewport().SetScale(scale_factor);
}

void WebViewImpl::SetDeviceScaleFactor(float scale_factor) {
  if (!GetPage())
    return;

  if (GetPage()->DeviceScaleFactorDeprecated() == scale_factor)
    return;

  GetPage()->SetDeviceScaleFactorDeprecated(scale_factor);
}

void WebViewImpl::SetZoomFactorForDeviceScaleFactor(
    float zoom_factor_for_device_scale_factor) {
  // We can't early-return here if these are already equal, because we may
  // need to propagate the correct zoom factor to newly navigated frames.
  zoom_factor_for_device_scale_factor_ = zoom_factor_for_device_scale_factor;
  if (!layer_tree_view_)
    return;
  SetZoomLevel(zoom_level_);
}

void WebViewImpl::EnableAutoResizeMode(const WebSize& min_size,
                                       const WebSize& max_size) {
  should_auto_resize_ = true;
  min_auto_size_ = min_size;
  max_auto_size_ = max_size;
  ConfigureAutoResizeMode();
}

void WebViewImpl::DisableAutoResizeMode() {
  should_auto_resize_ = false;
  ConfigureAutoResizeMode();
}

void WebViewImpl::SetDefaultPageScaleLimits(float min_scale, float max_scale) {
  GetPage()->SetDefaultPageScaleLimits(min_scale, max_scale);
}

void WebViewImpl::SetInitialPageScaleOverride(
    float initial_page_scale_factor_override) {
  PageScaleConstraints constraints =
      GetPageScaleConstraintsSet().UserAgentConstraints();
  constraints.initial_scale = initial_page_scale_factor_override;

  if (constraints == GetPageScaleConstraintsSet().UserAgentConstraints())
    return;

  GetPageScaleConstraintsSet().SetNeedsReset(true);
  GetPage()->SetUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::SetMaximumLegibleScale(float maximum_legible_scale) {
  maximum_legible_scale_ = maximum_legible_scale;
}

void WebViewImpl::SetIgnoreViewportTagScaleLimits(bool ignore) {
  PageScaleConstraints constraints =
      GetPageScaleConstraintsSet().UserAgentConstraints();
  if (ignore) {
    constraints.minimum_scale =
        GetPageScaleConstraintsSet().DefaultConstraints().minimum_scale;
    constraints.maximum_scale =
        GetPageScaleConstraintsSet().DefaultConstraints().maximum_scale;
  } else {
    constraints.minimum_scale = -1;
    constraints.maximum_scale = -1;
  }
  GetPage()->SetUserAgentPageScaleConstraints(constraints);
}

IntSize WebViewImpl::MainFrameSize() {
  // The frame size should match the viewport size at minimum scale, since the
  // viewport must always be contained by the frame.
  FloatSize frame_size(size_);
  frame_size.Scale(1 / MinimumPageScaleFactor());
  return ExpandedIntSize(frame_size);
}

PageScaleConstraintsSet& WebViewImpl::GetPageScaleConstraintsSet() const {
  return GetPage()->GetPageScaleConstraintsSet();
}

void WebViewImpl::RefreshPageScaleFactor() {
  if (!MainFrame() || !GetPage() || !GetPage()->MainFrame() ||
      !GetPage()->MainFrame()->IsLocalFrame() ||
      !GetPage()->DeprecatedLocalMainFrame()->View())
    return;
  UpdatePageDefinedViewportConstraints(MainFrameImpl()
                                           ->GetFrame()
                                           ->GetDocument()
                                           ->GetViewportData()
                                           .GetViewportDescription());
  GetPageScaleConstraintsSet().ComputeFinalConstraints();

  float new_page_scale_factor = PageScaleFactor();
  if (GetPageScaleConstraintsSet().NeedsReset() &&
      GetPageScaleConstraintsSet().FinalConstraints().initial_scale != -1) {
    new_page_scale_factor =
        GetPageScaleConstraintsSet().FinalConstraints().initial_scale;
    GetPageScaleConstraintsSet().SetNeedsReset(false);
  }
  SetPageScaleFactor(new_page_scale_factor);

  UpdateLayerTreeViewport();
}

void WebViewImpl::UpdatePageDefinedViewportConstraints(
    const ViewportDescription& description) {
  if (!GetPage() || (!size_.width && !size_.height) ||
      !GetPage()->MainFrame()->IsLocalFrame())
    return;

  // When viewport is disabled (non-mobile), we always use gpu rasterization.
  // Otherwise, on platforms that do support viewport tags, we only enable it
  // when they are present. But Why? Historically this was used to gate usage of
  // gpu rasterization to a smaller set of less complex cases to avoid driver
  // bugs dealing with websites designed for desktop. The concern is that on
  // older android devices (<L according to https://crbug.com/419521#c9),
  // drivers are more likely to encounter bugs with gpu raster when encountering
  // the full possibility of desktop web content. Further, Adreno devices <=L
  // have encountered problems that look like driver bugs when enabling
  // OOP-Raster which is gpu-based. Thus likely a blacklist would be required
  // for non-viewport-specified pages in order to avoid crashes or other
  // problems on mobile devices with gpu rasterization.
  bool viewport_enabled = GetSettings()->ViewportEnabled();
  matches_heuristics_for_gpu_rasterization_ =
      viewport_enabled ? description.MatchesHeuristicsForGpuRasterization()
                       : true;
  if (layer_tree_view_) {
    layer_tree_view_->HeuristicsForGpuRasterizationUpdated(
        matches_heuristics_for_gpu_rasterization_);
  }

  if (!viewport_enabled) {
    GetPageScaleConstraintsSet().ClearPageDefinedConstraints();
    UpdateMainFrameLayoutSize();
    return;
  }

  Document* document = GetPage()->DeprecatedLocalMainFrame()->GetDocument();

  Length default_min_width =
      document->GetViewportData().ViewportDefaultMinWidth();
  if (default_min_width.IsAuto())
    default_min_width = Length(kExtendToZoom);

  ViewportDescription adjusted_description = description;
  if (SettingsImpl()->ViewportMetaLayoutSizeQuirk() &&
      adjusted_description.type == ViewportDescription::kViewportMeta) {
    const int kLegacyWidthSnappingMagicNumber = 320;
    if (adjusted_description.max_width.IsFixed() &&
        adjusted_description.max_width.Value() <=
            kLegacyWidthSnappingMagicNumber)
      adjusted_description.max_width = Length(kDeviceWidth);
    if (adjusted_description.max_height.IsFixed() &&
        adjusted_description.max_height.Value() <= size_.height)
      adjusted_description.max_height = Length(kDeviceHeight);
    adjusted_description.min_width = adjusted_description.max_width;
    adjusted_description.min_height = adjusted_description.max_height;
  }

  float old_initial_scale =
      GetPageScaleConstraintsSet().PageDefinedConstraints().initial_scale;
  GetPageScaleConstraintsSet().UpdatePageDefinedConstraints(
      adjusted_description, default_min_width);

  if (SettingsImpl()->ClobberUserAgentInitialScaleQuirk() &&
      GetPageScaleConstraintsSet().UserAgentConstraints().initial_scale != -1 &&
      GetPageScaleConstraintsSet().UserAgentConstraints().initial_scale *
              DeviceScaleFactor() <=
          1) {
    if (description.max_width == Length(kDeviceWidth) ||
        (description.max_width.GetType() == kAuto &&
         GetPageScaleConstraintsSet().PageDefinedConstraints().initial_scale ==
             1.0f))
      SetInitialPageScaleOverride(-1);
  }

  Settings& page_settings = GetPage()->GetSettings();
  GetPageScaleConstraintsSet().AdjustForAndroidWebViewQuirks(
      adjusted_description, default_min_width.IntValue(), DeviceScaleFactor(),
      SettingsImpl()->SupportDeprecatedTargetDensityDPI(),
      page_settings.GetWideViewportQuirkEnabled(),
      page_settings.GetUseWideViewport(),
      page_settings.GetLoadWithOverviewMode(),
      SettingsImpl()->ViewportMetaNonUserScalableQuirk());
  float new_initial_scale =
      GetPageScaleConstraintsSet().PageDefinedConstraints().initial_scale;
  if (old_initial_scale != new_initial_scale && new_initial_scale != -1) {
    GetPageScaleConstraintsSet().SetNeedsReset(true);
    if (MainFrameImpl() && MainFrameImpl()->GetFrameView())
      MainFrameImpl()->GetFrameView()->SetNeedsLayout();
  }

  if (TextAutosizer* text_autosizer = document->GetTextAutosizer())
    text_autosizer->UpdatePageInfoInAllFrames();

  UpdateMainFrameLayoutSize();
}

void WebViewImpl::UpdateMainFrameLayoutSize() {
  if (should_auto_resize_ || !MainFrameImpl())
    return;

  LocalFrameView* view = MainFrameImpl()->GetFrameView();
  if (!view)
    return;

  WebSize layout_size = size_;

  if (GetSettings()->ViewportEnabled())
    layout_size = GetPageScaleConstraintsSet().GetLayoutSize();

  if (GetPage()->GetSettings().GetForceZeroLayoutHeight())
    layout_size.height = 0;

  view->SetLayoutSize(layout_size);
}

IntSize WebViewImpl::ContentsSize() const {
  if (!GetPage()->MainFrame()->IsLocalFrame())
    return IntSize();
  auto* layout_view =
      GetPage()->DeprecatedLocalMainFrame()->ContentLayoutObject();
  if (!layout_view)
    return IntSize();
  return layout_view->DocumentRect().Size();
}

WebSize WebViewImpl::ContentsPreferredMinimumSize() {
  Document* document = page_->MainFrame()->IsLocalFrame()
                           ? page_->DeprecatedLocalMainFrame()->GetDocument()
                           : nullptr;
  if (!document || !document->GetLayoutView() || !document->documentElement() ||
      !document->documentElement()->GetLayoutBox())
    return WebSize();

  // The preferred size requires an up-to-date layout tree.
  DCHECK(!document->NeedsLayoutTreeUpdate() &&
         !document->View()->NeedsLayout());

  // Needed for computing MinPreferredWidth.
  FontCachePurgePreventer fontCachePurgePreventer;
  int width_scaled = document->GetLayoutView()
                         ->MinPreferredLogicalWidth()
                         .Round();  // Already accounts for zoom.
  int height_scaled =
      document->documentElement()->GetLayoutBox()->ScrollHeight().Round();
  return IntSize(width_scaled, height_scaled);
}

float WebViewImpl::DefaultMinimumPageScaleFactor() const {
  return GetPageScaleConstraintsSet().DefaultConstraints().minimum_scale;
}

float WebViewImpl::DefaultMaximumPageScaleFactor() const {
  return GetPageScaleConstraintsSet().DefaultConstraints().maximum_scale;
}

float WebViewImpl::MinimumPageScaleFactor() const {
  return GetPageScaleConstraintsSet().FinalConstraints().minimum_scale;
}

float WebViewImpl::MaximumPageScaleFactor() const {
  return GetPageScaleConstraintsSet().FinalConstraints().maximum_scale;
}

void WebViewImpl::ResetScaleStateImmediately() {
  GetPageScaleConstraintsSet().SetNeedsReset(true);
}

void WebViewImpl::ResetScrollAndScaleState() {
  GetPage()->GetVisualViewport().Reset();

  if (!GetPage()->MainFrame()->IsLocalFrame())
    return;

  if (LocalFrameView* frame_view =
          ToLocalFrame(GetPage()->MainFrame())->View()) {
    ScrollableArea* scrollable_area = frame_view->LayoutViewport();

    if (!scrollable_area->GetScrollOffset().IsZero())
      scrollable_area->SetScrollOffset(ScrollOffset(), kProgrammaticScroll);
  }

  if (Document* document =
          ToLocalFrame(GetPage()->MainFrame())->GetDocument()) {
    if (DocumentLoader* loader = document->Loader()) {
      if (HistoryItem* item = loader->GetHistoryItem())
        item->ClearViewState();
    }
  }

  GetPageScaleConstraintsSet().SetNeedsReset(true);
}

void WebViewImpl::PerformPluginAction(const WebPluginAction& action,
                                      const WebPoint& location) {
  // FIXME: Location is probably in viewport coordinates
  HitTestResult result = HitTestResultForRootFramePos(LayoutPoint(location));
  Node* node = result.InnerNode();
  if (!IsHTMLObjectElement(*node) && !IsHTMLEmbedElement(*node))
    return;

  LayoutObject* object = node->GetLayoutObject();
  if (object && object->IsLayoutEmbeddedContent()) {
    WebPluginContainerImpl* plugin_view =
        ToLayoutEmbeddedContent(object)->Plugin();
    if (plugin_view) {
      switch (action.type) {
        case WebPluginAction::kRotate90Clockwise:
          plugin_view->Plugin()->RotateView(
              WebPlugin::kRotationType90Clockwise);
          break;
        case WebPluginAction::kRotate90Counterclockwise:
          plugin_view->Plugin()->RotateView(
              WebPlugin::kRotationType90Counterclockwise);
          break;
        default:
          NOTREACHED();
      }
    }
  }
}

void WebViewImpl::AudioStateChanged(bool is_audio_playing) {
  GetPage()->GetPageScheduler()->AudioStateChanged(is_audio_playing);
}

WebHitTestResult WebViewImpl::HitTestResultAt(const WebPoint& point) {
  return CoreHitTestResultAt(point);
}

HitTestResult WebViewImpl::CoreHitTestResultAt(
    const WebPoint& point_in_viewport) {
  // TODO(crbug.com/843128): When we do async hit-testing, we might try to do
  // hit-testing when the local main frame is not valid anymore. Look into if we
  // can avoid getting here earlier in the pipeline.
  if (!MainFrameImpl() || !MainFrameImpl()->GetFrameView())
    return HitTestResult();

  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      MainFrameImpl()->GetFrame()->GetDocument()->Lifecycle());
  LocalFrameView* view = MainFrameImpl()->GetFrameView();
  LayoutPoint point_in_root_frame =
      view->ViewportToFrame(LayoutPoint(point_in_viewport));
  return HitTestResultForRootFramePos(point_in_root_frame);
}

void WebViewImpl::SendResizeEventAndRepaint() {
  // FIXME: This is wrong. The LocalFrameView is responsible sending a
  // resizeEvent as part of layout. Layout is also responsible for sending
  // invalidations to the embedder. This method and all callers may be wrong. --
  // eseidel.
  if (MainFrameImpl()->GetFrameView()) {
    // Enqueues the resize event.
    MainFrameImpl()->GetFrame()->GetDocument()->EnqueueResizeEvent();
  }

  if (client_) {
    if (layer_tree_view_) {
      UpdateLayerTreeViewport();
    } else {
      WebRect damaged_rect(0, 0, size_.width, size_.height);
      client_->WidgetClient()->DidInvalidateRect(damaged_rect);
    }
  }
}

void WebViewImpl::ConfigureAutoResizeMode() {
  if (!MainFrameImpl() || !MainFrameImpl()->GetFrame() ||
      !MainFrameImpl()->GetFrame()->View())
    return;

  if (should_auto_resize_) {
    MainFrameImpl()->GetFrame()->View()->EnableAutoSizeMode(min_auto_size_,
                                                            max_auto_size_);
  } else {
    MainFrameImpl()->GetFrame()->View()->DisableAutoSizeMode();
  }
}

unsigned long WebViewImpl::CreateUniqueIdentifierForRequest() {
  return CreateUniqueIdentifier();
}

void WebViewImpl::SetCompositorDeviceScaleFactorOverride(
    float device_scale_factor) {
  if (compositor_device_scale_factor_override_ == device_scale_factor)
    return;
  compositor_device_scale_factor_override_ = device_scale_factor;
  if (zoom_factor_for_device_scale_factor_) {
    SetZoomLevel(ZoomLevel());
    return;
  }
}

void WebViewImpl::SetDeviceEmulationTransform(
    const TransformationMatrix& transform) {
  if (transform == device_emulation_transform_)
    return;
  device_emulation_transform_ = transform;
  UpdateDeviceEmulationTransform();
}

TransformationMatrix WebViewImpl::GetDeviceEmulationTransformForTesting()
    const {
  return device_emulation_transform_;
}

void WebViewImpl::EnableDeviceEmulation(
    const WebDeviceEmulationParams& params) {
  dev_tools_emulator_->EnableDeviceEmulation(params);
}

void WebViewImpl::DisableDeviceEmulation() {
  dev_tools_emulator_->DisableDeviceEmulation();
}

void WebViewImpl::PerformCustomContextMenuAction(unsigned action) {
  if (page_)
    page_->GetContextMenuController().CustomContextMenuItemSelected(action);
}

void WebViewImpl::ShowContextMenu(WebMenuSourceType source_type) {
  if (!MainFrameImpl())
    return;

  // If MainFrameImpl() is non-null, then FrameWidget() will also be non-null.
  DCHECK(MainFrameImpl()->FrameWidget());
  MainFrameImpl()->FrameWidget()->ShowContextMenu(source_type);
}

WebURL WebViewImpl::GetURLForDebugTrace() {
  WebFrame* main_frame = MainFrame();
  // TODO(crbug.com/896836): Avoid a crash in minimal way for merge. But we'll
  // avoid it properly in a followup.
  if (!main_frame)
    return {};
  if (main_frame->IsWebLocalFrame())
    return main_frame->ToWebLocalFrame()->GetDocument().Url();
  return {};
}

void WebViewImpl::DidCloseContextMenu() {
  LocalFrame* frame = page_->GetFocusController().FocusedFrame();
  if (frame)
    frame->Selection().SetCaretBlinkingSuspended(false);
}

void WebViewImpl::HidePopups() {
  CancelPagePopup();
}

WebInputMethodController* WebViewImpl::GetActiveWebInputMethodController()
    const {
  WebLocalFrameImpl* local_frame =
      WebLocalFrameImpl::FromFrame(FocusedLocalFrameInWidget());
  return local_frame ? local_frame->GetInputMethodController() : nullptr;
}

Color WebViewImpl::BaseBackgroundColor() const {
  return base_background_color_override_enabled_
             ? base_background_color_override_
             : base_background_color_;
}

void WebViewImpl::SetBaseBackgroundColor(SkColor color) {
  if (base_background_color_ == color)
    return;

  base_background_color_ = color;
  UpdateBaseBackgroundColor();
}

void WebViewImpl::SetBaseBackgroundColorOverride(SkColor color) {
  if (base_background_color_override_enabled_ &&
      base_background_color_override_ == color) {
    return;
  }

  base_background_color_override_enabled_ = true;
  base_background_color_override_ = color;
  if (MainFrameImpl()) {
    // Force lifecycle update to ensure we're good to call
    // LocalFrameView::setBaseBackgroundColor().
    MainFrameImpl()
        ->GetFrame()
        ->View()
        ->UpdateLifecycleToCompositingCleanPlusScrolling();
  }
  UpdateBaseBackgroundColor();
}

void WebViewImpl::ClearBaseBackgroundColorOverride() {
  if (!base_background_color_override_enabled_)
    return;

  base_background_color_override_enabled_ = false;
  if (MainFrameImpl()) {
    // Force lifecycle update to ensure we're good to call
    // LocalFrameView::setBaseBackgroundColor().
    MainFrameImpl()
        ->GetFrame()
        ->View()
        ->UpdateLifecycleToCompositingCleanPlusScrolling();
  }
  UpdateBaseBackgroundColor();
}

void WebViewImpl::UpdateBaseBackgroundColor() {
  Color color = BaseBackgroundColor();
  if (page_->MainFrame() && page_->MainFrame()->IsLocalFrame()) {
    LocalFrameView* view = page_->DeprecatedLocalMainFrame()->View();
    view->SetBaseBackgroundColor(color);
    view->UpdateBaseBackgroundColorRecursively(color);
  }
}

void WebViewImpl::SetIsActive(bool active) {
  if (GetPage())
    GetPage()->GetFocusController().SetActive(active);
}

bool WebViewImpl::IsActive() const {
  return GetPage() ? GetPage()->GetFocusController().IsActive() : false;
}

void WebViewImpl::SetDomainRelaxationForbidden(bool forbidden,
                                               const WebString& scheme) {
  SchemeRegistry::SetDomainRelaxationForbiddenForURLScheme(forbidden,
                                                           String(scheme));
}

void WebViewImpl::SetWindowFeatures(const WebWindowFeatures& features) {
  page_->SetWindowFeatures(features);
}

void WebViewImpl::SetOpenedByDOM() {
  page_->SetOpenedByDOM();
}

void WebViewImpl::SetSelectionColors(unsigned active_background_color,
                                     unsigned active_foreground_color,
                                     unsigned inactive_background_color,
                                     unsigned inactive_foreground_color) {
#if defined(WTF_USE_DEFAULT_RENDER_THEME)
  LayoutThemeDefault::SetSelectionColors(
      active_background_color, active_foreground_color,
      inactive_background_color, inactive_foreground_color);
  LayoutTheme::GetTheme().PlatformColorsDidChange();
#endif
}

void WebViewImpl::DidCommitLoad(bool is_new_navigation,
                                bool is_navigation_within_page) {
  if (!is_navigation_within_page) {
    should_dispatch_first_visually_non_empty_layout_ = true;
    should_dispatch_first_layout_after_finished_parsing_ = true;
    should_dispatch_first_layout_after_finished_loading_ = true;

    if (is_new_navigation) {
      GetPageScaleConstraintsSet().SetNeedsReset(true);
      page_importance_signals_.OnCommitLoad();
    }
  }

  // Give the visual viewport's scroll layer its initial size.
  GetPage()->GetVisualViewport().MainFrameDidChangeSize();
}

void WebViewImpl::ResizeAfterLayout() {
  DCHECK(MainFrameImpl());
  if (!client_ || !client_->CanUpdateLayout())
    return;

  if (should_auto_resize_) {
    LocalFrameView* view = MainFrameImpl()->GetFrame()->View();
    WebSize frame_size = view->Size();
    if (frame_size != size_) {
      size_ = frame_size;

      GetPage()->GetVisualViewport().SetSize(size_);
      GetPageScaleConstraintsSet().DidChangeInitialContainingBlockSize(size_);
      view->SetInitialViewportSize(size_);

      client_->DidAutoResize(size_);
      SendResizeEventAndRepaint();
    }
  }

  if (GetPageScaleConstraintsSet().ConstraintsDirty())
    RefreshPageScaleFactor();

  resize_viewport_anchor_->ResizeFrameView(MainFrameSize());
}

void WebViewImpl::MainFrameLayoutUpdated() {
  DCHECK(MainFrameImpl());
  if (!client_)
    return;

  client_->DidUpdateMainFrameLayout();
}

void WebViewImpl::DidChangeContentsSize() {
  if (!GetPage()->MainFrame()->IsLocalFrame())
    return;

  LocalFrameView* view = ToLocalFrame(GetPage()->MainFrame())->View();

  int vertical_scrollbar_width = 0;
  if (view && view->LayoutViewport()) {
    Scrollbar* vertical_scrollbar = view->LayoutViewport()->VerticalScrollbar();
    if (vertical_scrollbar && !vertical_scrollbar->IsOverlayScrollbar())
      vertical_scrollbar_width = vertical_scrollbar->Width();
  }

  GetPageScaleConstraintsSet().DidChangeContentsSize(
      ContentsSize(), vertical_scrollbar_width, PageScaleFactor());
}

void WebViewImpl::PageScaleFactorChanged() {
  GetPageScaleConstraintsSet().SetNeedsReset(false);
  UpdateLayerTreeViewport();
  client_->PageScaleFactorChanged();
  dev_tools_emulator_->MainFrameScrollOrScaleChanged();
}

void WebViewImpl::MainFrameScrollOffsetChanged() {
  dev_tools_emulator_->MainFrameScrollOrScaleChanged();
}

void WebViewImpl::SetBackgroundColorOverride(SkColor color) {
  background_color_override_enabled_ = true;
  background_color_override_ = color;
  UpdateLayerTreeBackgroundColor();
}

void WebViewImpl::ClearBackgroundColorOverride() {
  background_color_override_enabled_ = false;
  UpdateLayerTreeBackgroundColor();
}

void WebViewImpl::SetZoomFactorOverride(float zoom_factor) {
  zoom_factor_override_ = zoom_factor;
  SetZoomLevel(ZoomLevel());
}

void WebViewImpl::SetPageOverlayColor(SkColor color) {
  page_->SetPageOverlayColor(color);
}

WebPageImportanceSignals* WebViewImpl::PageImportanceSignals() {
  return &page_importance_signals_;
}

Element* WebViewImpl::FocusedElement() const {
  LocalFrame* frame = page_->GetFocusController().FocusedFrame();
  if (!frame)
    return nullptr;

  Document* document = frame->GetDocument();
  if (!document)
    return nullptr;

  return document->FocusedElement();
}

HitTestResult WebViewImpl::HitTestResultForRootFramePos(
    const LayoutPoint& pos_in_root_frame) {
  if (!page_->MainFrame()->IsLocalFrame())
    return HitTestResult();
  HitTestLocation location(
      page_->DeprecatedLocalMainFrame()->View()->ConvertFromRootFrame(
          pos_in_root_frame));
  HitTestResult result =
      page_->DeprecatedLocalMainFrame()
          ->GetEventHandler()
          .HitTestResultAtLocation(
              location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  result.SetToShadowHostIfInRestrictedShadowRoot();
  return result;
}

WebHitTestResult WebViewImpl::HitTestResultForTap(
    const WebPoint& tap_point_window_pos,
    const WebSize& tap_area) {
  if (!page_->MainFrame()->IsLocalFrame())
    return HitTestResult();

  WebGestureEvent tap_event(
      WebInputEvent::kGestureTap, WebInputEvent::kNoModifiers,
      WTF::CurrentTimeTicks(), kWebGestureDeviceTouchscreen);
  // GestureTap is only ever from a touchscreen.
  tap_event.SetPositionInWidget(FloatPoint(tap_point_window_pos));
  tap_event.data.tap.tap_count = 1;
  tap_event.data.tap.width = tap_area.width;
  tap_event.data.tap.height = tap_area.height;

  WebGestureEvent scaled_event =
      TransformWebGestureEvent(MainFrameImpl()->GetFrameView(), tap_event);

  HitTestResult result =
      page_->DeprecatedLocalMainFrame()
          ->GetEventHandler()
          .HitTestResultForGestureEvent(
              scaled_event, HitTestRequest::kReadOnly | HitTestRequest::kActive)
          .GetHitTestResult();

  result.SetToShadowHostIfInRestrictedShadowRoot();
  return result;
}

void WebViewImpl::SetTabsToLinks(bool enable) {
  tabs_to_links_ = enable;
}

bool WebViewImpl::TabsToLinks() const {
  return tabs_to_links_;
}

void WebViewImpl::RegisterViewportLayersWithCompositor() {
  DCHECK(layer_tree_view_);

  if (!GetPage()->MainFrame() || !GetPage()->MainFrame()->IsLocalFrame())
    return;

  Document* document = GetPage()->DeprecatedLocalMainFrame()->GetDocument();

  DCHECK(document);

  // Get the outer viewport scroll layers.
  GraphicsLayer* layout_viewport_container_layer =
      GetPage()->GlobalRootScrollerController().RootContainerLayer();
  cc::Layer* layout_viewport_container_cc_layer =
      layout_viewport_container_layer
          ? layout_viewport_container_layer->CcLayer()
          : nullptr;

  GraphicsLayer* layout_viewport_scroll_layer =
      GetPage()->GlobalRootScrollerController().RootScrollerLayer();
  cc::Layer* layout_viewport_scroll_cc_layer =
      layout_viewport_scroll_layer ? layout_viewport_scroll_layer->CcLayer()
                                   : nullptr;

  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();

  WebLayerTreeView::ViewportLayers viewport_layers;
  viewport_layers.overscroll_elasticity_element_id =
      visual_viewport.GetCompositorOverscrollElasticityElementId();
  viewport_layers.page_scale = visual_viewport.PageScaleLayer()->CcLayer();
  viewport_layers.inner_viewport_container =
      visual_viewport.ContainerLayer()->CcLayer();
  viewport_layers.outer_viewport_container = layout_viewport_container_cc_layer;
  viewport_layers.inner_viewport_scroll =
      visual_viewport.ScrollLayer()->CcLayer();
  viewport_layers.outer_viewport_scroll = layout_viewport_scroll_cc_layer;

  layer_tree_view_->RegisterViewportLayers(viewport_layers);
}

void WebViewImpl::SetRootGraphicsLayer(GraphicsLayer* graphics_layer) {
  if (!layer_tree_view_)
    return;

  // In SPv2, setRootLayer is used instead.
  DCHECK(!RuntimeEnabledFeatures::SlimmingPaintV2Enabled());

  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();
  visual_viewport.AttachLayerTree(graphics_layer);
  if (graphics_layer) {
    root_graphics_layer_ = visual_viewport.RootGraphicsLayer();
    visual_viewport_container_layer_ = visual_viewport.ContainerLayer();
    root_layer_ = root_graphics_layer_->CcLayer();
    UpdateDeviceEmulationTransform();
    layer_tree_view_->SetRootLayer(root_layer_);
    // We register viewport layers here since there may not be a layer
    // tree view prior to this point.
    RegisterViewportLayersWithCompositor();
  } else {
    root_graphics_layer_ = nullptr;
    visual_viewport_container_layer_ = nullptr;
    root_layer_ = nullptr;
    // This means that we're transitioning to a new page. Suppress
    // commits until Blink generates invalidations so we don't
    // attempt to paint too early in the next page load.
    scoped_defer_commits_ = layer_tree_view_->DeferCommits();
    layer_tree_view_->ClearRootLayer();
    layer_tree_view_->ClearViewportLayers();
  }
}

void WebViewImpl::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  if (!layer_tree_view_)
    return;

  if (layer) {
    root_layer_ = layer;
    layer_tree_view_->SetRootLayer(root_layer_);
  } else {
    root_layer_ = nullptr;
    // This means that we're transitioning to a new page. Suppress
    // commits until Blink generates invalidations so we don't
    // attempt to paint too early in the next page load.
    scoped_defer_commits_ = layer_tree_view_->DeferCommits();
    layer_tree_view_->ClearRootLayer();
    layer_tree_view_->ClearViewportLayers();
  }
}

void WebViewImpl::InvalidateRect(const IntRect& rect) {
  if (layer_tree_view_) {
    UpdateLayerTreeViewport();
  } else if (client_) {
    // This is only for WebViewPlugin.
    client_->WidgetClient()->DidInvalidateRect(rect);
  }
}

PaintLayerCompositor* WebViewImpl::Compositor() const {
  WebLocalFrameImpl* frame = MainFrameImpl();
  if (!frame)
    return nullptr;

  Document* document = frame->GetFrame()->GetDocument();
  if (!document || !document->GetLayoutView())
    return nullptr;

  return document->GetLayoutView()->Compositor();
}

GraphicsLayer* WebViewImpl::RootGraphicsLayer() {
  return root_graphics_layer_;
}

void WebViewImpl::ScheduleAnimationForWidget() {
  if (layer_tree_view_) {
    layer_tree_view_->SetNeedsBeginFrame();
    return;
  }
  if (client_)
    client_->WidgetClient()->ScheduleAnimation();
}

void WebViewImpl::SetLayerTreeView(WebLayerTreeView* layer_tree_view) {
  layer_tree_view_ = layer_tree_view;
  if (Platform::Current()->IsThreadedAnimationEnabled()) {
    animation_host_ = std::make_unique<CompositorAnimationHost>(
        layer_tree_view_->CompositorAnimationHost());
  }

  page_->LayerTreeViewInitialized(*layer_tree_view_, nullptr);
  // We don't yet have a page loaded at this point of the initialization of
  // WebViewImpl, so don't allow cc to commit any frames Blink might
  // try to create in the meantime.
  scoped_defer_commits_ = layer_tree_view_->DeferCommits();
}

void WebViewImpl::ApplyViewportChanges(const ApplyViewportChangesArgs& args) {
  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();

  // Store the desired offsets the visual viewport before setting the top
  // controls ratio since doing so will change the bounds and move the
  // viewports to keep the offsets valid. The compositor may have already
  // done that so we don't want to double apply the deltas here.
  FloatPoint visual_viewport_offset = visual_viewport.VisibleRect().Location();
  visual_viewport_offset.Move(args.inner_delta.x(), args.inner_delta.y());

  GetBrowserControls().SetShownRatio(GetBrowserControls().ShownRatio() +
                                     args.browser_controls_delta);

  SetPageScaleFactorAndLocation(PageScaleFactor() * args.page_scale_delta,
                                visual_viewport_offset);

  if (args.page_scale_delta != 1) {
    double_tap_zoom_pending_ = false;
    visual_viewport.UserDidChangeScale();
  }

  elastic_overscroll_ += FloatSize(args.elastic_overscroll_delta.x(),
                                   args.elastic_overscroll_delta.y());
  UpdateBrowserControlsConstraint(args.browser_controls_constraint);
}

void WebViewImpl::RecordWheelAndTouchScrollingCount(
    bool has_scrolled_by_wheel,
    bool has_scrolled_by_touch) {
  if (!MainFrameImpl())
    return;

  if (has_scrolled_by_wheel)
    UseCounter::Count(MainFrameImpl()->GetFrame(), WebFeature::kScrollByWheel);
  if (has_scrolled_by_touch)
    UseCounter::Count(MainFrameImpl()->GetFrame(), WebFeature::kScrollByTouch);
}

void WebViewImpl::UpdateLayerTreeViewport() {
  if (!GetPage() || !layer_tree_view_)
    return;

  layer_tree_view_->SetPageScaleFactorAndLimits(
      PageScaleFactor(), MinimumPageScaleFactor(), MaximumPageScaleFactor());
}

void WebViewImpl::UpdateLayerTreeBackgroundColor() {
  if (!layer_tree_view_)
    return;
  layer_tree_view_->SetBackgroundColor(BackgroundColor());
}

void WebViewImpl::UpdateDeviceEmulationTransform() {
  if (!visual_viewport_container_layer_)
    return;

  // When the device emulation transform is updated, to avoid incorrect
  // scales and fuzzy raster from the compositor, force all content to
  // pick ideal raster scales.
  visual_viewport_container_layer_->SetTransform(device_emulation_transform_);
  layer_tree_view_->ForceRecalculateRasterScales();
}

PageScheduler* WebViewImpl::Scheduler() const {
  DCHECK(GetPage());
  return GetPage()->GetPageScheduler();
}

void WebViewImpl::SetVisibilityState(
    mojom::PageVisibilityState visibility_state,
    bool is_initial_state) {
  DCHECK(GetPage());
  GetPage()->SetVisibilityState(visibility_state, is_initial_state);

  bool visible = visibility_state == mojom::PageVisibilityState::kVisible;
  if (layer_tree_view_ && !override_compositor_visibility_)
    layer_tree_view_->SetVisible(visible);
  GetPage()->GetPageScheduler()->SetPageVisible(visible);
}

void WebViewImpl::SetCompositorVisibility(bool is_visible) {
  if (!is_visible)
    override_compositor_visibility_ = true;
  else
    override_compositor_visibility_ = false;
  if (layer_tree_view_)
    layer_tree_view_->SetVisible(is_visible);
}

void WebViewImpl::ForceNextWebGLContextCreationToFail() {
  CoreInitializer::GetInstance().ForceNextWebGLContextCreationToFail();
}

void WebViewImpl::ForceNextDrawingBufferCreationToFail() {
  DrawingBuffer::ForceNextDrawingBufferCreationToFail();
}

base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
WebViewImpl::EnsureCompositorMutatorDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner>* mutator_task_runner) {
  if (!mutator_task_runner_) {
    layer_tree_view_->SetMutatorClient(
        AnimationWorkletMutatorDispatcherImpl::CreateCompositorThreadClient(
            &mutator_dispatcher_, &mutator_task_runner_));
  }

  DCHECK(mutator_task_runner_);
  *mutator_task_runner = mutator_task_runner_;
  return mutator_dispatcher_;
}

float WebViewImpl::DeviceScaleFactor() const {
  // TODO(oshima): Investigate if this should return the ScreenInfo's scale
  // factor rather than page's scale factor, which can be 1 in use-zoom-for-dsf
  // mode.
  if (!GetPage())
    return 1;

  return GetPage()->DeviceScaleFactorDeprecated();
}

LocalFrame* WebViewImpl::FocusedLocalFrameInWidget() const {
  if (!MainFrameImpl())
    return nullptr;

  LocalFrame* focused_frame = ToLocalFrame(FocusedCoreFrame());
  if (focused_frame->LocalFrameRoot() != MainFrameImpl()->GetFrame())
    return nullptr;
  return focused_frame;
}

LocalFrame* WebViewImpl::FocusedLocalFrameAvailableForIme() const {
  return ime_accept_events_ ? FocusedLocalFrameInWidget() : nullptr;
}

void WebViewImpl::SetPageFrozen(bool frozen) {
  Scheduler()->SetPageFrozen(frozen);
}

void WebViewImpl::AddAutoplayFlags(int32_t value) {
  page_->AddAutoplayFlags(value);
}

void WebViewImpl::ClearAutoplayFlags() {
  page_->ClearAutoplayFlags();
}

int32_t WebViewImpl::AutoplayFlagsForTest() {
  return page_->AutoplayFlags();
}

void WebViewImpl::DeferCommitsForTesting() {
  scoped_defer_commits_ = layer_tree_view_->DeferCommits();
}

}  // namespace blink
