// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_INTERACTION_EFFECTS_MONITOR_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_INTERACTION_EFFECTS_MONITOR_H_

#include <cstdint>

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"

namespace blink {
class InteractionEffectsMonitor;
class LocalFrame;
class WebInteractionEffectsMonitorObserver;
class WebLocalFrame;

// This class monitors the effects of ongoing user interactions, e.g. contentful
// paints, and reports them to a `WebInteractionEffectsMonitorObserver`. This is
// intended to be used in conjunction with other heuristics to help determine
// when a page becomes stable after user interaction. Observation occurs as long
// as the instance of this class is alive, and only for interactions that occur
// after it has been created.
//
// Note: Not all types of interactions are supported (only click and keyboard
// events are currently supported), and only main frames are currently
// supported.
class BLINK_EXPORT WebInteractionEffectsMonitor {
 public:
  WebInteractionEffectsMonitor(const WebLocalFrame&,
                               WebInteractionEffectsMonitorObserver*);
  WebInteractionEffectsMonitor(WebInteractionEffectsMonitor&&) = delete;
  WebInteractionEffectsMonitor& operator=(WebInteractionEffectsMonitor&&) =
      delete;
  ~WebInteractionEffectsMonitor();

  // Returns the number of interactions the monitor has observed.
  uint32_t InteractionCount() const;

  // Returns the total area in DIPs of contentful paints attributed to
  // interactions the monitor is observing.
  uint64_t TotalPaintedArea() const;

#if INSIDE_BLINK
  WebInteractionEffectsMonitor(LocalFrame*,
                               WebInteractionEffectsMonitorObserver*);
#endif

 private:
  WebPrivatePtrForGC<InteractionEffectsMonitor> interaction_effects_monitor_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_INTERACTION_EFFECTS_MONITOR_H_
