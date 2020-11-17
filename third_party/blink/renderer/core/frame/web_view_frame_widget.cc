// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"

#include "base/debug/crash_logging.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/events/ui_event_with_key_state.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/screen_metrics_emulator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {

WebViewFrameWidget::WebViewFrameWidget(
    util::PassKey<WebFrameWidget>,
    WebWidgetClient& client,
    WebViewImpl& web_view,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const viz::FrameSinkId& frame_sink_id,
    bool is_for_nested_main_frame,
    bool hidden,
    bool never_composited)
    : WebFrameWidgetBase(client,
                         std::move(frame_widget_host),
                         std::move(frame_widget),
                         std::move(widget_host),
                         std::move(widget),
                         task_runner,
                         frame_sink_id,
                         hidden,
                         never_composited,
                         /*is_for_child_local_root=*/false),
      web_view_(&web_view),
      is_for_nested_main_frame_(is_for_nested_main_frame),
      self_keep_alive_(PERSISTENT_FROM_HERE, this) {
  web_view_->SetMainFrameViewWidget(this);
}

WebViewFrameWidget::~WebViewFrameWidget() = default;

void WebViewFrameWidget::Close(
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner) {
  GetPage()->WillCloseAnimationHost(nullptr);
  // Closing the WebViewFrameWidget happens in response to the local main frame
  // being detached from the Page/WebViewImpl.
  web_view_->SetMainFrameViewWidget(nullptr);
  web_view_ = nullptr;
  WebFrameWidgetBase::Close(std::move(cleanup_runner));
  self_keep_alive_.Clear();
}

gfx::Size WebViewFrameWidget::Size() {
  return gfx::Size(web_view_->Size());
}

void WebViewFrameWidget::Resize(const gfx::Size& size) {
  size_ = size;
  web_view_->Resize(size);
}

void WebViewFrameWidget::UpdateLifecycle(WebLifecycleUpdate requested_update,
                                         DocumentUpdateReason reason) {
  web_view_->UpdateLifecycle(requested_update, reason);
}

void WebViewFrameWidget::ApplyViewportChanges(
    const ApplyViewportChangesArgs& args) {
  web_view_->ApplyViewportChanges(args);
}

void WebViewFrameWidget::RecordManipulationTypeCounts(
    cc::ManipulationInfo info) {
  web_view_->RecordManipulationTypeCounts(info);
}

void WebViewFrameWidget::MouseCaptureLost() {
  mouse_capture_element_ = nullptr;
  web_view_->MouseCaptureLost();
}

void WebViewFrameWidget::FocusChanged(bool enable) {
  web_view_->SetFocus(enable);
}

bool WebViewFrameWidget::ShouldHandleImeEvents() {
  return HasFocus();
}

void WebViewFrameWidget::SetWindowRect(const gfx::Rect& window_rect) {
  if (synchronous_resize_mode_for_testing_) {
    // This is a web-test-only path. At one point, it was planned to be
    // removed. See https://crbug.com/309760.
    SetWindowRectSynchronously(window_rect);
    return;
  }
  View()->SetWindowRect(window_rect);
}

bool WebViewFrameWidget::ScrollFocusedEditableElementIntoView() {
  return web_view_->ScrollFocusedEditableElementIntoView();
}

void WebViewFrameWidget::SetRootLayer(scoped_refptr<cc::Layer> root_layer) {
  if (!web_view_->does_composite()) {
    DCHECK(!root_layer);
    return;
  }
  cc::LayerTreeHost* layer_tree_host = widget_base_->LayerTreeHost();
  layer_tree_host->SetRootLayer(root_layer);
  web_view_->DidChangeRootLayer(!!root_layer);
}

void WebViewFrameWidget::ZoomToFindInPageRect(
    const WebRect& rect_in_root_frame) {
  web_view_->ZoomToFindInPageRect(rect_in_root_frame);
}

void WebViewFrameWidget::HandleMouseLeave(LocalFrame& main_frame,
                                          const WebMouseEvent& event) {
  web_view_->SetMouseOverURL(WebURL());
  PageWidgetEventHandler::HandleMouseLeave(main_frame, event);
}

WebInputEventResult WebViewFrameWidget::HandleGestureEvent(
    const WebGestureEvent& event) {
  if (!web_view_->Client() || !web_view_->Client()->CanHandleGestureEvent()) {
    return WebInputEventResult::kNotHandled;
  }

  // TODO(https://crbug.com/1148346): We need to figure out why MainFrameImpl is
  // null but LocalRootImpl isn't.
  CHECK(LocalRootImpl());
  if (!web_view_->MainFrameImpl())
    return WebInputEventResult::kNotHandled;

  WebInputEventResult event_result = WebInputEventResult::kNotHandled;
  bool event_cancelled = false;  // for disambiguation

  // Fling events are not sent to the renderer.
  CHECK(event.GetType() != WebInputEvent::Type::kGestureFlingStart);
  CHECK(event.GetType() != WebInputEvent::Type::kGestureFlingCancel);

  WebGestureEvent scaled_event = TransformWebGestureEvent(
      web_view_->MainFrameImpl()->GetFrameView(), event);

  // Special handling for double tap and scroll events as we don't want to
  // hit test for them.
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureDoubleTap:
      if (web_view_->SettingsImpl()->DoubleTapToZoomEnabled() &&
          web_view_->MinimumPageScaleFactor() !=
              web_view_->MaximumPageScaleFactor()) {
        if (auto* main_frame = web_view_->MainFrameImpl()) {
          IntPoint pos_in_root_frame =
              FlooredIntPoint(scaled_event.PositionInRootFrame());
          WebRect block_bounds =
              main_frame->FrameWidgetImpl()->ComputeBlockBound(
                  pos_in_root_frame, false);
          web_view_->AnimateDoubleTapZoom(pos_in_root_frame, block_bounds);
        }
      }
      event_result = WebInputEventResult::kHandledSystem;
      DidHandleGestureEvent(event, event_cancelled);
      return event_result;
    case WebInputEvent::Type::kGestureScrollBegin:
    case WebInputEvent::Type::kGestureScrollEnd:
    case WebInputEvent::Type::kGestureScrollUpdate:
      // Scrolling-related gesture events invoke EventHandler recursively for
      // each frame down the chain, doing a single-frame hit-test per frame.
      // This matches handleWheelEvent.  Perhaps we could simplify things by
      // rewriting scroll handling to work inner frame out, and then unify with
      // other gesture events.
      event_result = web_view_->MainFrameImpl()
                         ->GetFrame()
                         ->GetEventHandler()
                         .HandleGestureScrollEvent(scaled_event);
      DidHandleGestureEvent(event, event_cancelled);
      return event_result;
    default:
      break;
  }

  // Hit test across all frames and do touch adjustment as necessary for the
  // event type.
  GestureEventWithHitTestResults targeted_event =
      GetPage()
          ->DeprecatedLocalMainFrame()
          ->GetEventHandler()
          .TargetGestureEvent(scaled_event);

  // Handle link highlighting outside the main switch to avoid getting lost in
  // the complicated set of cases handled below.
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureShowPress:
      // Queue a highlight animation, then hand off to regular handler.
      web_view_->EnableTapHighlightAtPoint(targeted_event);
      break;
    case WebInputEvent::Type::kGestureTapCancel:
    case WebInputEvent::Type::kGestureTap:
    case WebInputEvent::Type::kGestureLongPress:
      GetPage()->GetLinkHighlight().StartHighlightAnimationIfNeeded();
      break;
    default:
      break;
  }

  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureTap: {
      {
        ContextMenuAllowedScope scope;
        event_result = web_view_->MainFrameImpl()
                           ->GetFrame()
                           ->GetEventHandler()
                           .HandleGestureEvent(targeted_event);
      }

      if (web_view_->GetPagePopup() && last_hidden_page_popup_ &&
          web_view_->GetPagePopup()->HasSamePopupClient(
              last_hidden_page_popup_.get())) {
        // The tap triggered a page popup that is the same as the one we just
        // closed. It needs to be closed.
        web_view_->CancelPagePopup();
      }
      // Don't have this value persist outside of a single tap gesture, plus
      // we're done with it now.
      last_hidden_page_popup_ = nullptr;
      break;
    }
    case WebInputEvent::Type::kGestureTwoFingerTap:
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureLongTap: {
      if (!web_view_->MainFrameImpl() ||
          !web_view_->MainFrameImpl()->GetFrameView())
        break;

      if (event.GetType() == WebInputEvent::Type::kGestureLongTap) {
        if (LocalFrame* inner_frame =
                targeted_event.GetHitTestResult().InnerNodeFrame()) {
          if (!inner_frame->GetEventHandler().LongTapShouldInvokeContextMenu())
            break;
        } else if (!web_view_->MainFrameImpl()
                        ->GetFrame()
                        ->GetEventHandler()
                        .LongTapShouldInvokeContextMenu()) {
          break;
        }
      }

      GetPage()->GetContextMenuController().ClearContextMenu();
      {
        ContextMenuAllowedScope scope;
        event_result = web_view_->MainFrameImpl()
                           ->GetFrame()
                           ->GetEventHandler()
                           .HandleGestureEvent(targeted_event);
      }

      break;
    }
    case WebInputEvent::Type::kGestureTapDown: {
      // Touch pinch zoom and scroll on the page (outside of a popup) must hide
      // the popup. In case of a touch scroll or pinch zoom, this function is
      // called with GestureTapDown rather than a GSB/GSU/GSE or GPB/GPU/GPE.
      // When we close a popup because of a GestureTapDown, we also save it so
      // we can prevent the following GestureTap from immediately reopening the
      // same popup.
      // This value should not persist outside of a gesture, so is cleared by
      // GestureTap (where it is used) and by GestureCancel.
      last_hidden_page_popup_ = web_view_->GetPagePopup();
      web_view_->CancelPagePopup();
      event_result = web_view_->MainFrameImpl()
                         ->GetFrame()
                         ->GetEventHandler()
                         .HandleGestureEvent(targeted_event);
      break;
    }
    case WebInputEvent::Type::kGestureTapCancel: {
      // Don't have this value persist outside of a single tap gesture.
      last_hidden_page_popup_ = nullptr;
      event_result = web_view_->MainFrameImpl()
                         ->GetFrame()
                         ->GetEventHandler()
                         .HandleGestureEvent(targeted_event);
      break;
    }
    case WebInputEvent::Type::kGestureShowPress: {
      event_result = web_view_->MainFrameImpl()
                         ->GetFrame()
                         ->GetEventHandler()
                         .HandleGestureEvent(targeted_event);
      break;
    }
    case WebInputEvent::Type::kGestureTapUnconfirmed: {
      event_result = web_view_->MainFrameImpl()
                         ->GetFrame()
                         ->GetEventHandler()
                         .HandleGestureEvent(targeted_event);
      break;
    }
    default: {
      NOTREACHED();
    }
  }
  DidHandleGestureEvent(event, event_cancelled);
  return event_result;
}

LocalFrameView* WebViewFrameWidget::GetLocalFrameViewForAnimationScrolling() {
  // Scrolling for the root frame is special we need to pass null indicating
  // we are at the top of the tree when setting up the Animation. Which will
  // cause ownership of the timeline and animation host.
  // See ScrollingCoordinator::AnimationHostInitialized.
  return nullptr;
}

void WebViewFrameWidget::SetAutoResizeMode(bool auto_resize,
                                           const gfx::Size& min_window_size,
                                           const gfx::Size& max_window_size,
                                           float device_scale_factor) {
  if (auto_resize) {
    if (!Platform::Current()->IsUseZoomForDSFEnabled())
      device_scale_factor = 1.f;
    web_view_->EnableAutoResizeMode(
        gfx::ScaleToCeiledSize(min_window_size, device_scale_factor),
        gfx::ScaleToCeiledSize(max_window_size, device_scale_factor));
  } else if (web_view_->AutoResizeMode()) {
    web_view_->DisableAutoResizeMode();
  }
}

void WebViewFrameWidget::SetIsNestedMainFrameWidget(bool is_nested) {
  is_for_nested_main_frame_ = is_nested;
}

void WebViewFrameWidget::SetPageScaleStateAndLimits(
    float page_scale_factor,
    bool is_pinch_gesture_active,
    float minimum,
    float maximum) {
  WebFrameWidgetBase::SetPageScaleStateAndLimits(
      page_scale_factor, is_pinch_gesture_active, minimum, maximum);

  // If page scale hasn't changed, then just return without notifying
  // the remote frames.
  if (page_scale_factor == page_scale_factor_in_mainframe_ &&
      is_pinch_gesture_active == is_pinch_gesture_active_in_mainframe_) {
    return;
  }

  NotifyPageScaleFactorChanged(page_scale_factor, is_pinch_gesture_active);
}

void WebViewFrameWidget::DidAutoResize(const gfx::Size& size) {
  gfx::Size size_in_dips = widget_base_->BlinkSpaceToFlooredDIPs(size);
  size_ = size;

  if (synchronous_resize_mode_for_testing_) {
    gfx::Rect new_pos(widget_base_->WindowRect());
    new_pos.set_size(size_in_dips);
    SetScreenRects(new_pos, new_pos);
  }

  // TODO(ccameron): Note that this destroys any information differentiating
  // |size| from the compositor's viewport size.
  gfx::Rect size_with_dsf = gfx::Rect(gfx::ScaleToCeiledSize(
      gfx::Rect(size_in_dips).size(),
      widget_base_->GetScreenInfo().device_scale_factor));
  widget_base_->LayerTreeHost()->RequestNewLocalSurfaceId();
  widget_base_->UpdateCompositorViewportRect(size_with_dsf);
}

void WebViewFrameWidget::SetDeviceColorSpaceForTesting(
    const gfx::ColorSpace& color_space) {
  // We are changing the device color space from the renderer, so allocate a
  // new viz::LocalSurfaceId to avoid surface invariants violations in tests.
  widget_base_->LayerTreeHost()->RequestNewLocalSurfaceId();

  blink::ScreenInfo info = widget_base_->GetScreenInfo();
  info.display_color_spaces = gfx::DisplayColorSpaces(color_space);
  widget_base_->UpdateScreenInfo(info);
}

void WebViewFrameWidget::RunPaintBenchmark(int repeat_count,
                                           cc::PaintBenchmarkResult& result) {
  web_view_->RunPaintBenchmark(repeat_count, result);
}

void WebViewFrameWidget::SetWindowRectSynchronouslyForTesting(
    const gfx::Rect& new_window_rect) {
  SetWindowRectSynchronously(new_window_rect);
}

void WebViewFrameWidget::SetWindowRectSynchronously(
    const gfx::Rect& new_window_rect) {
  // This method is only call in tests, and it applies the |new_window_rect| to
  // all three of:
  // a) widget size (in |size_|)
  // b) blink viewport (in |visible_viewport_size_|)
  // c) compositor viewport (in cc::LayerTreeHost)
  // Normally the browser controls these three things independently, but this is
  // used in tests to control the size from the renderer.

  // We are resizing the window from the renderer, so allocate a new
  // viz::LocalSurfaceId to avoid surface invariants violations in tests.
  widget_base_->LayerTreeHost()->RequestNewLocalSurfaceId();

  gfx::Rect compositor_viewport_pixel_rect(gfx::ScaleToCeiledSize(
      new_window_rect.size(),
      widget_base_->GetScreenInfo().device_scale_factor));
  widget_base_->UpdateSurfaceAndScreenInfo(
      widget_base_->local_surface_id_from_parent(),
      compositor_viewport_pixel_rect, widget_base_->GetScreenInfo());

  Resize(new_window_rect.size());
  widget_base_->SetScreenRects(new_window_rect, new_window_rect);
}

void WebViewFrameWidget::UseSynchronousResizeModeForTesting(bool enable) {
  synchronous_resize_mode_for_testing_ = enable;
}

gfx::Size WebViewFrameWidget::DIPsToCeiledBlinkSpace(const gfx::Size& size) {
  return widget_base_->DIPsToCeiledBlinkSpace(size);
}

void WebViewFrameWidget::ApplyVisualPropertiesSizing(
    const VisualProperties& visual_properties) {
  if (size_ !=
      widget_base_->DIPsToCeiledBlinkSpace(visual_properties.new_size)) {
    // Only hide popups when the size changes. Eg https://crbug.com/761908.
    web_view_->CancelPagePopup();
  }

  if (auto* device_emulator = DeviceEmulator()) {
    device_emulator->UpdateVisualProperties(visual_properties);
    return;
  }

  SetWindowSegments(visual_properties.root_widget_window_segments);

  // We can ignore browser-initialized resizing during synchronous
  // (renderer-controlled) mode, unless it is switching us to/from
  // fullsreen mode or changing the device scale factor.
  bool ignore_resize = synchronous_resize_mode_for_testing_;
  if (ignore_resize) {
    // TODO(danakj): Does the browser actually change DSF inside a web test??
    // TODO(danakj): Isn't the display mode check redundant with the
    // fullscreen one?
    if (visual_properties.is_fullscreen_granted != IsFullscreenGranted() ||
        visual_properties.screen_info.device_scale_factor !=
            widget_base_->GetScreenInfo().device_scale_factor)
      ignore_resize = false;
  }

  // When controlling the size in the renderer, we should ignore sizes given
  // by the browser IPC here.
  // TODO(danakj): There are many things also being ignored that aren't the
  // widget's size params. It works because tests that use this mode don't
  // change those parameters, I guess. But it's more complicated then because
  // it looks like they are related to sync resize mode. Let's move them out
  // of this block.
  gfx::Rect new_compositor_viewport_pixel_rect =
      visual_properties.compositor_viewport_pixel_rect;
  if (AutoResizeMode()) {
    new_compositor_viewport_pixel_rect = gfx::Rect(gfx::ScaleToCeiledSize(
        widget_base_->BlinkSpaceToFlooredDIPs(size_),
        visual_properties.screen_info.device_scale_factor));
  }

  widget_base_->UpdateSurfaceAndScreenInfo(
      visual_properties.local_surface_id.value_or(viz::LocalSurfaceId()),
      new_compositor_viewport_pixel_rect, visual_properties.screen_info);

  // Store this even when auto-resizing, it is the size of the full viewport
  // used for clipping, and this value is propagated down the Widget
  // hierarchy via the VisualProperties waterfall.
  widget_base_->SetVisibleViewportSizeInDIPs(
      visual_properties.visible_viewport_size);

  if (!AutoResizeMode()) {
    size_ = widget_base_->DIPsToCeiledBlinkSpace(visual_properties.new_size);

    View()->ResizeWithBrowserControls(
        size_,
        widget_base_->DIPsToCeiledBlinkSpace(
            widget_base_->VisibleViewportSizeInDIPs()),
        visual_properties.browser_controls_params);
  }
}

}  // namespace blink
