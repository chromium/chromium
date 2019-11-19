// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_length_list_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/length_list_property_functions.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/underlying_length_checker.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

CSSLengthListInterpolationType::CSSLengthListInterpolationType(
    PropertyHandle property)
    : CSSInterpolationType(property),
      value_range_(LengthListPropertyFunctions::GetValueRange(CssProperty())) {}

InterpolationValue CSSLengthListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  wtf_size_t underlying_length =
      UnderlyingLengthChecker::GetUnderlyingLength(underlying);
  conversion_checkers.push_back(
      std::make_unique<UnderlyingLengthChecker>(underlying_length));

  if (underlying_length == 0)
    return nullptr;

  return ListInterpolationFunctions::CreateList(
      underlying_length, [](wtf_size_t) {
        return InterpolationValue(InterpolableLength::CreateNeutral());
      });
}

static InterpolationValue MaybeConvertLengthList(
    const Vector<Length>& length_list,
    float zoom) {
  if (length_list.IsEmpty())
    return nullptr;

  return ListInterpolationFunctions::CreateList(
      length_list.size(), [&length_list, zoom](wtf_size_t index) {
        return InterpolationValue(
            InterpolableLength::MaybeConvertLength(length_list[index], zoom));
      });
}

InterpolationValue CSSLengthListInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  Vector<Length> initial_length_list;
  if (!LengthListPropertyFunctions::GetInitialLengthList(CssProperty(),
                                                         initial_length_list))
    return nullptr;
  return MaybeConvertLengthList(initial_length_list, 1);
}

class InheritedLengthListChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedLengthListChecker(const CSSProperty& property,
                             const Vector<Length>& inherited_length_list)
      : property_(property), inherited_length_list_(inherited_length_list) {}
  ~InheritedLengthListChecker() final = default;

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    Vector<Length> inherited_length_list;
    LengthListPropertyFunctions::GetLengthList(property_, *state.ParentStyle(),
                                               inherited_length_list);
    return inherited_length_list_ == inherited_length_list;
  }

  const CSSProperty& property_;
  Vector<Length> inherited_length_list_;
};

InterpolationValue CSSLengthListInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  Vector<Length> inherited_length_list;
  bool success = LengthListPropertyFunctions::GetLengthList(
      CssProperty(), *state.ParentStyle(), inherited_length_list);
  conversion_checkers.push_back(std::make_unique<InheritedLengthListChecker>(
      CssProperty(), inherited_length_list));
  if (!success)
    return nullptr;
  return MaybeConvertLengthList(inherited_length_list,
                                state.ParentStyle()->EffectiveZoom());
}

InterpolationValue CSSLengthListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsBaseValueList())
    return nullptr;

  const auto& list = To<CSSValueList>(value);
  return ListInterpolationFunctions::CreateList(
      list.length(), [&list](wtf_size_t index) {
        return InterpolationValue(
            InterpolableLength::MaybeConvertCSSValue(list.Item(index)));
      });
}

PairwiseInterpolationValue CSSLengthListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end),
      ListInterpolationFunctions::LengthMatchingStrategy::kLowestCommonMultiple,
      WTF::BindRepeating(
          [](InterpolationValue&& start_item, InterpolationValue&& end_item) {
            return InterpolableLength::MergeSingles(
                std::move(start_item.interpolable_value),
                std::move(end_item.interpolable_value));
          }));
}

InterpolationValue
CSSLengthListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  Vector<Length> underlying_length_list;
  if (!LengthListPropertyFunctions::GetLengthList(CssProperty(), style,
                                                  underlying_length_list))
    return nullptr;
  return MaybeConvertLengthList(underlying_length_list, style.EffectiveZoom());
}

void CSSLengthListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  ListInterpolationFunctions::Composite(
      underlying_value_owner, underlying_fraction, *this, value,
      ListInterpolationFunctions::LengthMatchingStrategy::kLowestCommonMultiple,
      WTF::BindRepeating(
          ListInterpolationFunctions::InterpolableValuesKnownCompatible),
      WTF::BindRepeating(
          ListInterpolationFunctions::VerifyNoNonInterpolableValues),
      WTF::BindRepeating([](UnderlyingValue& underlying_value,
                            double underlying_fraction,
                            const InterpolableValue& interpolable_value,
                            const NonInterpolableValue*) {
        underlying_value.MutableInterpolableValue().ScaleAndAdd(
            underlying_fraction, interpolable_value);
      }));
}

void CSSLengthListInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  const wtf_size_t length = interpolable_list.length();
  DCHECK_GT(length, 0U);
  const NonInterpolableList& non_interpolable_list =
      ToNonInterpolableList(*non_interpolable_value);
  DCHECK_EQ(non_interpolable_list.length(), length);
  Vector<Length> result(length);
  for (wtf_size_t i = 0; i < length; i++) {
    result[i] =
        To<InterpolableLength>(*interpolable_list.Get(i))
            .CreateLength(state.CssToLengthConversionData(), value_range_);
  }
  LengthListPropertyFunctions::SetLengthList(CssProperty(), *state.Style(),
                                             std::move(result));
}

}  // namespace blink
