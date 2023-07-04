// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_LONG_ANIMATION_FRAME_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_LONG_ANIMATION_FRAME_TIMING_H_

#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_script_timing.h"

namespace blink {

using PerformanceScriptVector = HeapVector<Member<PerformanceScriptTiming>>;

class PerformanceLongAnimationFrameTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // This constructor uses int for |duration| to coarsen it in advance.
  // LongAnimationFrameTiming is always at 1-ms granularity.
  PerformanceLongAnimationFrameTiming(AnimationFrameTimingInfo* info,
                                      base::TimeTicks time_origin,
                                      bool cross_origin_isolated_capability,
                                      DOMWindow* source);
  ~PerformanceLongAnimationFrameTiming() override;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  DOMHighResTimeStamp renderStart() const;
  DOMHighResTimeStamp desiredRenderStart() const;
  DOMHighResTimeStamp styleAndLayoutStart() const;
  DOMHighResTimeStamp firstUIEventTimestamp() const;
  DOMHighResTimeStamp blockingDuration() const;

  const PerformanceScriptVector& scripts() const;

  void Trace(Visitor*) const override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;
  DOMHighResTimeStamp ToMonotonicTime(base::TimeTicks) const;
  base::TimeTicks time_origin_;
  bool cross_origin_isolated_capability_;
  Member<AnimationFrameTimingInfo> info_;
  mutable PerformanceScriptVector scripts_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_LONG_ANIMATION_FRAME_TIMING_H_
