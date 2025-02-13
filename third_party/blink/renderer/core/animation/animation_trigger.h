// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_timelinerangeoffset.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class AnimationTrigger : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using RangeBoundary = V8UnionStringOrTimelineRangeOffset;
  using Type = V8AnimationTriggerType;

  AnimationTrigger(AnimationTimeline* timeline,
                   Type type,
                   RangeBoundary* range_start,
                   RangeBoundary* range_end,
                   RangeBoundary* exit_range_start,
                   RangeBoundary* exit_range_end);
  static AnimationTrigger* Create(ExecutionContext* execution_context,
                                  AnimationTriggerOptions* options,
                                  ExceptionState& exception_state);

  Type type() { return type_; }
  void setType(Type type) { type_ = type; }

  AnimationTimeline* timeline() { return timeline_.Get(); }
  AnimationTimeline* setTimeline(AnimationTimeline* timeline) {
    return timeline_ = timeline;
  }

  const RangeBoundary* rangeStart(ExecutionContext* execution_context);
  void setRangeStart(ExecutionContext* execution_context,
                     const RangeBoundary* boundary,
                     ExceptionState& exception_state);
  const RangeBoundary* rangeEnd(ExecutionContext* execution_context);
  void setRangeEnd(ExecutionContext* execution_contextconst,
                   const RangeBoundary* boundary,
                   ExceptionState& exception_state);
  const RangeBoundary* exitRangeStart(ExecutionContext* execution_context);
  void setExitRangeStart(ExecutionContext* execution_context,
                         const RangeBoundary* boundary,
                         ExceptionState& exception_state);
  const RangeBoundary* exitRangeEnd(ExecutionContext* execution_context);
  void setExitRangeEnd(ExecutionContext* execution_context,
                       const RangeBoundary* boundary,
                       ExceptionState& exception_state);

  void Trace(Visitor* visitor) const override {
    visitor->Trace(timeline_);
    visitor->Trace(animation_);
    visitor->Trace(range_start_);
    visitor->Trace(range_end_);
    visitor->Trace(exit_range_start_);
    visitor->Trace(exit_range_end_);
    ScriptWrappable::Trace(visitor);
  }

 private:
  Member<AnimationTimeline> timeline_;
  Member<Animation> animation_;
  Type type_;
  Member<const RangeBoundary> range_start_;
  Member<const RangeBoundary> range_end_;
  Member<const RangeBoundary> exit_range_start_;
  Member<const RangeBoundary> exit_range_end_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TRIGGER_H_
