// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_foreign_object_painter.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_foreign_object.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"

namespace blink {

SVGForeignObjectPainter::SVGForeignObjectPainter(
    const LayoutNGSVGForeignObject& layout_svg_foreign_object)
    : layout_svg_foreign_object_(layout_svg_foreign_object) {}

void SVGForeignObjectPainter::PaintLayer(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelectionDragImage)
    return;

  // Early out in the case of trying to paint an image filter before
  // pre-paint has finished.
  if (!layout_svg_foreign_object_.FirstFragment().HasLocalBorderBoxProperties())
    return;

  // TODO(crbug.com/797779): For now foreign object contents don't know whether
  // they are painted in a fragmented context and may do something bad in a
  // fragmented context, e.g. creating subsequences. Skip cache to avoid that.
  // This will be unnecessary when the contents are fragment aware.
  absl::optional<DisplayItemCacheSkipper> cache_skipper;
  if (layout_svg_foreign_object_.Layer()->Parent()->EnclosingPaginationLayer())
    cache_skipper.emplace(paint_info.context);

  // <foreignObject> is a replaced normal-flow stacking element.
  // See IsReplacedNormalFlowStacking in paint_layer_painter.cc.
  PaintLayerPainter(*layout_svg_foreign_object_.Layer())
      .Paint(paint_info.context, paint_info.GetPaintFlags());
}

}  // namespace blink
