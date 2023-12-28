// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/mathml/layout_mathml_block.h"

#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_under_over_element.h"

namespace blink {

LayoutMathMLBlock::LayoutMathMLBlock(Element* element) : LayoutBlock(element) {}

bool LayoutMathMLBlock::IsMathMLRoot() const {
  NOT_DESTROYED();
  return GetNode() && GetNode()->HasTagName(mathml_names::kMathTag);
}

bool LayoutMathMLBlock::IsChildAllowed(LayoutObject* child,
                                       const ComputedStyle&) const {
  return child->GetNode() && IsA<MathMLElement>(child->GetNode());
}

bool LayoutMathMLBlock::CanHaveChildren() const {
  if (GetNode() && GetNode()->HasTagName(mathml_names::kMspaceTag))
    return false;
  return LayoutBlock::CanHaveChildren();
}

void LayoutMathMLBlock::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  LayoutBlock::StyleDidChange(diff, old_style);
  if (!old_style)
    return;
  if (IsA<MathMLUnderOverElement>(GetNode()) &&
      old_style->MathStyle() != StyleRef().MathStyle()) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kAttributeChanged);
  }
}

}  // namespace blink
