// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/web_mouse_wheel_event_traits.h"

#include "third_party/blink/public/platform/web_input_event.h"

namespace content {

using blink::WebInputEvent;

// static
blink::WebMouseWheelEvent::EventAction WebMouseWheelEventTraits::GetEventAction(
    const blink::WebMouseWheelEvent& event) {
#if defined(USE_AURA)
  // Scroll events generated from the mouse wheel when the control key is held
  // don't trigger scrolling. Instead, they may cause zooming.
  if (event.delta_units !=
          ui::input_types::ScrollGranularity::kScrollByPrecisePixel &&
      (event.GetModifiers() & WebInputEvent::kControlKey)) {
    return blink::WebMouseWheelEvent::EventAction::kPageZoom;
  }

  if (event.delta_x == 0 && (event.GetModifiers() & WebInputEvent::kShiftKey))
    return blink::WebMouseWheelEvent::EventAction::kScrollHorizontal;
#endif
  if (event.rails_mode == WebInputEvent::kRailsModeHorizontal ||
      (event.delta_x != 0 && event.delta_y == 0)) {
    return blink::WebMouseWheelEvent::EventAction::kScrollHorizontal;
  }

  if (event.rails_mode == WebInputEvent::kRailsModeVertical ||
      (event.delta_x == 0 && event.delta_y != 0)) {
    return blink::WebMouseWheelEvent::EventAction::kScrollVertical;
  }

  return blink::WebMouseWheelEvent::EventAction::kScroll;
}

}  // namespace content
