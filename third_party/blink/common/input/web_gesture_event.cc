// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_gesture_event.h"

#include <limits>

#include "ui/gfx/geometry/transform.h"

namespace blink {

namespace {

bool IsContinuousGestureEvent(WebInputEvent::Type type) {
  switch (type) {
    case WebGestureEvent::Type::kGestureScrollUpdate:
    case WebGestureEvent::Type::kGesturePinchUpdate:
      return true;
    default:
      return false;
  }
}

// Returns the transform matrix corresponding to the gesture event.
gfx::Transform GetTransformForEvent(const WebGestureEvent& gesture_event) {
  gfx::Transform gesture_transform;
  if (gesture_event.GetType() == WebInputEvent::Type::kGestureScrollUpdate) {
    gesture_transform.Translate(gesture_event.data.scroll_update.delta_x,
                                gesture_event.data.scroll_update.delta_y);
  } else if (gesture_event.GetType() ==
             WebInputEvent::Type::kGesturePinchUpdate) {
    float scale = gesture_event.data.pinch_update.scale;
    gesture_transform.Translate(-gesture_event.PositionInWidget().x(),
                                -gesture_event.PositionInWidget().y());
    gesture_transform.Scale(scale, scale);
    gesture_transform.Translate(gesture_event.PositionInWidget().x(),
                                gesture_event.PositionInWidget().y());
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Invalid event type for transform retrieval: "
        << WebInputEvent::GetName(gesture_event.GetType());
  }
  return gesture_transform;
}

}  // namespace

std::unique_ptr<WebInputEvent> WebGestureEvent::Clone() const {
  return std::make_unique<WebGestureEvent>(*this);
}

bool WebGestureEvent::CanCoalesce(const WebInputEvent& event) const {
  if (!IsGestureEventType(event.GetType()))
    return false;
  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(event);
  if (GetType() != gesture_event.GetType() ||
      SourceDevice() != gesture_event.SourceDevice() ||
      GetModifiers() != gesture_event.GetModifiers())
    return false;

  if (GetType() == WebInputEvent::Type::kGestureScrollUpdate)
    return true;

  // GesturePinchUpdate scales can be combined only if they share a focal point,
  // e.g., with double-tap drag zoom.
  // Due to the imprecision of OOPIF coordinate conversions, the positions may
  // not be exactly equal, so we only require approximate equality.
  constexpr float kAnchorTolerance = 1.f;
  if (GetType() == WebInputEvent::Type::kGesturePinchUpdate &&
      (std::abs(PositionInWidget().x() - gesture_event.PositionInWidget().x()) <
       kAnchorTolerance) &&
      (std::abs(PositionInWidget().y() - gesture_event.PositionInWidget().y()) <
       kAnchorTolerance)) {
    return true;
  }

  return false;
}

void WebGestureEvent::Coalesce(const WebInputEvent& event) {
  DCHECK(CanCoalesce(event));
  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(event);
  if (GetType() == WebInputEvent::Type::kGestureScrollUpdate) {
    data.scroll_update.delta_x += gesture_event.data.scroll_update.delta_x;
    data.scroll_update.delta_y += gesture_event.data.scroll_update.delta_y;
  } else if (GetType() == WebInputEvent::Type::kGesturePinchUpdate) {
    data.pinch_update.scale *= gesture_event.data.pinch_update.scale;
    // Ensure the scale remains bounded above 0 and below Infinity so that
    // we can reliably perform operations like log on the values.
    if (data.pinch_update.scale < std::numeric_limits<float>::min())
      data.pinch_update.scale = std::numeric_limits<float>::min();
    else if (data.pinch_update.scale > std::numeric_limits<float>::max())
      data.pinch_update.scale = std::numeric_limits<float>::max();
  }
}

ui::ScrollInputType WebGestureEvent::GetScrollInputType() const {
  switch (SourceDevice()) {
    case WebGestureDevice::kTouchpad:
      DCHECK(IsGestureScroll() || IsPinchGestureEventType(GetType()));
      // TODO(crbug.com/1060268): Use of Wheel for Touchpad, especially for
      // pinch events, is confusing and not ideal. There are currently a few
      // different enum types in use across chromium code base for specifying
      // gesture input device. Since we don't want to add yet another one, the
      // most appropriate enum type to use here seems to be
      // `ui::ScrollInputType` which does not have a separate value for
      // touchpad. There is an intention to unify all these enum types. We
      // should consider having a separate touchpad device type in the unified
      // enum type.
      return ui::ScrollInputType::kWheel;
    case WebGestureDevice::kTouchscreen:
      DCHECK(IsGestureScroll() || IsPinchGestureEventType(GetType()));
      return ui::ScrollInputType::kTouchscreen;
    case WebGestureDevice::kSyntheticAutoscroll:
      DCHECK(IsGestureScroll());
      return ui::ScrollInputType::kAutoscroll;
    case WebGestureDevice::kScrollbar:
      DCHECK(IsGestureScroll());
      return ui::ScrollInputType::kScrollbar;
    case WebGestureDevice::kUninitialized:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return ui::ScrollInputType::kTouchscreen;
}

float WebGestureEvent::DeltaXInRootFrame() const {
  float delta_x = (type_ == WebInputEvent::Type::kGestureScrollBegin)
                      ? data.scroll_begin.delta_x_hint
                      : data.scroll_update.delta_x;

  bool is_percent = (type_ == WebInputEvent::Type::kGestureScrollBegin)
                        ? data.scroll_begin.delta_hint_units ==
                              ui::ScrollGranularity::kScrollByPercentage
                        : data.scroll_update.delta_units ==
                              ui::ScrollGranularity::kScrollByPercentage;

  return is_percent ? delta_x : delta_x / frame_scale_;
}

float WebGestureEvent::DeltaYInRootFrame() const {
  float delta_y = (type_ == WebInputEvent::Type::kGestureScrollBegin)
                      ? data.scroll_begin.delta_y_hint
                      : data.scroll_update.delta_y;

  bool is_percent = (type_ == WebInputEvent::Type::kGestureScrollBegin)
                        ? data.scroll_begin.delta_hint_units ==
                              ui::ScrollGranularity::kScrollByPercentage
                        : data.scroll_update.delta_units ==
                              ui::ScrollGranularity::kScrollByPercentage;

  return is_percent ? delta_y : delta_y / frame_scale_;
}

ui::ScrollGranularity WebGestureEvent::DeltaUnits() const {
  if (type_ == WebInputEvent::Type::kGestureScrollBegin)
    return data.scroll_begin.delta_hint_units;
  if (type_ == WebInputEvent::Type::kGestureScrollUpdate)
    return data.scroll_update.delta_units;
  DCHECK_EQ(type_, WebInputEvent::Type::kGestureScrollEnd);
  return data.scroll_end.delta_units;
}

WebGestureEvent::InertialPhaseState WebGestureEvent::InertialPhase() const {
  if (type_ == WebInputEvent::Type::kGestureScrollBegin)
    return data.scroll_begin.inertial_phase;
  if (type_ == WebInputEvent::Type::kGestureScrollUpdate)
    return data.scroll_update.inertial_phase;
  DCHECK_EQ(type_, WebInputEvent::Type::kGestureScrollEnd);
  return data.scroll_end.inertial_phase;
}

bool WebGestureEvent::Synthetic() const {
  if (type_ == WebInputEvent::Type::kGestureScrollBegin)
    return data.scroll_begin.synthetic;
  DCHECK_EQ(type_, WebInputEvent::Type::kGestureScrollEnd);
  return data.scroll_end.synthetic;
}

gfx::SizeF WebGestureEvent::TapAreaInRootFrame() const {
  if (type_ == WebInputEvent::Type::kGestureTwoFingerTap) {
    return gfx::SizeF(data.two_finger_tap.first_finger_width / frame_scale_,
                      data.two_finger_tap.first_finger_height / frame_scale_);
  } else if (type_ == WebInputEvent::Type::kGestureShortPress ||
             type_ == WebInputEvent::Type::kGestureLongPress ||
             type_ == WebInputEvent::Type::kGestureLongTap) {
    return gfx::SizeF(data.long_press.width / frame_scale_,
                      data.long_press.height / frame_scale_);
  } else if (type_ == WebInputEvent::Type::kGestureTap ||
             type_ == WebInputEvent::Type::kGestureTapUnconfirmed ||
             type_ == WebInputEvent::Type::kGestureDoubleTap) {
    return gfx::SizeF(data.tap.width / frame_scale_,
                      data.tap.height / frame_scale_);
  } else if (type_ == WebInputEvent::Type::kGestureTapDown) {
    return gfx::SizeF(data.tap_down.width / frame_scale_,
                      data.tap_down.height / frame_scale_);
  } else if (type_ == WebInputEvent::Type::kGestureShowPress) {
    return gfx::SizeF(data.show_press.width / frame_scale_,
                      data.show_press.height / frame_scale_);
  }
  // This function is called for all gestures and determined if the tap
  // area is empty or not; so return an empty rect here.
  return gfx::SizeF();
}

gfx::PointF WebGestureEvent::PositionInRootFrame() const {
  return gfx::ScalePoint(position_in_widget_, 1 / frame_scale_) +
         frame_translate_;
}

int WebGestureEvent::TapCount() const {
  DCHECK_EQ(type_, WebInputEvent::Type::kGestureTap);
  return data.tap.tap_count;
}

int WebGestureEvent::TapDownCount() const {
  DCHECK_EQ(type_, WebInputEvent::Type::kGestureTapDown);
  return data.tap_down.tap_down_count;
}

void WebGestureEvent::ApplyTouchAdjustment(
    const gfx::PointF& root_frame_coords) {
  // Update the window-relative position of the event so that the node that
  // was ultimately hit is under this point (i.e. elementFromPoint for the
  // client co-ordinates in a 'click' event should yield the target). The
  // global position is intentionally left unmodified because it's intended to
  // reflect raw co-ordinates unrelated to any content.
  frame_translate_ = root_frame_coords -
                     gfx::ScalePoint(position_in_widget_, 1 / frame_scale_);
}

void WebGestureEvent::FlattenTransform() {
  if (frame_scale_ != 1) {
    switch (type_) {
      case WebInputEvent::Type::kGestureScrollBegin:
        if (data.scroll_begin.delta_hint_units !=
            ui::ScrollGranularity::kScrollByPercentage) {
          data.scroll_begin.delta_x_hint /= frame_scale_;
          data.scroll_begin.delta_y_hint /= frame_scale_;
        }
        break;
      case WebInputEvent::Type::kGestureScrollUpdate:
        if (data.scroll_update.delta_units !=
            ui::ScrollGranularity::kScrollByPercentage) {
          data.scroll_update.delta_x /= frame_scale_;
          data.scroll_update.delta_y /= frame_scale_;
        }
        break;
      case WebInputEvent::Type::kGestureTwoFingerTap:
        data.two_finger_tap.first_finger_width /= frame_scale_;
        data.two_finger_tap.first_finger_height /= frame_scale_;
        break;
      case WebInputEvent::Type::kGestureShortPress:
      case WebInputEvent::Type::kGestureLongPress:
      case WebInputEvent::Type::kGestureLongTap:
        data.long_press.width /= frame_scale_;
        data.long_press.height /= frame_scale_;
        break;
      case WebInputEvent::Type::kGestureTap:
      case WebInputEvent::Type::kGestureTapUnconfirmed:
      case WebInputEvent::Type::kGestureDoubleTap:
        data.tap.width /= frame_scale_;
        data.tap.height /= frame_scale_;
        break;
      case WebInputEvent::Type::kGestureTapDown:
        data.tap_down.width /= frame_scale_;
        data.tap_down.height /= frame_scale_;
        break;
      case WebInputEvent::Type::kGestureShowPress:
        data.show_press.width /= frame_scale_;
        data.show_press.height /= frame_scale_;
        break;
      default:
        break;
    }
  }

  SetPositionInWidget(PositionInRootFrame());
  frame_translate_ = gfx::Vector2dF();
  frame_scale_ = 1;
}

// Whether |event_in_queue| is a touchscreen GesturePinchUpdate or
// GestureScrollUpdate and has the same modifiers/source as the new
// scroll/pinch event. Compatible touchscreen scroll and pinch event pairs
// can be logically coalesced.
bool WebGestureEvent::IsCompatibleScrollorPinch(
    const WebGestureEvent& new_event,
    const WebGestureEvent& event_in_queue) {
  DCHECK(new_event.GetType() == WebInputEvent::Type::kGestureScrollUpdate ||
         new_event.GetType() == WebInputEvent::Type::kGesturePinchUpdate)
      << "Invalid event type for pinch/scroll coalescing: "
      << WebInputEvent::GetName(new_event.GetType());
  DLOG_IF(WARNING, new_event.TimeStamp() < event_in_queue.TimeStamp())
      << "Event time not monotonic?\n";
  return (event_in_queue.GetType() ==
              WebInputEvent::Type::kGestureScrollUpdate ||
          event_in_queue.GetType() ==
              WebInputEvent::Type::kGesturePinchUpdate) &&
         event_in_queue.GetModifiers() == new_event.GetModifiers() &&
         event_in_queue.SourceDevice() == WebGestureDevice::kTouchscreen &&
         new_event.SourceDevice() == WebGestureDevice::kTouchscreen;
}

std::pair<std::unique_ptr<WebGestureEvent>, std::unique_ptr<WebGestureEvent>>
WebGestureEvent::CoalesceScrollAndPinch(
    const WebGestureEvent* second_last_event,
    const WebGestureEvent& last_event,
    const WebGestureEvent& new_event) {
  DCHECK(!last_event.CanCoalesce(new_event))
      << "New event can't be coalesced with the last event in queue directly.";
  DCHECK(IsContinuousGestureEvent(new_event.GetType()));
  DCHECK(IsCompatibleScrollorPinch(new_event, last_event));
  DCHECK(!second_last_event ||
         IsCompatibleScrollorPinch(new_event, *second_last_event));

  auto scroll_event = std::make_unique<WebGestureEvent>(
      WebInputEvent::Type::kGestureScrollUpdate, new_event.GetModifiers(),
      new_event.TimeStamp(), new_event.SourceDevice());
  scroll_event->primary_pointer_type = new_event.primary_pointer_type;
  scroll_event->primary_unique_touch_event_id =
      new_event.primary_unique_touch_event_id;
  auto pinch_event = std::make_unique<WebGestureEvent>(*scroll_event);
  pinch_event->SetType(WebInputEvent::Type::kGesturePinchUpdate);
  pinch_event->SetPositionInWidget(
      new_event.GetType() == WebInputEvent::Type::kGesturePinchUpdate
          ? new_event.PositionInWidget()
          : last_event.PositionInWidget());

  gfx::Transform combined_scroll_pinch = GetTransformForEvent(last_event);
  if (second_last_event) {
    combined_scroll_pinch.PreConcat(GetTransformForEvent(*second_last_event));
  }
  combined_scroll_pinch.PostConcat(GetTransformForEvent(new_event));

  float combined_scale = combined_scroll_pinch.To2dScale().x();
  gfx::Vector2dF combined_translation = combined_scroll_pinch.To2dTranslation();
  scroll_event->data.scroll_update.delta_x =
      (combined_translation.x() + pinch_event->PositionInWidget().x()) /
          combined_scale -
      pinch_event->PositionInWidget().x();
  scroll_event->data.scroll_update.delta_y =
      (combined_translation.y() + pinch_event->PositionInWidget().y()) /
          combined_scale -
      pinch_event->PositionInWidget().y();
  pinch_event->data.pinch_update.scale = combined_scale;

  return std::make_pair(std::move(scroll_event), std::move(pinch_event));
}

std::unique_ptr<blink::WebGestureEvent>
WebGestureEvent::GenerateInjectedScrollbarGestureScroll(
    WebInputEvent::Type type,
    base::TimeTicks timestamp,
    gfx::PointF position_in_widget,
    gfx::Vector2dF scroll_delta,
    ui::ScrollGranularity granularity) {
  std::unique_ptr<WebGestureEvent> generated_gesture_event =
      std::make_unique<WebGestureEvent>(type, WebInputEvent::kNoModifiers,
                                        timestamp,
                                        WebGestureDevice::kScrollbar);
  DCHECK(generated_gesture_event->IsGestureScroll());

  if (type == WebInputEvent::Type::kGestureScrollBegin) {
    // Gesture events expect the scroll delta to be flipped. Gesture events'
    // scroll deltas are interpreted as the finger's delta in relation to the
    // screen (which is the reverse of the scrolling direction).
    generated_gesture_event->data.scroll_begin.delta_x_hint = -scroll_delta.x();
    generated_gesture_event->data.scroll_begin.delta_y_hint = -scroll_delta.y();
    generated_gesture_event->data.scroll_begin.inertial_phase =
        WebGestureEvent::InertialPhaseState::kNonMomentum;
    generated_gesture_event->data.scroll_begin.delta_hint_units = granularity;
  } else if (type == WebInputEvent::Type::kGestureScrollUpdate) {
    generated_gesture_event->data.scroll_update.delta_x = -scroll_delta.x();
    generated_gesture_event->data.scroll_update.delta_y = -scroll_delta.y();
    generated_gesture_event->data.scroll_update.inertial_phase =
        WebGestureEvent::InertialPhaseState::kNonMomentum;
    generated_gesture_event->data.scroll_update.delta_units = granularity;
  }

  generated_gesture_event->SetPositionInWidget(position_in_widget);
  return generated_gesture_event;
}

}  // namespace blink
