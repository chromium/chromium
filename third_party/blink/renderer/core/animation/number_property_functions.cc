// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/number_property_functions.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

std::optional<double> NumberPropertyFunctions::GetInitialNumber(
    const CSSProperty& property,
    const ComputedStyle& initial_style) {
  return GetNumber(property, initial_style);
}

std::optional<double> NumberPropertyFunctions::GetNumber(
    const CSSProperty& property,
    const ComputedStyle& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kFillOpacity:
      return style.FillOpacity();
    case CSSPropertyID::kFlexGrow:
      return style.FlexGrow();
    case CSSPropertyID::kFlexShrink:
      return style.FlexShrink();
    case CSSPropertyID::kFloodOpacity:
      return style.FloodOpacity();
    case CSSPropertyID::kOpacity:
      return style.Opacity();
    case CSSPropertyID::kOrder:
      return style.Order();
    case CSSPropertyID::kOrphans:
      return style.Orphans();
    case CSSPropertyID::kShapeImageThreshold:
      return style.ShapeImageThreshold();
    case CSSPropertyID::kStopOpacity:
      return style.StopOpacity();
    case CSSPropertyID::kStrokeMiterlimit:
      return style.StrokeMiterLimit();
    case CSSPropertyID::kStrokeOpacity:
      return style.StrokeOpacity();
    case CSSPropertyID::kWidows:
      return style.Widows();
    case CSSPropertyID::kColumnCount:
      if (style.HasAutoColumnCount())
        return std::optional<double>();
      return style.ColumnCount();
    case CSSPropertyID::kZIndex:
      if (style.HasAutoZIndex())
        return std::optional<double>();
      return style.ZIndex();

    case CSSPropertyID::kTextSizeAdjust: {
      const TextSizeAdjust& text_size_adjust = style.GetTextSizeAdjust();
      if (text_size_adjust.IsAuto())
        return std::optional<double>();
      return text_size_adjust.Multiplier() * 100;
    }

    case CSSPropertyID::kLineHeight: {
      const Length& length = style.SpecifiedLineHeight();
      // Numbers are represented by percentages.
      if (!length.IsPercent())
        return std::optional<double>();
      double value = length.Value();
      // -100% represents the keyword "normal".
      if (value == -100)
        return std::optional<double>();
      return value / 100;
    }

    case CSSPropertyID::kTabSize: {
      if (!style.GetTabSize().IsSpaces())
        return std::nullopt;
      return style.GetTabSize().float_value_;
    }

    default:
      return std::optional<double>();
  }
}

double NumberPropertyFunctions::ClampNumber(const CSSProperty& property,
                                            double value) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kStrokeMiterlimit:
      return ClampTo<float>(value, 1);

    case CSSPropertyID::kFloodOpacity:
    case CSSPropertyID::kStopOpacity:
    case CSSPropertyID::kStrokeOpacity:
    case CSSPropertyID::kShapeImageThreshold:
      return ClampTo<float>(value, 0, 1);

    case CSSPropertyID::kFillOpacity:
    case CSSPropertyID::kOpacity:
      return ClampTo<float>(value, 0, 1);

    case CSSPropertyID::kFlexGrow:
    case CSSPropertyID::kFlexShrink:
    case CSSPropertyID::kLineHeight:
    case CSSPropertyID::kTabSize:
    case CSSPropertyID::kTextSizeAdjust:
      return ClampTo<float>(value, 0);

    case CSSPropertyID::kOrphans:
    case CSSPropertyID::kWidows:
      return ClampTo<int16_t>(round(value), 1);

    case CSSPropertyID::kColumnCount:
      return ClampTo<uint16_t>(round(value), 1);

    case CSSPropertyID::kOrder:
    case CSSPropertyID::kZIndex:
      return ClampTo<int>(RoundHalfTowardsPositiveInfinity(value));

    default:
      NOTREACHED_IN_MIGRATION();
      return value;
  }
}

bool NumberPropertyFunctions::SetNumber(const CSSProperty& property,
                                        ComputedStyleBuilder& builder,
                                        double value) {
  DCHECK_EQ(value, ClampNumber(property, value));
  switch (property.PropertyID()) {
    case CSSPropertyID::kFillOpacity:
      builder.SetFillOpacity(value);
      return true;
    case CSSPropertyID::kFlexGrow:
      builder.SetFlexGrow(value);
      return true;
    case CSSPropertyID::kFlexShrink:
      builder.SetFlexShrink(value);
      return true;
    case CSSPropertyID::kFloodOpacity:
      builder.SetFloodOpacity(value);
      return true;
    case CSSPropertyID::kLineHeight:
      builder.SetLineHeight(Length::Percent(value * 100));
      return true;
    case CSSPropertyID::kTabSize:
      builder.SetTabSize(TabSize(value));
      return true;
    case CSSPropertyID::kOpacity:
      builder.SetOpacity(value);
      return true;
    case CSSPropertyID::kOrder:
      builder.SetOrder(value);
      return true;
    case CSSPropertyID::kOrphans:
      builder.SetOrphans(value);
      return true;
    case CSSPropertyID::kShapeImageThreshold:
      builder.SetShapeImageThreshold(value);
      return true;
    case CSSPropertyID::kStopOpacity:
      builder.SetStopOpacity(value);
      return true;
    case CSSPropertyID::kStrokeMiterlimit:
      builder.SetStrokeMiterLimit(value);
      return true;
    case CSSPropertyID::kStrokeOpacity:
      builder.SetStrokeOpacity(value);
      return true;
    case CSSPropertyID::kColumnCount:
      builder.SetColumnCount(value);
      return true;
    case CSSPropertyID::kTextSizeAdjust:
      builder.SetTextSizeAdjust(value / 100.);
      return true;
    case CSSPropertyID::kWidows:
      builder.SetWidows(value);
      return true;
    case CSSPropertyID::kZIndex:
      builder.SetZIndex(value);
      return true;
    default:
      return false;
  }
}

}  // namespace blink
