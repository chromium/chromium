// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TABLE_PAINTERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TABLE_PAINTERS_H_

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_fragment_data.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class BoxDecorationData;
class LayoutBox;
class NGPhysicalBoxFragment;
struct PaintInfo;
struct PhysicalOffset;
struct PhysicalRect;

class NGTablePainter {
  STACK_ALLOCATED();

 public:
  explicit NGTablePainter(const NGPhysicalBoxFragment& table_wrapper_fragment)
      : fragment_(table_wrapper_fragment) {
    DCHECK(fragment_.IsTableNG());
  }

  bool WillCheckColumnBackgrounds();

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalRect&,
                                    const BoxDecorationData&);

  void PaintCollapsedBorders(const PaintInfo&,
                             const PhysicalOffset&,
                             const gfx::Rect& visual_rect);

 private:
  const NGPhysicalBoxFragment& fragment_;
};

class NGTableSectionPainter {
  STACK_ALLOCATED();

 public:
  explicit NGTableSectionPainter(
      const NGPhysicalBoxFragment& table_section_fragment)
      : fragment_(table_section_fragment) {
    DCHECK(fragment_.IsTableNGSection());
  }

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalRect&,
                                    const BoxDecorationData&);

  void PaintColumnsBackground(const PaintInfo&,
                              const PhysicalOffset& section_paint_offset,
                              const PhysicalRect& columns_paint_rect,
                              const NGTableFragmentData::ColumnGeometries&);

 private:
  const NGPhysicalBoxFragment& fragment_;
};

class NGTableRowPainter {
  STACK_ALLOCATED();

 public:
  explicit NGTableRowPainter(const NGPhysicalBoxFragment& table_row_fragment)
      : fragment_(table_row_fragment) {
    DCHECK(fragment_.IsTableNGRow());
  }

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalRect&,
                                    const BoxDecorationData&);

  void PaintTablePartBackgroundIntoCells(
      const PaintInfo& paint_info,
      const LayoutBox& table_part,
      const PhysicalRect& table_part_paint_rect,
      const PhysicalOffset& row_paint_offset);

  void PaintColumnsBackground(const PaintInfo&,
                              const PhysicalOffset& row_paint_offset,
                              const PhysicalRect& columns_paint_rect,
                              const NGTableFragmentData::ColumnGeometries&);

 private:
  const NGPhysicalBoxFragment& fragment_;
};

class NGTableCellPainter {
  STACK_ALLOCATED();

 public:
  explicit NGTableCellPainter(const NGPhysicalBoxFragment& table_cell_fragment)
      : fragment_(table_cell_fragment) {}

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalRect&,
                                    const BoxDecorationData&);

  void PaintBackgroundForTablePart(
      const PaintInfo& paint_info,
      const LayoutBox& table_part,
      const PhysicalRect& table_part_paint_rect,
      const PhysicalOffset& table_cell_paint_offset);

 private:
  const NGPhysicalBoxFragment& fragment_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TABLE_PAINTERS_H_
