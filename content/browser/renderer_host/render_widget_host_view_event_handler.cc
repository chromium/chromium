// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"

#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/common/features.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_aura.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_switches_internal.h"
#include "content/common/features.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/scoped_keyboard_hook.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/delegated_ink_point.h"
#include "ui/touch_selection/touch_selection_controller.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace {

// In mouse lock mode, we need to prevent the (invisible) cursor from hitting
// the border of the view, in order to get valid movement information. However,
// forcing the cursor back to the center of the view after each mouse move
// doesn't work well. It reduces the frequency of useful mouse move messages
// significantly. Therefore, we move the cursor to the center of the view only
// if it approaches the border. |kMouseLockBorderPercentage| specifies the width
// of the border area, in percentage of the corresponding dimension.
const int kMouseLockBorderPercentage = 15;

#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)

bool IsFractionalScaleFactor(float scale_factor) {
  return (scale_factor - static_cast<int>(scale_factor)) > 0;
}

// We don't mark these as handled so that they're sent back to the
// DefWindowProc so it can generate WM_APPCOMMAND as necessary.
bool ShouldGenerateAppCommand(const ui::MouseEvent* event) {
#if BUILDFLAG(IS_WIN)
  return (event->native_event().message == WM_NCXBUTTONUP);
#else
  return false;
#endif
}

// Reset unchanged touch points to StateStationary for touchmove and
// touchcancel.
void MarkUnchangedTouchPointsAsStationary(blink::WebTouchEvent* event,
                                          int changed_touch_id) {
  if (event->GetType() == blink::WebInputEvent::Type::kTouchMove ||
      event->GetType() == blink::WebInputEvent::Type::kTouchCancel) {
    for (size_t i = 0; i < event->touches_length; ++i) {
      if (event->touches[i].id != changed_touch_id)
        event->touches[i].state = blink::WebTouchPoint::State::kStateStationary;
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
      host_(host),
      host_view_(host_view),
      delegate_(delegate),
      mouse_wheel_phase_handler_(host_view) {}

RenderWidgetHostViewEventHandler::~RenderWidgetHostViewEventHandler() {
  DCHECK(!mouse_locked_);
}

void RenderWidgetHostViewEventHandler::SetPopupChild(
    RenderWidgetHostViewBase* popup_child_host_view,
    ui::EventHandler* popup_child_event_handler) {
  popup_child_host_view_ = popup_child_host_view;
  popup_child_event_handler_ = popup_child_event_handler;
}

blink::mojom::PointerLockResult RenderWidgetHostViewEventHandler::LockPointer(
    bool request_unadjusted_movement) {
  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return blink::mojom::PointerLockResult::kWrongDocument;

  if (mouse_locked_)
    return blink::mojom::PointerLockResult::kSuccess;

  if (request_unadjusted_movement && window_->GetHost()) {
    mouse_locked_unadjusted_movement_ =
        window_->GetHost()->RequestUnadjustedMovement();
    if (!mouse_locked_unadjusted_movement_)
      return blink::mojom::PointerLockResult::kUnsupportedOptions;
  }
  mouse_locked_ = true;

  window_->GetHost()->LockMouse(window_);

  if (ShouldMoveToCenter(unlocked_global_mouse_position_))
    MoveCursorToCenter(nullptr);

  delegate_->SetTooltipsEnabled(false);
  return blink::mojom::PointerLockResult::kSuccess;
}

blink::mojom::PointerLockResult
RenderWidgetHostViewEventHandler::ChangePointerLock(
    bool request_unadjusted_movement) {
  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window || !window_->GetHost())
    return blink::mojom::PointerLockResult::kWrongDocument;

  // If lock was lost before completing this change request
  // it was because the user hit escape or navigated away
  // from the page.
  if (!mouse_locked_)
    return blink::mojom::PointerLockResult::kUserRejected;

  if (!request_unadjusted_movement) {
    mouse_locked_unadjusted_movement_.reset();
    return blink::mojom::PointerLockResult::kSuccess;
  }

  if (mouse_locked_unadjusted_movement_) {
    // Desired state already acquired.
    return blink::mojom::PointerLockResult::kSuccess;
  }

  mouse_locked_unadjusted_movement_ =
      window_->GetHost()->RequestUnadjustedMovement();

  if (!mouse_locked_unadjusted_movement_)
    return blink::mojom::PointerLockResult::kUnsupportedOptions;

  return blink::mojom::PointerLockResult::kSuccess;
}

void RenderWidgetHostViewEventHandler::UnlockPointer() {
  delegate_->SetTooltipsEnabled(true);

  aura::Window* root_window = window_->GetRootWindow();
  if (!mouse_locked_ || !root_window)
    return;

  mouse_locked_ = false;
  mouse_locked_unadjusted_movement_.reset();

  window_->GetHost()->UnlockMouse(window_);

  // Ensure that the global mouse position is updated here to its original
  // value. If we don't do this then the synthesized mouse move which is posted
  // after the cursor is moved ends up getting a large movement delta which is
  // not what sites expect. The delta is computed in the
  // ModifyEventMovementAndCoords function.
  window_->MoveCursorTo(gfx::ToFlooredPoint(unlocked_mouse_position_));
  synthetic_move_position_ =
      gfx::ToFlooredPoint(unlocked_global_mouse_position_);

  host_->LostPointerLock();
}

bool RenderWidgetHostViewEventHandler::LockKeyboard(
    std::optional<base::flat_set<ui::DomCode>> codes) {
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

  if (event->key_code() == ui::VKEY_RETURN) {
    // Do not forward return key release events if no press event was handled.
    if (event->type() == ui::EventType::kKeyReleased &&
        !accept_return_character_) {
      return;
    }
    // Accept return key character events between press and release events.
    accept_return_character_ = event->type() == ui::EventType::kKeyPressed;
  }

  // Call SetKeyboardFocus() for not only EventType::kKeyPressed but also
  // EventType::kKeyReleased. If a user closed the hotdog menu with ESC key
  // press, we need to notify focus to Blink on EventType::kKeyReleased for ESC
  // key.
  SetKeyboardFocus();
  // We don't have to communicate with an input method here.
  input::NativeWebKeyboardEvent webkit_event(*event);

  // If the key has been reserved as part of the active KeyboardLock request,
  // then we want to mark it as such so it is not intercepted by the browser.
  if (IsKeyLocked(*event))
    webkit_event.skip_if_unhandled = true;

  bool mark_event_as_handled = true;
  delegate_->ForwardKeyboardEventWithLatencyInfo(
      webkit_event, *event->latency(), &mark_event_as_handled);
  if (mark_event_as_handled)
    event->SetHandled();
}

void RenderWidgetHostViewEventHandler::HandleMouseWheelEvent(
    ui::MouseEvent* event) {
  DCHECK(event);
  DCHECK_EQ(event->type(), ui::EventType::kMousewheel);

#if BUILDFLAG(IS_WIN)
  if (!mouse_locked_) {
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
  }
#endif

  blink::WebMouseWheelEvent mouse_wheel_event =
      ui::MakeWebMouseWheelEvent(*event->AsMouseWheelEvent());

  if (mouse_wheel_event.delta_x != 0 || mouse_wheel_event.delta_y != 0) {
    const bool should_route_event = ShouldRouteEvents();
    // End the touchpad scrolling sequence (if such exists) before handling
    // a ui::EventType::kMousewheel event.
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
      (event->type() == ui::EventType::kMouseEntered ||
       event->type() == ui::EventType::kMouseExited ||
       event->type() == ui::EventType::kMouseMoved)) {
    event->StopPropagation();
    return;
  }

  if (event->type() == ui::EventType::kMousewheel) {
    HandleMouseWheelEvent(event);
  } else {
    bool is_selection_popup = NeedsInputGrab(popup_child_host_view_);
    if (CanRendererHandleEvent(event, mouse_locked_, is_selection_popup) &&
        !(event->flags() & ui::EF_FROM_TOUCH)) {

      // Confirm existing composition text on mouse press, to make sure
      // the input caret won't be moved with an ongoing composition text.
      if (event->type() == ui::EventType::kMousePressed) {
        FinishImeCompositionSession();
      }

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
      if (event->type() == ui::EventType::kMousePressed) {
        SetKeyboardFocus();
      }
    }
  }

  switch (event->type()) {
    case ui::EventType::kMousePressed:
      window_->SetCapture();
      break;
    case ui::EventType::kMouseReleased:
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
  if (event->type() == ui::EventType::kScroll) {
#if !BUILDFLAG(IS_WIN)
    // TODO(ananta)
    // Investigate if this is true for Windows 8 Metro ASH as well.
    if (event->finger_count() != 2)
      return;
#endif
    blink::WebMouseWheelEvent mouse_wheel_event =
        ui::MakeWebMouseWheelEvent(*event);
    mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(
        mouse_wheel_event, should_route_event);

    std::optional<blink::WebGestureEvent> maybe_synthetic_fling_cancel;
    if (mouse_wheel_event.phase == blink::WebMouseWheelEvent::kPhaseBegan) {
      maybe_synthetic_fling_cancel =
          ui::MakeWebGestureEventFlingCancel(mouse_wheel_event);
    }

    if (should_route_event) {
      if (maybe_synthetic_fling_cancel) {
        host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
            host_view_, &*maybe_synthetic_fling_cancel, ui::LatencyInfo());
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
  } else if (event->type() == ui::EventType::kScrollFlingStart ||
             event->type() == ui::EventType::kScrollFlingCancel) {
    blink::WebGestureEvent gesture_event = ui::MakeWebGestureEvent(*event);
    if (should_route_event) {
      host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
          host_view_, &gesture_event, ui::LatencyInfo());
    } else {
      host_->ForwardGestureEvent(gesture_event);
    }
    if (event->type() == ui::EventType::kScrollFlingStart) {
      RecordAction(base::UserMetricsAction("TrackpadScrollFling"));
      // The user has lifted their fingers.
      mouse_wheel_phase_handler_.ResetTouchpadScrollSequence();
    } else if (event->type() == ui::EventType::kScrollFlingCancel) {
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

  // Ensure that we get keyboard focus on tap down as page may lose focus
  // state previously (e.g. tapping outside to dismiss a select pop-up menu).
  if (event->type() == ui::EventType::kGestureTap) {
    SetKeyboardFocus();
  }

  if ((event->type() == ui::EventType::kGesturePinchBegin ||
       event->type() == ui::EventType::kGesturePinchUpdate ||
       event->type() == ui::EventType::kGesturePinchEnd) &&
      !pinch_zoom_enabled_) {
    event->SetHandled();
    return;
  }

  HandleGestureForTouchSelection(event);
  if (event->handled())
    return;

  // Confirm existing composition text on TAP gesture, to make sure the input
  // caret won't be moved with an ongoing composition text.
  if (event->type() == ui::EventType::kGestureTap) {
    FinishImeCompositionSession();
  }

  blink::WebGestureEvent gesture = ui::MakeWebGestureEvent(*event);
  if (event->type() == ui::EventType::kGestureTapDown) {
    // Webkit does not stop a fling-scroll on tap-down. So explicitly send an
    // event to stop any in-progress flings.
    blink::WebGestureEvent fling_cancel = gesture;
    fling_cancel.SetType(blink::WebInputEvent::Type::kGestureFlingCancel);
    fling_cancel.SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
    fling_cancel.data.fling_cancel.prevent_boosting = false;
    fling_cancel.data.fling_cancel.target_viewport = false;
    if (ShouldRouteEvents()) {
      host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
          host_view_, &fling_cancel, ui::LatencyInfo());
    } else {
      host_->ForwardGestureEvent(fling_cancel);
    }
  }

  if (gesture.GetType() != blink::WebInputEvent::Type::kUndefined) {
    if (event->type() == ui::EventType::kGestureScrollBegin) {
      // If there is a current scroll going on and a new scroll that isn't
      // wheel based send a synthetic wheel event with kPhaseEnded to cancel
      // the current scroll.
      mouse_wheel_phase_handler_.DispatchPendingWheelEndEvent();
      mouse_wheel_phase_handler_.SendWheelEndForTouchpadScrollingIfNeeded(
          ShouldRouteEvents());
    } else if (event->type() == ui::EventType::kScrollFlingStart) {
      RecordAction(base::UserMetricsAction("TouchscreenScrollFling"));
    }

    if (event->type() == ui::EventType::kGestureScrollEnd ||
        event->type() == ui::EventType::kScrollFlingStart) {
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
      host_->GetRenderInputRouter()->ForwardGestureEventWithLatencyInfo(
          gesture, *event->latency());
    }
  }

  // If a gesture is not processed by the webpage, then WebKit processes it
  // (e.g. generates synthetic mouse events).
  event->SetHandled();
}

void RenderWidgetHostViewEventHandler::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  mouse_wheel_phase_handler_.GestureEventAck(event, ack_result);
  HandleSwipeToMoveCursorGestureAck(event);
}

bool RenderWidgetHostViewEventHandler::CanRendererHandleEvent(
    const ui::MouseEvent* event,
    bool mouse_locked,
    bool selection_popup) const {
  if (event->type() == ui::EventType::kMouseCaptureChanged) {
    return false;
  }

  if (event->type() == ui::EventType::kMouseExited) {
    if (mouse_locked || selection_popup)
      return false;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // Don't forward the mouse leave message which is received when the context
    // menu is displayed by the page. This confuses the page and causes state
    // changes.
    if (host_->delegate() && host_->delegate()->IsShowingContextMenuOnPage())
      return false;
#endif
    return true;
  }

#if BUILDFLAG(IS_WIN)
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
      /* keep_selection */ true);
  host_view_->ImeCancelComposition();
}

void RenderWidgetHostViewEventHandler::ForwardMouseEventToParent(
    ui::MouseEvent* event) {
  // Needed to propagate mouse event to |window_->parent()->delegate()|, but
  // note that it might be something other than a WebContentsViewAura instance.
  // TODO(pkotwicz): Find a better way of doing this.

  if (event->flags() & ui::EF_FROM_TOUCH)
    return;

  if (!window_->parent() || !window_->parent()->delegate())
    return;

  // Take a copy of |event|, to avoid ConvertLocationToTarget mutating the
  // event.
  std::unique_ptr<ui::Event> event_copy = event->Clone();
  ui::MouseEvent* mouse_event = static_cast<ui::MouseEvent*>(event_copy.get());
  mouse_event->ConvertLocationToTarget(window_.get(), window_->parent());
  window_->parent()->delegate()->OnMouseEvent(mouse_event);
  if (mouse_event->handled())
    event->SetHandled();
}

void RenderWidgetHostViewEventHandler::HandleGestureForTouchSelection(
    ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureLongPress:
      delegate_->selection_controller()->HandleLongPressEvent(
          event->time_stamp(), event->location_f());
      break;
    case ui::EventType::kGestureTapDown:
      if (event->details().tap_down_count() == 2) {
        delegate_->selection_controller()->HandleDoublePressEvent(
            event->time_stamp(), event->location_f());
      }
      break;
    case ui::EventType::kGestureTap:
      delegate_->selection_controller()->HandleTapEvent(
          event->location_f(), event->details().tap_count());
      break;
    case ui::EventType::kGestureScrollBegin:
      delegate_->selection_controller_client()->OnScrollStarted();
      break;
    case ui::EventType::kGestureScrollEnd:
      delegate_->selection_controller_client()->OnScrollCompleted();
      break;
    default:
      break;
  }
}

void RenderWidgetHostViewEventHandler::HandleSwipeToMoveCursorGestureAck(
    const blink::WebGestureEvent& event) {
  if (!delegate_->selection_controller_client()) {
    return;
  }

  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin: {
      if (!event.data.scroll_begin.cursor_control) {
        break;
      }
      swipe_to_move_cursor_activated_ = true;
      delegate_->selection_controller_client()->OnSwipeToMoveCursorBegin();
      break;
    }
    case blink::WebInputEvent::Type::kGestureScrollEnd: {
      if (!swipe_to_move_cursor_activated_) {
        break;
      }
      swipe_to_move_cursor_activated_ = false;
      delegate_->selection_controller_client()->OnSwipeToMoveCursorEnd();
      break;
    }
    default:
      break;
  }
}

void RenderWidgetHostViewEventHandler::HandleMouseEventWhileLocked(
    ui::MouseEvent* event) {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());

  DCHECK(!cursor_client || !cursor_client->IsCursorVisible());

  if (event->type() == ui::EventType::kMousewheel) {
    HandleMouseWheelEvent(event);
  } else {
    // If we receive non client mouse messages while we are in the locked state
    // it probably means that the mouse left the borders of our window and
    // needs to be moved back to the center.
    if ((event->flags() & ui::EF_IS_NON_CLIENT) &&
        !window_->GetHost()->SupportsMouseLock()) {
      MoveCursorToCenter(event);
      return;
    }

    blink::WebMouseEvent mouse_event = ui::MakeWebMouseEvent(*event);

    ModifyEventMovementAndCoords(*event, &mouse_event);

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
      if (event->type() == ui::EventType::kMousePressed) {
        SetKeyboardFocus();
      }
    }

    // Check if the mouse has reached the border and needs to be centered.
    if (ShouldMoveToCenter(gfx::PointF(mouse_event.PositionInScreen()))) {
      MoveCursorToCenter(event);
    }
  }
  if (!ShouldGenerateAppCommand(event))
    event->SetHandled();
}

void RenderWidgetHostViewEventHandler::ModifyEventMovementAndCoords(
    const ui::MouseEvent& ui_mouse_event,
    blink::WebMouseEvent* event) {
  // This logic is similar to |is_move_to_center_event| check when
  // consolidated_movement disabled. We can not guarantee that |MoveCursorTo|
  // is taking effect immediately, so wait for the event that has matching
  // coordinates to marked as synthesized event.
  if (mouse_locked_ && MatchesSynthesizedMovePosition(*event)) {
    event->SetModifiers(event->GetModifiers() |
                        blink::WebInputEvent::Modifiers::kRelativeMotionEvent);
    synthetic_move_position_.reset();
    return;
  }

  // Under mouse lock, coordinates of mouse are locked to what they were when
  // mouse lock was entered.
  if (!mouse_locked_) {
    unlocked_mouse_position_ = event->PositionInWidget();
    unlocked_global_mouse_position_ = event->PositionInScreen();
  }
}

void RenderWidgetHostViewEventHandler::MoveCursorToCenter(
    ui::MouseEvent* event) {
  DCHECK(!window_->GetHost()->SupportsMouseLock());

  gfx::Point center(gfx::Rect(window_->bounds().size()).CenterPoint());
  gfx::Point center_in_screen(window_->GetBoundsInScreen().CenterPoint());
  window_->MoveCursorTo(center);
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40547981): This is a workaround for a bug from Windows
  // update 16299, and should be remove once the bug is fixed in OS. Send a
  // synthesized event to update the blink side states.
  global_mouse_position_ = gfx::PointF(center_in_screen);
  if (event) {
    blink::WebMouseEvent mouse_event = ui::MakeWebMouseEvent(*event);
    mouse_event.SetType(blink::WebMouseEvent::Type::kMouseMove);
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
  if (event.GetType() == blink::WebInputEvent::Type::kMouseMove &&
      synthetic_move_position_.has_value()) {
    if (IsFractionalScaleFactor(host_view_->GetDeviceScaleFactor())) {
      // For fractional scale factors, the conversion from pixels to dip and
      // vice versa could result in off by 1 or 2 errors which hurts us because
      // the artificial move to center event cause the cursor to bounce around
      // the center of the screen leading to the lock operation not working
      // correctly. Workaround is to treat a mouse move or drag event off by
      // atmost 2 px from the center as a move to center event.
      // TODO(crbug.com/41474713): figure out a way to avoid the conversion
      // error.
      return ((std::abs(event.PositionInScreen().x() -
                        synthetic_move_position_->x()) <= 2) &&
              (std::abs(event.PositionInScreen().y() -
                        synthetic_move_position_->y()) <= 2));
    } else {
      return synthetic_move_position_.value() ==
             gfx::ToRoundedPoint(event.PositionInScreen());
    }
  }
  return false;
}

void RenderWidgetHostViewEventHandler::SetKeyboardFocus() {
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
#if BUILDFLAG(IS_WIN)
  if (mouse_locked_unadjusted_movement_)
    return false;
#endif

  if (window_->GetHost()->SupportsMouseLock())
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
  if (!host_->delegate()->IsWidgetForPrimaryMainFrame(host_))
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
  host_->GetRenderInputRouter()->ForwardTouchEventWithLatencyInfo(event,
                                                                  latency);
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
