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

#include "third_party/blink/renderer/bindings/core/v8/v8_performance_mark_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/core/timing/performance_measure.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

namespace {

void InsertPerformanceEntry(PerformanceEntryMap& performance_entry_map,
                            PerformanceEntry& entry) {
  PerformanceEntryMap::iterator it = performance_entry_map.find(entry.name());
  if (it != performance_entry_map.end()) {
    DCHECK(it->value);
    it->value->push_back(&entry);
  } else {
    PerformanceEntryVector* vector =
        MakeGarbageCollected<PerformanceEntryVector>();
    vector->push_back(&entry);
    performance_entry_map.Set(entry.name(), vector);
  }
}

void ClearPeformanceEntries(PerformanceEntryMap& performance_entry_map,
                            const AtomicString& name) {
  if (name.IsNull()) {
    performance_entry_map.clear();
    return;
  }

  if (performance_entry_map.Contains(name))
    performance_entry_map.erase(name);
}

}  // namespace

UserTiming::UserTiming(Performance& performance) : performance_(&performance) {}

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
  PerformanceEntryMap::const_iterator existing_marks =
      marks_map_.find(mark_name);
  if (existing_marks != marks_map_.end()) {
    return existing_marks->value->back()->startTime();
  }

  PerformanceTiming::PerformanceTimingGetter timing_function =
      PerformanceTiming::GetAttributeMapping().at(mark_name);
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

PerformanceMeasure* UserTiming::Measure(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const base::Optional<StringOrDouble>& start,
    const base::Optional<double>& duration,
    const base::Optional<StringOrDouble>& end,
    const ScriptValue& detail,
    ExceptionState& exception_state) {
  double start_time =
      start.has_value()
          ? GetTimeOrFindMarkTime(measure_name, start.value(), exception_state)
          : 0;
  if (exception_state.HadException())
    return nullptr;

  double end_time =
      end.has_value()
          ? GetTimeOrFindMarkTime(measure_name, end.value(), exception_state)
          : performance_->now();
  if (exception_state.HadException())
    return nullptr;

  if (duration.has_value()) {
    // When |duration| is specified, we require that exactly one of |start| and
    // |end| were specified. Then, since |start| + |duration| = |end|, we'll
    // compute the missing boundary.
    if (!start) {
      start_time = end_time - duration.value();
    } else {
      DCHECK(!end) << "When duration is specified, one of 'start' or "
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
    entries.AppendVector(*entry.value);

  return entries;
}

static PerformanceEntryVector GetEntrySequenceByName(
    const PerformanceEntryMap& performance_entry_map,
    const AtomicString& name) {
  PerformanceEntryVector entries;

  PerformanceEntryMap::const_iterator it = performance_entry_map.find(name);
  if (it != performance_entry_map.end())
    entries.AppendVector(*it->value);

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

void UserTiming::Trace(Visitor* visitor) const {
  visitor->Trace(performance_);
  visitor->Trace(marks_map_);
  visitor->Trace(measures_map_);
}

}  // namespace blink
