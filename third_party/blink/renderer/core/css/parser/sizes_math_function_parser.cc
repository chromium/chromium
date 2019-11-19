// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/sizes_math_function_parser.h"

#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

SizesMathFunctionParser::SizesMathFunctionParser(CSSParserTokenRange range,
                                                 MediaValues* media_values)
    : media_values_(media_values), result_(0) {
  is_valid_ = CalcToReversePolishNotation(range) && Calculate();
}

float SizesMathFunctionParser::Result() const {
  DCHECK(is_valid_);
  return result_;
}

static bool OperatorPriority(CSSMathOperator cc, bool& high_priority) {
  if (cc == CSSMathOperator::kAdd || cc == CSSMathOperator::kSubtract)
    high_priority = false;
  else if (cc == CSSMathOperator::kMultiply || cc == CSSMathOperator::kDivide)
    high_priority = true;
  else
    return false;
  return true;
}

bool SizesMathFunctionParser::HandleOperator(Vector<CSSParserToken>& stack,
                                             const CSSParserToken& token) {
  // If the token is not an operator, then return. Else determine the
  // precedence of the new operator (op1).
  bool incoming_operator_priority;
  if (!OperatorPriority(ParseCSSArithmeticOperator(token),
                        incoming_operator_priority))
    return false;

  while (!stack.IsEmpty()) {
    // While there is an operator (op2) at the top of the stack,
    // determine its precedence, and...
    const CSSParserToken& top_of_stack = stack.back();
    if (top_of_stack.GetType() != kDelimiterToken)
      break;
    bool stack_operator_priority;
    if (!OperatorPriority(ParseCSSArithmeticOperator(top_of_stack),
                          stack_operator_priority))
      return false;
    // ...if op1 is left-associative (all currently supported
    // operators are) and its precedence is equal to that of op2, or
    // op1 has precedence less than that of op2, ...
    if (incoming_operator_priority && !stack_operator_priority)
      break;
    // ...pop op2 off the stack and add it to the output queue.
    AppendOperator(top_of_stack);
    stack.pop_back();
  }
  // Push op1 onto the stack.
  stack.push_back(token);
  return true;
}

bool SizesMathFunctionParser::HandleRightParenthesis(
    Vector<CSSParserToken>& stack) {
  // If the token is a right parenthesis:
  // Until the token at the top of the stack is a left parenthesis or a
  // function, pop operators off the stack onto the output queue.
  // Also count the number of commas to get the number of function
  // parameters if this right parenthesis closes a function.
  wtf_size_t comma_count = 0;
  while (!stack.IsEmpty() && stack.back().GetType() != kLeftParenthesisToken &&
         stack.back().GetType() != kFunctionToken) {
    if (stack.back().GetType() == kCommaToken)
      ++comma_count;
    else
      AppendOperator(stack.back());
    stack.pop_back();
  }
  // If the stack runs out without finding a left parenthesis, then there
  // are mismatched parentheses.
  if (stack.IsEmpty())
    return false;

  CSSParserToken left_side = stack.back();
  stack.pop_back();

  if (left_side.GetType() == kLeftParenthesisToken ||
      left_side.FunctionId() == CSSValueID::kCalc) {
    // There should be exactly one calculation within calc() or parentheses.
    return !comma_count;
  }

  if (left_side.FunctionId() == CSSValueID::kClamp) {
    if (comma_count != 2)
      return false;
    // Convert clamp(MIN, VAL, MAX) into max(MIN, min(VAL, MAX))
    // https://www.w3.org/TR/css-values-4/#calc-notation
    value_list_.emplace_back(CSSMathOperator::kMin);
    value_list_.emplace_back(CSSMathOperator::kMax);
    return true;
  }

  // Break variadic min/max() into binary operations to fit in the reverse
  // polish notation.
  CSSMathOperator op = left_side.FunctionId() == CSSValueID::kMin
                           ? CSSMathOperator::kMin
                           : CSSMathOperator::kMax;
  for (wtf_size_t i = 0; i < comma_count; ++i)
    value_list_.emplace_back(op);
  return true;
}

bool SizesMathFunctionParser::HandleComma(Vector<CSSParserToken>& stack,
                                          const CSSParserToken& token) {
  if (!RuntimeEnabledFeatures::CSSComparisonFunctionsEnabled())
    return false;
  // Treat comma as a binary right-associative operation for now, so that
  // when reaching the right parenthesis of the function, we can get the
  // number of parameters by counting the number of commas.
  while (!stack.IsEmpty() && stack.back().GetType() != kFunctionToken &&
         stack.back().GetType() != kLeftParenthesisToken &&
         stack.back().GetType() != kCommaToken) {
    AppendOperator(stack.back());
    stack.pop_back();
  }
  // Commas are allowed as function parameter separators only
  if (stack.IsEmpty() || stack.back().GetType() == kLeftParenthesisToken)
    return false;
  stack.push_back(token);
  return true;
}

void SizesMathFunctionParser::AppendNumber(const CSSParserToken& token) {
  SizesMathValue value;
  value.value = token.NumericValue();
  value_list_.push_back(value);
}

bool SizesMathFunctionParser::AppendLength(const CSSParserToken& token) {
  SizesMathValue value;
  double result = 0;
  if (!media_values_->ComputeLength(token.NumericValue(), token.GetUnitType(),
                                    result))
    return false;
  value.value = result;
  value.is_length = true;
  value_list_.push_back(value);
  return true;
}

void SizesMathFunctionParser::AppendOperator(const CSSParserToken& token) {
  value_list_.emplace_back(ParseCSSArithmeticOperator(token));
}

bool SizesMathFunctionParser::CalcToReversePolishNotation(
    CSSParserTokenRange range) {
  // This method implements the shunting yard algorithm, to turn the calc syntax
  // into a reverse polish notation.
  // http://en.wikipedia.org/wiki/Shunting-yard_algorithm

  Vector<CSSParserToken> stack;
  while (!range.AtEnd()) {
    const CSSParserToken& token = range.Consume();
    switch (token.GetType()) {
      case kNumberToken:
        AppendNumber(token);
        break;
      case kDimensionToken:
        if (!CSSPrimitiveValue::IsLength(token.GetUnitType()) ||
            !AppendLength(token))
          return false;
        break;
      case kDelimiterToken:
        if (!HandleOperator(stack, token))
          return false;
        break;
      case kFunctionToken:
        if (RuntimeEnabledFeatures::CSSComparisonFunctionsEnabled()) {
          if (token.FunctionId() == CSSValueID::kMin ||
              token.FunctionId() == CSSValueID::kMax ||
              token.FunctionId() == CSSValueID::kClamp) {
            stack.push_back(token);
            break;
          }
        }
        if (token.FunctionId() != CSSValueID::kCalc)
          return false;
        // "calc(" is the same as "("
        FALLTHROUGH;
      case kLeftParenthesisToken:
        // If the token is a left parenthesis, then push it onto the stack.
        stack.push_back(token);
        break;
      case kRightParenthesisToken:
        if (!HandleRightParenthesis(stack))
          return false;
        break;
      case kCommaToken:
        if (!HandleComma(stack, token))
          return false;
        break;
      case kWhitespaceToken:
      case kEOFToken:
        break;
      case kCommentToken:
        NOTREACHED();
        FALLTHROUGH;
      case kCDOToken:
      case kCDCToken:
      case kAtKeywordToken:
      case kHashToken:
      case kUrlToken:
      case kBadUrlToken:
      case kPercentageToken:
      case kIncludeMatchToken:
      case kDashMatchToken:
      case kPrefixMatchToken:
      case kSuffixMatchToken:
      case kSubstringMatchToken:
      case kColumnToken:
      case kUnicodeRangeToken:
      case kIdentToken:
      case kColonToken:
      case kSemicolonToken:
      case kLeftBraceToken:
      case kLeftBracketToken:
      case kRightBraceToken:
      case kRightBracketToken:
      case kStringToken:
      case kBadStringToken:
        return false;
    }
  }

  // When there are no more tokens to read:
  // While there are still operator tokens in the stack:
  while (!stack.IsEmpty()) {
    // If the operator token on the top of the stack is a parenthesis, then
    // there are unclosed parentheses.
    CSSParserTokenType type = stack.back().GetType();
    if (type != kLeftParenthesisToken && type != kFunctionToken) {
      // Pop the operator onto the output queue.
      AppendOperator(stack.back());
    }
    stack.pop_back();
  }
  return true;
}

static bool OperateOnStack(Vector<SizesMathValue>& stack,
                           CSSMathOperator operation) {
  if (stack.size() < 2)
    return false;
  SizesMathValue right_operand = stack.back();
  stack.pop_back();
  SizesMathValue left_operand = stack.back();
  stack.pop_back();
  bool is_length;
  switch (operation) {
    case CSSMathOperator::kAdd:
      if (right_operand.is_length != left_operand.is_length)
        return false;
      is_length = (right_operand.is_length && left_operand.is_length);
      stack.push_back(
          SizesMathValue(left_operand.value + right_operand.value, is_length));
      break;
    case CSSMathOperator::kSubtract:
      if (right_operand.is_length != left_operand.is_length)
        return false;
      is_length = (right_operand.is_length && left_operand.is_length);
      stack.push_back(
          SizesMathValue(left_operand.value - right_operand.value, is_length));
      break;
    case CSSMathOperator::kMultiply:
      if (right_operand.is_length && left_operand.is_length)
        return false;
      is_length = (right_operand.is_length || left_operand.is_length);
      stack.push_back(
          SizesMathValue(left_operand.value * right_operand.value, is_length));
      break;
    case CSSMathOperator::kDivide:
      if (right_operand.is_length || right_operand.value == 0)
        return false;
      stack.push_back(SizesMathValue(left_operand.value / right_operand.value,
                                     left_operand.is_length));
      break;
    case CSSMathOperator::kMin:
      if (right_operand.is_length != left_operand.is_length)
        return false;
      is_length = (right_operand.is_length && left_operand.is_length);
      stack.push_back(SizesMathValue(
          std::min(left_operand.value, right_operand.value), is_length));
      break;
    case CSSMathOperator::kMax:
      if (right_operand.is_length != left_operand.is_length)
        return false;
      is_length = (right_operand.is_length && left_operand.is_length);
      stack.push_back(SizesMathValue(
          std::max(left_operand.value, right_operand.value), is_length));
      break;
    default:
      return false;
  }
  return true;
}

bool SizesMathFunctionParser::Calculate() {
  Vector<SizesMathValue> stack;
  for (const auto& value : value_list_) {
    if (value.operation == CSSMathOperator::kInvalid) {
      stack.push_back(value);
    } else {
      if (!OperateOnStack(stack, value.operation))
        return false;
    }
  }
  if (stack.size() == 1 && stack.back().is_length) {
    result_ = std::max(clampTo<float>(stack.back().value), (float)0.0);
    return true;
  }
  return false;
}

}  // namespace blink
