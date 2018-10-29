// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class LayoutPoint;
class LayoutTable;
struct PaintInfo;

class TablePainter {
  STACK_ALLOCATED();

 public:
  TablePainter(const LayoutTable& layout_table) : layout_table_(layout_table) {}

  void PaintObject(const PaintInfo&, const LayoutPoint& paint_offset);
  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const LayoutPoint& paint_offset);
  void PaintMask(const PaintInfo&, const LayoutPoint& paint_offset);

 private:
  void PaintCollapsedBorders(const PaintInfo&);

  const LayoutTable& layout_table_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_PAINTER_H_
