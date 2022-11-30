// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_table_cell_with_anonymous_mrow.h"

namespace blink {

LayoutNGTableCellWithAnonymousMrow::LayoutNGTableCellWithAnonymousMrow(
    Element* element)
    : LayoutNGTableCell(element) {
  DCHECK(element);
}

void LayoutNGTableCellWithAnonymousMrow::AddChild(LayoutObject* new_child,
                                                  LayoutObject* before_child) {
  LayoutBlock* anonymous_mrow = To<LayoutBlock>(FirstChild());
  if (!anonymous_mrow) {
    anonymous_mrow = LayoutBlock::CreateAnonymousWithParentAndDisplay(
        this, EDisplay::kBlockMath);
    LayoutNGTableCell::AddChild(anonymous_mrow);
  }
  anonymous_mrow->AddChild(new_child, before_child);
}

}  // namespace blink
