// Copyright 2014 The Chromium Authors
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

namespace gfx {
class Rect;
}

namespace blink {

class InlineFlowBox;
class LayoutRect;
class LayoutUnit;
struct PaintInfo;

class InlineFlowBoxPainter : public InlineBoxPainterBase {
  STACK_ALLOCATED();

 public:
  InlineFlowBoxPainter(const InlineFlowBox&);

  void Paint(const PaintInfo&,
             const PhysicalOffset& paint_offset,
             const LayoutUnit line_top,
             const LayoutUnit line_bottom);

  LayoutRect FrameRectClampedToLineTopAndBottomIfNeeded() const;

 private:
  // LayoutNG version adapters.
  PhysicalRect PaintRectForImageStrip(const PhysicalRect& rect,
                                      TextDirection direction) const override;
  void PaintNormalBoxShadow(const PaintInfo& info,
                            const ComputedStyle& style,
                            const PhysicalRect& rect) override;
  void PaintInsetBoxShadow(const PaintInfo& info,
                           const ComputedStyle& style,
                           const PhysicalRect& rect) override;
  BorderPaintingType GetBorderPaintType(
      const PhysicalRect& adjusted_frame_rect,
      gfx::Rect& adjusted_clip_rect,
      bool object_has_multiple_boxes) const override;

  void PaintBackgroundBorderShadow(const PaintInfo&,
                                   const PhysicalOffset& paint_offset);
  void PaintMask(const PaintInfo&, const PhysicalOffset& paint_offset);

  PhysicalRect AdjustedFrameRect(const PhysicalOffset& paint_offset) const;
  gfx::Rect VisualRect(const PhysicalRect& adjusted_frame_rect) const;

  // Expands the bounds of the current paint chunk for hit test, and records
  // special touch action if any. This should be called in the background paint
  // phase even if there is no other painted content.
  void RecordHitTestData(const PaintInfo&, const PhysicalOffset& paint_offset);

  // Records the bounds of the current paint chunk for potential cropping later
  // as part of tab capture.
  void RecordRegionCaptureData(const PaintInfo& paint_info,
                               const PhysicalOffset& paint_offset);

  const InlineFlowBox& inline_flow_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_FLOW_BOX_PAINTER_H_
