// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_container_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

using testing::ElementsAre;

namespace blink {

using SVGContainerPainterTest = PaintControllerPaintTest;

INSTANTIATE_PAINT_TEST_SUITE_P(SVGContainerPainterTest);

TEST_P(SVGContainerPainterTest, FilterPaintProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container, #before, #after { will-change: transform; }
    </style>
    <svg id="svg" width="40" height="40">
      <g id="container">
        <rect id="before" width="10" height="10" fill="lightgray" />
        <rect id="rect" width="11" height="11" fill="lightblue" />
        <rect id="after" width="12" height="12" fill="blue" />
      </g>
    </svg>
  )HTML");

  const auto* root = GetLayoutObjectByElementId("svg");

  const DisplayItem::Type kSVGEffectPaintPhaseForeground =
      static_cast<DisplayItem::Type>(DisplayItem::kSVGEffectPaintPhaseFirst +
                                     5);

  const auto* before = GetLayoutObjectByElementId("before");
  PaintChunk::Id before_id(*before, kSVGEffectPaintPhaseForeground);
  const auto& before_properties = before->FirstFragment().ContentsProperties();

  const auto* rect = GetLayoutObjectByElementId("rect");
  PaintChunk::Id rect_id(*rect, DisplayItem::kHitTest);
  const auto* container = GetLayoutObjectByElementId("container");
  // Because the rect doesn't create paint properties, it uses the container's.
  const auto& container_properties =
      container->FirstFragment().ContentsProperties();

  const auto* after = GetLayoutObjectByElementId("after");
  PaintChunk::Id after_id(*after, kSVGEffectPaintPhaseForeground);
  const auto& after_properties = after->FirstFragment().ContentsProperties();

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_THAT(ContentPaintChunks(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                            IsPaintChunk(1, 2, before_id, before_properties),
                            IsPaintChunk(2, 3, rect_id, container_properties),
                            IsPaintChunk(3, 4, after_id, after_properties)));
  } else {
    const auto* svg_paint_layer = To<LayoutSVGRoot>(root)->Layer();
    const auto* svg_graphics_layer =
        svg_paint_layer->GetCompositedLayerMapping()->MainGraphicsLayer();

    EXPECT_THAT(svg_graphics_layer->GetPaintController().PaintChunks(),
                ElementsAre(IsPaintChunk(0, 1, before_id, before_properties),
                            IsPaintChunk(1, 2, rect_id, container_properties),
                            IsPaintChunk(2, 3, after_id, after_properties)));
  }
}

}  // namespace blink
