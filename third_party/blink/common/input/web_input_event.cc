// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_input_event.h"

namespace blink {
namespace {
constexpr int kButtonModifiers =
    WebInputEvent::kLeftButtonDown | WebInputEvent::kMiddleButtonDown |
    WebInputEvent::kRightButtonDown | WebInputEvent::kBackButtonDown |
    WebInputEvent::kForwardButtonDown;
}

WebInputEvent::DispatchType WebInputEvent::MergeDispatchTypes(
    DispatchType type_1,
    DispatchType type_2) {
  static_assert(DispatchType::kBlocking < DispatchType::kEventNonBlocking,
                "Enum not ordered correctly");
  static_assert(DispatchType::kEventNonBlocking <
                    DispatchType::kListenersNonBlockingPassive,
                "Enum not ordered correctly");
  static_assert(DispatchType::kListenersNonBlockingPassive <
                    DispatchType::kListenersForcedNonBlockingDueToFling,
                "Enum not ordered correctly");
  return std::min(type_1, type_2);
}

ui::EventType WebInputEvent::GetTypeAsUiEventType() const {
  switch (type_) {
    case WebInputEvent::Type::kMouseDown:
      return ui::EventType::ET_MOUSE_PRESSED;
    case WebInputEvent::Type::kMouseUp:
      return ui::EventType::ET_MOUSE_RELEASED;
    case WebInputEvent::Type::kMouseMove:
      return modifiers_ & kButtonModifiers ? ui::EventType::ET_MOUSE_DRAGGED
                                           : ui::EventType::ET_MOUSE_MOVED;
    case WebInputEvent::Type::kMouseEnter:
      return ui::EventType::ET_MOUSE_ENTERED;
    case WebInputEvent::Type::kMouseLeave:
      return ui::EventType::ET_MOUSE_EXITED;
    case WebInputEvent::Type::kContextMenu:
      return ui::EventType::ET_UNKNOWN;
    case WebInputEvent::Type::kMouseWheel:
      return ui::EventType::ET_MOUSEWHEEL;
    case WebInputEvent::Type::kRawKeyDown:
      return ui::EventType::ET_KEY_PRESSED;
    case WebInputEvent::Type::kKeyDown:
      return ui::EventType::ET_KEY_PRESSED;
    case WebInputEvent::Type::kKeyUp:
      return ui::EventType::ET_KEY_RELEASED;
    case WebInputEvent::Type::kChar:
      return ui::EventType::ET_KEY_PRESSED;
    case WebInputEvent::Type::kGestureScrollBegin:
      return ui::EventType::ET_GESTURE_SCROLL_BEGIN;
    case WebInputEvent::Type::kGestureScrollEnd:
      return ui::EventType::ET_GESTURE_SCROLL_END;
    case WebInputEvent::Type::kGestureScrollUpdate:
      return ui::EventType::ET_GESTURE_SCROLL_UPDATE;
    case WebInputEvent::Type::kGestureFlingStart:
      return ui::EventType::ET_SCROLL_FLING_START;
    case WebInputEvent::Type::kGestureFlingCancel:
      return ui::EventType::ET_SCROLL_FLING_CANCEL;
    case WebInputEvent::Type::kGesturePinchBegin:
      return ui::EventType::ET_GESTURE_PINCH_BEGIN;
    case WebInputEvent::Type::kGesturePinchEnd:
      return ui::EventType::ET_GESTURE_PINCH_END;
    case WebInputEvent::Type::kGesturePinchUpdate:
      return ui::EventType::ET_GESTURE_PINCH_UPDATE;
    case WebInputEvent::Type::kGestureTapDown:
      return ui::EventType::ET_GESTURE_TAP_DOWN;
    case WebInputEvent::Type::kGestureShowPress:
      return ui::EventType::ET_GESTURE_SHOW_PRESS;
    case WebInputEvent::Type::kGestureTap:
      return ui::EventType::ET_GESTURE_TAP;
    case WebInputEvent::Type::kGestureTapCancel:
      return ui::EventType::ET_GESTURE_TAP_CANCEL;
    case WebInputEvent::Type::kGestureShortPress:
      return ui::EventType::ET_GESTURE_SHORT_PRESS;
    case WebInputEvent::Type::kGestureLongPress:
      return ui::EventType::ET_GESTURE_LONG_PRESS;
    case WebInputEvent::Type::kGestureLongTap:
      return ui::EventType::ET_GESTURE_LONG_TAP;
    case WebInputEvent::Type::kGestureTwoFingerTap:
      return ui::EventType::ET_GESTURE_TWO_FINGER_TAP;
    case WebInputEvent::Type::kGestureTapUnconfirmed:
      return ui::EventType::ET_GESTURE_TAP_UNCONFIRMED;
    case WebInputEvent::Type::kGestureDoubleTap:
      return ui::EventType::ET_GESTURE_DOUBLE_TAP;
    case WebInputEvent::Type::kTouchStart:
      return ui::EventType::ET_TOUCH_PRESSED;
    case WebInputEvent::Type::kTouchMove:
      return ui::EventType::ET_TOUCH_MOVED;
    case WebInputEvent::Type::kTouchEnd:
      return ui::EventType::ET_TOUCH_RELEASED;
    case WebInputEvent::Type::kTouchCancel:
      return ui::EventType::ET_TOUCH_CANCELLED;
    case WebInputEvent::Type::kTouchScrollStarted:
    case WebInputEvent::Type::kPointerDown:
      return ui::EventType::ET_TOUCH_PRESSED;
    case WebInputEvent::Type::kPointerUp:
      return ui::EventType::ET_TOUCH_RELEASED;
    case WebInputEvent::Type::kPointerMove:
      return ui::EventType::ET_TOUCH_MOVED;
    case WebInputEvent::Type::kPointerCancel:
      return ui::EventType::ET_TOUCH_CANCELLED;
    default:
      return ui::EventType::ET_UNKNOWN;
  }
}

}  // namespace blink
