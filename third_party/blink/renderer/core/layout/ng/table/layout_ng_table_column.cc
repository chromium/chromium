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
  NOT_DESTROYED();
  if (diff.HasDifference()) {
    if (LayoutNGTable* table = Table()) {
      if (old_style && diff.NeedsPaintInvalidation()) {
        // Regenerate table borders if needed
        if (!old_style->BorderVisuallyEqual(StyleRef()) ||
            (diff.TextDecorationOrColorChanged() &&
             StyleRef().HasBorderColorReferencingCurrentColor())) {
          table->GridBordersChanged();
        }
        // Table paints column background. Tell table to repaint.
        if (StyleRef().HasBackground() || old_style->HasBackground()) {
          table->SetShouldDoFullPaintInvalidationWithoutGeometryChange(
              PaintInvalidationReason::kBackground);
        }
      }
      if (diff.NeedsLayout()) {
        table->SetIntrinsicLogicalWidthsDirty();
      }
    }
  }
  LayoutBoxModelObject::StyleDidChange(diff, old_style);
}

void LayoutNGTableColumn::ImageChanged(WrappedImagePtr, CanDeferInvalidation) {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table()) {
    table->SetShouldDoFullPaintInvalidationWithoutGeometryChange(
        PaintInvalidationReason::kImage);
  }
}

void LayoutNGTableColumn::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutBox::InsertedIntoTree();
  DCHECK(Table());
  if (StyleRef().HasBackground()) {
    Table()->SetShouldDoFullPaintInvalidationWithoutGeometryChange(
        PaintInvalidationReason::kBackground);
  }
}

void LayoutNGTableColumn::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutBox::WillBeRemovedFromTree();
  DCHECK(Table());
  if (StyleRef().HasBackground()) {
    Table()->SetShouldDoFullPaintInvalidationWithoutGeometryChange(
        PaintInvalidationReason::kBackground);
  }
}

bool LayoutNGTableColumn::IsChildAllowed(LayoutObject* child,
                                         const ComputedStyle& style) const {
  NOT_DESTROYED();
  return child->IsLayoutTableCol() && style.Display() == EDisplay::kTableColumn;
}

bool LayoutNGTableColumn::CanHaveChildren() const {
  NOT_DESTROYED();
  // <col> cannot have children.
  return IsColumnGroup();
}

void LayoutNGTableColumn::ClearNeedsLayoutForChildren() const {
  NOT_DESTROYED();
  LayoutObject* child = children_.FirstChild();
  while (child) {
    child->ClearNeedsLayout();
    child = child->NextSibling();
  }
}

LayoutNGTable* LayoutNGTableColumn::Table() const {
  NOT_DESTROYED();
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
  NOT_DESTROYED();
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
