// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_

#include <cstdint>
#include <optional>

#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "third_party/blink/public/common/input/pointer_id.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_timeline_entry_id_generator.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"

namespace blink {

class LocalDOMWindow;
class PerformanceEventTiming;
class WindowPerformance;

// ResponsivenessMetrics is responsible for calculating and reporting
// User Interaction Latencies (part of INP).
// It manages the lifecycle of Interactions by grouping related events
// (e.g., pointerdown, pointerup, click) and assigning them a shared
// interactionId.
class CORE_EXPORT ResponsivenessMetrics
    : public GarbageCollected<ResponsivenessMetrics> {
 public:
  explicit ResponsivenessMetrics(WindowPerformance*);
  ~ResponsivenessMetrics();

  // Assigns an interactionId to the entry based on current interaction state.
  // This is called during the processing phase of an event. |entry| must not
  // be null. Note that for some entries, specifically pointerdown, the ID
  // cannot be assigned right away and will be assigned when a subsequent
  // pointerup, pointercancel, contextmenu, or click (etc) occurs.
  void TryAssignInteractionId(PerformanceEventTiming* entry);

  // Reports the entry to UKM and UMA metrics if it represents a valid
  // interaction. Called as soon as the entry has a known end time. |entry|
  // must not be null.
  void ReportToMetrics(PerformanceEventTiming* entry);

  // Lifecycle and Testing
  void FlushAllEvents();
  void StopUkmSamplingForTesting() { sampling_ = false; }
  uint32_t GetInteractionCount() const;

  void SetCurrentInteractionEventQueuedTimestamp(base::TimeTicks queued_time);
  base::TimeTicks CurrentInteractionEventQueuedTimestamp() const;

  void Trace(Visitor*) const;

 private:
  // Categorical handlers for different interaction types.
  void HandleKeyboardInteraction(PerformanceEventTiming* entry);
  void HandlePointerInteraction(PerformanceEventTiming* entry);
  void HandleCompositionInteraction(PerformanceEventTiming* entry);

  // ID Management
  // Assigns a specific interaction ID to the entry.
  void SetInteractionId(PerformanceEventTiming* entry,
                        PerformanceTimelineEntryIdInfo id);
  // Assigns a new interaction ID for a keyboard interaction and updates the
  // associated maps. Returns the new ID.
  PerformanceTimelineEntryIdInfo AssignNewKeyboardInteractionId(int key_code);
  // Assigns a new interaction ID for a pointer interaction and updates the
  // associated maps. Returns the new ID.
  PerformanceTimelineEntryIdInfo AssignNewPointerInteractionId(
      PointerId pointer_id);

  void CommitAllPendingPointerdowns();

  // Metrics Reporting
  void RecordUserInteractionUKM(LocalDOMWindow* window,
                                UserInteractionType interaction_type,
                                const PerformanceEventTiming& entry);

  void RecordUserInteractionHistograms(UserInteractionType interaction_type,
                                       const PerformanceEventTiming& entry);

  void RecordUserInteractionTracing(LocalDOMWindow* window,
                                    UserInteractionType interaction_type,
                                    const PerformanceEventTiming& entry);

  // This is used to store the set of unique histogram timings in a single
  // animation frame.  The first event for each interaction id should always
  // be the longest.  If they have the same end time, they perfectly overlap in
  // time and don't need to be repeated.
  // TODO(crbug.com/328902994): If it wasn't for "pending pointerdown"
  // reporting, we would know that ALL event timings in a single animation frame
  // always report together. In that case, we could change to pass a list of
  // event timings to |ReportMetrics()| instead of one by one, and we wouldn't
  // need to store this at all, instead just std::unique with a custom
  // comparator.
  struct ReportedInteractionKey {
    uint64_t interaction_id;
    base::TimeTicks end_time;

    bool operator==(const ReportedInteractionKey& other) const = default;
  };

  Member<WindowPerformance> window_performance_;

  // Keyboard and Composition State
  enum CompositionState {
    kNonComposition,
    kCompositionActive,
    kCompositionEndOnKeyup
  };
  CompositionState composition_state_ = kNonComposition;

  // Matches the `keyCode()` return type.
  using KeydownKeyType = int;
  // Map from keyCodes to the last keydown interaction ID.
  HashMap<KeydownKeyType,
          PerformanceTimelineEntryIdInfo,
          IntWithZeroKeyHashTraits<KeydownKeyType>>
      keycode_to_interactionid_;

  // During composition or for simulated clicks, we sometimes just match to most
  // recent keydown.
  std::optional<PerformanceTimelineEntryIdInfo> last_keydown_interaction_id_;

  // Ideally this type would be `PointerID` type, but that is signed value and
  // might take on -1 (for |kReservedNonPointerId|) or
  // std::numeric_limits<int>::max() (for |kMousePointerId|), so we cannot use
  // PointerID as the map key.  Using 64bit values to accommodate this, but it
  // may be possible to just carefully handle these special values.
  // Same solution as |PointerEventFactory::pointer_id_to_attributes_|.
  // Unfortunate.
  using PointerDownKeyType = int64_t;
  // Map from pointerId to the pending pointerdown event entry. Entries are
  // moved from here to the `pointerdown_ids_` map once the interaction ID is
  // assigned.
  HeapHashMap<PointerDownKeyType,
              Member<PerformanceEventTiming>,
              IntWithZeroKeyHashTraits<PointerDownKeyType>>
      pending_pointerdown_entries_;

  // Map from pointerId to the assigned interaction ID of the last pointerdown.
  // This is used to ensure subsequent events in the same interaction (e.g.
  // pointerup, click) get the same ID. Entries are removed once the
  // interaction is considered complete (on click or pointercancel).
  HashMap<PointerDownKeyType,
          PerformanceTimelineEntryIdInfo,
          IntWithZeroKeyHashTraits<PointerDownKeyType>>
      pointerid_to_interactionid_;

  base::TimeTicks current_interaction_event_queued_timestamp_;

  PerformanceTimelineEntryIdGenerator interaction_id_generator_;

  // Whether to perform UKM sampling.
  bool sampling_ = true;

  std::optional<uint64_t> last_recorded_frame_index_;
  Vector<ReportedInteractionKey> reported_interactions_in_frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_
