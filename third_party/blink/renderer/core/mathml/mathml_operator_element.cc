// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

MathMLOperatorElement::MathMLOperatorElement(Document& doc)
    : MathMLElement(mathml_names::kMoTag, doc) {}

void MathMLOperatorElement::AddMathLSpaceIfNeeded(
    ComputedStyle& style,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kLspaceAttr)) {
    style.SetMathLSpace(std::move(*length_or_percentage_value));
  }
}

void MathMLOperatorElement::AddMathRSpaceIfNeeded(
    ComputedStyle& style,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kRspaceAttr)) {
    style.SetMathRSpace(std::move(*length_or_percentage_value));
  }
}

void MathMLOperatorElement::AddMathMinSizeIfNeeded(
    ComputedStyle& style,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kMinsizeAttr)) {
    style.SetMathMinSize(std::move(*length_or_percentage_value));
  }
}

void MathMLOperatorElement::AddMathMaxSizeIfNeeded(
    ComputedStyle& style,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kMaxsizeAttr)) {
    style.SetMathMaxSize(std::move(*length_or_percentage_value));
  }
}

}  // namespace blink
