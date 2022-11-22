// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/table_cell_paint_invalidator.h"

#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_col.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/paint/block_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

static bool DisplayItemClientIsFullyInvalidated(
    const DisplayItemClient& client) {
  return IsFullPaintInvalidationReason(client.GetPaintInvalidationReason());
}

void TableCellPaintInvalidator::InvalidateContainerForCellGeometryChange(
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

void TableCellPaintInvalidator::InvalidatePaint() {
  // The cell's containing row and section paint backgrounds behind the cell,
  // and the row or table paints collapsed borders. If the cell's geometry
  // changed and the containers which will paint backgrounds and/or collapsed
  // borders haven't been full invalidated, invalidate the containers.
  if (context_.old_paint_offset != context_.fragment_data->PaintOffset() ||
      cell_.Size() != cell_.PreviousSize()) {
    const auto& row = *cell_.Row();
    const auto& section = *row.Section();
    const auto& table = *section.Table();
    if (!DisplayItemClientIsFullyInvalidated(row) &&
        (row.StyleRef().HasBackground() || table.HasCollapsedBorders())) {
      InvalidateContainerForCellGeometryChange(row, *context_.ParentContext());
      // Mark the table as needing repaint, in order to paint collapsed borders.
      context_.ParentContext()
          ->ParentContext()
          ->ParentContext()
          ->painting_layer->SetNeedsRepaint();
    }

    if (!DisplayItemClientIsFullyInvalidated(section)) {
      bool section_paints_background = section.StyleRef().HasBackground();
      if (!section_paints_background) {
        auto col_and_colgroup = section.Table()->ColElementAtAbsoluteColumn(
            cell_.AbsoluteColumnIndex());
        if ((col_and_colgroup.col &&
             col_and_colgroup.col->StyleRef().HasBackground()) ||
            (col_and_colgroup.colgroup &&
             col_and_colgroup.colgroup->StyleRef().HasBackground()))
          section_paints_background = true;
      }
      if (section_paints_background) {
        InvalidateContainerForCellGeometryChange(
            section, *context_.ParentContext()->ParentContext());
      }
    }
  }

  BlockPaintInvalidator(cell_).InvalidatePaint(context_);
}

}  // namespace blink
