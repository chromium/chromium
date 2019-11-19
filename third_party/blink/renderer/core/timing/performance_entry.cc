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
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"

namespace blink {

namespace {
static base::AtomicSequenceNumber index_seq;
}

PerformanceEntry::PerformanceEntry(const AtomicString& name,
                                   double start_time,
                                   double finish_time)
    : duration_(finish_time - start_time),
      name_(name),
      start_time_(start_time),
      index_(index_seq.GetNext()) {}

PerformanceEntry::~PerformanceEntry() = default;

DOMHighResTimeStamp PerformanceEntry::startTime() const {
  return start_time_;
}

DOMHighResTimeStamp PerformanceEntry::duration() const {
  return duration_;
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
  if (entry_type == performance_entry_names::kLongtask)
    return kLongTask;
  if (entry_type == performance_entry_names::kMark)
    return kMark;
  if (entry_type == performance_entry_names::kMeasure)
    return kMeasure;
  if (entry_type == performance_entry_names::kResource)
    return kResource;
  if (entry_type == performance_entry_names::kNavigation)
    return kNavigation;
  if (entry_type == performance_entry_names::kTaskattribution)
    return kTaskAttribution;
  if (entry_type == performance_entry_names::kPaint)
    return kPaint;
  if (entry_type == performance_entry_names::kEvent)
    return kEvent;
  if (entry_type == performance_entry_names::kFirstInput)
    return kFirstInput;
  if (entry_type == performance_entry_names::kElement)
    return kElement;
  if (entry_type == performance_entry_names::kLayoutShift)
    return kLayoutShift;
  if (entry_type == performance_entry_names::kLargestContentfulPaint)
    return kLargestContentfulPaint;
  return kInvalid;
}

ScriptValue PerformanceEntry::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  BuildJSONValue(result);
  return result.GetScriptValue();
}

void PerformanceEntry::BuildJSONValue(V8ObjectBuilder& builder) const {
  builder.AddString("name", name());
  builder.AddString("entryType", entryType());
  builder.AddNumber("startTime", startTime());
  builder.AddNumber("duration", duration());
}

}  // namespace blink
