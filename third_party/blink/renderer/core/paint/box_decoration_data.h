// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_DECORATION_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_DECORATION_DATA_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
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
      : BoxDecorationData(paint_info,
                          layout_box,
                          layout_box.StyleRef(),
                          layout_box.HasNonCollapsedBorderDecoration()) {}

  BoxDecorationData(const PaintInfo& paint_info,
                    const NGPhysicalFragment& fragment,
                    const ComputedStyle& style)
      : BoxDecorationData(
            paint_info,
            To<LayoutBox>(*fragment.GetLayoutObject()),
            style,
            !fragment.HasCollapsedBorders() && style.HasBorderDecoration()) {}

  BoxDecorationData(const PaintInfo& paint_info,
                    const NGPhysicalFragment& fragment)
      : BoxDecorationData(paint_info, fragment, fragment.Style()) {}

  bool IsPaintingBackgroundInContentsSpace() const {
    return paint_info_.IsPaintingBackgroundInContentsSpace();
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

 private:
  BoxDecorationData(const PaintInfo& paint_info,
                    const LayoutBox& layout_box,
                    const ComputedStyle& style,
                    const bool has_non_collapsed_border_decoration)
      : paint_info_(paint_info),
        layout_box_(layout_box),
        style_(style),
        has_appearance_(style.HasEffectiveAppearance()),
        should_paint_background_(ComputeShouldPaintBackground()),
        should_paint_border_(
            ComputeShouldPaintBorder(has_non_collapsed_border_decoration)),
        should_paint_shadow_(ComputeShouldPaintShadow()) {}

  bool ComputeShouldPaintBackground() const {
    return style_.HasBackground() && !layout_box_.BackgroundTransfersToView() &&
           !paint_info_.ShouldSkipBackground();
  }

  bool ComputeShouldPaintBorder(
      bool has_non_collapsed_border_decoration) const {
    if (paint_info_.IsPaintingBackgroundInContentsSpace())
      return false;
    return has_non_collapsed_border_decoration;
  }

  bool ComputeShouldPaintShadow() const {
    return !paint_info_.IsPaintingBackgroundInContentsSpace() &&
           style_.BoxShadow();
  }

  bool BorderObscuresBackgroundEdge() const;
  BackgroundBleedAvoidance ComputeBleedAvoidance() const;

  // Inputs.
  const PaintInfo& paint_info_;
  const LayoutBox& layout_box_;
  const ComputedStyle& style_;

  // Outputs that are initialized in the constructor.
  const bool has_appearance_;
  const bool should_paint_background_;
  const bool should_paint_border_;
  const bool should_paint_shadow_;
  // This is lazily initialized.
  mutable absl::optional<BackgroundBleedAvoidance> bleed_avoidance_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_DECORATION_DATA_H_
