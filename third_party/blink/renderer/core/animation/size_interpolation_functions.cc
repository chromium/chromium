// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/size_interpolation_functions.h"

#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"

namespace blink {

class CSSSizeNonInterpolableValue : public NonInterpolableValue {
 public:
  static scoped_refptr<CSSSizeNonInterpolableValue> Create(CSSValueID keyword) {
    return base::AdoptRef(new CSSSizeNonInterpolableValue(keyword));
  }

  static scoped_refptr<CSSSizeNonInterpolableValue> Create(
      scoped_refptr<const NonInterpolableValue> length_non_interpolable_value) {
    return base::AdoptRef(new CSSSizeNonInterpolableValue(
        std::move(length_non_interpolable_value)));
  }

  bool IsKeyword() const { return IsValidCSSValueID(keyword_); }
  CSSValueID Keyword() const {
    DCHECK(IsKeyword());
    return keyword_;
  }

  const NonInterpolableValue* LengthNonInterpolableValue() const {
    DCHECK(!IsKeyword());
    return length_non_interpolable_value_.get();
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSSizeNonInterpolableValue(CSSValueID keyword)
      : keyword_(keyword), length_non_interpolable_value_(nullptr) {
    DCHECK_NE(keyword, CSSValueID::kInvalid);
  }

  CSSSizeNonInterpolableValue(
      scoped_refptr<const NonInterpolableValue> length_non_interpolable_value)
      : keyword_(CSSValueID::kInvalid),
        length_non_interpolable_value_(
            std::move(length_non_interpolable_value)) {}

  CSSValueID keyword_;
  scoped_refptr<const NonInterpolableValue> length_non_interpolable_value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSSizeNonInterpolableValue);
template <>
struct DowncastTraits<CSSSizeNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == CSSSizeNonInterpolableValue::static_type_;
  }
};

static InterpolationValue ConvertKeyword(CSSValueID keyword) {
  return InterpolationValue(MakeGarbageCollected<InterpolableList>(0),
                            CSSSizeNonInterpolableValue::Create(keyword));
}

static InterpolationValue WrapConvertedLength(
    InterpolationValue&& converted_length) {
  if (!converted_length)
    return nullptr;
  return InterpolationValue(std::move(converted_length.interpolable_value),
                            CSSSizeNonInterpolableValue::Create(std::move(
                                converted_length.non_interpolable_value)));
}

InterpolationValue SizeInterpolationFunctions::ConvertFillSizeSide(
    const FillSize& fill_size,
    const CSSProperty& property,
    float zoom,
    bool convert_width) {
  switch (fill_size.type) {
    case EFillSizeType::kSizeLength: {
      const Length& side =
          convert_width ? fill_size.size.Width() : fill_size.size.Height();
      if (side.IsAuto())
        return ConvertKeyword(CSSValueID::kAuto);
      return WrapConvertedLength(
          InterpolationValue(InterpolableLength::MaybeConvertLength(
              side, property, zoom, /*interpolate_size=*/std::nullopt)));
    }
    case EFillSizeType::kContain:
      return ConvertKeyword(CSSValueID::kContain);
    case EFillSizeType::kCover:
      return ConvertKeyword(CSSValueID::kCover);
    case EFillSizeType::kSizeNone:
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

InterpolationValue SizeInterpolationFunctions::MaybeConvertCSSSizeSide(
    const CSSValue& value,
    bool convert_width) {
  if (const auto* pair = DynamicTo<CSSValuePair>(value)) {
    const CSSValue& side = convert_width ? pair->First() : pair->Second();
    auto* side_identifier_value = DynamicTo<CSSIdentifierValue>(side);
    if (side_identifier_value &&
        side_identifier_value->GetValueID() == CSSValueID::kAuto)
      return ConvertKeyword(CSSValueID::kAuto);
    return WrapConvertedLength(
        InterpolationValue(InterpolableLength::MaybeConvertCSSValue(side)));
  }

  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value && !value.IsPrimitiveValue())
    return nullptr;
  if (identifier_value)
    return ConvertKeyword(identifier_value->GetValueID());

  // A single length is equivalent to "<length> auto".
  if (convert_width) {
    return WrapConvertedLength(
        InterpolationValue(InterpolableLength::MaybeConvertCSSValue(value)));
  }
  return ConvertKeyword(CSSValueID::kAuto);
}

PairwiseInterpolationValue SizeInterpolationFunctions::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) {
  if (!NonInterpolableValuesAreCompatible(start.non_interpolable_value.get(),
                                          end.non_interpolable_value.get()))
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue SizeInterpolationFunctions::CreateNeutralValue(
    const NonInterpolableValue* non_interpolable_value) {
  auto& size = To<CSSSizeNonInterpolableValue>(*non_interpolable_value);
  if (size.IsKeyword())
    return ConvertKeyword(size.Keyword());
  return WrapConvertedLength(
      InterpolationValue(InterpolableLength::CreateNeutral()));
}

bool SizeInterpolationFunctions::NonInterpolableValuesAreCompatible(
    const NonInterpolableValue* a,
    const NonInterpolableValue* b) {
  const auto& size_a = To<CSSSizeNonInterpolableValue>(*a);
  const auto& size_b = To<CSSSizeNonInterpolableValue>(*b);
  if (size_a.IsKeyword() != size_b.IsKeyword())
    return false;
  if (size_a.IsKeyword())
    return size_a.Keyword() == size_b.Keyword();
  return true;
}

void SizeInterpolationFunctions::Composite(
    UnderlyingValue& underlying_value,
    double underlying_fraction,
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) {
  const auto& size_non_interpolable_value =
      To<CSSSizeNonInterpolableValue>(*non_interpolable_value);
  if (size_non_interpolable_value.IsKeyword())
    return;
  underlying_value.MutableInterpolableValue().ScaleAndAdd(underlying_fraction,
                                                          interpolable_value);
}

static Length CreateLength(
    const InterpolableValue& interpolable_value,
    const CSSSizeNonInterpolableValue& non_interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  if (non_interpolable_value.IsKeyword()) {
    DCHECK_EQ(non_interpolable_value.Keyword(), CSSValueID::kAuto);
    return Length::Auto();
  }
  return To<InterpolableLength>(interpolable_value)
      .CreateLength(conversion_data, Length::ValueRange::kNonNegative);
}

FillSize SizeInterpolationFunctions::CreateFillSize(
    const InterpolableValue& interpolable_value_a,
    const NonInterpolableValue* non_interpolable_value_a,
    const InterpolableValue& interpolable_value_b,
    const NonInterpolableValue* non_interpolable_value_b,
    const CSSToLengthConversionData& conversion_data) {
  const auto& side_a =
      To<CSSSizeNonInterpolableValue>(*non_interpolable_value_a);
  const auto& side_b =
      To<CSSSizeNonInterpolableValue>(*non_interpolable_value_b);
  if (side_a.IsKeyword()) {
    switch (side_a.Keyword()) {
      case CSSValueID::kCover:
        DCHECK_EQ(side_a.Keyword(), side_b.Keyword());
        return FillSize(EFillSizeType::kCover, LengthSize());
      case CSSValueID::kContain:
        DCHECK_EQ(side_a.Keyword(), side_b.Keyword());
        return FillSize(EFillSizeType::kContain, LengthSize());
      case CSSValueID::kAuto:
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  return FillSize(
      EFillSizeType::kSizeLength,
      LengthSize(CreateLength(interpolable_value_a, side_a, conversion_data),
                 CreateLength(interpolable_value_b, side_b, conversion_data)));
}

}  // namespace blink
