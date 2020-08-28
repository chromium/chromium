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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_double.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_navigation_timing.h"
#include "third_party/blink/renderer/core/timing/performance_paint_timing.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace blink {

class PerformanceMarkOptions;
class EventCounts;
class ExceptionState;
class LargestContentfulPaint;
class LayoutShift;
class MemoryInfo;
class Node;
class PerformanceElementTiming;
class PerformanceEventTiming;
class PerformanceMark;
class PerformanceMeasure;
class PerformanceNavigation;
class PerformanceObserver;
class PerformanceTiming;
class ProfilerInitOptions;
class ResourceResponse;
class ResourceTimingInfo;
class ScriptPromise;
class ScriptState;
class ScriptValue;
class SecurityOrigin;
class StringOrPerformanceMeasureOptions;
class UserTiming;
class V8ObjectBuilder;

using PerformanceEntryVector = HeapVector<Member<PerformanceEntry>>;
using PerformanceEntryDeque = HeapDeque<Member<PerformanceEntry>>;

class CORE_EXPORT Performance : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~Performance() override;

  const AtomicString& InterfaceName() const override;

  // Overriden by WindowPerformance but not by WorkerPerformance.
  virtual PerformanceTiming* timing() const;
  virtual PerformanceNavigation* navigation() const;
  virtual MemoryInfo* memory() const;
  virtual ScriptPromise measureMemory(ScriptState*,
                                      ExceptionState& exception_state) const;
  virtual EventCounts* eventCounts();

  // Reduce the resolution to prevent timing attacks. See:
  // http://www.w3.org/TR/hr-time-2/#privacy-security
  static double ClampTimeResolution(double time_seconds);

  static DOMHighResTimeStamp MonotonicTimeToDOMHighResTimeStamp(
      base::TimeTicks time_origin,
      base::TimeTicks monotonic_time,
      bool allow_negative_value);

  static base::TimeDelta MonotonicTimeToTimeDelta(
      base::TimeTicks time_origin,
      base::TimeTicks monotonic_time,
      bool allow_negative_value);

  // Translate given platform monotonic time in seconds into a high resolution
  // DOMHighResTimeStamp in milliseconds. The result timestamp is relative to
  // document's time origin and has a time resolution that is safe for
  // exposing to web.
  DOMHighResTimeStamp MonotonicTimeToDOMHighResTimeStamp(base::TimeTicks) const;
  DOMHighResTimeStamp now() const;

  // Translate given platform monotonic time in seconds into base::TimeDelta.
  // The result timestamp is relative to document's time origin and is
  // equivalent to the timestamp returned by the function
  // MonotonicTimeToDOMHighResTimeStamp.
  base::TimeDelta MonotonicTimeToTimeDelta(base::TimeTicks) const;

  // High Resolution Time Level 3 timeOrigin.
  // (https://www.w3.org/TR/hr-time-3/#dom-performance-timeorigin)
  DOMHighResTimeStamp timeOrigin() const;

  // Internal getter method for the time origin value.
  double GetTimeOrigin() const {
    return time_origin_.since_origin().InSecondsF();
  }

  PerformanceEntryVector getEntries();
  // Get BufferedEntriesByType will return all entries in the buffer regardless
  // of whether they are exposed in the Performance Timeline. getEntriesByType
  // will only return all entries for existing types in
  // PerformanceEntry.IsValidTimelineEntryType.
  PerformanceEntryVector getBufferedEntriesByType(
      const AtomicString& entry_type);
  PerformanceEntryVector getEntriesByType(const AtomicString& entry_type);
  PerformanceEntryVector getEntriesByName(
      const AtomicString& name,
      const AtomicString& entry_type = g_null_atom);

  void clearResourceTimings();
  void setResourceTimingBufferSize(unsigned);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(resourcetimingbufferfull,
                                  kResourcetimingbufferfull)

  void AddLongTaskTiming(base::TimeTicks start_time,
                         base::TimeTicks end_time,
                         const AtomicString& name,
                         const AtomicString& container_type,
                         const String& container_src,
                         const String& container_id,
                         const String& container_name);

  // Generates and add a performance entry for the given ResourceTimingInfo.
  // |overridden_initiator_type| allows the initiator type to be overridden to
  // the frame element name for the main resource.
  void GenerateAndAddResourceTiming(
      const ResourceTimingInfo&,
      const AtomicString& overridden_initiator_type = g_null_atom);
  // Generates timing info suitable for appending to the performance entries of
  // a context with |origin|. This should be rarely used; most callsites should
  // prefer the convenience method |GenerateAndAddResourceTiming()|.
  static mojom::blink::ResourceTimingInfoPtr GenerateResourceTiming(
      const SecurityOrigin& destination_origin,
      const ResourceTimingInfo&,
      ExecutionContext& context_for_use_counter);
  void AddResourceTiming(
      mojom::blink::ResourceTimingInfoPtr,
      const AtomicString& initiator_type,
      mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
          worker_timing_receiver,
      ExecutionContext* context);

  void NotifyNavigationTimingToObservers();

  void AddFirstPaintTiming(base::TimeTicks start_time);

  void AddFirstContentfulPaintTiming(base::TimeTicks start_time);

  bool IsElementTimingBufferFull() const;
  void AddElementTimingBuffer(PerformanceElementTiming&);

  bool IsEventTimingBufferFull() const;
  void AddEventTimingBuffer(PerformanceEventTiming&);

  void AddLayoutShiftBuffer(LayoutShift&);

  void AddLargestContentfulPaint(LargestContentfulPaint*);

  PerformanceMark* mark(ScriptState*,
                        const AtomicString& mark_name,
                        PerformanceMarkOptions* mark_options,
                        ExceptionState&);

  void clearMarks(const AtomicString& mark_name);
  void clearMarks() { return clearMarks(AtomicString()); }

  // This enum is used to index different possible strings for for UMA enum
  // histogram. New enum values can be added, but existing enums must never be
  // renumbered or deleted and reused.
  // This enum should be consistent with MeasureParameterType
  // in tools/metrics/histograms/enums.xml.
  enum class MeasureParameterType {
    kObjectObject = 0,
    // 1 to 8, 13 to 25 are navigation-timing types.
    kUnloadEventStart = 1,
    kUnloadEventEnd = 2,
    kDomInteractive = 3,
    kDomContentLoadedEventStart = 4,
    kDomContentLoadedEventEnd = 5,
    kDomComplete = 6,
    kLoadEventStart = 7,
    kLoadEventEnd = 8,
    kOther = 9,
    kUndefinedOrNull = 10,
    // Intentionally leaves out kNumber = 11 since number has been casted to
    // string when users pass number into the API.
    kUnprovided = 12,
    kNavigationStart = 13,
    kRedirectStart = 14,
    kRedirectEnd = 15,
    kFetchStart = 16,
    kDomainLookupStart = 17,
    kDomainLookupEnd = 18,
    kConnectStart = 19,
    kConnectEnd = 20,
    kSecureConnectionStart = 21,
    kRequestStart = 22,
    kResponseStart = 23,
    kResponseEnd = 24,
    kDomLoading = 25,
    kMaxValue = kDomLoading
  };

  UserTiming& GetUserTiming();
  PerformanceMeasure* measure(ScriptState*,
                              const AtomicString& measure_name,
                              ExceptionState&);

  PerformanceMeasure* measure(
      ScriptState*,
      const AtomicString& measure_name,
      const StringOrPerformanceMeasureOptions& start_or_options,
      ExceptionState&);

  PerformanceMeasure* measure(
      ScriptState*,
      const AtomicString& measure_name,
      const StringOrPerformanceMeasureOptions& start_or_options,
      const String& end,
      ExceptionState&);

  void clearMeasures(const AtomicString& measure_name);
  void clearMeasures() { return clearMeasures(AtomicString()); }

  ScriptPromise profile(ScriptState*,
                        const ProfilerInitOptions*,
                        ExceptionState&);

  void UnregisterPerformanceObserver(PerformanceObserver&);
  void RegisterPerformanceObserver(PerformanceObserver&);
  void UpdatePerformanceObserverFilterOptions();
  void ActivateObserver(PerformanceObserver&);
  void SuspendObserver(PerformanceObserver&);

  bool HasObserverFor(PerformanceEntry::EntryType) const;

  // Checks whether the single ResourceResponse passes the Timing-Allow-Origin
  // check. The first parameter is the ResourceResponse being checked. The
  // second parameter is the next ResourceResponse in the redirect chain, or is
  // equal to the first parameter if there is no such response. This parameter
  // is only introduced temporarily to enable computing a UseCounter within this
  // method. The first bool parameter is
  // https://fetch.spec.whatwg.org/#concept-request-response-tainting, while the
  // second bool is
  // https://fetch.spec.whatwg.org/#concept-request-tainted-origin.
  // The next ResourceResponse and tainted origin flag are currently only being
  // used in a UseCounter.
  static bool PassesTimingAllowCheck(const ResourceResponse& response,
                                     const ResourceResponse& next_response,
                                     const SecurityOrigin&,
                                     ExecutionContext*,
                                     bool* response_tainting_not_basic,
                                     bool* tainted_origin_flag);

  static bool AllowsTimingRedirect(const Vector<ResourceResponse>&,
                                   const ResourceResponse&,
                                   const SecurityOrigin&,
                                   ExecutionContext*);

  // Determine whether a given Node can be exposed via a Web Perf API.
  static bool CanExposeNode(Node*);

  ScriptValue toJSONForBinding(ScriptState*) const;

  void Trace(Visitor*) const override;

  // The caller owns the |clock|.
  void SetClocksForTesting(const base::Clock* clock,
                           const base::TickClock* tick_clock);
  void ResetTimeOriginForTesting(base::TimeTicks time_origin);

 private:
  void AddPaintTiming(PerformancePaintTiming::PaintType,
                      base::TimeTicks start_time);

  PerformanceMeasure* MeasureInternal(
      ScriptState*,
      const AtomicString& measure_name,
      const StringOrPerformanceMeasureOptions& start,
      base::Optional<String> end_mark,
      ExceptionState&);

  PerformanceMeasure* MeasureWithDetail(
      ScriptState*,
      const AtomicString& measure_name,
      const base::Optional<StringOrDouble>& start,
      const base::Optional<double>& duration,
      const base::Optional<StringOrDouble>& end,
      const ScriptValue& detail,
      ExceptionState&);

  void CopySecondaryBuffer();
  PerformanceEntryVector getEntriesByTypeInternal(
      PerformanceEntry::EntryType type);

  void MeasureMemoryExperimentTimerFired(TimerBase*);

 protected:
  Performance(base::TimeTicks time_origin,
              scoped_refptr<base::SingleThreadTaskRunner>);

  // Expect WindowPerformance to override this method,
  // WorkerPerformance doesn't have to override this.
  virtual PerformanceNavigationTiming* CreateNavigationTimingInstance() {
    return nullptr;
  }

  bool CanAddResourceTimingEntry();
  void FireResourceTimingBufferFull(TimerBase*);

  void NotifyObserversOfEntry(PerformanceEntry&) const;

  void DeliverObservationsTimerFired(TimerBase*);

  virtual void BuildJSONValue(V8ObjectBuilder&) const;

  PerformanceEntryVector resource_timing_buffer_;
  // The secondary RT buffer, used to store incoming entries after the main
  // buffer is full, until the resourcetimingbufferfull event fires.
  PerformanceEntryDeque resource_timing_secondary_buffer_;
  unsigned resource_timing_buffer_size_limit_;
  // A flag indicating that the buffer became full, the appropriate event was
  // queued, but haven't yet fired.
  bool resource_timing_buffer_full_event_pending_ = false;
  PerformanceEntryVector event_timing_buffer_;
  unsigned event_timing_buffer_max_size_;
  PerformanceEntryVector element_timing_buffer_;
  unsigned element_timing_buffer_max_size_;
  PerformanceEntryVector layout_shift_buffer_;
  PerformanceEntryVector largest_contentful_paint_buffer_;
  PerformanceEntryVector longtask_buffer_;
  PerformanceEntryVector visibility_state_buffer_;
  Member<PerformanceEntry> navigation_timing_;
  Member<UserTiming> user_timing_;
  Member<PerformanceEntry> first_paint_timing_;
  Member<PerformanceEntry> first_contentful_paint_timing_;
  Member<PerformanceEventTiming> first_input_timing_;

  base::TimeTicks time_origin_;
  DOMHighResTimeStamp unix_at_zero_monotonic_;
  const base::TickClock* tick_clock_;

  PerformanceEntryTypeMask observer_filter_options_;
  HeapLinkedHashSet<Member<PerformanceObserver>> observers_;
  HeapLinkedHashSet<Member<PerformanceObserver>> active_observers_;
  HeapLinkedHashSet<Member<PerformanceObserver>> suspended_observers_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  TaskRunnerTimer<Performance> deliver_observations_timer_;
  TaskRunnerTimer<Performance> resource_timing_buffer_full_timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_H_
