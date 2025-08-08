// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_gap_color_list_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/color_property_functions.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_color.h"
#include "third_party/blink/renderer/core/animation/interpolable_gap_data_repeater.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/style/gap_data_list.h"

namespace blink {

namespace {

InterpolationValue GetInterpolationValueFromGapData(
    const GapData<StyleColor>& data,
    const CSSProperty& property,
    const ComputedStyle* style,
    const ui::ColorProvider* color_provider = nullptr,
    const CSSValue* value = nullptr,
    const StyleResolverState* state = nullptr) {
  CHECK(style);
  if (data.IsRepeaterData()) {
    return InterpolationValue(
        InterpolableGapColorRepeater::Create(data.GetValueRepeater(), *style));
  }

  if (value) {
    CHECK(state);
    InterpolableColor* interpolable_color =
        CSSColorInterpolationType::MaybeCreateInterpolableColor(*value, state);
    if (!interpolable_color) {
      return InterpolationValue(nullptr);
    }

    return InterpolationValue(interpolable_color);
  }

  return InterpolationValue(
      CSSColorInterpolationType::CreateBaseInterpolableColor(
          data.GetValue(), style->UsedColorScheme(), color_provider));
}

bool IsCompatible(const InterpolableValue* a, const InterpolableValue* b) {
  if (a->IsGapColorRepeater() != b->IsGapColorRepeater()) {
    return false;
  }
  if (!a->IsGapColorRepeater()) {
    return true;  // colors are compatible.
  }
  return To<InterpolableGapColorRepeater>(*a).IsCompatibleWith(
      To<InterpolableGapColorRepeater>(*b));
}

}  // namespace

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
    auto& underlying_list =
        To<InterpolableList>(*underlying_->underlying().interpolable_value);
    auto& other_list = To<InterpolableList>(*underlying.interpolable_value);
    return ListInterpolationFunctions::InterpolableListsAreCompatible(
        underlying_list, other_list, underlying_list.length(),
        ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
        &IsCompatible);
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
        return GetInterpolationValueFromGapData(values[i], CssProperty(),
                                                &style);
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
      [this, &owner, &value](UnderlyingValue& underlying_value, double fraction,
                             const InterpolableValue& interpolable_value,
                             const NonInterpolableValue*) {
        if (!IsCompatible(&underlying_value.MutableInterpolableValue(),
                          &interpolable_value)) {
          owner.Set(this, value);
          return;
        }

        if (underlying_value.MutableInterpolableValue().IsGapColorRepeater()) {
          To<InterpolableGapColorRepeater>(
              underlying_value.MutableInterpolableValue())
              .Composite(To<InterpolableGapColorRepeater>(interpolable_value),
                         fraction);
          return;
        }

        auto& underlying_color = To<BaseInterpolableColor>(
            underlying_value.MutableInterpolableValue());
        auto& other_color = To<BaseInterpolableColor>(interpolable_value);

        underlying_color.Composite(other_color, fraction);
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
    if (auto* repeater =
            DynamicTo<InterpolableGapColorRepeater>(interpolable_list.Get(i))) {
      result.AddGapData(repeater->CreateGapData(state));
      continue;
    }

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
      [this, &inherited_gap_data_vector, &state,
       &color_provider](wtf_size_t index) {
        return GetInterpolationValueFromGapData(
            inherited_gap_data_vector[index], CssProperty(), state.CloneStyle(),
            color_provider);
      });
}

InterpolationValue CSSGapColorListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const auto* list = DynamicTo<CSSValueList>(value);
  wtf_size_t length = list ? list->length() : 1;

  GapDataList<StyleColor> gap_data_list =
      StyleBuilderConverter::ConvertGapDecorationColorDataList(state, value);
  const GapDataList<StyleColor>::GapDataVector& gap_data_vector =
      gap_data_list.GetGapDataList();
  CHECK_EQ(gap_data_vector.size(), length);
  return ListInterpolationFunctions::CreateList(
      length, [this, list, &state, &value, &gap_data_vector](wtf_size_t index) {
        const CSSValue& element = list ? list->Item(index) : value;
        const ComputedStyle* style = state.CloneStyle();
        CHECK(style);

        return GetInterpolationValueFromGapData(
            gap_data_vector[index], CssProperty(), style,
            /* color_provider= */ nullptr, &element, &state);
      });
}

PairwiseInterpolationValue CSSGapColorListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  InterpolableList& start_list =
      To<InterpolableList>(*start.interpolable_value);
  InterpolableList& end_list = To<InterpolableList>(*end.interpolable_value);
  if (start_list.length() != end_list.length()) {
    // If the lists are not compatible, return an empty
    // PairwiseInterpolationValue.
    return PairwiseInterpolationValue(nullptr);
  }

  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      [](InterpolationValue&& start_item, InterpolationValue&& end_item) {
        if (!IsCompatible(start_item.interpolable_value,
                          end_item.interpolable_value)) {
          return PairwiseInterpolationValue(nullptr);
        }

        if (start_item.interpolable_value->IsGapDataRepeater()) {
          return PairwiseInterpolationValue(
              std::move(start_item.interpolable_value),
              std::move(end_item.interpolable_value));
        }

        InterpolableValue* start_val = start_item.interpolable_value.Get();
        InterpolableValue* end_val = end_item.interpolable_value.Get();

        CSSColorInterpolationType::EnsureCompatibleInterpolableColorTypes(
            start_val, end_val);

        start_item.interpolable_value = start_val;
        end_item.interpolable_value = end_val;

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
