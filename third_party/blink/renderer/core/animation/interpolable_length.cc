// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_length.h"

#include "third_party/blink/renderer/core/animation/length_property_functions.h"
#include "third_party/blink/renderer/core/animation/underlying_value.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_math_operator.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_clamping_utils.h"
#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

using UnitType = CSSPrimitiveValue::UnitType;

namespace {

CSSMathExpressionNode* NumberNode(double number) {
  return CSSMathExpressionNumericLiteral::Create(
      CSSNumericLiteralValue::Create(number, UnitType::kNumber));
}

CSSMathExpressionNode* PercentageNode(double number) {
  return CSSMathExpressionNumericLiteral::Create(
      CSSNumericLiteralValue::Create(number, UnitType::kPercentage));
}

}  // namespace

// static
InterpolableLength* InterpolableLength::CreatePixels(double pixels) {
  CSSLengthArray length_array;
  length_array.values[CSSPrimitiveValue::kUnitTypePixels] = pixels;
  length_array.type_flags.set(CSSPrimitiveValue::kUnitTypePixels);
  return MakeGarbageCollected<InterpolableLength>(std::move(length_array));
}

// static
InterpolableLength* InterpolableLength::CreatePercent(double percent) {
  CSSLengthArray length_array;
  length_array.values[CSSPrimitiveValue::kUnitTypePercentage] = percent;
  length_array.type_flags.set(CSSPrimitiveValue::kUnitTypePercentage);
  return MakeGarbageCollected<InterpolableLength>(std::move(length_array));
}

// static
InterpolableLength* InterpolableLength::CreateNeutral() {
  return MakeGarbageCollected<InterpolableLength>(CSSLengthArray());
}

// static
InterpolableLength* InterpolableLength::MaybeConvertCSSValue(
    const CSSValue& value) {
  const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value)
    return nullptr;

  if (!primitive_value->IsLength() && !primitive_value->IsPercentage() &&
      !primitive_value->IsCalculatedPercentageWithLength())
    return nullptr;

  CSSLengthArray length_array;
  if (primitive_value->AccumulateLengthArray(length_array))
    return MakeGarbageCollected<InterpolableLength>(std::move(length_array));

  const CSSMathExpressionNode* expression_node = nullptr;

  if (const auto* numeric_literal =
          DynamicTo<CSSNumericLiteralValue>(primitive_value)) {
    expression_node = CSSMathExpressionNumericLiteral::Create(numeric_literal);
  } else {
    DCHECK(primitive_value->IsMathFunctionValue());
    expression_node =
        To<CSSMathFunctionValue>(primitive_value)->ExpressionNode();
  }

  return MakeGarbageCollected<InterpolableLength>(*expression_node);
}

CSSValueID InterpolableLength::LengthTypeToCSSValueID(Length::Type lt) {
  switch (lt) {
    case Length::Type::kAuto:
      return CSSValueID::kAuto;
    case Length::Type::kMinContent:
      return CSSValueID::kMinContent;
    case Length::Type::kMaxContent:
      return CSSValueID::kMaxContent;
    case Length::Type::kFitContent:
      return CSSValueID::kFitContent;
    case Length::Type::kStretch:
      return CSSValueID::kWebkitFillAvailable;
    case Length::Type::kContent:  // only valid for flex-basis.
      return CSSValueID::kContent;
    default:
      return CSSValueID::kInvalid;
  }
}

Length::Type InterpolableLength::CSSValueIDToLengthType(CSSValueID id) {
  switch (id) {
    case CSSValueID::kAuto:
      return Length::Type::kAuto;
    case CSSValueID::kMinContent:
    case CSSValueID::kWebkitMinContent:
      return Length::Type::kMinContent;
    case CSSValueID::kMaxContent:
    case CSSValueID::kWebkitMaxContent:
      return Length::Type::kMaxContent;
    case CSSValueID::kFitContent:
    case CSSValueID::kWebkitFitContent:
      return Length::Type::kFitContent;
    case CSSValueID::kWebkitFillAvailable:
      return Length::Type::kStretch;
    case CSSValueID::kContent:  // only valid for flex-basis.
      return Length::Type::kContent;
    default:
      NOTREACHED_IN_MIGRATION();
      return Length::Type::kFixed;
  }
}

// static
InterpolableLength* InterpolableLength::MaybeConvertLength(
    const Length& length,
    const CSSProperty& property,
    float zoom,
    std::optional<EInterpolateSize> interpolate_size) {
  if (!length.IsSpecified()) {
    if (!RuntimeEnabledFeatures::CSSCalcSizeFunctionEnabled()) {
      return nullptr;
    }
    CSSValueID keyword = LengthTypeToCSSValueID(length.GetType());
    if (keyword == CSSValueID::kInvalid ||
        !LengthPropertyFunctions::CanAnimateKeyword(property, keyword)) {
      return nullptr;
    }
    return MakeGarbageCollected<InterpolableLength>(keyword, interpolate_size);
  }

  if (length.IsCalculated() && length.GetCalculationValue().IsExpression()) {
    auto unzoomed_calc = length.GetCalculationValue().Zoom(1.0 / zoom);
    return MakeGarbageCollected<InterpolableLength>(
        *CSSMathExpressionNode::Create(*unzoomed_calc));
  }

  PixelsAndPercent pixels_and_percent = length.GetPixelsAndPercent();
  CSSLengthArray length_array;

  length_array.values[CSSPrimitiveValue::kUnitTypePixels] =
      pixels_and_percent.pixels / zoom;
  length_array.type_flags[CSSPrimitiveValue::kUnitTypePixels] =
      pixels_and_percent.has_explicit_pixels;

  length_array.values[CSSPrimitiveValue::kUnitTypePercentage] =
      pixels_and_percent.percent;
  length_array.type_flags[CSSPrimitiveValue::kUnitTypePercentage] =
      pixels_and_percent.has_explicit_percent;
  return MakeGarbageCollected<InterpolableLength>(std::move(length_array));
}

bool InterpolableLength::IsCalcSize() const {
  if (!IsExpression()) {
    return false;
  }
  const auto* operation =
      DynamicTo<CSSMathExpressionOperation>(expression_.Get());
  return operation && operation->IsCalcSize();
}

namespace {

const CSSMathExpressionNode& ExtractCalcSizeBasis(
    const CSSMathExpressionNode* node) {
  const auto* operation = DynamicTo<CSSMathExpressionOperation>(node);
  if (!operation || !operation->IsCalcSize()) {
    return *node;
  }

  return ExtractCalcSizeBasis(operation->GetOperands()[0]);
}

}  // namespace

// static
bool InterpolableLength::CanMergeValues(const InterpolableValue* start,
                                        const InterpolableValue* end) {
  const auto& start_length = To<InterpolableLength>(*start);
  const auto& end_length = To<InterpolableLength>(*end);

  // Implement the rules in
  // https://drafts.csswg.org/css-values-5/#interp-calc-size, but
  // without actually writing the implicit conversion of the "other"
  // value to a calc-size().  This means that if one value is a
  // calc-size(), the other value converts to:
  // * for intrinsic size keywords, a calc-size(value, size)
  // * for other values, a calc-size(any, value)

  // Only animate to or from width keywords if the other endpoint of the
  // animation is a calc-size() expression.  And only animate between
  // calc-size() expressions or between a keyword and a calc-size() expression
  // if they have compatible basis.

  const bool start_is_keyword = start_length.IsKeyword();
  const bool end_is_keyword = end_length.IsKeyword();
  if (start_is_keyword || end_is_keyword) {
    // Only animate to or from width keywords if the other endpoint of the
    // animation is a calc-size() expression.
    const InterpolableLength* keyword;
    const InterpolableLength* non_keyword;
    if (start_is_keyword) {
      if (end_is_keyword) {
        return false;
      }
      keyword = &start_length;
      non_keyword = &end_length;
    } else {
      non_keyword = &start_length;
      keyword = &end_length;
    }

    if (!non_keyword->IsCalcSize()) {
      // Check the 'interpolate-size' value stored with the keyword.
      return keyword->IsKeywordFullyInterpolable();
    }
    const CSSMathExpressionNode& basis =
        ExtractCalcSizeBasis(non_keyword->expression_);

    if (const auto* basis_literal =
            DynamicTo<CSSMathExpressionKeywordLiteral>(basis)) {
      return basis_literal->GetValue() == keyword->keyword_ ||
             basis_literal->GetValue() == CSSValueID::kAny;
    }

    return false;
  }

  // Only animate between calc-size() expressions if they have compatible
  // basis.  This includes checking the type of the keyword, but it also
  // includes broad compatibility for 'any', and for animating between
  // different <calc-sum> values.  There are also some cases where we
  // need to check that we don't exceed the expansion limit for
  // substituting to handle nested calc-size() expressions.
  //
  // CreateArithmeticOperationAndSimplifyCalcSize knows how to determine
  // this.
  if (start_length.IsCalcSize() && end_length.IsCalcSize()) {
    return CSSMathExpressionOperation::
               CreateArithmeticOperationAndSimplifyCalcSize(
                   start_length.expression_, end_length.expression_,
                   CSSMathOperator::kAdd) != nullptr;
  }

  return true;
}

// static
PairwiseInterpolationValue InterpolableLength::MaybeMergeSingles(
    InterpolableValue* start,
    InterpolableValue* end) {
  // TODO(crbug.com/991672): We currently have a lot of "fast paths" that do not
  // go through here, and hence, do not merge the percentage info of two
  // lengths. We should stop doing that.
  auto& start_length = To<InterpolableLength>(*start);
  auto& end_length = To<InterpolableLength>(*end);

  if (!CanMergeValues(start, end)) {
    return nullptr;
  }

  if (start_length.HasPercentage() || end_length.HasPercentage()) {
    start_length.SetHasPercentage();
    end_length.SetHasPercentage();
  }
  if (start_length.IsExpression() || end_length.IsExpression()) {
    start_length.SetExpression(start_length.AsExpression());
    end_length.SetExpression(end_length.AsExpression());
  }
  return PairwiseInterpolationValue(start, end);
}

InterpolableLength::InterpolableLength(CSSLengthArray&& length_array) {
  SetLengthArray(std::move(length_array));
}

void InterpolableLength::SetLengthArray(CSSLengthArray&& length_array) {
  type_ = Type::kLengthArray;
  length_array_ = std::move(length_array);
  expression_.Clear();
}

InterpolableLength::InterpolableLength(
    const CSSMathExpressionNode& expression) {
  SetExpression(expression);
}

void InterpolableLength::SetExpression(
    const CSSMathExpressionNode& expression) {
  type_ = Type::kExpression;
  expression_ = &expression;
}

InterpolableLength::InterpolableLength(
    CSSValueID keyword,
    std::optional<EInterpolateSize> interpolate_size) {
  SetKeyword(keyword, interpolate_size);
}

void InterpolableLength::SetKeyword(
    CSSValueID keyword,
    std::optional<EInterpolateSize> interpolate_size) {
  if (interpolate_size) {
    switch (*interpolate_size) {
      case EInterpolateSize::kNumericOnly:
        type_ = Type::kRestrictedKeyword;
        break;
      case EInterpolateSize::kAllowKeywords:
        type_ = Type::kFullyInterpolableKeyword;
        break;
      default:
        NOTREACHED();
    }
  } else {
    type_ = Type::kUnknownKeyword;
  }
  keyword_ = keyword;
  expression_.Clear();
}

void InterpolableLength::SetInterpolateSize(EInterpolateSize interpolate_size) {
  if (!IsKeyword()) {
    return;
  }

  // We can't make useful assertions about this not changing an
  // already-set type because, for CSS transitions, we do exactly that,
  // for the length that comes from the before-change style (in the case
  // where it comes from an underlying value), so that it uses the
  // interpolate-size value from the after-change style.

  switch (interpolate_size) {
    case EInterpolateSize::kNumericOnly:
      type_ = Type::kRestrictedKeyword;
      break;
    case EInterpolateSize::kAllowKeywords:
      type_ = Type::kFullyInterpolableKeyword;
      break;
    default:
      NOTREACHED();
  }
}

InterpolableLength* InterpolableLength::RawClone() const {
  return MakeGarbageCollected<InterpolableLength>(*this);
}

bool InterpolableLength::HasPercentage() const {
  switch (type_) {
    case Type::kRestrictedKeyword:
    case Type::kFullyInterpolableKeyword:
    case Type::kUnknownKeyword:
      return false;
    case Type::kLengthArray:
      return length_array_.type_flags.test(
          CSSPrimitiveValue::kUnitTypePercentage);
    case Type::kExpression:
      return expression_->HasPercentage();
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

void InterpolableLength::SetHasPercentage() {
  if (HasPercentage())
    return;

  if (IsLengthArray()) {
    length_array_.type_flags.set(CSSPrimitiveValue::kUnitTypePercentage);
    return;
  }

  if (IsKeyword()) {
    SetExpression(AsExpression());
  }

  DEFINE_STATIC_LOCAL(Persistent<CSSMathExpressionNode>, zero_percent,
                      {PercentageNode(0)});
  SetExpression(
      *CSSMathExpressionOperation::CreateArithmeticOperationAndSimplifyCalcSize(
          expression_, zero_percent, CSSMathOperator::kAdd));
}

void InterpolableLength::SubtractFromOneHundredPercent() {
  if (IsLengthArray()) {
    for (double& value : length_array_.values)
      value *= -1;
    length_array_.values[CSSPrimitiveValue::kUnitTypePercentage] += 100;
    length_array_.type_flags.set(CSSPrimitiveValue::kUnitTypePercentage);
    return;
  }

  if (IsKeyword()) {
    SetExpression(AsExpression());
  }

  DEFINE_STATIC_LOCAL(Persistent<CSSMathExpressionNode>, hundred_percent,
                      {PercentageNode(100)});
  SetExpression(
      *CSSMathExpressionOperation::CreateArithmeticOperationAndSimplifyCalcSize(
          hundred_percent, expression_, CSSMathOperator::kSubtract));
}

static double ClampToRange(double x, Length::ValueRange range) {
  return (range == Length::ValueRange::kNonNegative && x < 0) ? 0 : x;
}

static const CSSNumericLiteralValue& ClampNumericLiteralValueToRange(
    const CSSNumericLiteralValue& value,
    CSSPrimitiveValue::ValueRange range) {
  if (range == CSSPrimitiveValue::ValueRange::kAll || value.DoubleValue() >= 0)
    return value;
  return *CSSNumericLiteralValue::Create(0, value.GetType());
}

static UnitType IndexToUnitType(wtf_size_t index) {
  return CSSPrimitiveValue::LengthUnitTypeToUnitType(
      static_cast<CSSPrimitiveValue::LengthUnitType>(index));
}

Length InterpolableLength::CreateLength(
    const CSSToLengthConversionData& conversion_data,
    Length::ValueRange range) const {
  if (IsExpression()) {
    if (expression_->Category() == kCalcLength) {
      double pixels = expression_->ComputeLengthPx(conversion_data);
      return Length::Fixed(CSSPrimitiveValue::ClampToCSSLengthRange(
          ClampToRange(pixels, range)));
    }
    // Passing true for ToCalcValue is a dirty hack to ensure that we don't
    // create a degenerate value when animating 'background-position', while we
    // know it may cause some minor animation glitches for the other properties.
    return Length(expression_->ToCalcValue(conversion_data, range, true));
  }

  if (IsKeyword()) {
    return Length(CSSValueIDToLengthType(keyword_));
  }

  DCHECK(IsLengthArray());
  bool has_percentage = HasPercentage();
  double pixels = 0;
  double percentage = 0;
  for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
    double value = CSSValueClampingUtils::ClampLength(length_array_.values[i]);
    if (value == 0)
      continue;
    if (i == CSSPrimitiveValue::kUnitTypePercentage) {
      percentage = value;
    } else {
      pixels += conversion_data.ZoomedComputedPixels(value, IndexToUnitType(i));
    }
  }
  pixels = CSSValueClampingUtils::ClampLength(pixels);

  if (percentage != 0)
    has_percentage = true;
  if (pixels != 0 && has_percentage) {
    pixels = ClampTo<float>(pixels);
    if (percentage == 0) {
      // Match the clamping behavior in the StyleBuilder code path,
      // which goes through CSSPrimitiveValue::CreateFromLength and then
      // CSSPrimitiveValue::ConvertToLength.
      pixels = CSSPrimitiveValue::ClampToCSSLengthRange(pixels);
    }
    return Length(CalculationValue::Create(
        PixelsAndPercent(pixels, ClampTo<float>(percentage),
                         /*has_explicit_pixels=*/true,
                         /*has_explicit_percent=*/true),
        range));
  }
  if (has_percentage)
    return Length::Percent(ClampToRange(percentage, range));
  return Length::Fixed(
      CSSPrimitiveValue::ClampToCSSLengthRange(ClampToRange(pixels, range)));
}

const CSSPrimitiveValue* InterpolableLength::CreateCSSValue(
    Length::ValueRange range) const {
  if (!IsLengthArray()) {
    return CSSMathFunctionValue::Create(
        &AsExpression(),
        CSSPrimitiveValue::ValueRangeForLengthValueRange(range));
  }

  DCHECK(IsLengthArray());
  if (length_array_.type_flags.count() > 1u) {
    const CSSMathExpressionNode& expression = AsExpression();
    if (!expression.IsNumericLiteral()) {
      return CSSMathFunctionValue::Create(
          &expression, CSSPrimitiveValue::ValueRangeForLengthValueRange(range));
    }

    // This creates a temporary CSSMathExpressionNode. Eliminate it if this
    // results in significant performance regression.
    return &ClampNumericLiteralValueToRange(
        To<CSSMathExpressionNumericLiteral>(expression).GetValue(),
        CSSPrimitiveValue::ValueRangeForLengthValueRange(range));
  }

  for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
    if (length_array_.type_flags.test(i)) {
      double value = ClampToRange(length_array_.values[i], range);
      UnitType unit_type = IndexToUnitType(i);
      return CSSNumericLiteralValue::Create(value, unit_type);
    }
  }

  return CSSNumericLiteralValue::Create(0, UnitType::kPixels);
}

const CSSMathExpressionNode& InterpolableLength::AsExpression() const {
  if (IsExpression())
    return *expression_;

  if (IsKeyword()) {
    const auto* basis = CSSMathExpressionKeywordLiteral::Create(
        keyword_, CSSMathExpressionKeywordLiteral::Context::kCalcSize);
    const auto* calculation = CSSMathExpressionKeywordLiteral::Create(
        CSSValueID::kSize, CSSMathExpressionKeywordLiteral::Context::kCalcSize);
    return *CSSMathExpressionOperation::CreateCalcSizeOperation(basis,
                                                                calculation);
  }

  DCHECK(IsLengthArray());
  bool has_percentage = HasPercentage();

  CSSMathExpressionNode* root_node = nullptr;
  for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
    double value = length_array_.values[i];
    if (value == 0 &&
        (i != CSSPrimitiveValue::kUnitTypePercentage || !has_percentage)) {
      continue;
    }
    CSSNumericLiteralValue* current_value =
        CSSNumericLiteralValue::Create(value, IndexToUnitType(i));
    CSSMathExpressionNode* current_node =
        CSSMathExpressionNumericLiteral::Create(current_value);
    if (!root_node) {
      root_node = current_node;
    } else {
      root_node = CSSMathExpressionOperation::CreateArithmeticOperation(
          root_node, current_node, CSSMathOperator::kAdd);
    }
  }

  if (root_node)
    return *root_node;
  return *CSSMathExpressionNumericLiteral::Create(
      CSSNumericLiteralValue::Create(0, UnitType::kPixels));
}

void InterpolableLength::Scale(double scale) {
  if (IsLengthArray()) {
    for (auto& value : length_array_.values)
      value *= scale;
    return;
  }

  if (IsKeyword()) {
    SetExpression(AsExpression());
  }

  DCHECK(IsExpression());
  SetExpression(
      *CSSMathExpressionOperation::CreateArithmeticOperationAndSimplifyCalcSize(
          expression_, NumberNode(scale), CSSMathOperator::kMultiply));
}

void InterpolableLength::Add(const InterpolableValue& other) {
  const InterpolableLength& other_length = To<InterpolableLength>(other);
  if (IsLengthArray() && other_length.IsLengthArray()) {
    for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
      length_array_.values[i] =
          length_array_.values[i] + other_length.length_array_.values[i];
    }
    length_array_.type_flags |= other_length.length_array_.type_flags;
    return;
  }

  CSSMathExpressionNode* result =
      CSSMathExpressionOperation::CreateArithmeticOperationAndSimplifyCalcSize(
          &AsExpression(), &other_length.AsExpression(), CSSMathOperator::kAdd);
  CHECK(result)
      << "should not attempt to interpolate when result would be IACVT";
  SetExpression(*result);
}

void InterpolableLength::ScaleAndAdd(double scale,
                                     const InterpolableValue& other) {
  const InterpolableLength& other_length = To<InterpolableLength>(other);
  if (IsLengthArray() && other_length.IsLengthArray()) {
    for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
      length_array_.values[i] = length_array_.values[i] * scale +
                                other_length.length_array_.values[i];
    }
    length_array_.type_flags |= other_length.length_array_.type_flags;
    return;
  }

  CSSMathExpressionNode* scaled =
      CSSMathExpressionOperation::CreateArithmeticOperationAndSimplifyCalcSize(
          &AsExpression(), NumberNode(scale), CSSMathOperator::kMultiply);
  CSSMathExpressionNode* result =
      CSSMathExpressionOperation::CreateArithmeticOperationAndSimplifyCalcSize(
          scaled, &other_length.AsExpression(), CSSMathOperator::kAdd);
  CHECK(result)
      << "should not attempt to interpolate when result would be IACVT";
  SetExpression(*result);
}

void InterpolableLength::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  DCHECK(other.IsLength());
  // TODO(crbug.com/991672): Ensure that all |MergeSingles| variants that merge
  // two |InterpolableLength| objects should also assign them the same shape
  // (i.e. type flags) after merging into a |PairwiseInterpolationValue|. We
  // currently fail to do that, and hit the following DCHECK:
  // DCHECK_EQ(HasPercentage(),
  //           To<InterpolableLength>(other).HasPercentage());
}

void InterpolableLength::Interpolate(const InterpolableValue& to,
                                     const double progress,
                                     InterpolableValue& result) const {
  const auto& to_length = To<InterpolableLength>(to);
  auto& result_length = To<InterpolableLength>(result);
  if (IsLengthArray() && to_length.IsLengthArray()) {
    if (!result_length.IsLengthArray())
      result_length.SetLengthArray(CSSLengthArray());
    const CSSLengthArray& to_length_array = to_length.length_array_;
    CSSLengthArray& result_length_array =
        To<InterpolableLength>(result).length_array_;
    for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
      result_length_array.values[i] =
          Blend(length_array_.values[i], to_length_array.values[i], progress);
    }
    result_length_array.type_flags =
        length_array_.type_flags | to_length_array.type_flags;
    return;
  }

  CSSMathExpressionNode* blended_from =
      CSSMathExpressionOperation::CreateArithmeticOperationAndSimplifyCalcSize(
          &AsExpression(), NumberNode(1 - progress),
          CSSMathOperator::kMultiply);
  CSSMathExpressionNode* blended_to =
      CSSMathExpressionOperation::CreateArithmeticOperationAndSimplifyCalcSize(
          &to_length.AsExpression(), NumberNode(progress),
          CSSMathOperator::kMultiply);
  CSSMathExpressionNode* result_expression =
      CSSMathExpressionOperation::CreateArithmeticOperationAndSimplifyCalcSize(
          blended_from, blended_to, CSSMathOperator::kAdd);
  CHECK(result_expression)
      << "should not attempt to interpolate when result would be IACVT";
  result_length.SetExpression(*result_expression);
}

void InterpolableLength::Trace(Visitor* v) const {
  InterpolableValue::Trace(v);
  v->Trace(expression_);
}

}  // namespace blink
