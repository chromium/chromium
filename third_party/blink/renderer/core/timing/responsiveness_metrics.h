// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_

#include <unordered_map>

#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

// Enum class for user interaction types. It's used in UKM and should not be
// changed.
enum class UserInteractionType { kKeyboard = 0, kTapOrClick = 1, kDrag = 2 };

class ResponsivenessMetrics {
 public:
  ResponsivenessMetrics();
  ~ResponsivenessMetrics();

  // Timestamps for input events.
  struct EventTimestamps {
    // The event creation time.
    base::TimeTicks start_time;
    // The time when the first display update caused by the input event was
    // performed.
    base::TimeTicks end_time;
  };

  // Track ongoing user interactions and calculate the latency when an
  // interaction is completed. The latency data for each interaction will be
  // recored in UKM.
  void RecordPerInteractionLatency(LocalDOMWindow* window,
                                   const AtomicString& event_type,
                                   absl::optional<int> key_code,
                                   absl::optional<PointerId> pointer_id,
                                   EventTimestamps event_timestamps);

  // Stop UKM sampling for testing.
  void StopUkmSamplingForTesting() { sampling_ = false; }

  // The use might be dragging. The function will be called whenever we have a
  // pointermove.
  void NotifyPotentialDrag();

 private:
  // Record UKM for user interaction latencies.
  void RecordUserInteractionUKM(LocalDOMWindow* window,
                                UserInteractionType interaction_type,
                                base::TimeDelta max_event_duration,
                                base::TimeDelta total_devent_duration);

  void RecordKeyboardInteractions(LocalDOMWindow* window,
                                  const AtomicString& event_type,
                                  int key_code,
                                  EventTimestamps event_timestamps);

  // Might not be accurate for multi-fingers touch.
  void RecordTapOrClickOrDrag(LocalDOMWindow* window,
                              const AtomicString& event_type,
                              EventTimestamps event_timestamps);
  // Flush the latency data for pending tap or drag.
  void FlushPendingInteraction(LocalDOMWindow* window);

  // Reset the latency data for pointer events.
  void ResetPendingPointers();

  // Variables for per-interaction latencies.
  std::unordered_map<int, EventTimestamps> key_down_timestamps_map_;

  absl::optional<EventTimestamps> pending_pointer_up_timestamps_;
  absl::optional<EventTimestamps> pending_pointer_down_timestamps_;
  bool is_drag_ = false;

  // Whether to perform UKM sampling.
  bool sampling_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_
