// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_text_decoration_inset_interpolation_type.h"

#include <utility>

#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/text_decoration_inset.h"
#include "third_party/blink/renderer/core/style/text_decoration_thickness.h"
#include "third_party/blink/renderer/platform/fonts/font.h"

namespace blink {

class CSSTextDecorationInsetAutoValue final : public NonInterpolableValue {
 public:
  ~CSSTextDecorationInsetAutoValue() final = default;

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSTextDecorationInsetAutoValue);
template <>
struct DowncastTraits<CSSTextDecorationInsetAutoValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == CSSTextDecorationInsetAutoValue::static_type_;
  }
};

namespace {

constexpr wtf_size_t kStartIndex = 0;
constexpr wtf_size_t kEndIndex = 1;
constexpr wtf_size_t kAutoContributionIndex = 2;
constexpr wtf_size_t kComponentCount = 3;

template <typename Style>
float ResolveAutoInsetForAnimation(const Style& style) {
  const Font& font = *style.GetFont();
  const float font_size = font.GetFontDescription().ComputedSize();
  const float thickness =
      style.GetTextDecorationThickness().Resolve(font_size, font.PrimaryFont());
  return TextDecorationInset::ResolveAutoInset(thickness) /
         style.EffectiveZoom();
}

const CSSTextDecorationInsetAutoValue* CreateAutoValue() {
  return MakeGarbageCollected<CSSTextDecorationInsetAutoValue>();
}

bool IsAuto(const NonInterpolableValue* value) {
  return DynamicTo<CSSTextDecorationInsetAutoValue>(value);
}

bool IsAuto(const InterpolationValue& value) {
  return IsAuto(value.non_interpolable_value);
}

InterpolationValue CreateInsetValue(const TextDecorationInset& inset,
                                    const CSSProperty& property,
                                    float zoom) {
  auto* result = MakeGarbageCollected<InterpolableList>(kComponentCount);
  DCHECK(!inset.GetStart().IsAuto());
  result->Set(kStartIndex, InterpolableLength::MaybeConvertLength(
                               inset.GetStart(), property, zoom,
                               /*interpolate_size=*/std::nullopt));
  result->Set(kEndIndex, InterpolableLength::MaybeConvertLength(
                             inset.GetEnd(), property, zoom,
                             /*interpolate_size=*/std::nullopt));
  result->Set(kAutoContributionIndex,
              MakeGarbageCollected<InterpolableNumber>(0));
  return InterpolationValue(result);
}

InterpolationValue CreateInsetValue(const TextDecorationInset& inset,
                                    const CSSProperty& property,
                                    const ComputedStyle& style) {
  if (!inset.GetStart().IsAuto()) {
    return CreateInsetValue(inset, property, style.EffectiveZoom());
  }

  auto* result = MakeGarbageCollected<InterpolableList>(kComponentCount);
  result->Set(kStartIndex, InterpolableLength::CreateNeutral());
  result->Set(kEndIndex, InterpolableLength::CreateNeutral());
  result->Set(kAutoContributionIndex,
              MakeGarbageCollected<InterpolableNumber>(1));
  return InterpolationValue(result, CreateAutoValue());
}

InterpolationValue ConvertCSSValue(const CSSValue& value) {
  auto* result = MakeGarbageCollected<InterpolableList>(kComponentCount);
  if (const auto* identifier = DynamicTo<CSSIdentifierValue>(value)) {
    if (identifier->GetValueID() != CSSValueID::kAuto) {
      return nullptr;
    }
    result->Set(kStartIndex, InterpolableLength::CreateNeutral());
    result->Set(kEndIndex, InterpolableLength::CreateNeutral());
    result->Set(kAutoContributionIndex,
                MakeGarbageCollected<InterpolableNumber>(1));
    return InterpolationValue(result, CreateAutoValue());
  }

  // Parsing and computed-style serialization both represent a single
  // non-auto value as a pair with identical items.
  const auto* pair = DynamicTo<CSSValuePair>(value);
  if (!pair) {
    return nullptr;
  }
  result->Set(kStartIndex,
              InterpolableLength::MaybeConvertCSSValue(pair->First()));
  result->Set(kEndIndex,
              InterpolableLength::MaybeConvertCSSValue(pair->Second()));
  if (!result->Get(kStartIndex) || !result->Get(kEndIndex)) {
    return nullptr;
  }
  result->Set(kAutoContributionIndex,
              MakeGarbageCollected<InterpolableNumber>(0));
  return InterpolationValue(result);
}

class InheritedInsetChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedInsetChecker(const ComputedStyle& parent_style)
      : inset_(parent_style.GetTextDecorationInset()) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    const ComputedStyle& parent_style = *state.ParentStyle();
    return inset_ == parent_style.GetTextDecorationInset();
  }

  const TextDecorationInset inset_;
};

}  // namespace

InterpolationValue CSSTextDecorationInsetInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers&) const {
  auto* result = To<InterpolableList>(underlying.interpolable_value->Clone());
  result->Set(kStartIndex, InterpolableLength::CreateNeutral());
  result->Set(kEndIndex, InterpolableLength::CreateNeutral());
  result->Set(kAutoContributionIndex,
              MakeGarbageCollected<InterpolableNumber>(0));
  return InterpolationValue(result);
}

InterpolationValue CSSTextDecorationInsetInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers&) const {
  const ComputedStyle& initial_style =
      state.GetDocument().GetStyleResolver().InitialStyle();
  return CreateInsetValue(initial_style.GetTextDecorationInset(), CssProperty(),
                          initial_style);
}

InterpolationValue CSSTextDecorationInsetInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const ComputedStyle& parent_style = *state.ParentStyle();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedInsetChecker>(parent_style));
  return CreateInsetValue(parent_style.GetTextDecorationInset(), CssProperty(),
                          parent_style);
}

InterpolationValue CSSTextDecorationInsetInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  return ConvertCSSValue(value);
}

InterpolationValue CSSTextDecorationInsetInterpolationType::
    MaybeConvertStandardPropertyUnderlyingValue(
        const ComputedStyle& style) const {
  return CreateInsetValue(style.GetTextDecorationInset(), CssProperty(), style);
}

PairwiseInterpolationValue
CSSTextDecorationInsetInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const bool result_is_auto = IsAuto(start) && IsAuto(end);
  auto* start_list = To<InterpolableList>(start.interpolable_value.Get());
  auto* end_list = To<InterpolableList>(end.interpolable_value.Get());
  DCHECK_EQ(start_list->length(), kComponentCount);
  DCHECK_EQ(end_list->length(), kComponentCount);
  for (wtf_size_t index : {kStartIndex, kEndIndex}) {
    if (!InterpolableLength::MaybeMergeSingles(
            start_list->GetMutable(index).Get(),
            end_list->GetMutable(index).Get())) {
      return nullptr;
    }
  }
  return PairwiseInterpolationValue(
      std::move(start.interpolable_value), std::move(end.interpolable_value),
      result_is_auto ? CreateAutoValue() : nullptr);
}

void CSSTextDecorationInsetInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  if (IsAuto(underlying_value_owner.Value()) && IsAuto(value)) {
    return;
  }
  underlying_value_owner.MutableInterpolableValue().ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
  underlying_value_owner.SetNonInterpolableValue(nullptr);
}

void CSSTextDecorationInsetInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  if (IsAuto(non_interpolable_value)) {
    state.StyleBuilder().SetTextDecorationInset(
        TextDecorationInset(Length::Auto(), Length::Auto()));
    return;
  }
  const auto& list = To<InterpolableList>(interpolable_value);
  const double auto_contribution =
      To<InterpolableNumber>(*list.Get(kAutoContributionIndex)).Value() *
      ResolveAutoInsetForAnimation(state.StyleBuilder());
  auto resolve = [&](wtf_size_t index) {
    InterpolableLength* length =
        To<InterpolableLength>(*list.Get(index)).Clone();
    length->Add(*InterpolableLength::CreatePixels(auto_contribution));
    return length->CreateLength(state.CssToLengthConversionData(),
                                Length::ValueRange::kAll);
  };
  state.StyleBuilder().SetTextDecorationInset(
      TextDecorationInset(resolve(kStartIndex), resolve(kEndIndex)));
}

}  // namespace blink
