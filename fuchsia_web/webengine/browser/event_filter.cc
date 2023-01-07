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
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
      return IsTypeEnabled(InputTypes::KEY);

    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_RELEASED:
      return IsTypeEnabled(InputTypes::MOUSE_CLICK);

    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      return IsTypeEnabled(InputTypes::MOUSE_MOVE);

    case ui::ET_MOUSEWHEEL:
      return IsTypeEnabled(InputTypes::MOUSE_WHEEL);

    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_DOWN:
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_TAP_UNCONFIRMED:
    case ui::ET_GESTURE_DOUBLE_TAP:
    case ui::ET_GESTURE_TWO_FINGER_TAP:
    case ui::ET_GESTURE_SHORT_PRESS:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
      return IsTypeEnabled(InputTypes::GESTURE_TAP);

    case ui::ET_GESTURE_PINCH_BEGIN:
    case ui::ET_GESTURE_PINCH_END:
    case ui::ET_GESTURE_PINCH_UPDATE:
      return IsTypeEnabled(InputTypes::GESTURE_PINCH);

    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_GESTURE_SCROLL_UPDATE:
    case ui::ET_GESTURE_SWIPE:
    case ui::ET_SCROLL:
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_SCROLL_FLING_CANCEL:
      return IsTypeEnabled(InputTypes::GESTURE_DRAG);

    // Allow low-level touch events and non-input control messages to pass
    // through unimpeded.
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_MOVED:
    case ui::ET_TOUCH_CANCELLED:
    case ui::ET_DROP_TARGET_EVENT:
    case ui::ET_GESTURE_SHOW_PRESS:
    case ui::ET_GESTURE_BEGIN:
    case ui::ET_GESTURE_END:
    case ui::ET_CANCEL_MODE:
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      return true;

    case ui::ET_UMA_DATA:
      NOTREACHED();  // ChromeOS only.
      break;

    case ui::ET_LAST:
      NOTREACHED();
      [[fallthrough]];

    case ui::ET_UNKNOWN:
      break;
  }

  return enable_unknown_types_;
}

bool EventFilter::IsTypeEnabled(InputTypes type) const {
  return (enabled_input_types_ & static_cast<uint64_t>(type));
}
