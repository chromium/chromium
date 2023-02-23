// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_fraction_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

MathMLFractionElement::MathMLFractionElement(Document& doc)
    : MathMLElement(mathml_names::kMfracTag, doc) {}

void MathMLFractionElement::AddMathFractionBarThicknessIfNeeded(
    ComputedStyleBuilder& builder,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kLinethicknessAttr))
    builder.SetMathFractionBarThickness(std::move(*length_or_percentage_value));
}

void MathMLFractionElement::ParseAttribute(
    const AttributeModificationParams& param) {
  if (param.name == mathml_names::kLinethicknessAttr &&
      param.new_value != param.old_value) {
    SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kAttribute));
  }
  MathMLElement::ParseAttribute(param);
}

}  // namespace blink
