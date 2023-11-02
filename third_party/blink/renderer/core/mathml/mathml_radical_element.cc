// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_radical_element.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block_with_anonymous_mrow.h"

namespace blink {

MathMLRadicalElement::MathMLRadicalElement(const QualifiedName& tagName,
                                           Document& document)
    : MathMLRowElement(tagName, document) {}

bool MathMLRadicalElement::HasIndex() const {
  return HasTagName(mathml_names::kMrootTag);
}

LayoutObject* MathMLRadicalElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy) {
  if (!RuntimeEnabledFeatures::MathMLCoreEnabled() ||
      !style.IsDisplayMathType() || legacy == LegacyLayout::kForce)
    return MathMLElement::CreateLayoutObject(style, legacy);
  if (HasTagName(mathml_names::kMsqrtTag))
    return MakeGarbageCollected<LayoutNGMathMLBlockWithAnonymousMrow>(this);
  return MakeGarbageCollected<LayoutNGMathMLBlock>(this);
}

}  // namespace blink
