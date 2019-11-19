/*
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/timing/performance_user_timing.h"

#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/core/timing/performance_mark_options.h"
#include "third_party/blink/renderer/core/timing/performance_measure.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

namespace {

typedef uint64_t (PerformanceTiming::*NavigationTimingFunction)() const;
using RestrictedKeyMap = HashMap<AtomicString, NavigationTimingFunction>;

const RestrictedKeyMap& GetRestrictedKeyMap() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<RestrictedKeyMap>, map, ());
  if (!map.IsSet()) {
    *map = {
        {"navigationStart", &PerformanceTiming::navigationStart},
        {"unloadEventStart", &PerformanceTiming::unloadEventStart},
        {"unloadEventEnd", &PerformanceTiming::unloadEventEnd},
        {"redirectStart", &PerformanceTiming::redirectStart},
        {"redirectEnd", &PerformanceTiming::redirectEnd},
        {"fetchStart", &PerformanceTiming::fetchStart},
        {"domainLookupStart", &PerformanceTiming::domainLookupStart},
        {"domainLookupEnd", &PerformanceTiming::domainLookupEnd},
        {"connectStart", &PerformanceTiming::connectStart},
        {"connectEnd", &PerformanceTiming::connectEnd},
        {"secureConnectionStart", &PerformanceTiming::secureConnectionStart},
        {"requestStart", &PerformanceTiming::requestStart},
        {"responseStart", &PerformanceTiming::responseStart},
        {"responseEnd", &PerformanceTiming::responseEnd},
        {"domLoading", &PerformanceTiming::domLoading},
        {"domInteractive", &PerformanceTiming::domInteractive},
        {"domContentLoadedEventStart",
         &PerformanceTiming::domContentLoadedEventStart},
        {"domContentLoadedEventEnd",
         &PerformanceTiming::domContentLoadedEventEnd},
        {"domComplete", &PerformanceTiming::domComplete},
        {"loadEventStart", &PerformanceTiming::loadEventStart},
        {"loadEventEnd", &PerformanceTiming::loadEventEnd},
    };
  }
  return *map;
}

}  // namespace

UserTiming::UserTiming(Performance& performance) : performance_(&performance) {}

static void InsertPerformanceEntry(PerformanceEntryMap& performance_entry_map,
                                   PerformanceEntry& entry) {
  PerformanceEntryMap::iterator it = performance_entry_map.find(entry.name());
  if (it != performance_entry_map.end()) {
    it->value.push_back(&entry);
  } else {
    PerformanceEntryVector vector(1);
    vector[0] = Member<PerformanceEntry>(entry);
    performance_entry_map.Set(entry.name(), vector);
  }
}

static void ClearPeformanceEntries(PerformanceEntryMap& performance_entry_map,
                                   const AtomicString& name) {
  if (name.IsNull()) {
    performance_entry_map.clear();
    return;
  }

  if (performance_entry_map.Contains(name))
    performance_entry_map.erase(name);
}

PerformanceMark* UserTiming::CreatePerformanceMark(
    ScriptState* script_state,
    const AtomicString& mark_name,
    PerformanceMarkOptions* mark_options,
    ExceptionState& exception_state) {
  DOMHighResTimeStamp start = 0.0;
  if (mark_options && mark_options->hasStartTime()) {
    start = mark_options->startTime();
    if (start < 0.0) {
      exception_state.ThrowTypeError("'" + mark_name +
                                     "' cannot have a negative start time.");
      return nullptr;
    }
  } else {
    start = performance_->now();
  }

  ScriptValue detail = ScriptValue::CreateNull(script_state->GetIsolate());
  if (mark_options)
    detail = mark_options->detail();

  bool is_worker_global_scope =
      performance_->GetExecutionContext() &&
      performance_->GetExecutionContext()->IsWorkerGlobalScope();
  if (!is_worker_global_scope && GetRestrictedKeyMap().Contains(mark_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "'" + mark_name +
            "' is part of the PerformanceTiming interface, and "
            "cannot be used as a mark name.");
    return nullptr;
  }

  return PerformanceMark::Create(script_state, mark_name, start, detail,
                                 exception_state);
}

void UserTiming::AddMarkToPerformanceTimeline(PerformanceMark& mark) {
  if (performance_->timing()) {
    TRACE_EVENT_COPY_MARK1("blink.user_timing", mark.name().Utf8().c_str(),
                           "data",
                           performance_->timing()->GetNavigationTracingData());
  } else {
    TRACE_EVENT_COPY_MARK("blink.user_timing", mark.name().Utf8().c_str());
  }
  InsertPerformanceEntry(marks_map_, mark);
}

void UserTiming::ClearMarks(const AtomicString& mark_name) {
  ClearPeformanceEntries(marks_map_, mark_name);
}

double UserTiming::FindExistingMarkStartTime(const AtomicString& mark_name,
                                             ExceptionState& exception_state) {
  if (marks_map_.Contains(mark_name))
    return marks_map_.at(mark_name).back()->startTime();

  NavigationTimingFunction timing_function =
      GetRestrictedKeyMap().at(mark_name);
  if (!timing_function) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The mark '" + mark_name + "' does not exist.");
    return 0.0;
  }

  PerformanceTiming* timing = performance_->timing();
  if (!timing) {
    // According to
    // https://w3c.github.io/user-timing/#convert-a-name-to-a-timestamp.
    exception_state.ThrowTypeError(
        "When converting a mark name ('" + mark_name +
        "') to a timestamp given a name that is a read only attribute in the "
        "PerformanceTiming interface, the global object has to be a Window "
        "object.");
    return 0.0;
  }

  double value = static_cast<double>((timing->*timing_function)());
  if (!value) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "'" + mark_name +
                                          "' is empty: either the event hasn't "
                                          "happened yet, or it would provide "
                                          "cross-origin timing information.");
    return 0.0;
  }

  return value - timing->navigationStart();
}

double UserTiming::GetTimeOrFindMarkTime(const AtomicString& measure_name,
                                         const StringOrDouble& mark_or_time,
                                         ExceptionState& exception_state) {
  if (mark_or_time.IsString()) {
    return FindExistingMarkStartTime(AtomicString(mark_or_time.GetAsString()),
                                     exception_state);
  }
  DCHECK(mark_or_time.IsDouble());
  const double time = mark_or_time.GetAsDouble();
  if (time < 0.0) {
    exception_state.ThrowTypeError("'" + measure_name +
                                   "' cannot have a negative time stamp.");
  }
  return time;
}

PerformanceMeasure* UserTiming::Measure(ScriptState* script_state,
                                        const AtomicString& measure_name,
                                        const StringOrDouble& start,
                                        base::Optional<double> duration,
                                        const StringOrDouble& end,
                                        const ScriptValue& detail,
                                        ExceptionState& exception_state) {
  double start_time =
      start.IsNull()
          ? 0.0
          : GetTimeOrFindMarkTime(measure_name, start, exception_state);
  if (exception_state.HadException())
    return nullptr;

  double end_time =
      end.IsNull() ? performance_->now()
                   : GetTimeOrFindMarkTime(measure_name, end, exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (duration.has_value()) {
    // When |duration| is specified, we require that exactly one of |start| and
    // |end| were specified. Then, since |start| + |duration| = |end|, we'll
    // compute the missing boundary.
    if (start.IsNull()) {
      start_time = end_time - duration.value();
    } else {
      DCHECK(end.IsNull()) << "When duration is specified, one of 'start' or "
                              "'end' must be unspecified";
      end_time = start_time + duration.value();
    }
  }

  // User timing events are stored as integer milliseconds from the start of
  // navigation, whereas trace events accept double seconds based off of
  // CurrentTime::monotonicallyIncreasingTime().
  double start_time_monotonic =
      performance_->GetTimeOrigin() + start_time / 1000.0;
  double end_time_monotonic = performance_->GetTimeOrigin() + end_time / 1000.0;
  unsigned hash = WTF::StringHash::GetHash(measure_name);
  WTF::AddFloatToHash(hash, start_time);
  WTF::AddFloatToHash(hash, end_time);

  TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "blink.user_timing", measure_name.Utf8().c_str(), hash,
      trace_event::ToTraceTimestamp(start_time_monotonic));
  TRACE_EVENT_COPY_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "blink.user_timing", measure_name.Utf8().c_str(), hash,
      trace_event::ToTraceTimestamp(end_time_monotonic));

  PerformanceMeasure* measure =
      PerformanceMeasure::Create(script_state, measure_name, start_time,
                                 end_time, detail, exception_state);
  if (!measure)
    return nullptr;
  InsertPerformanceEntry(measures_map_, *measure);
  return measure;
}

void UserTiming::ClearMeasures(const AtomicString& measure_name) {
  ClearPeformanceEntries(measures_map_, measure_name);
}

static PerformanceEntryVector ConvertToEntrySequence(
    const PerformanceEntryMap& performance_entry_map) {
  PerformanceEntryVector entries;

  for (const auto& entry : performance_entry_map)
    entries.AppendVector(entry.value);

  return entries;
}

static PerformanceEntryVector GetEntrySequenceByName(
    const PerformanceEntryMap& performance_entry_map,
    const AtomicString& name) {
  PerformanceEntryVector entries;

  PerformanceEntryMap::const_iterator it = performance_entry_map.find(name);
  if (it != performance_entry_map.end())
    entries.AppendVector(it->value);

  return entries;
}

PerformanceEntryVector UserTiming::GetMarks() const {
  return ConvertToEntrySequence(marks_map_);
}

PerformanceEntryVector UserTiming::GetMarks(const AtomicString& name) const {
  return GetEntrySequenceByName(marks_map_, name);
}

PerformanceEntryVector UserTiming::GetMeasures() const {
  return ConvertToEntrySequence(measures_map_);
}

PerformanceEntryVector UserTiming::GetMeasures(const AtomicString& name) const {
  return GetEntrySequenceByName(measures_map_, name);
}

void UserTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(performance_);
  visitor->Trace(marks_map_);
  visitor->Trace(measures_map_);
}

}  // namespace blink
