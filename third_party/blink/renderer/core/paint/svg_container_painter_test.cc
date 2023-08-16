// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_container_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
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
      #container, #before, #after { will-change: filter; }
    </style>
    <svg id="svg" width="40" height="40">
      <g id="container">
        <rect id="before" width="10" height="10" fill="lightgray" />
        <rect id="rect" width="11" height="11" fill="lightblue" />
        <rect id="after" width="12" height="12" fill="blue" />
      </g>
    </svg>
  )HTML");

  const DisplayItem::Type kSVGEffectPaintPhaseForeground =
      static_cast<DisplayItem::Type>(DisplayItem::kSVGEffectPaintPhaseFirst +
                                     5);

  const auto* before = GetLayoutObjectByElementId("before");
  PaintChunk::Id before_id(before->Id(), kSVGEffectPaintPhaseForeground);
  const auto& before_properties = before->FirstFragment().ContentsProperties();

  const auto* rect = GetLayoutObjectByElementId("rect");
  PaintChunk::Id rect_id(rect->Id(), DisplayItem::kHitTest);
  const auto* container = GetLayoutObjectByElementId("container");
  // Because the rect doesn't create paint properties, it uses the container's.
  const auto& container_properties =
      container->FirstFragment().ContentsProperties();

  const auto* after = GetLayoutObjectByElementId("after");
  PaintChunk::Id after_id(after->Id(), kSVGEffectPaintPhaseForeground);

  const auto& after_properties = after->FirstFragment().ContentsProperties();

  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_THAT(ContentPaintChunks(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                            IsPaintChunk(1, 1),  // Hit test for svg.
                            IsPaintChunk(1, 2, before_id, before_properties),
                            IsPaintChunk(2, 3, rect_id, container_properties),
                            IsPaintChunk(3, 4, after_id, after_properties)));
  } else {
    EXPECT_THAT(ContentPaintChunks(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                            IsPaintChunk(1, 2, before_id, before_properties),
                            IsPaintChunk(2, 3, rect_id, container_properties),
                            IsPaintChunk(3, 4, after_id, after_properties)));
  }
}

TEST_P(SVGContainerPainterTest, ScaleAnimationFrom0) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <style>
        @keyframes scale { to { scale: 1; } }
        .scale { animation: 1s scale 1s forwards; }
        @keyframes transform-scale { to { transform: scale(1); } }
        .transform-scale { animation: 1s transform-scale 1s forwards; }
        #rect1 { scale: 0; }
        #rect2 { transform: scale(0); }
      </style>
      <g>
        <g>
          <rect id="rect1" width="100" height="100"/>
        </g>
      </g>
      <g>
        <g>
          <rect id="rect2" width="100" height="100"/>
        </g>
      </g>
    </svg>
  )HTML");

  // Initially all <g>s and <rect>s are empty and don't paint.

  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_THAT(ContentPaintChunks(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                            IsPaintChunk(1, 1)));  // Svg hit test.
  } else {
    EXPECT_THAT(ContentPaintChunks(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON));
  }

  auto* rect1_element = GetDocument().getElementById(AtomicString("rect1"));
  auto* rect2_element = GetDocument().getElementById(AtomicString("rect2"));
  rect1_element->setAttribute(html_names::kClassAttr, AtomicString("scale"));
  rect2_element->setAttribute(html_names::kClassAttr,
                              AtomicString("transform-scale"));
  UpdateAllLifecyclePhasesForTest();

  // Start animations on the rects.
  const DisplayItem::Type kSVGTransformPaintPhaseForeground =
      static_cast<DisplayItem::Type>(DisplayItem::kSVGTransformPaintPhaseFirst +
                                     5);
  auto* rect1 = GetLayoutObjectByElementId("rect1");
  auto* rect2 = GetLayoutObjectByElementId("rect2");
  PaintChunk::Id rect1_id(rect1->Id(), kSVGTransformPaintPhaseForeground);
  auto rect1_properties = rect1->FirstFragment().ContentsProperties();
  PaintChunk::Id rect2_id(rect2->Id(), kSVGTransformPaintPhaseForeground);
  auto rect2_properties = rect2->FirstFragment().ContentsProperties();
  // Both rects should be painted to be ready for composited animation.
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_THAT(ContentPaintChunks(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                            IsPaintChunk(1, 1),  // Svg hit test.
                            IsPaintChunk(1, 2, rect1_id, rect1_properties),
                            IsPaintChunk(2, 3, rect2_id, rect2_properties)));
  } else {
    EXPECT_THAT(ContentPaintChunks(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                            IsPaintChunk(1, 2, rect1_id, rect1_properties),
                            IsPaintChunk(2, 3, rect2_id, rect2_properties)));
  }

  // Remove the animations.
  rect1_element->removeAttribute(html_names::kClassAttr);
  rect2_element->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  // We don't remove the paintings of the rects immediately because they are
  // harmless and we want to avoid repaints.
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_THAT(ContentPaintChunks(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                            IsPaintChunk(1, 1),  // Svg hit test.
                            IsPaintChunk(1, 2, rect1_id, rect1_properties),
                            IsPaintChunk(2, 3, rect2_id, rect2_properties)));
  } else {
    EXPECT_THAT(ContentPaintChunks(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                            IsPaintChunk(1, 2, rect1_id, rect1_properties),
                            IsPaintChunk(2, 3, rect2_id, rect2_properties)));
  }

  // We remove the paintings only after anything else trigger a layout and a
  // repaint.
  rect1->Parent()->SetNeedsLayout("test");
  rect2->Parent()->SetNeedsLayout("test");
  rect1->EnclosingLayer()->SetNeedsRepaint();
  UpdateAllLifecyclePhasesForTest();
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_THAT(ContentPaintChunks(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                            IsPaintChunk(1, 1)));  // Svg hit test.
  } else {
    EXPECT_THAT(ContentPaintChunks(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON));
  }
}

}  // namespace blink
