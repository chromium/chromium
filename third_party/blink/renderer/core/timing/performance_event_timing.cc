// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_event_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

// static
PerformanceEventTiming* PerformanceEventTiming::Create(
    const AtomicString& event_type,
    EventTimingReportingInfo reporting_info,
    bool cancelable,
    Node* target,
    DOMWindow* source) {
  // TODO(npm): enable this DCHECK once https://crbug.com/852846 is fixed.
  // DCHECK_LE(start_time, processing_start);
  DCHECK_LE(reporting_info.processing_start_time,
            reporting_info.processing_end_time);
  CHECK(source);
  return MakeGarbageCollected<PerformanceEventTiming>(
      event_type, performance_entry_names::kEvent, std::move(reporting_info),
      cancelable, target, source);
}

// static
PerformanceEventTiming* PerformanceEventTiming::CreateFirstInputTiming(
    PerformanceEventTiming* entry) {
  PerformanceEventTiming* first_input =
      MakeGarbageCollected<PerformanceEventTiming>(
          entry->name(), performance_entry_names::kFirstInput,
          *entry->GetEventTimingReportingInfo(), entry->cancelable(),
          entry->target(), entry->source());
  first_input->SetDuration(entry->duration());
  if (entry->HasKnownInteractionID()) {
    first_input->SetInteractionIdAndOffset(entry->interactionId(),
                                           entry->interactionOffset());
  }
  return first_input;
}

PerformanceEventTiming::PerformanceEventTiming(
    const AtomicString& event_type,
    const AtomicString& entry_type,
    EventTimingReportingInfo reporting_info,
    bool cancelable,
    Node* target,
    DOMWindow* source)
    : PerformanceEntry(
          event_type,
          DOMWindowPerformance::performance(*source->ToLocalDOMWindow())
              ->MonotonicTimeToDOMHighResTimeStamp(
                  reporting_info.creation_time),
          0.0,
          source),
      entry_type_(entry_type),
      processing_start_(
          DOMWindowPerformance::performance(*source->ToLocalDOMWindow())
              ->MonotonicTimeToDOMHighResTimeStamp(
                  reporting_info.processing_start_time)),
      processing_end_(
          DOMWindowPerformance::performance(*source->ToLocalDOMWindow())
              ->MonotonicTimeToDOMHighResTimeStamp(
                  reporting_info.processing_end_time)),
      cancelable_(cancelable),
      target_(target),
      reporting_info_(reporting_info) {}

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
  return interaction_id_.value_or(0);
}

void PerformanceEventTiming::SetInteractionId(uint32_t interaction_id) {
  interaction_id_ = interaction_id;
}

bool PerformanceEventTiming::HasKnownInteractionID() {
  return interaction_id_.has_value();
}

uint32_t PerformanceEventTiming::interactionOffset() const {
  return interaction_offset_;
}

void PerformanceEventTiming::SetInteractionIdAndOffset(
    uint32_t interaction_id,
    uint32_t interaction_offset) {
  interaction_id_ = interaction_id;
  interaction_offset_ = interaction_offset;
}

void PerformanceEventTiming::SetDuration(double duration) {
  // TODO(npm): enable this DCHECK once https://crbug.com/852846 is fixed.
  // DCHECK_LE(0, duration);
  duration_ = duration;
}

void PerformanceEventTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddInteger("interactionId", interactionId());
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
  // Recalculate this as the stored duration value is rounded.
  traced_value->SetDouble("duration", (reporting_info_.fallback_time.value_or(
                                           reporting_info_.presentation_time) -
                                       reporting_info_.creation_time)
                                          .InMillisecondsF());
  traced_value->SetBoolean("cancelable", cancelable());
  // If int overflows occurs, the static_cast may not work correctly.
  traced_value->SetInteger("interactionId", static_cast<int>(interactionId()));
  traced_value->SetInteger("interactionOffset",
                           static_cast<int>(interactionOffset()));
  traced_value->SetInteger(
      "nodeId", target_ ? target_->GetDomNodeId() : kInvalidDOMNodeId);
  traced_value->SetString("frame", GetFrameIdForTracing(frame));
  if (!source() || !source()->IsLocalDOMWindow()) {
    // Only report timing data if there is a valid source window to base the
    // origin time on.
    return traced_value;
  }
  base::TimeTicks origin_time =
      WindowPerformance::GetTimeOrigin(To<LocalDOMWindow>(source()));
  traced_value->SetDouble(
      "timeStamp",
      (reporting_info_.creation_time - origin_time).InMillisecondsF());
  traced_value->SetDouble(
      "processingStart",
      (reporting_info_.processing_start_time - origin_time).InMillisecondsF());
  traced_value->SetDouble(
      "processingEnd",
      (reporting_info_.processing_end_time - origin_time).InMillisecondsF());
  traced_value->SetDouble(
      "enqueuedToMainThreadTime",
      (reporting_info_.enqueued_to_main_thread_time - origin_time)
          .InMillisecondsF());
  traced_value->SetDouble(
      "commitFinishTime",
      (reporting_info_.commit_finish_time - origin_time).InMillisecondsF());
  return traced_value;
}

}  // namespace blink
