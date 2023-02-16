// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class PerformanceEventTiming;
class WindowPerformance;

class ResponsivenessMetrics : public GarbageCollected<ResponsivenessMetrics> {
 public:
  // Timestamps for input events.
  struct EventTimestamps {
    // The event creation time.
    base::TimeTicks start_time;
    // The time when the first display update caused by the input event was
    // performed.
    base::TimeTicks end_time;
  };

  // Wrapper class to store PerformanceEventTiming and timestamps
  // on a HeapHashMap.
  class KeyboardEntryAndTimestamps
      : public GarbageCollected<KeyboardEntryAndTimestamps> {
   public:
    KeyboardEntryAndTimestamps(PerformanceEventTiming* entry,
                               EventTimestamps timestamps)
        : entry_(entry), timestamps_({timestamps}) {}

    static KeyboardEntryAndTimestamps* Create(PerformanceEventTiming* entry,
                                              EventTimestamps timestamps) {
      return MakeGarbageCollected<KeyboardEntryAndTimestamps>(entry,
                                                              timestamps);
    }
    ~KeyboardEntryAndTimestamps() = default;
    void Trace(Visitor*) const;
    PerformanceEventTiming* GetEntry() const { return entry_; }
    EventTimestamps GetTimeStamps() { return timestamps_; }

   private:
    // The PerformanceEventTiming entry that has not been sent to observers
    // yet: the event dispatch has been completed but the presentation promise
    // used to determine |duration| has not yet been resolved, or the
    // interactionId has not yet been computed yet.
    Member<PerformanceEventTiming> entry_;
    // Timestamps associated with the entry.
    EventTimestamps timestamps_;
  };

  // Wrapper class to store PerformanceEventTiming, pointerdown and pointerup
  // timestamps, and whether drag has been detected on a HeapHashMap.
  class PointerEntryAndInfo : public GarbageCollected<PointerEntryAndInfo> {
   public:
    PointerEntryAndInfo(PerformanceEventTiming* entry,
                        EventTimestamps timestamps)
        : entry_(entry), timestamps_({timestamps}) {}

    static PointerEntryAndInfo* Create(PerformanceEventTiming* entry,
                                       EventTimestamps timestamps) {
      return MakeGarbageCollected<PointerEntryAndInfo>(entry, timestamps);
    }
    ~PointerEntryAndInfo() = default;
    void Trace(Visitor*) const;
    PerformanceEventTiming* GetEntry() const { return entry_; }
    Vector<EventTimestamps>& GetTimeStamps() { return timestamps_; }
    void SetIsDrag() { is_drag_ = true; }
    bool IsDrag() const { return is_drag_; }

   private:
    // The PerformanceEventTiming entry that has not been sent to observers
    // yet: the event dispatch has been completed but the presentation promise
    // used to determine |duration| has not yet been resolved, , or the
    // interactionId has not yet been computed yet.
    Member<PerformanceEventTiming> entry_;
    // Timestamps associated with the entry. The first should always be
    // for a pointerdown, the second for a pointerup, and optionally the third
    // for a click.
    Vector<EventTimestamps> timestamps_;
    // Whether drag has been detected.
    bool is_drag_;
  };

  explicit ResponsivenessMetrics(WindowPerformance*);
  ~ResponsivenessMetrics();

  // Stop UKM sampling for testing.
  void StopUkmSamplingForTesting() { sampling_ = false; }

  // The use might be dragging. The function will be called whenever we have a
  // pointermove.
  void NotifyPotentialDrag(PointerId pointer_id);

  // Assigns an interactionId and records interaction latency for pointer
  // events. Returns true if the entry is ready to be surfaced in
  // PerformanceObservers and the Performance Timeline.
  bool SetPointerIdAndRecordLatency(PerformanceEventTiming* entry,
                                    PointerId pointer_id,
                                    EventTimestamps event_timestamps);

  // Assigns interactionId and records interaction latency for keyboard events.
  // We care about input, compositionstart, and compositionend events, so
  // |key_code| will be absl::nullopt in those cases. Returns true if the entry
  // would be ready to be surfaced in PerformanceObservers and the Performance
  // Timeline.
  bool SetKeyIdAndRecordLatency(PerformanceEventTiming* entry,
                                absl::optional<int> key_code,
                                EventTimestamps event_timestamps);

  // Clears some entries in |key_codes_to_remove| if we have stored them for a
  // while.
  void MaybeFlushKeyboardEntries(DOMHighResTimeStamp current_time);

  uint64_t GetInteractionCount() const;

  void Trace(Visitor*) const;

 private:
  // Record UKM for user interaction latencies.
  void RecordUserInteractionUKM(
      LocalDOMWindow* window,
      UserInteractionType interaction_type,
      const WTF::Vector<ResponsivenessMetrics::EventTimestamps>& timestamps);

  void RecordDragTapOrClickUKM(LocalDOMWindow*, PointerEntryAndInfo&);

  void RecordKeyboardUKM(LocalDOMWindow* window,
                         const WTF::Vector<EventTimestamps>& event_timestamps);

  // Updates the interactionId counter which is used by Event Timing.
  void UpdateInteractionId();

  uint32_t GetCurrentInteractionId() const;

  // Method called when |pointer_flush_timer_| fires. Ensures that the last
  // interaction of any given pointerId is reported, even if it does not receive
  // a click.
  void FlushPointerTimerFired(TimerBase*);

  // Used to flush any entries in |pointer_id_entry_map_| which already have
  // pointerup. We either know there is no click happening or waited long enough
  // for a click to occur.
  void FlushPointerMap();
  void StopTimerAndFlush();

  void NotifyPointerdown(PerformanceEventTiming* entry) const;

  Member<WindowPerformance> window_performance_;

  // Map from keyCodes to keydown entries and keydown timestamps.
  HeapHashMap<int,
              Member<KeyboardEntryAndTimestamps>,
              IntWithZeroKeyHashTraits<int>>
      key_code_entry_map_;
  // Whether we are composing or not. When we are not composing, we set
  // interactionId for keydown and keyup events. When we are composing, we set
  // interactionId for input events.
  bool composition_started_ = false;

  // Map from pointerId to the first pointer event entry seen for the user
  // interaction, and other information.
  HeapHashMap<PointerId,
              Member<PointerEntryAndInfo>,
              IntWithZeroKeyHashTraits<PointerId>>
      pointer_id_entry_map_;
  HeapTaskRunnerTimer<ResponsivenessMetrics> pointer_flush_timer_;
  // The PointerId of the last pointerdown or pointerup event processed. Used to
  // know which interactionId to use for click events. If pointecancel or
  // keyboard events are seen, the value is reset. TODO(crbug.com/1264930):
  // remove this attribute once PointerId for clicks correctly points to the
  // same value as its corresponding pointerdown and pointerup.
  absl::optional<PointerId> last_pointer_id_;

  uint32_t current_interaction_id_for_event_timing_;
  uint64_t interaction_count_ = 0;

  // Whether to perform UKM sampling.
  bool sampling_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESPONSIVENESS_METRICS_H_
