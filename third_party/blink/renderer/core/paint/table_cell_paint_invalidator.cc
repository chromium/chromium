// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/table_cell_paint_invalidator.h"

#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/paint/block_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

namespace {

bool DisplayItemClientIsFullyInvalidated(const DisplayItemClient& client) {
  return IsFullPaintInvalidationReason(client.GetPaintInvalidationReason());
}

void InvalidateContainerForCellGeometryChange(
    const LayoutObject& container,
    const PaintInvalidatorContext& container_context) {
  // We only need to do this if the container hasn't been fully invalidated.
  DCHECK(!DisplayItemClientIsFullyInvalidated(container));

  // At this time we have already walked the container for paint invalidation,
  // so we should invalidate the container immediately here instead of setting
  // paint invalidation flags.
  container_context.painting_layer->SetNeedsRepaint();
  container.InvalidateDisplayItemClients(PaintInvalidationReason::kLayout);
}

}  // namespace

void TableCellPaintInvalidator::InvalidatePaint() {
  // The cell's containing row and section paint backgrounds behind the cell,
  // and the row or table paints collapsed borders. If the cell's geometry
  // changed and the containers which will paint backgrounds and/or collapsed
  // borders haven't been full invalidated, invalidate the containers.
  if (context_.old_paint_offset != context_.fragment_data->PaintOffset() ||
      cell_.Size() != cell_.PreviousSize()) {
    // Table row background is painted inside cell's geometry.
    const auto& row = *cell_.Parent();
    DCHECK(row.IsTableRow());
    if (!DisplayItemClientIsFullyInvalidated(row) &&
        row.StyleRef().HasBackground()) {
      InvalidateContainerForCellGeometryChange(row, *context_.ParentContext());
    }
    // Table section background is painted inside cell's geometry.
    const auto& section = *row.Parent();
    DCHECK(section.IsTableSection());
    if (!DisplayItemClientIsFullyInvalidated(section) &&
        section.StyleRef().HasBackground()) {
      InvalidateContainerForCellGeometryChange(
          section, *context_.ParentContext()->ParentContext());
    }
    // Table paints its background, and column backgrounds inside cell's
    // geometry.
    const auto& table = *cell_.Table();
    if (!DisplayItemClientIsFullyInvalidated(table) &&
        (table.HasBackgroundForPaint() || table.HasCollapsedBorders())) {
      InvalidateContainerForCellGeometryChange(
          table, *context_.ParentContext()->ParentContext()->ParentContext());
    }
  }

  BlockPaintInvalidator(cell_).InvalidatePaint(context_);
}

}  // namespace blink
