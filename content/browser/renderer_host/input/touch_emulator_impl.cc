// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_emulator_impl.h"

#include <memory>
#include <utility>

#include "base/containers/queue.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/input/render_widget_host_view_input.h"
#include "components/input/web_touch_event_traits.h"
#include "content/browser/renderer_host/input/motion_event_web.h"
#include "content/common/input/events_helper.h"
#include "content/grit/content_resources.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/skia_conversions.h"
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
    input::TouchEmulator::Mode mode) {
  ui::GestureProvider::Config config =
      ui::GetGestureProviderConfig(config_type);
  config.gesture_begin_end_types_enabled = false;
  config.gesture_detector_config.swipe_enabled = false;
  config.gesture_detector_config.two_finger_tap_enabled = false;
  if (mode == input::TouchEmulator::Mode::kInjectingTouchEvents) {
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
constexpr base::TimeDelta kMouseMoveDropInterval = base::Milliseconds(5);

}  // namespace

TouchEmulatorImpl::TouchEmulatorImpl(input::TouchEmulatorClient* client,
                                     float device_scale_factor)
    : client_(client),
      gesture_provider_config_type_(
          ui::GestureProviderConfigType::CURRENT_PLATFORM),
      double_tap_enabled_(true),
      pinch_gesture_mode_for_testing_(false),
      emulated_stream_active_sequence_count_(0),
      native_stream_active_sequence_count_(0),
      last_emulated_start_target_(nullptr),
      pending_taps_count_(0) {
  DCHECK(client_);
  ResetState();
  SetDeviceScaleFactor(device_scale_factor);
}

TouchEmulatorImpl::~TouchEmulatorImpl() {
  // We cannot cleanup properly in destructor, as we need roundtrip to the
  // renderer for ack. Instead, the owner should call Disable, and only
  // destroy this object when renderer is dead.
}

void TouchEmulatorImpl::ResetState() {
  last_mouse_event_was_move_ = false;
  last_mouse_move_timestamp_ = base::TimeTicks();
  mouse_pressed_ = false;
  shift_pressed_ = false;
  suppress_next_fling_cancel_ = false;
  pinch_scale_ = 1.f;
  pinch_gesture_active_ = false;
}

void TouchEmulatorImpl::Enable(Mode mode,
                               ui::GestureProviderConfigType config_type) {
  if (gesture_provider_ && mode_ != mode)
    client_->SetCursor(ui::mojom::CursorType::kPointer);

  if (!gesture_provider_ || gesture_provider_config_type_ != config_type ||
      mode_ != mode) {
    mode_ = mode;
    gesture_provider_config_type_ = config_type;
    gesture_provider_ = std::make_unique<ui::FilteredGestureProvider>(
        GetEmulatorGestureProviderConfig(config_type, mode), this);
    gesture_provider_->SetDoubleTapSupportForPageEnabled(double_tap_enabled_);
    // TODO(dgozman): Use synthetic secondary touch to support multi-touch.
    gesture_provider_->SetMultiTouchZoomSupportEnabled(
        mode != Mode::kEmulatingTouchFromMouse);
  }

  UpdateCursor();
}

void TouchEmulatorImpl::Disable() {
  if (!IsEnabled())
    return;

  mode_ = Mode::kEmulatingTouchFromMouse;
  CancelTouch();
  gesture_provider_.reset();
  base::queue<base::OnceClosure> empty;
  injected_touch_completion_callbacks_.swap(empty);
  client_->SetCursor(ui::mojom::CursorType::kPointer);
  ResetState();
}

void TouchEmulatorImpl::SetDeviceScaleFactor(float device_scale_factor) {
  // Make sure the scale factor corresponds to the one of the available cursor
  // images.
  const float cursor_scale_factor = device_scale_factor < 1.5f ? 1.0f : 2.0f;
  if (cursor_scale_factor == cursor_scale_factor_) {
    return;
  }

  cursor_scale_factor_ = cursor_scale_factor;
  InitCursors();
  if (IsEnabled())
    UpdateCursor();
}

void TouchEmulatorImpl::SetDoubleTapSupportForPageEnabled(bool enabled) {
  double_tap_enabled_ = enabled;
  if (gesture_provider_)
    gesture_provider_->SetDoubleTapSupportForPageEnabled(enabled);
}

bool TouchEmulatorImpl::IsEnabled() const {
  return !!gesture_provider_;
}

void TouchEmulatorImpl::InitCursors() {
  touch_cursor_ = InitCursorFromResource(
      cursor_scale_factor_ == 1.0f ? IDR_DEVTOOLS_TOUCH_CURSOR_ICON
                                   : IDR_DEVTOOLS_TOUCH_CURSOR_ICON_2X);
  pinch_cursor_ = InitCursorFromResource(
      cursor_scale_factor_ == 1.0f ? IDR_DEVTOOLS_PINCH_CURSOR_ICON
                                   : IDR_DEVTOOLS_PINCH_CURSOR_ICON_2X);
  // The touch cursor is bigger. Use its size in DIPs for both cursors.
  cursor_size_ = gfx::ScaleSize(
      gfx::SizeF(
          gfx::SkISizeToSize(touch_cursor_.custom_bitmap().dimensions())),
      1 / cursor_scale_factor_);
}

ui::Cursor TouchEmulatorImpl::InitCursorFromResource(int resource_id) {
  const gfx::Image& cursor_image =
      GetContentClient()->GetNativeImageNamed(resource_id);
  SkBitmap bitmap = cursor_image.AsBitmap();
  gfx::Point hotspot(bitmap.width() / 2, bitmap.height() / 2);
  return ui::Cursor::NewCustom(std::move(bitmap), std::move(hotspot),
                               cursor_scale_factor_);
}

bool TouchEmulatorImpl::HandleMouseEvent(
    const WebMouseEvent& mouse_event,
    input::RenderWidgetHostViewInput* target_view) {
  if (!IsEnabled() || mode_ != Mode::kEmulatingTouchFromMouse)
    return false;

  UpdateCursor();

  if (mouse_event.button == WebMouseEvent::Button::kRight &&
      mouse_event.GetType() == WebInputEvent::Type::kMouseDown) {
    client_->ShowContextMenuAtPoint(
        gfx::Point(mouse_event.PositionInWidget().x(),
                   mouse_event.PositionInWidget().y()),
        ui::MENU_SOURCE_MOUSE, target_view);
  }

  if (mouse_event.button != WebMouseEvent::Button::kLeft)
    return true;

  if (mouse_event.GetType() == WebInputEvent::Type::kMouseMove) {
    if (last_mouse_event_was_move_ &&
        mouse_event.TimeStamp() <
            last_mouse_move_timestamp_ + kMouseMoveDropInterval)
      return true;

    last_mouse_event_was_move_ = true;
    last_mouse_move_timestamp_ = mouse_event.TimeStamp();
  } else {
    last_mouse_event_was_move_ = false;
  }

  if (mouse_event.GetType() == WebInputEvent::Type::kMouseDown)
    mouse_pressed_ = true;
  else if (mouse_event.GetType() == WebInputEvent::Type::kMouseUp)
    mouse_pressed_ = false;

  UpdateShiftPressed((mouse_event.GetModifiers() & WebInputEvent::kShiftKey) !=
                     0);

  if (mouse_event.GetType() != WebInputEvent::Type::kMouseDown &&
      mouse_event.GetType() != WebInputEvent::Type::kMouseMove &&
      mouse_event.GetType() != WebInputEvent::Type::kMouseUp) {
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

bool TouchEmulatorImpl::HandleMouseWheelEvent(const WebMouseWheelEvent& event) {
  if (!IsEnabled() || mode_ != Mode::kEmulatingTouchFromMouse)
    return false;

  // Send mouse wheel for easy scrolling when there is no active touch.
  return emulated_stream_active_sequence_count_ > 0;
}

bool TouchEmulatorImpl::HandleKeyboardEvent(const WebKeyboardEvent& event) {
  if (!IsEnabled() || mode_ != Mode::kEmulatingTouchFromMouse)
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

bool TouchEmulatorImpl::HandleTouchEvent(const blink::WebTouchEvent& event) {
  // Block native event when emulated touch stream is active.
  if (emulated_stream_active_sequence_count_)
    return true;

  bool is_sequence_start = event.IsTouchSequenceStart();
  // Do not allow middle-sequence event to pass through, if start was blocked.
  if (!native_stream_active_sequence_count_ && !is_sequence_start)
    return true;

  if (is_sequence_start)
    native_stream_active_sequence_count_++;
  return false;
}

bool TouchEmulatorImpl::HandleEmulatedTouchEvent(
    blink::WebTouchEvent event,
    input::RenderWidgetHostViewInput* target_view) {
  DCHECK(gesture_provider_);
  event.unique_touch_event_id = ui::GetNextTouchEventId();
  auto result = gesture_provider_->OnTouchEvent(MotionEventWeb(event));
  if (!result.succeeded)
    return true;

  const bool event_consumed = true;
  const bool is_source_touch_event_set_blocking = false;
  // Block emulated event when emulated native stream is active.
  if (native_stream_active_sequence_count_) {
    gesture_provider_->OnTouchEventAck(event.unique_touch_event_id,
                                       event_consumed,
                                       is_source_touch_event_set_blocking);
    return true;
  }

  bool is_sequence_start = event.IsTouchSequenceStart();
  // Do not allow middle-sequence event to pass through, if start was blocked.
  if (!emulated_stream_active_sequence_count_ && !is_sequence_start) {
    gesture_provider_->OnTouchEventAck(event.unique_touch_event_id,
                                       event_consumed,
                                       is_source_touch_event_set_blocking);
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

bool TouchEmulatorImpl::HandleTouchEventAck(
    const blink::WebTouchEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  bool is_sequence_end = event.IsTouchSequenceEnd();
  if (emulated_stream_active_sequence_count_) {
    if (is_sequence_end)
      emulated_stream_active_sequence_count_--;

    int taps_count_before = pending_taps_count_;
    const bool event_consumed =
        ack_result == blink::mojom::InputEventResultState::kConsumed;
    if (gesture_provider_) {
      gesture_provider_->OnTouchEventAck(
          event.unique_touch_event_id, event_consumed,
          InputEventResultStateIsSetBlocking(ack_result));
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

void TouchEmulatorImpl::OnGestureEventAck(const WebGestureEvent& event,
                                          input::RenderWidgetHostViewInput*) {
  if (event.GetType() != WebInputEvent::Type::kGestureTap)
    return;
  if (pending_taps_count_) {
    pending_taps_count_--;
    OnInjectedTouchCompleted();
  }
}

void TouchEmulatorImpl::OnViewDestroyed(
    input::RenderWidgetHostViewInput* destroyed_view) {
  if (destroyed_view != last_emulated_start_target_)
    return;

  emulated_stream_active_sequence_count_ = 0;
  last_emulated_start_target_ = nullptr;

  // If we aren't enabled, we can just stop here.
  if (!IsEnabled()) {
    return;
  }

  // TouchEmulator is still enabled. To reset the state go through a
  // Disable/Enable sequence to destroy the GestureRecognizer otherwise it will
  // be left in an unknown state because the associated view was destroyed.
  ui::GestureProviderConfigType config_type = gesture_provider_config_type_;
  Mode mode = mode_;
  Disable();
  Enable(mode, config_type);
}

void TouchEmulatorImpl::OnGestureEvent(const ui::GestureEventData& gesture) {
  WebGestureEvent gesture_event =
      ui::CreateWebGestureEventFromGestureEventData(gesture);

  DCHECK(gesture_event.unique_touch_event_id);

  switch (gesture_event.GetType()) {
    case WebInputEvent::Type::kUndefined:
      NOTREACHED_IN_MIGRATION() << "Undefined WebInputEvent type";
      // Bail without sending the junk event to the client.
      return;

    case WebInputEvent::Type::kGestureScrollBegin:
      client_->ForwardEmulatedGestureEvent(gesture_event);
      // PinchBegin must always follow ScrollBegin.
      if (InPinchGestureMode())
        PinchBegin(gesture_event);
      break;

    case WebInputEvent::Type::kGestureScrollUpdate:
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

    case WebInputEvent::Type::kGestureScrollEnd:
      // PinchEnd must precede ScrollEnd.
      if (pinch_gesture_active_)
        PinchEnd(gesture_event);
      client_->ForwardEmulatedGestureEvent(gesture_event);
      break;

    case WebInputEvent::Type::kGestureFlingStart:
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

    case WebInputEvent::Type::kGestureFlingCancel:
      // If fling start was suppressed, we should not send fling cancel either.
      if (!suppress_next_fling_cancel_)
        client_->ForwardEmulatedGestureEvent(gesture_event);
      suppress_next_fling_cancel_ = false;
      break;

    case WebInputEvent::Type::kGestureTap:
      pending_taps_count_++;
      client_->ForwardEmulatedGestureEvent(gesture_event);
      break;

    default:
      // Everything else goes through.
      client_->ForwardEmulatedGestureEvent(gesture_event);
  }
}

bool TouchEmulatorImpl::RequiresDoubleTapGestureEvents() const {
  return true;
}

void TouchEmulatorImpl::InjectTouchEvent(
    const blink::WebTouchEvent& event,
    input::RenderWidgetHostViewInput* target_view,
    base::OnceClosure callback) {
  DCHECK(IsEnabled() && mode_ == Mode::kInjectingTouchEvents);
  touch_event_ = event;
  injected_touch_completion_callbacks_.push(std::move(callback));
  if (HandleEmulatedTouchEvent(touch_event_, target_view))
    OnInjectedTouchCompleted();
}

void TouchEmulatorImpl::OnInjectedTouchCompleted() {
  if (injected_touch_completion_callbacks_.empty()) {
    return;
  }
  if (!injected_touch_completion_callbacks_.front().is_null()) {
    std::move(injected_touch_completion_callbacks_.front()).Run();
  }
  injected_touch_completion_callbacks_.pop();
}

void TouchEmulatorImpl::CancelTouch() {
  if (!emulated_stream_active_sequence_count_ || !IsEnabled() ||
      mode_ != Mode::kEmulatingTouchFromMouse) {
    return;
  }

  input::WebTouchEventTraits::ResetTypeAndTouchStates(
      WebInputEvent::Type::kTouchCancel, ui::EventTimeForNow(), &touch_event_);
  DCHECK(gesture_provider_);
  if (gesture_provider_->GetCurrentDownEvent())
    HandleEmulatedTouchEvent(touch_event_, last_emulated_start_target_);
}

void TouchEmulatorImpl::UpdateCursor() {
  DCHECK(IsEnabled());
  if (mode_ == Mode::kEmulatingTouchFromMouse)
    client_->SetCursor(InPinchGestureMode() ? pinch_cursor_ : touch_cursor_);
}

bool TouchEmulatorImpl::UpdateShiftPressed(bool shift_pressed) {
  DCHECK(IsEnabled() && mode_ == Mode::kEmulatingTouchFromMouse);
  if (shift_pressed_ == shift_pressed)
    return false;
  shift_pressed_ = shift_pressed;
  UpdateCursor();
  return true;
}

void TouchEmulatorImpl::PinchBegin(const WebGestureEvent& event) {
  DCHECK(InPinchGestureMode());
  DCHECK(!pinch_gesture_active_);
  pinch_gesture_active_ = true;
  pinch_anchor_ = event.PositionInWidget();
  pinch_scale_ = 1.f;
  WebGestureEvent pinch_event =
      GetPinchGestureEvent(WebInputEvent::Type::kGesturePinchBegin, event);
  client_->ForwardEmulatedGestureEvent(pinch_event);
}

void TouchEmulatorImpl::PinchUpdate(const WebGestureEvent& event) {
  DCHECK(pinch_gesture_active_);
  float dy = pinch_anchor_.y() - event.PositionInWidget().y();
  float scale = exp(dy * 0.002f);
  WebGestureEvent pinch_event =
      GetPinchGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, event);
  pinch_event.data.pinch_update.scale = scale / pinch_scale_;
  client_->ForwardEmulatedGestureEvent(pinch_event);
  pinch_scale_ = scale;
}

void TouchEmulatorImpl::PinchEnd(const WebGestureEvent& event) {
  DCHECK(pinch_gesture_active_);
  pinch_gesture_active_ = false;
  WebGestureEvent pinch_event =
      GetPinchGestureEvent(WebInputEvent::Type::kGesturePinchEnd, event);
  client_->ForwardEmulatedGestureEvent(pinch_event);
}

void TouchEmulatorImpl::ScrollEnd(const WebGestureEvent& event) {
  WebGestureEvent scroll_event(WebInputEvent::Type::kGestureScrollEnd,
                               ModifiersWithoutMouseButtons(event),
                               event.TimeStamp(),
                               blink::WebGestureDevice::kTouchscreen);
  scroll_event.unique_touch_event_id = event.unique_touch_event_id;
  client_->ForwardEmulatedGestureEvent(scroll_event);
}

WebGestureEvent TouchEmulatorImpl::GetPinchGestureEvent(
    WebInputEvent::Type type,
    const WebGestureEvent& original_event) {
  WebGestureEvent event(type, ModifiersWithoutMouseButtons(original_event),
                        original_event.TimeStamp(),
                        blink::WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(pinch_anchor_);
  event.unique_touch_event_id = original_event.unique_touch_event_id;
  return event;
}

void TouchEmulatorImpl::FillTouchEventAndPoint(const WebMouseEvent& mouse_event,
                                               const gfx::PointF& pos_in_root) {
  WebInputEvent::Type eventType;
  switch (mouse_event.GetType()) {
    case WebInputEvent::Type::kMouseDown:
      eventType = WebInputEvent::Type::kTouchStart;
      break;
    case WebInputEvent::Type::kMouseMove:
      eventType = WebInputEvent::Type::kTouchMove;
      break;
    case WebInputEvent::Type::kMouseUp:
      eventType = WebInputEvent::Type::kTouchEnd;
      break;
    default:
      eventType = WebInputEvent::Type::kUndefined;
      NOTREACHED_IN_MIGRATION()
          << "Invalid event for touch emulation: " << mouse_event.GetType();
  }
  touch_event_.touches_length = 1;
  touch_event_.SetModifiers(ModifiersWithoutMouseButtons(mouse_event));
  input::WebTouchEventTraits::ResetTypeAndTouchStates(
      eventType, mouse_event.TimeStamp(), &touch_event_);
  WebTouchPoint& point = touch_event_.touches[0];
  point.id = 0;
  point.radius_x = 0.5f * cursor_size_.width();
  point.radius_y = 0.5f * cursor_size_.height();
  point.force = eventType == WebInputEvent::Type::kTouchEnd ? 0.f : 1.f;
  point.rotation_angle = 0.f;
  // We need to convert this to the root-view's coord space, otherwise the
  // GestureRecognizer will potentially receive events for a moving widget,
  // for example when scroll bubbling is taking place. The GestureRecognizer
  // isn't designed to handle that.
  point.SetPositionInWidget(pos_in_root);
  point.SetPositionInScreen(mouse_event.PositionInScreen().x(),
                            mouse_event.PositionInScreen().y());
  point.tilt_x = 0;
  point.tilt_y = 0;
  point.pointer_type = blink::WebPointerProperties::PointerType::kTouch;
}

bool TouchEmulatorImpl::InPinchGestureMode() const {
  return shift_pressed_ || pinch_gesture_mode_for_testing_;
}

void TouchEmulatorImpl::SetPinchGestureModeForTesting(bool pinch_gesture_mode) {
  pinch_gesture_mode_for_testing_ = pinch_gesture_mode;
}

}  // namespace content
