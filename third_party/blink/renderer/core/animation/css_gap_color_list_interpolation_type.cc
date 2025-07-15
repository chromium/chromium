// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_gap_color_list_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/color_property_functions.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_color.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/style/gap_data_list.h"

namespace blink {

class UnderlyingGapColorListChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingGapColorListChecker(const InterpolationValue& underlying)
      : underlying_(MakeGarbageCollected<InterpolationValueGCed>(underlying)) {}
  ~UnderlyingGapColorListChecker() final = default;

  void Trace(Visitor* visitor) const final {
    InterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(underlying_);
  }

 private:
  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return To<InterpolableList>(*underlying_->underlying().interpolable_value)
        .Equals(To<InterpolableList>(*underlying.interpolable_value));
  }

  const Member<const InterpolationValueGCed> underlying_;
};

InterpolationValue
CSSGapColorListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  GapDataList<StyleColor> list = GetProperty(style);
  const GapDataList<StyleColor>::GapDataVector& values = list.GetGapDataList();
  return ListInterpolationFunctions::CreateList(
      values.size(), [this, &style, &values](wtf_size_t i) {
        // TODO(javiercon): Handle the case where the gap data is a repeater.
        if (values[i].IsRepeaterData()) {
          return InterpolationValue(nullptr);
        }

        return InterpolationValue(
            CSSColorInterpolationType::CreateBaseInterpolableColor(
                ColorPropertyFunctions::GetUnvisitedColor(CssProperty(), style)
                    .value(),
                style.UsedColorScheme(),
                /*color_provider=*/nullptr));
      });
}

void CSSGapColorListInterpolationType::Composite(
    UnderlyingValueOwner& owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  ListInterpolationFunctions::Composite(
      owner, underlying_fraction, this, value,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      ListInterpolationFunctions::InterpolableValuesKnownCompatible,
      ListInterpolationFunctions::VerifyNoNonInterpolableValues,
      [](UnderlyingValue& underlying_value, double fraction,
         const InterpolableValue& interpolable_value,
         const NonInterpolableValue*) {
        auto& underlying = To<BaseInterpolableColor>(
            underlying_value.MutableInterpolableValue());
        auto& other = To<BaseInterpolableColor>(interpolable_value);
        underlying.Composite(other, fraction);
      });
}

void CSSGapColorListInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const auto& interpolable_list = To<InterpolableList>(interpolable_value);
  const wtf_size_t length = interpolable_list.length();
  DCHECK_GT(length, 0U);
  const auto& non_interpolable_list =
      To<NonInterpolableList>(*non_interpolable_value);
  DCHECK_EQ(non_interpolable_list.length(), length);
  GapDataList<StyleColor> result(length);
  for (wtf_size_t i = 0; i < length; i++) {
    // TODO(javiercon): Handle the case where the gap data is a repeater.
    result.AddGapData(
        StyleColor(CSSColorInterpolationType::ResolveInterpolableColor(
            To<InterpolableColor>(*interpolable_list.Get(i)), state,
            /*is_visited=*/false,
            /*is_text_decoration=*/false)));
  }

  if (property_id_ == CSSPropertyID::kColumnRuleColor) {
    state.StyleBuilder().SetColumnRuleColor(result);
  } else {
    CHECK_EQ(property_id_, CSSPropertyID::kRowRuleColor);
    state.StyleBuilder().SetRowRuleColor(result);
  }
}

GapDataList<StyleColor> CSSGapColorListInterpolationType::GetList(
    const CSSProperty& property,
    const ComputedStyle& style) {
  CHECK(property.PropertyID() == CSSPropertyID::kColumnRuleColor ||
        property.PropertyID() == CSSPropertyID::kRowRuleColor);
  return property.PropertyID() == CSSPropertyID::kColumnRuleColor
             ? style.ColumnRuleColor()
             : style.RowRuleColor();
}

InterpolationValue CSSGapColorListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingGapColorListChecker>(underlying));
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue CSSGapColorListInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  HeapVector<StyleColor, 1> initial_list;
  GetInitialStyleColorList(
      CssProperty(), state.GetDocument().GetStyleResolver().InitialStyle(),
      initial_list);

  mojom::blink::ColorScheme color_scheme =
      state.StyleBuilder().UsedColorScheme();
  const ui::ColorProvider* color_provider =
      state.GetDocument().GetColorProviderForPainting(color_scheme);

  return ListInterpolationFunctions::CreateList(
      initial_list.size(),
      [&initial_list, &color_scheme, &color_provider](wtf_size_t index) {
        return InterpolationValue(
            CSSColorInterpolationType::CreateBaseInterpolableColor(
                initial_list[index], color_scheme, color_provider));
      });
}

class InheritedGapColorListChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedGapColorListChecker(const CSSProperty& property,
                               const GapDataList<StyleColor>& inherited_list)
      : property_(property), inherited_list_(inherited_list) {}
  ~InheritedGapColorListChecker() final = default;

  void Trace(Visitor* visitor) const final {
    InterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(inherited_list_);
  }

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    GapDataList<StyleColor> inherited_list =
        CSSGapColorListInterpolationType::GetList(property_,
                                                  *state.ParentStyle());
    return inherited_list_ == inherited_list;
  }

  const CSSProperty& property_;
  GapDataList<StyleColor> inherited_list_;
};

InterpolationValue CSSGapColorListInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return nullptr;
  }

  GapDataList<StyleColor> inherited_list =
      GetList(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedGapColorListChecker>(CssProperty(),
                                                         inherited_list));

  const GapDataList<StyleColor>::GapDataVector& inherited_gap_data_vector =
      inherited_list.GetGapDataList();

  if (inherited_gap_data_vector.empty()) {
    return nullptr;
  }

  mojom::blink::ColorScheme color_scheme =
      state.StyleBuilder().UsedColorScheme();
  const ui::ColorProvider* color_provider =
      state.GetDocument().GetColorProviderForPainting(color_scheme);

  return ListInterpolationFunctions::CreateList(
      inherited_gap_data_vector.size(),
      [&inherited_gap_data_vector, &color_scheme,
       &color_provider](wtf_size_t index) {
        return InterpolationValue(
            CSSColorInterpolationType::CreateBaseInterpolableColor(
                inherited_gap_data_vector[index].GetValue(), color_scheme,
                color_provider));
      });
}

InterpolationValue CSSGapColorListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const auto* list = DynamicTo<CSSValueList>(value);
  wtf_size_t length = list ? list->length() : 1;

  return ListInterpolationFunctions::CreateList(
      length, [list, &state, &value](wtf_size_t index) {
        const CSSValue& element = list ? list->Item(index) : value;

        InterpolableColor* interpolable_color =
            CSSColorInterpolationType::MaybeCreateInterpolableColor(element,
                                                                    &state);
        if (!interpolable_color) {
          return InterpolationValue(nullptr);
        }

        return InterpolationValue(interpolable_color);
      });
}

PairwiseInterpolationValue CSSGapColorListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  // TODO(crbug.com/357648037): Once repeaters are supported, adjust logic
  InterpolableList& start_list =
      To<InterpolableList>(*start.interpolable_value);
  InterpolableList& end_list = To<InterpolableList>(*end.interpolable_value);
  if (start_list.length() != end_list.length()) {
    // If the lists are not compatible, return an empty
    // PairwiseInterpolationValue.
    return PairwiseInterpolationValue(nullptr);
  }

  CSSColorInterpolationType::EnsureCompatibleInterpolableColorTypes(start_list,
                                                                    end_list);

  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      [](InterpolationValue&& start_item, InterpolationValue&& end_item) {
        InterpolableColor& start_color =
            To<InterpolableColor>(*start_item.interpolable_value);
        InterpolableColor& end_color =
            To<InterpolableColor>(*end_item.interpolable_value);
        // Confirm that both colors are in the same colorspace and adjust if
        // necessary.
        InterpolableColor::SetupColorInterpolationSpaces(start_color,
                                                         end_color);
        return PairwiseInterpolationValue(
            std::move(start_item.interpolable_value),
            std::move(end_item.interpolable_value));
      });
}

GapDataList<StyleColor> CSSGapColorListInterpolationType::GetProperty(
    const ComputedStyle& style) const {
  if (property_id_ == CSSPropertyID::kColumnRuleColor) {
    return style.ColumnRuleColor();
  }
  CHECK(property_id_ == CSSPropertyID::kRowRuleColor);
  return style.RowRuleColor();
}

void CSSGapColorListInterpolationType::GetInitialStyleColorList(
    const CSSProperty& property,
    const ComputedStyle& style,
    HeapVector<StyleColor, 1>& result) const {
  CHECK(property_id_ == CSSPropertyID::kColumnRuleColor ||
        property_id_ == CSSPropertyID::kRowRuleColor);
  OptionalStyleColor initial_color =
      ColorPropertyFunctions::GetInitialColor(CssProperty(), style);
  if (!initial_color.has_value()) {
    return;
  }
  result.push_back(initial_color.value());
}

}  // namespace blink
