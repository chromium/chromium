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
      return ui::EventType::kMousePressed;
    case WebInputEvent::Type::kMouseUp:
      return ui::EventType::kMouseReleased;
    case WebInputEvent::Type::kMouseMove:
      return modifiers_ & kButtonModifiers ? ui::EventType::kMouseDragged
                                           : ui::EventType::kMouseMoved;
    case WebInputEvent::Type::kMouseEnter:
      return ui::EventType::kMouseEntered;
    case WebInputEvent::Type::kMouseLeave:
      return ui::EventType::kMouseExited;
    case WebInputEvent::Type::kContextMenu:
      return ui::EventType::kUnknown;
    case WebInputEvent::Type::kMouseWheel:
      return ui::EventType::kMousewheel;
    case WebInputEvent::Type::kRawKeyDown:
      return ui::EventType::kKeyPressed;
    case WebInputEvent::Type::kKeyDown:
      return ui::EventType::kKeyPressed;
    case WebInputEvent::Type::kKeyUp:
      return ui::EventType::kKeyReleased;
    case WebInputEvent::Type::kChar:
      return ui::EventType::kKeyPressed;
    case WebInputEvent::Type::kGestureScrollBegin:
      return ui::EventType::kGestureScrollBegin;
    case WebInputEvent::Type::kGestureScrollEnd:
      return ui::EventType::kGestureScrollEnd;
    case WebInputEvent::Type::kGestureScrollUpdate:
      return ui::EventType::kGestureScrollUpdate;
    case WebInputEvent::Type::kGestureFlingStart:
      return ui::EventType::kScrollFlingStart;
    case WebInputEvent::Type::kGestureFlingCancel:
      return ui::EventType::kScrollFlingCancel;
    case WebInputEvent::Type::kGesturePinchBegin:
      return ui::EventType::kGesturePinchBegin;
    case WebInputEvent::Type::kGesturePinchEnd:
      return ui::EventType::kGesturePinchEnd;
    case WebInputEvent::Type::kGesturePinchUpdate:
      return ui::EventType::kGesturePinchUpdate;
    case WebInputEvent::Type::kGestureTapDown:
      return ui::EventType::kGestureTapDown;
    case WebInputEvent::Type::kGestureShowPress:
      return ui::EventType::kGestureShowPress;
    case WebInputEvent::Type::kGestureTap:
      return ui::EventType::kGestureTap;
    case WebInputEvent::Type::kGestureTapCancel:
      return ui::EventType::kGestureTapCancel;
    case WebInputEvent::Type::kGestureShortPress:
      return ui::EventType::kGestureShortPress;
    case WebInputEvent::Type::kGestureLongPress:
      return ui::EventType::kGestureLongPress;
    case WebInputEvent::Type::kGestureLongTap:
      return ui::EventType::kGestureLongTap;
    case WebInputEvent::Type::kGestureTwoFingerTap:
      return ui::EventType::kGestureTwoFingerTap;
    case WebInputEvent::Type::kGestureTapUnconfirmed:
      return ui::EventType::kGestureTapUnconfirmed;
    case WebInputEvent::Type::kGestureDoubleTap:
      return ui::EventType::kGestureDoubleTap;
    case WebInputEvent::Type::kTouchStart:
      return ui::EventType::kTouchPressed;
    case WebInputEvent::Type::kTouchMove:
      return ui::EventType::kTouchMoved;
    case WebInputEvent::Type::kTouchEnd:
      return ui::EventType::kTouchReleased;
    case WebInputEvent::Type::kTouchCancel:
      return ui::EventType::kTouchCancelled;
    case WebInputEvent::Type::kTouchScrollStarted:
    case WebInputEvent::Type::kPointerDown:
      return ui::EventType::kTouchPressed;
    case WebInputEvent::Type::kPointerUp:
      return ui::EventType::kTouchReleased;
    case WebInputEvent::Type::kPointerMove:
      return ui::EventType::kTouchMoved;
    case WebInputEvent::Type::kPointerCancel:
      return ui::EventType::kTouchCancelled;
    default:
      return ui::EventType::kUnknown;
  }
}

}  // namespace blink
