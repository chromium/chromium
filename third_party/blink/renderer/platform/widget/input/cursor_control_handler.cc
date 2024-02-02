// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/cursor_control_handler.h"

#include "third_party/blink/public/common/input/web_gesture_event.h"

namespace blink {

std::optional<InputHandlerProxy::EventDisposition>
CursorControlHandler::ObserveInputEvent(const WebInputEvent& event) {
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin:
      return HandleGestureScrollBegin(
          static_cast<const WebGestureEvent&>(event));
    case WebInputEvent::Type::kGestureScrollUpdate:
      return HandleGestureScrollUpdate(
          static_cast<const WebGestureEvent&>(event));
    case WebInputEvent::Type::kGestureScrollEnd:
      return HandleGestureScrollEnd(static_cast<const WebGestureEvent&>(event));
    default:
      return std::nullopt;
  }
}

std::optional<InputHandlerProxy::EventDisposition>
CursorControlHandler::HandleGestureScrollBegin(const WebGestureEvent& event) {
  if (event.data.scroll_begin.cursor_control) {
    cursor_control_in_progress_ = true;
    return InputHandlerProxy::EventDisposition::DID_NOT_HANDLE;
  }
  return std::nullopt;
}

std::optional<InputHandlerProxy::EventDisposition>
CursorControlHandler::HandleGestureScrollUpdate(const WebGestureEvent& event) {
  if (cursor_control_in_progress_) {
    // Ignore if this event is for fling scroll.
    if (event.data.scroll_update.inertial_phase ==
        mojom::InertialPhaseState::kMomentum)
      return InputHandlerProxy::EventDisposition::DROP_EVENT;
    return InputHandlerProxy::EventDisposition::DID_NOT_HANDLE;
  }
  return std::nullopt;
}

std::optional<InputHandlerProxy::EventDisposition>
CursorControlHandler::HandleGestureScrollEnd(const WebGestureEvent& event) {
  if (cursor_control_in_progress_) {
    cursor_control_in_progress_ = false;
    return InputHandlerProxy::EventDisposition::DID_NOT_HANDLE;
  }
  return std::nullopt;
}

}  // namespace blink
