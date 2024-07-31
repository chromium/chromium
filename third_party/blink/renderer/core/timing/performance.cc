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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/timing/performance.h"

#include <algorithm>
#include <optional>

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/permissions_policy/document_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_mark_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_measure_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_init_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_performancemeasureoptions_string.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/back_forward_cache_restoration.h"
#include "third_party/blink/renderer/core/timing/background_tracing_helper.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/layout_shift.h"
#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_controller.h"
#include "third_party/blink/renderer/core/timing/performance_element_timing.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_long_task_timing.h"
#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/core/timing/performance_measure.h"
#include "third_party/blink/renderer/core/timing/performance_observer.h"
#include "third_party/blink/renderer/core/timing/performance_resource_timing.h"
#include "third_party/blink/renderer/core/timing/performance_server_timing.h"
#include "third_party/blink/renderer/core/timing/performance_user_timing.h"
#include "third_party/blink/renderer/core/timing/profiler.h"
#include "third_party/blink/renderer/core/timing/profiler_group.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_entry.h"
#include "third_party/blink/renderer/core/timing/time_clamper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "v8/include/v8-metrics.h"

namespace blink {

namespace {

// LongTask API can be a source of many events. Filter on Performance object
// level before reporting to UKM to smooth out recorded events over all pages.
constexpr size_t kLongTaskUkmSampleInterval = 100;

const char kSwapsPerInsertionHistogram[] =
    "Renderer.Core.Timing.Performance.SwapsPerPerformanceEntryInsertion";

bool IsMeasureOptionsEmpty(const PerformanceMeasureOptions& options) {
  return !options.hasDetail() && !options.hasEnd() && !options.hasStart() &&
         !options.hasDuration();
}

base::TimeDelta GetUnixAtZeroMonotonic(const base::Clock* clock,
                                       const base::TickClock* tick_clock) {
  base::TimeDelta unix_time_now = clock->Now() - base::Time::UnixEpoch();
  base::TimeDelta time_since_origin = tick_clock->NowTicks().since_origin();
  return unix_time_now - time_since_origin;
}

void RecordLongTaskUkm(ExecutionContext* execution_context,
                       base::TimeDelta start_time,
                       base::TimeDelta duration) {
  v8::metrics::LongTaskStats stats =
      v8::metrics::LongTaskStats::Get(execution_context->GetIsolate());
  // TODO(cbruni, 1275056): Filter out stats without v8_execute_us.
  ukm::builders::PerformanceAPI_LongTask(execution_context->UkmSourceID())
      .SetStartTime(start_time.InMilliseconds())
      .SetDuration(duration.InMicroseconds())
      .SetDuration_V8_GC(stats.gc_full_atomic_wall_clock_duration_us +
                         stats.gc_full_incremental_wall_clock_duration_us +
                         stats.gc_young_wall_clock_duration_us)
      .SetDuration_V8_GC_Full_Atomic(
          stats.gc_full_atomic_wall_clock_duration_us)
      .SetDuration_V8_GC_Full_Incremental(
          stats.gc_full_incremental_wall_clock_duration_us)
      .SetDuration_V8_GC_Young(stats.gc_young_wall_clock_duration_us)
      .SetDuration_V8_Execute(stats.v8_execute_us)
      .Record(execution_context->UkmRecorder());
}

PerformanceEntry::EntryType kDroppableEntryTypes[] = {
    PerformanceEntry::kResource,
    PerformanceEntry::kLongTask,
    PerformanceEntry::kElement,
    PerformanceEntry::kEvent,
    PerformanceEntry::kLayoutShift,
    PerformanceEntry::kLargestContentfulPaint,
    PerformanceEntry::kPaint,
    PerformanceEntry::kBackForwardCacheRestoration,
    PerformanceEntry::kSoftNavigation,
};

void SwapEntries(PerformanceEntryVector& entries,
                 int leftIndex,
                 int rightIndex) {
  auto tmp = entries[leftIndex];
  entries[leftIndex] = entries[rightIndex];
  entries[rightIndex] = tmp;
}

inline bool CheckName(const PerformanceEntry* entry,
                      const AtomicString& maybe_name) {
  // If we're not filtering by name, then any entry matches.
  if (!maybe_name) {
    return true;
  }
  return entry->name() == maybe_name;
}

// |output_entries| either gets reassigned to or is appended to.
// Therefore, it must point to a valid PerformanceEntryVector.
void FilterEntriesTriggeredBySoftNavigationIfNeeded(
    PerformanceEntryVector& input_entries,
    PerformanceEntryVector** output_entries,
    bool include_soft_navigation_observations) {
  if (include_soft_navigation_observations) {
    *output_entries = &input_entries;
  } else {
    DCHECK(output_entries && *output_entries);
    std::copy_if(input_entries.begin(), input_entries.end(),
                 std::back_inserter(**output_entries),
                 [&](const PerformanceEntry* entry) {
                   return !entry->IsTriggeredBySoftNavigation();
                 });
  }
}

}  // namespace

PerformanceEntryVector MergePerformanceEntryVectors(
    const PerformanceEntryVector& first_entry_vector,
    const PerformanceEntryVector& second_entry_vector,
    const AtomicString& maybe_name) {
  PerformanceEntryVector merged_entries;
  merged_entries.reserve(first_entry_vector.size() +
                         second_entry_vector.size());

  auto first_it = first_entry_vector.begin();
  auto first_end = first_entry_vector.end();
  auto second_it = second_entry_vector.begin();
  auto second_end = second_entry_vector.end();

  // Advance the second iterator past any entries with disallowed names.
  while (second_it != second_end && !CheckName(*second_it, maybe_name)) {
    ++second_it;
  }

  auto PushBackSecondIteratorAndAdvance = [&]() {
    DCHECK(CheckName(*second_it, maybe_name));
    merged_entries.push_back(*second_it);
    ++second_it;
    while (second_it != second_end && !CheckName(*second_it, maybe_name)) {
      ++second_it;
    }
  };

  // What follows is based roughly on a reference implementation of std::merge,
  // except that after copying a value from the second iterator, it must also
  // advance the second iterator past any entries with disallowed names.

  while (first_it != first_end) {
    // If the second iterator has ended, just copy the rest of the contents
    // from the first iterator.
    if (second_it == second_end) {
      std::copy(first_it, first_end, std::back_inserter(merged_entries));
      break;
    }

    // Add an entry to the result vector from either the first or second
    // iterator, whichever has an earlier time. The first iterator wins ties.
    if (PerformanceEntry::StartTimeCompareLessThan(*second_it, *first_it)) {
      PushBackSecondIteratorAndAdvance();
    } else {
      DCHECK(CheckName(*first_it, maybe_name));
      merged_entries.push_back(*first_it);
      ++first_it;
    }
  }

  // If there are still entries in the second iterator after the first iterator
  // has ended, copy all remaining entries that have allowed names.
  while (second_it != second_end) {
    PushBackSecondIteratorAndAdvance();
  }

  return merged_entries;
}

using PerformanceObserverVector = HeapVector<Member<PerformanceObserver>>;

constexpr size_t kDefaultResourceTimingBufferSize = 250;
constexpr size_t kDefaultEventTimingBufferSize = 150;
constexpr size_t kDefaultElementTimingBufferSize = 150;
constexpr size_t kDefaultLayoutShiftBufferSize = 150;
constexpr size_t kDefaultLargestContenfulPaintSize = 150;
constexpr size_t kDefaultLongTaskBufferSize = 200;
constexpr size_t kDefaultLongAnimationFrameBufferSize = 200;
constexpr size_t kDefaultBackForwardCacheRestorationBufferSize = 200;
constexpr size_t kDefaultSoftNavigationBufferSize = 50;
// Paint timing entries is more than twice as much as the soft navigation buffer
// size, as there can be 2 paint entries for each soft navigation, plus 2
// entries for the initial navigation.
constexpr size_t kDefaultPaintEntriesBufferSize =
    kDefaultSoftNavigationBufferSize * 2 + 2;

Performance::Performance(
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ExecutionContext* context)
    : resource_timing_buffer_size_limit_(kDefaultResourceTimingBufferSize),
      back_forward_cache_restoration_buffer_size_limit_(
          kDefaultBackForwardCacheRestorationBufferSize),
      event_timing_buffer_max_size_(kDefaultEventTimingBufferSize),
      element_timing_buffer_max_size_(kDefaultElementTimingBufferSize),
      user_timing_(nullptr),
      time_origin_(time_origin),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      cross_origin_isolated_capability_(cross_origin_isolated_capability),
      observer_filter_options_(PerformanceEntry::kInvalid),
      task_runner_(std::move(task_runner)),
      deliver_observations_timer_(task_runner_,
                                  this,
                                  &Performance::DeliverObservationsTimerFired),
      resource_timing_buffer_full_timer_(
          task_runner_,
          this,
          &Performance::FireResourceTimingBufferFull) {
  unix_at_zero_monotonic_ =
      GetUnixAtZeroMonotonic(base::DefaultClock::GetInstance(), tick_clock_);
  // |context| may be null in tests.
  if (context) {
    background_tracing_helper_ =
        MakeGarbageCollected<BackgroundTracingHelper>(context);
  }
  // Initialize the map of dropped entry types only with those which could be
  // dropped (saves some unnecessary 0s).
  for (const auto type : kDroppableEntryTypes) {
    dropped_entries_count_map_.insert(type, 0);
  }
}

Performance::~Performance() = default;

const AtomicString& Performance::InterfaceName() const {
  return event_target_names::kPerformance;
}

PerformanceTiming* Performance::timing() const {
  return nullptr;
}

PerformanceNavigation* Performance::navigation() const {
  return nullptr;
}

MemoryInfo* Performance::memory(ScriptState*) const {
  return nullptr;
}

EventCounts* Performance::eventCounts() {
  return nullptr;
}

ScriptPromise<MemoryMeasurement> Performance::measureUserAgentSpecificMemory(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return MeasureMemoryController::StartMeasurement(script_state,
                                                   exception_state);
}

DOMHighResTimeStamp Performance::timeOrigin() const {
  DCHECK(!time_origin_.is_null());
  base::TimeDelta time_origin_from_zero_monotonic =
      time_origin_ - base::TimeTicks();
  return ClampTimeResolution(
      unix_at_zero_monotonic_ + time_origin_from_zero_monotonic,
      cross_origin_isolated_capability_);
}

PerformanceEntryVector Performance::getEntries() {
  return GetEntriesForCurrentFrame();
}

PerformanceEntryVector Performance::getEntries(
    ScriptState* script_state,
    PerformanceEntryFilterOptions* options) {
  if (!RuntimeEnabledFeatures::CrossFramePerformanceTimelineEnabled() ||
      !options) {
    return GetEntriesForCurrentFrame();
  }

  PerformanceEntryVector entries;

  AtomicString name =
      options->hasName() ? AtomicString(options->name()) : g_null_atom;

  AtomicString entry_type = options->hasEntryType()
                                ? AtomicString(options->entryType())
                                : g_null_atom;

  // Get sorted entry list based on provided input.
  if (options->getIncludeChildFramesOr(false)) {
    entries = GetEntriesWithChildFrames(script_state, entry_type, name);
  } else {
    if (!entry_type) {
      entries = GetEntriesForCurrentFrame(name);
    } else {
      entries = GetEntriesByTypeForCurrentFrame(entry_type, name);
    }
  }

  return entries;
}

PerformanceEntryVector Performance::GetEntriesForCurrentFrame(
    const AtomicString& maybe_name) {
  PerformanceEntryVector entries;

  entries = MergePerformanceEntryVectors(entries, resource_timing_buffer_,
                                         maybe_name);
  if (first_input_timing_ && CheckName(first_input_timing_, maybe_name)) {
    InsertEntryIntoSortedBuffer(entries, *first_input_timing_,
                                kDoNotRecordSwaps);
  }
  // This extra checking is needed when WorkerPerformance
  // calls this method.
  if (navigation_timing_ && CheckName(navigation_timing_, maybe_name)) {
    InsertEntryIntoSortedBuffer(entries, *navigation_timing_,
                                kDoNotRecordSwaps);
  }

  if (user_timing_) {
    if (maybe_name) {
      // UserTiming already stores lists of marks and measures by name, so
      // requesting them directly is much more efficient than getting the full
      // lists of marks and measures and then filtering during the merge.
      entries = MergePerformanceEntryVectors(
          entries, user_timing_->GetMarks(maybe_name), g_null_atom);
      entries = MergePerformanceEntryVectors(
          entries, user_timing_->GetMeasures(maybe_name), g_null_atom);
    } else {
      entries = MergePerformanceEntryVectors(entries, user_timing_->GetMarks(),
                                             g_null_atom);
      entries = MergePerformanceEntryVectors(
          entries, user_timing_->GetMeasures(), g_null_atom);
    }
  }

  if (paint_entries_timing_.size()) {
    entries = MergePerformanceEntryVectors(entries, paint_entries_timing_,
                                           maybe_name);
  }

  if (RuntimeEnabledFeatures::NavigationIdEnabled(GetExecutionContext())) {
    entries = MergePerformanceEntryVectors(
        entries, back_forward_cache_restoration_buffer_, maybe_name);
  }

  if (RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(
          GetExecutionContext()) &&
      soft_navigation_buffer_.size()) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kSoftNavigationHeuristics);
    entries = MergePerformanceEntryVectors(entries, soft_navigation_buffer_,
                                           maybe_name);
  }

  if (RuntimeEnabledFeatures::LongAnimationFrameTimingEnabled(
          GetExecutionContext()) &&
      long_animation_frame_buffer_.size()) {
    entries = MergePerformanceEntryVectors(
        entries, long_animation_frame_buffer_, maybe_name);
  }

  if (visibility_state_buffer_.size()) {
    entries = MergePerformanceEntryVectors(entries, visibility_state_buffer_,
                                           maybe_name);
  }

  return entries;
}

PerformanceEntryVector Performance::getBufferedEntriesByType(
    const AtomicString& entry_type,
    bool include_soft_navigation_observations) {
  PerformanceEntry::EntryType type =
      PerformanceEntry::ToEntryTypeEnum(entry_type);
  return getEntriesByTypeInternal(type, /*maybe_name=*/g_null_atom,
                                  include_soft_navigation_observations);
}

PerformanceEntryVector Performance::getEntriesByType(
    const AtomicString& entry_type) {
  return GetEntriesByTypeForCurrentFrame(entry_type);
}

PerformanceEntryVector Performance::GetEntriesByTypeForCurrentFrame(
    const AtomicString& entry_type,
    const AtomicString& maybe_name) {
  PerformanceEntry::EntryType type =
      PerformanceEntry::ToEntryTypeEnum(entry_type);
  if (!PerformanceEntry::IsValidTimelineEntryType(type)) {
    PerformanceEntryVector empty_entries;
    if (ExecutionContext* execution_context = GetExecutionContext()) {
      String message = "Deprecated API for given entry type.";
      execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kWarning, message));
    }
    return empty_entries;
  }
  return getEntriesByTypeInternal(type, maybe_name);
}

PerformanceEntryVector Performance::getEntriesByTypeInternal(
    PerformanceEntry::EntryType type,
    const AtomicString& maybe_name,
    bool include_soft_navigation_observations) {
  // This vector may be used by any cases below which require local storage.
  // Cases which refer to pre-existing vectors may simply set `entries` instead.
  PerformanceEntryVector entries_storage;

  PerformanceEntryVector* entries = &entries_storage;
  bool already_filtered_by_name = false;
  switch (type) {
    case PerformanceEntry::kResource:
      UseCounter::Count(GetExecutionContext(), WebFeature::kResourceTiming);
      entries = &resource_timing_buffer_;
      break;

    case PerformanceEntry::kElement:
      entries = &element_timing_buffer_;
      break;

    case PerformanceEntry::kEvent:
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kEventTimingExplicitlyRequested);
      entries = &event_timing_buffer_;
      break;

    case PerformanceEntry::kFirstInput:
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kEventTimingExplicitlyRequested);
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kEventTimingFirstInputExplicitlyRequested);
      if (first_input_timing_)
        entries_storage = {first_input_timing_};
      break;

    case PerformanceEntry::kNavigation:
      UseCounter::Count(GetExecutionContext(), WebFeature::kNavigationTimingL2);
      if (navigation_timing_)
        entries_storage = {navigation_timing_};
      break;

    case PerformanceEntry::kMark:
      if (user_timing_) {
        if (maybe_name) {
          entries_storage = user_timing_->GetMarks(maybe_name);
          already_filtered_by_name = true;
        } else {
          entries_storage = user_timing_->GetMarks();
        }
      }
      break;

    case PerformanceEntry::kMeasure:
      if (user_timing_) {
        if (maybe_name) {
          entries_storage = user_timing_->GetMeasures(maybe_name);
          already_filtered_by_name = true;
        } else {
          entries_storage = user_timing_->GetMeasures();
        }
      }
      break;

    case PerformanceEntry::kPaint: {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kPaintTimingRequested);

      FilterEntriesTriggeredBySoftNavigationIfNeeded(
          paint_entries_timing_, &entries,
          include_soft_navigation_observations);
      break;
    }

    case PerformanceEntry::kLongTask:
      entries = &longtask_buffer_;
      break;

    // TaskAttribution & script entries are only associated to longtask entries.
    case PerformanceEntry::kTaskAttribution:
    case PerformanceEntry::kScript:
      break;

    case PerformanceEntry::kLayoutShift:
      entries = &layout_shift_buffer_;
      break;

    case PerformanceEntry::kLargestContentfulPaint:
      FilterEntriesTriggeredBySoftNavigationIfNeeded(
          largest_contentful_paint_buffer_, &entries,
          include_soft_navigation_observations);
      break;

    case PerformanceEntry::kVisibilityState:
      entries = &visibility_state_buffer_;
      break;

    case PerformanceEntry::kBackForwardCacheRestoration:
      if (RuntimeEnabledFeatures::NavigationIdEnabled(GetExecutionContext()))
        entries = &back_forward_cache_restoration_buffer_;
      break;

    case PerformanceEntry::kSoftNavigation:
      if (RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(
              GetExecutionContext())) {
        UseCounter::Count(GetExecutionContext(),
                          WebFeature::kSoftNavigationHeuristics);
        entries = &soft_navigation_buffer_;
      }
      break;

    case PerformanceEntry::kLongAnimationFrame:
      if (RuntimeEnabledFeatures::LongAnimationFrameTimingEnabled(
              GetExecutionContext())) {
        UseCounter::Count(GetExecutionContext(),
                          WebFeature::kLongAnimationFrameRequested);
        entries = &long_animation_frame_buffer_;
      }
      break;

    case PerformanceEntry::kInvalid:
      break;
  }

  DCHECK_NE(entries, nullptr);
  if (!maybe_name || already_filtered_by_name) {
    return *entries;
  }

  PerformanceEntryVector filtered_entries;
  std::copy_if(entries->begin(), entries->end(),
               std::back_inserter(filtered_entries),
               [&](const PerformanceEntry* entry) {
                 return entry->name() == maybe_name;
               });
  return filtered_entries;
}

PerformanceEntryVector Performance::getEntriesByName(
    const AtomicString& name,
    const AtomicString& entry_type) {
  PerformanceEntryVector entries;

  // Get sorted entry list based on provided input.
  if (entry_type.IsNull()) {
    entries = GetEntriesForCurrentFrame(name);
  } else {
    entries = GetEntriesByTypeForCurrentFrame(entry_type, name);
  }

  return entries;
}

PerformanceEntryVector Performance::GetEntriesWithChildFrames(
    ScriptState* script_state,
    const AtomicString& maybe_type,
    const AtomicString& maybe_name) {
  PerformanceEntryVector entries;

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window) {
    return entries;
  }
  LocalFrame* root_frame = window->GetFrame();
  if (!root_frame) {
    return entries;
  }
  const SecurityOrigin* root_origin = window->GetSecurityOrigin();

  HeapDeque<Member<Frame>> queue;
  queue.push_back(root_frame);

  while (!queue.empty()) {
    Frame* current_frame = queue.TakeFirst();

    if (LocalFrame* local_frame = DynamicTo<LocalFrame>(current_frame)) {
      // Get the Performance object from the current frame.
      LocalDOMWindow* current_window = local_frame->DomWindow();
      // As we verified that the frame this was called with is not detached when
      // entring this loop, we can assume that all its children are also not
      // detached, and hence have a window object.
      DCHECK(current_window);

      // Validate that the child frame's origin is the same as the root
      // frame.
      const SecurityOrigin* current_origin =
          current_window->GetSecurityOrigin();
      if (root_origin->IsSameOriginWith(current_origin)) {
        WindowPerformance* window_performance =
            DOMWindowPerformance::performance(*current_window);

        // Get the performance entries based on maybe_type input. Since the root
        // frame can script the current frame, its okay to expose the current
        // frame's performance entries to the root.
        PerformanceEntryVector current_entries;
        if (!maybe_type) {
          current_entries =
              window_performance->GetEntriesForCurrentFrame(maybe_name);
        } else {
          current_entries = window_performance->GetEntriesByTypeForCurrentFrame(
              maybe_type, maybe_name);
        }

        entries.AppendVector(current_entries);
      }
    }

    // Add both Local and Remote Frame children to the queue.
    for (Frame* child = current_frame->FirstChild(); child;
         child = child->NextSibling()) {
      queue.push_back(child);
    }
  }

  std::sort(entries.begin(), entries.end(),
            PerformanceEntry::StartTimeCompareLessThan);

  return entries;
}

void Performance::clearResourceTimings() {
  resource_timing_buffer_.clear();
}

void Performance::setResourceTimingBufferSize(unsigned size) {
  resource_timing_buffer_size_limit_ = size;
}

void Performance::setBackForwardCacheRestorationBufferSizeForTest(
    unsigned size) {
  back_forward_cache_restoration_buffer_size_limit_ = size;
}

void Performance::setEventTimingBufferSizeForTest(unsigned size) {
  event_timing_buffer_max_size_ = size;
}

void Performance::AddResourceTiming(mojom::blink::ResourceTimingInfoPtr info,
                                    const AtomicString& initiator_type) {
  ExecutionContext* context = GetExecutionContext();
  auto* entry = MakeGarbageCollected<PerformanceResourceTiming>(
      std::move(info), initiator_type, time_origin_,
      cross_origin_isolated_capability_, context);
  NotifyObserversOfEntry(*entry);
  // https://w3c.github.io/resource-timing/#dfn-add-a-performanceresourcetiming-entry
  if (CanAddResourceTimingEntry() &&
      !resource_timing_buffer_full_event_pending_) {
    InsertEntryIntoSortedBuffer(resource_timing_buffer_, *entry, kRecordSwaps);
    return;
  }

  // The Resource Timing entries have a special processing model in which there
  // is a secondary buffer but getting those entries requires handling the
  // buffer full event, and the PerformanceObserver with buffered flag only
  // receives the entries from the primary buffer, so it's ok to increase
  // the dropped entries count here.
  ++(dropped_entries_count_map_.find(PerformanceEntry::kResource)->value);
  if (!resource_timing_buffer_full_event_pending_) {
    resource_timing_buffer_full_event_pending_ = true;
    resource_timing_buffer_full_timer_.StartOneShot(base::TimeDelta(),
                                                    FROM_HERE);
  }
  resource_timing_secondary_buffer_.push_back(entry);
}

// Called after loadEventEnd happens.
void Performance::NotifyNavigationTimingToObservers() {
  if (navigation_timing_)
    NotifyObserversOfEntry(*navigation_timing_);
}

bool Performance::IsElementTimingBufferFull() const {
  return element_timing_buffer_.size() >= element_timing_buffer_max_size_;
}

bool Performance::IsEventTimingBufferFull() const {
  return event_timing_buffer_.size() >= event_timing_buffer_max_size_;
}

bool Performance::IsLongAnimationFrameBufferFull() const {
  return long_animation_frame_buffer_.size() >=
         kDefaultLongAnimationFrameBufferSize;
}

void Performance::CopySecondaryBuffer() {
  // https://w3c.github.io/resource-timing/#dfn-copy-secondary-buffer
  while (!resource_timing_secondary_buffer_.empty() &&
         CanAddResourceTimingEntry()) {
    PerformanceEntry* entry = resource_timing_secondary_buffer_.front();
    DCHECK(entry);
    resource_timing_secondary_buffer_.pop_front();
    resource_timing_buffer_.push_back(entry);
  }
}

void Performance::FireResourceTimingBufferFull(TimerBase*) {
  // https://w3c.github.io/resource-timing/#dfn-fire-a-buffer-full-event
  while (!resource_timing_secondary_buffer_.empty()) {
    int excess_entries_before = resource_timing_secondary_buffer_.size();
    if (!CanAddResourceTimingEntry()) {
      DispatchEvent(
          *Event::Create(event_type_names::kResourcetimingbufferfull));
    }
    CopySecondaryBuffer();
    int excess_entries_after = resource_timing_secondary_buffer_.size();
    if (excess_entries_after >= excess_entries_before) {
      resource_timing_secondary_buffer_.clear();
      break;
    }
  }
  resource_timing_buffer_full_event_pending_ = false;
}

void Performance::AddToElementTimingBuffer(PerformanceElementTiming& entry) {
  if (!IsElementTimingBufferFull()) {
    InsertEntryIntoSortedBuffer(element_timing_buffer_, entry, kRecordSwaps);
  } else {
    ++(dropped_entries_count_map_.find(PerformanceEntry::kElement)->value);
  }
}

void Performance::AddToEventTimingBuffer(PerformanceEventTiming& entry) {
  if (!IsEventTimingBufferFull()) {
    InsertEntryIntoSortedBuffer(event_timing_buffer_, entry, kRecordSwaps);
  } else {
    ++(dropped_entries_count_map_.find(PerformanceEntry::kEvent)->value);
  }
}

void Performance::AddToLayoutShiftBuffer(LayoutShift& entry) {
  probe::PerformanceEntryAdded(GetExecutionContext(), &entry);
  if (layout_shift_buffer_.size() < kDefaultLayoutShiftBufferSize) {
    InsertEntryIntoSortedBuffer(layout_shift_buffer_, entry, kRecordSwaps);
  } else {
    ++(dropped_entries_count_map_.find(PerformanceEntry::kLayoutShift)->value);
  }
}

void Performance::AddLargestContentfulPaint(LargestContentfulPaint* entry) {
  probe::PerformanceEntryAdded(GetExecutionContext(), entry);
  if (largest_contentful_paint_buffer_.size() <
      kDefaultLargestContenfulPaintSize) {
    InsertEntryIntoSortedBuffer(largest_contentful_paint_buffer_, *entry,
                                kRecordSwaps);
  } else {
    ++(dropped_entries_count_map_
           .find(PerformanceEntry::kLargestContentfulPaint)
           ->value);
  }
}

void Performance::AddSoftNavigationToPerformanceTimeline(
    SoftNavigationEntry* entry) {
  probe::PerformanceEntryAdded(GetExecutionContext(), entry);
  if (soft_navigation_buffer_.size() < kDefaultSoftNavigationBufferSize) {
    InsertEntryIntoSortedBuffer(soft_navigation_buffer_, *entry, kRecordSwaps);
  } else {
    ++(dropped_entries_count_map_.find(PerformanceEntry::kSoftNavigation)
           ->value);
  }
}

void Performance::AddFirstPaintTiming(base::TimeTicks start_time,
                                      bool is_triggered_by_soft_navigation) {
  AddPaintTiming(PerformancePaintTiming::PaintType::kFirstPaint, start_time,
                 is_triggered_by_soft_navigation);
}

void Performance::AddFirstContentfulPaintTiming(
    base::TimeTicks start_time,
    bool is_triggered_by_soft_navigation) {
  AddPaintTiming(PerformancePaintTiming::PaintType::kFirstContentfulPaint,
                 start_time, is_triggered_by_soft_navigation);
}

void Performance::AddPaintTiming(PerformancePaintTiming::PaintType type,
                                 base::TimeTicks start_time,
                                 bool is_triggered_by_soft_navigation) {
  PerformanceEntry* entry = MakeGarbageCollected<PerformancePaintTiming>(
      type, MonotonicTimeToDOMHighResTimeStamp(start_time),
      DynamicTo<LocalDOMWindow>(GetExecutionContext()),
      is_triggered_by_soft_navigation);
  DCHECK((type == PerformancePaintTiming::PaintType::kFirstPaint) ||
         (type == PerformancePaintTiming::PaintType::kFirstContentfulPaint));
  if (paint_entries_timing_.size() < kDefaultPaintEntriesBufferSize) {
    InsertEntryIntoSortedBuffer(paint_entries_timing_, *entry, kRecordSwaps);
  } else {
    ++(dropped_entries_count_map_.find(PerformanceEntry::kPaint)->value);
  }
  NotifyObserversOfEntry(*entry);
}

bool Performance::CanAddResourceTimingEntry() {
  // https://w3c.github.io/resource-timing/#dfn-can-add-resource-timing-entry
  return resource_timing_buffer_.size() < resource_timing_buffer_size_limit_;
}

void Performance::AddLongTaskTiming(base::TimeTicks start_time,
                                    base::TimeTicks end_time,
                                    const AtomicString& name,
                                    const AtomicString& container_type,
                                    const AtomicString& container_src,
                                    const AtomicString& container_id,
                                    const AtomicString& container_name) {
  double dom_high_res_start_time =
      MonotonicTimeToDOMHighResTimeStamp(start_time);

  ExecutionContext* execution_context = GetExecutionContext();
  auto* entry = MakeGarbageCollected<PerformanceLongTaskTiming>(
      dom_high_res_start_time,
      // Convert the delta between start and end times to an int to reduce the
      // granularity of the duration to 1 ms.
      static_cast<int>(MonotonicTimeToDOMHighResTimeStamp(end_time) -
                       dom_high_res_start_time),
      name, container_type, container_src, container_id, container_name,
      DynamicTo<LocalDOMWindow>(execution_context));
  if (longtask_buffer_.size() < kDefaultLongTaskBufferSize) {
    InsertEntryIntoSortedBuffer(longtask_buffer_, *entry, kRecordSwaps);
  } else {
    ++(dropped_entries_count_map_.find(PerformanceEntry::kLongTask)->value);
    UseCounter::Count(execution_context, WebFeature::kLongTaskBufferFull);
  }
  if ((++long_task_counter_ % kLongTaskUkmSampleInterval) == 0) {
    RecordLongTaskUkm(execution_context,
                      base::Milliseconds(dom_high_res_start_time),
                      end_time - start_time);
  }
  NotifyObserversOfEntry(*entry);
}

void Performance::AddBackForwardCacheRestoration(
    base::TimeTicks start_time,
    base::TimeTicks pageshow_start_time,
    base::TimeTicks pageshow_end_time) {
  auto* entry = MakeGarbageCollected<BackForwardCacheRestoration>(
      MonotonicTimeToDOMHighResTimeStamp(start_time),
      MonotonicTimeToDOMHighResTimeStamp(pageshow_start_time),
      MonotonicTimeToDOMHighResTimeStamp(pageshow_end_time),
      DynamicTo<LocalDOMWindow>(GetExecutionContext()));
  if (back_forward_cache_restoration_buffer_.size() <
      back_forward_cache_restoration_buffer_size_limit_) {
    InsertEntryIntoSortedBuffer(back_forward_cache_restoration_buffer_, *entry,
                                kRecordSwaps);
  } else {
    ++(dropped_entries_count_map_
           .find(PerformanceEntry::kBackForwardCacheRestoration)
           ->value);
  }
  NotifyObserversOfEntry(*entry);
}

UserTiming& Performance::GetUserTiming() {
  if (!user_timing_)
    user_timing_ = MakeGarbageCollected<UserTiming>(*this);
  return *user_timing_;
}

PerformanceMark* Performance::mark(ScriptState* script_state,
                                   const AtomicString& mark_name,
                                   PerformanceMarkOptions* mark_options,
                                   ExceptionState& exception_state) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const AtomicString, mark_fully_loaded,
                                  ("mark_fully_loaded"));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const AtomicString, mark_fully_visible,
                                  ("mark_fully_visible"));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const AtomicString, mark_interactive,
                                  ("mark_interactive"));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const AtomicString, mark_feature_usage,
                                  ("mark_feature_usage"));
  bool has_start_time = mark_options && mark_options->hasStartTime();
  if (has_start_time || (mark_options && mark_options->hasDetail())) {
    UseCounter::Count(GetExecutionContext(), WebFeature::kUserTimingL3);
  }
  PerformanceMark* performance_mark = PerformanceMark::Create(
      script_state, mark_name, mark_options, exception_state);
  if (performance_mark) {
    background_tracing_helper_->MaybeEmitBackgroundTracingPerformanceMarkEvent(
        *performance_mark);
    GetUserTiming().AddMarkToPerformanceTimeline(*performance_mark,
                                                 mark_options);
    if (mark_name == mark_fully_loaded) {
      if (LocalDOMWindow* window = LocalDOMWindow::From(script_state)) {
        window->GetFrame()
            ->Loader()
            .GetDocumentLoader()
            ->GetTiming()
            .SetUserTimingMarkFullyLoaded(
                base::Milliseconds(performance_mark->startTime()));
      }
    } else if (mark_name == mark_fully_visible) {
      if (LocalDOMWindow* window = LocalDOMWindow::From(script_state)) {
        window->GetFrame()
            ->Loader()
            .GetDocumentLoader()
            ->GetTiming()
            .SetUserTimingMarkFullyVisible(
                base::Milliseconds(performance_mark->startTime()));
      }
    } else if (mark_name == mark_interactive) {
      if (LocalDOMWindow* window = LocalDOMWindow::From(script_state)) {
        window->GetFrame()
            ->Loader()
            .GetDocumentLoader()
            ->GetTiming()
            .SetUserTimingMarkInteractive(
                base::Milliseconds(performance_mark->startTime()));
      }
    } else if (mark_name == mark_feature_usage && mark_options->hasDetail()) {
      if (RuntimeEnabledFeatures::PerformanceMarkFeatureUsageEnabled()) {
        ProcessUserFeatureMark(mark_options);
      }
    } else {
      if (LocalDOMWindow* window = LocalDOMWindow::From(script_state)) {
        if (window->GetFrame() && window->GetFrame()->IsOutermostMainFrame()) {
          window->GetFrame()
              ->Loader()
              .GetDocumentLoader()
              ->GetTiming()
              .NotifyCustomUserTimingMarkAdded(
                  mark_name, base::Milliseconds(performance_mark->startTime()));
        }
      }
    }
    NotifyObserversOfEntry(*performance_mark);
  }
  return performance_mark;
}

void Performance::ProcessUserFeatureMark(
    const PerformanceMarkOptions* mark_options) {
  const ExecutionContext* exec_context = GetExecutionContext();
  if (!exec_context) {
    return;
  }

  const ScriptValue& detail = mark_options->detail();
  if (!detail.IsObject()) {
    return;
  }

  v8::Isolate* isolate = GetExecutionContext()->GetIsolate();
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
  v8::Local<v8::Object> object;
  if (!detail.V8Value()->ToObject(current_context).ToLocal(&object)) {
    return;
  }

  v8::Local<v8::Value> user_feature_name_val;
  if (!object->Get(current_context, V8AtomicString(isolate, "feature"))
           .ToLocal(&user_feature_name_val) ||
      user_feature_name_val->IsUndefined()) {
    return;
  }

  v8::Local<v8::String> user_feature_name;
  if (!user_feature_name_val->ToString(current_context)
           .ToLocal(&user_feature_name)) {
    return;
  }

  String blink_user_feature_name =
      ToBlinkString<String>(isolate, user_feature_name, kDoNotExternalize);

  // Check if the user feature name is mapped to an allowed WebFeature.
  auto maybe_web_feature =
      PerformanceMark::GetWebFeatureForUserFeatureName(blink_user_feature_name);
  if (!maybe_web_feature.has_value()) {
    // We have no matching WebFeature translation yet, skip.
    return;
  }

  // Tick the corresponding use counter.
  UseCounter::Count(GetExecutionContext(), maybe_web_feature.value());
}

void Performance::clearMarks(const AtomicString& mark_name) {
  GetUserTiming().ClearMarks(mark_name);
}

PerformanceMeasure* Performance::measure(ScriptState* script_state,
                                         const AtomicString& measure_name,
                                         ExceptionState& exception_state) {
  // When |startOrOptions| is not provided, it's assumed to be an empty
  // dictionary.
  return MeasureInternal(script_state, measure_name, nullptr, std::nullopt,
                         exception_state);
}

PerformanceMeasure* Performance::measure(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const V8UnionPerformanceMeasureOptionsOrString* start_or_options,
    ExceptionState& exception_state) {
  return MeasureInternal(script_state, measure_name, start_or_options,
                         std::nullopt, exception_state);
}

PerformanceMeasure* Performance::measure(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const V8UnionPerformanceMeasureOptionsOrString* start_or_options,
    const String& end,
    ExceptionState& exception_state) {
  return MeasureInternal(script_state, measure_name, start_or_options,
                         std::optional<String>(end), exception_state);
}

// |MeasureInternal| exists to unify the arguments from different
// `performance.measure()` overloads into a consistent form, then delegate to
// |MeasureWithDetail|.
//
// |start_or_options| is either a String or a dictionary of options. When it's
// a String, it represents a starting performance mark. When it's a dictionary,
// the allowed fields are 'start', 'duration', 'end' and 'detail'. However,
// there are some combinations of fields and parameters which must raise
// errors. Specifically, the spec (https://https://w3c.github.io/user-timing/)
// requires errors to thrown in the following cases:
//  - If |start_or_options| is a dictionary and 'end_mark' is passed.
//  - If an options dictionary contains neither a 'start' nor an 'end' field.
//  - If an options dictionary contains all of 'start', 'duration' and 'end'.
//
// |end_mark| will be std::nullopt unless the `performance.measure()` overload
// specified an end mark.
PerformanceMeasure* Performance::MeasureInternal(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const V8UnionPerformanceMeasureOptionsOrString* start_or_options,
    std::optional<String> end_mark,
    ExceptionState& exception_state) {
  // An empty option is treated with no difference as null, undefined.
  if (start_or_options && start_or_options->IsPerformanceMeasureOptions() &&
      !IsMeasureOptionsEmpty(
          *start_or_options->GetAsPerformanceMeasureOptions())) {
    UseCounter::Count(GetExecutionContext(), WebFeature::kUserTimingL3);
    // measure("name", { start, end }, *)
    if (end_mark) {
      exception_state.ThrowTypeError(
          "If a non-empty PerformanceMeasureOptions object was passed, "
          "|end_mark| must not be passed.");
      return nullptr;
    }
    const PerformanceMeasureOptions* options =
        start_or_options->GetAsPerformanceMeasureOptions();
    if (!options->hasStart() && !options->hasEnd()) {
      exception_state.ThrowTypeError(
          "If a non-empty PerformanceMeasureOptions object was passed, at "
          "least one of its 'start' or 'end' properties must be present.");
      return nullptr;
    }

    if (options->hasStart() && options->hasDuration() && options->hasEnd()) {
      exception_state.ThrowTypeError(
          "If a non-empty PerformanceMeasureOptions object was passed, it "
          "must not have all of its 'start', 'duration', and 'end' "
          "properties defined");
      return nullptr;
    }

    V8UnionDoubleOrString* start = options->getStartOr(nullptr);
    std::optional<double> duration;
    if (options->hasDuration()) {
      duration = options->duration();
    }
    V8UnionDoubleOrString* end = options->getEndOr(nullptr);

    return MeasureWithDetail(
        script_state, measure_name, start, duration, end,
        options->hasDetail() ? options->detail() : ScriptValue(),
        exception_state);
  }

  // measure("name", "mark1", *)
  V8UnionDoubleOrString* start = nullptr;
  if (start_or_options && start_or_options->IsString()) {
    start = MakeGarbageCollected<V8UnionDoubleOrString>(
        start_or_options->GetAsString());
  }
  // We let |end_mark| behave the same whether it's empty, undefined or null
  // in JS, as long as |end_mark| is null in C++.
  V8UnionDoubleOrString* end = nullptr;
  if (end_mark) {
    end = MakeGarbageCollected<V8UnionDoubleOrString>(*end_mark);
  }
  return MeasureWithDetail(script_state, measure_name, start,
                           /* duration = */ std::nullopt, end, ScriptValue(),
                           exception_state);
}

PerformanceMeasure* Performance::MeasureWithDetail(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const V8UnionDoubleOrString* start,
    const std::optional<double>& duration,
    const V8UnionDoubleOrString* end,
    const ScriptValue& detail,
    ExceptionState& exception_state) {
  PerformanceMeasure* performance_measure = GetUserTiming().Measure(
      script_state, measure_name, start, duration, end, detail, exception_state,
      LocalDOMWindow::From(script_state));
  if (performance_measure)
    NotifyObserversOfEntry(*performance_measure);
  return performance_measure;
}

void Performance::clearMeasures(const AtomicString& measure_name) {
  GetUserTiming().ClearMeasures(measure_name);
}

void Performance::RegisterPerformanceObserver(PerformanceObserver& observer) {
  observer_filter_options_ |= observer.FilterOptions();
  observers_.insert(&observer);
}

void Performance::UnregisterPerformanceObserver(
    PerformanceObserver& old_observer) {
  observers_.erase(&old_observer);
  UpdatePerformanceObserverFilterOptions();
}

void Performance::UpdatePerformanceObserverFilterOptions() {
  observer_filter_options_ = PerformanceEntry::kInvalid;
  for (const auto& observer : observers_) {
    observer_filter_options_ |= observer->FilterOptions();
  }
}

void Performance::NotifyObserversOfEntry(PerformanceEntry& entry) const {
  bool observer_found = false;
  for (auto& observer : observers_) {
    if (observer->FilterOptions() & entry.EntryTypeEnum() &&
        (!entry.IsTriggeredBySoftNavigation() ||
         observer->IncludeSoftNavigationObservations()) &&
        observer->CanObserve(entry)) {
      observer->EnqueuePerformanceEntry(entry);
      observer_found = true;
    }
  }
  if (observer_found && entry.EntryTypeEnum() == PerformanceEntry::kPaint)
    UseCounter::Count(GetExecutionContext(), WebFeature::kPaintTimingObserved);
}

bool Performance::HasObserverFor(
    PerformanceEntry::EntryType filter_type) const {
  return observer_filter_options_ & filter_type;
}

void Performance::ActivateObserver(PerformanceObserver& observer) {
  if (active_observers_.empty())
    deliver_observations_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  if (suspended_observers_.Contains(&observer))
    suspended_observers_.erase(&observer);
  active_observers_.insert(&observer);
}

void Performance::SuspendObserver(PerformanceObserver& observer) {
  DCHECK(!suspended_observers_.Contains(&observer));
  if (!active_observers_.Contains(&observer))
    return;
  active_observers_.erase(&observer);
  suspended_observers_.insert(&observer);
}

void Performance::DeliverObservationsTimerFired(TimerBase*) {
  decltype(active_observers_) observers;
  active_observers_.Swap(observers);
  for (const auto& observer : observers) {
    observer->Deliver(observer->RequiresDroppedEntries()
                          ? std::optional<int>(GetDroppedEntriesForTypes(
                                observer->FilterOptions()))
                          : std::nullopt);
  }
}

int Performance::GetDroppedEntriesForTypes(PerformanceEntryTypeMask types) {
  int dropped_count = 0;
  for (const auto type : kDroppableEntryTypes) {
    if (types & type)
      dropped_count += dropped_entries_count_map_.at(type);
  }
  return dropped_count;
}

// static
DOMHighResTimeStamp Performance::ClampTimeResolution(
    base::TimeDelta time,
    bool cross_origin_isolated_capability) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(TimeClamper, clamper, ());
  return clamper.ClampTimeResolution(time, cross_origin_isolated_capability)
      .InMillisecondsF();
}

// static
DOMHighResTimeStamp Performance::MonotonicTimeToDOMHighResTimeStamp(
    base::TimeTicks time_origin,
    base::TimeTicks monotonic_time,
    bool allow_negative_value,
    bool cross_origin_isolated_capability) {
  // Avoid exposing raw platform timestamps.
  if (monotonic_time.is_null() || time_origin.is_null())
    return 0.0;

  DOMHighResTimeStamp clamped_time =
      ClampTimeResolution(monotonic_time.since_origin(),
                          cross_origin_isolated_capability) -
      ClampTimeResolution(time_origin.since_origin(),
                          cross_origin_isolated_capability);
  if (clamped_time < 0 && !allow_negative_value)
    return 0.0;
  return clamped_time;
}

DOMHighResTimeStamp Performance::MonotonicTimeToDOMHighResTimeStamp(
    base::TimeTicks monotonic_time) const {
  return MonotonicTimeToDOMHighResTimeStamp(time_origin_, monotonic_time,
                                            false /* allow_negative_value */,
                                            cross_origin_isolated_capability_);
}

DOMHighResTimeStamp Performance::now() const {
  return MonotonicTimeToDOMHighResTimeStamp(tick_clock_->NowTicks());
}

// static
bool Performance::CanExposeNode(Node* node) {
  if (!node || !node->isConnected() || node->IsInShadowTree())
    return false;

  // Do not expose |node| when the document is not 'fully active'.
  const Document& document = node->GetDocument();
  if (!document.IsActive() || !document.GetFrame())
    return false;

  return true;
}

ScriptValue Performance::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  BuildJSONValue(result);
  return result.GetScriptValue();
}

void Performance::BuildJSONValue(V8ObjectBuilder& builder) const {
  builder.AddNumber("timeOrigin", timeOrigin());
  // |memory| is not part of the spec, omitted.
}

// Insert entry in PerformanceEntryVector while maintaining sorted order (via
// Bubble Sort). We assume that the order of insertion roughly corresponds to
// the order of the StartTime, hence the sort beginning from the tail-end.
void Performance::InsertEntryIntoSortedBuffer(PerformanceEntryVector& entries,
                                              PerformanceEntry& entry,
                                              Metrics record) {
  entries.push_back(&entry);

  int number_of_swaps = 0;

  if (entries.size() > 1) {
    // Bubble Sort from tail.
    int left = entries.size() - 2;
    while (left >= 0 &&
           entries[left]->startTime() > entries[left + 1]->startTime()) {
      if (record == kRecordSwaps) {
        UseCounter::Count(GetExecutionContext(),
                          WebFeature::kPerformanceEntryBufferSwaps);
      }
      number_of_swaps++;
      SwapEntries(entries, left, left + 1);
      left--;
    }
  }

  UMA_HISTOGRAM_COUNTS_1000(kSwapsPerInsertionHistogram, number_of_swaps);

  return;
}

void Performance::Trace(Visitor* visitor) const {
  visitor->Trace(resource_timing_buffer_);
  visitor->Trace(resource_timing_secondary_buffer_);
  visitor->Trace(element_timing_buffer_);
  visitor->Trace(event_timing_buffer_);
  visitor->Trace(layout_shift_buffer_);
  visitor->Trace(largest_contentful_paint_buffer_);
  visitor->Trace(longtask_buffer_);
  visitor->Trace(visibility_state_buffer_);
  visitor->Trace(back_forward_cache_restoration_buffer_);
  visitor->Trace(soft_navigation_buffer_);
  visitor->Trace(long_animation_frame_buffer_);
  visitor->Trace(navigation_timing_);
  visitor->Trace(user_timing_);
  visitor->Trace(paint_entries_timing_);
  visitor->Trace(first_input_timing_);
  visitor->Trace(observers_);
  visitor->Trace(active_observers_);
  visitor->Trace(suspended_observers_);
  visitor->Trace(deliver_observations_timer_);
  visitor->Trace(resource_timing_buffer_full_timer_);
  visitor->Trace(background_tracing_helper_);
  EventTarget::Trace(visitor);
}

void Performance::SetClocksForTesting(const base::Clock* clock,
                                      const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  // Recompute |unix_at_zero_monotonic_|.
  unix_at_zero_monotonic_ = GetUnixAtZeroMonotonic(clock, tick_clock_);
}

void Performance::ResetTimeOriginForTesting(base::TimeTicks time_origin) {
  time_origin_ = time_origin;
}

// TODO(https://crbug.com/1457049): remove this once visited links are
// partitioned.
bool Performance::softNavPaintMetricsSupported() const {
  CHECK(
      RuntimeEnabledFeatures::SoftNavigationHeuristicsExposeFPAndFCPEnabled());
  return true;
}

}  // namespace blink
