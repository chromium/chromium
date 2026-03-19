// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mouse_wheel_rails_filter_mac.h"

using blink::WebInputEvent;
using blink::WebMouseWheelEvent;

namespace content {

// Minimum ratio between the dominant and non-dominant axis required to engage
// railing. Below this ratio, the scroll is considered diagonal and rails are
// not applied.
constexpr float kMinRailRatio = 2.0f;

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
    return WebInputEvent::RailsMode::kRailsModeFree;
  }

  // A somewhat-arbitrary decay constant for hysteresis.
  const float kDecayConstant = 0.8f;

  if (event.phase == WebMouseWheelEvent::kPhaseBegan) {
    decayed_delta_ = gfx::Vector2dF();
  }
  if (event.delta_x == 0 && event.delta_y == 0)
    return WebInputEvent::RailsMode::kRailsModeFree;

  decayed_delta_.Scale(kDecayConstant);
  decayed_delta_ +=
      gfx::Vector2dF(std::abs(event.delta_x), std::abs(event.delta_y));

  float max_delta = std::max(decayed_delta_.x(), decayed_delta_.y());
  float min_delta = std::min(decayed_delta_.x(), decayed_delta_.y());

  // Allow free diagonal scrolling when neither axis clearly dominates. The
  // ratio is re-evaluated on every event, so that railing naturally re-engages
  // when the user transitions between scroll directions.
  if (min_delta * kMinRailRatio > max_delta) {
    return WebInputEvent::RailsMode::kRailsModeFree;
  }

  if (decayed_delta_.y() >= decayed_delta_.x())
    return WebInputEvent::RailsMode::kRailsModeVertical;
  return WebInputEvent::RailsMode::kRailsModeHorizontal;
}

}  // namespace content
