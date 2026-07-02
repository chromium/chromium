// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_REPLACED_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_REPLACED_PAINTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace blink {

struct PaintInfo;
struct PhysicalRect;
class DisplayItemClient;
class ScopedPaintState;
class LayoutReplaced;

class ReplacedPainter {
  STACK_ALLOCATED();

 public:
  ReplacedPainter(const LayoutReplaced& layout_replaced)
      : layout_replaced_(layout_replaced) {}

  void Paint(const PaintInfo&);

  bool ShouldPaint(const ScopedPaintState&) const;

  // Returns the per-layer background colors that PaintCustomHighlights
  // would tint over `replaced` for the highlight names in `sorted_names`
  // (which must already be sorted lowest-priority first, i.e. bottom-up
  // in stacking order). `currentColor` on a higher-priority layer
  // resolves against the previous layer's resolved current color,
  // matching the text-marker pipeline
  // (HighlightOverlay::ComputeParts). Layers whose resolved background
  // is fully transparent are skipped. This helper neither sorts the
  // names nor validates that they are still registered highlights.
  static CORE_EXPORT Vector<Color> ResolveStackedCustomHighlightBackgrounds(
      const LayoutReplaced& replaced,
      const Vector<AtomicString>& sorted_names,
      const PaintInfo& paint_info);

 private:
  bool ShouldPaintBoxDecorationBackground(const PaintInfo&);
  void MeasureOverflowMetrics() const;

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalOffset& paint_offset);

  // |visual_rect| is for the drawing display item, covering overflowing box
  // shadows and border image outsets. |paint_rect| is the border box rect in
  // paint coordinates.
  void PaintBoxDecorationBackgroundWithRect(
      const PaintInfo& paint_info,
      const gfx::Rect& visual_rect,
      const PhysicalRect& paint_rect,
      const DisplayItemClient& background_client);

  void PaintBackground(const PaintInfo&,
                       const PhysicalRect&,
                       const Color& background_color,
                       BackgroundBleedAvoidance = kBackgroundBleedNone);

  void PaintMask(const PaintInfo&, const PhysicalOffset& paint_offset);
  void PaintMaskImages(const PaintInfo&, const PhysicalRect&);

  // Paints background colors from any custom CSS Highlights (::highlight())
  // that cover this replaced element. No-op outside of the foreground phase
  // and when no highlights are active.
  void PaintCustomHighlights(const PaintInfo&,
                             const PhysicalOffset& paint_offset);

  const LayoutReplaced& layout_replaced_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_REPLACED_PAINTER_H_
