// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_row_element.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

MathMLRowElement::MathMLRowElement(const QualifiedName& tagName,
                                   Document& document)
    : MathMLElement(tagName, document) {}

LayoutObject* MathMLRowElement::CreateLayoutObject(const ComputedStyle& style,
                                                   LegacyLayout legacy) {
  DCHECK_NE(legacy, LegacyLayout::kForce);
  if (!RuntimeEnabledFeatures::MathMLCoreEnabled() ||
      (!style.IsDisplayMathType() && !HasTagName(mathml_names::kMathTag)))
    return MathMLElement::CreateLayoutObject(style, legacy);
  return new LayoutNGMathMLBlock(this);
}

}  // namespace blink
