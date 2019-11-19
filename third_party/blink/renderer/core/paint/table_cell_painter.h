// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_CELL_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_CELL_PAINTER_H_

#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutTableCell;
class LayoutObject;
struct PaintInfo;
struct PhysicalOffset;
struct PhysicalRect;

class TableCellPainter {
  STACK_ALLOCATED();

 public:
  TableCellPainter(const LayoutTableCell& layout_table_cell)
      : layout_table_cell_(layout_table_cell) {}

  void PaintContainerBackgroundBehindCell(
      const PaintInfo&,
      const LayoutObject& background_object);
  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalOffset& paint_offset);
  void PaintMask(const PaintInfo&, const PhysicalOffset& paint_offset);

  PhysicalRect PaintRectNotIncludingVisualOverflow(
      const PhysicalOffset& paint_offset);

 private:
  void PaintBackground(const PaintInfo&,
                       const PhysicalRect&,
                       const LayoutObject& background_object);

  const LayoutTableCell& layout_table_cell_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_CELL_PAINTER_H_
