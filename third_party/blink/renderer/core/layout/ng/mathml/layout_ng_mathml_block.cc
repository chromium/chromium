// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"

#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_under_over_element.h"

namespace blink {

LayoutNGMathMLBlock::LayoutNGMathMLBlock(Element* element)
    : LayoutBlock(element) {}

bool LayoutNGMathMLBlock::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectMathML ||
         (type == kLayoutObjectMathMLRoot && GetNode() &&
          GetNode()->HasTagName(mathml_names::kMathTag)) ||
         LayoutBlock::IsOfType(type);
}

bool LayoutNGMathMLBlock::IsChildAllowed(LayoutObject* child,
                                         const ComputedStyle&) const {
  return child->GetNode() && IsA<MathMLElement>(child->GetNode());
}

bool LayoutNGMathMLBlock::CanHaveChildren() const {
  if (GetNode() && GetNode()->HasTagName(mathml_names::kMspaceTag))
    return false;
  return LayoutBlock::CanHaveChildren();
}

void LayoutNGMathMLBlock::StyleDidChange(StyleDifference diff,
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
