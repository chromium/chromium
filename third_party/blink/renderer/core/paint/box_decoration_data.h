// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_DECORATION_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_DECORATION_DATA_H_

#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

// Data for box decoration painting.
class BoxDecorationData {
  STACK_ALLOCATED();

 public:
  BoxDecorationData(const PaintInfo& paint_info, const LayoutBox& layout_box)
      : BoxDecorationData(paint_info, layout_box, layout_box.StyleRef()) {}

  BoxDecorationData(const PaintInfo& paint_info,
                    const NGPhysicalFragment& fragment,
                    const ComputedStyle& style)
      : BoxDecorationData(paint_info,
                          ToLayoutBox(*fragment.GetLayoutObject()),
                          style) {}

  BoxDecorationData(const PaintInfo& paint_info,
                    const NGPhysicalFragment& fragment)
      : BoxDecorationData(paint_info, fragment, fragment.Style()) {}

  bool IsPaintingScrollingBackground() const {
    return is_painting_scrolling_background_;
  }
  bool HasAppearance() const { return has_appearance_; }
  bool ShouldPaintBackground() const { return should_paint_background_; }
  bool ShouldPaintBorder() const { return should_paint_border_; }
  bool ShouldPaintShadow() const { return should_paint_shadow_; }

  BackgroundBleedAvoidance GetBackgroundBleedAvoidance() const {
    if (!bleed_avoidance_)
      bleed_avoidance_ = ComputeBleedAvoidance();
    return *bleed_avoidance_;
  }

  bool ShouldPaint() const {
    return HasAppearance() || ShouldPaintBackground() || ShouldPaintBorder() ||
           ShouldPaintShadow();
  }

  // This is not cached because the caller is unlikely to call this repeatedly.
  Color BackgroundColor() const {
    return style_.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  }

  static bool IsPaintingScrollingBackground(const PaintInfo& paint_info,
                                            const LayoutBox& layout_box) {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      return paint_info.IsPaintingScrollingBackground();
    return (paint_info.PaintFlags() & kPaintLayerPaintingOverflowContents) &&
           !(paint_info.PaintFlags() &
             kPaintLayerPaintingCompositingBackgroundPhase) &&
           layout_box == paint_info.PaintContainer();
  }

 private:
  BoxDecorationData(const PaintInfo& paint_info,
                    const LayoutBox& layout_box,
                    const ComputedStyle& style)
      : paint_info_(paint_info),
        layout_box_(layout_box),
        style_(style),
        is_painting_scrolling_background_(
            IsPaintingScrollingBackground(paint_info, layout_box)),
        has_appearance_(style.HasEffectiveAppearance()),
        should_paint_background_(ComputeShouldPaintBackground()),
        should_paint_border_(ComputeShouldPaintBorder()),
        should_paint_shadow_(ComputeShouldPaintShadow()) {}

  bool ComputeShouldPaintBackground() const {
    if (!style_.HasBackground())
      return false;
    if (layout_box_.BackgroundTransfersToView())
      return false;
    if (paint_info_.SkipRootBackground() &&
        paint_info_.PaintContainer() == &layout_box_)
      return false;
    return true;
  }

  bool ComputeShouldPaintBorder() const {
    if (is_painting_scrolling_background_)
      return false;
    return layout_box_.HasNonCollapsedBorderDecoration();
  }

  bool ComputeShouldPaintShadow() const {
    return !is_painting_scrolling_background_ && style_.BoxShadow();
  }

  bool BorderObscuresBackgroundEdge() const;
  BackgroundBleedAvoidance ComputeBleedAvoidance() const;

  // Inputs.
  const PaintInfo& paint_info_;
  const LayoutBox& layout_box_;
  const ComputedStyle& style_;
  // Outputs that are initialized in the constructor.
  const bool is_painting_scrolling_background_;
  const bool has_appearance_;
  const bool should_paint_background_;
  const bool should_paint_border_;
  const bool should_paint_shadow_;
  // This is lazily initialized.
  mutable base::Optional<BackgroundBleedAvoidance> bleed_avoidance_;
};

}  // namespace blink

#endif
