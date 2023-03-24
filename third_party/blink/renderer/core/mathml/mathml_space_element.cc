// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_space_element.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

MathMLSpaceElement::MathMLSpaceElement(Document& doc)
    : MathMLElement(mathml_names::kMspaceTag, doc) {}

void MathMLSpaceElement::AddMathBaselineIfNeeded(
    ComputedStyleBuilder& builder,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kHeightAttr, AllowPercentages::kNo,
          CSSPrimitiveValue::ValueRange::kNonNegative)) {
    builder.SetMathBaseline(std::move(*length_or_percentage_value));
  }
}

bool MathMLSpaceElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == mathml_names::kWidthAttr || name == mathml_names::kHeightAttr ||
      name == mathml_names::kDepthAttr)
    return true;
  return MathMLElement::IsPresentationAttribute(name);
}

void MathMLSpaceElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == mathml_names::kWidthAttr) {
    if (const CSSPrimitiveValue* width_value =
            ParseMathLength(name, AllowPercentages::kNo,
                            CSSPrimitiveValue::ValueRange::kNonNegative)) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kWidth,
                                              *width_value);
    }
  } else if (name == mathml_names::kHeightAttr ||
             name == mathml_names::kDepthAttr) {
    // TODO(rbuis): this can be simplified once attr() is supported for
    // width/height.
    const CSSPrimitiveValue* height_value =
        ParseMathLength(mathml_names::kHeightAttr, AllowPercentages::kNo,
                        CSSPrimitiveValue::ValueRange::kNonNegative);
    const CSSPrimitiveValue* depth_value =
        ParseMathLength(mathml_names::kDepthAttr, AllowPercentages::kNo,
                        CSSPrimitiveValue::ValueRange::kNonNegative);
    const CSSPrimitiveValue* attribute_value =
        (name == mathml_names::kHeightAttr ? height_value : depth_value);
    if (height_value && depth_value) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kHeight,
          "calc(" + FastGetAttribute(mathml_names::kHeightAttr) + " + " +
              FastGetAttribute(mathml_names::kDepthAttr) + ")");
    } else if (attribute_value) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kHeight,
                                              *attribute_value);
    }
  } else {
    MathMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

}  // namespace blink
