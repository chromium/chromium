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

#include "third_party/blink/renderer/core/page/chrome_client_impl.h"

#include <memory>
#include <utility>

#include "base/debug/alias.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_popup_menu_info.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser_client.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser_popup_ui_controller.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser_ui_controller.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser_client.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser_impl.h"
#include "third_party/blink/renderer/core/html/forms/external_date_time_chooser.h"
#include "third_party/blink/renderer/core/html/forms/external_popup_menu.h"
#include "third_party/blink/renderer/core/html/forms/file_chooser.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/internal_popup_menu.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/popup_opening_observer.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_concatenate.h"

namespace blink {

namespace {

const char* UIElementTypeToString(ChromeClient::UIElementType ui_element_type) {
  switch (ui_element_type) {
    case ChromeClient::UIElementType::kAlertDialog:
      return "alert";
    case ChromeClient::UIElementType::kConfirmDialog:
      return "confirm";
    case ChromeClient::UIElementType::kPromptDialog:
      return "prompt";
    case ChromeClient::UIElementType::kPrintDialog:
      return "print";
    case ChromeClient::UIElementType::kPopup:
      return "popup";
  }
  NOTREACHED();
  return "";
}

const char* DismissalTypeToString(Document::PageDismissalType dismissal_type) {
  switch (dismissal_type) {
    case Document::kBeforeUnloadDismissal:
      return "beforeunload";
    case Document::kPageHideDismissal:
      return "pagehide";
    case Document::kUnloadVisibilityChangeDismissal:
      return "visibilitychange";
    case Document::kUnloadDismissal:
      return "unload";
    case Document::kNoDismissal:
      NOTREACHED();
  }
  NOTREACHED();
  return "";
}

String TruncateDialogMessage(const String& message) {
  if (message.IsNull())
    return g_empty_string;

  // 10k ought to be enough for anyone.
  const wtf_size_t kMaxMessageSize = 10 * 1024;
  return message.Substring(0, kMaxMessageSize);
}

}  // namespace

class CompositorAnimationTimeline;

ChromeClientImpl::ChromeClientImpl(WebViewImpl* web_view)
    : web_view_(web_view),
      cursor_overridden_(false),
      did_request_non_empty_tool_tip_(false) {}

ChromeClientImpl::~ChromeClientImpl() {
  DCHECK(file_chooser_queue_.IsEmpty());
}

void ChromeClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(popup_opening_observers_);
  visitor->Trace(external_date_time_chooser_);
  ChromeClient::Trace(visitor);
}

WebViewImpl* ChromeClientImpl::GetWebView() const {
  return web_view_;
}

void ChromeClientImpl::ChromeDestroyed() {
  // Our lifetime is bound to the WebViewImpl.
}

void ChromeClientImpl::SetWindowRect(const IntRect& r, LocalFrame& frame) {
  DCHECK_EQ(&frame, web_view_->MainFrameImpl()->GetFrame());
  web_view_->MainFrameViewWidget()->SetWindowRect(r);
}

IntRect ChromeClientImpl::RootWindowRect(LocalFrame& frame) {
  // The WindowRect() for each WebWidgetClient will be the same rect of the top
  // level window. Since there is not always a WebWidgetClient attached to the
  // WebView, we ask the WebWidget associated with the |frame|'s local root.
  return IntRect(frame.GetWidgetForLocalRoot()->WindowRect());
}

void ChromeClientImpl::FocusPage() {
  web_view_->Focus();
}

void ChromeClientImpl::DidFocusPage() {
  if (web_view_->Client())
    web_view_->Client()->DidFocus();
}

bool ChromeClientImpl::CanTakeFocus(mojom::blink::FocusType) {
  // For now the browser can always take focus if we're not running layout
  // tests.
  return !WebTestSupport::IsRunningWebTest();
}

void ChromeClientImpl::TakeFocus(mojom::blink::FocusType type) {
  if (!web_view_->Client())
    return;
  if (type == mojom::blink::FocusType::kBackward)
    web_view_->Client()->FocusPrevious();
  else
    web_view_->Client()->FocusNext();
}

void ChromeClientImpl::SetKeyboardFocusURL(Element* new_focus_element) {
  KURL focus_url;
  if (new_focus_element && new_focus_element->IsLiveLink() &&
      new_focus_element->ShouldHaveFocusAppearance())
    focus_url = new_focus_element->HrefURL();
  web_view_->SetKeyboardFocusURL(focus_url);
}

void ChromeClientImpl::StartDragging(LocalFrame* frame,
                                     const WebDragData& drag_data,
                                     WebDragOperationsMask mask,
                                     const SkBitmap& drag_image,
                                     const gfx::Point& drag_image_offset) {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  web_frame->LocalRootFrameWidget()->StartDragging(drag_data, mask, drag_image,
                                                   drag_image_offset);
}

bool ChromeClientImpl::AcceptsLoadDrops() const {
  return !web_view_->Client() || web_view_->Client()->AcceptsLoadDrops();
}

Page* ChromeClientImpl::CreateWindowDelegate(
    LocalFrame* frame,
    const FrameLoadRequest& r,
    const AtomicString& name,
    const WebWindowFeatures& features,
    network::mojom::blink::WebSandboxFlags sandbox_flags,
    const FeaturePolicyFeatureState& opener_feature_state,
    const SessionStorageNamespaceId& session_storage_namespace_id) {
  if (!web_view_->Client())
    return nullptr;

  if (!frame->GetPage() || frame->GetPage()->Paused())
    return nullptr;

  NotifyPopupOpeningObservers();
  const AtomicString& frame_name =
      !EqualIgnoringASCIICase(name, "_blank") ? name : g_empty_atom;
  WebViewImpl* new_view =
      static_cast<WebViewImpl*>(web_view_->Client()->CreateView(
          WebLocalFrameImpl::FromFrame(frame),
          WrappedResourceRequest(r.GetResourceRequest()), features, frame_name,
          static_cast<WebNavigationPolicy>(r.GetNavigationPolicy()),
          sandbox_flags, opener_feature_state, session_storage_namespace_id));
  if (!new_view)
    return nullptr;
  return new_view->GetPage();
}

void ChromeClientImpl::DidOverscroll(
    const gfx::Vector2dF& overscroll_delta,
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::PointF& position_in_viewport,
    const gfx::Vector2dF& velocity_in_viewport) {
  // WebWidgetClient can be null when not compositing, and this behaviour only
  // applies when compositing is enabled.
  if (!web_view_->does_composite())
    return;
  // TODO(darin): Change caller to pass LocalFrame.
  DCHECK(web_view_->MainFrameImpl());
  web_view_->MainFrameImpl()->FrameWidgetImpl()->DidOverscroll(
      overscroll_delta, accumulated_overscroll, position_in_viewport,
      velocity_in_viewport);
}

void ChromeClientImpl::InjectGestureScrollEvent(
    LocalFrame& local_frame,
    WebGestureDevice device,
    const gfx::Vector2dF& delta,
    ScrollGranularity granularity,
    CompositorElementId scrollable_area_element_id,
    WebInputEvent::Type injected_type) {
  local_frame.GetWidgetForLocalRoot()->InjectGestureScrollEvent(
      device, delta, granularity, scrollable_area_element_id, injected_type);
}

void ChromeClientImpl::SetOverscrollBehavior(
    LocalFrame& main_frame,
    const cc::OverscrollBehavior& overscroll_behavior) {
  DCHECK(main_frame.IsMainFrame());
  main_frame.GetWidgetForLocalRoot()->SetOverscrollBehavior(
      overscroll_behavior);
}

void ChromeClientImpl::Show(NavigationPolicy navigation_policy) {
  // TODO(darin): Change caller to pass LocalFrame.
  WebLocalFrameImpl* main_frame = web_view_->MainFrameImpl();
  DCHECK(main_frame);
  main_frame->FrameWidgetImpl()->Client()->Show(
      static_cast<WebNavigationPolicy>(navigation_policy));
  main_frame->DevToolsAgentImpl()->DidShowNewWindow();
}

bool ChromeClientImpl::ShouldReportDetailedMessageForSource(
    LocalFrame& local_frame,
    const String& url) {
  WebLocalFrameImpl* webframe =
      WebLocalFrameImpl::FromFrame(local_frame.LocalFrameRoot());
  return webframe && webframe->Client() &&
         webframe->Client()->ShouldReportDetailedMessageForSource(url);
}

void ChromeClientImpl::AddMessageToConsole(LocalFrame* local_frame,
                                           mojom::ConsoleMessageSource source,
                                           mojom::ConsoleMessageLevel level,
                                           const String& message,
                                           unsigned line_number,
                                           const String& source_id,
                                           const String& stack_trace) {
  WebLocalFrameImpl* frame = WebLocalFrameImpl::FromFrame(local_frame);
  if (frame && frame->Client()) {
    frame->Client()->DidAddMessageToConsole(
        WebConsoleMessage(static_cast<mojom::ConsoleMessageLevel>(level),
                          message),
        source_id, line_number, stack_trace);
  }
}

bool ChromeClientImpl::CanOpenBeforeUnloadConfirmPanel() {
  return !!web_view_->Client();
}

bool ChromeClientImpl::OpenBeforeUnloadConfirmPanelDelegate(LocalFrame* frame,
                                                            bool is_reload) {
  NotifyPopupOpeningObservers();

  if (before_unload_confirm_panel_result_for_testing_.has_value()) {
    bool success = before_unload_confirm_panel_result_for_testing_.value();
    before_unload_confirm_panel_result_for_testing_.reset();
    return success;
  }
  bool success = false;
  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().RunBeforeUnloadConfirm(is_reload, &success);
  return success;
}

void ChromeClientImpl::SetBeforeUnloadConfirmPanelResultForTesting(
    bool result) {
  before_unload_confirm_panel_result_for_testing_ = result;
}

void ChromeClientImpl::CloseWindowSoon() {
  web_view_->CloseWindowSoon();
}

bool ChromeClientImpl::OpenJavaScriptAlertDelegate(LocalFrame* frame,
                                                   const String& message) {
  NotifyPopupOpeningObservers();
  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().RunModalAlertDialog(
      TruncateDialogMessage(message));
  return true;
}

bool ChromeClientImpl::OpenJavaScriptConfirmDelegate(LocalFrame* frame,
                                                     const String& message) {
  NotifyPopupOpeningObservers();
  bool success = false;
  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().RunModalConfirmDialog(
      TruncateDialogMessage(message), &success);
  return success;
}

bool ChromeClientImpl::OpenJavaScriptPromptDelegate(LocalFrame* frame,
                                                    const String& message,
                                                    const String& default_value,
                                                    String& result) {
  NotifyPopupOpeningObservers();
  bool success = false;
  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().RunModalPromptDialog(
      TruncateDialogMessage(message),
      default_value.IsNull() ? g_empty_string : default_value, &success,
      &result);
  return success;
}
bool ChromeClientImpl::TabsToLinks() {
  return web_view_->TabsToLinks();
}

void ChromeClientImpl::InvalidateRect(const IntRect& update_rect) {
  if (!update_rect.IsEmpty())
    web_view_->InvalidateRect(update_rect);
}

void ChromeClientImpl::ScheduleAnimation(const LocalFrameView* frame_view,
                                         base::TimeDelta delay) {
  LocalFrame& frame = frame_view->GetFrame();
  // If the frame is still being created, it might not yet have a WebWidget.
  // TODO(dcheng): Is this the right thing to do? Is there a way to avoid having
  // a local frame root that doesn't have a WebWidget? During initialization
  // there is no content to draw so this call serves no purpose. Maybe the
  // WebFrameWidget needs to be initialized before initializing the core frame?
  FrameWidget* widget = frame.GetWidgetForLocalRoot();
  if (widget) {
    if (delay.is_zero()) {
      // LocalRootFrameWidget() is a WebWidget, its client is the embedder.
      WebWidgetClient* web_widget_client = widget->Client();
      web_widget_client->ScheduleAnimation();
    } else {
      widget->RequestAnimationAfterDelay(delay);
    }
  }
}

IntRect ChromeClientImpl::ViewportToScreen(
    const IntRect& rect_in_viewport,
    const LocalFrameView* frame_view) const {
  WebRect screen_rect(rect_in_viewport);

  LocalFrame& frame = frame_view->GetFrame();

  WebWidgetClient* client = frame.GetWidgetForLocalRoot()->Client();
  // TODO(dcheng): Is this null check needed?
  if (client) {
    client->ConvertViewportToWindow(&screen_rect);
    gfx::Rect view_rect = frame.GetWidgetForLocalRoot()->ViewRect();

    base::CheckedNumeric<int> screen_rect_x = screen_rect.x;
    base::CheckedNumeric<int> screen_rect_y = screen_rect.y;

    screen_rect_x += view_rect.x();
    screen_rect_y += view_rect.y();

    screen_rect.x =
        screen_rect_x.ValueOrDefault(std::numeric_limits<int>::max());
    screen_rect.y =
        screen_rect_y.ValueOrDefault(std::numeric_limits<int>::max());
  }

  return screen_rect;
}

float ChromeClientImpl::WindowToViewportScalar(LocalFrame* frame,
                                               const float scalar_value) const {

  // TODO(darin): Clean up callers to not pass null. E.g., VisualViewport::
  // ScrollbarThickness() is one such caller. See https://pastebin.com/axgctw0N
  // for a sample call stack.
  if (!frame) {
    DLOG(WARNING) << "LocalFrame is null!";
    return scalar_value;
  }

  WebFloatRect viewport_rect(0, 0, scalar_value, 0);
  frame->GetWidgetForLocalRoot()->Client()->ConvertWindowToViewport(
      &viewport_rect);
  return viewport_rect.width;
}

void ChromeClientImpl::WindowToViewportRect(LocalFrame& frame,
                                            WebFloatRect* viewport_rect) const {
  frame.GetWidgetForLocalRoot()->Client()->ConvertWindowToViewport(
      viewport_rect);
}

ScreenInfo ChromeClientImpl::GetScreenInfo(LocalFrame& frame) const {
  return frame.GetWidgetForLocalRoot()->GetScreenInfo();
}

void ChromeClientImpl::OverrideVisibleRectForMainFrame(
    LocalFrame& frame,
    IntRect* visible_rect) const {
  DCHECK(frame.IsMainFrame());
  return web_view_->GetDevToolsEmulator()->OverrideVisibleRect(
      IntRect(frame.GetWidgetForLocalRoot()->ViewRect()).Size(), visible_rect);
}

float ChromeClientImpl::InputEventsScaleForEmulation() const {
  return web_view_->GetDevToolsEmulator()->InputEventsScaleForEmulation();
}

void ChromeClientImpl::ContentsSizeChanged(LocalFrame* frame,
                                           const IntSize& size) const {
  web_view_->DidChangeContentsSize();

  WebLocalFrameImpl* webframe = WebLocalFrameImpl::FromFrame(frame);
  webframe->DidChangeContentsSize(size);
}

bool ChromeClientImpl::DoubleTapToZoomEnabled() const {
  return web_view_->SettingsImpl()->DoubleTapToZoomEnabled();
}

void ChromeClientImpl::EnablePreferredSizeChangedMode() {
  web_view_->EnablePreferredSizeChangedMode();
}

void ChromeClientImpl::ZoomToFindInPageRect(const WebRect& rect_in_root_frame) {
  web_view_->ZoomToFindInPageRect(rect_in_root_frame);
}

void ChromeClientImpl::PageScaleFactorChanged() const {
  web_view_->PageScaleFactorChanged();
}

void ChromeClientImpl::MainFrameScrollOffsetChanged(
    LocalFrame& main_frame) const {
  DCHECK(main_frame.IsMainFrame());
  web_view_->MainFrameScrollOffsetChanged();
}

float ChromeClientImpl::ClampPageScaleFactorToLimits(float scale) const {
  return web_view_->ClampPageScaleFactorToLimits(scale);
}

void ChromeClientImpl::ResizeAfterLayout() const {
  web_view_->ResizeAfterLayout();
}

void ChromeClientImpl::MainFrameLayoutUpdated() const {
  web_view_->MainFrameLayoutUpdated();
}

void ChromeClientImpl::ShowMouseOverURL(const HitTestResult& result) {
  if (!web_view_->Client())
    return;

  KURL url;

  // Ignore URL if hitTest include scrollbar since we might have both a
  // scrollbar and an element in the case of overlay scrollbars.
  if (!result.GetScrollbar()) {
    // Find out if the mouse is over a link, and if so, let our UI know...
    if (result.IsLiveLink() &&
        !result.AbsoluteLinkURL().GetString().IsEmpty()) {
      url = result.AbsoluteLinkURL();
    } else if (result.InnerNode() &&
               (IsA<HTMLObjectElement>(*result.InnerNode()) ||
                IsA<HTMLEmbedElement>(*result.InnerNode()))) {
      LayoutObject* object = result.InnerNode()->GetLayoutObject();
      if (object && object->IsLayoutEmbeddedContent()) {
        WebPluginContainerImpl* plugin_view =
            ToLayoutEmbeddedContent(object)->Plugin();
        if (plugin_view) {
          url = plugin_view->Plugin()->LinkAtPosition(
              result.RoundedPointInInnerNodeFrame());
        }
      }
    }
  }

  web_view_->SetMouseOverURL(url);
}

void ChromeClientImpl::SetToolTip(LocalFrame& frame,
                                  const String& tooltip_text,
                                  TextDirection dir) {
  WebFrameWidgetBase* widget =
      WebLocalFrameImpl::FromFrame(frame)->LocalRootFrameWidget();
  if (!tooltip_text.IsEmpty()) {
    widget->SetToolTipText(tooltip_text, dir);
    did_request_non_empty_tool_tip_ = true;
  } else if (did_request_non_empty_tool_tip_) {
    // WebFrameWidgetBase::SetToolTipText will send a Mojo message via
    // mojom::blink::WidgetHost. We'd like to reduce the number of
    // SetToolTipText calls.
    widget->SetToolTipText(tooltip_text, dir);
    did_request_non_empty_tool_tip_ = false;
  }
}

void ChromeClientImpl::DispatchViewportPropertiesDidChange(
    const ViewportDescription& description) const {
  web_view_->UpdatePageDefinedViewportConstraints(description);
}

void ChromeClientImpl::PrintDelegate(LocalFrame* frame) {
  NotifyPopupOpeningObservers();
  if (web_view_->Client())
    web_view_->Client()->PrintPage(WebLocalFrameImpl::FromFrame(frame));
}

ColorChooser* ChromeClientImpl::OpenColorChooser(
    LocalFrame* frame,
    ColorChooserClient* chooser_client,
    const Color&) {
  NotifyPopupOpeningObservers();
  ColorChooserUIController* controller = nullptr;

  // TODO(crbug.com/779126): add support for the chooser in immersive mode.
  if (frame->GetDocument()->GetSettings()->GetImmersiveModeEnabled())
    return nullptr;

  if (RuntimeEnabledFeatures::PagePopupEnabled()) {
    controller = MakeGarbageCollected<ColorChooserPopupUIController>(
        frame, this, chooser_client);
  } else {
    controller =
        MakeGarbageCollected<ColorChooserUIController>(frame, chooser_client);
  }
  controller->OpenUI();
  return controller;
}

DateTimeChooser* ChromeClientImpl::OpenDateTimeChooser(
    LocalFrame* frame,
    DateTimeChooserClient* picker_client,
    const DateTimeChooserParameters& parameters) {
  // TODO(crbug.com/779126): add support for the chooser in immersive mode.
  if (picker_client->OwnerElement()
          .GetDocument()
          .GetSettings()
          ->GetImmersiveModeEnabled())
    return nullptr;

  NotifyPopupOpeningObservers();
  if (RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled()) {
    return MakeGarbageCollected<DateTimeChooserImpl>(frame, picker_client,
                                                     parameters);
  }

  // JavaScript may try to open a date time chooser while one is already open.
  if (external_date_time_chooser_ &&
      external_date_time_chooser_->IsShowingDateTimeChooserUI())
    return nullptr;

  external_date_time_chooser_ =
      MakeGarbageCollected<ExternalDateTimeChooser>(picker_client);
  external_date_time_chooser_->OpenDateTimeChooser(frame, parameters);
  return external_date_time_chooser_;
}

ExternalDateTimeChooser*
ChromeClientImpl::GetExternalDateTimeChooserForTesting() {
  return external_date_time_chooser_;
}

void ChromeClientImpl::OpenFileChooser(
    LocalFrame* frame,
    scoped_refptr<FileChooser> file_chooser) {
  NotifyPopupOpeningObservers();

  Document* doc = frame->GetDocument();
  if (doc)
    doc->MaybeQueueSendDidEditFieldInInsecureContext();

  static const wtf_size_t kMaximumPendingFileChooseRequests = 4;
  if (file_chooser_queue_.size() > kMaximumPendingFileChooseRequests) {
    // This sanity check prevents too many file choose requests from getting
    // queued which could DoS the user. Getting these is most likely a
    // programming error (there are many ways to DoS the user so it's not
    // considered a "real" security check), either in JS requesting many file
    // choosers to pop up, or in a plugin.
    //
    // TODO(brettw): We might possibly want to require a user gesture to open
    // a file picker, which will address this issue in a better way.
    return;
  }
  file_chooser_queue_.push_back(file_chooser.get());
  if (file_chooser_queue_.size() == 1) {
    // Actually show the browse dialog when this is the first request.
    if (file_chooser->OpenFileChooser(*this))
      return;
    // Choosing failed, so try the next chooser.
    DidCompleteFileChooser(*file_chooser);
  }
}

void ChromeClientImpl::DidCompleteFileChooser(FileChooser& chooser) {
  if (!file_chooser_queue_.IsEmpty() &&
      file_chooser_queue_.front().get() != &chooser) {
    // This function is called even if |chooser| wasn't stored in
    // file_chooser_queue_.
    return;
  }
  file_chooser_queue_.EraseAt(0);
  if (file_chooser_queue_.IsEmpty())
    return;
  FileChooser* next_chooser = file_chooser_queue_.front().get();
  if (next_chooser->OpenFileChooser(*this))
    return;
  // Choosing failed, so try the next chooser.
  DidCompleteFileChooser(*next_chooser);
}

ui::Cursor ChromeClientImpl::LastSetCursorForTesting() const {
  return last_set_mouse_cursor_for_testing_;
}

void ChromeClientImpl::SetCursor(const ui::Cursor& cursor,
                                 LocalFrame* local_frame) {
  last_set_mouse_cursor_for_testing_ = cursor;
  SetCursorInternal(cursor, local_frame);
}

void ChromeClientImpl::SetCursorInternal(const ui::Cursor& cursor,
                                         LocalFrame* local_frame) {
  if (cursor_overridden_)
    return;

#if defined(OS_MAC)
  // On Mac the mousemove event propagates to both the popup and main window.
  // If a popup is open we don't want the main window to change the cursor.
  if (web_view_->HasOpenedPopup())
    return;
#endif

  // TODO(dcheng): Why is this null check necessary?
  if (FrameWidget* widget = local_frame->GetWidgetForLocalRoot())
    widget->DidChangeCursor(cursor);
}

void ChromeClientImpl::SetCursorForPlugin(const ui::Cursor& cursor,
                                          LocalFrame* local_frame) {
  SetCursorInternal(cursor, local_frame);
}

void ChromeClientImpl::SetCursorOverridden(bool overridden) {
  cursor_overridden_ = overridden;
}

void ChromeClientImpl::AutoscrollStart(const gfx::PointF& viewport_point,
                                       LocalFrame* local_frame) {
  // TODO(dcheng): Why is this null check necessary?
  if (WebFrameWidgetBase* widget =
          WebLocalFrameImpl::FromFrame(local_frame)->LocalRootFrameWidget())
    widget->AutoscrollStart(viewport_point);
}

void ChromeClientImpl::AutoscrollFling(const gfx::Vector2dF& velocity,
                                       LocalFrame* local_frame) {
  // TODO(dcheng): Why is this null check necessary?
  if (WebFrameWidgetBase* widget =
          WebLocalFrameImpl::FromFrame(local_frame)->LocalRootFrameWidget())
    widget->AutoscrollFling(velocity);
}

void ChromeClientImpl::AutoscrollEnd(LocalFrame* local_frame) {
  // TODO(dcheng): Why is this null check necessary?
  if (WebFrameWidgetBase* widget =
          WebLocalFrameImpl::FromFrame(local_frame)->LocalRootFrameWidget())
    widget->AutoscrollEnd();
}

String ChromeClientImpl::AcceptLanguages() {
  return web_view_->Client()->AcceptLanguages();
}

void ChromeClientImpl::AttachRootLayer(scoped_refptr<cc::Layer> root_layer,
                                       LocalFrame* local_frame) {
  DCHECK(local_frame->IsLocalRoot());

  // This method is called during Document::Shutdown with a null |root_layer|,
  // but a widget may have never been created in some tests, so it would also
  // be null (we don't call here with a valid |root_layer| in those tests).
  FrameWidget* widget = local_frame->GetWidgetForLocalRoot();
  DCHECK(widget || !root_layer);
  if (widget)
    widget->SetRootLayer(std::move(root_layer));
}

void ChromeClientImpl::AttachCompositorAnimationTimeline(
    CompositorAnimationTimeline* compositor_timeline,
    LocalFrame* local_frame) {
  DCHECK(Platform::Current()->IsThreadedAnimationEnabled());
  FrameWidget* widget = local_frame->GetWidgetForLocalRoot();
  DCHECK(widget);
  widget->AnimationHost()->AddAnimationTimeline(
      compositor_timeline->GetAnimationTimeline());
}

void ChromeClientImpl::DetachCompositorAnimationTimeline(
    CompositorAnimationTimeline* compositor_timeline,
    LocalFrame* local_frame) {
  DCHECK(Platform::Current()->IsThreadedAnimationEnabled());
  FrameWidget* widget = local_frame->GetWidgetForLocalRoot();
  DCHECK(widget);
  widget->AnimationHost()->RemoveAnimationTimeline(
      compositor_timeline->GetAnimationTimeline());
}

void ChromeClientImpl::EnterFullscreen(LocalFrame& frame,
                                       const FullscreenOptions* options,
                                       FullscreenRequestType request_type) {
  web_view_->EnterFullscreen(frame, options, request_type);
}

void ChromeClientImpl::ExitFullscreen(LocalFrame& frame) {
  web_view_->ExitFullscreen(frame);
}

void ChromeClientImpl::FullscreenElementChanged(Element* old_element,
                                                Element* new_element) {
  web_view_->FullscreenElementChanged(old_element, new_element);
}

void ChromeClientImpl::AnimateDoubleTapZoom(const gfx::Point& point,
                                            const gfx::Rect& rect) {
  web_view_->AnimateDoubleTapZoom(point, WebRect(rect));
}

void ChromeClientImpl::ClearLayerSelection(LocalFrame* frame) {
  frame->GetWidgetForLocalRoot()->RegisterSelection(cc::LayerSelection());
}

void ChromeClientImpl::UpdateLayerSelection(
    LocalFrame* frame,
    const cc::LayerSelection& selection) {
  frame->GetWidgetForLocalRoot()->RegisterSelection(selection);
}

bool ChromeClientImpl::HasOpenedPopup() const {
  return web_view_->HasOpenedPopup();
}

PopupMenu* ChromeClientImpl::OpenPopupMenu(LocalFrame& frame,
                                           HTMLSelectElement& select) {
  NotifyPopupOpeningObservers();
  if (WebViewImpl::UseExternalPopupMenus())
    return MakeGarbageCollected<ExternalPopupMenu>(frame, select);

  DCHECK(RuntimeEnabledFeatures::PagePopupEnabled());
  return MakeGarbageCollected<InternalPopupMenu>(this, select);
}

PagePopup* ChromeClientImpl::OpenPagePopup(PagePopupClient* client) {
  return web_view_->OpenPagePopup(client);
}

void ChromeClientImpl::ClosePagePopup(PagePopup* popup) {
  web_view_->ClosePagePopup(popup);
}

DOMWindow* ChromeClientImpl::PagePopupWindowForTesting() const {
  return web_view_->PagePopupWindow();
}

void ChromeClientImpl::SetBrowserControlsState(float top_height,
                                               float bottom_height,
                                               bool shrinks_layout) {
  DCHECK(web_view_->MainFrameWidget());
  WebSize size = web_view_->MainFrameWidget()->Size();
  if (shrinks_layout)
    size.height -= top_height + bottom_height;

  web_view_->ResizeWithBrowserControls(size, top_height, bottom_height,
                                       shrinks_layout);
}

void ChromeClientImpl::SetBrowserControlsShownRatio(float top_ratio,
                                                    float bottom_ratio) {
  web_view_->GetBrowserControls().SetShownRatio(top_ratio, bottom_ratio);
}

bool ChromeClientImpl::ShouldOpenUIElementDuringPageDismissal(
    LocalFrame& frame,
    UIElementType ui_element_type,
    const String& dialog_message,
    Document::PageDismissalType dismissal_type) const {
  // TODO(https://crbug.com/937569): Remove this in Chrome 88.
  if (ui_element_type == ChromeClient::UIElementType::kPopup &&
      web_view_->Client() &&
      web_view_->Client()->AllowPopupsDuringPageUnload()) {
    return true;
  }

  StringBuilder builder;
  builder.Append("Blocked ");
  builder.Append(UIElementTypeToString(ui_element_type));
  if (dialog_message.length()) {
    builder.Append("('");
    builder.Append(dialog_message);
    builder.Append("')");
  }
  builder.Append(" during ");
  builder.Append(DismissalTypeToString(dismissal_type));
  builder.Append(".");

  WebLocalFrameImpl::FromFrame(frame)->AddMessageToConsole(WebConsoleMessage(
      mojom::ConsoleMessageLevel::kError, builder.ToString()));

  return false;
}

viz::FrameSinkId ChromeClientImpl::GetFrameSinkId(LocalFrame* frame) {
  WebWidgetClient* client = frame->GetWidgetForLocalRoot()->Client();
  return client->GetFrameSinkId();
}

void ChromeClientImpl::RequestDecode(LocalFrame* frame,
                                     const PaintImage& image,
                                     base::OnceCallback<void(bool)> callback) {
  FrameWidget* widget = frame->GetWidgetForLocalRoot();
  widget->RequestDecode(image, std::move(callback));
}

void ChromeClientImpl::NotifySwapTime(LocalFrame& frame,
                                      ReportTimeCallback callback) {
  FrameWidget* widget = frame.GetWidgetForLocalRoot();
  if (!widget)
    return;
  widget->NotifySwapAndPresentationTimeInBlink(
      base::NullCallback(), ConvertToBaseOnceCallback(std::move(callback)));
}

void ChromeClientImpl::RequestBeginMainFrameNotExpected(LocalFrame& frame,
                                                        bool request) {
  frame.GetWidgetForLocalRoot()->RequestBeginMainFrameNotExpected(request);
}

int ChromeClientImpl::GetLayerTreeId(LocalFrame& frame) {
  return frame.GetWidgetForLocalRoot()->GetLayerTreeId();
}

void ChromeClientImpl::SetEventListenerProperties(
    LocalFrame* frame,
    cc::EventListenerClass event_class,
    cc::EventListenerProperties properties) {
  // This method is only useful when compositing is enabled.
  if (!web_view_->does_composite())
    return;

  // |frame| might be null if called via TreeScopeAdopter::
  // moveNodeToNewDocument() and the new document has no frame attached.
  // Since a document without a frame cannot attach one later, it is safe to
  // exit early.
  if (!frame)
    return;

  FrameWidget* widget = frame->GetWidgetForLocalRoot();
  // TODO(https://crbug.com/820787): When creating a local root, the widget
  // won't be set yet. While notifications in this case are technically
  // redundant, it adds an awkward special case.
  if (!widget) {
    WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
    if (web_frame->IsProvisional()) {
      // If we hit a provisional frame, we expect it to be during initialization
      // in which case the |properties| should be 'nothing'.
      DCHECK(properties == cc::EventListenerProperties::kNone);
    }
    return;
  }

  widget->SetEventListenerProperties(event_class, properties);
}

cc::EventListenerProperties ChromeClientImpl::EventListenerProperties(
    LocalFrame* frame,
    cc::EventListenerClass event_class) const {
  if (!frame)
    return cc::EventListenerProperties::kNone;

  WebFrameWidgetBase* widget =
      WebLocalFrameImpl::FromFrame(frame)->LocalRootFrameWidget();
  if (!widget)
    return cc::EventListenerProperties::kNone;
  return widget->EventListenerProperties(event_class);
}

void ChromeClientImpl::BeginLifecycleUpdates(LocalFrame& main_frame) {
  DCHECK(main_frame.IsMainFrame());
  web_view_->StopDeferringMainFrameUpdate();
}

void ChromeClientImpl::StartDeferringCommits(LocalFrame& main_frame,
                                             base::TimeDelta timeout) {
  DCHECK(main_frame.IsMainFrame());
  WebLocalFrameImpl::FromFrame(main_frame)
      ->FrameWidgetImpl()
      ->StartDeferringCommits(timeout);
}

void ChromeClientImpl::StopDeferringCommits(
    LocalFrame& main_frame,
    cc::PaintHoldingCommitTrigger trigger) {
  DCHECK(main_frame.IsMainFrame());
  WebLocalFrameImpl::FromFrame(main_frame)
      ->FrameWidgetImpl()
      ->StopDeferringCommits(trigger);
}

void ChromeClientImpl::SetHasScrollEventHandlers(LocalFrame* frame,
                                                 bool has_event_handlers) {
  // |frame| might be null if called via
  // TreeScopeAdopter::MoveNodeToNewDocument() and the new document has no frame
  // attached. Since a document without a frame cannot attach one later, it is
  // safe to exit early.
  if (!frame)
    return;

  WebLocalFrameImpl::FromFrame(frame)
      ->LocalRootFrameWidget()
      ->SetHaveScrollEventHandlers(has_event_handlers);
}

void ChromeClientImpl::SetNeedsLowLatencyInput(LocalFrame* frame,
                                               bool needs_low_latency) {
  DCHECK(frame);
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetBase* widget = web_frame->LocalRootFrameWidget();
  if (!widget)
    return;

  widget->SetNeedsLowLatencyInput(needs_low_latency);
}

void ChromeClientImpl::SetNeedsUnbufferedInputForDebugger(LocalFrame* frame,
                                                          bool unbuffered) {
  DCHECK(frame);
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetBase* widget = web_frame->LocalRootFrameWidget();
  if (!widget)
    return;

  widget->SetNeedsUnbufferedInputForDebugger(unbuffered);
}

void ChromeClientImpl::RequestUnbufferedInputEvents(LocalFrame* frame) {
  DCHECK(frame);
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetBase* widget = web_frame->LocalRootFrameWidget();
  if (!widget)
    return;

  widget->RequestUnbufferedInputEvents();
}

void ChromeClientImpl::SetTouchAction(LocalFrame* frame,
                                      TouchAction touch_action) {
  DCHECK(frame);
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetBase* widget = web_frame->LocalRootFrameWidget();
  if (!widget)
    return;

  widget->ProcessTouchAction(touch_action);
}

bool ChromeClientImpl::RequestPointerLock(
    LocalFrame* frame,
    WebWidgetClient::PointerLockCallback callback,
    bool request_unadjusted_movement) {
  return WebLocalFrameImpl::FromFrame(frame)
      ->LocalRootFrameWidget()
      ->Client()
      ->RequestPointerLock(WebLocalFrameImpl::FromFrame(frame),
                           std::move(callback), request_unadjusted_movement);
}

bool ChromeClientImpl::RequestPointerLockChange(
    LocalFrame* frame,
    WebWidgetClient::PointerLockCallback callback,
    bool request_unadjusted_movement) {
  return WebLocalFrameImpl::FromFrame(frame)
      ->LocalRootFrameWidget()
      ->Client()
      ->RequestPointerLockChange(WebLocalFrameImpl::FromFrame(frame),
                                 std::move(callback),
                                 request_unadjusted_movement);
}

void ChromeClientImpl::RequestPointerUnlock(LocalFrame* frame) {
  return WebLocalFrameImpl::FromFrame(frame)
      ->LocalRootFrameWidget()
      ->Client()
      ->RequestPointerUnlock();
}

void ChromeClientImpl::DidAssociateFormControlsAfterLoad(LocalFrame* frame) {
  if (auto* fill_client = AutofillClientFromFrame(frame))
    fill_client->DidAssociateFormControlsDynamically();
}

void ChromeClientImpl::ShowVirtualKeyboardOnElementFocus(LocalFrame& frame) {
  WebLocalFrameImpl::FromFrame(frame)
      ->LocalRootFrameWidget()
      ->ShowVirtualKeyboardOnElementFocus();
}

void ChromeClientImpl::OnMouseDown(Node& mouse_down_node) {
  if (auto* fill_client =
          AutofillClientFromFrame(mouse_down_node.GetDocument().GetFrame())) {
    fill_client->DidReceiveLeftMouseDownOrGestureTapInNode(
        WebNode(&mouse_down_node));
  }
}

void ChromeClientImpl::HandleKeyboardEventOnTextField(
    HTMLInputElement& input_element,
    KeyboardEvent& event) {
  if (auto* fill_client =
          AutofillClientFromFrame(input_element.GetDocument().GetFrame())) {
    fill_client->TextFieldDidReceiveKeyDown(WebInputElement(&input_element),
                                            WebKeyboardEventBuilder(event));
  }
}

void ChromeClientImpl::DidChangeValueInTextField(
    HTMLFormControlElement& element) {
  Document& doc = element.GetDocument();
  if (auto* fill_client = AutofillClientFromFrame(doc.GetFrame()))
    fill_client->TextFieldDidChange(WebFormControlElement(&element));

  // Value changes caused by |document.execCommand| calls should not be
  // interpreted as a user action. See https://crbug.com/764760.
  if (!doc.IsRunningExecCommand()) {
    UseCounter::Count(doc, doc.GetExecutionContext()->IsSecureContext()
                               ? WebFeature::kFieldEditInSecureContext
                               : WebFeature::kFieldEditInNonSecureContext);
    doc.MaybeQueueSendDidEditFieldInInsecureContext();
    // The resource coordinator is not available in some tests.
    if (auto* rc = doc.GetResourceCoordinator())
      rc->SetHadFormInteraction();
  }
}

void ChromeClientImpl::DidEndEditingOnTextField(
    HTMLInputElement& input_element) {
  if (auto* fill_client =
          AutofillClientFromFrame(input_element.GetDocument().GetFrame())) {
    fill_client->TextFieldDidEndEditing(WebInputElement(&input_element));
  }
}

void ChromeClientImpl::OpenTextDataListChooser(HTMLInputElement& input) {
  NotifyPopupOpeningObservers();
  if (auto* fill_client =
          AutofillClientFromFrame(input.GetDocument().GetFrame())) {
    fill_client->OpenTextDataListChooser(WebInputElement(&input));
  }
}

void ChromeClientImpl::TextFieldDataListChanged(HTMLInputElement& input) {
  if (auto* fill_client =
          AutofillClientFromFrame(input.GetDocument().GetFrame())) {
    fill_client->DataListOptionsChanged(WebInputElement(&input));
  }
}

void ChromeClientImpl::DidChangeSelectionInSelectControl(
    HTMLFormControlElement& element) {
  Document& doc = element.GetDocument();
  if (auto* fill_client = AutofillClientFromFrame(doc.GetFrame()))
    fill_client->SelectControlDidChange(WebFormControlElement(&element));
}

void ChromeClientImpl::SelectFieldOptionsChanged(
    HTMLFormControlElement& element) {
  Document& doc = element.GetDocument();
  if (auto* fill_client = AutofillClientFromFrame(doc.GetFrame()))
    fill_client->SelectFieldOptionsChanged(WebFormControlElement(&element));
}

void ChromeClientImpl::AjaxSucceeded(LocalFrame* frame) {
  if (auto* fill_client = AutofillClientFromFrame(frame))
    fill_client->AjaxSucceeded();
}

TransformationMatrix ChromeClientImpl::GetDeviceEmulationTransform() const {
  return web_view_->GetDeviceEmulationTransform();
}

void ChromeClientImpl::DidUpdateBrowserControls() const {
  web_view_->DidUpdateBrowserControls();
}

void ChromeClientImpl::RegisterPopupOpeningObserver(
    PopupOpeningObserver* observer) {
  DCHECK(observer);
  popup_opening_observers_.insert(observer);
}

void ChromeClientImpl::UnregisterPopupOpeningObserver(
    PopupOpeningObserver* observer) {
  DCHECK(popup_opening_observers_.Contains(observer));
  popup_opening_observers_.erase(observer);
}

void ChromeClientImpl::NotifyPopupOpeningObservers() const {
  const HeapHashSet<WeakMember<PopupOpeningObserver>> observers(
      popup_opening_observers_);
  for (const auto& observer : observers)
    observer->WillOpenPopup();
}

FloatSize ChromeClientImpl::ElasticOverscroll() const {
  return web_view_->ElasticOverscroll();
}

WebAutofillClient* ChromeClientImpl::AutofillClientFromFrame(
    LocalFrame* frame) {
  if (!frame) {
    // It is possible to pass nullptr to this method. For instance the call from
    // OnMouseDown might be nullptr. See https://crbug.com/739199.
    return nullptr;
  }

  return WebLocalFrameImpl::FromFrame(frame)->AutofillClient();
}

void ChromeClientImpl::DidUpdateTextAutosizerPageInfo(
    const mojom::blink::TextAutosizerPageInfo& page_info) {
  web_view_->TextAutosizerPageInfoChanged(page_info);
}

void ChromeClientImpl::DocumentDetached(Document& document) {
  for (auto& it : file_chooser_queue_) {
    if (it->FrameOrNull() == document.GetFrame())
      it->DisconnectClient();
  }
}

double ChromeClientImpl::UserZoomFactor() const {
  return PageZoomLevelToZoomFactor(web_view_->ZoomLevel());
}

void ChromeClientImpl::SetDelegatedInkMetadata(
    LocalFrame* frame,
    std::unique_ptr<viz::DelegatedInkMetadata> metadata) {
  frame->GetWidgetForLocalRoot()->SetDelegatedInkMetadata(std::move(metadata));
}

void ChromeClientImpl::BatterySavingsChanged(LocalFrame& main_frame,
                                             WebBatterySavingsFlags savings) {
  DCHECK(main_frame.IsMainFrame());
  WebLocalFrameImpl::FromFrame(main_frame)
      ->FrameWidgetImpl()
      ->BatterySavingsChanged(savings);
}

}  // namespace blink
