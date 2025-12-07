// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_gap_color_list_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/color_property_functions.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/gap_data_list_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/interpolable_color.h"
#include "third_party/blink/renderer/core/animation/interpolable_gap_data_auto_repeater.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"
#include "third_party/blink/renderer/core/css/css_repeat_value.h"
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
    const StyleResolverState* state = nullptr) {
  CHECK(style);
  if (data.IsRepeaterData()) {
    // At this stage, the GapData list should be fully expanded, so any
    // remaining repeater must be an auto repeater.
    CHECK(data.GetValueRepeater()->IsAutoRepeater());
    return InterpolationValue(InterpolableGapColorAutoRepeater::Create(
        data.GetValueRepeater(), *style));
  }

  return InterpolationValue(
      CSSColorInterpolationType::CreateBaseInterpolableColor(
          data.GetValue(), style->UsedColorScheme(), color_provider));
}

InterpolationValue GetInterpolationValueFromCSSValue(
    const CSSValue* value,
    const CSSProperty& property,
    const StyleResolverState& state,
    const ComputedStyle* style) {
  CHECK(value);
  CHECK(style);
  if (auto* gap_repeat_value = DynamicTo<cssvalue::CSSRepeatValue>(value)) {
    CHECK(gap_repeat_value->IsAutoRepeatValue());
    typename ValueRepeater<StyleColor>::VectorType gap_values;
    gap_values.ReserveInitialCapacity(gap_repeat_value->Values().length());

    for (const auto& repeated_value : gap_repeat_value->Values()) {
      gap_values.push_back(StyleBuilderConverter::ConvertStyleColor(
          state, *repeated_value, /*for_visited_link*/ false));
    }
    ValueRepeater<StyleColor>* value_repeater =
        MakeGarbageCollected<ValueRepeater<StyleColor>>(
            std::move(gap_values), /*repeat_count=*/std::nullopt);
    return InterpolationValue(
        InterpolableGapColorAutoRepeater::Create(value_repeater, *style));
  }

  return InterpolationValue(
      CSSColorInterpolationType::MaybeCreateInterpolableColor(*value, &state));
}

bool IsCompatible(const InterpolableValue* a, const InterpolableValue* b) {
  if (a->IsGapColorAutoRepeater() != b->IsGapColorAutoRepeater()) {
    return false;
  }

  // If both are auto repeaters or both are singular colors, they are
  // compatible.
  return true;
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

    GapDataListInterpolationFunctions::GapSegmentsData underlying_segments =
        GapDataListInterpolationFunctions::CreateGapSegmentsData(
            underlying_list);
    GapDataListInterpolationFunctions::GapSegmentsData other_segments =
        GapDataListInterpolationFunctions::CreateGapSegmentsData(other_list);

    if (!GapDataListInterpolationFunctions::GapSegmentShapesMatch(
            underlying_segments, other_segments)) {
      return false;
    }

    ListInterpolationFunctions::LengthMatchingStrategy
        length_matching_strategy =
            underlying_segments.pattern == GapDataListInterpolationFunctions::
                                               GapDataListPattern::kSimple
                ? ListInterpolationFunctions::LengthMatchingStrategy::
                      kLowestCommonMultiple
                : ListInterpolationFunctions::LengthMatchingStrategy::kEqual;

    return ListInterpolationFunctions::InterpolableListsAreCompatible(
        underlying_list, other_list, underlying_list.length(),
        length_matching_strategy, &IsCompatible);
  }

  const Member<const InterpolationValueGCed> underlying_;
};

InterpolationValue
CSSGapColorListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  GapDataList<StyleColor> list = GetProperty(style);
  const GapDataList<StyleColor>::GapDataVector& values =
      CSSGapDecorationUtils::GetExpandedGapDataList(list);

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
  auto& underlying_list =
      To<InterpolableList>(*owner.Value().interpolable_value);
  auto& incoming_list = To<InterpolableList>(*value.interpolable_value);

  const GapDataListInterpolationFunctions::GapSegmentsData underlying_segments =
      GapDataListInterpolationFunctions::CreateGapSegmentsData(underlying_list);
  const GapDataListInterpolationFunctions::GapSegmentsData incoming_segments =
      GapDataListInterpolationFunctions::CreateGapSegmentsData(incoming_list);

  if (!GapDataListInterpolationFunctions::GapSegmentShapesMatch(
          underlying_segments, incoming_segments)) {
    owner.Set(this, value);
    return;
  }

  ListInterpolationFunctions::LengthMatchingStrategy length_matching_strategy =
      underlying_segments.pattern ==
              GapDataListInterpolationFunctions::GapDataListPattern::kSimple
          ? ListInterpolationFunctions::LengthMatchingStrategy::
                kLowestCommonMultiple
          : ListInterpolationFunctions::LengthMatchingStrategy::kEqual;

  ListInterpolationFunctions::Composite(
      owner, underlying_fraction, this, value, length_matching_strategy,
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

        if (underlying_value.MutableInterpolableValue()
                .IsGapColorAutoRepeater()) {
          To<InterpolableGapColorAutoRepeater>(
              underlying_value.MutableInterpolableValue())
              .Composite(
                  To<InterpolableGapColorAutoRepeater>(interpolable_value),
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
    if (auto* repeater = DynamicTo<InterpolableGapColorAutoRepeater>(
            interpolable_list.Get(i))) {
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
      CSSGapDecorationUtils::GetExpandedGapDataList(inherited_list);

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
  // When CSSGapDecorations feature is enabled, the `color` property might still
  // be represented as a single CSSValue instead of a CSSValueList. This can
  // happen when the properties are parsed via the fast parsing path rather than
  // the standard `ParseSingleValue()` method. In such cases, wrap the single
  // value in a list to ensure consistent handling.
  auto getValueAsList = [&](const CSSValue* value) -> const CSSValueList* {
    if (const CSSValueList* value_list = DynamicTo<CSSValueList>(value)) {
      return value_list;
    }
    CSSValueList* wrapper_list = CSSValueList::CreateSpaceSeparated();
    wrapper_list->Append(*value);
    return wrapper_list;
  };

  const CSSValueList* list = getValueAsList(&value);
  CHECK(list);

  const CSSValueList* expanded_list =
      CSSGapDecorationUtils::GetExpandedCSSValueListForGapData(*list, state);

  return ListInterpolationFunctions::CreateList(
      expanded_list->length(), [this, expanded_list, &state](wtf_size_t index) {
        return GetInterpolationValueFromCSSValue(&expanded_list->Item(index),
                                                 CssProperty(), state,
                                                 state.CloneStyle());
      });
}

PairwiseInterpolationValue CSSGapColorListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const GapDataListInterpolationFunctions::GapSegmentsData start_segment =
      GapDataListInterpolationFunctions::CreateGapSegmentsData(
          To<InterpolableList>(*start.interpolable_value));
  const GapDataListInterpolationFunctions::GapSegmentsData end_segment =
      GapDataListInterpolationFunctions::CreateGapSegmentsData(
          To<InterpolableList>(*end.interpolable_value));

  if (!GapDataListInterpolationFunctions::GapSegmentShapesMatch(start_segment,
                                                                end_segment)) {
    return PairwiseInterpolationValue(nullptr);
  }

  ListInterpolationFunctions::LengthMatchingStrategy strategy =
      start_segment.pattern ==
              GapDataListInterpolationFunctions::GapDataListPattern::kSimple
          ? ListInterpolationFunctions::LengthMatchingStrategy::
                kLowestCommonMultiple
          : ListInterpolationFunctions::LengthMatchingStrategy::kEqual;

  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end), strategy,
      [](InterpolationValue&& start_item, InterpolationValue&& end_item) {
        if (!IsCompatible(start_item.interpolable_value,
                          end_item.interpolable_value)) {
          return PairwiseInterpolationValue(nullptr);
        }

        if (start_item.interpolable_value->IsGapColorAutoRepeater()) {
          auto& start_list = To<InterpolableGapColorAutoRepeater>(
              *start_item.interpolable_value);
          auto& end_list = To<InterpolableGapColorAutoRepeater>(
              *end_item.interpolable_value);

          // The inner lists in an auto repeater are allowed to be different
          // lengths, so we must align them with `kLowestCommonMultiple`.
          InterpolationValue inner_start(start_list.InnerValues());
          InterpolationValue inner_end(end_list.InnerValues());

          auto merged = ListInterpolationFunctions::MaybeMergeSingles(
              std::move(inner_start), std::move(inner_end),
              ListInterpolationFunctions::LengthMatchingStrategy::
                  kLowestCommonMultiple,
              [](InterpolationValue&& start, InterpolationValue&& end) {
                InterpolableValue* start_val = start.interpolable_value.Get();
                InterpolableValue* end_val = end.interpolable_value.Get();

                CSSColorInterpolationType::
                    EnsureCompatibleInterpolableColorTypes(start_val, end_val);

                start.interpolable_value = start_val;
                end.interpolable_value = end_val;

                InterpolableColor& start_color =
                    To<InterpolableColor>(*start.interpolable_value);
                InterpolableColor& end_color =
                    To<InterpolableColor>(*end.interpolable_value);
                // Confirm that both colors are in the same colorspace and
                // adjust if necessary.
                InterpolableColor::SetupColorInterpolationSpaces(start_color,
                                                                 end_color);
                return PairwiseInterpolationValue(
                    std::move(start.interpolable_value),
                    std::move(end.interpolable_value));
              });
          if (!merged) {
            return PairwiseInterpolationValue(nullptr);
          }

          // Re-wrap the aligned and merged inners back into auto repeater
          // nodes. Note that even though the repeater's inner values may have
          // expanded, the actual `repeater_` member will not have changed, and
          // thus may not actually match the values in `start_list`.
          auto* start_merged =
              InterpolableGapColorAutoRepeater::CreateFromMergedInner(
                  To<InterpolableList>(*merged.start_interpolable_value),
                  start_list);
          auto* end_merged =
              InterpolableGapColorAutoRepeater::CreateFromMergedInner(
                  To<InterpolableList>(*merged.end_interpolable_value),
                  end_list);

          return PairwiseInterpolationValue(start_merged, end_merged);
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
