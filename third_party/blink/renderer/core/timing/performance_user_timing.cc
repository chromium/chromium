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

#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_mark_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/core/timing/performance_measure.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

namespace {

bool IsTracingEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("blink.user_timing", &enabled);
  return enabled;
}

}  // namespace

UserTiming::UserTiming(Performance& performance) : performance_(&performance) {}

String UserTiming::GetSerializedDetail(const ScriptValue& detail) {
  String serialized_detail = "";
  if (ExecutionContext* execution_context =
          performance_->GetExecutionContext()) {
    v8::Isolate* isolate = execution_context->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    if (!(detail.IsEmpty() || detail.V8Value()->IsNullOrUndefined())) {
      v8::Local<v8::String> v8_string;
      if (v8::JSON::Stringify(context, detail.V8Value()).ToLocal(&v8_string)) {
        serialized_detail = ToCoreString(isolate, v8_string);
      }
    }
  }
  return serialized_detail;
}
void UserTiming::AddMarkToPerformanceTimeline(
    PerformanceMark& mark,
    PerformanceMarkOptions* mark_options) {
  InsertPerformanceEntry(marks_map_, marks_buffer_, mark);
  if (!IsTracingEnabled()) {
    return;
  }
  ScriptValue detail = mark_options && mark_options->hasDetail()
                           ? mark_options->detail()
                           : ScriptValue();
  String serialized_detail = GetSerializedDetail(detail);
  auto source_location = CaptureSourceLocation();

  const auto trace_event_details = [&](perfetto::EventContext ctx) {
    ctx.event()->set_name(mark.name().Utf8().c_str());
    ctx.AddDebugAnnotation("data", [&](perfetto::TracedValue trace_context) {
      auto dict = std::move(trace_context).WriteDictionary();
      dict.Add("startTime", mark.startTime());
      dict.Add("stackTrace", source_location);
      // Only set when performance_ is a WindowPerformance.
      // performance_->timing() returns null when performance_ is a
      // WorkerPerformance.
      if (serialized_detail.length()) {
        dict.Add("detail", serialized_detail);
      }
      if (performance_->timing()) {
        performance_->timing()->WriteInto(dict);
      }
    });
  };
  TRACE_EVENT_INSTANT("blink.user_timing", nullptr, mark.UnsafeTimeForTraces(),
                      trace_event_details);
}

void UserTiming::ClearMarks(const AtomicString& mark_name) {
  ClearPerformanceEntries(marks_map_, marks_buffer_, mark_name);
  if (IsTracingEnabled()) {
    TRACE_EVENT_INSTANT("blink.user_timing", "clearMarks", "name",
                        mark_name.Utf8().c_str());
  }
}

const PerformanceMark* UserTiming::FindExistingMark(
    const AtomicString& mark_name) {
  PerformanceEntryMap::const_iterator existing_marks =
      marks_map_.find(mark_name);
  if (existing_marks != marks_map_.end()) {
    PerformanceEntry* entry = existing_marks->value->back().Get();
    DCHECK(entry->entryType() == performance_entry_names::kMark);
    return static_cast<PerformanceMark*>(entry);
  }
  return nullptr;
}

double UserTiming::FindExistingMarkStartTime(const AtomicString& mark_name,
                                             ExceptionState& exception_state) {
  const PerformanceMark* mark = FindExistingMark(mark_name);
  if (mark) {
    return mark->startTime();
  }

  // Although there was no mark with the given name in UserTiming, we need to
  // support measuring with respect to |PerformanceTiming| attributes.
  if (!PerformanceTiming::IsAttributeName(mark_name)) {
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

  // Because we know |PerformanceTiming::IsAttributeName(mark_name)| is true
  // (from above), we know calling |GetNamedAttribute| won't fail.
  double value = static_cast<double>(timing->GetNamedAttribute(mark_name));
  if (!value) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "'" + mark_name +
                                          "' is empty: either the event hasn't "
                                          "happened yet, or it would provide "
                                          "cross-origin timing information.");
    return 0.0;
  }

  // Count the usage of PerformanceTiming attribute names in performance
  // measure. See crbug.com/1318445.
  blink::UseCounter::Count(performance_->GetExecutionContext(),
                           WebFeature::kPerformanceMeasureFindExistingName);

  return value - timing->navigationStart();
}

double UserTiming::GetTimeOrFindMarkTime(
    const AtomicString& measure_name,
    const V8UnionDoubleOrString* mark_or_time,
    ExceptionState& exception_state) {
  DCHECK(mark_or_time);

  switch (mark_or_time->GetContentType()) {
    case V8UnionDoubleOrString::ContentType::kDouble: {
      const double time = mark_or_time->GetAsDouble();
      if (time < 0.0) {
        exception_state.ThrowTypeError("'" + measure_name +
                                       "' cannot have a negative time stamp.");
      }
      return time;
    }
    case V8UnionDoubleOrString::ContentType::kString:
      return FindExistingMarkStartTime(
          AtomicString(mark_or_time->GetAsString()), exception_state);
  }

  NOTREACHED_IN_MIGRATION();
  return 0;
}

base::TimeTicks UserTiming::GetPerformanceMarkUnsafeTimeForTraces(
    double start_time,
    const V8UnionDoubleOrString* maybe_mark_name) {
  if (maybe_mark_name && maybe_mark_name->IsString()) {
    const PerformanceMark* mark =
        FindExistingMark(AtomicString(maybe_mark_name->GetAsString()));
    if (mark) {
      return mark->UnsafeTimeForTraces();
    }
  }
  return performance_->GetTimeOriginInternal() + base::Milliseconds(start_time);
}

PerformanceMeasure* UserTiming::Measure(ScriptState* script_state,
                                        const AtomicString& measure_name,
                                        const V8UnionDoubleOrString* start,
                                        const std::optional<double>& duration,
                                        const V8UnionDoubleOrString* end,
                                        const ScriptValue& detail,
                                        ExceptionState& exception_state,
                                        DOMWindow* source) {
  double start_time =
      start ? GetTimeOrFindMarkTime(measure_name, start, exception_state) : 0;
  if (exception_state.HadException())
    return nullptr;

  double end_time =
      end ? GetTimeOrFindMarkTime(measure_name, end, exception_state)
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

  if (IsTracingEnabled()) {
    base::TimeTicks unsafe_start_time =
        GetPerformanceMarkUnsafeTimeForTraces(start_time, start);
    base::TimeTicks unsafe_end_time =
        GetPerformanceMarkUnsafeTimeForTraces(end_time, end);
    unsigned hash = WTF::GetHash(measure_name);
    WTF::AddFloatToHash(hash, start_time);
    WTF::AddFloatToHash(hash, end_time);
    String serialized_detail = GetSerializedDetail(detail);
    auto source_location = CaptureSourceLocation();
    if (serialized_detail.length()) {
      TRACE_EVENT_BEGIN("blink.user_timing", nullptr, perfetto::Track(hash),
                        unsafe_start_time, "startTime", start_time,
                        "stackTrace", source_location, "detail",
                        serialized_detail, [&](perfetto::EventContext ctx) {
                          ctx.event()->set_name(measure_name.Utf8().c_str());
                        });
    } else {
      TRACE_EVENT_BEGIN("blink.user_timing", nullptr, perfetto::Track(hash),
                        unsafe_start_time, "startTime", start_time,
                        "stackTrace", source_location,
                        [&](perfetto::EventContext ctx) {
                          ctx.event()->set_name(measure_name.Utf8().c_str());
                        });
    }
    TRACE_EVENT_END("blink.user_timing", perfetto::Track(hash),
                    unsafe_end_time);
  }

  PerformanceMeasure* measure =
      PerformanceMeasure::Create(script_state, measure_name, start_time,
                                 end_time, detail, exception_state, source);
  if (!measure)
    return nullptr;
  InsertPerformanceEntry(measures_map_, measures_buffer_, *measure);
  return measure;
}

void UserTiming::ClearMeasures(const AtomicString& measure_name) {
  ClearPerformanceEntries(measures_map_, measures_buffer_, measure_name);
  if (IsTracingEnabled()) {
    TRACE_EVENT_INSTANT("blink.user_timing", "clearMeasures", "name",
                        measure_name.Utf8().c_str());
  }
}

PerformanceEntryVector UserTiming::GetMarks() const {
  return marks_buffer_;
}

PerformanceEntryVector UserTiming::GetMarks(const AtomicString& name) const {
  PerformanceEntryMap::const_iterator it = marks_map_.find(name);
  if (it != marks_map_.end()) {
    return *it->value;
  }
  return {};
}

PerformanceEntryVector UserTiming::GetMeasures() const {
  return measures_buffer_;
}

PerformanceEntryVector UserTiming::GetMeasures(const AtomicString& name) const {
  PerformanceEntryMap::const_iterator it = measures_map_.find(name);
  if (it != measures_map_.end()) {
    return *it->value;
  }
  return {};
}

void UserTiming::InsertPerformanceEntry(
    PerformanceEntryMap& performance_entry_map,
    PerformanceEntryVector& performance_entry_buffer,
    PerformanceEntry& entry) {
  performance_->InsertEntryIntoSortedBuffer(performance_entry_buffer, entry,
                                            Performance::kDoNotRecordSwaps);

  auto it = performance_entry_map.find(entry.name());
  if (it == performance_entry_map.end()) {
    PerformanceEntryVector* entries =
        MakeGarbageCollected<PerformanceEntryVector>();
    entries->push_back(&entry);
    performance_entry_map.Set(entry.name(), entries);
    return;
  }

  DCHECK(it->value);
  performance_->InsertEntryIntoSortedBuffer(*it->value.Get(), entry,
                                            Performance::kDoNotRecordSwaps);
}

void UserTiming::ClearPerformanceEntries(
    PerformanceEntryMap& performance_entry_map,
    PerformanceEntryVector& performance_entry_buffer,
    const AtomicString& name) {
  if (name.IsNull()) {
    performance_entry_map.clear();
    performance_entry_buffer.clear();
    return;
  }

  if (performance_entry_map.Contains(name)) {
    UseCounter::Count(performance_->GetExecutionContext(),
                      WebFeature::kClearPerformanceEntries);

    // Remove key/value pair from the map.
    performance_entry_map.erase(name);

    // In favor of quicker getEntries() calls, we tradeoff performance here to
    // linearly 'clear' entries in the vector.
    performance_entry_buffer.erase(
        std::remove_if(performance_entry_buffer.begin(),
                       performance_entry_buffer.end(),
                       [name](auto& entry) { return entry->name() == name; }),
        performance_entry_buffer.end());
  }
}

void UserTiming::Trace(Visitor* visitor) const {
  visitor->Trace(performance_);
  visitor->Trace(marks_map_);
  visitor->Trace(measures_map_);
  visitor->Trace(marks_buffer_);
  visitor->Trace(measures_buffer_);
}

}  // namespace blink
