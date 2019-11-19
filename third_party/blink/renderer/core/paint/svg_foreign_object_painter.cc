// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_foreign_object_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"

namespace blink {

void SVGForeignObjectPainter::PaintLayer(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection)
    return;

  // Early out in the case of trying to paint an image filter before
  // pre-paint has finished.
  if (!layout_svg_foreign_object_.FirstFragment().HasLocalBorderBoxProperties())
    return;

  // TODO(crbug.com/797779): For now foreign object contents don't know whether
  // they are painted in a fragmented context and may do something bad in a
  // fragmented context, e.g. creating subsequences. Skip cache to avoid that.
  // This will be unnecessary when the contents are fragment aware.
  base::Optional<DisplayItemCacheSkipper> cache_skipper;
  if (layout_svg_foreign_object_.Layer()->Parent()->EnclosingPaginationLayer())
    cache_skipper.emplace(paint_info.context);

  // <foreignObject> is a replaced normal-flow stacking element.
  // See IsReplacedNormalFlowStacking in paint_layer_painter.cc.
  PaintLayerPaintingInfo layer_painting_info(
      layout_svg_foreign_object_.Layer(),
      // Reset to an infinite cull rect, for simplicity. Otherwise
      // an adjustment would be needed for ancestor scrolling, and any
      // SVG transforms would have to be taken into account. Further,
      // cull rects under transform are intentionally reset to infinity,
      // to improve cache invalidation performance in the pre-paint tree
      // walk (see https://http://crrev.com/482854).
      CullRect::Infinite(), paint_info.GetGlobalPaintFlags(), PhysicalOffset());
  PaintLayerPainter(*layout_svg_foreign_object_.Layer())
      .Paint(paint_info.context, layer_painting_info, paint_info.PaintFlags());
}

void SVGForeignObjectPainter::Paint(const PaintInfo& paint_info) {
  PaintInfo paint_info_before_filtering(paint_info);
  ScopedSVGPaintState paint_state(layout_svg_foreign_object_,
                                  paint_info_before_filtering);

  if (paint_state.GetPaintInfo().phase == PaintPhase::kForeground &&
      !paint_state.ApplyClipMaskAndFilterIfNecessary())
    return;

  BlockPainter(layout_svg_foreign_object_).Paint(paint_info);
}

}  // namespace blink
