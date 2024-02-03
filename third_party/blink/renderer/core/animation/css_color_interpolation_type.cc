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

namespace {

Color ResolveCurrentColor(const StyleResolverState& state,
                          bool is_visited,
                          bool is_text_decoration) {
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
  return current_style_color.Resolve(Color(),
                                     state.StyleBuilder().UsedColorScheme());
}

}  // end anonymous namespace

/* static */
void CSSColorInterpolationType::EnsureInterpolableStyleColor(
    InterpolableList& list,
    wtf_size_t index) {
  BaseInterpolableColor& base =
      To<BaseInterpolableColor>(*list.GetMutable(index));
  if (!base.IsStyleColor()) {
    list.Set(index, InterpolableStyleColor::Create(&base));
  }
}

/* static */
void CSSColorInterpolationType::EnsureCompatibleInterpolableColorTypes(
    InterpolableList& list_a,
    InterpolableList& list_b) {
  CHECK_EQ(list_a.length(), list_b.length());
  for (wtf_size_t i = 0; i < list_a.length(); i++) {
    if (list_a.Get(i)->IsStyleColor() != list_b.Get(i)->IsStyleColor()) {
      // If either value is a style color then both must be.
      EnsureInterpolableStyleColor(list_a, i);
      EnsureInterpolableStyleColor(list_b, i);
    }
    DCHECK_EQ(list_a.Get(i)->IsStyleColor(), list_b.Get(i)->IsStyleColor());
  }
}

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

BaseInterpolableColor* CSSColorInterpolationType::CreateBaseInterpolableColor(
    const StyleColor& color) {
  if (color.IsUnresolvedColorMixFunction()) {
    return InterpolableStyleColor::Create(color);
  }
  return CreateInterpolableColor(color);
}

InterpolableColor* CSSColorInterpolationType::MaybeCreateInterpolableColor(
    const CSSValue& value) {
  if (auto* color_value = DynamicTo<cssvalue::CSSColor>(value)) {
    return CreateInterpolableColor(color_value->Value());
  }
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return nullptr;

  // TODO(crbug.com/1500708): Handle unresolved-color-mix. CSS-animations go
  // through this code path. Unresolved color-mix results in a discrete
  // animation.
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

Color CSSColorInterpolationType::ResolveInterpolableColor(
    const InterpolableValue& value,
    const StyleResolverState& state,
    bool is_visited,
    bool is_text_decoration) {
  Color current_color;
  const TextLinkColors& text_link_colors =
      state.GetDocument().GetTextLinkColors();
  const Color& active_link_color = text_link_colors.ActiveLinkColor();
  const Color& link_color = is_visited ? text_link_colors.VisitedLinkColor()
                                       : text_link_colors.LinkColor();
  const Color& text_color = text_link_colors.TextColor();
  const BaseInterpolableColor& base = To<BaseInterpolableColor>(value);
  if (base.HasCurrentColorDependency()) {
    current_color = ResolveCurrentColor(state, is_visited, is_text_decoration);
  }
  return base.Resolve(current_color, active_link_color, link_color, text_color,
                      state.StyleBuilder().UsedColorScheme());
}

class InheritedColorChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedColorChecker(const CSSProperty& property,
                        const std::optional<StyleColor>& color)
      : property_(property), color_(color) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return color_ == ColorPropertyFunctions::GetUnvisitedColor(
                         property_, *state.ParentStyle());
  }

  const CSSProperty& property_;
  const std::optional<StyleColor> color_;
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
  std::optional<StyleColor> initial_color =
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
  std::optional<StyleColor> inherited_color =
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
  if (!interpolable_color) {
    return nullptr;
  }

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
  EnsureCompatibleInterpolableColorTypes(start_list, end_list);

  for (unsigned i = 0; i < start_list.length(); i++) {
    if (start_list.Get(i)->IsStyleColor()) {
      continue;
    }

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
    const std::optional<StyleColor>& unvisited_color,
    const std::optional<StyleColor>& visited_color) {
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
  color_pair->Set(kUnvisited, CreateBaseInterpolableColor(unvisited_color));
  color_pair->Set(kVisited, CreateBaseInterpolableColor(visited_color));
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
  auto& other_list = To<InterpolableList>(*value.interpolable_value);
  // Both lists should have kUnvisited and kVisited.
  DCHECK(underlying_list.length() == kInterpolableColorPairIndexCount);
  DCHECK(other_list.length() == kInterpolableColorPairIndexCount);
  EnsureCompatibleInterpolableColorTypes(underlying_list, other_list);
  for (wtf_size_t i = 0; i < underlying_list.length(); i++) {
    auto& underlying =
        To<BaseInterpolableColor>(*underlying_list.GetMutable(i));
    auto& other = To<BaseInterpolableColor>(*other_list.Get(i));
    DCHECK_EQ(underlying.IsStyleColor(), other.IsStyleColor());
    underlying.Composite(other, underlying_fraction);
  }
}

}  // namespace blink
