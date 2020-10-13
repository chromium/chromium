// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_row_element.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"
#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

MathMLRowElement::MathMLRowElement(const QualifiedName& tagName,
                                   Document& document)
    : MathMLElement(tagName, document) {}

LayoutObject* MathMLRowElement::CreateLayoutObject(const ComputedStyle& style,
                                                   LegacyLayout legacy) {
  DCHECK(!style.IsDisplayMathType() || legacy != LegacyLayout::kForce);
  if (!RuntimeEnabledFeatures::MathMLCoreEnabled() ||
      !style.IsDisplayMathType())
    return MathMLElement::CreateLayoutObject(style, legacy);
  return new LayoutNGMathMLBlock(this);
}

void MathMLRowElement::ChildrenChanged(const ChildrenChange& change) {
  if (change.by_parser == ChildrenChangeSource::kAPI) {
    for (auto* child = firstChild(); child; child = child->nextSibling()) {
      if (child->HasTagName(mathml_names::kMoTag)) {
        // TODO(crbug.com/1124298): make this work for embellished operators.
        static_cast<MathMLOperatorElement*>(child)
            ->CheckFormAfterSiblingChange();
      }
    }
  }

  MathMLElement::ChildrenChanged(change);
}

}  // namespace blink
