// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/timing_function.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

String LinearTimingFunction::ToString() const {
  return "linear";
}

double LinearTimingFunction::Evaluate(double fraction) const {
  return fraction;
}

void LinearTimingFunction::Range(double* min_value, double* max_value) const {}

std::unique_ptr<cc::TimingFunction> LinearTimingFunction::CloneToCC() const {
  return nullptr;
}

CubicBezierTimingFunction* CubicBezierTimingFunction::Preset(
    EaseType ease_type) {
  DEFINE_STATIC_REF(
      CubicBezierTimingFunction, ease,
      (base::AdoptRef(new CubicBezierTimingFunction(EaseType::EASE))));
  DEFINE_STATIC_REF(
      CubicBezierTimingFunction, ease_in,
      (base::AdoptRef(new CubicBezierTimingFunction(EaseType::EASE_IN))));
  DEFINE_STATIC_REF(
      CubicBezierTimingFunction, ease_out,
      (base::AdoptRef(new CubicBezierTimingFunction(EaseType::EASE_OUT))));
  DEFINE_STATIC_REF(
      CubicBezierTimingFunction, ease_in_out,
      (base::AdoptRef(new CubicBezierTimingFunction(EaseType::EASE_IN_OUT))));

  switch (ease_type) {
    case EaseType::EASE:
      return ease;
    case EaseType::EASE_IN:
      return ease_in;
    case EaseType::EASE_OUT:
      return ease_out;
    case EaseType::EASE_IN_OUT:
      return ease_in_out;
    default:
      NOTREACHED();
      return nullptr;
  }
}

String CubicBezierTimingFunction::ToString() const {
  switch (this->GetEaseType()) {
    case CubicBezierTimingFunction::EaseType::EASE:
      return "ease";
    case CubicBezierTimingFunction::EaseType::EASE_IN:
      return "ease-in";
    case CubicBezierTimingFunction::EaseType::EASE_OUT:
      return "ease-out";
    case CubicBezierTimingFunction::EaseType::EASE_IN_OUT:
      return "ease-in-out";
    case CubicBezierTimingFunction::EaseType::CUSTOM:
      return "cubic-bezier(" + String::NumberToStringECMAScript(this->X1()) +
             ", " + String::NumberToStringECMAScript(this->Y1()) + ", " +
             String::NumberToStringECMAScript(this->X2()) + ", " +
             String::NumberToStringECMAScript(this->Y2()) + ")";
    default:
      NOTREACHED();
      return "";
  }
}

double CubicBezierTimingFunction::Evaluate(double fraction) const {
  return bezier_->bezier().Solve(fraction);
}

void CubicBezierTimingFunction::Range(double* min_value,
                                      double* max_value) const {
  const double solution1 = bezier_->bezier().range_min();
  const double solution2 = bezier_->bezier().range_max();

  // Since our input values can be out of the range 0->1 so we must also
  // consider the minimum and maximum points.
  double solution_min = bezier_->bezier().SolveWithEpsilon(
      *min_value, std::numeric_limits<double>::epsilon());
  double solution_max = bezier_->bezier().SolveWithEpsilon(
      *max_value, std::numeric_limits<double>::epsilon());
  *min_value = std::min(std::min(solution_min, solution_max), 0.0);
  *max_value = std::max(std::max(solution_min, solution_max), 1.0);
  *min_value = std::min(std::min(*min_value, solution1), solution2);
  *max_value = std::max(std::max(*max_value, solution1), solution2);
}

std::unique_ptr<cc::TimingFunction> CubicBezierTimingFunction::CloneToCC()
    const {
  return bezier_->Clone();
}

String StepsTimingFunction::ToString() const {
  const char* position_string = nullptr;
  switch (GetStepPosition()) {
    case StepPosition::START:
      position_string = "start";
      break;

    case StepPosition::END:
      // do not specify step position in output
      break;

    case StepPosition::JUMP_BOTH:
      position_string = "jump-both";
      break;

    case StepPosition::JUMP_END:
      // do not specify step position in output
      break;

    case StepPosition::JUMP_NONE:
      position_string = "jump-none";
      break;

    case StepPosition::JUMP_START:
      position_string = "jump-start";
      break;
  }

  StringBuilder builder;
  builder.Append("steps(");
  builder.Append(String::NumberToStringECMAScript(this->NumberOfSteps()));
  if (position_string) {
    builder.Append(", ");
    builder.Append(position_string);
  }
  builder.Append(')');
  return builder.ToString();
}

void StepsTimingFunction::Range(double* min_value, double* max_value) const {
  *min_value = 0;
  *max_value = 1;
}

double StepsTimingFunction::Evaluate(double fraction,
                                     LimitDirection limit_direction) const {
  return steps_->GetPreciseValue(fraction, limit_direction);
}

double StepsTimingFunction::Evaluate(double fraction) const {
  NOTREACHED() << "Use Evaluate(fraction, limit_direction) instead.";
  return steps_->GetPreciseValue(fraction, LimitDirection::RIGHT);
}

std::unique_ptr<cc::TimingFunction> StepsTimingFunction::CloneToCC() const {
  return steps_->Clone();
}

scoped_refptr<TimingFunction> CreateCompositorTimingFunctionFromCC(
    const cc::TimingFunction* timing_function) {
  if (!timing_function)
    return LinearTimingFunction::Shared();

  switch (timing_function->GetType()) {
    case cc::TimingFunction::Type::CUBIC_BEZIER: {
      auto* cubic_timing_function =
          static_cast<const cc::CubicBezierTimingFunction*>(timing_function);
      if (cubic_timing_function->ease_type() !=
          cc::CubicBezierTimingFunction::EaseType::CUSTOM)
        return CubicBezierTimingFunction::Preset(
            cubic_timing_function->ease_type());

      const auto& bezier = cubic_timing_function->bezier();
      return CubicBezierTimingFunction::Create(bezier.GetX1(), bezier.GetY1(),
                                               bezier.GetX2(), bezier.GetY2());
    }

    case cc::TimingFunction::Type::STEPS: {
      auto* steps_timing_function =
          static_cast<const cc::StepsTimingFunction*>(timing_function);
      return StepsTimingFunction::Create(
          steps_timing_function->steps(),
          steps_timing_function->step_position());
    }

    default:
      NOTREACHED();
      return nullptr;
  }
}

// Equals operators
bool operator==(const LinearTimingFunction& lhs, const TimingFunction& rhs) {
  return rhs.GetType() == TimingFunction::Type::LINEAR;
}

bool operator==(const CubicBezierTimingFunction& lhs,
                const TimingFunction& rhs) {
  if (rhs.GetType() != TimingFunction::Type::CUBIC_BEZIER)
    return false;

  const CubicBezierTimingFunction& ctf = ToCubicBezierTimingFunction(rhs);
  if ((lhs.GetEaseType() == CubicBezierTimingFunction::EaseType::CUSTOM) &&
      (ctf.GetEaseType() == CubicBezierTimingFunction::EaseType::CUSTOM))
    return (lhs.X1() == ctf.X1()) && (lhs.Y1() == ctf.Y1()) &&
           (lhs.X2() == ctf.X2()) && (lhs.Y2() == ctf.Y2());

  return lhs.GetEaseType() == ctf.GetEaseType();
}

bool operator==(const StepsTimingFunction& lhs, const TimingFunction& rhs) {
  if (rhs.GetType() != TimingFunction::Type::STEPS)
    return false;

  const StepsTimingFunction& stf = ToStepsTimingFunction(rhs);
  return (lhs.NumberOfSteps() == stf.NumberOfSteps()) &&
         (lhs.GetStepPosition() == stf.GetStepPosition());
}

// The generic operator== *must* come after the
// non-generic operator== otherwise it will end up calling itself.
bool operator==(const TimingFunction& lhs, const TimingFunction& rhs) {
  switch (lhs.GetType()) {
    case TimingFunction::Type::LINEAR: {
      const LinearTimingFunction& linear = ToLinearTimingFunction(lhs);
      return (linear == rhs);
    }
    case TimingFunction::Type::CUBIC_BEZIER: {
      const CubicBezierTimingFunction& cubic = ToCubicBezierTimingFunction(lhs);
      return (cubic == rhs);
    }
    case TimingFunction::Type::STEPS: {
      const StepsTimingFunction& step = ToStepsTimingFunction(lhs);
      return (step == rhs);
    }
    default:
      NOTREACHED();
  }
  return false;
}

// No need to define specific operator!= as they can all come via this function.
bool operator!=(const TimingFunction& lhs, const TimingFunction& rhs) {
  return !(lhs == rhs);
}

}  // namespace blink
