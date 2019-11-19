// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_math_operator.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSMathOperator ParseCSSArithmeticOperator(const CSSParserToken& token) {
  if (token.GetType() != kDelimiterToken)
    return CSSMathOperator::kInvalid;
  switch (token.Delimiter()) {
    case '+':
      return CSSMathOperator::kAdd;
    case '-':
      return CSSMathOperator::kSubtract;
    case '*':
      return CSSMathOperator::kMultiply;
    case '/':
      return CSSMathOperator::kDivide;
    default:
      return CSSMathOperator::kInvalid;
  }
}

String ToString(CSSMathOperator op) {
  switch (op) {
    case CSSMathOperator::kAdd:
      return "+";
    case CSSMathOperator::kSubtract:
      return "-";
    case CSSMathOperator::kMultiply:
      return "*";
    case CSSMathOperator::kDivide:
      return "/";
    case CSSMathOperator::kMin:
      return "min";
    case CSSMathOperator::kMax:
      return "max";
    default:
      NOTREACHED();
      return String();
  }
}

}  // namespace blink
