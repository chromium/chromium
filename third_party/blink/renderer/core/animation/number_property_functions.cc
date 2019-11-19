// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/number_property_functions.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

base::Optional<double> NumberPropertyFunctions::GetInitialNumber(
    const CSSProperty& property) {
  return GetNumber(property, ComputedStyle::InitialStyle());
}

base::Optional<double> NumberPropertyFunctions::GetNumber(
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

    case CSSPropertyID::kFontSizeAdjust:
      if (!style.HasFontSizeAdjust())
        return base::Optional<double>();
      return style.FontSizeAdjust();
    case CSSPropertyID::kColumnCount:
      if (style.HasAutoColumnCount())
        return base::Optional<double>();
      return style.ColumnCount();
    case CSSPropertyID::kZIndex:
      if (style.HasAutoZIndex())
        return base::Optional<double>();
      return style.ZIndex();

    case CSSPropertyID::kTextSizeAdjust: {
      const TextSizeAdjust& text_size_adjust = style.GetTextSizeAdjust();
      if (text_size_adjust.IsAuto())
        return base::Optional<double>();
      return text_size_adjust.Multiplier() * 100;
    }

    case CSSPropertyID::kLineHeight: {
      const Length& length = style.SpecifiedLineHeight();
      // Numbers are represented by percentages.
      if (!length.IsPercent())
        return base::Optional<double>();
      double value = length.Value();
      // -100% represents the keyword "normal".
      if (value == -100)
        return base::Optional<double>();
      return value / 100;
    }

    default:
      return base::Optional<double>();
  }
}

double NumberPropertyFunctions::ClampNumber(const CSSProperty& property,
                                            double value) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kStrokeMiterlimit:
      return clampTo<float>(value, 1);

    case CSSPropertyID::kFloodOpacity:
    case CSSPropertyID::kStopOpacity:
    case CSSPropertyID::kStrokeOpacity:
    case CSSPropertyID::kShapeImageThreshold:
      return clampTo<float>(value, 0, 1);

    case CSSPropertyID::kFillOpacity:
    case CSSPropertyID::kOpacity:
      return clampTo<float>(value, 0, nextafterf(1, 0));

    case CSSPropertyID::kFlexGrow:
    case CSSPropertyID::kFlexShrink:
    case CSSPropertyID::kFontSizeAdjust:
    case CSSPropertyID::kLineHeight:
    case CSSPropertyID::kTextSizeAdjust:
      return clampTo<float>(value, 0);

    case CSSPropertyID::kOrphans:
    case CSSPropertyID::kWidows:
      return clampTo<int16_t>(round(value), 1);

    case CSSPropertyID::kColumnCount:
      return clampTo<uint16_t>(round(value), 1);

    case CSSPropertyID::kOrder:
    case CSSPropertyID::kZIndex:
      return clampTo<int>(round(value));

    default:
      NOTREACHED();
      return value;
  }
}

bool NumberPropertyFunctions::SetNumber(const CSSProperty& property,
                                        ComputedStyle& style,
                                        double value) {
  DCHECK_EQ(value, ClampNumber(property, value));
  switch (property.PropertyID()) {
    case CSSPropertyID::kFillOpacity:
      style.SetFillOpacity(value);
      return true;
    case CSSPropertyID::kFlexGrow:
      style.SetFlexGrow(value);
      return true;
    case CSSPropertyID::kFlexShrink:
      style.SetFlexShrink(value);
      return true;
    case CSSPropertyID::kFloodOpacity:
      style.SetFloodOpacity(value);
      return true;
    case CSSPropertyID::kLineHeight:
      style.SetLineHeight(Length::Percent(value * 100));
      return true;
    case CSSPropertyID::kOpacity:
      style.SetOpacity(value);
      return true;
    case CSSPropertyID::kOrder:
      style.SetOrder(value);
      return true;
    case CSSPropertyID::kOrphans:
      style.SetOrphans(value);
      return true;
    case CSSPropertyID::kShapeImageThreshold:
      style.SetShapeImageThreshold(value);
      return true;
    case CSSPropertyID::kStopOpacity:
      style.SetStopOpacity(value);
      return true;
    case CSSPropertyID::kStrokeMiterlimit:
      style.SetStrokeMiterLimit(value);
      return true;
    case CSSPropertyID::kStrokeOpacity:
      style.SetStrokeOpacity(value);
      return true;
    case CSSPropertyID::kColumnCount:
      style.SetColumnCount(value);
      return true;
    case CSSPropertyID::kTextSizeAdjust:
      style.SetTextSizeAdjust(value / 100.);
      return true;
    case CSSPropertyID::kWidows:
      style.SetWidows(value);
      return true;
    case CSSPropertyID::kZIndex:
      style.SetZIndex(value);
      return true;
    default:
      return false;
  }
}

}  // namespace blink
