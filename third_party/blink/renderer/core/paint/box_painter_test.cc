// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

using testing::ElementsAre;

namespace blink {

using BoxPainterTest = PaintControllerPaintTest;

INSTANTIATE_PAINT_TEST_SUITE_P(BoxPainterTest);

TEST_P(BoxPainterTest, EmptyDecorationBackground) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0;
        /* to force a subsequene and paint chunk */
        opacity: 0.5;
        /* to verify child empty backgrounds expand chunk bounds */
        height: 0;
      }
    </style>
    <div id="div1" style="width: 100px; height: 100px; background: green">
    </div>
    <div id="div2" style="width: 100px; height: 100px; outline: 2px solid blue">
    </div>
    <div id="div3" style="width: 200px; height: 150px"></div>
  )HTML");

  auto* div1 = GetLayoutObjectByElementId("div1");
  auto* div2 = GetLayoutObjectByElementId("div2");
  auto* body = GetDocument().body()->GetLayoutBox();
  // Empty backgrounds don't generate display items.
  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(div1->Id(), kBackgroundType),
                  IsSameId(div2->Id(), DisplayItem::PaintPhaseToDrawingType(
                                           PaintPhase::kSelfOutlineOnly))));

  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                  // Empty backgrounds contribute to bounds of paint chunks.
                  IsPaintChunk(1, 3,
                               PaintChunk::Id(body->Layer()->Id(),
                                              DisplayItem::kLayerChunk),
                               body->FirstFragment().LocalBorderBoxProperties(),
                               nullptr, gfx::Rect(-2, 0, 202, 350))));
}

TEST_P(BoxPainterTest, ScrollHitTestOrderWithScrollBackgroundAttachment) {
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

  auto& container = *GetLayoutBoxByElementId("container");
  auto& child = *GetLayoutObjectByElementId("child");

  // As a reminder, "background-attachment: scroll" does not move when the
  // container's scroll offset changes.

  // The scroll hit test should be after the non-scrolling (attachment:
  // scroll) container background so that it does not prevent squashing the
  // non-scrolling container background into the root layer.
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(container.Id(), kBackgroundType),
                          IsSameId(child.Id(), kBackgroundType)));
  auto* scroll_hit_test = MakeGarbageCollected<HitTestData>();
  scroll_hit_test->scroll_translation =
      container.FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test->scroll_hit_test_rect = gfx::Rect(0, 0, 200, 200);
  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(
              1, 2,
              PaintChunk::Id(container.Layer()->Id(), DisplayItem::kLayerChunk),
              container.FirstFragment().LocalBorderBoxProperties()),
          IsPaintChunk(
              2, 2, PaintChunk::Id(container.Id(), DisplayItem::kScrollHitTest),
              container.FirstFragment().LocalBorderBoxProperties(),
              scroll_hit_test, gfx::Rect(0, 0, 200, 200)),
          IsPaintChunk(2, 3)));
}

TEST_P(BoxPainterTest, ScrollHitTestOrderWithLocalBackgroundAttachment) {
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

  auto& container = *GetLayoutBoxByElementId("container");
  auto& child = *GetLayoutObjectByElementId("child");
  auto* container_scrolling_client =
      &container.GetScrollableArea()->GetScrollingBackgroundDisplayItemClient();

  // As a reminder, "background-attachment: local" moves when the container's
  // scroll offset changes.

  // The scroll hit test should be before the scrolling (attachment: local)
  // container background so that it does not prevent squashing the scrolling
  // background into the scrolling contents.
  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(container_scrolling_client->Id(), kBackgroundType),
                  IsSameId(child.Id(), kBackgroundType)));
  auto* scroll_hit_test = MakeGarbageCollected<HitTestData>();
  scroll_hit_test->scroll_translation =
      container.FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test->scroll_hit_test_rect = gfx::Rect(0, 0, 200, 200);
  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(container.Layer()->Id(), DisplayItem::kLayerChunk),
              container.FirstFragment().LocalBorderBoxProperties()),
          IsPaintChunk(
              1, 1, PaintChunk::Id(container.Id(), DisplayItem::kScrollHitTest),
              container.FirstFragment().LocalBorderBoxProperties(),
              scroll_hit_test, gfx::Rect(0, 0, 200, 200)),
          IsPaintChunk(
              1, 3,
              PaintChunk::Id(container.Id(), kScrollingBackgroundChunkType),
              container.FirstFragment().ContentsProperties())));
}

TEST_P(BoxPainterTest, ScrollHitTestProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #container {
        width: 200px;
        height: 200px;
        overflow-y: scroll;
        background: rgba(0, 128, 0, 0.5);  /* to prevent compositing */
      }
      #child { width: 100px; height: 300px; background: green; }
    </style>
    <div id='container'>
      <div id='child'></div>
    </div>
  )HTML");

  auto& container = To<LayoutBlock>(*GetLayoutObjectByElementId("container"));
  const auto& paint_chunks = ContentPaintChunks();
  auto& child = *GetLayoutObjectByElementId("child");

  // The scroll hit test should be after the container background but before the
  // scrolled contents.
  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            container.GetBackgroundPaintLocation());
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(container.Id(), kBackgroundType),
                          IsSameId(child.Id(), kBackgroundType)));

  auto* scroll_hit_test_data = MakeGarbageCollected<HitTestData>();
  const auto& scrolling_contents_properties =
      container.FirstFragment().ContentsProperties();
  scroll_hit_test_data->scroll_translation =
      container.FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test_data->scroll_hit_test_rect = gfx::Rect(0, 0, 200, 200);
  EXPECT_THAT(
      paint_chunks,
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(1, 2,
                       PaintChunk::Id(container.Id(), kBackgroundChunkType),
                       container.FirstFragment().LocalBorderBoxProperties()),
          IsPaintChunk(
              2, 2, PaintChunk::Id(container.Id(), DisplayItem::kScrollHitTest),
              container.FirstFragment().LocalBorderBoxProperties(),
              scroll_hit_test_data, gfx::Rect(0, 0, 200, 200)),
          IsPaintChunk(
              2, 3,
              PaintChunk::Id(container.Id(),
                             RuntimeEnabledFeatures::HitTestOpaquenessEnabled()
                                 ? kScrollingBackgroundChunkType
                                 : kClippedContentsBackgroundChunkType),
              scrolling_contents_properties)));

  // We always create scroll node for the root layer.
  const auto& root_transform =
      ToUnaliased(paint_chunks[0].properties.Transform());
  EXPECT_NE(nullptr, root_transform.ScrollNode());

  // The container's background chunk should not scroll and therefore should use
  // the root transform. Its local transform is actually a paint offset
  // transform.
  const auto& container_transform =
      ToUnaliased(paint_chunks[1].properties.Transform());
  EXPECT_EQ(&root_transform, container_transform.Parent());
  EXPECT_EQ(nullptr, container_transform.ScrollNode());

  // The scroll hit test should not be scrolled and should not be clipped.
  // Its local transform is actually a paint offset transform.
  const auto& scroll_hit_test_chunk = paint_chunks[2];
  const auto& scroll_hit_test_transform =
      ToUnaliased(scroll_hit_test_chunk.properties.Transform());
  EXPECT_EQ(nullptr, scroll_hit_test_transform.ScrollNode());
  EXPECT_EQ(&root_transform, scroll_hit_test_transform.Parent());
  const auto& scroll_hit_test_clip =
      ToUnaliased(scroll_hit_test_chunk.properties.Clip());
  EXPECT_EQ(gfx::RectF(0, 0, 800, 600),
            scroll_hit_test_clip.PaintClipRect().Rect());

  // The scrolled contents should be scrolled and clipped.
  const auto& contents_chunk = paint_chunks[3];
  const auto& contents_transform =
      ToUnaliased(contents_chunk.properties.Transform());
  const auto* contents_scroll = contents_transform.ScrollNode();
  EXPECT_EQ(gfx::Rect(0, 0, 200, 300), contents_scroll->ContentsRect());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200), contents_scroll->ContainerRect());
  const auto& contents_clip = ToUnaliased(contents_chunk.properties.Clip());
  EXPECT_EQ(gfx::RectF(0, 0, 200, 200), contents_clip.PaintClipRect().Rect());

  // The scroll paint chunk maintains a reference to a scroll translation node
  // and the contents should be scrolled by this node.
  EXPECT_EQ(&contents_transform,
            scroll_hit_test_chunk.hit_test_data->scroll_translation);
}

// crbug.com/1256990
TEST_P(BoxPainterTest, ScrollerUnderInlineTransform3DSceneLeafCrash) {
  SetBodyInnerHTML(R"HTML(
    <div style="transform-style: preserve-3d">
      <div style="display:inline">
        <div style="display: inline-block; overflow: scroll;
                    width: 100px; height: 100px">
          <div style="height: 200px"></div>
        </div>
      </div>
    </div>
  )HTML");
  // This should not crash.
}

size_t CountDrawImagesWithConstraint(const cc::PaintRecord& record,
                                     SkCanvas::SrcRectConstraint constraint) {
  size_t count = 0;
  for (const cc::PaintOp& op : record) {
    if (op.GetType() == cc::PaintOpType::kDrawImageRect) {
      const auto& image_op = static_cast<const cc::DrawImageRectOp&>(op);
      if (image_op.constraint == constraint)
        ++count;
    } else if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const auto& record_op = static_cast<const cc::DrawRecordOp&>(op);
      count += CountDrawImagesWithConstraint(record_op.record, constraint);
    }
  }
  return count;
}

TEST_P(BoxPainterTest, ImageClampingMode) {
  SetBodyInnerHTML(R"HTML(
    <!doctype html>
    <style>
      div#test {
        height: 500px;
        width: 353.743px;
        background-image: url("data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==");
        background-size: contain;
        background-repeat: no-repeat;
      }
    </style>
    <div id="test"></div>
  )HTML");

  PaintRecord record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(1U, CountDrawImagesWithConstraint(
                    record, SkCanvas::kFast_SrcRectConstraint));
}

}  // namespace blink
