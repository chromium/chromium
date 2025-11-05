// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_gap_length_list_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/css_length_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/gap_data_list_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/interpolable_gap_data_auto_repeater.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/length_list_property_functions.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"
#include "third_party/blink/renderer/core/css/css_repeat_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/style/gap_data_list.h"

namespace blink {

namespace {

InterpolationValue GetInterpolationValueFromGapData(const GapData<int>& data,
                                                    const CSSProperty& property,
                                                    float zoom) {
  if (data.IsRepeaterData()) {
    return InterpolationValue(InterpolableGapLengthAutoRepeater::Create(
        data.GetValueRepeater(), property, zoom));
  }

  return InterpolationValue(InterpolableLength::MaybeConvertLength(
      Length(data.GetValue(), Length::Type::kFixed), property, zoom,
      /*interpolate_size=*/std::nullopt));
}

InterpolationValue GetInterpolationValueFromCSSValue(
    const CSSValue* value,
    const CSSProperty& property,
    float zoom,
    const StyleResolverState& state) {
  CHECK(value);
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    // For the case where the value is "thin", "medium", or "thick".
    double pixels;
    if (LengthPropertyFunctions::GetPixelsForKeyword(
            property, identifier_value->GetValueID(), pixels)) {
      return InterpolationValue(InterpolableLength::CreatePixels(pixels));
    }

    return InterpolationValue(nullptr);
  }

  if (auto* gap_repeat_value = DynamicTo<cssvalue::CSSRepeatValue>(value)) {
    typename ValueRepeater<int>::VectorType gap_values;
    gap_values.ReserveInitialCapacity(gap_repeat_value->Values().length());
    CHECK(gap_repeat_value->IsAutoRepeatValue());
    for (const auto& repeat_value : gap_repeat_value->Values()) {
      gap_values.push_back(ClampTo<uint16_t>(
          StyleBuilderConverter::ConvertBorderWidth(state, *repeat_value)));
    }
    ValueRepeater<int>* value_repeater =
        MakeGarbageCollected<ValueRepeater<int>>(std::move(gap_values),
                                                 /*repeat_count=*/std::nullopt);
    GapData<int> gap_data = GapData<int>(value_repeater);
    return InterpolationValue(InterpolableGapLengthAutoRepeater::Create(
        gap_data.GetValueRepeater(), property, zoom));
  }
  return InterpolationValue(InterpolableLength::MaybeConvertCSSValue(*value));
}

bool IsCompatible(const InterpolableValue* a, const InterpolableValue* b) {
  if (a->IsGapLengthAutoRepeater() != b->IsGapLengthAutoRepeater()) {
    return false;
  }

  // If both are auto repeaters or both are lengths, they are compatible.
  return true;
}

}  // namespace

class UnderlyingGapLengthListChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingGapLengthListChecker(const InterpolationValue& underlying)
      : underlying_(MakeGarbageCollected<InterpolationValueGCed>(underlying)) {}
  ~UnderlyingGapLengthListChecker() final = default;

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
CSSGapLengthListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  GapDataList<int> list = GetProperty(style);
  GapDataList<int>::GapDataVector values =
      CSSGapDecorationUtils::GetExpandedGapDataList(list);

  return ListInterpolationFunctions::CreateList(
      values.size(), [this, &values, &style](wtf_size_t i) {
        return GetInterpolationValueFromGapData(values[i], CssProperty(),
                                                style.EffectiveZoom());
      });
}

void CSSGapLengthListInterpolationType::Composite(
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
                .IsGapLengthAutoRepeater()) {
          To<InterpolableGapLengthAutoRepeater>(
              underlying_value.MutableInterpolableValue())
              .Composite(
                  To<InterpolableGapLengthAutoRepeater>(interpolable_value),
                  fraction);
          return;
        }

        underlying_value.MutableInterpolableValue().ScaleAndAdd(
            fraction, interpolable_value);
      });
}

void CSSGapLengthListInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const auto& interpolable_list = To<InterpolableList>(interpolable_value);
  const wtf_size_t length = interpolable_list.length();
  DCHECK_GT(length, 0U);
  const auto& non_interpolable_list =
      To<NonInterpolableList>(*non_interpolable_value);
  DCHECK_EQ(non_interpolable_list.length(), length);
  GapDataList<int> result(length);
  for (wtf_size_t i = 0; i < length; i++) {
    Length::ValueRange value_range =
        LengthListPropertyFunctions::GetValueRange(CssProperty());
    if (auto* repeater = DynamicTo<InterpolableGapLengthAutoRepeater>(
            interpolable_list.Get(i))) {
      result.AddGapData(repeater->CreateGapData(
          state.CssToLengthConversionData(), value_range));
      continue;
    }

    result.AddGapData(
        To<InterpolableLength>(*interpolable_list.Get(i))
            .CreateLength(state.CssToLengthConversionData(), value_range));
  }

  if (CssProperty().PropertyID() == CSSPropertyID::kColumnRuleWidth) {
    state.StyleBuilder().SetColumnRuleWidth(result);
  } else {
    CHECK_EQ(property_id_, CSSPropertyID::kRowRuleWidth);
    state.StyleBuilder().SetRowRuleWidth(result);
  }
}

GapDataList<int> CSSGapLengthListInterpolationType::GetList(
    const CSSProperty& property,
    const ComputedStyle& style) {
  CHECK(property.PropertyID() == CSSPropertyID::kColumnRuleWidth ||
        property.PropertyID() == CSSPropertyID::kRowRuleWidth);
  return property.PropertyID() == CSSPropertyID::kColumnRuleWidth
             ? style.ColumnRuleWidth()
             : style.RowRuleWidth();
}

InterpolationValue CSSGapLengthListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingGapLengthListChecker>(underlying));
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue CSSGapLengthListInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  Vector<Length> initial_list;
  GetInitialLengthList(CssProperty(),
                       state.GetDocument().GetStyleResolver().InitialStyle(),
                       initial_list);
  return ListInterpolationFunctions::CreateList(
      initial_list.size(), [this, &initial_list](wtf_size_t index) {
        return InterpolationValue(InterpolableLength::MaybeConvertLength(
            initial_list[index], CssProperty(), /*zoom=*/1,
            /*interpolate_size=*/std::nullopt));
      });
}

class InheritedGapLengthListChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedGapLengthListChecker(const CSSProperty& property,
                                const GapDataList<int>& inherited_list)
      : property_(property), inherited_list_(inherited_list) {}
  ~InheritedGapLengthListChecker() final = default;

  void Trace(Visitor* visitor) const final {
    InterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(inherited_list_);
  }

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    GapDataList<int> inherited_list =
        CSSGapLengthListInterpolationType::GetList(property_,
                                                   *state.ParentStyle());
    return inherited_list_ == inherited_list;
  }

  const CSSProperty& property_;
  GapDataList<int> inherited_list_;
};

InterpolationValue CSSGapLengthListInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return nullptr;
  }

  GapDataList<int> inherited_list =
      GetList(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedGapLengthListChecker>(CssProperty(),
                                                          inherited_list));

  GapDataList<int>::GapDataVector inherited_gap_data_vector =
      CSSGapDecorationUtils::GetExpandedGapDataList(inherited_list);

  if (inherited_gap_data_vector.empty()) {
    return nullptr;
  }

  return ListInterpolationFunctions::CreateList(
      inherited_gap_data_vector.size(),
      [this, &inherited_gap_data_vector](wtf_size_t index) {
        return GetInterpolationValueFromGapData(
            inherited_gap_data_vector[index], CssProperty(), /*zoom=*/1);
      });
}

InterpolationValue CSSGapLengthListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!value.IsBaseValueList()) {
    return nullptr;
  }

  const auto& list = To<CSSValueList>(value);
  const CSSValueList* expanded_list =
      CSSGapDecorationUtils::GetExpandedCSSValueListForGapData(list, state);

  return ListInterpolationFunctions::CreateList(
      expanded_list->length(), [this, expanded_list, &state](wtf_size_t index) {
        return GetInterpolationValueFromCSSValue(&expanded_list->Item(index),
                                                 CssProperty(),
                                                 /*zoom=*/1, state);
      });
}

PairwiseInterpolationValue CSSGapLengthListInterpolationType::MaybeMergeSingles(
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

        if (start_item.interpolable_value->IsGapLengthAutoRepeater()) {
          auto& start_list = To<InterpolableGapLengthAutoRepeater>(
              *start_item.interpolable_value);
          auto& end_list = To<InterpolableGapLengthAutoRepeater>(
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
                return InterpolableLength::MaybeMergeSingles(
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
              InterpolableGapLengthAutoRepeater::CreateFromMergedInner(
                  To<InterpolableList>(*merged.start_interpolable_value),
                  start_list);
          auto* end_merged =
              InterpolableGapLengthAutoRepeater::CreateFromMergedInner(
                  To<InterpolableList>(*merged.end_interpolable_value),
                  end_list);

          return PairwiseInterpolationValue(start_merged, end_merged);
        }

        return InterpolableLength::MaybeMergeSingles(
            std::move(start_item.interpolable_value),
            std::move(end_item.interpolable_value));
      });
}

GapDataList<int> CSSGapLengthListInterpolationType::GetProperty(
    const ComputedStyle& style) const {
  if (property_id_ == CSSPropertyID::kColumnRuleWidth) {
    return style.ColumnRuleWidth();
  }
  return style.RowRuleWidth();
}

void CSSGapLengthListInterpolationType::GetInitialLengthList(
    const CSSProperty& property,
    const ComputedStyle& style,
    Vector<Length>& result) const {
  Length initial_length;
  LengthPropertyFunctions::GetInitialLength(property, style, initial_length);
  result.push_back(initial_length);
}

}  // namespace blink
