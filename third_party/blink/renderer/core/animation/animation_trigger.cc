// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_trigger.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

using RangeBoundary = AnimationTrigger::RangeBoundary;

bool ValidateBoundary(ExecutionContext* execution_context,
                      const RangeBoundary* boundary,
                      ExceptionState& exception_state,
                      double default_percent) {
  if (boundary->IsString()) {
    CSSParserTokenStream stream(boundary->GetAsString());
    const CSSValue* value = css_parsing_utils::ConsumeAnimationRange(
        stream,
        *To<LocalDOMWindow>(execution_context)
             ->document()
             ->ElementSheet()
             .Contents()
             ->ParserContext(),
        /* default_offset_percent */ default_percent);
    if (!value || !stream.AtEnd()) {
      exception_state.ThrowTypeError(
          "AnimationTrigger range must be a name <length-percent> pair");
      return false;
    }
  } else {
    TimelineRangeOffset* value = boundary->GetAsTimelineRangeOffset();
    if (value->hasOffset()) {
      CSSNumericValue* offset = value->offset();
      const CSSPrimitiveValue* css_value =
          DynamicTo<CSSPrimitiveValue>(offset->ToCSSValue());

      if (!css_value) {
        exception_state.ThrowTypeError(
            "CSSNumericValue must be a length or percentage for animation "
            " trigger range.");
        return false;
      }
    }
  }
  return true;
}

AnimationTrigger::AnimationTrigger(AnimationTimeline* timeline,
                                   Type type,
                                   RangeBoundary* range_start,
                                   RangeBoundary* range_end,
                                   RangeBoundary* exit_range_start,
                                   RangeBoundary* exit_range_end)
    : timeline_(timeline),
      type_(type),
      range_start_(range_start),
      range_end_(range_end),
      exit_range_start_(exit_range_start),
      exit_range_end_(exit_range_end) {}

/* static */
AnimationTrigger* AnimationTrigger::Create(ExecutionContext* execution_context,
                                           AnimationTriggerOptions* options,
                                           ExceptionState& exception_state) {
  if (!ValidateBoundary(execution_context, options->rangeStart(),
                        exception_state, 0) ||
      !ValidateBoundary(execution_context, options->rangeEnd(), exception_state,
                        1) ||
      !ValidateBoundary(execution_context, options->exitRangeStart(),
                        exception_state, 0) ||
      !ValidateBoundary(execution_context, options->exitRangeEnd(),
                        exception_state, 1)) {
    return nullptr;
  }
  AnimationTimeline* timeline =
      (options->hasTimeline() ? options->timeline() : nullptr);
  if (!timeline) {
    timeline = &To<LocalDOMWindow>(execution_context)->document()->Timeline();
  }
  return MakeGarbageCollected<AnimationTrigger>(
      timeline, options->type(), options->rangeStart(), options->rangeEnd(),
      options->exitRangeStart(), options->exitRangeEnd());
}

const RangeBoundary* AnimationTrigger::rangeStart(
    ExecutionContext* execution_context) {
  return range_start_;
}
void AnimationTrigger::setRangeStart(ExecutionContext* execution_context,
                                     const RangeBoundary* boundary,
                                     ExceptionState& exception_state) {
  if (ValidateBoundary(execution_context, boundary, exception_state, 0)) {
    range_start_ = boundary;
  }
}
const RangeBoundary* AnimationTrigger::rangeEnd(
    ExecutionContext* execution_context) {
  return range_end_;
}
void AnimationTrigger::setRangeEnd(ExecutionContext* execution_context,
                                   const RangeBoundary* boundary,
                                   ExceptionState& exception_state) {
  if (ValidateBoundary(execution_context, boundary, exception_state, 1)) {
    range_end_ = boundary;
  }
}
const RangeBoundary* AnimationTrigger::exitRangeStart(
    ExecutionContext* execution_context) {
  return exit_range_start_;
}
void AnimationTrigger::setExitRangeStart(ExecutionContext* execution_context,
                                         const RangeBoundary* boundary,
                                         ExceptionState& exception_state) {
  if (ValidateBoundary(execution_context, boundary, exception_state, 0)) {
    exit_range_start_ = boundary;
  }
}
const RangeBoundary* AnimationTrigger::exitRangeEnd(
    ExecutionContext* execution_context) {
  return exit_range_end_;
}
void AnimationTrigger::setExitRangeEnd(ExecutionContext* execution_context,
                                       const RangeBoundary* boundary,
                                       ExceptionState& exception_state) {
  if (ValidateBoundary(execution_context, boundary, exception_state, 1)) {
    exit_range_end_ = boundary;
  }
}

}  // namespace blink
