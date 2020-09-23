// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"

#include "third_party/blink/renderer/core/html/html_table_col_element.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"

namespace blink {

LayoutNGTableColumn::LayoutNGTableColumn(Element* element)
    : LayoutBox(element) {
  UpdateFromElement();
}

void LayoutNGTableColumn::StyleDidChange(StyleDifference diff,
                                         const ComputedStyle* old_style) {
  CheckIsNotDestroyed();
  if (diff.NeedsPaintInvalidation() && old_style) {
    if (LayoutNGTable* table = Table()) {
      if (NGTableBorders::HasBorder(old_style) ||
          NGTableBorders::HasBorder(Style()))
        table->GridBordersChanged();
    }
  }
  LayoutBoxModelObject::StyleDidChange(diff, old_style);
}

void LayoutNGTableColumn::ImageChanged(WrappedImagePtr, CanDeferInvalidation) {
  CheckIsNotDestroyed();
  if (LayoutNGTable* table = Table()) {
    table->SetShouldDoFullPaintInvalidationWithoutGeometryChange(
        PaintInvalidationReason::kImage);
  }
}

bool LayoutNGTableColumn::IsChildAllowed(LayoutObject* child,
                                         const ComputedStyle& style) const {
  CheckIsNotDestroyed();
  return child->IsLayoutTableCol() && style.Display() == EDisplay::kTableColumn;
}

bool LayoutNGTableColumn::CanHaveChildren() const {
  CheckIsNotDestroyed();
  // <col> cannot have children.
  return IsColumnGroup();
}

void LayoutNGTableColumn::ClearNeedsLayoutForChildren() const {
  CheckIsNotDestroyed();
  LayoutObject* child = children_.FirstChild();
  while (child) {
    child->ClearNeedsLayout();
    child = child->NextSibling();
  }
}

LayoutNGTable* LayoutNGTableColumn::Table() const {
  CheckIsNotDestroyed();
  LayoutObject* table = Parent();
  if (table && !table->IsTable())
    table = table->Parent();
  if (table) {
    DCHECK(table->IsTable());
    return To<LayoutNGTable>(table);
  }
  return nullptr;
}

void LayoutNGTableColumn::UpdateFromElement() {
  CheckIsNotDestroyed();
  unsigned old_span = span_;
  if (const auto* tc = DynamicTo<HTMLTableColElement>(GetNode())) {
    span_ = tc->span();
  } else {
    span_ = 1;
  }
  if (span_ != old_span && Style() && Parent()) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kAttributeChanged);
  }
}

}  // namespace blink
