// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/adjust_mask_layer_geometry.h"

#include <math.h>
#include <algorithm>

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

void AdjustMaskLayerGeometry(const TransformPaintPropertyNode& transform,
                             gfx::Vector2dF& layer_offset,
                             gfx::Size& layer_bounds) {
  // Normally the mask layer and the masked layer use the same raster scale
  // because they are normally in the same transform space. However, if a
  // masked layer is a surface layer, solid color layer or a directly
  // composited image layer, the mask layer and the masked layer may use
  // different raster scales, and the rounding errors of the mask clip on the
  // masked layers might cause pixels in the masked layers along the mask clip
  // edges not fully covered by the mask layer. Now expand the mask layer by
  // about 2 screen pixels (a heuristic value that works at different raster
  // scales) each side.
  gfx::RectF pixel_rect(1, 1);
  // Map a screen pixel into the layer.
  GeometryMapper::SourceToDestinationRect(TransformPaintPropertyNode::Root(),
                                          transform, pixel_rect);
  // Don't expand too far in extreme cases.
  constexpr int kMaxOutset = 1000;
  int outset =
      ClampTo(std::ceil(std::max(pixel_rect.width(), pixel_rect.height()) * 2),
              0, kMaxOutset);
  layer_offset -= gfx::Vector2dF(outset, outset);
  layer_bounds += gfx::Size(2 * outset, 2 * outset);
}

}  // namespace blink
