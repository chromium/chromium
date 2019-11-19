// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_LONG_TASK_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_LONG_TASK_TIMING_H_

#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class TaskAttributionTiming;
using TaskAttributionVector = HeapVector<Member<TaskAttributionTiming>>;

class PerformanceLongTaskTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PerformanceLongTaskTiming(double start_time,
                            double end_time,
                            const AtomicString& name,
                            const String& frame_src,
                            const String& frame_id,
                            const String& frame_name);

  AtomicString entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  TaskAttributionVector attribution() const;

  void Trace(blink::Visitor*) override;

 private:
  ~PerformanceLongTaskTiming() override;

  void BuildJSONValue(V8ObjectBuilder&) const override;

  TaskAttributionVector attribution_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_LONG_TASK_TIMING_H_
