// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_text_indent_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CSSTextIndentNonInterpolableValue : public NonInterpolableValue {
 public:
  static scoped_refptr<CSSTextIndentNonInterpolableValue> Create(
      scoped_refptr<const NonInterpolableValue> length_non_interpolable_value) {
    return base::AdoptRef(new CSSTextIndentNonInterpolableValue(
        std::move(length_non_interpolable_value)));
  }

  const NonInterpolableValue* LengthNonInterpolableValue() const {
    return length_non_interpolable_value_.get();
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  explicit CSSTextIndentNonInterpolableValue(
      scoped_refptr<const NonInterpolableValue> length_non_interpolable_value)
      : length_non_interpolable_value_(
            std::move(length_non_interpolable_value)) {}

  scoped_refptr<const NonInterpolableValue> length_non_interpolable_value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSTextIndentNonInterpolableValue);
template <>
struct DowncastTraits<CSSTextIndentNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == CSSTextIndentNonInterpolableValue::static_type_;
  }
};

namespace {

class InheritedIndentChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedIndentChecker(const Length& length) : length_(length) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return length_ == state.ParentStyle()->TextIndent();
  }

 private:
  const Length length_;
};

InterpolationValue CreateValue(const Length& length,
                               const CSSProperty& property,
                               double zoom) {
  InterpolationValue converted_length(InterpolableLength::MaybeConvertLength(
      length, property, zoom, /*interpolate_size=*/std::nullopt));
  DCHECK(converted_length);
  return InterpolationValue(std::move(converted_length.interpolable_value),
                            CSSTextIndentNonInterpolableValue::Create(std::move(
                                converted_length.non_interpolable_value)));
}

}  // namespace

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  return CreateValue(Length::Fixed(0), CssProperty(), 1);
}

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return CreateValue(ComputedStyleInitialValues::InitialTextIndent(),
                     CssProperty(), 1);
}

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const ComputedStyle& parent_style = *state.ParentStyle();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedIndentChecker>(parent_style.TextIndent()));
  return CreateValue(parent_style.TextIndent(), CssProperty(),
                     parent_style.EffectiveZoom());
}

InterpolationValue CSSTextIndentInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  InterpolationValue length = nullptr;

  for (const auto& item : To<CSSValueList>(value)) {
    length =
        InterpolationValue(InterpolableLength::MaybeConvertCSSValue(*item));
  }
  DCHECK(length);

  return InterpolationValue(std::move(length.interpolable_value),
                            CSSTextIndentNonInterpolableValue::Create(
                                std::move(length.non_interpolable_value)));
}

InterpolationValue
CSSTextIndentInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateValue(style.TextIndent(), CssProperty(), style.EffectiveZoom());
}

PairwiseInterpolationValue CSSTextIndentInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  PairwiseInterpolationValue result = InterpolableLength::MaybeMergeSingles(
      std::move(start.interpolable_value), std::move(end.interpolable_value));
  result.non_interpolable_value = CSSTextIndentNonInterpolableValue::Create(
      std::move(result.non_interpolable_value));
  return result;
}

void CSSTextIndentInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.MutableInterpolableValue().ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
}

void CSSTextIndentInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  state.StyleBuilder().SetTextIndent(
      To<InterpolableLength>(interpolable_value)
          .CreateLength(state.CssToLengthConversionData(),
                        Length::ValueRange::kAll));
}

}  // namespace blink
