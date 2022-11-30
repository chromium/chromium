// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_MOMENTUM_SCROLL_JANK_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_MOMENTUM_SCROLL_JANK_TRACKER_H_

#include "base/time/time.h"

namespace blink {
class EventWithCallback;

// Class which is used during a scroll event to detect, accumulate, and log
// jank metrics for the momentum phase.
class MomentumScrollJankTracker {
 public:
  ~MomentumScrollJankTracker();
  void OnDispatchedInputEvent(EventWithCallback* event_with_callback,
                              const base::TimeTicks& now);

 private:
  // The number of expected momentum events which should be coalesced in a
  // single frame.
  // If we update momentum event generation to happen more than once per frame,
  // |kExpectedMomentumEventsPerFrame| should be updated or this data plumbed
  // in from a different source.
  static constexpr uint32_t kExpectedMomentumEventsPerFrame = 1;

  // The amount of time elapsed between coalescing an event and dispatching the
  // event for which we consider the coalescing to be "recent" for the purposes
  // of https://crbug.com/952930.
  static constexpr base::TimeDelta kRecentEventCutoff = base::Milliseconds(2);

  // |jank_count_| is the number of coalesced momentum input events above
  // kExptectedMomentumEventsPerFrame.
  uint32_t jank_count_ = 0;

  // The number of events processed during a gesture.
  uint32_t total_event_count_ = 0;

  // Used to avoid tracking jank in the first momentum event, as this may be
  // unreliable.
  bool seen_first_momentum_input_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_MOMENTUM_SCROLL_JANK_TRACKER_H_
