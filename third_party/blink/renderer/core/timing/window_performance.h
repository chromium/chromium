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

#include "base/rand_util.h"
#include "third_party/blink/public/web/web_swap_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/core/timing/event_counts.h"
#include "third_party/blink/renderer/core/timing/memory_info.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_navigation.h"
#include "third_party/blink/renderer/core/timing/performance_timing.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class IntSize;

class CORE_EXPORT WindowPerformance final : public Performance,
                                            public PerformanceMonitor::Client,
                                            public ExecutionContextClient,
                                            public PageVisibilityObserver {
  friend class WindowPerformanceTest;

 public:
  explicit WindowPerformance(LocalDOMWindow*);
  ~WindowPerformance() override;

  ExecutionContext* GetExecutionContext() const override;

  PerformanceTiming* timing() const override;
  PerformanceNavigation* navigation() const override;

  MemoryInfo* memory() const override;

  EventCounts* eventCounts() override;

  bool FirstInputDetected() const { return !!first_input_timing_; }

  // This method creates a PerformanceEventTiming and if needed creates a swap
  // promise to calculate the |duration| attribute when such promise is
  // resolved.
  void RegisterEventTiming(const AtomicString& event_type,
                           base::TimeTicks start_time,
                           base::TimeTicks processing_start,
                           base::TimeTicks processing_end,
                           bool cancelable,
                           Node*);

  void OnPaintFinished();

  void AddElementTiming(const AtomicString& name,
                        const String& url,
                        const FloatRect& rect,
                        base::TimeTicks start_time,
                        base::TimeTicks load_time,
                        const AtomicString& identifier,
                        const IntSize& intrinsic_size,
                        const AtomicString& id,
                        Element*);

  void AddLayoutShiftEntry(LayoutShift*);
  void AddVisibilityStateEntry(bool is_visible, base::TimeTicks start_time);

  // PageVisibilityObserver
  void PageVisibilityChanged() override;

  void OnLargestContentfulPaintUpdated(base::TimeTicks paint_time,
                                       uint64_t paint_size,
                                       base::TimeTicks load_time,
                                       const AtomicString& id,
                                       const String& url,
                                       Element*);

  void Trace(Visitor*) const override;

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

  // Method called once swap promise is resolved. It will add all event timings
  // that have not been added since the last swap promise.
  void ReportEventTimings(uint64_t frame_index,
                          WebSwapResult result,
                          base::TimeTicks timestamp);

  void DispatchFirstInputTiming(PerformanceEventTiming* entry);

  void MeasureMemoryExperimentTimerFired(TimerBase*);

  // Counter of the current frame index, based on calls to OnPaintFinished().
  uint64_t frame_index_ = 1;
  // Monotonically increasing value with the last frame index on which a swap
  // promise was queued;
  uint64_t last_registered_frame_index_ = 0;
  // Number of pending swap promises.
  uint16_t pending_swap_promise_count_ = 0;
  // PerformanceEventTiming entries that have not been sent to observers yet:
  // the event dispatch has been completed but the swap promise used to
  // determine |duration| has not yet been resolved. It is handled as a queue:
  // FIFO.
  HeapDeque<Member<PerformanceEventTiming>> event_timings_;
  // Entries corresponding to frame indices in which the entries in
  // |event_timings_| were added. This could be combined with |event_timings_|
  // into a single deque, but PerformanceEventTiming is GarbageCollected so it
  // would need to be a HeapDeque. HeapDeque does not allow std::pair as its
  // type, so we would have to add a new wrapper GarbageCollected class that
  // contains the PerformanceEventTiming object as well as the frame index. This
  // is more work than having two separate deques.
  Deque<uint64_t> event_frames_;
  Member<PerformanceEventTiming> first_pointer_down_event_timing_;
  Member<EventCounts> event_counts_;
  mutable Member<PerformanceNavigation> navigation_;
  mutable Member<PerformanceTiming> timing_;

  // This is used in a Finch experiment to perform a memory measurement without
  // reporting the results to evaluate its impact on stability and performance.
  TaskRunnerTimer<WindowPerformance> measure_memory_experiment_timer_;
  static const int kMaxMeasureMemoryExperimentDelayInMs = 30000;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_WINDOW_PERFORMANCE_H_
