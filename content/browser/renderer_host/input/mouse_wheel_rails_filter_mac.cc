// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mouse_wheel_rails_filter_mac.h"

using blink::WebInputEvent;
using blink::WebMouseWheelEvent;

namespace content {

MouseWheelRailsFilterMac::MouseWheelRailsFilterMac() {
}

MouseWheelRailsFilterMac::~MouseWheelRailsFilterMac() {
}

WebInputEvent::RailsMode MouseWheelRailsFilterMac::UpdateRailsMode(
    const WebMouseWheelEvent& event) {
  if (event.phase == WebMouseWheelEvent::kPhaseNone &&
      event.momentum_phase == WebMouseWheelEvent::kPhaseNone) {
    // We should only set the rails mode for trackpad wheel events. The AppKit
    // documentation state that legacy mouse events (legacy mouse) do not have
    // |phase| and |momentum_phase|.
    // https://developer.apple.com/documentation/appkit/nsevent/1533550-phase.
    return WebInputEvent::kRailsModeFree;
  }

  // A somewhat-arbitrary decay constant for hysteresis.
  const float kDecayConstant = 0.8f;

  if (event.phase == WebMouseWheelEvent::kPhaseBegan) {
    decayed_delta_ = gfx::Vector2dF();
  }
  if (event.delta_x == 0 && event.delta_y == 0)
    return WebInputEvent::kRailsModeFree;

  decayed_delta_.Scale(kDecayConstant);
  decayed_delta_ +=
      gfx::Vector2dF(std::abs(event.delta_x), std::abs(event.delta_y));

  if (decayed_delta_.y() >= decayed_delta_.x())
    return WebInputEvent::kRailsModeVertical;
  return WebInputEvent::kRailsModeHorizontal;
}

}  // namespace content
