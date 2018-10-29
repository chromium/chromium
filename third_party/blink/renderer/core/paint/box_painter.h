// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_PAINTER_H_

#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/paint/rounded_inner_rect_clipper.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

struct PaintInfo;
class Color;
class LayoutBox;
class LayoutPoint;
class LayoutRect;

class BoxPainter {
  STACK_ALLOCATED();

 public:
  BoxPainter(const LayoutBox& layout_box) : layout_box_(layout_box) {}
  void Paint(const PaintInfo&);

  void PaintChildren(const PaintInfo&);
  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const LayoutPoint& paint_offset);
  void PaintMask(const PaintInfo&, const LayoutPoint& paint_offset);

  void PaintMaskImages(const PaintInfo&, const LayoutRect&);
  void PaintBoxDecorationBackgroundWithRect(const PaintInfo&,
                                            const LayoutRect&);

  // Paint a hit test display item and record hit test data. This should be
  // called in the background paint phase even if there is no other painted
  // content.
  void RecordHitTestData(const PaintInfo&,
                         const LayoutPoint& paint_offset,
                         const LayoutRect& paint_rect);

 private:
  bool BackgroundIsKnownToBeOpaque(const PaintInfo&);
  void PaintBackground(const PaintInfo&,
                       const LayoutRect&,
                       const Color& background_color,
                       BackgroundBleedAvoidance = kBackgroundBleedNone);

  const LayoutBox& layout_box_;
};

}  // namespace blink

#endif
