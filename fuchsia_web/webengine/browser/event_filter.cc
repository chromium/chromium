// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/event_filter.h"

#include <limits>

#include "base/notreached.h"
#include "ui/events/event.h"

namespace {

using fuchsia::web::InputTypes;

const uint64_t kInputTypeNone = 0;
const uint64_t kInputTypeAll = std::numeric_limits<uint64_t>::max();

static_assert(
    std::is_same<uint64_t,
                 std::underlying_type<fuchsia::web::InputTypes>::type>::value,
    "InputTypes is not an uint64.");

}  // namespace

EventFilter::EventFilter() {
  // Allow all inputs by default.
  ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                      fuchsia::web::AllowInputState::ALLOW);
}

EventFilter::~EventFilter() = default;

void EventFilter::ConfigureInputTypes(fuchsia::web::InputTypes types,
                                      fuchsia::web::AllowInputState allow) {
  // If |types| contains ALL, all other type bits are superseded.
  if (allow == fuchsia::web::AllowInputState::ALLOW) {
    if (static_cast<uint64_t>(types) &
        static_cast<uint64_t>(fuchsia::web::InputTypes::ALL)) {
      enabled_input_types_ = kInputTypeAll;
      enable_unknown_types_ = true;
    } else {
      enabled_input_types_ |= static_cast<uint64_t>(types);
    }
  } else {
    if (static_cast<uint64_t>(types) &
        static_cast<uint64_t>(fuchsia::web::InputTypes::ALL)) {
      enabled_input_types_ = kInputTypeNone;
      enable_unknown_types_ = false;
    } else {
      enabled_input_types_ &= static_cast<uint64_t>(~types);
    }
  }
}

void EventFilter::OnEvent(ui::Event* event) {
  if (!IsEventAllowed(event->type())) {
    event->StopPropagation();
    return;
  }

  // Allow base class to route |event| to event type-specific handlers.
  ui::EventHandler::OnEvent(event);
}

void EventFilter::OnGestureEvent(ui::GestureEvent* event) {
  if (!IsEventAllowed(event->type())) {
    event->StopPropagation();
    return;
  }

  ui::EventHandler::OnGestureEvent(event);
}

bool EventFilter::IsEventAllowed(ui::EventType type) {
  switch (type) {
    case ui::EventType::kKeyPressed:
    case ui::EventType::kKeyReleased:
      return IsTypeEnabled(InputTypes::KEY);

    case ui::EventType::kMousePressed:
    case ui::EventType::kMouseDragged:
    case ui::EventType::kMouseReleased:
      return IsTypeEnabled(InputTypes::MOUSE_CLICK);

    case ui::EventType::kMouseMoved:
    case ui::EventType::kMouseEntered:
    case ui::EventType::kMouseExited:
      return IsTypeEnabled(InputTypes::MOUSE_MOVE);

    case ui::EventType::kMousewheel:
      return IsTypeEnabled(InputTypes::MOUSE_WHEEL);

    case ui::EventType::kGestureTap:
    case ui::EventType::kGestureTapDown:
    case ui::EventType::kGestureTapCancel:
    case ui::EventType::kGestureTapUnconfirmed:
    case ui::EventType::kGestureDoubleTap:
    case ui::EventType::kGestureTwoFingerTap:
    case ui::EventType::kGestureShortPress:
    case ui::EventType::kGestureLongPress:
    case ui::EventType::kGestureLongTap:
      return IsTypeEnabled(InputTypes::GESTURE_TAP);

    case ui::EventType::kGesturePinchBegin:
    case ui::EventType::kGesturePinchEnd:
    case ui::EventType::kGesturePinchUpdate:
      return IsTypeEnabled(InputTypes::GESTURE_PINCH);

    case ui::EventType::kGestureScrollBegin:
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kGestureScrollUpdate:
    case ui::EventType::kGestureSwipe:
    case ui::EventType::kScroll:
    case ui::EventType::kScrollFlingStart:
    case ui::EventType::kScrollFlingCancel:
      return IsTypeEnabled(InputTypes::GESTURE_DRAG);

    // Allow low-level touch events and non-input control messages to pass
    // through unimpeded.
    case ui::EventType::kTouchReleased:
    case ui::EventType::kTouchPressed:
    case ui::EventType::kTouchMoved:
    case ui::EventType::kTouchCancelled:
    case ui::EventType::kDropTargetEvent:
    case ui::EventType::kGestureShowPress:
    case ui::EventType::kGestureBegin:
    case ui::EventType::kGestureEnd:
    case ui::EventType::kCancelMode:
    case ui::EventType::kMouseCaptureChanged:
      return true;

    case ui::EventType::kUmaData:
      NOTREACHED();  // ChromeOS only.

    case ui::EventType::kLast:
      NOTREACHED();

    case ui::EventType::kUnknown:
      break;
  }

  return enable_unknown_types_;
}

bool EventFilter::IsTypeEnabled(InputTypes type) const {
  return (enabled_input_types_ & static_cast<uint64_t>(type));
}
