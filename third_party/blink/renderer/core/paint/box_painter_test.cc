// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_hit_test_display_item.h"

using testing::ElementsAre;

namespace blink {

using BoxPainterTest = PaintControllerPaintTest;

INSTANTIATE_PAINT_TEST_SUITE_P(BoxPainterTest);

TEST_P(BoxPainterTest, DontPaintEmptyDecorationBackground) {
  SetBodyInnerHTML(R"HTML(
    <div id="div1" style="width: 100px; height: 100px; background: green">
    </div>
    <div id="div2" style="width: 100px; height: 100px; outline: 2px solid blue">
    </div>
  )HTML");

  auto* div1 = GetLayoutObjectByElementId("div1");
  auto* div2 = GetLayoutObjectByElementId("div2");
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType),
                          IsSameId(div1, kBackgroundType),
                          IsSameId(div2, DisplayItem::PaintPhaseToDrawingType(
                                             PaintPhase::kSelfOutlineOnly))));
}

using BoxPainterScrollHitTestTest = PaintControllerPaintTest;

INSTANTIATE_SCROLL_HIT_TEST_SUITE_P(BoxPainterScrollHitTestTest);

TEST_P(BoxPainterScrollHitTestTest,
       ScrollHitTestOrderWithScrollBackgroundAttachment) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #container {
        width: 200px;
        height: 200px;
        overflow-y: scroll;
        background: linear-gradient(yellow, blue);
        background-attachment: scroll;
        will-change: transform;
      }
      #child { height: 300px; width: 10px; background: blue; }
    </style>
    <div id='container'>
      <div id='child'></div>
    </div>
  )HTML");

  auto& container = *GetLayoutObjectByElementId("container");
  auto& child = *GetLayoutObjectByElementId("child");

  // As a reminder, "background-attachment: scroll" does not move when the
  // container's scroll offset changes.

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // The scroll hit test should be after the non-scrolling (attachment:
    // scroll) container background so that it does not prevent squashing the
    // non-scrolling container background into the root layer.
    EXPECT_THAT(RootPaintController().GetDisplayItemList(),
                ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                     kDocumentBackgroundType),
                            IsSameId(&container, kBackgroundType),
                            IsSameId(&container, kScrollHitTestType),
                            IsSameId(&child, kBackgroundType)));
  } else {
    // Because the frame composited scrolls, no scroll hit test display item is
    // needed.
    const auto* non_scrolling_layer = To<LayoutBlock>(container)
                                          .Layer()
                                          ->GetCompositedLayerMapping()
                                          ->MainGraphicsLayer();
    EXPECT_THAT(non_scrolling_layer->GetPaintController().GetDisplayItemList(),
                ElementsAre(IsSameId(&container, kBackgroundType)));
    const auto* scrolling_layer = To<LayoutBlock>(container)
                                      .Layer()
                                      ->GetCompositedLayerMapping()
                                      ->ScrollingContentsLayer();
    EXPECT_THAT(scrolling_layer->GetPaintController().GetDisplayItemList(),
                ElementsAre(IsSameId(&child, kBackgroundType)));
  }
}

TEST_P(BoxPainterScrollHitTestTest,
       ScrollHitTestOrderWithLocalBackgroundAttachment) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #container {
        width: 200px;
        height: 200px;
        overflow-y: scroll;
        background: linear-gradient(yellow, blue);
        background-attachment: local;
        will-change: transform;
      }
      #child { height: 300px; width: 10px; background: blue; }
    </style>
    <div id='container'>
      <div id='child'></div>
    </div>
  )HTML");

  auto& container = ToLayoutBox(*GetLayoutObjectByElementId("container"));
  auto& child = *GetLayoutObjectByElementId("child");
  auto* container_scrolling_client =
      &container.GetScrollableArea()->GetScrollingBackgroundDisplayItemClient();

  // As a reminder, "background-attachment: local" moves when the container's
  // scroll offset changes.

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // The scroll hit test should be before the scrolling (attachment: local)
    // container background so that it does not prevent squashing the scrolling
    // background into the scrolling contents.
    EXPECT_THAT(
        RootPaintController().GetDisplayItemList(),
        ElementsAre(
            IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
            IsSameId(&container, kScrollHitTestType),
            IsSameId(container_scrolling_client, kBackgroundType),
            IsSameId(&child, kBackgroundType)));
  } else {
    // Because the frame composited scrolls, no scroll hit test display item is
    // needed.
    const auto* non_scrolling_layer =
        container.Layer()->GetCompositedLayerMapping()->MainGraphicsLayer();
    EXPECT_TRUE(non_scrolling_layer->GetPaintController()
                    .GetDisplayItemList()
                    .IsEmpty());
    const auto* scrolling_layer = container.Layer()
                                      ->GetCompositedLayerMapping()
                                      ->ScrollingContentsLayer();
    EXPECT_THAT(
        scrolling_layer->GetPaintController().GetDisplayItemList(),
        ElementsAre(IsSameId(container_scrolling_client, kBackgroundType),
                    IsSameId(&child, kBackgroundType)));
  }
}

TEST_P(BoxPainterScrollHitTestTest, ScrollHitTestProperties) {
  // This test depends on the CompositeAfterPaint behavior of painting solid
  // color backgrounds into both the non-scrolled and scrolled spaces.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #container {
        width: 200px;
        height: 200px;
        overflow-y: scroll;
        background: green;
      }
      #child { width: 100px; height: 300px; background: green; }
    </style>
    <div id='container'>
      <div id='child'></div>
    </div>
  )HTML");

  auto& container = To<LayoutBlock>(*GetLayoutObjectByElementId("container"));
  const auto& paint_chunks = RootPaintController().PaintChunks();
  auto& child = *GetLayoutObjectByElementId("child");

  // The scroll hit test should be after the container background but before the
  // scrolled contents.
  EXPECT_EQ(
      kBackgroundPaintInGraphicsLayer | kBackgroundPaintInScrollingContents,
      container.GetBackgroundPaintLocation());
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(
          IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
          IsSameId(&container, kBackgroundType),
          IsSameId(&container, kScrollHitTestType),
          IsSameId(&container.GetScrollableArea()
                        ->GetScrollingBackgroundDisplayItemClient(),
                   kBackgroundType),
          IsSameId(&child, kBackgroundType)));

  HitTestData scroll_hit_test_data;
  const auto& scrolling_contents_properties =
      container.FirstFragment().ContentsProperties();
  scroll_hit_test_data.SetScrollHitTest(
      &scrolling_contents_properties.Transform(), IntRect(0, 0, 200, 200));
  EXPECT_THAT(
      paint_chunks,
      ElementsAre(
          IsPaintChunk(0, 1,
                       PaintChunk::Id(ViewScrollingBackgroundClient(),
                                      kDocumentBackgroundType),
                       GetLayoutView().FirstFragment().ContentsProperties()),
          IsPaintChunk(1, 2,
                       PaintChunk::Id(*container.Layer(),
                                      kNonScrollingBackgroundChunkType),
                       container.FirstFragment().LocalBorderBoxProperties()),
          IsPaintChunk(2, 3, PaintChunk::Id(container, kScrollHitTestType),
                       container.FirstFragment().LocalBorderBoxProperties(),
                       scroll_hit_test_data),
          IsPaintChunk(3, 5,
                       PaintChunk::Id(container, kScrollingBackgroundChunkType),
                       scrolling_contents_properties)));

  // We always create scroll node for the root layer.
  const auto& root_transform = paint_chunks[0].properties.Transform();
  EXPECT_NE(nullptr, root_transform.ScrollNode());

  // The container's background chunk should not scroll and therefore should use
  // the root transform. Its local transform is actually a paint offset
  // transform.
  const auto& container_transform = paint_chunks[1].properties.Transform();
  EXPECT_EQ(&root_transform, container_transform.Parent());
  EXPECT_EQ(nullptr, container_transform.ScrollNode());

  // The scroll hit test should not be scrolled and should not be clipped.
  // Its local transform is actually a paint offset transform.
  const auto& scroll_hit_test_chunk = paint_chunks[2];
  const auto& scroll_hit_test_transform =
      scroll_hit_test_chunk.properties.Transform();
  EXPECT_EQ(nullptr, scroll_hit_test_transform.ScrollNode());
  EXPECT_EQ(&root_transform, scroll_hit_test_transform.Parent());
  const auto& scroll_hit_test_clip = scroll_hit_test_chunk.properties.Clip();
  EXPECT_EQ(FloatRect(0, 0, 800, 600), scroll_hit_test_clip.ClipRect().Rect());

  // The scrolled contents should be scrolled and clipped.
  const auto& contents_chunk = RootPaintController().PaintChunks()[3];
  const auto& contents_transform = contents_chunk.properties.Transform();
  const auto* contents_scroll = contents_transform.ScrollNode();
  EXPECT_EQ(IntSize(200, 300), contents_scroll->ContentsSize());
  EXPECT_EQ(IntRect(0, 0, 200, 200), contents_scroll->ContainerRect());
  const auto& contents_clip = contents_chunk.properties.Clip();
  EXPECT_EQ(FloatRect(0, 0, 200, 200), contents_clip.ClipRect().Rect());

  // The scroll hit test display item maintains a reference to a scroll offset
  // translation node and the contents should be scrolled by this node.
  const auto& scroll_hit_test_display_item =
      static_cast<const ScrollHitTestDisplayItem&>(
          RootPaintController()
              .GetDisplayItemList()[scroll_hit_test_chunk.begin_index]);
  EXPECT_EQ(&contents_transform,
            scroll_hit_test_display_item.scroll_offset_node());
}

}  // namespace blink
