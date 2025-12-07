// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_INTERACTION_EFFECTS_MONITOR_OBSERVER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_INTERACTION_EFFECTS_MONITOR_OBSERVER_H_

#include <cstdint>

namespace blink {

// `WebInteractionEffectsMonitorObserver` is an interface for observing the
// effects of user interactions, e.g. paints that were caused by an interaction.
// See `InteractionEffectsMonitor` for usage inside of blink and
// `WebInteractionEffectsMonitor` for usage outside of blink.
class WebInteractionEffectsMonitorObserver {
 public:
  virtual ~WebInteractionEffectsMonitorObserver() = default;

  // Called when an interaction-attributed contentful paint occurs, which are
  // image or text paints of DOM nodes updated by an interaction.
  // `new_painted_area` is the toal area in DIPs of the painted elements.
  virtual void OnContentfulPaint(uint64_t new_painted_area) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_INTERACTION_EFFECTS_MONITOR_OBSERVER_H_
