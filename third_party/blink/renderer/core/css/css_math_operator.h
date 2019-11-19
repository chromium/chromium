// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_OPERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_OPERATOR_H_

#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CSSParserToken;

enum class CSSMathOperator {
  kAdd,
  kSubtract,
  kMultiply,
  kDivide,
  kMin,
  kMax,
  kInvalid
};

CSSMathOperator ParseCSSArithmeticOperator(const CSSParserToken& token);
String ToString(CSSMathOperator);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_OPERATOR_H_
