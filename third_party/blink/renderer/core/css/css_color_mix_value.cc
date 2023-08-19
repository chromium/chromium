// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

bool CSSColorMixValue::NormalizePercentages(
    const CSSPrimitiveValue* percentage1,
    const CSSPrimitiveValue* percentage2,
    double& mix_amount,
    double& alpha_multiplier) {
  double p1 = 0.5;
  double p2 = 0.5;
  if (percentage1 && !percentage2) {
    p1 = ClampTo<double>(percentage1->GetDoubleValue(), 0.0, 100.0) / 100.0;
    p2 = 1.0 - p1;
  } else if (percentage2 && !percentage1) {
    p2 = ClampTo<double>(percentage2->GetDoubleValue(), 0.0, 100.0) / 100.0;
    p1 = 1.0 - p2;
  } else if (percentage1 && percentage2) {
    p1 = ClampTo<double>(percentage1->GetDoubleValue(), 0.0, 100.0) / 100.0;
    p2 = ClampTo<double>(percentage2->GetDoubleValue(), 0.0, 100.0) / 100.0;
  }

  if (p1 == 0.0 && p2 == 0.0) {
    return false;
  }

  alpha_multiplier = 1.0;

  double scale = p1 + p2;
  if (scale != 0.0) {
    p1 /= scale;
    p2 /= scale;
    if (scale <= 1.0) {
      alpha_multiplier = scale;
    }
  }

  mix_amount = p2;
  if (p1 == 0.0) {
    mix_amount = 1.0;
  }

  return true;
}

bool CSSColorMixValue::Equals(const CSSColorMixValue& other) const {
  return color1_ == other.color1_ && color2_ == other.color2_ &&
         percentage1_ == other.percentage1_ &&
         percentage2_ == other.percentage2_ &&
         color_interpolation_space_ == other.color_interpolation_space_ &&
         hue_interpolation_method_ == other.hue_interpolation_method_;
}

// https://drafts.csswg.org/css-color-5/#serial-color-mix
String CSSColorMixValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("color-mix(in ");
  result.Append(Color::SerializeInterpolationSpace(color_interpolation_space_,
                                                   hue_interpolation_method_));
  result.Append(", ");
  result.Append(color1_->CssText());
  if (percentage1_ && percentage1_->GetDoubleValue() != 50.0) {
    result.Append(" ");
    result.Append(percentage1_->CssText());
  }
  if (!percentage1_ && percentage2_ && percentage2_->GetDoubleValue() != 50.0) {
    result.Append(" ");
    result.AppendNumber(100.0 - percentage2_->GetDoubleValue());
    result.Append("%");
  }
  result.Append(", ");
  result.Append(color2_->CssText());
  if (percentage1_ && percentage2_ &&
      percentage1_->GetDoubleValue() + percentage2_->GetDoubleValue() !=
          100.0 &&
      percentage2_->GetDoubleValue() != 50.0) {
    result.Append(" ");
    result.Append(percentage2_->CssText());
  }
  result.Append(")");

  return result.ReleaseString();
}

void CSSColorMixValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(color1_);
  visitor->Trace(color2_);
  visitor->Trace(percentage1_);
  visitor->Trace(percentage2_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
