// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/color_property_functions.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

OptionalStyleColor ColorPropertyFunctions::GetInitialColor(
    const CSSProperty& property) {
  return GetUnvisitedColor(property, ComputedStyle::InitialStyle());
}

OptionalStyleColor ColorPropertyFunctions::GetUnvisitedColor(
    const CSSProperty& property,
    const ComputedStyle& style) {
  switch (property.PropertyID()) {
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
    case CSSPropertyID::kWebkitTextEmphasisColor:
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
    case CSSPropertyID::kWebkitTextEmphasisColor:
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
  switch (property.PropertyID()) {
    case CSSPropertyID::kBackgroundColor:
      style.SetBackgroundColor(color);
      return;
    case CSSPropertyID::kBorderBottomColor:
      style.SetBorderBottomColor(color);
      return;
    case CSSPropertyID::kBorderLeftColor:
      style.SetBorderLeftColor(color);
      return;
    case CSSPropertyID::kBorderRightColor:
      style.SetBorderRightColor(color);
      return;
    case CSSPropertyID::kBorderTopColor:
      style.SetBorderTopColor(color);
      return;
    case CSSPropertyID::kCaretColor:
      return style.SetCaretColor(color);
    case CSSPropertyID::kColor:
      style.SetColor(color);
      return;
    case CSSPropertyID::kFloodColor:
      style.SetFloodColor(color);
      return;
    case CSSPropertyID::kLightingColor:
      style.SetLightingColor(color);
      return;
    case CSSPropertyID::kOutlineColor:
      style.SetOutlineColor(color);
      return;
    case CSSPropertyID::kStopColor:
      style.SetStopColor(color);
      return;
    case CSSPropertyID::kTextDecorationColor:
      style.SetTextDecorationColor(color);
      return;
    case CSSPropertyID::kColumnRuleColor:
      style.SetColumnRuleColor(color);
      return;
    case CSSPropertyID::kWebkitTextStrokeColor:
      style.SetTextStrokeColor(color);
      return;
    default:
      NOTREACHED();
      return;
  }
}

void ColorPropertyFunctions::SetVisitedColor(const CSSProperty& property,
                                             ComputedStyle& style,
                                             const Color& color) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBackgroundColor:
      style.SetInternalVisitedBackgroundColor(color);
      return;
    case CSSPropertyID::kBorderBottomColor:
      style.SetInternalVisitedBorderBottomColor(color);
      return;
    case CSSPropertyID::kBorderLeftColor:
      style.SetInternalVisitedBorderLeftColor(color);
      return;
    case CSSPropertyID::kBorderRightColor:
      style.SetInternalVisitedBorderRightColor(color);
      return;
    case CSSPropertyID::kBorderTopColor:
      style.SetInternalVisitedBorderTopColor(color);
      return;
    case CSSPropertyID::kCaretColor:
      return style.SetInternalVisitedCaretColor(color);
    case CSSPropertyID::kColor:
      style.SetInternalVisitedColor(color);
      return;
    case CSSPropertyID::kFloodColor:
      style.SetFloodColor(color);
      return;
    case CSSPropertyID::kLightingColor:
      style.SetLightingColor(color);
      return;
    case CSSPropertyID::kOutlineColor:
      style.SetInternalVisitedOutlineColor(color);
      return;
    case CSSPropertyID::kStopColor:
      style.SetStopColor(color);
      return;
    case CSSPropertyID::kTextDecorationColor:
      style.SetInternalVisitedTextDecorationColor(color);
      return;
    case CSSPropertyID::kColumnRuleColor:
      style.SetInternalVisitedColumnRuleColor(color);
      return;
    case CSSPropertyID::kWebkitTextStrokeColor:
      style.SetInternalVisitedTextStrokeColor(color);
      return;
    default:
      NOTREACHED();
      return;
  }
}

}  // namespace blink
