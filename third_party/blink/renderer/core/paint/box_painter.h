// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_PAINTER_H_

#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/paint/rounded_inner_rect_clipper.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Color;
class DisplayItemClient;
class LayoutBox;
struct PaintInfo;
struct PhysicalOffset;
struct PhysicalRect;

class BoxPainter {
  STACK_ALLOCATED();

 public:
  BoxPainter(const LayoutBox& layout_box) : layout_box_(layout_box) {}
  void Paint(const PaintInfo&);

  void PaintChildren(const PaintInfo&);
  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalOffset& paint_offset);
  void PaintMask(const PaintInfo&, const PhysicalOffset& paint_offset);

  void PaintMaskImages(const PaintInfo&, const PhysicalRect&);
  void PaintBoxDecorationBackgroundWithRect(
      const PaintInfo&,
      const PhysicalRect&,
      const DisplayItemClient& background_client);

  // Paint a hit test display item and record hit test data. This should be
  // called in the background paint phase even if there is no other painted
  // content.
  void RecordHitTestData(const PaintInfo&,
                         const PhysicalRect& paint_rect,
                         const DisplayItemClient& background_client);

  // Paint a scroll hit test display item and record scroll hit test data. This
  // should be called in the background paint phase even if there is no other
  // painted content.
  void RecordScrollHitTestData(const PaintInfo&,
                               const DisplayItemClient& background_client);

 private:
  bool BackgroundIsKnownToBeOpaque(const PaintInfo&);
  void PaintBackground(const PaintInfo&,
                       const PhysicalRect&,
                       const Color& background_color,
                       BackgroundBleedAvoidance = kBackgroundBleedNone);

  const LayoutBox& layout_box_;
};

}  // namespace blink

#endif
