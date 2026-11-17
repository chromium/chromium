/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/timing/performance_entry.h"

#include "base/atomic_sequence_num.h"
#include "third_party/blink/public/mojom/timing/performance_mark_or_measure.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace {
static base::AtomicSequenceNumber index_seq;
}  // namespace

PerformanceEntry::PerformanceEntry(const AtomicString& name,
                                   double start_time,
                                   double finish_time,
                                   DOMWindow* source,
                                   uint64_t navigation_id)
    : PerformanceEntry(finish_time - start_time,
                       name,
                       start_time,
                       source,
                       navigation_id) {}

PerformanceEntry::PerformanceEntry(double duration,
                                   const AtomicString& name,
                                   double start_time,
                                   DOMWindow* source,
                                   uint64_t navigation_id)
    : duration_(duration),
      name_(name),
      start_time_(start_time),
      index_(index_seq.GetNext()),
      source_(source),
      navigation_id_(navigation_id) {}

PerformanceEntry::~PerformanceEntry() = default;

DOMHighResTimeStamp PerformanceEntry::startTime() const {
  return start_time_;
}

DOMHighResTimeStamp PerformanceEntry::duration() const {
  return duration_;
}

uint64_t PerformanceEntry::navigationId() const {
  return navigation_id_;
}

DOMWindow* PerformanceEntry::source() const {
  return source_.Get();
}

mojom::blink::PerformanceMarkOrMeasurePtr
PerformanceEntry::ToMojoPerformanceMarkOrMeasure() {
  DCHECK(EntryTypeEnum() == kMark || EntryTypeEnum() == kMeasure);
  auto mojo_performance_mark_or_measure =
      mojom::blink::PerformanceMarkOrMeasure::New();
  mojo_performance_mark_or_measure->name = name_;
  mojo_performance_mark_or_measure->entry_type =
      EntryTypeEnum() == kMark
          ? mojom::blink::PerformanceMarkOrMeasure::EntryType::kMark
          : mojom::blink::PerformanceMarkOrMeasure::EntryType::kMeasure;
  mojo_performance_mark_or_measure->start_time = start_time_;
  mojo_performance_mark_or_measure->duration = duration_;
  // PerformanceMark/Measure overrides will add the detail field.
  return mojo_performance_mark_or_measure;
}

PerformanceEntry::EntryType PerformanceEntry::ToEntryTypeEnum(
    const AtomicString& entry_type) {
  if (entry_type == performance_entry_names::kLongtask) {
    return kLongTask;
  }
  if (entry_type == performance_entry_names::kMark) {
    return kMark;
  }
  if (entry_type == performance_entry_names::kMeasure) {
    return kMeasure;
  }
  if (entry_type == performance_entry_names::kResource) {
    return kResource;
  }
  if (entry_type == performance_entry_names::kNavigation) {
    return kNavigation;
  }
  if (entry_type == performance_entry_names::kTaskattribution) {
    return kTaskAttribution;
  }
  if (entry_type == performance_entry_names::kPaint) {
    return kPaint;
  }
  if (entry_type == performance_entry_names::kEvent) {
    return kEvent;
  }
  if (entry_type == performance_entry_names::kFirstInput) {
    return kFirstInput;
  }
  if (entry_type == performance_entry_names::kElement) {
    return kElement;
  }
  if (entry_type == performance_entry_names::kLayoutShift) {
    return kLayoutShift;
  }
  if (entry_type == performance_entry_names::kLargestContentfulPaint) {
    return kLargestContentfulPaint;
  }
  if (entry_type == performance_entry_names::kInteractionContentfulPaint) {
    return kInteractionContentfulPaint;
  }
  if (entry_type == performance_entry_names::kVisibilityState) {
    return kVisibilityState;
  }
  if (entry_type == performance_entry_names::kBackForwardCacheRestoration) {
    return kBackForwardCacheRestoration;
  }
  if (entry_type == performance_entry_names::kSoftNavigation) {
    return kSoftNavigation;
  }
  if (entry_type == performance_entry_names::kLongAnimationFrame) {
    return kLongAnimationFrame;
  }
  if (entry_type == performance_entry_names::kContainer) {
    return kContainer;
  }
  if (entry_type == performance_entry_names::kScroll) {
    return kScroll;
  }
  return kInvalid;
}

const AtomicString& PerformanceEntry::ToEntryTypeString(EntryType entry_type) {
  switch (entry_type) {
    case kLongTask:
      return performance_entry_names::kLongtask;
    case kMark:
      return performance_entry_names::kMark;
    case kMeasure:
      return performance_entry_names::kMeasure;
    case kResource:
      return performance_entry_names::kResource;
    case kNavigation:
      return performance_entry_names::kNavigation;
    case kTaskAttribution:
      return performance_entry_names::kTaskattribution;
    case kPaint:
      return performance_entry_names::kPaint;
    case kEvent:
      return performance_entry_names::kEvent;
    case kFirstInput:
      return performance_entry_names::kFirstInput;
    case kElement:
      return performance_entry_names::kElement;
    case kLayoutShift:
      return performance_entry_names::kLayoutShift;
    case kLargestContentfulPaint:
      return performance_entry_names::kLargestContentfulPaint;
    case kInteractionContentfulPaint:
      return performance_entry_names::kInteractionContentfulPaint;
    case kVisibilityState:
      return performance_entry_names::kVisibilityState;
    case kBackForwardCacheRestoration:
      return performance_entry_names::kBackForwardCacheRestoration;
    case kSoftNavigation:
      return performance_entry_names::kSoftNavigation;
    case kLongAnimationFrame:
      return performance_entry_names::kLongAnimationFrame;
    case kContainer:
      return performance_entry_names::kContainer;
    case kScroll:
      return performance_entry_names::kScroll;
    case kScript:
    case kMarkConditional:
    case kMeasureConditional:
    case kInvalid:
      return g_empty_atom;
  }
}

base::span<const PerformanceEntry::EntryTypeUseCounters>
PerformanceEntry::GetUseCounters() {
  static constexpr EntryTypeUseCounters kUseCounters[] = {
      {kBackForwardCacheRestoration,
       WebFeature::kBackForwardCacheRestorationEntryEmitted,
       WebFeature::kBackForwardCacheRestorationExplicitlyRequested,
       WebFeature::kBackForwardCacheRestorationPotentiallyObserved,
       WebFeature::kBackForwardCacheRestorationObserved},
      {kLongAnimationFrame, WebFeature::kLongAnimationFrameEntryEmitted,
       WebFeature::kLongAnimationFrameExplicitlyRequested,
       WebFeature::kLongAnimationFramePotentiallyObserved,
       WebFeature::kLongAnimationFrameObserved},
      {kElement, WebFeature::kElementTimingEntryEmitted,
       WebFeature::kElementTimingExplicitlyRequested,
       WebFeature::kElementTimingPotentiallyObserved,
       WebFeature::kElementTimingObserved},
      {kContainer, WebFeature::kContainerTimingEntryEmitted,
       WebFeature::kContainerTimingExplicitlyRequested,
       WebFeature::kContainerTimingPotentiallyObserved,
       WebFeature::kContainerTimingObserved},
      {kEvent, WebFeature::kEventTimingEntryEmitted,
       WebFeature::kEventTimingExplicitlyRequested,
       WebFeature::kEventTimingPotentiallyObserved,
       WebFeature::kEventTimingObserved},
      {kFirstInput, WebFeature::kEventTimingFirstInputEntryEmitted,
       WebFeature::kEventTimingFirstInputExplicitlyRequested,
       WebFeature::kEventTimingFirstInputPotentiallyObserved,
       WebFeature::kEventTimingFirstInputObserved},
      {kSoftNavigation, WebFeature::kSoftNavigationEntryEmitted,
       WebFeature::kSoftNavigationExplicitlyRequested,
       WebFeature::kSoftNavigationPotentiallyObserved,
       WebFeature::kSoftNavigationObserved},
      {kInteractionContentfulPaint,
       WebFeature::kInteractionContentfulPaintEntryEmitted,
       WebFeature::kInteractionContentfulPaintExplicitlyRequested,
       WebFeature::kInteractionContentfulPaintPotentiallyObserved,
       WebFeature::kInteractionContentfulPaintObserved},
      {kLargestContentfulPaint, WebFeature::kLargestContentfulPaintEntryEmitted,
       WebFeature::kLargestContentfulPaintExplicitlyRequested,
       WebFeature::kLargestContentfulPaintPotentiallyObserved,
       WebFeature::kLargestContentfulPaintObserved},
      {kLayoutShift, WebFeature::kLayoutShiftEntryEmitted,
       WebFeature::kLayoutShiftExplicitlyRequested,
       WebFeature::kLayoutShiftPotentiallyObserved,
       WebFeature::kLayoutShiftObserved},
      {kLongTask, WebFeature::kLongTaskEntryEmitted,
       WebFeature::kLongTaskObserver, WebFeature::kLongTaskPotentiallyObserved,
       WebFeature::kLongTaskObserved},
      {kPaint, WebFeature::kPaintTimingEntryEmitted,
       WebFeature::kPaintTimingRequested,
       WebFeature::kPaintTimingPotentiallyObserved,
       WebFeature::kPaintTimingObserved},
      {kNavigation, WebFeature::kNavigationTimingEntryEmitted,
       WebFeature::kNavigationTimingL2,
       WebFeature::kNavigationTimingPotentiallyObserved,
       WebFeature::kNavigationTimingObserved},
      {kResource, WebFeature::kResourceTimingEntryEmitted,
       WebFeature::kResourceTiming,
       WebFeature::kResourceTimingPotentiallyObserved,
       WebFeature::kResourceTimingObserved},
      {kVisibilityState, WebFeature::kVisibilityStateEntryEmitted,
       WebFeature::kVisibilityStateObserver,
       WebFeature::kVisibilityStatePotentiallyObserved,
       WebFeature::kVisibilityStateObserved},
      {kMark, WebFeature::kUserTimingMarkEntryEmitted,
       WebFeature::kUserTimingMarkExplicitlyRequested,
       WebFeature::kUserTimingMarkPotentiallyObserved,
       WebFeature::kUserTimingMarkObserved},
      {kMeasure, WebFeature::kUserTimingMeasureEntryEmitted,
       WebFeature::kUserTimingMeasureExplicitlyRequested,
       WebFeature::kUserTimingMeasurePotentiallyObserved,
       WebFeature::kUserTimingMeasureObserved},
      {kScroll, WebFeature::kScrollTimingEntryEmitted,
       WebFeature::kScrollTimingExplicitlyRequested,
       WebFeature::kScrollTimingPotentiallyObserved,
       WebFeature::kScrollTimingObserved},
  };
  return kUseCounters;
}

const PerformanceEntry::EntryTypeUseCounters*
PerformanceEntry::GetUseCountersForType(EntryType entry_type) {
  // No `default:` case so that `-Wswitch` enforces at compile time that every
  // `EntryType` enumerator is explicitly handled here.
  switch (entry_type) {
    case kTaskAttribution:
    case kScript:
    case kMarkConditional:
    case kMeasureConditional:
    case kInvalid:
      return nullptr;
    case kBackForwardCacheRestoration:
    case kLongAnimationFrame:
    case kElement:
    case kContainer:
    case kEvent:
    case kFirstInput:
    case kSoftNavigation:
    case kInteractionContentfulPaint:
    case kLargestContentfulPaint:
    case kLayoutShift:
    case kLongTask:
    case kPaint:
    case kNavigation:
    case kResource:
    case kVisibilityState:
    case kMark:
    case kMeasure:
    case kScroll:
      for (const auto& counters : GetUseCounters()) {
        if (counters.entry_type == entry_type) {
          return &counters;
        }
      }
      NOTREACHED();
  }
}

DOMHighResTimeStamp PerformanceEntry::paintTime() const {
  return paint_timing_info_ ? paint_timing_info_->paint_time : 0;
}
std::optional<DOMHighResTimeStamp> PerformanceEntry::presentationTime() const {
  return paint_timing_info_ ? paint_timing_info_->presentation_time : 0;
}

void PerformanceEntry::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  ScriptWrappable::Trace(visitor);
}

ScriptObject PerformanceEntry::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  BuildJSONValue(result);
  return result.ToScriptObject();
}

void PerformanceEntry::BuildJSONValue(V8ObjectBuilder& builder) const {
  builder.AddString("name", name());
  builder.AddString("entryType", entryType());
  builder.AddNumber("startTime", startTime());
  builder.AddNumber("duration", duration());
  builder.AddNumber("navigationId", navigationId());

  if (paint_timing_info_) {
    builder.AddNumber("paintTime", paint_timing_info_->paint_time);
    builder.AddNumber("presentationTime",
                      paint_timing_info_->presentation_time);
  }
}

}  // namespace blink
