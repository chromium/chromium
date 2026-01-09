// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timeline_trigger_range_list.h"

#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

TimelineTriggerRangeList::TimelineTriggerRangeList(
    const HeapVector<Member<TimelineTriggerRange>>& ranges)
    : ranges_(ranges) {}

TimelineTriggerRangeList* TimelineTriggerRangeList::Create(
    ExecutionContext* execution_context,
    const HeapVector<Member<TimelineTriggerOptions>>& options_list,
    ExceptionState& exception_state) {
  HeapVector<Member<TimelineTriggerRange>> ranges;

  DCHECK_LE(options_list.size(), 1u);
  for (const Member<TimelineTriggerOptions>& options : options_list) {
    if (TimelineTriggerRange* range = TimelineTriggerRange::Create(
            execution_context, options, exception_state)) {
      ranges.push_back(range);
    }
  }

  return MakeGarbageCollected<TimelineTriggerRangeList>(ranges);
}

TimelineTriggerRange* TimelineTriggerRangeList::item(unsigned index) {
  if (index < ranges_.size()) {
    return ranges_[index];
  }
  return nullptr;
}

void TimelineTriggerRangeList::Trace(Visitor* visitor) const {
  visitor->Trace(ranges_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
