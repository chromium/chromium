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
    base::PassKey<WebFrameWidget>,
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
                         /*is_for_child_local_root=*/false,
                         is_for_nested_main_frame),
      web_view_(&web_view),
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

bool WebViewFrameWidget::ShouldHandleImeEvents() {
  return HasFocus();
}

bool WebViewFrameWidget::ScrollFocusedEditableElementIntoView() {
  return web_view_->ScrollFocusedEditableElementIntoView();
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

}  // namespace blink
