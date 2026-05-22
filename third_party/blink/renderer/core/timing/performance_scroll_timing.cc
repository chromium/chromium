// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_scroll_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

PerformanceScrollTiming::PerformanceScrollTiming(
    DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp duration,
    DOMHighResTimeStamp first_frame_time,
    int delta_x,
    int delta_y,
    const AtomicString& scroll_source,
    unsigned frames_expected,
    unsigned frames_produced,
    DOMHighResTimeStamp checkerboard_time,
    Node* target,
    DOMWindow* source,
    uint64_t navigation_id)
    : PerformanceEntry(duration,
                       performance_entry_names::kScroll,
                       start_time,
                       source,
                       navigation_id),
      target_(target),
      first_frame_time_(first_frame_time),
      delta_x_(delta_x),
      delta_y_(delta_y),
      scroll_source_(scroll_source),
      frames_expected_(frames_expected),
      frames_produced_(frames_produced),
      checkerboard_time_(checkerboard_time) {}

PerformanceScrollTiming::~PerformanceScrollTiming() = default;

const AtomicString& PerformanceScrollTiming::entryType() const {
  return performance_entry_names::kScroll;
}

PerformanceEntryType PerformanceScrollTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kScroll;
}

Node* PerformanceScrollTiming::target() const {
  return Performance::CanExposeNode(target_) ? target_ : nullptr;
}

void PerformanceScrollTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddNumber("firstFrameTime", first_frame_time_);
  builder.AddInteger("deltaX", delta_x_);
  builder.AddInteger("deltaY", delta_y_);
  builder.AddString("scrollSource", scroll_source_);
  builder.AddNumber("framesExpected", frames_expected_);
  builder.AddNumber("framesProduced", frames_produced_);
  builder.AddNumber("checkerboardTime", checkerboard_time_);
}

void PerformanceScrollTiming::Trace(Visitor* visitor) const {
  visitor->Trace(target_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
