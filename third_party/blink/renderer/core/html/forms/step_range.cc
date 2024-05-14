/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/forms/step_range.h"

#include <float.h>
#include "base/notreached.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

StepRange::StepRange()
    : maximum_(100),
      minimum_(0),
      step_(1),
      step_base_(0),
      has_step_(false),
      has_range_limitations_(false),
      supports_reversed_range_(false) {}

StepRange::StepRange(const StepRange& step_range) = default;

StepRange::StepRange(const Decimal& step_base,
                     const Decimal& minimum,
                     const Decimal& maximum,
                     bool has_range_limitations,
                     bool supports_reversed_range,
                     const Decimal& step,
                     const StepDescription& step_description)
    : maximum_(maximum),
      minimum_(minimum),
      step_(step.IsFinite() ? step : 1),
      step_base_(step_base.IsFinite() ? step_base : 1),
      step_description_(step_description),
      has_step_(step.IsFinite()),
      has_range_limitations_(has_range_limitations),
      supports_reversed_range_(supports_reversed_range) {
  DCHECK(maximum_.IsFinite());
  DCHECK(minimum_.IsFinite());
  DCHECK(step_.IsFinite());
  DCHECK(step_base_.IsFinite());
}

Decimal StepRange::AcceptableError() const {
  // FIXME: We should use DBL_MANT_DIG instead of FLT_MANT_DIG regarding to
  // HTML5 specification.
  DEFINE_STATIC_LOCAL(const Decimal, two_power_of_float_mantissa_bits,
                      (Decimal::kPositive, 0, UINT64_C(1) << FLT_MANT_DIG));
  return step_description_.step_value_should_be == kStepValueShouldBeReal
             ? step_ / two_power_of_float_mantissa_bits
             : Decimal(0);
}

Decimal StepRange::AlignValueForStep(const Decimal& current_value,
                                     const Decimal& new_value) const {
  DEFINE_STATIC_LOCAL(const Decimal, ten_power_of21,
                      (Decimal::kPositive, 21, 1));
  if (new_value >= ten_power_of21)
    return new_value;

  return StepMismatch(current_value) ? new_value
                                     : RoundByStep(new_value, step_base_);
}

Decimal StepRange::ClampValue(const Decimal& value) const {
  const Decimal in_range_value = std::max(minimum_, std::min(value, maximum_));
  if (!has_step_)
    return in_range_value;
  // Rounds inRangeValue to stepBase + N * step.
  const Decimal rounded_value = RoundByStep(in_range_value, step_base_);
  const Decimal clamped_value =
      rounded_value > maximum_
          ? rounded_value - step_
          : (rounded_value < minimum_ ? rounded_value + step_ : rounded_value);
  // clamped_value can be outside of [minimum_, maximum_] if step_ is huge.
  if (clamped_value < minimum_ || clamped_value > maximum_)
    return in_range_value;
  return clamped_value;
}

Decimal StepRange::ParseStep(AnyStepHandling any_step_handling,
                             const StepDescription& step_description,
                             const String& step_string) {
  if (step_string.empty())
    return step_description.DefaultValue();

  if (EqualIgnoringASCIICase(step_string, "any")) {
    switch (any_step_handling) {
      case kRejectAny:
        return Decimal::Nan();
      case kAnyIsDefaultStep:
        return step_description.DefaultValue();
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  Decimal step = ParseToDecimalForNumberType(step_string);
  if (!step.IsFinite() || step <= 0)
    return step_description.DefaultValue();

  switch (step_description.step_value_should_be) {
    case kStepValueShouldBeReal:
      step *= step_description.step_scale_factor;
      break;
    case kParsedStepValueShouldBeInteger:
      // For date, month, and week, the parsed value should be an integer for
      // some types.
      step = std::max(step.Round(), Decimal(1));
      step *= step_description.step_scale_factor;
      break;
    case kScaledStepValueShouldBeInteger:
      // For datetime, datetime-local, time, the result should be an integer.
      step *= step_description.step_scale_factor;
      step = std::max(step.Round(), Decimal(1));
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  DCHECK_GT(step, 0);
  return step;
}

Decimal StepRange::RoundByStep(const Decimal& value,
                               const Decimal& base) const {
  return base + ((value - base) / step_).Round() * step_;
}

bool StepRange::StepMismatch(const Decimal& value_for_check) const {
  if (!has_step_)
    return false;
  if (!value_for_check.IsFinite())
    return false;
  const Decimal value = (value_for_check - step_base_).Abs();
  if (!value.IsFinite())
    return false;
  // Decimal's fractional part size is DBL_MAN_DIG-bit. If the current value
  // is greater than step*2^DBL_MANT_DIG, the following computation for
  // remainder makes no sense.
  DEFINE_STATIC_LOCAL(const Decimal, two_power_of_double_mantissa_bits,
                      (Decimal::kPositive, 0, UINT64_C(1) << DBL_MANT_DIG));
  if (value / two_power_of_double_mantissa_bits > step_)
    return false;
  // The computation follows HTML5 4.10.7.2.10 `The step attribute' :
  // ... that number subtracted from the step base is not an integral multiple
  // of the allowed value step, the element is suffering from a step mismatch.
  const Decimal remainder = (value - step_ * (value / step_).Round()).Abs();
  // Accepts errors in lower fractional part which IEEE 754 single-precision
  // can't represent.
  const Decimal computed_acceptable_error = AcceptableError();
  return computed_acceptable_error < remainder &&
         remainder < (step_ - computed_acceptable_error);
}

Decimal StepRange::StepSnappedMaximum() const {
  Decimal base = StepBase();
  Decimal step = Step();
  if (step < Decimal(0))
    return Decimal::Nan();
  if (base - step == base || !(base / step).IsFinite())
    return Decimal::Nan();
  Decimal divided = ((Maximum() - base) / step);
  Decimal aligned_maximum;
  if (divided == divided.Floor())
    aligned_maximum = Maximum();
  else
    aligned_maximum = base + divided.Floor() * step;
  if (aligned_maximum > Maximum())
    aligned_maximum -= step;
  DCHECK_LE(aligned_maximum, Maximum());
  if (aligned_maximum < Minimum())
    return Decimal::Nan();
  return aligned_maximum;
}

// https://html.spec.whatwg.org/C/#has-a-reversed-range
bool StepRange::HasReversedRange() const {
  return supports_reversed_range_ && Maximum() < Minimum();
}

}  // namespace blink
