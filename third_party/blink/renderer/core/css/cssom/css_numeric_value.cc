// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"

#include <numeric>

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_invert.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_max.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_min.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_negate.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_product.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_sum.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

template <CSSStyleValue::StyleValueType type>
void PrependValueForArithmetic(CSSNumericValueVector& vector,
                               CSSNumericValue* value) {
  DCHECK(value);
  if (value->GetType() == type)
    vector.PrependVector(static_cast<CSSMathVariadic*>(value)->NumericValues());
  else
    vector.push_front(value);
}

template <class BinaryOperation>
CSSUnitValue* MaybeSimplifyAsUnitValue(const CSSNumericValueVector& values,
                                       const BinaryOperation& op) {
  DCHECK(!values.IsEmpty());

  auto* first_unit_value = DynamicTo<CSSUnitValue>(values[0].Get());
  if (!first_unit_value)
    return nullptr;

  double final_value = first_unit_value->value();
  for (wtf_size_t i = 1; i < values.size(); i++) {
    auto* unit_value = DynamicTo<CSSUnitValue>(values[i].Get());
    if (!unit_value ||
        unit_value->GetInternalUnit() != first_unit_value->GetInternalUnit())
      return nullptr;

    final_value = op(final_value, unit_value->value());
  }

  return CSSUnitValue::Create(final_value, first_unit_value->GetInternalUnit());
}

CSSUnitValue* MaybeMultiplyAsUnitValue(const CSSNumericValueVector& values) {
  DCHECK(!values.IsEmpty());

  // We are allowed one unit value with type other than kNumber.
  auto unit_other_than_number = CSSPrimitiveValue::UnitType::kNumber;

  double final_value = 1.0;
  for (wtf_size_t i = 0; i < values.size(); i++) {
    auto* unit_value = DynamicTo<CSSUnitValue>(values[i].Get());
    if (!unit_value)
      return nullptr;

    if (unit_value->GetInternalUnit() != CSSPrimitiveValue::UnitType::kNumber) {
      if (unit_other_than_number != CSSPrimitiveValue::UnitType::kNumber)
        return nullptr;
      unit_other_than_number = unit_value->GetInternalUnit();
    }

    final_value *= unit_value->value();
  }

  return CSSUnitValue::Create(final_value, unit_other_than_number);
}

CSSMathOperator CanonicalOperator(CSSMathOperator op) {
  if (op == CSSMathOperator::kAdd || op == CSSMathOperator::kSubtract)
    return CSSMathOperator::kAdd;
  return CSSMathOperator::kMultiply;
}

bool CanCombineNodes(const CSSMathExpressionNode& root,
                     const CSSMathExpressionNode& node) {
  DCHECK(root.IsBinaryOperation());
  if (!node.IsBinaryOperation())
    return false;
  if (node.IsNestedCalc())
    return false;
  return CanonicalOperator(
             To<CSSMathExpressionBinaryOperation>(root).OperatorType()) ==
         CanonicalOperator(
             To<CSSMathExpressionBinaryOperation>(node).OperatorType());
}

CSSNumericValue* NegateOrInvertIfRequired(CSSMathOperator parent_op,
                                          CSSNumericValue* value) {
  DCHECK(value);
  if (parent_op == CSSMathOperator::kSubtract)
    return CSSMathNegate::Create(value);
  if (parent_op == CSSMathOperator::kDivide)
    return CSSMathInvert::Create(value);
  return value;
}

CSSNumericValue* CalcToNumericValue(const CSSMathExpressionNode& root) {
  if (root.IsNumericLiteral()) {
    const CSSPrimitiveValue::UnitType unit = root.ResolvedUnitType();
    auto* value = CSSUnitValue::Create(
        root.DoubleValue(), unit == CSSPrimitiveValue::UnitType::kInteger
                                ? CSSPrimitiveValue::UnitType::kNumber
                                : unit);
    DCHECK(value);

    // For cases like calc(1), we need to wrap the value in a CSSMathSum
    if (!root.IsNestedCalc())
      return value;

    CSSNumericValueVector values;
    values.push_back(value);
    return CSSMathSum::Create(std::move(values));
  }

  CSSNumericValueVector values;

  // When the node is a variadic operation, we return either a CSSMathMin or a
  // CSSMathMax.
  if (root.IsVariadicOperation()) {
    const auto& node = To<CSSMathExpressionVariadicOperation>(root);
    for (const auto& operand : node.GetOperands())
      values.push_back(CalcToNumericValue(*operand));
    if (node.OperatorType() == CSSMathOperator::kMin)
      return CSSMathMin::Create(std::move(values));
    DCHECK(node.OperatorType() == CSSMathOperator::kMax);
    return CSSMathMax::Create(std::move(values));
  }

  DCHECK(root.IsBinaryOperation());
  // When the node is a binary operator, we return either a CSSMathSum or a
  // CSSMathProduct.
  // For cases like calc(1 + 2 + 3), the calc expression tree looks like:
  //       +     //
  //      / \    //
  //     +   3   //
  //    / \      //
  //   1   2     //
  //
  // But we want to produce a CSSMathValue tree that looks like:
  //       +     //
  //      /|\    //
  //     1 2 3   //
  //
  // So when the left child has the same operator as its parent, we can combine
  // the two nodes. We keep moving down the left side of the tree as long as the
  // current node and the root can be combined, collecting the right child of
  // the nodes that we encounter.
  const CSSMathExpressionNode* cur_node = &root;
  do {
    DCHECK(cur_node->IsBinaryOperation());
    const CSSMathExpressionBinaryOperation* binary_op =
        To<CSSMathExpressionBinaryOperation>(cur_node);
    DCHECK(binary_op->LeftExpressionNode());
    DCHECK(binary_op->RightExpressionNode());

    auto* const value = CalcToNumericValue(*binary_op->RightExpressionNode());

    // If the current node is a '-' or '/', it's really just a '+' or '*' with
    // the right child negated or inverted, respectively.
    values.push_back(
        NegateOrInvertIfRequired(binary_op->OperatorType(), value));
    cur_node = binary_op->LeftExpressionNode();
  } while (CanCombineNodes(root, *cur_node));

  DCHECK(cur_node);
  values.push_back(CalcToNumericValue(*cur_node));

  // Our algorithm collects the children in reverse order, so we have to reverse
  // the values.
  std::reverse(values.begin(), values.end());
  CSSMathOperator operator_type =
      To<CSSMathExpressionBinaryOperation>(root).OperatorType();
  if (operator_type == CSSMathOperator::kAdd ||
      operator_type == CSSMathOperator::kSubtract)
    return CSSMathSum::Create(std::move(values));
  return CSSMathProduct::Create(std::move(values));
}

CSSUnitValue* CSSNumericSumValueEntryToUnitValue(
    const CSSNumericSumValue::Term& term) {
  if (term.units.size() == 0)
    return CSSUnitValue::Create(term.value);
  if (term.units.size() == 1 && term.units.begin()->value == 1)
    return CSSUnitValue::Create(term.value, term.units.begin()->key);
  return nullptr;
}

}  // namespace

bool CSSNumericValue::IsValidUnit(CSSPrimitiveValue::UnitType unit) {
  // UserUnits returns true for CSSPrimitiveValue::IsLength below.
  if (unit == CSSPrimitiveValue::UnitType::kUserUnits)
    return false;
  if (unit == CSSPrimitiveValue::UnitType::kNumber ||
      unit == CSSPrimitiveValue::UnitType::kPercentage ||
      CSSPrimitiveValue::IsLength(unit) || CSSPrimitiveValue::IsAngle(unit) ||
      CSSPrimitiveValue::IsTime(unit) || CSSPrimitiveValue::IsFrequency(unit) ||
      CSSPrimitiveValue::IsResolution(unit) || CSSPrimitiveValue::IsFlex(unit))
    return true;
  return false;
}

CSSPrimitiveValue::UnitType CSSNumericValue::UnitFromName(const String& name) {
  if (name.IsEmpty())
    return CSSPrimitiveValue::UnitType::kUnknown;
  if (EqualIgnoringASCIICase(name, "number"))
    return CSSPrimitiveValue::UnitType::kNumber;
  if (EqualIgnoringASCIICase(name, "percent") || name == "%")
    return CSSPrimitiveValue::UnitType::kPercentage;
  return CSSPrimitiveValue::StringToUnitType(name);
}

CSSNumericValue* CSSNumericValue::parse(const String& css_text,
                                        ExceptionState& exception_state) {
  CSSTokenizer tokenizer(css_text);
  CSSParserTokenStream stream(tokenizer);
  stream.ConsumeWhitespace();
  auto range = stream.ConsumeUntilPeekedTypeIs<>();
  stream.ConsumeWhitespace();
  if (!stream.AtEnd()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid math expression");
    return nullptr;
  }

  switch (range.Peek().GetType()) {
    case kNumberToken:
    case kPercentageToken:
    case kDimensionToken: {
      const auto token = range.ConsumeIncludingWhitespace();
      if (!range.AtEnd())
        break;
      return CSSUnitValue::Create(token.NumericValue(), token.GetUnitType());
    }
    case kFunctionToken:
      if (range.Peek().FunctionId() == CSSValueID::kCalc ||
          range.Peek().FunctionId() == CSSValueID::kWebkitCalc ||
          (RuntimeEnabledFeatures::CSSComparisonFunctionsEnabled() &&
           (range.Peek().FunctionId() == CSSValueID::kMin ||
            range.Peek().FunctionId() == CSSValueID::kMax ||
            range.Peek().FunctionId() == CSSValueID::kClamp))) {
        CSSMathExpressionNode* expression =
            CSSMathExpressionNode::ParseCalc(range);
        if (expression)
          return CalcToNumericValue(*expression);
      }
      break;
    default:
      break;
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                    "Invalid math expression");
  return nullptr;
}

CSSNumericValue* CSSNumericValue::FromCSSValue(const CSSPrimitiveValue& value) {
  if (value.IsCalculated()) {
    return CalcToNumericValue(
        *To<CSSMathFunctionValue>(value).ExpressionNode());
  }
  return CSSUnitValue::FromCSSValue(To<CSSNumericLiteralValue>(value));
}

/* static */
CSSNumericValue* CSSNumericValue::FromNumberish(const CSSNumberish& value) {
  if (value.IsDouble()) {
    return CSSUnitValue::Create(value.GetAsDouble(),
                                CSSPrimitiveValue::UnitType::kNumber);
  }
  return value.GetAsCSSNumericValue();
}

CSSUnitValue* CSSNumericValue::to(const String& unit_string,
                                  ExceptionState& exception_state) {
  CSSPrimitiveValue::UnitType target_unit = UnitFromName(unit_string);
  if (!IsValidUnit(target_unit)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid unit for conversion");
    return nullptr;
  }

  CSSUnitValue* result = to(target_unit);
  if (!result) {
    exception_state.ThrowTypeError("Cannot convert to " + unit_string);
    return nullptr;
  }

  return result;
}

CSSUnitValue* CSSNumericValue::to(CSSPrimitiveValue::UnitType unit) const {
  const auto sum = SumValue();
  if (!sum || sum->terms.size() != 1)
    return nullptr;

  CSSUnitValue* value = CSSNumericSumValueEntryToUnitValue(sum->terms[0]);
  if (!value)
    return nullptr;
  return value->ConvertTo(unit);
}

CSSMathSum* CSSNumericValue::toSum(const Vector<String>& unit_strings,
                                   ExceptionState& exception_state) {
  for (const auto& unit_string : unit_strings) {
    if (!IsValidUnit(UnitFromName(unit_string))) {
      exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                        "Invalid unit for conversion");
      return nullptr;
    }
  }

  const base::Optional<CSSNumericSumValue> sum = SumValue();
  if (!sum) {
    exception_state.ThrowTypeError("Invalid value for conversion");
    return nullptr;
  }

  CSSNumericValueVector values;
  for (const auto& term : sum->terms) {
    CSSUnitValue* value = CSSNumericSumValueEntryToUnitValue(term);
    if (!value) {
      exception_state.ThrowTypeError("Invalid value for conversion");
      return nullptr;
    }
    values.push_back(value);
  }

  if (unit_strings.size() == 0) {
    std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
      return WTF::CodeUnitCompareLessThan(To<CSSUnitValue>(a.Get())->unit(),
                                          To<CSSUnitValue>(b.Get())->unit());
    });

    // We got 'values' from a sum value, so it must be a valid CSSMathSum.
    CSSMathSum* result = CSSMathSum::Create(values);
    DCHECK(result);
    return result;
  }

  CSSNumericValueVector result;
  for (const auto& unit_string : unit_strings) {
    CSSPrimitiveValue::UnitType target_unit = UnitFromName(unit_string);
    DCHECK(IsValidUnit(target_unit));

    // Collect all the terms that are compatible with this unit.
    // We mark used terms as null so we don't use them again.
    double total_value =
        std::accumulate(values.begin(), values.end(), 0.0,
                        [target_unit](double cur_sum, auto& value) {
                          if (value) {
                            auto& unit_value = To<CSSUnitValue>(*value);
                            if (const auto* converted_value =
                                    unit_value.ConvertTo(target_unit)) {
                              cur_sum += converted_value->value();
                              value = nullptr;
                            }
                          }
                          return cur_sum;
                        });

    result.push_back(CSSUnitValue::Create(total_value, target_unit));
  }

  if (std::any_of(values.begin(), values.end(),
                  [](const auto& v) { return v; })) {
    exception_state.ThrowTypeError(
        "There were leftover terms that were not converted");
    return nullptr;
  }

  CSSMathSum* value = CSSMathSum::Create(result);
  if (!value) {
    exception_state.ThrowTypeError("Can't create CSSMathSum");
    return nullptr;
  }
  return value;
}

CSSNumericType* CSSNumericValue::type() const {
  CSSNumericType* type = CSSNumericType::Create();
  using BaseType = CSSNumericValueType::BaseType;

  if (int exponent = type_.Exponent(BaseType::kLength))
    type->setLength(exponent);
  if (int exponent = type_.Exponent(BaseType::kAngle))
    type->setAngle(exponent);
  if (int exponent = type_.Exponent(BaseType::kTime))
    type->setTime(exponent);
  if (int exponent = type_.Exponent(BaseType::kFrequency))
    type->setFrequency(exponent);
  if (int exponent = type_.Exponent(BaseType::kResolution))
    type->setResolution(exponent);
  if (int exponent = type_.Exponent(BaseType::kFlex))
    type->setFlex(exponent);
  if (int exponent = type_.Exponent(BaseType::kPercent))
    type->setPercent(exponent);
  if (type_.HasPercentHint()) {
    type->setPercentHint(
        CSSNumericValueType::BaseTypeToString(type_.PercentHint()));
  }
  return type;
}

CSSNumericValue* CSSNumericValue::add(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  PrependValueForArithmetic<kSumType>(values, this);

  if (CSSUnitValue* unit_value =
          MaybeSimplifyAsUnitValue(values, std::plus<double>())) {
    return unit_value;
  }
  return CSSMathSum::Create(std::move(values));
}

CSSNumericValue* CSSNumericValue::sub(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  std::transform(values.begin(), values.end(), values.begin(),
                 [](CSSNumericValue* v) { return v->Negate(); });
  PrependValueForArithmetic<kSumType>(values, this);

  if (CSSUnitValue* unit_value =
          MaybeSimplifyAsUnitValue(values, std::plus<double>())) {
    return unit_value;
  }
  return CSSMathSum::Create(std::move(values));
}

CSSNumericValue* CSSNumericValue::mul(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  PrependValueForArithmetic<kProductType>(values, this);

  if (CSSUnitValue* unit_value = MaybeMultiplyAsUnitValue(values))
    return unit_value;
  return CSSMathProduct::Create(std::move(values));
}

CSSNumericValue* CSSNumericValue::div(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  for (auto& v : values) {
    auto* invert_value = v->Invert();
    if (!invert_value) {
      exception_state.ThrowRangeError("Can't divide-by-zero");
      return nullptr;
    }
    v = invert_value;
  }

  PrependValueForArithmetic<kProductType>(values, this);

  if (CSSUnitValue* unit_value = MaybeMultiplyAsUnitValue(values))
    return unit_value;
  return CSSMathProduct::Create(std::move(values));
}

CSSNumericValue* CSSNumericValue::min(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  PrependValueForArithmetic<kMinType>(values, this);

  if (CSSUnitValue* unit_value = MaybeSimplifyAsUnitValue(
          values, [](double a, double b) { return std::min(a, b); })) {
    return unit_value;
  }
  return CSSMathMin::Create(std::move(values));
}

CSSNumericValue* CSSNumericValue::max(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  PrependValueForArithmetic<kMaxType>(values, this);

  if (CSSUnitValue* unit_value = MaybeSimplifyAsUnitValue(
          values, [](double a, double b) { return std::max(a, b); })) {
    return unit_value;
  }
  return CSSMathMax::Create(std::move(values));
}

bool CSSNumericValue::equals(const HeapVector<CSSNumberish>& args) {
  CSSNumericValueVector values = CSSNumberishesToNumericValues(args);
  return std::all_of(values.begin(), values.end(),
                     [this](const auto& v) { return this->Equals(*v); });
}

String CSSNumericValue::toString() const {
  StringBuilder result;
  BuildCSSText(Nested::kNo, ParenLess::kNo, result);
  return result.ToString();
}

CSSNumericValue* CSSNumericValue::Negate() {
  return CSSMathNegate::Create(this);
}

CSSNumericValue* CSSNumericValue::Invert() {
  return CSSMathInvert::Create(this);
}

CSSNumericValueVector CSSNumberishesToNumericValues(
    const HeapVector<CSSNumberish>& values) {
  CSSNumericValueVector result;
  for (const CSSNumberish& value : values) {
    result.push_back(CSSNumericValue::FromNumberish(value));
  }
  return result;
}

}  // namespace blink
