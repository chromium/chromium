// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/mathml/layout_mathml_block_with_anonymous_mrow.h"

namespace blink {

LayoutMathMLBlockWithAnonymousMrow::LayoutMathMLBlockWithAnonymousMrow(
    Element* element)
    : LayoutMathMLBlock(element) {
  DCHECK(element);
}

void LayoutMathMLBlockWithAnonymousMrow::AddChild(LayoutObject* new_child,
                                                  LayoutObject* before_child) {
  LayoutBlock* anonymous_mrow = To<LayoutBlock>(FirstChild());
  if (!anonymous_mrow) {
    anonymous_mrow = LayoutBlock::CreateAnonymousWithParentAndDisplay(
        this, EDisplay::kBlockMath);
    LayoutMathMLBlock::AddChild(anonymous_mrow);
  }
  anonymous_mrow->AddChild(new_child, before_child);
}

}  // namespace blink
