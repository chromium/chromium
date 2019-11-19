// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_decoration_data.h"

#include "third_party/blink/renderer/core/style/border_edge.h"

namespace blink {

bool BoxDecorationData::BorderObscuresBackgroundEdge() const {
  BorderEdge edges[4];
  style_.GetBorderEdgeInfo(edges);

  for (auto& edge : edges) {
    if (!edge.ObscuresBackgroundEdge())
      return false;
  }

  return true;
}

BackgroundBleedAvoidance BoxDecorationData::ComputeBleedAvoidance() const {
  if (!should_paint_background_ || is_painting_scrolling_background_ ||
      layout_box_.IsDocumentElement())
    return kBackgroundBleedNone;

  const bool has_border_radius = style_.HasBorderRadius();
  if (!should_paint_border_ || !has_border_radius ||
      style_.CanRenderBorderImage()) {
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
      if ((BackgroundColor().Alpha() || fill_layer.Next()) &&
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
