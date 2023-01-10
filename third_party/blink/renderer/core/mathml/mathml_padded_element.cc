// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_padded_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block_with_anonymous_mrow.h"

namespace blink {

MathMLPaddedElement::MathMLPaddedElement(Document& document)
    : MathMLRowElement(mathml_names::kMpaddedTag, document) {}

void MathMLPaddedElement::AddMathBaselineIfNeeded(
    ComputedStyleBuilder& builder,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kHeightAttr, AllowPercentages::kNo))
    builder.SetMathBaseline(std::move(*length_or_percentage_value));
}

void MathMLPaddedElement::AddMathPaddedDepthIfNeeded(
    ComputedStyleBuilder& builder,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kDepthAttr, AllowPercentages::kNo))
    builder.SetMathPaddedDepth(std::move(*length_or_percentage_value));
}

void MathMLPaddedElement::AddMathPaddedLSpaceIfNeeded(
    ComputedStyleBuilder& builder,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kLspaceAttr, AllowPercentages::kNo)) {
    builder.SetMathLSpace(std::move(*length_or_percentage_value));
  }
}

void MathMLPaddedElement::AddMathPaddedVOffsetIfNeeded(
    ComputedStyleBuilder& builder,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kVoffsetAttr, AllowPercentages::kNo)) {
    builder.SetMathPaddedVOffset(std::move(*length_or_percentage_value));
  }
}

void MathMLPaddedElement::ParseAttribute(
    const AttributeModificationParams& param) {
  if (param.name == mathml_names::kLspaceAttr ||
      param.name == mathml_names::kVoffsetAttr) {
    // TODO(crbug.com/1121113): Isn't it enough to set needs style recalc and
    // let the style system perform proper layout and paint invalidation?
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
    if (const CSSPrimitiveValue* width_value =
            ParseMathLength(name, AllowPercentages::kNo)) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kWidth,
                                              *width_value);
    }
  } else {
    MathMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

LayoutObject* MathMLPaddedElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy) {
  if (!RuntimeEnabledFeatures::MathMLCoreEnabled() ||
      !style.IsDisplayMathType() || legacy == LegacyLayout::kForce)
    return MathMLElement::CreateLayoutObject(style, legacy);
  return MakeGarbageCollected<LayoutNGMathMLBlockWithAnonymousMrow>(this);
}

}  // namespace blink
