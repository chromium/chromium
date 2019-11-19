/*
 * Copyright (C) 2007, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TIMING_FUNCTION_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TIMING_FUNCTION_VALUE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {
namespace cssvalue {

class CSSCubicBezierTimingFunctionValue : public CSSValue {
 public:
  CSSCubicBezierTimingFunctionValue(double x1, double y1, double x2, double y2)
      : CSSValue(kCubicBezierTimingFunctionClass),
        x1_(x1),
        y1_(y1),
        x2_(x2),
        y2_(y2) {}

  String CustomCSSText() const;

  double X1() const { return x1_; }
  double Y1() const { return y1_; }
  double X2() const { return x2_; }
  double Y2() const { return y2_; }

  bool Equals(const CSSCubicBezierTimingFunctionValue&) const;

  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValue::TraceAfterDispatch(visitor);
  }

 private:
  double x1_;
  double y1_;
  double x2_;
  double y2_;
};

class CSSStepsTimingFunctionValue : public CSSValue {
 public:
  static CSSStepsTimingFunctionValue* Create(
      int steps,
      StepsTimingFunction::StepPosition step_position) {
    return MakeGarbageCollected<CSSStepsTimingFunctionValue>(steps,
                                                             step_position);
  }

  CSSStepsTimingFunctionValue(int steps,
                              StepsTimingFunction::StepPosition step_position)
      : CSSValue(kStepsTimingFunctionClass),
        steps_(steps),
        step_position_(step_position) {}

  int NumberOfSteps() const { return steps_; }
  StepsTimingFunction::StepPosition GetStepPosition() const {
    return step_position_;
  }

  String CustomCSSText() const;

  bool Equals(const CSSStepsTimingFunctionValue&) const;

  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValue::TraceAfterDispatch(visitor);
  }

 private:
  int steps_;
  StepsTimingFunction::StepPosition step_position_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSCubicBezierTimingFunctionValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsCubicBezierTimingFunctionValue();
  }
};

template <>
struct DowncastTraits<cssvalue::CSSStepsTimingFunctionValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsStepsTimingFunctionValue();
  }
};

}  // namespace blink

#endif
