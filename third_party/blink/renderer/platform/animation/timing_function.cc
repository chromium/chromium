// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/timing_function.h"

#include <algorithm>
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/animation/keyframe/timing_function.h"

namespace blink {

String LinearTimingFunction::ToString() const {
  if (linear_->IsTrivial()) {
    return "linear";
  }
  WTF::StringBuilder builder;
  builder.Append("linear(");
  for (wtf_size_t i = 0; i < linear_->Points().size(); ++i) {
    if (i != 0) {
      builder.Append(", ");
    }
    builder.Append(String::NumberToStringECMAScript(linear_->Point(i).output));
    builder.Append(" ");
    builder.Append(String::NumberToStringECMAScript(linear_->Point(i).input));
    builder.Append("%");
  }
  builder.Append(")");
  return builder.ReleaseString();
}

double LinearTimingFunction::Evaluate(
    double fraction,
    TimingFunction::LimitDirection limit_direction) const {
  return linear_->GetValue(fraction, limit_direction);
}

void LinearTimingFunction::Range(double* min_value, double* max_value) const {
  if (IsTrivial()) {
    return;
  }
  //
  //        (min_it) # *               (max_it) ^ *
  //                 | | *                      | |
  //  (min_value) @  | | |   (max_value) %      | |
  //                 * | | *                    | |
  // ________________|_|_|_|____________________|_|_
  // @ - min_value.
  // % - max_value.
  // # - min_it is first of points with same input (and input >= min_value).
  // ^ - max_it.
  // for min_comp we want the first of points in case of input equality.
  // (e.g. begin of range).
  const auto min_comp = [](double value, const auto& point) {
    return value <= point.input;
  };
  // for max_comp we want the last of points in case of input equality.
  // (e.g. end of range).
  const auto max_comp = [](double value, const auto& point) {
    return value < point.input;
  };
  auto min_it = std::upper_bound(Points().cbegin(), Points().cend(),
                                 100 * *min_value, min_comp);
  min_it = min_it == Points().cend() ? std::prev(min_it) : min_it;
  auto max_it = std::upper_bound(Points().cbegin(), Points().cend(),
                                 100 * *max_value, max_comp);
  const auto [min, max] = std::minmax_element(
      min_it, max_it,
      [](const auto& a, const auto& b) { return a.output < b.output; });
  double min_val = Evaluate(*min_value);
  double max_val = Evaluate(*max_value);
  *min_value = std::min({min_val, max_val, min->output});
  *max_value = std::max({min_val, max_val, max->output});
}

std::unique_ptr<gfx::TimingFunction> LinearTimingFunction::CloneToCC() const {
  return linear_->Clone();
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
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

String CubicBezierTimingFunction::ToString() const {
  switch (GetEaseType()) {
    case CubicBezierTimingFunction::EaseType::EASE:
      return "ease";
    case CubicBezierTimingFunction::EaseType::EASE_IN:
      return "ease-in";
    case CubicBezierTimingFunction::EaseType::EASE_OUT:
      return "ease-out";
    case CubicBezierTimingFunction::EaseType::EASE_IN_OUT:
      return "ease-in-out";
    case CubicBezierTimingFunction::EaseType::CUSTOM:
      return "cubic-bezier(" + String::NumberToStringECMAScript(X1()) + ", " +
             String::NumberToStringECMAScript(Y1()) + ", " +
             String::NumberToStringECMAScript(X2()) + ", " +
             String::NumberToStringECMAScript(Y2()) + ")";
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

double CubicBezierTimingFunction::Evaluate(
    double fraction,
    TimingFunction::LimitDirection limit_direction) const {
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

std::unique_ptr<gfx::TimingFunction> CubicBezierTimingFunction::CloneToCC()
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
  builder.Append(String::NumberToStringECMAScript(NumberOfSteps()));
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
  return steps_->GetValue(fraction, limit_direction);
}

std::unique_ptr<gfx::TimingFunction> StepsTimingFunction::CloneToCC() const {
  return steps_->Clone();
}

scoped_refptr<TimingFunction> CreateCompositorTimingFunctionFromCC(
    const gfx::TimingFunction* timing_function) {
  if (!timing_function)
    return LinearTimingFunction::Shared();

  switch (timing_function->GetType()) {
    case gfx::TimingFunction::Type::CUBIC_BEZIER: {
      auto* cubic_timing_function =
          static_cast<const gfx::CubicBezierTimingFunction*>(timing_function);
      if (cubic_timing_function->ease_type() !=
          gfx::CubicBezierTimingFunction::EaseType::CUSTOM)
        return CubicBezierTimingFunction::Preset(
            cubic_timing_function->ease_type());

      const auto& bezier = cubic_timing_function->bezier();
      return CubicBezierTimingFunction::Create(bezier.GetX1(), bezier.GetY1(),
                                               bezier.GetX2(), bezier.GetY2());
    }

    case gfx::TimingFunction::Type::STEPS: {
      auto* steps_timing_function =
          static_cast<const gfx::StepsTimingFunction*>(timing_function);
      return StepsTimingFunction::Create(
          steps_timing_function->steps(),
          steps_timing_function->step_position());
    }

    case gfx::TimingFunction::Type::LINEAR: {
      auto* linear_timing_function =
          static_cast<const gfx::LinearTimingFunction*>(timing_function);
      if (linear_timing_function->IsTrivial()) {
        return LinearTimingFunction::Shared();
      }
      return LinearTimingFunction::Create(linear_timing_function->Points());
    }

    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

// Equals operators
bool operator==(const LinearTimingFunction& lhs, const TimingFunction& rhs) {
  if (auto* rhs_func = DynamicTo<LinearTimingFunction>(rhs)) {
    return lhs == *rhs_func;
  }
  return false;
}

bool operator==(const CubicBezierTimingFunction& lhs,
                const TimingFunction& rhs) {
  if (rhs.GetType() != TimingFunction::Type::CUBIC_BEZIER)
    return false;

  const auto& ctf = To<CubicBezierTimingFunction>(rhs);
  if ((lhs.GetEaseType() == CubicBezierTimingFunction::EaseType::CUSTOM) &&
      (ctf.GetEaseType() == CubicBezierTimingFunction::EaseType::CUSTOM))
    return (lhs.X1() == ctf.X1()) && (lhs.Y1() == ctf.Y1()) &&
           (lhs.X2() == ctf.X2()) && (lhs.Y2() == ctf.Y2());

  return lhs.GetEaseType() == ctf.GetEaseType();
}

bool operator==(const StepsTimingFunction& lhs, const TimingFunction& rhs) {
  if (rhs.GetType() != TimingFunction::Type::STEPS)
    return false;

  const auto& stf = To<StepsTimingFunction>(rhs);
  return (lhs.NumberOfSteps() == stf.NumberOfSteps()) &&
         (lhs.GetStepPosition() == stf.GetStepPosition());
}

// The generic operator== *must* come after the
// non-generic operator== otherwise it will end up calling itself.
bool operator==(const TimingFunction& lhs, const TimingFunction& rhs) {
  switch (lhs.GetType()) {
    case TimingFunction::Type::LINEAR: {
      const auto& linear = To<LinearTimingFunction>(lhs);
      return (linear == rhs);
    }
    case TimingFunction::Type::CUBIC_BEZIER: {
      const auto& cubic = To<CubicBezierTimingFunction>(lhs);
      return (cubic == rhs);
    }
    case TimingFunction::Type::STEPS: {
      const auto& step = To<StepsTimingFunction>(lhs);
      return (step == rhs);
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return false;
}

// No need to define specific operator!= as they can all come via this function.
bool operator!=(const TimingFunction& lhs, const TimingFunction& rhs) {
  return !(lhs == rhs);
}

}  // namespace blink
