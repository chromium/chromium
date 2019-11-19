// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ELLIPSIS_BOX_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ELLIPSIS_BOX_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct PaintInfo;

class EllipsisBox;
class LayoutPoint;
class LayoutUnit;
class ComputedStyle;

class EllipsisBoxPainter {
  STACK_ALLOCATED();

 public:
  EllipsisBoxPainter(const EllipsisBox& ellipsis_box)
      : ellipsis_box_(ellipsis_box) {}

  void Paint(const PaintInfo&,
             const LayoutPoint&,
             LayoutUnit line_top,
             LayoutUnit line_bottom);

 private:
  void PaintEllipsis(const PaintInfo&,
                     const LayoutPoint& paint_offset,
                     LayoutUnit line_top,
                     LayoutUnit line_bottom,
                     const ComputedStyle&);

  const EllipsisBox& ellipsis_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ELLIPSIS_BOX_PAINTER_H_
