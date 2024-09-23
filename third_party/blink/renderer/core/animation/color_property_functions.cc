// Copyright 2015 The Chromium Authors
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

template <typename ComputedStyleOrBuilder>
OptionalStyleColor ColorPropertyFunctions::GetUnvisitedColor(
    const CSSProperty& property,
    const ComputedStyleOrBuilder& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kAccentColor:
      if (style.AccentColor().IsAutoColor())
        return OptionalStyleColor();
      return OptionalStyleColor(style.AccentColor().ToStyleColor());
    case CSSPropertyID::kBackgroundColor:
      return OptionalStyleColor(style.BackgroundColor());
    case CSSPropertyID::kBorderLeftColor:
      return OptionalStyleColor(style.BorderLeftColor());
    case CSSPropertyID::kBorderRightColor:
      return OptionalStyleColor(style.BorderRightColor());
    case CSSPropertyID::kBorderTopColor:
      return OptionalStyleColor(style.BorderTopColor());
    case CSSPropertyID::kBorderBottomColor:
      return OptionalStyleColor(style.BorderBottomColor());
    case CSSPropertyID::kCaretColor:
      if (style.CaretColor().IsAutoColor())
        return OptionalStyleColor();
      return OptionalStyleColor(style.CaretColor().ToStyleColor());
    case CSSPropertyID::kColor:
      return OptionalStyleColor(style.Color());
    case CSSPropertyID::kOutlineColor:
      return OptionalStyleColor(style.OutlineColor());
    case CSSPropertyID::kColumnRuleColor:
      return OptionalStyleColor(style.ColumnRuleColor());
    case CSSPropertyID::kTextEmphasisColor:
      return OptionalStyleColor(style.TextEmphasisColor());
    case CSSPropertyID::kWebkitTextFillColor:
      return OptionalStyleColor(style.TextFillColor());
    case CSSPropertyID::kWebkitTextStrokeColor:
      return OptionalStyleColor(style.TextStrokeColor());
    case CSSPropertyID::kFloodColor:
      return OptionalStyleColor(style.FloodColor());
    case CSSPropertyID::kLightingColor:
      return OptionalStyleColor(style.LightingColor());
    case CSSPropertyID::kStopColor:
      return OptionalStyleColor(style.StopColor());
    case CSSPropertyID::kWebkitTapHighlightColor:
      return OptionalStyleColor(style.TapHighlightColor());
    case CSSPropertyID::kTextDecorationColor:
      return OptionalStyleColor(style.TextDecorationColor());
    default:
      NOTREACHED_IN_MIGRATION();
      return OptionalStyleColor();
  }
}

template OptionalStyleColor
ColorPropertyFunctions::GetUnvisitedColor<ComputedStyle>(const CSSProperty&,
                                                         const ComputedStyle&);
template OptionalStyleColor ColorPropertyFunctions::GetUnvisitedColor<
    ComputedStyleBuilder>(const CSSProperty&, const ComputedStyleBuilder&);

template <typename ComputedStyleOrBuilder>
OptionalStyleColor ColorPropertyFunctions::GetVisitedColor(
    const CSSProperty& property,
    const ComputedStyleOrBuilder& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kAccentColor:
      return OptionalStyleColor(style.AccentColor());
    case CSSPropertyID::kBackgroundColor:
      return OptionalStyleColor(style.InternalVisitedBackgroundColor());
    case CSSPropertyID::kBorderLeftColor:
      return OptionalStyleColor(style.InternalVisitedBorderLeftColor());
    case CSSPropertyID::kBorderRightColor:
      return OptionalStyleColor(style.InternalVisitedBorderRightColor());
    case CSSPropertyID::kBorderTopColor:
      return OptionalStyleColor(style.InternalVisitedBorderTopColor());
    case CSSPropertyID::kBorderBottomColor:
      return OptionalStyleColor(style.InternalVisitedBorderBottomColor());
    case CSSPropertyID::kCaretColor:
      // TODO(rego): "auto" value for caret-color should not interpolate
      // (http://crbug.com/676295).
      if (style.InternalVisitedCaretColor().IsAutoColor())
        return OptionalStyleColor(StyleColor::CurrentColor());
      return OptionalStyleColor(
          style.InternalVisitedCaretColor().ToStyleColor());
    case CSSPropertyID::kColor:
      return OptionalStyleColor(style.InternalVisitedColor());
    case CSSPropertyID::kOutlineColor:
      return OptionalStyleColor(style.InternalVisitedOutlineColor());
    case CSSPropertyID::kColumnRuleColor:
      return OptionalStyleColor(style.InternalVisitedColumnRuleColor());
    case CSSPropertyID::kTextEmphasisColor:
      return OptionalStyleColor(style.InternalVisitedTextEmphasisColor());
    case CSSPropertyID::kWebkitTextFillColor:
      return OptionalStyleColor(style.InternalVisitedTextFillColor());
    case CSSPropertyID::kWebkitTextStrokeColor:
      return OptionalStyleColor(style.InternalVisitedTextStrokeColor());
    case CSSPropertyID::kFloodColor:
      return OptionalStyleColor(style.FloodColor());
    case CSSPropertyID::kLightingColor:
      return OptionalStyleColor(style.LightingColor());
    case CSSPropertyID::kStopColor:
      return OptionalStyleColor(style.StopColor());
    case CSSPropertyID::kWebkitTapHighlightColor:
      return OptionalStyleColor(style.TapHighlightColor());
    case CSSPropertyID::kTextDecorationColor:
      return OptionalStyleColor(style.InternalVisitedTextDecorationColor());
    default:
      NOTREACHED_IN_MIGRATION();
      return OptionalStyleColor();
  }
}

template OptionalStyleColor
ColorPropertyFunctions::GetVisitedColor<ComputedStyle>(const CSSProperty&,
                                                       const ComputedStyle&);
template OptionalStyleColor ColorPropertyFunctions::GetVisitedColor<
    ComputedStyleBuilder>(const CSSProperty&, const ComputedStyleBuilder&);

void ColorPropertyFunctions::SetUnvisitedColor(const CSSProperty& property,
                                               ComputedStyleBuilder& builder,
                                               const Color& color) {
  StyleColor style_color(color);
  switch (property.PropertyID()) {
    case CSSPropertyID::kAccentColor:
      builder.SetAccentColor(StyleAutoColor(std::move(style_color)));
      return;
    case CSSPropertyID::kBackgroundColor:
      builder.SetBackgroundColor(style_color);
      return;
    case CSSPropertyID::kBorderBottomColor:
      builder.SetBorderBottomColor(style_color);
      return;
    case CSSPropertyID::kBorderLeftColor:
      builder.SetBorderLeftColor(style_color);
      return;
    case CSSPropertyID::kBorderRightColor:
      builder.SetBorderRightColor(style_color);
      return;
    case CSSPropertyID::kBorderTopColor:
      builder.SetBorderTopColor(style_color);
      return;
    case CSSPropertyID::kCaretColor:
      builder.SetCaretColor(StyleAutoColor(std::move(style_color)));
      return;
    case CSSPropertyID::kColor:
      builder.SetColor(style_color);
      return;
    case CSSPropertyID::kFloodColor:
      builder.SetFloodColor(style_color);
      return;
    case CSSPropertyID::kLightingColor:
      builder.SetLightingColor(style_color);
      return;
    case CSSPropertyID::kOutlineColor:
      builder.SetOutlineColor(style_color);
      return;
    case CSSPropertyID::kStopColor:
      builder.SetStopColor(style_color);
      return;
    case CSSPropertyID::kTextDecorationColor:
      builder.SetTextDecorationColor(style_color);
      return;
    case CSSPropertyID::kTextEmphasisColor:
      builder.SetTextEmphasisColor(style_color);
      return;
    case CSSPropertyID::kColumnRuleColor:
      builder.SetColumnRuleColor(style_color);
      return;
    case CSSPropertyID::kWebkitTextStrokeColor:
      builder.SetTextStrokeColor(style_color);
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

void ColorPropertyFunctions::SetVisitedColor(const CSSProperty& property,
                                             ComputedStyleBuilder& builder,
                                             const Color& color) {
  StyleColor style_color(color);
  switch (property.PropertyID()) {
    case CSSPropertyID::kAccentColor:
      // The accent-color property is not valid for :visited.
      return;
    case CSSPropertyID::kBackgroundColor:
      builder.SetInternalVisitedBackgroundColor(style_color);
      return;
    case CSSPropertyID::kBorderBottomColor:
      builder.SetInternalVisitedBorderBottomColor(style_color);
      return;
    case CSSPropertyID::kBorderLeftColor:
      builder.SetInternalVisitedBorderLeftColor(style_color);
      return;
    case CSSPropertyID::kBorderRightColor:
      builder.SetInternalVisitedBorderRightColor(style_color);
      return;
    case CSSPropertyID::kBorderTopColor:
      builder.SetInternalVisitedBorderTopColor(style_color);
      return;
    case CSSPropertyID::kCaretColor:
      builder.SetInternalVisitedCaretColor(
          StyleAutoColor(std::move(style_color)));
      return;
    case CSSPropertyID::kColor:
      builder.SetInternalVisitedColor(style_color);
      return;
    case CSSPropertyID::kFloodColor:
      builder.SetFloodColor(style_color);
      return;
    case CSSPropertyID::kLightingColor:
      builder.SetLightingColor(style_color);
      return;
    case CSSPropertyID::kOutlineColor:
      builder.SetInternalVisitedOutlineColor(style_color);
      return;
    case CSSPropertyID::kStopColor:
      builder.SetStopColor(style_color);
      return;
    case CSSPropertyID::kTextDecorationColor:
      builder.SetInternalVisitedTextDecorationColor(style_color);
      return;
    case CSSPropertyID::kTextEmphasisColor:
      builder.SetInternalVisitedTextEmphasisColor(style_color);
      return;
    case CSSPropertyID::kColumnRuleColor:
      builder.SetInternalVisitedColumnRuleColor(style_color);
      return;
    case CSSPropertyID::kWebkitTextStrokeColor:
      builder.SetInternalVisitedTextStrokeColor(style_color);
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

}  // namespace blink
