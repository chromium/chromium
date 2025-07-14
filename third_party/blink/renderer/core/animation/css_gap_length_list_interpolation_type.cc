// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_gap_length_list_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/css_length_list_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_gap_data_repeater.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/length_list_property_functions.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/style/gap_data_list.h"

namespace blink {

namespace {

InterpolationValue GetInterpolationValueFromGapData(
    const GapData<int>& data,
    const CSSProperty& property,
    float zoom,
    const CSSValue* value = nullptr) {
  if (data.IsRepeaterData()) {
    return InterpolationValue(InterpolableGapLengthRepeater::Create(
        data.GetValueRepeater(), property, zoom));
  }

  if (value) {
    if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
      // For the case where the value is "thin", "medium", or "thick".
      double pixels;
      if (LengthPropertyFunctions::GetPixelsForKeyword(
              property, identifier_value->GetValueID(), pixels)) {
        return InterpolationValue(InterpolableLength::CreatePixels(pixels));
      }

      return InterpolationValue(nullptr);
    }
    return InterpolationValue(InterpolableLength::MaybeConvertCSSValue(*value));
  }
  return InterpolationValue(InterpolableLength::MaybeConvertLength(
      Length(data.GetValue(), Length::Type::kFixed), property, zoom,
      /*interpolate_size=*/std::nullopt));
}

bool IsCompatible(const InterpolableValue& a, const InterpolableValue& b) {
  if (a.IsGapLengthRepeater() != b.IsGapLengthRepeater()) {
    return false;
  }
  if (!a.IsGapLengthRepeater()) {
    return true;  // lengths are compatible.
  }
  return To<InterpolableGapLengthRepeater>(a).IsCompatibleWith(
      To<InterpolableGapLengthRepeater>(b));
}

}  // namespace

class UnderlyingGapDataListChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingGapDataListChecker(const InterpolationValue& underlying)
      : underlying_(MakeGarbageCollected<InterpolationValueGCed>(underlying)) {}
  ~UnderlyingGapDataListChecker() final = default;

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
CSSGapLengthListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  GapDataList<int> list = GetProperty(style);
  const GapDataList<int>::GapDataVector& values = list.GetGapDataList();

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
  ListInterpolationFunctions::Composite(
      owner, underlying_fraction, this, value,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      ListInterpolationFunctions::InterpolableValuesKnownCompatible,
      ListInterpolationFunctions::VerifyNoNonInterpolableValues,
      [this, &owner, &value](UnderlyingValue& underlying_value, double fraction,
                             const InterpolableValue& interpolable_value,
                             const NonInterpolableValue*) {
        if (!IsCompatible(underlying_value.MutableInterpolableValue(),
                          interpolable_value)) {
          owner.Set(this, value);
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
    if (auto* repeater = DynamicTo<InterpolableGapLengthRepeater>(
            interpolable_list.Get(i))) {
      result.AddGapData(repeater->CreateGapData(
          state.CssToLengthConversionData(),
          LengthListPropertyFunctions::GetValueRange(CssProperty())));
      continue;
    }

    result.AddGapData(
        To<InterpolableLength>(*interpolable_list.Get(i))
            .CreateLength(
                state.CssToLengthConversionData(),
                LengthListPropertyFunctions::GetValueRange(CssProperty())));
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
      MakeGarbageCollected<UnderlyingGapDataListChecker>(underlying));
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
  GapDataList<int> inherited_list =
      GetList(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedGapLengthListChecker>(CssProperty(),
                                                          inherited_list));

  const GapDataList<int>::GapDataVector& inherited_gap_data_vector =
      inherited_list.GetGapDataList();

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

  GapDataList<int> gap_data_list =
      StyleBuilderConverter::ConvertGapDecorationWidthDataList(
          const_cast<StyleResolverState&>(state), value);
  const GapDataList<int>::GapDataVector& gap_data_vector =
      gap_data_list.GetGapDataList();
  return ListInterpolationFunctions::CreateList(
      gap_data_vector.size(),
      [this, &list, &gap_data_vector](wtf_size_t index) {
        return GetInterpolationValueFromGapData(gap_data_vector[index],
                                                CssProperty(), /*zoom=*/1,
                                                &list.Item(index));
      });
}

PairwiseInterpolationValue CSSGapLengthListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      [](InterpolationValue&& start_item, InterpolationValue&& end_item) {
        if (!IsCompatible(*start_item.interpolable_value,
                          *end_item.interpolable_value)) {
          return PairwiseInterpolationValue(nullptr);
        }

        if (start_item.interpolable_value->IsGapDataRepeater()) {
          return PairwiseInterpolationValue(
              std::move(start_item.interpolable_value),
              std::move(end_item.interpolable_value));
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
