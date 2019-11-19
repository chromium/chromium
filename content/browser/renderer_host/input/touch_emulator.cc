// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_emulator.h"

#include "base/containers/queue.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/motion_event_web.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "content/common/input/web_touch_event_traits.h"
#include "content/grit/content_resources.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/platform/web_cursor_info.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/image/image.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

ui::GestureProvider::Config GetEmulatorGestureProviderConfig(
    ui::GestureProviderConfigType config_type,
    TouchEmulator::Mode mode) {
  ui::GestureProvider::Config config =
      ui::GetGestureProviderConfig(config_type);
  config.gesture_begin_end_types_enabled = false;
  config.gesture_detector_config.swipe_enabled = false;
  config.gesture_detector_config.two_finger_tap_enabled = false;
  if (mode == TouchEmulator::Mode::kInjectingTouchEvents) {
    config.gesture_detector_config.longpress_timeout = base::TimeDelta::Max();
    config.gesture_detector_config.showpress_timeout = base::TimeDelta::Max();
  }
  return config;
}

int ModifiersWithoutMouseButtons(const WebInputEvent& event) {
  const int all_buttons = WebInputEvent::kLeftButtonDown |
                          WebInputEvent::kMiddleButtonDown |
                          WebInputEvent::kRightButtonDown;
  return event.GetModifiers() & ~all_buttons;
}

// Time between two consecutive mouse moves, during which second mouse move
// is not converted to touch.
constexpr base::TimeDelta kMouseMoveDropInterval =
    base::TimeDelta::FromMilliseconds(5);

} // namespace

TouchEmulator::TouchEmulator(TouchEmulatorClient* client,
                             float device_scale_factor)
    : client_(client),
      gesture_provider_config_type_(
          ui::GestureProviderConfigType::CURRENT_PLATFORM),
      double_tap_enabled_(true),
      use_2x_cursors_(false),
      pinch_gesture_mode_for_testing_(false),
      emulated_stream_active_sequence_count_(0),
      native_stream_active_sequence_count_(0),
      last_emulated_start_target_(nullptr),
      pending_taps_count_(0) {
  DCHECK(client_);
  ResetState();
  InitCursors(device_scale_factor, true);
}

TouchEmulator::~TouchEmulator() {
  // We cannot cleanup properly in destructor, as we need roundtrip to the
  // renderer for ack. Instead, the owner should call Disable, and only
  // destroy this object when renderer is dead.
}

void TouchEmulator::ResetState() {
  last_mouse_event_was_move_ = false;
  last_mouse_move_timestamp_ = base::TimeTicks();
  mouse_pressed_ = false;
  shift_pressed_ = false;
  suppress_next_fling_cancel_ = false;
  pinch_scale_ = 1.f;
  pinch_gesture_active_ = false;
}

void TouchEmulator::Enable(Mode mode,
                           ui::GestureProviderConfigType config_type) {
  if (gesture_provider_ && mode_ != mode)
    client_->SetCursor(pointer_cursor_);

  if (!gesture_provider_ || gesture_provider_config_type_ != config_type ||
      mode_ != mode) {
    mode_ = mode;
    gesture_provider_config_type_ = config_type;
    gesture_provider_.reset(new ui::FilteredGestureProvider(
        GetEmulatorGestureProviderConfig(config_type, mode), this));
    gesture_provider_->SetDoubleTapSupportForPageEnabled(double_tap_enabled_);
    // TODO(dgozman): Use synthetic secondary touch to support multi-touch.
    gesture_provider_->SetMultiTouchZoomSupportEnabled(
        mode != Mode::kEmulatingTouchFromMouse);
  }

  UpdateCursor();
}

void TouchEmulator::Disable() {
  if (!enabled())
    return;

  mode_ = Mode::kEmulatingTouchFromMouse;
  CancelTouch();
  gesture_provider_.reset();
  base::queue<base::OnceClosure> empty;
  injected_touch_completion_callbacks_.swap(empty);
  client_->SetCursor(pointer_cursor_);
  ResetState();
}

void TouchEmulator::SetDeviceScaleFactor(float device_scale_factor) {
  if (!InitCursors(device_scale_factor, false))
    return;
  if (enabled())
    UpdateCursor();
}

void TouchEmulator::SetDoubleTapSupportForPageEnabled(bool enabled) {
  double_tap_enabled_ = enabled;
  if (gesture_provider_)
    gesture_provider_->SetDoubleTapSupportForPageEnabled(enabled);
}

bool TouchEmulator::InitCursors(float device_scale_factor, bool force) {
  bool use_2x = device_scale_factor > 1.5f;
  if (use_2x == use_2x_cursors_ && !force)
    return false;
  use_2x_cursors_ = use_2x;
  float cursor_scale_factor = use_2x ? 2.f : 1.f;
  cursor_size_ = InitCursorFromResource(&touch_cursor_,
      cursor_scale_factor,
      use_2x ? IDR_DEVTOOLS_TOUCH_CURSOR_ICON_2X :
          IDR_DEVTOOLS_TOUCH_CURSOR_ICON);
  InitCursorFromResource(&pinch_cursor_,
      cursor_scale_factor,
      use_2x ? IDR_DEVTOOLS_PINCH_CURSOR_ICON_2X :
          IDR_DEVTOOLS_PINCH_CURSOR_ICON);

  pointer_cursor_ = WebCursor(CursorInfo(ui::CursorType::kPointer));
  return true;
}

gfx::SizeF TouchEmulator::InitCursorFromResource(
    WebCursor* cursor, float scale, int resource_id) {
  gfx::Image& cursor_image =
      content::GetContentClient()->GetNativeImageNamed(resource_id);
  CursorInfo cursor_info;
  cursor_info.type = ui::CursorType::kCustom;
  cursor_info.image_scale_factor = scale;
  cursor_info.custom_image = cursor_image.AsBitmap();
  cursor_info.hotspot =
      gfx::Point(cursor_image.Width() / 2, cursor_image.Height() / 2);

  *cursor = WebCursor(cursor_info);
  return gfx::ScaleSize(gfx::SizeF(cursor_image.Size()), 1.f / scale);
}

bool TouchEmulator::HandleMouseEvent(const WebMouseEvent& mouse_event,
                                     RenderWidgetHostViewBase* target_view) {
  if (!enabled() || mode_ != Mode::kEmulatingTouchFromMouse)
    return false;

  UpdateCursor();

  if (mouse_event.button == WebMouseEvent::Button::kRight &&
      mouse_event.GetType() == WebInputEvent::kMouseDown) {
    client_->ShowContextMenuAtPoint(
        gfx::Point(mouse_event.PositionInWidget().x,
                   mouse_event.PositionInWidget().y),
        ui::MENU_SOURCE_MOUSE, target_view);
  }

  if (mouse_event.button != WebMouseEvent::Button::kLeft)
    return true;

  if (mouse_event.GetType() == WebInputEvent::kMouseMove) {
    if (last_mouse_event_was_move_ &&
        mouse_event.TimeStamp() <
            last_mouse_move_timestamp_ + kMouseMoveDropInterval)
      return true;

    last_mouse_event_was_move_ = true;
    last_mouse_move_timestamp_ = mouse_event.TimeStamp();
  } else {
    last_mouse_event_was_move_ = false;
  }

  if (mouse_event.GetType() == WebInputEvent::kMouseDown)
    mouse_pressed_ = true;
  else if (mouse_event.GetType() == WebInputEvent::kMouseUp)
    mouse_pressed_ = false;

  UpdateShiftPressed((mouse_event.GetModifiers() & WebInputEvent::kShiftKey) !=
                     0);

  if (mouse_event.GetType() != WebInputEvent::kMouseDown &&
      mouse_event.GetType() != WebInputEvent::kMouseMove &&
      mouse_event.GetType() != WebInputEvent::kMouseUp) {
    return true;
  }

  gfx::PointF pos_in_root = mouse_event.PositionInWidget();
  if (target_view)
    pos_in_root = target_view->TransformPointToRootCoordSpaceF(pos_in_root);
  FillTouchEventAndPoint(mouse_event, pos_in_root);
  HandleEmulatedTouchEvent(touch_event_, target_view);

  // Do not pass mouse events to the renderer.
  return true;
}

bool TouchEmulator::HandleMouseWheelEvent(const WebMouseWheelEvent& event) {
  if (!enabled() || mode_ != Mode::kEmulatingTouchFromMouse)
    return false;

  // Send mouse wheel for easy scrolling when there is no active touch.
  return emulated_stream_active_sequence_count_ > 0;
}

bool TouchEmulator::HandleKeyboardEvent(const WebKeyboardEvent& event) {
  if (!enabled() || mode_ != Mode::kEmulatingTouchFromMouse)
    return false;

  if (!UpdateShiftPressed((event.GetModifiers() & WebInputEvent::kShiftKey) !=
                          0))
    return false;

  if (!mouse_pressed_)
    return false;

  // Note: The necessary pinch events will be lazily inserted by
  // |OnGestureEvent| depending on the state of |shift_pressed_|, using the
  // scroll stream as the event driver.
  if (shift_pressed_) {
    // TODO(dgozman): Add secondary touch point and set anchor.
  } else {
    // TODO(dgozman): Remove secondary touch point and anchor.
  }

  // Never block keyboard events.
  return false;
}

bool TouchEmulator::HandleTouchEvent(const blink::WebTouchEvent& event) {
  // Block native event when emulated touch stream is active.
  if (emulated_stream_active_sequence_count_)
    return true;

  bool is_sequence_start = WebTouchEventTraits::IsTouchSequenceStart(event);
  // Do not allow middle-sequence event to pass through, if start was blocked.
  if (!native_stream_active_sequence_count_ && !is_sequence_start)
    return true;

  if (is_sequence_start)
    native_stream_active_sequence_count_++;
  return false;
}

bool TouchEmulator::HandleEmulatedTouchEvent(
    blink::WebTouchEvent event,
    RenderWidgetHostViewBase* target_view) {
  DCHECK(gesture_provider_);
  event.unique_touch_event_id = ui::GetNextTouchEventId();
  auto result = gesture_provider_->OnTouchEvent(MotionEventWeb(event));
  if (!result.succeeded)
    return true;

  const bool event_consumed = true;
  const bool is_source_touch_event_set_non_blocking = false;
  // Block emulated event when emulated native stream is active.
  if (native_stream_active_sequence_count_) {
    gesture_provider_->OnTouchEventAck(event.unique_touch_event_id,
                                       event_consumed,
                                       is_source_touch_event_set_non_blocking);
    return true;
  }

  bool is_sequence_start = WebTouchEventTraits::IsTouchSequenceStart(event);
  // Do not allow middle-sequence event to pass through, if start was blocked.
  if (!emulated_stream_active_sequence_count_ && !is_sequence_start) {
    gesture_provider_->OnTouchEventAck(event.unique_touch_event_id,
                                       event_consumed,
                                       is_source_touch_event_set_non_blocking);
    return true;
  }

  if (is_sequence_start) {
    emulated_stream_active_sequence_count_++;
    last_emulated_start_target_ = target_view;
  }

  event.moved_beyond_slop_region = result.moved_beyond_slop_region;
  client_->ForwardEmulatedTouchEvent(event, target_view);
  return false;
}

bool TouchEmulator::HandleTouchEventAck(
    const blink::WebTouchEvent& event, InputEventAckState ack_result) {
  bool is_sequence_end = WebTouchEventTraits::IsTouchSequenceEnd(event);
  if (emulated_stream_active_sequence_count_) {
    if (is_sequence_end)
      emulated_stream_active_sequence_count_--;

    int taps_count_before = pending_taps_count_;
    const bool event_consumed = ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;
    if (gesture_provider_) {
      gesture_provider_->OnTouchEventAck(
          event.unique_touch_event_id, event_consumed,
          InputEventAckStateIsSetNonBlocking(ack_result));
    }
    if (pending_taps_count_ == taps_count_before)
      OnInjectedTouchCompleted();
    return true;
  }

  // We may have not seen native touch sequence start (when created in the
  // middle of a sequence), so don't decrement sequence count below zero.
  if (is_sequence_end && native_stream_active_sequence_count_)
    native_stream_active_sequence_count_--;
  return false;
}

void TouchEmulator::OnGestureEventAck(const WebGestureEvent& event,
                                      RenderWidgetHostViewBase*) {
  if (event.GetType() != WebInputEvent::kGestureTap)
    return;
  if (pending_taps_count_) {
    pending_taps_count_--;
    OnInjectedTouchCompleted();
  }
}

void TouchEmulator::OnViewDestroyed(RenderWidgetHostViewBase* destroyed_view) {
  if (destroyed_view != last_emulated_start_target_)
    return;

  last_emulated_start_target_ = nullptr;
  emulated_stream_active_sequence_count_ = 0;
}

void TouchEmulator::OnGestureEvent(const ui::GestureEventData& gesture) {
  WebGestureEvent gesture_event =
      ui::CreateWebGestureEventFromGestureEventData(gesture);

  DCHECK(gesture_event.unique_touch_event_id);

  switch (gesture_event.GetType()) {
    case WebInputEvent::kUndefined:
      NOTREACHED() << "Undefined WebInputEvent type";
      // Bail without sending the junk event to the client.
      return;

    case WebInputEvent::kGestureScrollBegin:
      client_->ForwardEmulatedGestureEvent(gesture_event);
      // PinchBegin must always follow ScrollBegin.
      if (InPinchGestureMode())
        PinchBegin(gesture_event);
      break;

    case WebInputEvent::kGestureScrollUpdate:
      if (InPinchGestureMode()) {
        // Convert scrolls to pinches while shift is pressed.
        if (!pinch_gesture_active_)
          PinchBegin(gesture_event);
        else
          PinchUpdate(gesture_event);
      } else {
        // Pass scroll update further. If shift was released, end the pinch.
        if (pinch_gesture_active_)
          PinchEnd(gesture_event);
        client_->ForwardEmulatedGestureEvent(gesture_event);
      }
      break;

    case WebInputEvent::kGestureScrollEnd:
      // PinchEnd must precede ScrollEnd.
      if (pinch_gesture_active_)
        PinchEnd(gesture_event);
      client_->ForwardEmulatedGestureEvent(gesture_event);
      break;

    case WebInputEvent::kGestureFlingStart:
      // PinchEnd must precede FlingStart.
      if (pinch_gesture_active_)
        PinchEnd(gesture_event);
      if (InPinchGestureMode()) {
        // No fling in pinch mode. Forward scroll end instead of fling start.
        suppress_next_fling_cancel_ = true;
        ScrollEnd(gesture_event);
      } else {
        suppress_next_fling_cancel_ = false;
        client_->ForwardEmulatedGestureEvent(gesture_event);
      }
      break;

    case WebInputEvent::kGestureFlingCancel:
      // If fling start was suppressed, we should not send fling cancel either.
      if (!suppress_next_fling_cancel_)
        client_->ForwardEmulatedGestureEvent(gesture_event);
      suppress_next_fling_cancel_ = false;
      break;

    case WebInputEvent::kGestureTap:
      pending_taps_count_++;
      client_->ForwardEmulatedGestureEvent(gesture_event);
      break;

    default:
      // Everything else goes through.
      client_->ForwardEmulatedGestureEvent(gesture_event);
  }
}

bool TouchEmulator::RequiresDoubleTapGestureEvents() const {
  return true;
}

void TouchEmulator::InjectTouchEvent(const blink::WebTouchEvent& event,
                                     RenderWidgetHostViewBase* target_view,
                                     base::OnceClosure callback) {
  DCHECK(enabled() && mode_ == Mode::kInjectingTouchEvents);
  touch_event_ = event;
  injected_touch_completion_callbacks_.push(std::move(callback));
  if (HandleEmulatedTouchEvent(touch_event_, target_view))
    OnInjectedTouchCompleted();
}

void TouchEmulator::OnInjectedTouchCompleted() {
  if (injected_touch_completion_callbacks_.empty())
    return;
  if (!injected_touch_completion_callbacks_.front().is_null())
    std::move(injected_touch_completion_callbacks_.front()).Run();
  injected_touch_completion_callbacks_.pop();
}

void TouchEmulator::CancelTouch() {
  if (!emulated_stream_active_sequence_count_ || !enabled() ||
      mode_ != Mode::kEmulatingTouchFromMouse) {
    return;
  }

  WebTouchEventTraits::ResetTypeAndTouchStates(
      WebInputEvent::kTouchCancel, ui::EventTimeForNow(), &touch_event_);
  DCHECK(gesture_provider_);
  if (gesture_provider_->GetCurrentDownEvent())
    HandleEmulatedTouchEvent(touch_event_, last_emulated_start_target_);
}

void TouchEmulator::UpdateCursor() {
  DCHECK(enabled());
  if (mode_ == Mode::kEmulatingTouchFromMouse)
    client_->SetCursor(InPinchGestureMode() ? pinch_cursor_ : touch_cursor_);
}

bool TouchEmulator::UpdateShiftPressed(bool shift_pressed) {
  DCHECK(enabled() && mode_ == Mode::kEmulatingTouchFromMouse);
  if (shift_pressed_ == shift_pressed)
    return false;
  shift_pressed_ = shift_pressed;
  UpdateCursor();
  return true;
}

void TouchEmulator::PinchBegin(const WebGestureEvent& event) {
  DCHECK(InPinchGestureMode());
  DCHECK(!pinch_gesture_active_);
  pinch_gesture_active_ = true;
  pinch_anchor_ = event.PositionInWidget();
  pinch_scale_ = 1.f;
  WebGestureEvent pinch_event =
      GetPinchGestureEvent(WebInputEvent::kGesturePinchBegin, event);
  client_->ForwardEmulatedGestureEvent(pinch_event);
}

void TouchEmulator::PinchUpdate(const WebGestureEvent& event) {
  DCHECK(pinch_gesture_active_);
  float dy = pinch_anchor_.y() - event.PositionInWidget().y;
  float scale = exp(dy * 0.002f);
  WebGestureEvent pinch_event =
      GetPinchGestureEvent(WebInputEvent::kGesturePinchUpdate, event);
  pinch_event.data.pinch_update.scale = scale / pinch_scale_;
  client_->ForwardEmulatedGestureEvent(pinch_event);
  pinch_scale_ = scale;
}

void TouchEmulator::PinchEnd(const WebGestureEvent& event) {
  DCHECK(pinch_gesture_active_);
  pinch_gesture_active_ = false;
  WebGestureEvent pinch_event =
      GetPinchGestureEvent(WebInputEvent::kGesturePinchEnd, event);
  client_->ForwardEmulatedGestureEvent(pinch_event);
}

void TouchEmulator::ScrollEnd(const WebGestureEvent& event) {
  WebGestureEvent scroll_event(
      WebInputEvent::kGestureScrollEnd, ModifiersWithoutMouseButtons(event),
      event.TimeStamp(), blink::WebGestureDevice::kTouchscreen);
  scroll_event.unique_touch_event_id = event.unique_touch_event_id;
  client_->ForwardEmulatedGestureEvent(scroll_event);
}

WebGestureEvent TouchEmulator::GetPinchGestureEvent(
    WebInputEvent::Type type,
    const WebGestureEvent& original_event) {
  WebGestureEvent event(type, ModifiersWithoutMouseButtons(original_event),
                        original_event.TimeStamp(),
                        blink::WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(pinch_anchor_);
  event.unique_touch_event_id = original_event.unique_touch_event_id;
  return event;
}

void TouchEmulator::FillTouchEventAndPoint(const WebMouseEvent& mouse_event,
                                           const gfx::PointF& pos_in_root) {
  WebInputEvent::Type eventType;
  switch (mouse_event.GetType()) {
    case WebInputEvent::kMouseDown:
      eventType = WebInputEvent::kTouchStart;
      break;
    case WebInputEvent::kMouseMove:
      eventType = WebInputEvent::kTouchMove;
      break;
    case WebInputEvent::kMouseUp:
      eventType = WebInputEvent::kTouchEnd;
      break;
    default:
      eventType = WebInputEvent::kUndefined;
      NOTREACHED() << "Invalid event for touch emulation: "
                   << mouse_event.GetType();
  }
  touch_event_.touches_length = 1;
  touch_event_.SetModifiers(ModifiersWithoutMouseButtons(mouse_event));
  WebTouchEventTraits::ResetTypeAndTouchStates(
      eventType, mouse_event.TimeStamp(), &touch_event_);
  WebTouchPoint& point = touch_event_.touches[0];
  point.id = 0;
  point.radius_x = 0.5f * cursor_size_.width();
  point.radius_y = 0.5f * cursor_size_.height();
  point.force = eventType == WebInputEvent::kTouchEnd ? 0.f : 1.f;
  point.rotation_angle = 0.f;
  // We need to convert this to the root-view's coord space, otherwise the
  // GestureRecognizer will potentially receive events for a moving widget,
  // for example when scroll bubbling is taking place. The GestureRecognizer
  // isn't designed to handle that.
  point.SetPositionInWidget(pos_in_root);
  point.SetPositionInScreen(mouse_event.PositionInScreen().x,
                            mouse_event.PositionInScreen().y);
  point.tilt_x = 0;
  point.tilt_y = 0;
  point.pointer_type = blink::WebPointerProperties::PointerType::kTouch;
}

bool TouchEmulator::InPinchGestureMode() const {
  return shift_pressed_ || pinch_gesture_mode_for_testing_;
}

void TouchEmulator::SetPinchGestureModeForTesting(bool pinch_gesture_mode) {
  pinch_gesture_mode_for_testing_ = pinch_gesture_mode;
}

}  // namespace content
