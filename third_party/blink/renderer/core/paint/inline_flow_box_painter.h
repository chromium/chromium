// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_FLOW_BOX_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_FLOW_BOX_PAINTER_H_

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/inline_box_painter_base.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class InlineFlowBox;
class IntRect;
class LayoutPoint;
class LayoutRect;
class LayoutUnit;
struct PaintInfo;

class InlineFlowBoxPainter : public InlineBoxPainterBase {
  STACK_ALLOCATED();

 public:
  InlineFlowBoxPainter(const InlineFlowBox&);

  void Paint(const PaintInfo&,
             const LayoutPoint& paint_offset,
             const LayoutUnit line_top,
             const LayoutUnit line_bottom);

  LayoutRect FrameRectClampedToLineTopAndBottomIfNeeded() const;

 private:
  // LayoutNG version adapters.
  PhysicalRect PaintRectForImageStrip(const PhysicalRect& rect,
                                      TextDirection direction) const override {
    return PhysicalRect(PaintRectForImageStrip(rect.ToLayoutRect(), direction));
  }
  void PaintNormalBoxShadow(const PaintInfo& info,
                            const ComputedStyle& style,
                            const PhysicalRect& rect) override {
    return PaintNormalBoxShadow(info, style, rect.ToLayoutRect());
  }
  void PaintInsetBoxShadow(const PaintInfo& info,
                           const ComputedStyle& style,
                           const PhysicalRect& rect) override {
    return PaintInsetBoxShadow(info, style, rect.ToLayoutRect());
  }
  BorderPaintingType GetBorderPaintType(
      const PhysicalRect& adjusted_frame_rect,
      IntRect& adjusted_clip_rect,
      bool object_has_multiple_boxes) const override {
    return GetBorderPaintType(adjusted_frame_rect.ToLayoutRect(),
                              adjusted_clip_rect, object_has_multiple_boxes);
  }

  // Legacy version.
  LayoutRect PaintRectForImageStrip(const LayoutRect&, TextDirection) const;
  void PaintNormalBoxShadow(const PaintInfo&,
                            const ComputedStyle&,
                            const LayoutRect& paint_rect);
  void PaintInsetBoxShadow(const PaintInfo&,
                           const ComputedStyle&,
                           const LayoutRect& paint_rect);
  BorderPaintingType GetBorderPaintType(const LayoutRect& adjusted_frame_rect,
                                        IntRect& adjusted_clip_rect,
                                        bool object_has_multiple_boxes) const;

  void PaintBackgroundBorderShadow(const PaintInfo&,
                                   const LayoutPoint& paint_offset);
  void PaintMask(const PaintInfo&, const LayoutPoint& paint_offset);

  LayoutRect AdjustedPaintRect(const LayoutPoint& paint_offset) const;

  // Paint a hit test display item and record hit test data. This should be
  // called when painting the background even if there is no other painted
  // content.
  void RecordHitTestData(const PaintInfo&, const LayoutPoint& paint_offset);

  const InlineFlowBox& inline_flow_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_FLOW_BOX_PAINTER_H_
