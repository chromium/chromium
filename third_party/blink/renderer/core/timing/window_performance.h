/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_WINDOW_PERFORMANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_WINDOW_PERFORMANCE_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/core/timing/event_counts.h"
#include "third_party/blink/renderer/core/timing/memory_info.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_navigation.h"
#include "third_party/blink/renderer/core/timing/performance_timing.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/core/timing/responsiveness_metrics.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class CORE_EXPORT WindowPerformance final : public Performance,
                                            public PerformanceMonitor::Client,
                                            public ExecutionContextClient,
                                            public PageVisibilityObserver {
  friend class WindowPerformanceTest;
  friend class ResponsivenessMetrics;

  class EventData : public GarbageCollected<EventData> {
   public:
    EventData(PerformanceEventTiming* event_timing,
              uint64_t frame,
              base::TimeTicks event_timestamp,
              absl::optional<int> key_code,
              absl::optional<PointerId> pointer_id)
        : event_timing_(event_timing),
          frame_(frame),
          event_timestamp_(event_timestamp),
          key_code_(key_code),
          pointer_id_(pointer_id) {}

    static EventData* Create(PerformanceEventTiming* event_timing,
                             uint64_t frame,
                             base::TimeTicks event_timestamp,
                             absl::optional<int> key_code,
                             absl::optional<PointerId> pointer_id) {
      return MakeGarbageCollected<EventData>(
          event_timing, frame, event_timestamp, key_code, pointer_id);
    }
    ~EventData() = default;
    void Trace(Visitor*) const;
    PerformanceEventTiming* GetEventTiming() const { return event_timing_; }
    uint64_t GetFrameIndex() const { return frame_; }
    base::TimeTicks GetEventTimestamp() const { return event_timestamp_; }
    absl::optional<int> GetKeyCode() const { return key_code_; }
    absl::optional<PointerId> GetPointerId() const { return pointer_id_; }

   private:
    // Event PerformanceEventTiming entry that has not been sent to observers
    // yet: the event dispatch has been completed but the presentation promise
    // used to determine |duration| has not yet been resolved.
    Member<PerformanceEventTiming> event_timing_;
    // Frame index in which the entry in |event_timing_| were added.
    uint64_t frame_;
    // The event creation timestamp.
    base::TimeTicks event_timestamp_;
    // Keycode for the event. If the event is not a keyboard event, the keycode
    // wouldn't be set.
    absl::optional<int> key_code_;
    // PointerId for the event. If the event is not a pointer event, the
    // PointerId wouldn't be set.
    absl::optional<PointerId> pointer_id_;
  };

 public:
  explicit WindowPerformance(LocalDOMWindow*);
  ~WindowPerformance() override;

  ExecutionContext* GetExecutionContext() const override;

  PerformanceTiming* timing() const override;
  PerformanceTimingForReporting* timingForReporting() const;
  PerformanceNavigation* navigation() const override;

  MemoryInfo* memory(ScriptState*) const override;

  EventCounts* eventCounts() override;

  bool FirstInputDetected() const { return !!first_input_timing_; }

  // This method creates a PerformanceEventTiming and if needed creates a
  // presentation promise to calculate the |duration| attribute when such
  // promise is resolved.
  void RegisterEventTiming(const Event& event,
                           base::TimeTicks start_time,
                           base::TimeTicks processing_start,
                           base::TimeTicks processing_end);

  void OnPaintFinished();

  void AddElementTiming(const AtomicString& name,
                        const String& url,
                        const gfx::RectF& rect,
                        base::TimeTicks start_time,
                        base::TimeTicks load_time,
                        const AtomicString& identifier,
                        const gfx::Size& intrinsic_size,
                        const AtomicString& id,
                        Element*);

  void AddLayoutShiftEntry(LayoutShift*);
  void AddVisibilityStateEntry(bool is_visible, base::TimeTicks start_time);
  void AddSoftNavigationEntry(const AtomicString& name,
                              base::TimeTicks start_time);

  // PageVisibilityObserver
  void PageVisibilityChanged() override;

  void OnLargestContentfulPaintUpdated(
      base::TimeTicks start_time,
      base::TimeTicks render_time,
      uint64_t paint_size,
      base::TimeTicks load_time,
      base::TimeTicks first_animated_frame_time,
      const AtomicString& id,
      const String& url,
      Element*);

  void Trace(Visitor*) const override;

  ResponsivenessMetrics& GetResponsivenessMetrics() {
    return *responsiveness_metrics_;
  }

  void NotifyPotentialDrag(PointerId pointer_id);

  void SetCurrentEventTimingEvent(const Event* event) {
    current_event_ = event;
  }
  const Event* GetCurrentEventTimingEvent() { return current_event_; }

 private:
  PerformanceNavigationTiming* CreateNavigationTimingInstance() override;

  static std::pair<AtomicString, DOMWindow*> SanitizedAttribution(
      ExecutionContext*,
      bool has_multiple_contexts,
      LocalFrame* observer_frame);

  // PerformanceMonitor::Client implementation.
  void ReportLongTask(base::TimeTicks start_time,
                      base::TimeTicks end_time,
                      ExecutionContext* task_context,
                      bool has_multiple_contexts) override;

  void BuildJSONValue(V8ObjectBuilder&) const override;

  // Method called once presentation promise for a frame is resolved. It will
  // add all event timings that have not been added since the last presentation
  // promise.
  void ReportEventTimings(uint64_t frame_index,
                          base::TimeTicks presentation_timestamp);

  void DispatchFirstInputTiming(PerformanceEventTiming* entry);

  // Assign an interaction id to an event timing entry if needed. Also records
  // the interaction latency. Returns true if the entry is ready to be surfaced
  // in PerformanceObservers and the Performance Timeline
  bool SetInteractionIdAndRecordLatency(
      PerformanceEventTiming* entry,
      absl::optional<int> key_code,
      absl::optional<PointerId> pointer_id,
      ResponsivenessMetrics::EventTimestamps event_timestamps);

  // Notify observer that an event timing entry is ready and add it to the event
  // timing buffer if needed.
  void NotifyAndAddEventTimingBuffer(PerformanceEventTiming* entry);

  // The last time the page visibility was changed.
  base::TimeTicks last_visibility_change_timestamp_;

  // Counter of the current frame index, based on calls to OnPaintFinished().
  uint64_t frame_index_ = 1;
  // Monotonically increasing value with the last frame index on which a
  // presentation promise was queued;
  uint64_t last_registered_frame_index_ = 0;
  // Number of pending presentation promises.
  uint16_t pending_presentation_promise_count_ = 0;
  // Store all event timing and latency related data, including
  // PerformanceEventTiming, frame_index, keycode and pointerId. We use the data
  // to calculate events latencies.
  HeapDeque<Member<EventData>> events_data_;
  Member<PerformanceEventTiming> first_pointer_down_event_timing_;
  Member<EventCounts> event_counts_;
  mutable Member<PerformanceNavigation> navigation_;
  mutable Member<PerformanceTiming> timing_;
  mutable Member<PerformanceTimingForReporting> timing_for_reporting_;
  absl::optional<base::TimeDelta> pending_pointer_down_input_delay_;
  absl::optional<base::TimeDelta> pending_pointer_down_processing_time_;
  absl::optional<base::TimeDelta> pending_pointer_down_time_to_next_paint_;

  // Calculate responsiveness metrics and record UKM for them.
  Member<ResponsivenessMetrics> responsiveness_metrics_;
  // The event we are currently processing.
  WeakMember<const Event> current_event_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_WINDOW_PERFORMANCE_H_
