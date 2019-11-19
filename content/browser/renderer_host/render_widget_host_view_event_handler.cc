// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"

#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/viz/common/features.h"
#include "content/browser/renderer_host/hit_test_debug_key_event_observer.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_aura.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_switches_internal.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_features.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/scoped_keyboard_hook.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/touch_selection/touch_selection_controller.h"

#if defined(OS_WIN)
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/common/context_menu_params.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#endif  // defined(OS_WIN)

namespace {

// In mouse lock mode, we need to prevent the (invisible) cursor from hitting
// the border of the view, in order to get valid movement information. However,
// forcing the cursor back to the center of the view after each mouse move
// doesn't work well. It reduces the frequency of useful mouse move messages
// significantly. Therefore, we move the cursor to the center of the view only
// if it approaches the border. |kMouseLockBorderPercentage| specifies the width
// of the border area, in percentage of the corresponding dimension.
const int kMouseLockBorderPercentage = 15;

#if defined(OS_WIN)
// A callback function for EnumThreadWindows to enumerate and dismiss
// any owned popup windows.
BOOL CALLBACK DismissOwnedPopups(HWND window, LPARAM arg) {
  const HWND toplevel_hwnd = reinterpret_cast<HWND>(arg);

  if (::IsWindowVisible(window)) {
    const HWND owner = ::GetWindow(window, GW_OWNER);
    if (toplevel_hwnd == owner) {
      ::PostMessageW(window, WM_CANCELMODE, 0, 0);
    }
  }

  return TRUE;
}
#endif  // defined(OS_WIN)

bool IsFractionalScaleFactor(float scale_factor) {
  return (scale_factor - static_cast<int>(scale_factor)) > 0;
}

// We don't mark these as handled so that they're sent back to the
// DefWindowProc so it can generate WM_APPCOMMAND as necessary.
bool ShouldGenerateAppCommand(const ui::MouseEvent* event) {
#if defined(OS_WIN)
  return (event->native_event().message == WM_NCXBUTTONUP);
#endif
  return false;
}

// Reset unchanged touch points to StateStationary for touchmove and
// touchcancel.
void MarkUnchangedTouchPointsAsStationary(blink::WebTouchEvent* event,
                                          int changed_touch_id) {
  if (event->GetType() == blink::WebInputEvent::kTouchMove ||
      event->GetType() == blink::WebInputEvent::kTouchCancel) {
    for (size_t i = 0; i < event->touches_length; ++i) {
      if (event->touches[i].id != changed_touch_id)
        event->touches[i].state = blink::WebTouchPoint::kStateStationary;
    }
  }
}

bool NeedsInputGrab(content::RenderWidgetHostViewBase* view) {
  if (!view)
    return false;
  return view->GetWidgetType() == content::WidgetType::kPopup;
}

}  // namespace

namespace content {

RenderWidgetHostViewEventHandler::Delegate::Delegate() = default;

RenderWidgetHostViewEventHandler::Delegate::~Delegate() {}

RenderWidgetHostViewEventHandler::RenderWidgetHostViewEventHandler(
    RenderWidgetHostImpl* host,
    RenderWidgetHostViewBase* host_view,
    Delegate* delegate)
    : pinch_zoom_enabled_(content::IsPinchToZoomEnabled()),
      enable_consolidated_movement_(
          base::FeatureList::IsEnabled(features::kConsolidatedMovementXY)),
      host_(host),
      host_view_(host_view),
      delegate_(delegate),
      mouse_wheel_phase_handler_(host_view),
      debug_observer_(features::IsVizHitTestingDebugEnabled()
                          ? std::make_unique<HitTestDebugKeyEventObserver>(host)
                          : nullptr) {}

RenderWidgetHostViewEventHandler::~RenderWidgetHostViewEventHandler() {
  DCHECK(!mouse_locked_);
}

void RenderWidgetHostViewEventHandler::SetPopupChild(
    RenderWidgetHostViewBase* popup_child_host_view,
    ui::EventHandler* popup_child_event_handler) {
  popup_child_host_view_ = popup_child_host_view;
  popup_child_event_handler_ = popup_child_event_handler;
}

void RenderWidgetHostViewEventHandler::TrackHost(
    aura::Window* reference_window) {
  if (!reference_window)
    return;
  DCHECK(!host_tracker_);
  host_tracker_.reset(new aura::WindowTracker);
  host_tracker_->Add(reference_window);
}

#if defined(OS_WIN)
void RenderWidgetHostViewEventHandler::UpdateMouseLockRegion() {
  RECT window_rect =
      display::Screen::GetScreen()
          ->DIPToScreenRectInWindow(window_, window_->GetBoundsInScreen())
          .ToRECT();
  ::ClipCursor(&window_rect);
}
#endif

bool RenderWidgetHostViewEventHandler::LockMouse(
    bool request_unadjusted_movement) {
  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return false;

  if (mouse_locked_)
    return true;

  if (request_unadjusted_movement && window_->GetHost()) {
    mouse_locked_unadjusted_movement_ =
        window_->GetHost()->RequestUnadjustedMovement();
    if (!mouse_locked_unadjusted_movement_)
      return false;
  }

  mouse_locked_ = true;

#if !defined(OS_WIN)
  window_->SetCapture();
#else
  UpdateMouseLockRegion();
#endif
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->HideCursor();
    cursor_client->LockCursor();
  }

  if (ShouldMoveToCenter(unlocked_global_mouse_position_))
    MoveCursorToCenter(nullptr);

  delegate_->SetTooltipsEnabled(false);
  return true;
}

void RenderWidgetHostViewEventHandler::UnlockMouse() {
  delegate_->SetTooltipsEnabled(true);

  aura::Window* root_window = window_->GetRootWindow();
  if (!mouse_locked_ || !root_window)
    return;

  mouse_locked_ = false;
  mouse_locked_unadjusted_movement_.reset();

  if (window_->HasCapture())
    window_->ReleaseCapture();

#if defined(OS_WIN)
  ::ClipCursor(NULL);
#endif

  // Ensure that the global mouse position is updated here to its original
  // value. If we don't do this then the synthesized mouse move which is posted
  // after the cursor is moved ends up getting a large movement delta which is
  // not what sites expect. The delta is computed in the
  // ModifyEventMovementAndCoords function.
  global_mouse_position_ = unlocked_global_mouse_position_;
  window_->MoveCursorTo(gfx::ToFlooredPoint(unlocked_mouse_position_));
  synthetic_move_position_ =
      gfx::ToFlooredPoint(unlocked_global_mouse_position_);

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->UnlockCursor();
    cursor_client->ShowCursor();
  }
  host_->LostMouseLock();
}

bool RenderWidgetHostViewEventHandler::LockKeyboard(
    base::Optional<base::flat_set<ui::DomCode>> codes) {
  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return false;

  // Remove existing hook, if registered.
  UnlockKeyboard();
  scoped_keyboard_hook_ = root_window->CaptureSystemKeyEvents(std::move(codes));

  return IsKeyboardLocked();
}

void RenderWidgetHostViewEventHandler::UnlockKeyboard() {
  scoped_keyboard_hook_.reset();
}

bool RenderWidgetHostViewEventHandler::IsKeyboardLocked() const {
  return scoped_keyboard_hook_ != nullptr;
}

void RenderWidgetHostViewEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewBase::OnKeyEvent");

  if (NeedsInputGrab(popup_child_host_view_)) {
    popup_child_event_handler_->OnKeyEvent(event);
    if (event->handled())
      return;
  }

  bool mark_event_as_handled = true;
  // We need to handle the Escape key for Pepper Flash.
  if (host_view_->is_fullscreen() && event->key_code() == ui::VKEY_ESCAPE) {
    // Focus the window we were created from.
    if (host_tracker_.get() && !host_tracker_->windows().empty()) {
      aura::Window* host = *(host_tracker_->windows().begin());
      aura::client::FocusClient* client = aura::client::GetFocusClient(host);
      if (client) {
        // Calling host->Focus() may delete |this|. We create a local observer
        // for that. In that case we exit without further access to any members.
        auto local_tracker = std::move(host_tracker_);
        local_tracker->Add(window_);
        host->Focus();
        if (!local_tracker->Contains(window_)) {
          event->SetHandled();
          return;
        }
      }
    }
    delegate_->Shutdown();
    host_tracker_.reset();
  } else {
    if (event->key_code() == ui::VKEY_RETURN) {
      // Do not forward return key release events if no press event was handled.
      if (event->type() == ui::ET_KEY_RELEASED && !accept_return_character_)
        return;
      // Accept return key character events between press and release events.
      accept_return_character_ = event->type() == ui::ET_KEY_PRESSED;
    }

    // Call SetKeyboardFocus() for not only ET_KEY_PRESSED but also
    // ET_KEY_RELEASED. If a user closed the hotdog menu with ESC key press,
    // we need to notify focus to Blink on ET_KEY_RELEASED for ESC key.
    SetKeyboardFocus();
    // We don't have to communicate with an input method here.
    NativeWebKeyboardEvent webkit_event(*event);

    // If the key has been reserved as part of the active KeyboardLock request,
    // then we want to mark it as such so it is not intercepted by the browser.
    if (IsKeyLocked(*event))
      webkit_event.skip_in_browser = true;

    delegate_->ForwardKeyboardEventWithLatencyInfo(
        webkit_event, *event->latency(), &mark_event_as_handled);
  }
  if (mark_event_as_handled)
    event->SetHandled();
}

void RenderWidgetHostViewEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewBase::OnMouseEvent");

  // CrOS will send a mouse exit event to update hover state when mouse is
  // hidden which we want to filter out in renderer. crbug.com/723535.
  if (event->flags() & ui::EF_CURSOR_HIDE)
    return;

  ForwardMouseEventToParent(event);
  // TODO(mgiuca): Return if event->handled() returns true. This currently
  // breaks drop-down lists which means something is incorrectly setting
  // event->handled to true (http://crbug.com/577983).

  if (mouse_locked_) {
    HandleMouseEventWhileLocked(event);
    return;
  }

  // As the overscroll is handled during scroll events from the trackpad, the
  // RWHVA window is transformed by the overscroll controller. This transform
  // triggers a synthetic mouse-move event to be generated (by the aura
  // RootWindow). Also, with a touchscreen, we may get a synthetic mouse-move
  // caused by a pointer grab. But these events interfere with the overscroll
  // gesture. So, ignore such synthetic mouse-move events if an overscroll
  // gesture is in progress.
  OverscrollController* overscroll_controller =
      delegate_->overscroll_controller();
  if (overscroll_controller &&
      overscroll_controller->overscroll_mode() != OVERSCROLL_NONE &&
      event->flags() & ui::EF_IS_SYNTHESIZED &&
      (event->type() == ui::ET_MOUSE_ENTERED ||
       event->type() == ui::ET_MOUSE_EXITED ||
       event->type() == ui::ET_MOUSE_MOVED)) {
    event->StopPropagation();
    return;
  }

  if (event->type() == ui::ET_MOUSEWHEEL) {
#if defined(OS_WIN)
    // We get mouse wheel/scroll messages even if we are not in the foreground.
    // So here we check if we have any owned popup windows in the foreground and
    // dismiss them.
    aura::WindowTreeHost* host = window_->GetHost();
    if (host) {
      HWND parent = host->GetAcceleratedWidget();
      HWND toplevel_hwnd = ::GetAncestor(parent, GA_ROOT);
      EnumThreadWindows(GetCurrentThreadId(), DismissOwnedPopups,
                        reinterpret_cast<LPARAM>(toplevel_hwnd));
    }
#endif
    blink::WebMouseWheelEvent mouse_wheel_event =
        ui::MakeWebMouseWheelEvent(*event->AsMouseWheelEvent());

    if (mouse_wheel_event.delta_x != 0 || mouse_wheel_event.delta_y != 0) {
      const bool should_route_event = ShouldRouteEvents();
      // End the touchpad scrolling sequence (if such exists) before handling
      // a ui::ET_MOUSEWHEEL event.
      mouse_wheel_phase_handler_.SendWheelEndForTouchpadScrollingIfNeeded(
          should_route_event);

      mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(
          mouse_wheel_event, should_route_event);
      if (should_route_event) {
        host_->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
            host_view_, &mouse_wheel_event, *event->latency());
      } else {
        ProcessMouseWheelEvent(mouse_wheel_event, *event->latency());
      }
    }
  } else {
    bool is_selection_popup = NeedsInputGrab(popup_child_host_view_);
    if (CanRendererHandleEvent(event, mouse_locked_, is_selection_popup) &&
        !(event->flags() & ui::EF_FROM_TOUCH)) {
      // Confirm existing composition text on mouse press, to make sure
      // the input caret won't be moved with an ongoing composition text.
      if (event->type() == ui::ET_MOUSE_PRESSED)
        FinishImeCompositionSession();

      blink::WebMouseEvent mouse_event = ui::MakeWebMouseEvent(*event);
      ModifyEventMovementAndCoords(*event, &mouse_event);
      if (ShouldRouteEvents()) {
        host_->delegate()->GetInputEventRouter()->RouteMouseEvent(
            host_view_, &mouse_event, *event->latency());
      } else {
        ProcessMouseEvent(mouse_event, *event->latency());
      }

      // Ensure that we get keyboard focus on mouse down as a plugin window may
      // have grabbed keyboard focus.
      if (event->type() == ui::ET_MOUSE_PRESSED)
        SetKeyboardFocus();
    }
  }

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      window_->SetCapture();
      break;
    case ui::ET_MOUSE_RELEASED:
      if (!delegate_->NeedsMouseCapture())
        window_->ReleaseCapture();
      break;
    default:
      break;
  }

  if (!ShouldGenerateAppCommand(event))
    event->SetHandled();
}

void RenderWidgetHostViewEventHandler::OnScrollEvent(ui::ScrollEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewBase::OnScrollEvent");
  const bool should_route_event = ShouldRouteEvents();
  if (event->type() == ui::ET_SCROLL) {
#if !defined(OS_WIN)
    // TODO(ananta)
    // Investigate if this is true for Windows 8 Metro ASH as well.
    if (event->finger_count() != 2)
      return;
#endif
    blink::WebMouseWheelEvent mouse_wheel_event =
        ui::MakeWebMouseWheelEvent(*event);
    mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(
        mouse_wheel_event, should_route_event);

    base::Optional<blink::WebGestureEvent> maybe_synthetic_fling_cancel;
    if (mouse_wheel_event.phase == blink::WebMouseWheelEvent::kPhaseBegan) {
      maybe_synthetic_fling_cancel =
          ui::MakeWebGestureEventFlingCancel(mouse_wheel_event);
    }

    if (should_route_event) {
      if (maybe_synthetic_fling_cancel) {
        host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
            host_view_, &*maybe_synthetic_fling_cancel,
            ui::LatencyInfo(ui::SourceEventType::WHEEL));
      }
      host_->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
          host_view_, &mouse_wheel_event, *event->latency());
    } else {
      if (maybe_synthetic_fling_cancel) {
        host_->ForwardGestureEvent(*maybe_synthetic_fling_cancel);
      }
      host_->ForwardWheelEventWithLatencyInfo(mouse_wheel_event,
                                              *event->latency());
    }
  } else if (event->type() == ui::ET_SCROLL_FLING_START ||
             event->type() == ui::ET_SCROLL_FLING_CANCEL) {
    blink::WebGestureEvent gesture_event = ui::MakeWebGestureEvent(*event);
    if (should_route_event) {
      host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
          host_view_, &gesture_event,
          ui::LatencyInfo(ui::SourceEventType::WHEEL));
    } else {
      host_->ForwardGestureEvent(gesture_event);
    }
    if (event->type() == ui::ET_SCROLL_FLING_START) {
      RecordAction(base::UserMetricsAction("TrackpadScrollFling"));
      // The user has lifted their fingers.
      mouse_wheel_phase_handler_.ResetTouchpadScrollSequence();
    } else if (event->type() == ui::ET_SCROLL_FLING_CANCEL) {
      // The user has put their fingers down.
      DCHECK_EQ(blink::WebGestureDevice::kTouchpad,
                gesture_event.SourceDevice());
      mouse_wheel_phase_handler_.TouchpadScrollingMayBegin();
    }
  }

  event->SetHandled();
}

void RenderWidgetHostViewEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewBase::OnTouchEvent");

  bool had_no_pointer = !pointer_state_.GetPointerCount();

  // Update the touch event first.
  if (!pointer_state_.OnTouch(*event)) {
    event->StopPropagation();
    return;
  }

  blink::WebTouchEvent touch_event;
  bool handled =
      delegate_->selection_controller()->WillHandleTouchEvent(pointer_state_);
  if (handled) {
    event->SetHandled();
  } else {
    touch_event = ui::CreateWebTouchEventFromMotionEvent(
        pointer_state_, event->may_cause_scrolling(), event->hovering());
  }
  pointer_state_.CleanupRemovedTouchPoints(*event);

  if (handled)
    return;

  if (had_no_pointer)
    delegate_->selection_controller_client()->OnTouchDown();
  if (!pointer_state_.GetPointerCount())
    delegate_->selection_controller_client()->OnTouchUp();

  // It is important to always mark events as being handled asynchronously when
  // they are forwarded. This ensures that the current event does not get
  // processed by the gesture recognizer before events currently awaiting
  // dispatch in the touch queue.
  event->DisableSynchronousHandling();

  // Set unchanged touch point to StateStationary for touchmove and
  // touchcancel to make sure only send one ack per WebTouchEvent.
  MarkUnchangedTouchPointsAsStationary(&touch_event,
                                       event->pointer_details().id);
  if (ShouldRouteEvents()) {
    host_->delegate()->GetInputEventRouter()->RouteTouchEvent(
        host_view_, &touch_event, *event->latency());
  } else {
    ProcessTouchEvent(touch_event, *event->latency());
  }
}

void RenderWidgetHostViewEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewBase::OnGestureEvent");

  if ((event->type() == ui::ET_GESTURE_PINCH_BEGIN ||
       event->type() == ui::ET_GESTURE_PINCH_UPDATE ||
       event->type() == ui::ET_GESTURE_PINCH_END) &&
      !pinch_zoom_enabled_) {
    event->SetHandled();
    return;
  }

  HandleGestureForTouchSelection(event);
  if (event->handled())
    return;

  // Confirm existing composition text on TAP gesture, to make sure the input
  // caret won't be moved with an ongoing composition text.
  if (event->type() == ui::ET_GESTURE_TAP)
    FinishImeCompositionSession();

  blink::WebGestureEvent gesture = ui::MakeWebGestureEvent(*event);
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    // Webkit does not stop a fling-scroll on tap-down. So explicitly send an
    // event to stop any in-progress flings.
    blink::WebGestureEvent fling_cancel = gesture;
    fling_cancel.SetType(blink::WebInputEvent::kGestureFlingCancel);
    fling_cancel.SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
    if (ShouldRouteEvents()) {
      host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
          host_view_, &fling_cancel,
          ui::LatencyInfo(ui::SourceEventType::TOUCH));
    } else {
      host_->ForwardGestureEvent(fling_cancel);
    }
  }

  if (gesture.GetType() != blink::WebInputEvent::kUndefined) {
    if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
      // If there is a current scroll going on and a new scroll that isn't
      // wheel based send a synthetic wheel event with kPhaseEnded to cancel
      // the current scroll.
      mouse_wheel_phase_handler_.DispatchPendingWheelEndEvent();
      mouse_wheel_phase_handler_.SendWheelEndForTouchpadScrollingIfNeeded(
          ShouldRouteEvents());
    } else if (event->type() == ui::ET_SCROLL_FLING_START) {
      RecordAction(base::UserMetricsAction("TouchscreenScrollFling"));
    }

    if (event->type() == ui::ET_GESTURE_SCROLL_END ||
        event->type() == ui::ET_SCROLL_FLING_START) {
      // Scrolling with touchscreen has finished. Make sure that the next wheel
      // event will have phase = |kPhaseBegan|. This is for maintaining the
      // correct phase info when some of the wheel events get ignored while a
      // touchscreen scroll is going on.
      mouse_wheel_phase_handler_.IgnorePendingWheelEndEvent();
      mouse_wheel_phase_handler_.ResetTouchpadScrollSequence();
    }

    if (ShouldRouteEvents()) {
      host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
          host_view_, &gesture, *event->latency());
    } else {
      host_->ForwardGestureEventWithLatencyInfo(gesture, *event->latency());
    }
  }

  // If a gesture is not processed by the webpage, then WebKit processes it
  // (e.g. generates synthetic mouse events).
  event->SetHandled();
}

void RenderWidgetHostViewEventHandler::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  mouse_wheel_phase_handler_.GestureEventAck(event, ack_result);
}

bool RenderWidgetHostViewEventHandler::CanRendererHandleEvent(
    const ui::MouseEvent* event,
    bool mouse_locked,
    bool selection_popup) const {
  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED)
    return false;

  if (event->type() == ui::ET_MOUSE_EXITED) {
    if (mouse_locked || selection_popup)
      return false;
#if defined(OS_WIN) || defined(OS_LINUX)
    // Don't forward the mouse leave message which is received when the context
    // menu is displayed by the page. This confuses the page and causes state
    // changes.
    if (host_->delegate() && host_->delegate()->IsShowingContextMenuOnPage())
      return false;
#endif
    return true;
  }

#if defined(OS_WIN)
  // Renderer cannot handle WM_XBUTTON or NC events.
  switch (event->native_event().message) {
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
      return true;
    case WM_NCMOUSELEAVE:
    case WM_NCMOUSEMOVE:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCXBUTTONDOWN:
    case WM_NCXBUTTONUP:
    case WM_NCXBUTTONDBLCLK:
      return false;
    default:
      break;
  }
#endif
  return true;
}

void RenderWidgetHostViewEventHandler::FinishImeCompositionSession() {
  // RenderWidgetHostViewAura keeps track of existing composition texts. The
  // call to finish composition text should be made through the RWHVA itself
  // otherwise the following call to cancel composition will lead to an extra
  // IPC for finishing the ongoing composition (see https://crbug.com/723024).
  host_view_->GetTextInputClient()->ConfirmCompositionText(
      /* keep_selection */ false);
  host_view_->ImeCancelComposition();
}

void RenderWidgetHostViewEventHandler::ForwardMouseEventToParent(
    ui::MouseEvent* event) {
  // Needed to propagate mouse event to |window_->parent()->delegate()|, but
  // note that it might be something other than a WebContentsViewAura instance.
  // TODO(pkotwicz): Find a better way of doing this.
  // In fullscreen mode which is typically used by flash, don't forward
  // the mouse events to the parent. The renderer and the plugin process
  // handle these events.
  if (host_view_->is_fullscreen())
    return;

  if (event->flags() & ui::EF_FROM_TOUCH)
    return;

  if (!window_->parent() || !window_->parent()->delegate())
    return;

  // Take a copy of |event|, to avoid ConvertLocationToTarget mutating the
  // event.
  std::unique_ptr<ui::Event> event_copy = ui::Event::Clone(*event);
  ui::MouseEvent* mouse_event = static_cast<ui::MouseEvent*>(event_copy.get());
  mouse_event->ConvertLocationToTarget(window_, window_->parent());
  window_->parent()->delegate()->OnMouseEvent(mouse_event);
  if (mouse_event->handled())
    event->SetHandled();
}

void RenderWidgetHostViewEventHandler::HandleGestureForTouchSelection(
    ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_LONG_PRESS:
      delegate_->selection_controller()->HandleLongPressEvent(
          event->time_stamp(), event->location_f());
      break;
    case ui::ET_GESTURE_TAP:
      delegate_->selection_controller()->HandleTapEvent(
          event->location_f(), event->details().tap_count());
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      delegate_->selection_controller_client()->OnScrollStarted();
      break;
    case ui::ET_GESTURE_SCROLL_END:
      delegate_->selection_controller_client()->OnScrollCompleted();
      break;
    default:
      break;
  }
}

void RenderWidgetHostViewEventHandler::HandleMouseEventWhileLocked(
    ui::MouseEvent* event) {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());

  DCHECK(!cursor_client || !cursor_client->IsCursorVisible());

  if (event->type() == ui::ET_MOUSEWHEEL) {
    blink::WebMouseWheelEvent mouse_wheel_event =
        ui::MakeWebMouseWheelEvent(*event->AsMouseWheelEvent());
    if (mouse_wheel_event.delta_x != 0 || mouse_wheel_event.delta_y != 0) {
      if (ShouldRouteEvents()) {
        host_->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
            host_view_, &mouse_wheel_event, *event->latency());
      } else {
        ProcessMouseWheelEvent(mouse_wheel_event, *event->latency());
      }
    }
  } else {
    // If we receive non client mouse messages while we are in the locked state
    // it probably means that the mouse left the borders of our window and
    // needs to be moved back to the center.
    if (event->flags() & ui::EF_IS_NON_CLIENT) {
      // TODO(jonross): ideally this would not be done for mus
      // (crbug.com/621412)
      MoveCursorToCenter(event);
      return;
    }

    blink::WebMouseEvent mouse_event = ui::MakeWebMouseEvent(*event);

    bool should_not_forward = MatchesSynthesizedMovePosition(mouse_event);

    ModifyEventMovementAndCoords(*event, &mouse_event);

    if (!enable_consolidated_movement_ && should_not_forward) {
      synthetic_move_position_.reset();
    } else {
      bool is_selection_popup = NeedsInputGrab(popup_child_host_view_);
      // Forward event to renderer.
      if (CanRendererHandleEvent(event, mouse_locked_, is_selection_popup) &&
          !(event->flags() & ui::EF_FROM_TOUCH)) {
        if (ShouldRouteEvents()) {
          host_->delegate()->GetInputEventRouter()->RouteMouseEvent(
              host_view_, &mouse_event, *event->latency());
        } else {
          ProcessMouseEvent(mouse_event, *event->latency());
        }
        // Ensure that we get keyboard focus on mouse down as a plugin window
        // may have grabbed keyboard focus.
        if (event->type() == ui::ET_MOUSE_PRESSED)
          SetKeyboardFocus();
      }

      // Check if the mouse has reached the border and needs to be centered.
      // Use event position if consolidated_movement_ is enabled, otherwise use
      // stored global_mouse_position_.
      if (ShouldMoveToCenter(enable_consolidated_movement_
                                 ? gfx::PointF(mouse_event.PositionInScreen())
                                 : global_mouse_position_)) {
        MoveCursorToCenter(event);
      }
    }
  }
  if (!ShouldGenerateAppCommand(event))
    event->SetHandled();
}

void RenderWidgetHostViewEventHandler::ModifyEventMovementAndCoords(
    const ui::MouseEvent& ui_mouse_event,
    blink::WebMouseEvent* event) {
  if (!enable_consolidated_movement_) {
    // If the mouse has just entered, we must report zero movementX/Y. Hence we
    // reset any global_mouse_position set previously.
    if (ui_mouse_event.type() == ui::ET_MOUSE_ENTERED ||
        ui_mouse_event.type() == ui::ET_MOUSE_EXITED) {
      global_mouse_position_.SetPoint(event->PositionInScreen().x,
                                      event->PositionInScreen().y);
    }

    // Movement is computed by taking the difference of the new cursor position
    // and the previous. Under mouse lock the cursor will be warped back to the
    // center so that we are not limited by clipping boundaries.
    // We do not measure movement as the delta from cursor to center because
    // we may receive more mouse movement events before our warp has taken
    // effect.
    // TODO(crbug.com/802067): We store event coordinates as pointF but
    // movement_x/y are integer. In order not to lose fractional part, we need
    // to keep the movement calculation as "floor(cur_pos) - floor(last_pos)".
    // Remove the floor here when movement_x/y is changed to double.
    if (!(ui_mouse_event.flags() & ui::EF_UNADJUSTED_MOUSE)) {
      event->movement_x = gfx::ToFlooredInt(event->PositionInScreen().x) -
                          gfx::ToFlooredInt(global_mouse_position_.x());
      event->movement_y = gfx::ToFlooredInt(event->PositionInScreen().y) -
                          gfx::ToFlooredInt(global_mouse_position_.y());
    }

    global_mouse_position_.SetPoint(event->PositionInScreen().x,
                                    event->PositionInScreen().y);
  }

  // This logic is similar to |is_move_to_center_event| check when
  // consolidated_movement disabled. We can not guarantee that |MoveCursorTo|
  // is taking effect immediately, so wait for the event that has matching
  // coordiantes to marked as synthesized event.
  if (enable_consolidated_movement_ && mouse_locked_ &&
      MatchesSynthesizedMovePosition(*event)) {
    event->SetModifiers(event->GetModifiers() |
                        blink::WebInputEvent::Modifiers::kRelativeMotionEvent);
    synthetic_move_position_.reset();
    return;
  }

  // Under mouse lock, coordinates of mouse are locked to what they were when
  // mouse lock was entered.
  if (mouse_locked_) {
    if (!enable_consolidated_movement_) {
      event->SetPositionInWidget(unlocked_mouse_position_.x(),
                                 unlocked_mouse_position_.y());
      event->SetPositionInScreen(unlocked_global_mouse_position_.x(),
                                 unlocked_global_mouse_position_.y());
    }
  } else {
    unlocked_mouse_position_.SetPoint(event->PositionInWidget().x,
                                      event->PositionInWidget().y);
    unlocked_global_mouse_position_.SetPoint(event->PositionInScreen().x,
                                             event->PositionInScreen().y);
  }
}

void RenderWidgetHostViewEventHandler::MoveCursorToCenter(
    ui::MouseEvent* event) {
  gfx::Point center(gfx::Rect(window_->bounds().size()).CenterPoint());
  gfx::Point center_in_screen(window_->GetBoundsInScreen().CenterPoint());
  window_->MoveCursorTo(center);
#if defined(OS_WIN)
  // TODO(crbug.com/781182): Set the global position when move cursor to center.
  // This is a workaround for a bug from Windows update 16299, and should be
  // remove once the bug is fixed in OS. When consolidate_movement_ flag is
  // enabled, send a synthesized event to update the blink side states.
  global_mouse_position_ = gfx::PointF(center_in_screen);
  if (enable_consolidated_movement_ && event) {
    blink::WebMouseEvent mouse_event = ui::MakeWebMouseEvent(*event);
    mouse_event.SetModifiers(
        mouse_event.GetModifiers() |
        blink::WebInputEvent::Modifiers::kRelativeMotionEvent);
    mouse_event.SetPositionInScreen(gfx::PointF(center_in_screen));
    if (ShouldRouteEvents()) {
      host_->delegate()->GetInputEventRouter()->RouteMouseEvent(
          host_view_, &mouse_event, ui::LatencyInfo());
    } else {
      ProcessMouseEvent(mouse_event, ui::LatencyInfo());
    }
    return;
  }
#endif
  synthetic_move_position_ = center_in_screen;
}

bool RenderWidgetHostViewEventHandler::MatchesSynthesizedMovePosition(
    const blink::WebMouseEvent& event) {
  if (event.GetType() == blink::WebInputEvent::kMouseMove &&
      synthetic_move_position_.has_value()) {
    if (IsFractionalScaleFactor(host_view_->current_device_scale_factor())) {
      // For fractional scale factors, the conversion from pixels to dip and
      // vice versa could result in off by 1 or 2 errors which hurts us because
      // the artificial move to center event cause the cursor to bounce around
      // the center of the screen leading to the lock operation not working
      // correctly. Workaround is to treat a mouse move or drag event off by
      // atmost 2 px from the center as a move to center event.
      // TODO(crbug.com/991236): figure out a way to avoid the conversion error.
      return ((std::abs(event.PositionInScreen().x -
                        synthetic_move_position_->x()) <= 2) &&
              (std::abs(event.PositionInScreen().y -
                        synthetic_move_position_->y()) <= 2));
    } else {
      return synthetic_move_position_.value() ==
             gfx::ToRoundedPoint(event.PositionInScreen());
    }
  }
  return false;
}

void RenderWidgetHostViewEventHandler::SetKeyboardFocus() {
#if defined(OS_WIN)
  if (window_ && window_->delegate()->CanFocus()) {
    aura::WindowTreeHost* host = window_->GetHost();
    if (host) {
      gfx::AcceleratedWidget hwnd = host->GetAcceleratedWidget();
      if (!(::GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE))
        ::SetFocus(hwnd);
    }
  }
#endif
  // TODO(wjmaclean): can host_ ever be null?
  if (host_ && set_focus_on_mouse_down_or_key_event_) {
    set_focus_on_mouse_down_or_key_event_ = false;
    host_->Focus();
  }
}

bool RenderWidgetHostViewEventHandler::ShouldMoveToCenter(
    gfx::PointF mouse_screen_position) {
  // Do not need to move to center in unadjusted movement mode as
  // the movement value are directly from OS.
  if (mouse_locked_unadjusted_movement_)
    return false;

  gfx::Rect rect = window_->bounds();
  rect = delegate_->ConvertRectToScreen(rect);
  float border_x = rect.width() * kMouseLockBorderPercentage / 100.0;
  float border_y = rect.height() * kMouseLockBorderPercentage / 100.0;

  return mouse_screen_position.x() < rect.x() + border_x ||
         mouse_screen_position.x() > rect.right() - border_x ||
         mouse_screen_position.y() < rect.y() + border_y ||
         mouse_screen_position.y() > rect.bottom() - border_y;
}

bool RenderWidgetHostViewEventHandler::ShouldRouteEvents() const {
  if (!host_->delegate())
    return false;

  // Do not route events that are currently targeted to page popups such as
  // <select> element drop-downs, since these cannot contain cross-process
  // frames.
  if (!host_->delegate()->IsWidgetForMainFrame(host_))
    return false;

  return !!host_->delegate()->GetInputEventRouter();
}

void RenderWidgetHostViewEventHandler::ProcessMouseEvent(
    const blink::WebMouseEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardMouseEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewEventHandler::ProcessMouseWheelEvent(
    const blink::WebMouseWheelEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardWheelEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewEventHandler::ProcessTouchEvent(
    const blink::WebTouchEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardTouchEventWithLatencyInfo(event, latency);
}

bool RenderWidgetHostViewEventHandler::IsKeyLocked(const ui::KeyEvent& event) {
  // Note: We never consider 'ESC' to be locked as we don't want to prevent it
  // from being handled by the browser.  Doing so would have adverse effects
  // such as the user being unable to exit fullscreen mode.
  if (!IsKeyboardLocked() || event.code() == ui::DomCode::ESCAPE)
    return false;

  return scoped_keyboard_hook_->IsKeyLocked(event.code());
}

}  // namespace content
