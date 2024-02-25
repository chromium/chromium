// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_under_over_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

MathMLUnderOverElement::MathMLUnderOverElement(const QualifiedName& tagName,
                                               Document& document)
    : MathMLScriptsElement(tagName, document) {}

std::optional<bool> MathMLUnderOverElement::Accent() const {
  return BooleanAttribute(mathml_names::kAccentAttr);
}

std::optional<bool> MathMLUnderOverElement::AccentUnder() const {
  return BooleanAttribute(mathml_names::kAccentunderAttr);
}

void MathMLUnderOverElement::ParseAttribute(
    const AttributeModificationParams& param) {
  if ((param.name == mathml_names::kAccentAttr ||
       param.name == mathml_names::kAccentunderAttr) &&
      GetLayoutObject() && GetLayoutObject()->IsMathML() &&
      param.new_value != param.old_value) {
    GetLayoutObject()
        ->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
            layout_invalidation_reason::kAttributeChanged);
  }
  MathMLScriptsElement::ParseAttribute(param);
}

}  // namespace blink
