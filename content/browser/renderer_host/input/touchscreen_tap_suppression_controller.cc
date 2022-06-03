// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"

#include <utility>

#include "content/browser/renderer_host/input/gesture_event_queue.h"

using blink::WebInputEvent;

namespace content {

TouchscreenTapSuppressionController::TouchscreenTapSuppressionController(
    const TapSuppressionController::Config& config)
    : TapSuppressionController(config) {}

TouchscreenTapSuppressionController::~TouchscreenTapSuppressionController() {}

bool TouchscreenTapSuppressionController::FilterTapEvent(
    const GestureEventWithLatencyInfo& event) {
  switch (event.event.GetType()) {
    case WebInputEvent::Type::kGestureTapDown:
      return ShouldSuppressTapDown();

    case WebInputEvent::Type::kGestureShowPress:
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureTapUnconfirmed:
    case WebInputEvent::Type::kGestureTapCancel:
    case WebInputEvent::Type::kGestureTap:
    case WebInputEvent::Type::kGestureDoubleTap:
    case WebInputEvent::Type::kGestureLongTap:
    case WebInputEvent::Type::kGestureTwoFingerTap:
      return ShouldSuppressTapEnd();

    default:
      break;
  }
  return false;
}

}  // namespace content
