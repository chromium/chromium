// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/table_paint_invalidator.h"

#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_col.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/paint/box_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"

namespace blink {

void TablePaintInvalidator::InvalidatePaint() {
  BoxPaintInvalidator(table_, context_).InvalidatePaint();

  // If any col changed background, we need to invalidate all sections because
  // col background paints into section's background display item.
  bool has_col_changed_background = false;
  if (table_.HasColElements()) {
    bool visual_rect_changed =
        context_.old_visual_rect != context_.fragment_data->VisualRect();
    for (LayoutTableCol* col = table_.FirstColumn(); col;
         col = col->NextColumn()) {
      // LayoutTableCol uses the table's LocalVisualRect(). Should check column
      // for paint invalidation when table's visual rect changed.
      if (visual_rect_changed)
        col->SetShouldCheckForPaintInvalidation();
      // This ensures that the BackgroundNeedsFullPaintInvalidation flag is
      // up-to-date.
      col->EnsureIsReadyForPaintInvalidation();
      if (col->BackgroundNeedsFullPaintInvalidation()) {
        has_col_changed_background = true;
        break;
      }
    }
  }

  if (has_col_changed_background) {
    for (LayoutObject* child = table_.FirstChild(); child;
         child = child->NextSibling()) {
      if (!child->IsTableSection())
        continue;
      LayoutTableSection* section = To<LayoutTableSection>(child);
      section->EnsureIsReadyForPaintInvalidation();
      ObjectPaintInvalidator(*section)
          .SlowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
              *section, PaintInvalidationReason::kStyle);
    }
  }
}

}  // namespace blink
