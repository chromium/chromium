// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_decoration_data.h"

#include "third_party/blink/renderer/core/style/border_edge.h"

namespace blink {

bool BoxDecorationData::BorderObscuresBackgroundEdge() const {
  BorderEdgeArray edges;
  style_.GetBorderEdgeInfo(edges);

  for (auto& edge : edges) {
    if (!edge.ObscuresBackgroundEdge())
      return false;
  }

  return true;
}

BackgroundBleedAvoidance BoxDecorationData::ComputeBleedAvoidance() const {
  if (!should_paint_background_ ||
      paint_info_.IsPaintingBackgroundInContentsSpace() ||
      layout_box_.IsDocumentElement())
    return kBackgroundBleedNone;

  const bool has_border_image = style_.CanRenderBorderImage();
  const bool has_border_radius = style_.HasBorderRadius();
  if (!should_paint_border_ || !has_border_radius || has_border_image) {
    if (has_border_image) {
      // Border images are not affected by border radius, and thus clipping to
      // the border box would break the border image.
      if (has_border_radius) {
        return kBackgroundBleedNone;
      }
      // If a border image has a non-zero border image outset, it will extend
      // outside the border box, which means that the "clip" bleed avoidance
      // strategies will not work since they will end up clipping the border
      // image.
      if (!style_.ImageOutsets(style_.BorderImage()).IsZero()) {
        return kBackgroundBleedNone;
      }
    }
    if (layout_box_.BackgroundShouldAlwaysBeClipped())
      return kBackgroundBleedClipOnly;
    // Border radius clipping may require layer bleed avoidance if we are going
    // to draw an image over something else, because we do not want the
    // antialiasing to lead to bleeding
    if (style_.HasBackgroundImage() && has_border_radius) {
      // But if the top layer is opaque for the purposes of background painting,
      // we do not need the bleed avoidance because we will not paint anything
      // behind the top layer.  But only if we need to draw something
      // underneath.
      const FillLayer& fill_layer = style_.BackgroundLayers();
      if ((!BackgroundColor().IsFullyTransparent() || fill_layer.Next()) &&
          !fill_layer.ImageOccludesNextLayers(layout_box_.GetDocument(),
                                              style_)) {
        return kBackgroundBleedClipLayer;
      }
    }
    return kBackgroundBleedNone;
  }

  if (BorderObscuresBackgroundEdge())
    return kBackgroundBleedShrinkBackground;

  return kBackgroundBleedClipLayer;
}

}  // namespace blink
