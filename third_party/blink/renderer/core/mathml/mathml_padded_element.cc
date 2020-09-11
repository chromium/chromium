// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_padded_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block_with_anonymous_mrow.h"

namespace blink {

MathMLPaddedElement::MathMLPaddedElement(Document& document)
    : MathMLRowElement(mathml_names::kMpaddedTag, document) {}

void MathMLPaddedElement::AddMathBaselineIfNeeded(
    ComputedStyle& style,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kHeightAttr, AllowPercentages::kNo))
    style.SetMathBaseline(std::move(*length_or_percentage_value));
}

void MathMLPaddedElement::AddMathPaddedDepthIfNeeded(
    ComputedStyle& style,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kDepthAttr, AllowPercentages::kNo))
    style.SetMathPaddedDepth(std::move(*length_or_percentage_value));
}

void MathMLPaddedElement::AddMathPaddedLSpaceIfNeeded(
    ComputedStyle& style,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kLspaceAttr))
    style.SetMathLSpace(std::move(*length_or_percentage_value));
}

void MathMLPaddedElement::AddMathPaddedVOffsetIfNeeded(
    ComputedStyle& style,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kVoffsetAttr))
    style.SetMathPaddedVOffset(std::move(*length_or_percentage_value));
}

void MathMLPaddedElement::ParseAttribute(
    const AttributeModificationParams& param) {
  if (param.name == mathml_names::kLspaceAttr ||
      param.name == mathml_names::kVoffsetAttr) {
    SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kAttribute));
    if (GetLayoutObject() && GetLayoutObject()->IsMathML()) {
      GetLayoutObject()
          ->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
              layout_invalidation_reason::kAttributeChanged);
    }
  }

  MathMLRowElement::ParseAttribute(param);
}

bool MathMLPaddedElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == mathml_names::kWidthAttr)
    return true;
  return MathMLElement::IsPresentationAttribute(name);
}

void MathMLPaddedElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == mathml_names::kWidthAttr) {
    if (!value.EndsWith('%')) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kWidth,
                                              value);
    }
  } else {
    MathMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

LayoutObject* MathMLPaddedElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy) {
  DCHECK_NE(legacy, LegacyLayout::kForce);
  if (!RuntimeEnabledFeatures::MathMLCoreEnabled() ||
      !style.IsDisplayMathType())
    return MathMLElement::CreateLayoutObject(style, legacy);
  return new LayoutNGMathMLBlockWithAnonymousMrow(this);
}

}  // namespace blink
