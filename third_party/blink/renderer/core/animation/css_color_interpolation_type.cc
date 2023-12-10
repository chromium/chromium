// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"

#include <memory>
#include <tuple>
#include <utility>

#include "third_party/blink/renderer/core/animation/color_property_functions.h"
#include "third_party/blink/renderer/core/animation/interpolable_color.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

InterpolableColor* CSSColorInterpolationType::CreateInterpolableColor(
    const Color& color) {
  return InterpolableColor::Create(color);
}

InterpolableColor* CSSColorInterpolationType::CreateInterpolableColor(
    CSSValueID keyword) {
  return InterpolableColor::Create(keyword);
}

InterpolableColor* CSSColorInterpolationType::CreateInterpolableColor(
    const StyleColor& color) {
  if (!color.IsNumeric()) {
    CSSValueID color_keyword = color.GetColorKeyword();
    DCHECK(StyleColor::IsColorKeyword(color_keyword))
        << color << " is not a recognized color keyword";
    return CreateInterpolableColor(color_keyword);
  }
  return CreateInterpolableColor(color.GetColor());
}

InterpolableColor* CSSColorInterpolationType::MaybeCreateInterpolableColor(
    const CSSValue& value) {
  if (auto* color_value = DynamicTo<cssvalue::CSSColor>(value)) {
    return CreateInterpolableColor(color_value->Value());
  }
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return nullptr;
  if (!StyleColor::IsColorKeyword(identifier_value->GetValueID()))
    return nullptr;
  return CreateInterpolableColor(identifier_value->GetValueID());
}

Color CSSColorInterpolationType::GetColor(const InterpolableValue& value) {
  const InterpolableColor& color = To<InterpolableColor>(value);
  return color.GetColor();
}

bool CSSColorInterpolationType::IsNonKeywordColor(
    const InterpolableValue& value) {
  if (!value.IsColor())
    return false;

  const InterpolableColor& color = To<InterpolableColor>(value);
  return !color.IsKeywordColor();
}

namespace {
std::tuple<double, double, double, double> AddPremultipliedColor(
    double param0,
    double param1,
    double param2,
    double alpha,
    double fraction,
    Color color,
    Color::ColorSpace color_space) {
  color.ConvertToColorSpace(color_space);
  return std::make_tuple(param0 + fraction * color.Param0() * color.Alpha(),
                         param1 + fraction * color.Param1() * color.Alpha(),
                         param2 + fraction * color.Param2() * color.Alpha(),
                         alpha + fraction * color.Alpha());
}

std::tuple<double, double, double> UnpremultiplyColor(double param0,
                                                      double param1,
                                                      double param2,
                                                      double alpha) {
  return std::make_tuple(param0 / alpha, param1 / alpha, param2 / alpha);
}
}  // namespace

Color CSSColorInterpolationType::ResolveInterpolableColor(
    const InterpolableValue& interpolable_color,
    const StyleResolverState& state,
    bool is_visited,
    bool is_text_decoration) {
  const InterpolableColor& color = To<InterpolableColor>(interpolable_color);

  double param0 = color.Param0();
  double param1 = color.Param1();
  double param2 = color.Param2();
  double alpha = color.Alpha();
  Color::ColorSpace color_space = color.ColorSpace();

  if (double currentcolor_fraction = color.GetColorFraction(
          InterpolableColor::ColorKeyword::kCurrentcolor)) {
    auto current_color_getter =
        is_visited
            ? ColorPropertyFunctions::GetVisitedColor<ComputedStyleBuilder>
            : ColorPropertyFunctions::GetUnvisitedColor<ComputedStyleBuilder>;
    StyleColor current_style_color = StyleColor::CurrentColor();
    if (is_text_decoration) {
      current_style_color =
          current_color_getter(
              CSSProperty::Get(CSSPropertyID::kWebkitTextFillColor),
              state.StyleBuilder())
              .value();
    }
    if (current_style_color.IsCurrentColor()) {
      current_style_color =
          current_color_getter(CSSProperty::Get(CSSPropertyID::kColor),
                               state.StyleBuilder())
              .value();
    }
    std::tie(param0, param1, param2, alpha) = AddPremultipliedColor(
        param0, param1, param2, alpha, currentcolor_fraction,
        current_style_color.Resolve(Color(),
                                    state.StyleBuilder().UsedColorScheme()),
        color_space);
  }
  const TextLinkColors& colors = state.GetDocument().GetTextLinkColors();
  if (double webkit_activelink_fraction = color.GetColorFraction(
          InterpolableColor::ColorKeyword::kWebkitActivelink)) {
    std::tie(param0, param1, param2, alpha) = AddPremultipliedColor(
        param0, param1, param2, alpha, webkit_activelink_fraction,
        colors.ActiveLinkColor(), color_space);
  }
  if (double webkit_link_fraction = color.GetColorFraction(
          InterpolableColor::ColorKeyword::kWebkitLink)) {
    std::tie(param0, param1, param2, alpha) = AddPremultipliedColor(
        param0, param1, param2, alpha, webkit_link_fraction,
        is_visited ? colors.VisitedLinkColor() : colors.LinkColor(),
        color_space);
  }
  if (double quirk_inherit_fraction = color.GetColorFraction(
          InterpolableColor::ColorKeyword::kQuirkInherit)) {
    std::tie(param0, param1, param2, alpha) = AddPremultipliedColor(
        param0, param1, param2, alpha, quirk_inherit_fraction,
        colors.TextColor(), color_space);
  }

  alpha = ClampTo<double>(alpha, 0, 1);
  if (alpha == 0)
    return Color::kTransparent;
  std::tie(param0, param1, param2) =
      UnpremultiplyColor(param0, param1, param2, alpha);

  switch (color_space) {
    case Color::ColorSpace::kSRGBLegacy:
    case Color::ColorSpace::kOklab:
      return Color::FromColorSpace(color_space, param0, param1, param2, alpha);
    default:
      // There is no way for the user to specify which color spaces should be
      // used for interpolation, so sRGB (for legacy colors) and Oklab are
      // the only possibilities.
      // https://www.w3.org/TR/css-color-4/#interpolation-space
      NOTREACHED();
      return Color();
  }
}

class InheritedColorChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedColorChecker(const CSSProperty& property,
                        const absl::optional<StyleColor>& color)
      : property_(property), color_(color) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return color_ == ColorPropertyFunctions::GetUnvisitedColor(
                         property_, *state.ParentStyle());
  }

  const CSSProperty& property_;
  const absl::optional<StyleColor> color_;
};

InterpolationValue CSSColorInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return ConvertStyleColorPair(StyleColor(Color::kTransparent),
                               StyleColor(Color::kTransparent));
}

InterpolationValue CSSColorInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  absl::optional<StyleColor> initial_color =
      ColorPropertyFunctions::GetInitialColor(
          CssProperty(), state.GetDocument().GetStyleResolver().InitialStyle());
  if (!initial_color.has_value()) {
    return nullptr;
  }
  return ConvertStyleColorPair(initial_color.value(), initial_color.value());
}

InterpolationValue CSSColorInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  // Visited color can never explicitly inherit from parent visited color so
  // only use the unvisited color.
  absl::optional<StyleColor> inherited_color =
      ColorPropertyFunctions::GetUnvisitedColor(CssProperty(),
                                                *state.ParentStyle());
  conversion_checkers.push_back(
      std::make_unique<InheritedColorChecker>(CssProperty(), inherited_color));
  return ConvertStyleColorPair(inherited_color, inherited_color);
}

enum InterpolableColorPairIndex : unsigned {
  kUnvisited,
  kVisited,
  kInterpolableColorPairIndexCount,
};

InterpolationValue CSSColorInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers& conversion_checkers) const {
  if (CssProperty().PropertyID() == CSSPropertyID::kColor) {
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
    if (identifier_value &&
        identifier_value->GetValueID() == CSSValueID::kCurrentcolor) {
      DCHECK(state);
      return MaybeConvertInherit(*state, conversion_checkers);
    }
  }

  InterpolableColor* interpolable_color = MaybeCreateInterpolableColor(value);
  if (!interpolable_color)
    return nullptr;
  auto* color_pair =
      MakeGarbageCollected<InterpolableList>(kInterpolableColorPairIndexCount);
  color_pair->Set(kUnvisited, interpolable_color->Clone());
  color_pair->Set(kVisited, interpolable_color);
  return InterpolationValue(color_pair);
}

PairwiseInterpolationValue CSSColorInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  DCHECK(!start.non_interpolable_value);
  DCHECK(!end.non_interpolable_value);

  InterpolableList& start_list =
      To<InterpolableList>(*start.interpolable_value);
  InterpolableList& end_list = To<InterpolableList>(*end.interpolable_value);
  DCHECK_EQ(start_list.length(), end_list.length());

  for (unsigned i = 0; i < start_list.length(); i++) {
    InterpolableColor& start_color =
        To<InterpolableColor>(*(start_list.GetMutable(i)));
    InterpolableColor& end_color =
        To<InterpolableColor>(*(end_list.GetMutable(i)));

    // Confirm that both colors are in the same colorspace and adjust if
    // necessary.
    InterpolableColor::SetupColorInterpolationSpaces(start_color, end_color);
  }

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value), nullptr);
}

InterpolationValue CSSColorInterpolationType::ConvertStyleColorPair(
    const absl::optional<StyleColor>& unvisited_color,
    const absl::optional<StyleColor>& visited_color) {
  if (!unvisited_color.has_value() || !visited_color.has_value()) {
    return nullptr;
  }
  return ConvertStyleColorPair(unvisited_color.value(), visited_color.value());
}

InterpolationValue CSSColorInterpolationType::ConvertStyleColorPair(
    const StyleColor& unvisited_color,
    const StyleColor& visited_color) {
  auto* color_pair =
      MakeGarbageCollected<InterpolableList>(kInterpolableColorPairIndexCount);
  color_pair->Set(kUnvisited, CreateInterpolableColor(unvisited_color));
  color_pair->Set(kVisited, CreateInterpolableColor(visited_color));
  return InterpolationValue(color_pair);
}

InterpolationValue
CSSColorInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertStyleColorPair(
      ColorPropertyFunctions::GetUnvisitedColor(CssProperty(), style),
      ColorPropertyFunctions::GetVisitedColor(CssProperty(), style));
}

void CSSColorInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  const auto& color_pair = To<InterpolableList>(interpolable_value);
  DCHECK_EQ(color_pair.length(), kInterpolableColorPairIndexCount);
  ColorPropertyFunctions::SetUnvisitedColor(
      CssProperty(), state.StyleBuilder(),
      ResolveInterpolableColor(
          *color_pair.Get(kUnvisited), state, false,
          CssProperty().PropertyID() == CSSPropertyID::kTextDecorationColor));
  ColorPropertyFunctions::SetVisitedColor(
      CssProperty(), state.StyleBuilder(),
      ResolveInterpolableColor(
          *color_pair.Get(kVisited), state, true,
          CssProperty().PropertyID() == CSSPropertyID::kTextDecorationColor));
}

const CSSValue* CSSColorInterpolationType::CreateCSSValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    const StyleResolverState& state) const {
  const auto& color_pair = To<InterpolableList>(interpolable_value);
  Color color = ResolveInterpolableColor(*color_pair.Get(kUnvisited), state);
  return cssvalue::CSSColor::Create(color);
}

void CSSColorInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double) const {
  DCHECK(!underlying_value_owner.Value().non_interpolable_value);
  DCHECK(!value.non_interpolable_value);
  auto& underlying_list = To<InterpolableList>(
      *underlying_value_owner.MutableValue().interpolable_value);
  const auto& other_list = To<InterpolableList>(*value.interpolable_value);
  // Both lists should have kUnvisited and kVisited.
  DCHECK(underlying_list.length() == kInterpolableColorPairIndexCount);
  DCHECK(other_list.length() == kInterpolableColorPairIndexCount);

  for (wtf_size_t i = 0; i < underlying_list.length(); i++) {
    auto& underlying = To<InterpolableColor>(*underlying_list.GetMutable(i));
    const auto& other = To<InterpolableColor>(*other_list.Get(i));
    underlying.Composite(other, underlying_fraction);
  }
}

}  // namespace blink
