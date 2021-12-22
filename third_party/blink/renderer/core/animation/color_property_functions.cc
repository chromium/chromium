// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/color_property_functions.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

OptionalStyleColor ColorPropertyFunctions::GetInitialColor(
    const CSSProperty& property,
    const ComputedStyle& initial_style) {
  return GetUnvisitedColor(property, initial_style);
}

OptionalStyleColor ColorPropertyFunctions::GetUnvisitedColor(
    const CSSProperty& property,
    const ComputedStyle& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kAccentColor:
      if (style.AccentColor().IsAutoColor())
        return nullptr;
      return style.AccentColor().ToStyleColor();
    case CSSPropertyID::kBackgroundColor:
      return style.BackgroundColor();
    case CSSPropertyID::kBorderLeftColor:
      return style.BorderLeftColor();
    case CSSPropertyID::kBorderRightColor:
      return style.BorderRightColor();
    case CSSPropertyID::kBorderTopColor:
      return style.BorderTopColor();
    case CSSPropertyID::kBorderBottomColor:
      return style.BorderBottomColor();
    case CSSPropertyID::kCaretColor:
      if (style.CaretColor().IsAutoColor())
        return nullptr;
      return style.CaretColor().ToStyleColor();
    case CSSPropertyID::kColor:
      return style.GetColor();
    case CSSPropertyID::kOutlineColor:
      return style.OutlineColor();
    case CSSPropertyID::kColumnRuleColor:
      return style.ColumnRuleColor();
    case CSSPropertyID::kTextEmphasisColor:
      return style.TextEmphasisColor();
    case CSSPropertyID::kWebkitTextFillColor:
      return style.TextFillColor();
    case CSSPropertyID::kWebkitTextStrokeColor:
      return style.TextStrokeColor();
    case CSSPropertyID::kFloodColor:
      return style.FloodColor();
    case CSSPropertyID::kLightingColor:
      return style.LightingColor();
    case CSSPropertyID::kStopColor:
      return style.StopColor();
    case CSSPropertyID::kWebkitTapHighlightColor:
      return style.TapHighlightColor();
    case CSSPropertyID::kTextDecorationColor:
      return style.TextDecorationColor();
    default:
      NOTREACHED();
      return nullptr;
  }
}

OptionalStyleColor ColorPropertyFunctions::GetVisitedColor(
    const CSSProperty& property,
    const ComputedStyle& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kAccentColor:
      return style.AccentColor();
    case CSSPropertyID::kBackgroundColor:
      return style.InternalVisitedBackgroundColor();
    case CSSPropertyID::kBorderLeftColor:
      return style.InternalVisitedBorderLeftColor();
    case CSSPropertyID::kBorderRightColor:
      return style.InternalVisitedBorderRightColor();
    case CSSPropertyID::kBorderTopColor:
      return style.InternalVisitedBorderTopColor();
    case CSSPropertyID::kBorderBottomColor:
      return style.InternalVisitedBorderBottomColor();
    case CSSPropertyID::kCaretColor:
      // TODO(rego): "auto" value for caret-color should not interpolate
      // (http://crbug.com/676295).
      if (style.InternalVisitedCaretColor().IsAutoColor())
        return StyleColor::CurrentColor();
      return style.InternalVisitedCaretColor().ToStyleColor();
    case CSSPropertyID::kColor:
      return style.InternalVisitedColor();
    case CSSPropertyID::kOutlineColor:
      return style.InternalVisitedOutlineColor();
    case CSSPropertyID::kColumnRuleColor:
      return style.InternalVisitedColumnRuleColor();
    case CSSPropertyID::kTextEmphasisColor:
      return style.InternalVisitedTextEmphasisColor();
    case CSSPropertyID::kWebkitTextFillColor:
      return style.InternalVisitedTextFillColor();
    case CSSPropertyID::kWebkitTextStrokeColor:
      return style.InternalVisitedTextStrokeColor();
    case CSSPropertyID::kFloodColor:
      return style.FloodColor();
    case CSSPropertyID::kLightingColor:
      return style.LightingColor();
    case CSSPropertyID::kStopColor:
      return style.StopColor();
    case CSSPropertyID::kWebkitTapHighlightColor:
      return style.TapHighlightColor();
    case CSSPropertyID::kTextDecorationColor:
      return style.InternalVisitedTextDecorationColor();
    default:
      NOTREACHED();
      return nullptr;
  }
}

void ColorPropertyFunctions::SetUnvisitedColor(const CSSProperty& property,
                                               ComputedStyle& style,
                                               const Color& color) {
  StyleColor style_color(color);
  switch (property.PropertyID()) {
    case CSSPropertyID::kAccentColor:
      style.SetAccentColor(StyleAutoColor(color));
      return;
    case CSSPropertyID::kBackgroundColor:
      style.SetBackgroundColor(style_color);
      return;
    case CSSPropertyID::kBorderBottomColor:
      style.SetBorderBottomColor(style_color);
      return;
    case CSSPropertyID::kBorderLeftColor:
      style.SetBorderLeftColor(style_color);
      return;
    case CSSPropertyID::kBorderRightColor:
      style.SetBorderRightColor(style_color);
      return;
    case CSSPropertyID::kBorderTopColor:
      style.SetBorderTopColor(style_color);
      return;
    case CSSPropertyID::kCaretColor:
      style.SetCaretColor(StyleAutoColor(color));
      return;
    case CSSPropertyID::kColor:
      style.SetColor(style_color);
      return;
    case CSSPropertyID::kFloodColor:
      style.SetFloodColor(style_color);
      return;
    case CSSPropertyID::kLightingColor:
      style.SetLightingColor(style_color);
      return;
    case CSSPropertyID::kOutlineColor:
      style.SetOutlineColor(style_color);
      return;
    case CSSPropertyID::kStopColor:
      style.SetStopColor(style_color);
      return;
    case CSSPropertyID::kTextDecorationColor:
      style.SetTextDecorationColor(style_color);
      return;
    case CSSPropertyID::kTextEmphasisColor:
      style.SetTextEmphasisColor(style_color);
      return;
    case CSSPropertyID::kColumnRuleColor:
      style.SetColumnRuleColor(style_color);
      return;
    case CSSPropertyID::kWebkitTextStrokeColor:
      style.SetTextStrokeColor(style_color);
      return;
    default:
      NOTREACHED();
      return;
  }
}

void ColorPropertyFunctions::SetVisitedColor(const CSSProperty& property,
                                             ComputedStyle& style,
                                             const Color& color) {
  StyleColor style_color(color);
  switch (property.PropertyID()) {
    case CSSPropertyID::kAccentColor:
      // The accent-color property is not valid for :visited.
      return;
    case CSSPropertyID::kBackgroundColor:
      style.SetInternalVisitedBackgroundColor(style_color);
      return;
    case CSSPropertyID::kBorderBottomColor:
      style.SetInternalVisitedBorderBottomColor(style_color);
      return;
    case CSSPropertyID::kBorderLeftColor:
      style.SetInternalVisitedBorderLeftColor(style_color);
      return;
    case CSSPropertyID::kBorderRightColor:
      style.SetInternalVisitedBorderRightColor(style_color);
      return;
    case CSSPropertyID::kBorderTopColor:
      style.SetInternalVisitedBorderTopColor(style_color);
      return;
    case CSSPropertyID::kCaretColor:
      style.SetInternalVisitedCaretColor(StyleAutoColor(color));
      return;
    case CSSPropertyID::kColor:
      style.SetInternalVisitedColor(style_color);
      return;
    case CSSPropertyID::kFloodColor:
      style.SetFloodColor(style_color);
      return;
    case CSSPropertyID::kLightingColor:
      style.SetLightingColor(style_color);
      return;
    case CSSPropertyID::kOutlineColor:
      style.SetInternalVisitedOutlineColor(style_color);
      return;
    case CSSPropertyID::kStopColor:
      style.SetStopColor(style_color);
      return;
    case CSSPropertyID::kTextDecorationColor:
      style.SetInternalVisitedTextDecorationColor(style_color);
      return;
    case CSSPropertyID::kTextEmphasisColor:
      style.SetInternalVisitedTextEmphasisColor(style_color);
      return;
    case CSSPropertyID::kColumnRuleColor:
      style.SetInternalVisitedColumnRuleColor(style_color);
      return;
    case CSSPropertyID::kWebkitTextStrokeColor:
      style.SetInternalVisitedTextStrokeColor(style_color);
      return;
    default:
      NOTREACHED();
      return;
  }
}

}  // namespace blink
