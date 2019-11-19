// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COLLAPSED_BORDER_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COLLAPSED_BORDER_PAINTER_H_

#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct PaintInfo;

class CollapsedBorderPainter {
  STACK_ALLOCATED();

 public:
  CollapsedBorderPainter(const LayoutTableCell& cell)
      : cell_(cell), table_(*cell.Table()) {}

  void PaintCollapsedBorders(const PaintInfo&);

 private:
  void SetupBorders();
  void AdjustJoints();
  void AdjustForWritingModeAndDirection();

  const LayoutTableCell& cell_;
  const LayoutTable& table_;

  struct Border {
    // This will be set to null if we don't need to paint this border.
    const CollapsedBorderValue* value = nullptr;
    int inner_width = 0;
    int outer_width = 0;
    // We may paint the border not in full length if the corner is covered by
    // another higher priority border. The following are the outsets from the
    // border box of the begin and end points of the painted segment.
    int begin_outset = 0;
    int end_outset = 0;
  };
  Border start_;
  Border end_;
  Border before_;
  Border after_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COLLAPSED_BORDER_PAINTER_H_
