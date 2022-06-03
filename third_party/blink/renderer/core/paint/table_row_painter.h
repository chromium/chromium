// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_ROW_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_ROW_PAINTER_H_

#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CellSpan;
class LayoutTableRow;
struct PaintInfo;

class TableRowPainter {
  STACK_ALLOCATED();

 public:
  TableRowPainter(const LayoutTableRow& layout_table_row)
      : layout_table_row_(layout_table_row) {}

  void Paint(const PaintInfo&);
  void PaintOutline(const PaintInfo&);
  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const CellSpan& dirtied_columns);
  void PaintCollapsedBorders(const PaintInfo&, const CellSpan& dirtied_columns);

 private:
  void HandleChangedPartialPaint(const PaintInfo&,
                                 const CellSpan& dirtied_columns);

  const LayoutTableRow& layout_table_row_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_ROW_PAINTER_H_
