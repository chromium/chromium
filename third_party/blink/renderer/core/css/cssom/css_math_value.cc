// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_math_value.h"

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"

namespace blink {

const CSSValue* CSSMathValue::ToCSSValue() const {
  CSSMathExpressionNode* node = ToCalcExpressionNode();
  if (!node) {
    return nullptr;
  }
  return CSSMathFunctionValue::Create(node);
}

}  // namespace blink
