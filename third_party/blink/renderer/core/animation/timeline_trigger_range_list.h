// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_RANGE_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_RANGE_LIST_H_

#include "third_party/blink/renderer/core/animation/timeline_trigger_range.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

// This class represents the list of TimelineTriggerRange objects with which a
// TimelineTrigger can be configured. While it maintains a HeapVector of these
// configurations, we currently support at most one such configuration per
// TimelineTrigger: crbug.com/473568234.
class CORE_EXPORT TimelineTriggerRangeList : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit TimelineTriggerRangeList(
      const HeapVector<Member<TimelineTriggerRange>>& ranges);
  static TimelineTriggerRangeList* Create(
      ExecutionContext* execution_context,
      const HeapVector<Member<TimelineTriggerOptions>>& options,
      ExceptionState& exception_state);

  unsigned length() const { return ranges_.size(); }
  TimelineTriggerRange* item(unsigned);

  const HeapVector<Member<TimelineTriggerRange>>& Ranges() const {
    return ranges_;
  }

  void Trace(Visitor* visitor) const override;

 private:
  HeapVector<Member<TimelineTriggerRange>> ranges_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_TRIGGER_RANGE_LIST_H_
