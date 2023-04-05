// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_event_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

// static
PerformanceEventTiming* PerformanceEventTiming::Create(
    const AtomicString& event_type,
    DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp processing_start,
    DOMHighResTimeStamp processing_end,
    bool cancelable,
    Node* target,
    DOMWindow* source) {
  // TODO(npm): enable this DCHECK once https://crbug.com/852846 is fixed.
  // DCHECK_LE(start_time, processing_start);
  DCHECK_LE(processing_start, processing_end);
  return MakeGarbageCollected<PerformanceEventTiming>(
      event_type, performance_entry_names::kEvent, start_time, processing_start,
      processing_end, cancelable, target, source);
}

// static
PerformanceEventTiming* PerformanceEventTiming::CreateFirstInputTiming(
    PerformanceEventTiming* entry) {
  PerformanceEventTiming* first_input =
      MakeGarbageCollected<PerformanceEventTiming>(
          entry->name(), performance_entry_names::kFirstInput,
          entry->startTime(), entry->processingStart(), entry->processingEnd(),
          entry->cancelable(), entry->target(), entry->source());
  first_input->SetDuration(entry->duration());
  return first_input;
}

PerformanceEventTiming::PerformanceEventTiming(
    const AtomicString& event_type,
    const AtomicString& entry_type,
    DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp processing_start,
    DOMHighResTimeStamp processing_end,
    bool cancelable,
    Node* target,
    DOMWindow* source)
    : PerformanceEntry(event_type, start_time, 0.0, source),
      entry_type_(entry_type),
      processing_start_(processing_start),
      processing_end_(processing_end),
      cancelable_(cancelable),
      target_(target) {}

PerformanceEventTiming::~PerformanceEventTiming() = default;

PerformanceEntryType PerformanceEventTiming::EntryTypeEnum() const {
  return entry_type_ == performance_entry_names::kEvent
             ? PerformanceEntry::EntryType::kEvent
             : PerformanceEntry::EntryType::kFirstInput;
}

DOMHighResTimeStamp PerformanceEventTiming::processingStart() const {
  return processing_start_;
}

DOMHighResTimeStamp PerformanceEventTiming::processingEnd() const {
  return processing_end_;
}

Node* PerformanceEventTiming::target() const {
  return Performance::CanExposeNode(target_) ? target_ : nullptr;
}

uint32_t PerformanceEventTiming::interactionId() const {
  return interaction_id_;
}

void PerformanceEventTiming::SetInteractionId(uint32_t interaction_id) {
  interaction_id_ = interaction_id;
}

base::TimeTicks PerformanceEventTiming::unsafePresentationTimestamp() const {
  return unsafe_presentation_timestamp_;
}

void PerformanceEventTiming::SetUnsafePresentationTimestamp(
    base::TimeTicks presentation_timestamp) {
  unsafe_presentation_timestamp_ = presentation_timestamp;
}

void PerformanceEventTiming::SetDuration(double duration) {
  // TODO(npm): enable this DCHECK once https://crbug.com/852846 is fixed.
  // DCHECK_LE(0, duration);
  duration_ = duration;
}

void PerformanceEventTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddNumber("processingStart", processingStart());
  builder.AddNumber("processingEnd", processingEnd());
  builder.AddBoolean("cancelable", cancelable_);
}

void PerformanceEventTiming::Trace(Visitor* visitor) const {
  PerformanceEntry::Trace(visitor);
  visitor->Trace(target_);
}

std::unique_ptr<TracedValue> PerformanceEventTiming::ToTracedValue(
    Frame* frame) const {
  auto traced_value = std::make_unique<TracedValue>();
  traced_value->SetString("type", name());
  traced_value->SetInteger("timeStamp", startTime());
  traced_value->SetInteger("processingStart", processingStart());
  traced_value->SetInteger("processingEnd", processingEnd());
  traced_value->SetInteger("duration", duration());
  traced_value->SetBoolean("cancelable", cancelable());
  // If int overflows occurs, the static_cast may not work correctly.
  traced_value->SetInteger("interactionId", static_cast<int>(interactionId()));
  traced_value->SetInteger("nodeId", DOMNodeIds::IdForNode(target_));
  traced_value->SetString("frame",
                          String::FromUTF8(GetFrameIdForTracing(frame)));
  return traced_value;
}

}  // namespace blink
