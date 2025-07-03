// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_gap_length_list_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/css_length_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/length_list_property_functions.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/style/gap_data_list.h"

namespace blink {

InterpolationValue
CSSGapLengthListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  auto list = GetProperty(style);
  const auto& values = list.GetGapDataList();
  return ListInterpolationFunctions::CreateList(
      values.size(), [this, &values, &style](wtf_size_t i) {
        if (values[i].IsRepeaterData()) {
          return InterpolationValue(nullptr);
        }
        return InterpolationValue(InterpolableLength::MaybeConvertLength(
            Length(values[i].GetValue(), Length::Type::kFixed), CssProperty(),
            style.EffectiveZoom(), std::nullopt));
      });
}

void CSSGapLengthListInterpolationType::Composite(
    UnderlyingValueOwner& owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  ListInterpolationFunctions::Composite(
      owner, underlying_fraction, this, value,
      ListInterpolationFunctions::LengthMatchingStrategy::kLowestCommonMultiple,
      ListInterpolationFunctions::InterpolableValuesKnownCompatible,
      ListInterpolationFunctions::VerifyNoNonInterpolableValues,
      [](UnderlyingValue& underlying_value, double fraction,
         const InterpolableValue& interpolable_value,
         const NonInterpolableValue*) {
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
  Vector<Length> result(length);
  for (wtf_size_t i = 0; i < length; i++) {
    result[i] =
        To<InterpolableLength>(*interpolable_list.Get(i))
            .CreateLength(
                state.CssToLengthConversionData(),
                LengthListPropertyFunctions::GetValueRange(CssProperty()));
  }
  if (property_id_ == CSSPropertyID::kColumnRuleWidth) {
    state.StyleBuilder().SetColumnRuleWidth(GapDataList<int>(result));
  } else {
    CHECK_EQ(property_id_, CSSPropertyID::kRowRuleWidth);
    state.StyleBuilder().SetRowRuleWidth(GapDataList<int>(result));
  }
}

void CSSGapLengthListInterpolationType::GetList(const CSSProperty& property,
                                                const ComputedStyle& style,
                                                Vector<int>& result) {
  auto gap_list = property.PropertyID() == CSSPropertyID::kColumnRuleWidth
                      ? style.ColumnRuleWidth()
                      : style.RowRuleWidth();
  for (const auto& gap_data : gap_list.GetGapDataList()) {
    result.push_back(gap_data.GetValue());
  }
}

InterpolationValue CSSGapLengthListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  wtf_size_t underlying_length =
      UnderlyingLengthChecker::GetUnderlyingLength(underlying);
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingLengthChecker>(underlying_length));
  if (underlying_length == 0) {
    return nullptr;
  }
  return ListInterpolationFunctions::CreateList(
      underlying_length, [](wtf_size_t) {
        return InterpolationValue(InterpolableLength::CreateNeutral());
      });
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
                                const Vector<int>& inherited_list)
      : property_(property), inherited_list_(inherited_list) {}
  ~InheritedGapLengthListChecker() final = default;

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    Vector<int> inherited_list;
    CSSGapLengthListInterpolationType::GetList(property_, *state.ParentStyle(),
                                               inherited_list);
    return inherited_list_ == inherited_list;
  }

  const CSSProperty& property_;
  Vector<int> inherited_list_;
};

InterpolationValue CSSGapLengthListInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  Vector<int> inherited_list;
  GetList(CssProperty(), *state.ParentStyle(), inherited_list);
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedGapLengthListChecker>(CssProperty(),
                                                          inherited_list));
  if (inherited_list.empty()) {
    return nullptr;
  }
  return ListInterpolationFunctions::CreateList(
      inherited_list.size(), [this, &inherited_list](wtf_size_t index) {
        return InterpolationValue(InterpolableLength::MaybeConvertLength(
            Length(inherited_list[index], Length::Type::kFixed), CssProperty(),
            /*zoom=*/1,
            /*interpolate_size=*/std::nullopt));
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
  return ListInterpolationFunctions::CreateList(
      list.length(), [this, &list](wtf_size_t index) {
        if (auto* id = DynamicTo<CSSIdentifierValue>(list.Item(index))) {
          double pixels;
          if (LengthPropertyFunctions::GetPixelsForKeyword(
                  CssProperty(), id->GetValueID(), pixels)) {
            return InterpolationValue(InterpolableLength::CreatePixels(pixels));
          }
          return InterpolationValue(nullptr);
        }
        return InterpolationValue(
            InterpolableLength::MaybeConvertCSSValue(list.Item(index)));
      });
}

PairwiseInterpolationValue CSSGapLengthListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  // TODO(crbug.com/357648037): Once repeaters are supported, adjust logic
  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end),
      ListInterpolationFunctions::LengthMatchingStrategy::kLowestCommonMultiple,
      [](InterpolationValue&& start_item, InterpolationValue&& end_item) {
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
