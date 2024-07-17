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

#include <optional>

#include "base/time/time.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/core/timing/event_counts.h"
#include "third_party/blink/renderer/core/timing/memory_info.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_navigation.h"
#include "third_party/blink/renderer/core/timing/performance_timing.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/core/timing/responsiveness_metrics.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace viz {
struct FrameTimingDetails;
}

namespace blink {

class AnimationFrameTimingInfo;

class CORE_EXPORT WindowPerformance final : public Performance,
                                            public PerformanceMonitor::Client,
                                            public ExecutionContextClient,
                                            public PageVisibilityObserver {
  friend class WindowPerformanceTest;
  friend class ResponsivenessMetrics;

  class EventData : public GarbageCollected<EventData> {
   public:
    EventData(PerformanceEventTiming* event_timing,
              uint64_t presentation_index,
              base::TimeTicks event_timestamp,
              base::TimeTicks processing_start,
              base::TimeTicks processing_end,
              std::optional<int> key_code,
              std::optional<PointerId> pointer_id)
        : event_timing_(event_timing),
          presentation_index_(presentation_index),
          event_timestamp_(event_timestamp),
          processing_start_(processing_start),
          processing_end_(processing_end),
          key_code_(key_code),
          pointer_id_(pointer_id) {}

    static EventData* Create(PerformanceEventTiming* event_timing,
                             uint64_t presentation_index,
                             base::TimeTicks event_timestamp,
                             base::TimeTicks processing_start,
                             base::TimeTicks processing_end,
                             std::optional<int> key_code,
                             std::optional<PointerId> pointer_id) {
      return MakeGarbageCollected<EventData>(
          event_timing, presentation_index, event_timestamp, processing_start,
          processing_end, key_code, pointer_id);
    }
    ~EventData() = default;
    void Trace(Visitor*) const;
    PerformanceEventTiming* GetEventTiming() const {
      return event_timing_.Get();
    }
    uint64_t GetPresentationIndex() const { return presentation_index_; }
    base::TimeTicks GetEventTimestamp() const { return event_timestamp_; }
    base::TimeTicks GetProcessingStart() const { return processing_start_; }
    base::TimeTicks GetProcessingEnd() const { return processing_end_; }
    std::optional<int> GetKeyCode() const { return key_code_; }
    std::optional<PointerId> GetPointerId() const { return pointer_id_; }

   private:
    // Event PerformanceEventTiming entry that has not been sent to observers
    // yet: the event dispatch has been completed but the presentation promise
    // used to determine |duration| has not yet been resolved.
    Member<PerformanceEventTiming> event_timing_;
    // Presentation promise index in which the entry in |event_timing_| was
    // added.
    uint64_t presentation_index_;
    // The event creation timestamp.
    base::TimeTicks event_timestamp_;

    base::TimeTicks processing_start_;

    base::TimeTicks processing_end_;
    // Keycode for the event. If the event is not a keyboard event, the keycode
    // wouldn't be set.
    std::optional<int> key_code_;
    // PointerId for the event. If the event is not a pointer event, the
    // PointerId wouldn't be set.
    std::optional<PointerId> pointer_id_;
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
  uint64_t interactionCount() const override;

  bool FirstInputDetected() const { return !!first_input_timing_; }

  void WillShowModalDialog();

  // This method creates a PerformanceEventTiming and if needed creates a
  // presentation promise to calculate the |duration| attribute when such
  // promise is resolved.
  void RegisterEventTiming(const Event& event,
                           EventTarget* event_target,
                           base::TimeTicks start_time,
                           base::TimeTicks processing_start,
                           base::TimeTicks processing_end);

  // Set commit finish time for all pending events that have finished processing
  // and are watiting for presentation promise to resolve.
  void SetCommitFinishTimeStampForPendingEvents(
      base::TimeTicks commit_finish_time);

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

  void OnBodyLoadFinished(int64_t encoded_body_size, int64_t decoded_body_size);
  void ReportLongAnimationFrameTiming(AnimationFrameTimingInfo*);
  // PerformanceMonitor::Client implementation.
  void ReportLongTask(base::TimeTicks start_time,
                      base::TimeTicks end_time,
                      ExecutionContext* task_context,
                      bool has_multiple_contexts) override;

  void AddLayoutShiftEntry(LayoutShift*);
  void AddVisibilityStateEntry(bool is_visible, base::TimeTicks start_time);
  void AddSoftNavigationEntry(const AtomicString& name,
                              base::TimeTicks start_time);

  // PageVisibilityObserver
  void PageVisibilityChanged() override;
  void PageVisibilityChangedWithTimestamp(
      base::TimeTicks visibility_change_timestamp);

  void OnLargestContentfulPaintUpdated(
      base::TimeTicks start_time,
      base::TimeTicks render_time,
      uint64_t paint_size,
      base::TimeTicks load_time,
      base::TimeTicks first_animated_frame_time,
      const AtomicString& id,
      const String& url,
      Element*,
      bool is_triggered_by_soft_navigation);

  void Trace(Visitor*) const override;

  ResponsivenessMetrics& GetResponsivenessMetrics() {
    return *responsiveness_metrics_;
  }

  void NotifyPotentialDrag(PointerId pointer_id);

  void SetCurrentEventTimingEvent(const Event* event) {
    current_event_ = event;
  }
  const Event* GetCurrentEventTimingEvent() { return current_event_.Get(); }

  void CreateNavigationTimingInstance(
      mojom::blink::ResourceTimingInfoPtr navigation_resource_timing);

 private:
  static std::pair<AtomicString, DOMWindow*> SanitizedAttribution(
      ExecutionContext*,
      bool has_multiple_contexts,
      LocalFrame* observer_frame);

  void BuildJSONValue(V8ObjectBuilder&) const override;

  void ReportAllPendingEventTimingsOnPageHidden();

  void FlushEventTimingsOnPageHidden();

  void OnPresentationPromiseResolved(
      uint64_t presentation_index,
      const viz::FrameTimingDetails& presentation_details);
  // Report buffered events with presentation time following their registered
  // order; stop as soon as seeing an event with pending presentation promise.
  void ReportEventTimings();
  void ReportEvent(InteractiveDetector* interactive_detector,
                   Member<EventData> event_data,
                   base::TimeTicks presentation_timestamp);

  void DispatchFirstInputTiming(PerformanceEventTiming* entry);

  // Assign an interaction id to an event timing entry if needed. Also records
  // the interaction latency. Returns true if the entry is ready to be surfaced
  // in PerformanceObservers and the Performance Timeline
  bool SetInteractionIdAndRecordLatency(
      PerformanceEventTiming* entry,
      std::optional<int> key_code,
      std::optional<PointerId> pointer_id,
      ResponsivenessMetrics::EventTimestamps event_timestamps);

  // Notify observer that an event timing entry is ready and add it to the event
  // timing buffer if needed.
  void NotifyAndAddEventTimingBuffer(PerformanceEventTiming* entry);

  // Return a valid fallback time in event timing if there's one; otherwise
  // return nullopt.
  std::optional<base::TimeTicks> GetFallbackTime(
      PerformanceEventTiming* entry,
      base::TimeTicks event_timestamp,
      base::TimeTicks processing_end,
      base::TimeTicks presentation_timestamp);

  // The last time the page visibility was changed.
  base::TimeTicks last_hidden_timestamp_;

  // A list of timestamps that javascript modal dialogs was showing. These are
  // timestamps right before start showing each dialog.
  Deque<base::TimeTicks> show_modal_dialog_timestamps_;

  // Controls if we register a new presentation promise upon events arrival.
  bool need_new_promise_for_event_presentation_time_ = true;
  // Counts the total number of presentation promises we've registered for
  // events' presentation feedback since the beginning.
  uint64_t event_presentation_promise_count_ = 0;
  // Map from presentation promise index to pending event presentation
  // timestamp. It gets emptied consistently once corresponding entries are
  // reported.
  HashMap<uint64_t, base::TimeTicks> pending_event_presentation_time_map_;
  // Store all event timing and latency related data, including
  // PerformanceEventTiming, presentation_index, keycode and pointerId.
  // We use the data to calculate events latencies.
  HeapDeque<Member<EventData>> events_data_;
  Member<PerformanceEventTiming> first_pointer_down_event_timing_;
  Member<EventCounts> event_counts_;
  mutable Member<PerformanceNavigation> navigation_;
  mutable Member<PerformanceTiming> timing_;
  mutable Member<PerformanceTimingForReporting> timing_for_reporting_;
  DOMHighResTimeStamp pending_pointer_down_start_time_;
  std::optional<base::TimeDelta> pending_pointer_down_processing_time_;
  std::optional<base::TimeDelta> pending_pointer_down_time_to_next_paint_;

  // Calculate responsiveness metrics and record UKM for them.
  Member<ResponsivenessMetrics> responsiveness_metrics_;
  // The event we are currently processing.
  WeakMember<const Event> current_event_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_WINDOW_PERFORMANCE_H_
