/*
 * Copyright (C) 2007 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_timing_function_value.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace cssvalue {

String CSSCubicBezierTimingFunctionValue::CustomCSSText() const {
  return "cubic-bezier(" + String::Number(x1_) + ", " + String::Number(y1_) +
         ", " + String::Number(x2_) + ", " + String::Number(y2_) + ")";
}

bool CSSCubicBezierTimingFunctionValue::Equals(
    const CSSCubicBezierTimingFunctionValue& other) const {
  return x1_ == other.x1_ && x2_ == other.x2_ && y1_ == other.y1_ &&
         y2_ == other.y2_;
}

String CSSStepsTimingFunctionValue::CustomCSSText() const {
  String step_position_string;
  switch (step_position_) {
    case StepsTimingFunction::StepPosition::START:
      step_position_string = "start";
      break;

    case StepsTimingFunction::StepPosition::END:
      step_position_string = "";
      break;

    case StepsTimingFunction::StepPosition::JUMP_BOTH:
      step_position_string = "jump-both";
      break;

    case StepsTimingFunction::StepPosition::JUMP_END:
      step_position_string = "";
      break;

    case StepsTimingFunction::StepPosition::JUMP_NONE:
      step_position_string = "jump-none";
      break;

    case StepsTimingFunction::StepPosition::JUMP_START:
      step_position_string = "jump-start";
  }

  // https://drafts.csswg.org/css-easing-1/#serialization
  // If the step position is jump-end or end, serialize as steps(<integer>).
  // Otherwise, serialize as steps(<integer>, <step-position>).
  if (step_position_string.IsEmpty())
    return "steps(" + String::Number(steps_) + ')';

  return "steps(" + String::Number(steps_) + ", " + step_position_string + ')';
}

bool CSSStepsTimingFunctionValue::Equals(
    const CSSStepsTimingFunctionValue& other) const {
  return steps_ == other.steps_ && step_position_ == other.step_position_;
}

}  // namespace cssvalue
}  // namespace blink
