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
#include <optional>
#include <utility>

#include "base/debug/alias.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/paint_holding_reason.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/widget/constants.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_form_element.h"
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
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
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
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/internal_popup_menu.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/popup_opening_observer.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_concatenate.h"
#include "ui/gfx/geometry/rect.h"

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
  NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

String TruncateDialogMessage(const String& message) {
  if (message.IsNull())
    return g_empty_string;

  // 10k ought to be enough for anyone.
  const wtf_size_t kMaxMessageSize = 10 * 1024;
  return message.Substring(0, kMaxMessageSize);
}

bool DisplayModeIsBorderless(LocalFrame& frame) {
  FrameWidget* widget = frame.GetWidgetForLocalRoot();
  return widget->DisplayMode() == mojom::blink::DisplayMode::kBorderless;
}

}  // namespace

static bool g_can_browser_handle_focus = false;

// Function defined in third_party/blink/public/web/blink.h.
void SetBrowserCanHandleFocusForWebTest(bool value) {
  g_can_browser_handle_focus = value;
}

ChromeClientImpl::ChromeClientImpl(WebViewImpl* web_view)
    : web_view_(web_view),
      cursor_overridden_(false),
      did_request_non_empty_tool_tip_(false) {
  DCHECK(web_view_);
}

ChromeClientImpl::~ChromeClientImpl() {
  DCHECK(file_chooser_queue_.empty());
}

void ChromeClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(popup_opening_observers_);
  visitor->Trace(external_date_time_chooser_);
  visitor->Trace(commit_observers_);
  ChromeClient::Trace(visitor);
}

WebViewImpl* ChromeClientImpl::GetWebView() const {
  return web_view_;
}

void ChromeClientImpl::ChromeDestroyed() {
  // Clear |web_view_| since it is refcounted and this class is a GC'd object
  // and may outlive the WebViewImpl.
  web_view_ = nullptr;
}

void ChromeClientImpl::SetWindowRect(const gfx::Rect& requested_rect,
                                     LocalFrame& frame) {
  DCHECK(web_view_);
  DCHECK_EQ(&frame, web_view_->MainFrameImpl()->GetFrame());

  int minimum_size = DisplayModeIsBorderless(frame)
                         ? blink::kMinimumBorderlessWindowSize
                         : blink::kMinimumWindowSize;

  // TODO(crbug.com/1515106): Refactor so that the limits only live browser-side
  // instead of now partly being duplicated browser-side and renderer side.
  const gfx::Rect rect_adjusted_for_minimum =
      AdjustWindowRectForMinimum(requested_rect, minimum_size);
  const gfx::Rect adjusted_rect = AdjustWindowRectForDisplay(
      rect_adjusted_for_minimum, frame, minimum_size);
  // Request the unadjusted rect if the browser may honor cross-screen bounds.
  // Permission state is not readily available, so adjusted bounds are clamped
  // to the same-screen, to retain legacy behavior of synchronous pending values
  // and to avoid exposing other screen details to frames without permission.
  // TODO(crbug.com/897300): Use permission state for better sync estimates or
  // store unadjusted pending window rects if that will not break many sites.
  web_view_->MainFrameViewWidget()->SetWindowRect(rect_adjusted_for_minimum,
                                                  adjusted_rect);
}

void ChromeClientImpl::Minimize(LocalFrame&) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  DCHECK(web_view_);
  web_view_->Minimize();
#endif
}

void ChromeClientImpl::Maximize(LocalFrame&) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  DCHECK(web_view_);
  web_view_->Maximize();
#endif
}

void ChromeClientImpl::Restore(LocalFrame&) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  DCHECK(web_view_);
  web_view_->Restore();
#endif
}

void ChromeClientImpl::SetResizable(bool resizable, LocalFrame& frame) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  DCHECK(web_view_);
  web_view_->SetResizable(resizable);
#endif
}

gfx::Rect ChromeClientImpl::RootWindowRect(LocalFrame& frame) {
  // The WindowRect() for each WebFrameWidget will be the same rect of the top
  // level window.
  return frame.GetWidgetForLocalRoot()->WindowRect();
}

void ChromeClientImpl::DidAccessInitialMainDocument() {
  DCHECK(web_view_);
  web_view_->DidAccessInitialMainDocument();
}

void ChromeClientImpl::FocusPage() {
  DCHECK(web_view_);
  web_view_->Focus();
}

void ChromeClientImpl::DidFocusPage() {
  DCHECK(web_view_);
  if (web_view_->Client())
    web_view_->Client()->DidFocus();
}

bool ChromeClientImpl::CanTakeFocus(mojom::blink::FocusType) {
  // For now the browser can always take focus if we're not running layout
  // tests.
  if (!WebTestSupport::IsRunningWebTest())
    return true;
  return g_can_browser_handle_focus;
}

void ChromeClientImpl::TakeFocus(mojom::blink::FocusType type) {
  DCHECK(web_view_);
  web_view_->TakeFocus(type == mojom::blink::FocusType::kBackward);
}

void ChromeClientImpl::SetKeyboardFocusURL(Element* new_focus_element) {
  DCHECK(web_view_);
  KURL focus_url;
  if (new_focus_element && new_focus_element->IsLiveLink() &&
      new_focus_element->ShouldHaveFocusAppearance())
    focus_url = new_focus_element->HrefURL();
  web_view_->SetKeyboardFocusURL(focus_url);
}

bool ChromeClientImpl::SupportsDraggableRegions() {
  return web_view_->SupportsDraggableRegions();
}

void ChromeClientImpl::DraggableRegionsChanged() {
  return web_view_->DraggableRegionsChanged();
}

void ChromeClientImpl::StartDragging(LocalFrame* frame,
                                     const WebDragData& drag_data,
                                     DragOperationsMask mask,
                                     const SkBitmap& drag_image,
                                     const gfx::Vector2d& cursor_offset,
                                     const gfx::Rect& drag_obj_rect) {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  web_frame->LocalRootFrameWidget()->StartDragging(
      frame, drag_data, mask, drag_image, cursor_offset, drag_obj_rect);
}

bool ChromeClientImpl::AcceptsLoadDrops() const {
  DCHECK(web_view_);
  return web_view_->GetRendererPreferences().can_accept_load_drops;
}

Page* ChromeClientImpl::CreateWindowDelegate(
    LocalFrame* frame,
    const FrameLoadRequest& r,
    const AtomicString& name,
    const WebWindowFeatures& features,
    network::mojom::blink::WebSandboxFlags sandbox_flags,
    const SessionStorageNamespaceId& session_storage_namespace_id,
    bool& consumed_user_gesture) {
  if (!frame->GetPage() || frame->GetPage()->Paused())
    return nullptr;

  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  if (!web_frame)
    return nullptr;

  NotifyPopupOpeningObservers();
  const AtomicString& frame_name =
      !EqualIgnoringASCIICase(name, "_blank") ? name : g_empty_atom;
  WebViewImpl* new_view =
      static_cast<WebViewImpl*>(web_frame->Client()->CreateNewWindow(
          WrappedResourceRequest(r.GetResourceRequest()), features, frame_name,
          static_cast<WebNavigationPolicy>(r.GetNavigationPolicy()),
          sandbox_flags, session_storage_namespace_id, consumed_user_gesture,
          r.Impression(), r.GetPictureInPictureWindowOptions(),
          r.GetRequestorBaseURL()));
  if (!new_view)
    return nullptr;
  return new_view->GetPage();
}

void ChromeClientImpl::InjectScrollbarGestureScroll(
    LocalFrame& local_frame,
    const gfx::Vector2dF& delta,
    ui::ScrollGranularity granularity,
    CompositorElementId scrollable_area_element_id,
    WebInputEvent::Type injected_type) {
  local_frame.GetWidgetForLocalRoot()->InjectScrollbarGestureScroll(
      delta, granularity, scrollable_area_element_id, injected_type);
}

void ChromeClientImpl::FinishScrollFocusedEditableIntoView(
    const gfx::RectF& caret_rect_in_root_frame,
    mojom::blink::ScrollIntoViewParamsPtr params) {
  DCHECK(web_view_);
  DCHECK(web_view_->MainFrameImpl());
  DCHECK(!web_view_->IsFencedFrameRoot());
  web_view_->FinishScrollFocusedEditableIntoView(caret_rect_in_root_frame,
                                                 std::move(params));
}

void ChromeClientImpl::SetOverscrollBehavior(
    LocalFrame& main_frame,
    const cc::OverscrollBehavior& overscroll_behavior) {
  DCHECK(main_frame.IsOutermostMainFrame());
  main_frame.GetWidgetForLocalRoot()->SetOverscrollBehavior(
      overscroll_behavior);
}

void ChromeClientImpl::Show(LocalFrame& frame,
                            LocalFrame& opener_frame,
                            NavigationPolicy navigation_policy,
                            bool user_gesture) {
  DCHECK(web_view_);
  const WebWindowFeatures& features = frame.GetPage()->GetWindowFeatures();
  gfx::Rect bounds(features.x, features.y, features.width, features.height);

  // The minimum size from popups opened from borderless apps differs from
  // normal apps. When window.open is called, display-mode for the new frame is
  // still undefined as the app hasn't loaded yet, thus opener frame is used.
  int minimum_size =
      navigation_policy == NavigationPolicy::kNavigationPolicyNewPopup &&
              DisplayModeIsBorderless(opener_frame)
          ? blink::kMinimumBorderlessWindowSize
          : blink::kMinimumWindowSize;

  // TODO(crbug.com/1515106): Refactor so that the limits only live browser-side
  // instead of now partly being duplicated browser-side and renderer side.
  const gfx::Rect rect_adjusted_for_minimum =
      AdjustWindowRectForMinimum(bounds, minimum_size);
  const gfx::Rect adjusted_rect = AdjustWindowRectForDisplay(
      rect_adjusted_for_minimum, frame, minimum_size);
  // Request the unadjusted rect if the browser may honor cross-screen bounds.
  // Permission state is not readily available, so adjusted bounds are clamped
  // to the same-screen, to retain legacy behavior of synchronous pending values
  // and to avoid exposing other screen details to frames without permission.
  // TODO(crbug.com/897300): Use permission state for better sync estimates or
  // store unadjusted pending window rects if that will not break many sites.
  web_view_->Show(opener_frame.GetLocalFrameToken(), navigation_policy,
                  rect_adjusted_for_minimum, adjusted_rect, user_gesture);
}

bool ChromeClientImpl::ShouldReportDetailedMessageForSourceAndSeverity(
    LocalFrame& local_frame,
    mojom::blink::ConsoleMessageLevel log_level,
    const String& url) {
  WebLocalFrameImpl* webframe =
      WebLocalFrameImpl::FromFrame(local_frame.LocalFrameRoot());
  return webframe && webframe->Client() &&
         webframe->Client()->ShouldReportDetailedMessageForSourceAndSeverity(
             log_level, url);
}

void ChromeClientImpl::AddMessageToConsole(LocalFrame* local_frame,
                                           mojom::ConsoleMessageSource source,
                                           mojom::ConsoleMessageLevel level,
                                           const String& message,
                                           unsigned line_number,
                                           const String& source_id,
                                           const String& stack_trace) {
  if (!message.IsNull()) {
    local_frame->GetLocalFrameHostRemote().DidAddMessageToConsole(
        level, message, static_cast<int32_t>(line_number), source_id,
        stack_trace);
  }

  WebLocalFrameImpl* frame = WebLocalFrameImpl::FromFrame(local_frame);
  if (frame && frame->Client()) {
    frame->Client()->DidAddMessageToConsole(
        WebConsoleMessage(static_cast<mojom::ConsoleMessageLevel>(level),
                          message),
        source_id, line_number, stack_trace);
  }
}

bool ChromeClientImpl::CanOpenBeforeUnloadConfirmPanel() {
  DCHECK(web_view_);
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

void ChromeClientImpl::CloseWindow() {
  DCHECK(web_view_);
  web_view_->CloseWindow();
}

bool ChromeClientImpl::OpenJavaScriptAlertDelegate(LocalFrame* frame,
                                                   const String& message) {
  NotifyPopupOpeningObservers();
  bool disable_suppression = false;
  if (frame && frame->GetDocument()) {
    disable_suppression = RuntimeEnabledFeatures::
        DisableDifferentOriginSubframeDialogSuppressionEnabled(
            frame->GetDocument()->GetExecutionContext());
  }
  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().RunModalAlertDialog(
      TruncateDialogMessage(message), disable_suppression);
  return true;
}

bool ChromeClientImpl::OpenJavaScriptConfirmDelegate(LocalFrame* frame,
                                                     const String& message) {
  NotifyPopupOpeningObservers();
  bool success = false;
  bool disable_suppression = false;
  if (frame && frame->GetDocument()) {
    disable_suppression = RuntimeEnabledFeatures::
        DisableDifferentOriginSubframeDialogSuppressionEnabled(
            frame->GetDocument()->GetExecutionContext());
  }
  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().RunModalConfirmDialog(
      TruncateDialogMessage(message), disable_suppression, &success);
  return success;
}

bool ChromeClientImpl::OpenJavaScriptPromptDelegate(LocalFrame* frame,
                                                    const String& message,
                                                    const String& default_value,
                                                    String& result) {
  NotifyPopupOpeningObservers();
  bool success = false;
  bool disable_suppression = false;
  if (frame && frame->GetDocument()) {
    disable_suppression = RuntimeEnabledFeatures::
        DisableDifferentOriginSubframeDialogSuppressionEnabled(
            frame->GetDocument()->GetExecutionContext());
  }
  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().RunModalPromptDialog(
      TruncateDialogMessage(message),
      default_value.IsNull() ? g_empty_string : default_value,
      disable_suppression, &success, &result);
  return success;
}
bool ChromeClientImpl::TabsToLinks() {
  DCHECK(web_view_);
  return web_view_->TabsToLinks();
}

void ChromeClientImpl::InvalidateContainer() {
  DCHECK(web_view_);
  web_view_->InvalidateContainer();
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
    widget->RequestAnimationAfterDelay(delay);
  }
}

gfx::Rect ChromeClientImpl::LocalRootToScreenDIPs(
    const gfx::Rect& rect_in_local_root,
    const LocalFrameView* frame_view) const {
  LocalFrame& frame = frame_view->GetFrame();

  WebFrameWidgetImpl* widget =
      WebLocalFrameImpl::FromFrame(frame)->LocalRootFrameWidget();

  gfx::Rect rect_in_widget;
  if (widget->ForTopMostMainFrame()) {
    rect_in_widget = frame.GetPage()->GetVisualViewport().RootFrameToViewport(
        rect_in_local_root);
  } else {
    // TODO(bokan): This method needs to account for the visual viewport
    // transform when in a non-top-most local frame root. Unfortunately, the
    // widget's ViewRect doesn't include the visual viewport so this cannot be
    // done from here yet. See: https://crbug.com/928825,
    // https://crbug.com/840944.
    rect_in_widget = rect_in_local_root;
  }

  gfx::Rect view_rect = widget->ViewRect();

  gfx::Rect screen_rect = widget->BlinkSpaceToEnclosedDIPs(rect_in_widget);
  screen_rect.Offset(view_rect.x(), view_rect.y());

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

  if (auto* widget = frame->GetWidgetForLocalRoot()) {
    return widget->DIPsToBlinkSpace(scalar_value);
  }
  return scalar_value;
}

const display::ScreenInfo& ChromeClientImpl::GetScreenInfo(
    LocalFrame& frame) const {
  return frame.GetWidgetForLocalRoot()->GetScreenInfo();
}

const display::ScreenInfos& ChromeClientImpl::GetScreenInfos(
    LocalFrame& frame) const {
  return frame.GetWidgetForLocalRoot()->GetScreenInfos();
}

float ChromeClientImpl::InputEventsScaleForEmulation() const {
  DCHECK(web_view_);
  return web_view_->GetDevToolsEmulator()->InputEventsScaleForEmulation();
}

void ChromeClientImpl::ContentsSizeChanged(LocalFrame* frame,
                                           const gfx::Size& size) const {
  DCHECK(web_view_);
  web_view_->DidChangeContentsSize();

  WebLocalFrameImpl* webframe = WebLocalFrameImpl::FromFrame(frame);
  webframe->DidChangeContentsSize(size);
}

bool ChromeClientImpl::DoubleTapToZoomEnabled() const {
  DCHECK(web_view_);
  return web_view_->SettingsImpl()->DoubleTapToZoomEnabled();
}

void ChromeClientImpl::EnablePreferredSizeChangedMode() {
  DCHECK(web_view_);
  web_view_->EnablePreferredSizeChangedMode();
}

void ChromeClientImpl::ZoomToFindInPageRect(
    const gfx::Rect& rect_in_root_frame) {
  DCHECK(web_view_);
  web_view_->ZoomToFindInPageRect(rect_in_root_frame);
}

void ChromeClientImpl::PageScaleFactorChanged() const {
  DCHECK(web_view_);
  web_view_->PageScaleFactorChanged();
}

void ChromeClientImpl::OutermostMainFrameScrollOffsetChanged() const {
  web_view_->OutermostMainFrameScrollOffsetChanged();
}

float ChromeClientImpl::ClampPageScaleFactorToLimits(float scale) const {
  DCHECK(web_view_);
  return web_view_->ClampPageScaleFactorToLimits(scale);
}

void ChromeClientImpl::ResizeAfterLayout() const {
  DCHECK(web_view_);
  web_view_->ResizeAfterLayout();
}

void ChromeClientImpl::MainFrameLayoutUpdated() const {
  DCHECK(web_view_);
  web_view_->MainFrameLayoutUpdated();
}

void ChromeClientImpl::ShowMouseOverURL(const HitTestResult& result) {
  DCHECK(web_view_);
  if (!web_view_->Client())
    return;

  KURL url;

  // Ignore URL if hitTest include scrollbar since we might have both a
  // scrollbar and an element in the case of overlay scrollbars.
  if (!result.GetScrollbar()) {
    // Find out if the mouse is over a link, and if so, let our UI know...
    if (result.IsLiveLink() && !result.AbsoluteLinkURL().GetString().empty()) {
      url = result.AbsoluteLinkURL();
    } else if (result.InnerNode() &&
               (IsA<HTMLObjectElement>(*result.InnerNode()) ||
                IsA<HTMLEmbedElement>(*result.InnerNode()))) {
      if (auto* embedded = DynamicTo<LayoutEmbeddedContent>(
              result.InnerNode()->GetLayoutObject())) {
        if (WebPluginContainerImpl* plugin_view = embedded->Plugin()) {
          url = plugin_view->Plugin()->LinkAtPosition(
              result.RoundedPointInInnerNodeFrame());
        }
      }
    }
  }

  web_view_->SetMouseOverURL(url);
}

void ChromeClientImpl::UpdateTooltipUnderCursor(LocalFrame& frame,
                                                const String& tooltip_text,
                                                TextDirection dir) {
  WebFrameWidgetImpl* widget =
      WebLocalFrameImpl::FromFrame(frame)->LocalRootFrameWidget();
  if (!tooltip_text.empty()) {
    widget->UpdateTooltipUnderCursor(tooltip_text, dir);
    did_request_non_empty_tool_tip_ = true;
  } else if (did_request_non_empty_tool_tip_) {
    // WebFrameWidgetImpl::UpdateTooltipUnderCursor will send a Mojo message via
    // mojom::blink::WidgetHost. We'd like to reduce the number of
    // UpdateTooltipUnderCursor calls.
    widget->UpdateTooltipUnderCursor(tooltip_text, dir);
    did_request_non_empty_tool_tip_ = false;
  }
}

void ChromeClientImpl::UpdateTooltipFromKeyboard(LocalFrame& frame,
                                                 const String& tooltip_text,
                                                 TextDirection dir,
                                                 const gfx::Rect& bounds) {
  if (!RuntimeEnabledFeatures::KeyboardAccessibleTooltipEnabled())
    return;

  WebLocalFrameImpl::FromFrame(frame)
      ->LocalRootFrameWidget()
      ->UpdateTooltipFromKeyboard(tooltip_text, dir, bounds);
}

void ChromeClientImpl::ClearKeyboardTriggeredTooltip(LocalFrame& frame) {
  if (!RuntimeEnabledFeatures::KeyboardAccessibleTooltipEnabled())
    return;

  WebLocalFrameImpl::FromFrame(frame)
      ->LocalRootFrameWidget()
      ->ClearKeyboardTriggeredTooltip();
}

void ChromeClientImpl::DispatchViewportPropertiesDidChange(
    const ViewportDescription& description) const {
  DCHECK(web_view_);
  web_view_->UpdatePageDefinedViewportConstraints(description);
}

void ChromeClientImpl::PrintDelegate(LocalFrame* frame) {
  NotifyPopupOpeningObservers();
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  web_frame->Client()->ScriptedPrint();
}

ColorChooser* ChromeClientImpl::OpenColorChooser(
    LocalFrame* frame,
    ColorChooserClient* chooser_client,
    const Color&) {
  NotifyPopupOpeningObservers();
  ColorChooserUIController* controller = nullptr;

  if (RuntimeEnabledFeatures::PagePopupEnabled()) {
    controller = MakeGarbageCollected<ColorChooserPopupUIController>(
        frame, this, chooser_client);
  } else {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    NOTREACHED_IN_MIGRATION()
        << "Page popups should be enabled on all but Android or iOS";
#endif
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
  return external_date_time_chooser_.Get();
}

ExternalDateTimeChooser*
ChromeClientImpl::GetExternalDateTimeChooserForTesting() {
  return external_date_time_chooser_.Get();
}

void ChromeClientImpl::OpenFileChooser(
    LocalFrame* frame,
    scoped_refptr<FileChooser> file_chooser) {
  NotifyPopupOpeningObservers();

  static const wtf_size_t kMaximumPendingFileChooseRequests = 4;
  if (file_chooser_queue_.size() > kMaximumPendingFileChooseRequests) {
    // This check prevents too many file choose requests from getting
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
  if (!file_chooser_queue_.empty() &&
      file_chooser_queue_.front().get() != &chooser) {
    // This function is called even if |chooser| wasn't stored in
    // file_chooser_queue_.
    return;
  }
  file_chooser_queue_.EraseAt(0);
  if (file_chooser_queue_.empty())
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

#if BUILDFLAG(IS_MAC)
  DCHECK(web_view_);
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
  if (WebFrameWidgetImpl* widget =
          WebLocalFrameImpl::FromFrame(local_frame)->LocalRootFrameWidget())
    widget->AutoscrollStart(viewport_point);
}

void ChromeClientImpl::AutoscrollFling(const gfx::Vector2dF& velocity,
                                       LocalFrame* local_frame) {
  // TODO(dcheng): Why is this null check necessary?
  if (WebFrameWidgetImpl* widget =
          WebLocalFrameImpl::FromFrame(local_frame)->LocalRootFrameWidget())
    widget->AutoscrollFling(velocity);
}

void ChromeClientImpl::AutoscrollEnd(LocalFrame* local_frame) {
  // TODO(dcheng): Why is this null check necessary?
  if (WebFrameWidgetImpl* widget =
          WebLocalFrameImpl::FromFrame(local_frame)->LocalRootFrameWidget())
    widget->AutoscrollEnd();
}

String ChromeClientImpl::AcceptLanguages() {
  DCHECK(web_view_);
  return String::FromUTF8(web_view_->GetRendererPreferences().accept_languages);
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

cc::AnimationHost* ChromeClientImpl::GetCompositorAnimationHost(
    LocalFrame& local_frame) const {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(local_frame);
  if (!web_frame || web_frame->IsProvisional()) {
    return nullptr;
  }
  FrameWidget* widget = local_frame.GetWidgetForLocalRoot();
  DCHECK(widget);
  return widget->AnimationHost();
}

cc::AnimationTimeline* ChromeClientImpl::GetScrollAnimationTimeline(
    LocalFrame& local_frame) const {
  FrameWidget* widget = local_frame.GetWidgetForLocalRoot();
  DCHECK(widget);
  return widget->ScrollAnimationTimeline();
}

void ChromeClientImpl::EnterFullscreen(LocalFrame& frame,
                                       const FullscreenOptions* options,
                                       FullscreenRequestType request_type) {
  DCHECK(web_view_);
  web_view_->EnterFullscreen(frame, options, request_type);
}

void ChromeClientImpl::ExitFullscreen(LocalFrame& frame) {
  DCHECK(web_view_);
  web_view_->ExitFullscreen(frame);
}

void ChromeClientImpl::FullscreenElementChanged(
    Element* old_element,
    Element* new_element,
    const FullscreenOptions* options,
    FullscreenRequestType request_type) {
  DCHECK(web_view_);
  web_view_->FullscreenElementChanged(old_element, new_element, options,
                                      request_type);
}

void ChromeClientImpl::AnimateDoubleTapZoom(const gfx::Point& point,
                                            const gfx::Rect& rect) {
  DCHECK(web_view_);
  web_view_->AnimateDoubleTapZoom(point, rect);
}

bool ChromeClientImpl::HasOpenedPopup() const {
  DCHECK(web_view_);
  return web_view_->HasOpenedPopup();
}

PopupMenu* ChromeClientImpl::OpenPopupMenu(LocalFrame& frame,
                                           HTMLSelectElement& select) {
  NotifyPopupOpeningObservers();

  if (WebViewImpl::UseExternalPopupMenus()) {
    return MakeGarbageCollected<ExternalPopupMenu>(frame, select);
  }

  DCHECK(RuntimeEnabledFeatures::PagePopupEnabled());
  return MakeGarbageCollected<InternalPopupMenu>(this, select);
}

PagePopup* ChromeClientImpl::OpenPagePopup(PagePopupClient* client) {
  DCHECK(web_view_);
  return web_view_->OpenPagePopup(client);
}

void ChromeClientImpl::ClosePagePopup(PagePopup* popup) {
  DCHECK(web_view_);
  web_view_->ClosePagePopup(popup);
}

DOMWindow* ChromeClientImpl::PagePopupWindowForTesting() const {
  DCHECK(web_view_);
  return web_view_->PagePopupWindow();
}

void ChromeClientImpl::SetBrowserControlsState(float top_height,
                                               float bottom_height,
                                               bool shrinks_layout) {
  DCHECK(web_view_);
  DCHECK(web_view_->MainFrameWidget());
  gfx::Size size = web_view_->MainFrameWidget()->Size();
  if (shrinks_layout)
    size -= gfx::Size(0, top_height + bottom_height);

  web_view_->ResizeWithBrowserControls(size, top_height, bottom_height,
                                       shrinks_layout);
}

void ChromeClientImpl::SetBrowserControlsShownRatio(float top_ratio,
                                                    float bottom_ratio) {
  DCHECK(web_view_);
  web_view_->GetBrowserControls().SetShownRatio(top_ratio, bottom_ratio);
}

bool ChromeClientImpl::ShouldOpenUIElementDuringPageDismissal(
    LocalFrame& frame,
    UIElementType ui_element_type,
    const String& dialog_message,
    Document::PageDismissalType dismissal_type) const {
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
  return frame->GetWidgetForLocalRoot()->GetFrameSinkId();
}

void ChromeClientImpl::RequestDecode(LocalFrame* frame,
                                     const PaintImage& image,
                                     base::OnceCallback<void(bool)> callback) {
  FrameWidget* widget = frame->GetWidgetForLocalRoot();
  widget->RequestDecode(image, std::move(callback));
}

void ChromeClientImpl::NotifyPresentationTime(LocalFrame& frame,
                                              ReportTimeCallback callback) {
  FrameWidget* widget = frame.GetWidgetForLocalRoot();
  if (!widget)
    return;
  widget->NotifyPresentationTimeInBlink(
      ConvertToBaseOnceCallback(std::move(callback)));
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
  DCHECK(web_view_);
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

void ChromeClientImpl::BeginLifecycleUpdates(LocalFrame& main_frame) {
  DCHECK(main_frame.IsMainFrame());
  DCHECK(web_view_);
  web_view_->StopDeferringMainFrameUpdate();
}

void ChromeClientImpl::RegisterForCommitObservation(CommitObserver* observer) {
  commit_observers_.insert(observer);
}

void ChromeClientImpl::UnregisterFromCommitObservation(
    CommitObserver* observer) {
  commit_observers_.erase(observer);
}

void ChromeClientImpl::WillCommitCompositorFrame() {
  // Make a copy since callbacks may modify the set as we're iterating it.
  auto observers = commit_observers_;
  for (auto& observer : observers)
    observer->WillCommitCompositorFrame();
}

std::unique_ptr<cc::ScopedPauseRendering> ChromeClientImpl::PauseRendering(
    LocalFrame& frame) {
  // If |frame| corresponds to an iframe this implies a transition in an iframe
  // will pause rendering for the all ancestor frames (including the main frame)
  // hosted in this process.
  DCHECK(frame.IsLocalRoot());
  return WebLocalFrameImpl::FromFrame(frame)
      ->FrameWidgetImpl()
      ->PauseRendering();
}

std::optional<int> ChromeClientImpl::GetMaxRenderBufferBounds(
    LocalFrame& frame) const {
  return WebLocalFrameImpl::FromFrame(frame)
      ->LocalRootFrameWidget()
      ->GetMaxRenderBufferBounds();
}

bool ChromeClientImpl::StartDeferringCommits(LocalFrame& main_frame,
                                             base::TimeDelta timeout,
                                             cc::PaintHoldingReason reason) {
  DCHECK(main_frame.IsLocalRoot());
  return WebLocalFrameImpl::FromFrame(main_frame)
      ->FrameWidgetImpl()
      ->StartDeferringCommits(timeout, reason);
}

void ChromeClientImpl::StopDeferringCommits(
    LocalFrame& main_frame,
    cc::PaintHoldingCommitTrigger trigger) {
  DCHECK(main_frame.IsLocalRoot());
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
  WebFrameWidgetImpl* widget = web_frame->LocalRootFrameWidget();
  if (!widget)
    return;

  widget->SetNeedsLowLatencyInput(needs_low_latency);
}

void ChromeClientImpl::SetNeedsUnbufferedInputForDebugger(LocalFrame* frame,
                                                          bool unbuffered) {
  DCHECK(frame);
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetImpl* widget = web_frame->LocalRootFrameWidget();
  if (!widget)
    return;

  widget->SetNeedsUnbufferedInputForDebugger(unbuffered);
}

void ChromeClientImpl::RequestUnbufferedInputEvents(LocalFrame* frame) {
  DCHECK(frame);
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetImpl* widget = web_frame->LocalRootFrameWidget();
  if (!widget)
    return;

  widget->RequestUnbufferedInputEvents();
}

void ChromeClientImpl::SetTouchAction(LocalFrame* frame,
                                      TouchAction touch_action) {
  DCHECK(frame);
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetImpl* widget = web_frame->LocalRootFrameWidget();
  if (!widget)
    return;

  widget->ProcessTouchAction(touch_action);
}

void ChromeClientImpl::SetPanAction(LocalFrame* frame,
                                    mojom::blink::PanAction pan_action) {
  DCHECK(frame);
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  WebFrameWidgetImpl* widget = web_frame->LocalRootFrameWidget();
  if (!widget)
    return;

  widget->SetPanAction(pan_action);
}

void ChromeClientImpl::DidChangeFormRelatedElementDynamically(
    LocalFrame* frame,
    HTMLElement* element,
    WebFormRelatedChangeType form_related_change) {
  if (auto* fill_client = AutofillClientFromFrame(frame)) {
    fill_client->DidChangeFormRelatedElementDynamically(element,
                                                        form_related_change);
  }
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
    // The resource coordinator is not available in some tests.
    if (auto* rc = doc.GetResourceCoordinator())
      rc->SetHadFormInteraction();
  }
}

void ChromeClientImpl::DidClearValueInTextField(
    HTMLFormControlElement& element) {
  Document& doc = element.GetDocument();
  if (auto* fill_client = AutofillClientFromFrame(doc.GetFrame())) {
    fill_client->TextFieldCleared(WebFormControlElement(&element));
  }
}

void ChromeClientImpl::DidUserChangeContentEditableContent(Element& element) {
  Document& doc = element.GetDocument();
  // Selecting the focused element as we are only interested in changes made by
  // the user. We assume the user must focus the field to type into it.
  WebElement focused_element = doc.FocusedElement();
  // If element argument is not the focused element we can assume the user
  // was not typing (this covers cases like element.innerText = 'foo').
  // Value changes caused by |document.execCommand| calls should not be
  // interpreted as a user action. See https://crbug.com/764760.
  if (!element.IsFocusedElementInDocument() || doc.IsRunningExecCommand()) {
    return;
  }
  if (auto* fill_client = AutofillClientFromFrame(doc.GetFrame())) {
    fill_client->ContentEditableDidChange(focused_element);
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
  if (auto* fill_client = AutofillClientFromFrame(doc.GetFrame())) {
    fill_client->SelectFieldOptionsChanged(WebFormControlElement(&element));
  }
}

void ChromeClientImpl::AjaxSucceeded(LocalFrame* frame) {
  if (auto* fill_client = AutofillClientFromFrame(frame))
    fill_client->AjaxSucceeded();
}

void ChromeClientImpl::JavaScriptChangedValue(HTMLFormControlElement& element,
                                              const String& old_value,
                                              bool was_autofilled) {
  Document& doc = element.GetDocument();
  if (auto* fill_client = AutofillClientFromFrame(doc.GetFrame())) {
    fill_client->JavaScriptChangedValue(WebFormControlElement(&element),
                                        old_value, was_autofilled);
  }
}

gfx::Transform ChromeClientImpl::GetDeviceEmulationTransform() const {
  DCHECK(web_view_);
  return web_view_->GetDeviceEmulationTransform();
}

void ChromeClientImpl::DidUpdateBrowserControls() const {
  DCHECK(web_view_);
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

gfx::Vector2dF ChromeClientImpl::ElasticOverscroll() const {
  DCHECK(web_view_);
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
  DCHECK(web_view_);
  web_view_->TextAutosizerPageInfoChanged(page_info);
}

void ChromeClientImpl::DocumentDetached(Document& document) {
  for (auto& it : file_chooser_queue_) {
    if (it->FrameOrNull() == document.GetFrame())
      it->DisconnectClient();
  }
}

double ChromeClientImpl::UserZoomFactor(LocalFrame* frame) const {
  DCHECK(web_view_);
  return ZoomLevelToZoomFactor(
      WebLocalFrameImpl::FromFrame(frame->LocalFrameRoot())
          ->FrameWidgetImpl()
          ->GetZoomLevel());
}

void ChromeClientImpl::SetDelegatedInkMetadata(
    LocalFrame* frame,
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  frame->GetWidgetForLocalRoot()->SetDelegatedInkMetadata(std::move(metadata));
}

void ChromeClientImpl::FormElementReset(HTMLFormElement& element) {
  Document& doc = element.GetDocument();
  if (auto* fill_client = AutofillClientFromFrame(doc.GetFrame()))
    fill_client->FormElementReset(WebFormElement(&element));
}

void ChromeClientImpl::PasswordFieldReset(HTMLInputElement& element) {
  if (auto* fill_client =
          AutofillClientFromFrame(element.GetDocument().GetFrame())) {
    fill_client->PasswordFieldReset(WebInputElement(&element));
  }
}

float ChromeClientImpl::ZoomFactorForViewportLayout() {
  DCHECK(web_view_);
  return web_view_->ZoomFactorForViewportLayout();
}

gfx::Rect ChromeClientImpl::AdjustWindowRectForMinimum(
    const gfx::Rect& pending_rect,
    int minimum_size) {
  gfx::Rect window = pending_rect;

  // Let size 0 pass through, since that indicates default size, not minimum
  // size.
  if (window.width()) {
    window.set_width(std::max(minimum_size, window.width()));
  }
  if (window.height()) {
    window.set_height(std::max(minimum_size, window.height()));
  }
  return window;
}

gfx::Rect ChromeClientImpl::AdjustWindowRectForDisplay(
    const gfx::Rect& pending_rect,
    LocalFrame& frame,
    int minimum_size) {
  DCHECK_EQ(pending_rect,
            AdjustWindowRectForMinimum(pending_rect, minimum_size))
      << "Make sure to first use AdjustWindowRectForMinimum to adjust "
         "pending_rect for minimum.";
  gfx::Rect screen = GetScreenInfo(frame).available_rect;
  gfx::Rect window = pending_rect;

  gfx::Size size_for_constraining_move = MinimumWindowSize();
  // Let size 0 pass through, since that indicates default size, not minimum
  // size.
  if (window.width()) {
    window.set_width(std::min(window.width(), screen.width()));
    size_for_constraining_move.set_width(window.width());
  }
  if (window.height()) {
    window.set_height(std::min(window.height(), screen.height()));
    size_for_constraining_move.set_height(window.height());
  }

  // Constrain the window position within the valid screen area.
  window.set_x(
      std::max(screen.x(),
               std::min(window.x(),
                        screen.right() - size_for_constraining_move.width())));
  window.set_y(std::max(
      screen.y(),
      std::min(window.y(),
               screen.bottom() - size_for_constraining_move.height())));

  // Coarsely measure whether coordinates may be requesting another screen.
  if (!screen.Contains(window)) {
    UseCounter::Count(frame.DomWindow(),
                      WebFeature::kDOMWindowSetWindowRectCrossScreen);
  }

  return window;
}

}  // namespace blink
