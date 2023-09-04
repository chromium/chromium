// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_paint_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

static bool GetColorFromPaint(const SVGPaint& paint, StyleColor& result) {
  if (!paint.IsColor())
    return false;
  result = paint.GetColor();
  return true;
}

bool GetColor(const CSSProperty& property,
              const ComputedStyle& style,
              StyleColor& result) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kFill:
      return GetColorFromPaint(style.FillPaint(), result);
    case CSSPropertyID::kStroke:
      return GetColorFromPaint(style.StrokePaint(), result);
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace

InterpolationValue CSSPaintInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(
      CSSColorInterpolationType::CreateInterpolableColor(Color::kTransparent));
}

InterpolationValue CSSPaintInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  StyleColor initial_color;
  if (!GetColor(CssProperty(),
                state.GetDocument().GetStyleResolver().InitialStyle(),
                initial_color))
    return nullptr;
  return InterpolationValue(
      CSSColorInterpolationType::CreateInterpolableColor(initial_color));
}

PairwiseInterpolationValue CSSPaintInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  DCHECK(!start.non_interpolable_value);
  DCHECK(!end.non_interpolable_value);

  // Confirm that both colors are in the same colorspace and adjust if
  // necessary.
  auto& start_color = To<InterpolableColor>(*start.interpolable_value);
  auto& end_color = To<InterpolableColor>(*end.interpolable_value);
  InterpolableColor::SetupColorInterpolationSpaces(start_color, end_color);

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value), nullptr);
}

class InheritedPaintChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedPaintChecker(const CSSProperty& property)
      : property_(property), valid_color_(false) {}
  InheritedPaintChecker(const CSSProperty& property, const StyleColor& color)
      : property_(property), valid_color_(true), color_(color) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    StyleColor parent_color;
    if (!GetColor(property_, *state.ParentStyle(), parent_color))
      return !valid_color_;
    return valid_color_ && parent_color == color_;
  }

  const CSSProperty& property_;
  const bool valid_color_;
  const StyleColor color_;
};

InterpolationValue CSSPaintInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  StyleColor parent_color;
  if (!GetColor(CssProperty(), *state.ParentStyle(), parent_color)) {
    conversion_checkers.push_back(
        std::make_unique<InheritedPaintChecker>(CssProperty()));
    return nullptr;
  }
  conversion_checkers.push_back(
      std::make_unique<InheritedPaintChecker>(CssProperty(), parent_color));
  return InterpolationValue(
      CSSColorInterpolationType::CreateInterpolableColor(parent_color));
}

InterpolationValue CSSPaintInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  std::unique_ptr<InterpolableValue> interpolable_color =
      CSSColorInterpolationType::MaybeCreateInterpolableColor(value);
  if (!interpolable_color)
    return nullptr;
  return InterpolationValue(std::move(interpolable_color));
}

InterpolationValue
CSSPaintInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  // TODO(alancutter): Support capturing and animating with the visited paint
  // color.
  StyleColor underlying_color;
  if (!GetColor(CssProperty(), style, underlying_color))
    return nullptr;
  return InterpolationValue(
      CSSColorInterpolationType::CreateInterpolableColor(underlying_color));
}

void CSSPaintInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_color,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  Color color = CSSColorInterpolationType::ResolveInterpolableColor(
      interpolable_color, state);
  switch (CssProperty().PropertyID()) {
    case CSSPropertyID::kFill:
      builder.SetFillPaint(SVGPaint(color));
      builder.SetInternalVisitedFillPaint(SVGPaint(color));
      break;
    case CSSPropertyID::kStroke:
      builder.SetStrokePaint(SVGPaint(color));
      builder.SetInternalVisitedStrokePaint(SVGPaint(color));
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace blink
