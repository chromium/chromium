// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_relative_color_value.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink::cssvalue {

CSSRelativeColorValue::CSSRelativeColorValue(
    const CSSValue& origin_color,
    Color::ColorSpace color_interpolation_space,
    const CSSValue& channel0,
    const CSSValue& channel1,
    const CSSValue& channel2,
    const CSSValue* alpha)
    : CSSValue(kRelativeColorClass),
      origin_color_(origin_color),
      color_interpolation_space_(color_interpolation_space),
      channel0_(channel0),
      channel1_(channel1),
      channel2_(channel2),
      alpha_(alpha) {}

String CSSRelativeColorValue::CustomCSSText() const {
  // https://drafts.csswg.org/css-color-5/#serial-relative-color
  StringBuilder result;
  const bool serialize_as_color_function =
      Color::IsPredefinedColorSpace(color_interpolation_space_);
  if (serialize_as_color_function) {
    result.Append("color");
  } else {
    result.Append(Color::ColorSpaceToString(color_interpolation_space_));
  }
  result.Append("(from ");
  result.Append(origin_color_->CssText());
  result.Append(" ");
  if (serialize_as_color_function) {
    result.Append(Color::ColorSpaceToString(color_interpolation_space_));
    result.Append(" ");
  }
  result.Append(channel0_->CssText());
  result.Append(" ");
  result.Append(channel1_->CssText());
  result.Append(" ");
  result.Append(channel2_->CssText());
  if (alpha_ != nullptr) {
    result.Append(" / ");
    result.Append(alpha_->CssText());
  }
  result.Append(")");
  return result.ReleaseString();
}

void CSSRelativeColorValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(origin_color_);
  visitor->Trace(channel0_);
  visitor->Trace(channel1_);
  visitor->Trace(channel2_);
  visitor->Trace(alpha_);
  CSSValue::TraceAfterDispatch(visitor);
}

bool CSSRelativeColorValue::Equals(const CSSRelativeColorValue& other) const {
  return base::ValuesEquivalent(origin_color_, other.origin_color_) &&
         (color_interpolation_space_ == other.color_interpolation_space_) &&
         base::ValuesEquivalent(channel0_, other.channel0_) &&
         base::ValuesEquivalent(channel1_, other.channel1_) &&
         base::ValuesEquivalent(channel2_, other.channel2_) &&
         base::ValuesEquivalent(alpha_, other.alpha_);
}

const CSSValue& CSSRelativeColorValue::OriginColor() const {
  return *origin_color_;
}

Color::ColorSpace CSSRelativeColorValue::ColorInterpolationSpace() const {
  return color_interpolation_space_;
}

const CSSValue& CSSRelativeColorValue::Channel0() const {
  return *channel0_;
}

const CSSValue& CSSRelativeColorValue::Channel1() const {
  return *channel1_;
}

const CSSValue& CSSRelativeColorValue::Channel2() const {
  return *channel2_;
}

const CSSValue* CSSRelativeColorValue::Alpha() const {
  return alpha_;
}

const CSSValue*
CSSRelativeColorValue::CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
    const CSSPropertyName& property_name,
    wtf_size_t property_value_index) const {
  const CSSValue* origin_color =
      origin_color_->CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
          property_name, property_value_index);
  const CSSValue* channel0 =
      channel0_->CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
          property_name, property_value_index + 1);
  const CSSValue* channel1 =
      channel1_->CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
          property_name, property_value_index + 2);
  const CSSValue* channel2 =
      channel2_->CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
          property_name, property_value_index + 3);
  const CSSValue* alpha =
      alpha_ ? alpha_->CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
                   property_name, property_value_index + 4)
             : nullptr;
  if (origin_color != origin_color_ || alpha != alpha_ ||
      channel0 != channel0_ || channel1 != channel1_ || channel2 != channel2_) {
    return MakeGarbageCollected<CSSRelativeColorValue>(
        *origin_color, color_interpolation_space_, *channel0, *channel1,
        *channel2, alpha);
  }
  return this;
}

}  // namespace blink::cssvalue
