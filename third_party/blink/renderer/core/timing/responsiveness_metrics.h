// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

class ResponsivenessMetrics {
 public:
  // Timestamps for input events.
  struct EventTimestamps {
    // The event creation time.
    base::TimeTicks start_time;
    // The time when the first display update caused by the input event was
    // performed.
    base::TimeTicks end_time;
  };

  ResponsivenessMetrics();
  ~ResponsivenessMetrics();

  // Stop UKM sampling for testing.
  void StopUkmSamplingForTesting() { sampling_ = false; }

  // The use might be dragging. The function will be called whenever we have a
  // pointermove.
  void NotifyPotentialDrag();

  void RecordKeyboardInteractions(
      LocalDOMWindow* window,
      const WTF::Vector<EventTimestamps>& event_timestamps);

  // Might not be accurate for multi-fingers touch.
  void RecordTapOrClickOrDrag(LocalDOMWindow* window,
                              const AtomicString& event_type,
                              EventTimestamps event_timestamps);

 private:
  // Record UKM for user interaction latencies.
  void RecordUserInteractionUKM(
      LocalDOMWindow* window,
      UserInteractionType interaction_type,
      const WTF::Vector<ResponsivenessMetrics::EventTimestamps>& timestamps);

  // Flush the latency data for pending tap or drag.
  void FlushPendingInteraction(LocalDOMWindow* window);

  // Reset the latency data for pointer events.
  void ResetPendingPointers();

  absl::optional<EventTimestamps> pending_pointer_up_timestamps_;
  absl::optional<EventTimestamps> pending_pointer_down_timestamps_;
  bool is_drag_ = false;

  // Whether to perform UKM sampling.
  bool sampling_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_
