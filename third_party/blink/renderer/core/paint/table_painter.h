// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutTable;
struct PaintInfo;
struct PhysicalOffset;

class TablePainter {
  STACK_ALLOCATED();

 public:
  TablePainter(const LayoutTable& layout_table) : layout_table_(layout_table) {}

  void PaintObject(const PaintInfo&, const PhysicalOffset& paint_offset);
  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalOffset& paint_offset);
  void PaintMask(const PaintInfo&, const PhysicalOffset& paint_offset);

 private:
  void PaintCollapsedBorders(const PaintInfo&,
                             const PhysicalOffset& paint_offset);

  const LayoutTable& layout_table_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_PAINTER_H_
