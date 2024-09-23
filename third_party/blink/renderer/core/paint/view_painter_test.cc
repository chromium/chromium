// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/view_painter.h"

#include <gtest/gtest.h>
#include "cc/test/paint_op_matchers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {
namespace {

using ::cc::PaintOpIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ResultOf;

class ViewPainterFixedBackgroundTest : public PaintControllerPaintTest {
 protected:
  void RunFixedBackgroundTest(bool prefer_compositing_to_lcd_text);
};

INSTANTIATE_PAINT_TEST_SUITE_P(ViewPainterFixedBackgroundTest);

void ViewPainterFixedBackgroundTest::RunFixedBackgroundTest(
    bool prefer_compositing_to_lcd_text) {
  SetPreferCompositingToLCDText(prefer_compositing_to_lcd_text);
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body {
        margin: 0;
        width: 1200px;
        height: 900px;
        background: radial-gradient(
          circle at 100px 100px, blue, transparent 200px) fixed;
      }
    </style>
  )HTML");

  LocalFrameView* frame_view = GetDocument().View();
  ScrollableArea* layout_viewport = frame_view->LayoutViewport();

  ScrollOffset scroll_offset(200, 150);
  layout_viewport->SetScrollOffset(scroll_offset,
                                   mojom::blink::ScrollType::kUser);
  frame_view->UpdateAllLifecyclePhasesForTest();

  const auto& display_items = GetPersistentData().GetDisplayItemList();
  const auto& background_client = prefer_compositing_to_lcd_text
                                      ? GetLayoutView()
                                      : ViewScrollingBackgroundClient();
  const DisplayItem* background_display_item = &display_items[0];
  EXPECT_THAT(
      *background_display_item,
      IsSameId(background_client.Id(), DisplayItem::kDocumentBackground));

  PaintRecord record =
      To<DrawingDisplayItem>(background_display_item)->GetPaintRecord();

  SkRect expected_rect =
      prefer_compositing_to_lcd_text
          ? SkRect::MakeXYWH(0, 0, 800, 600)
          : SkRect::MakeXYWH(scroll_offset.x(), scroll_offset.y(), 800, 600);
  EXPECT_THAT(
      record,
      ElementsAre(
          _, AllOf(PaintOpIs<cc::DrawRectOp>(),
                   ResultOf(
                       [](const cc::PaintOp& op) {
                         return static_cast<const cc::DrawRectOp&>(op).rect;
                       },
                       expected_rect))));
}

TEST_P(ViewPainterFixedBackgroundTest,
       DocumentFixedBackgroundNotPreferCompositing) {
  RunFixedBackgroundTest(false);
}

TEST_P(ViewPainterFixedBackgroundTest,
       DocumentFixedBackgroundPreferCompositing) {
  RunFixedBackgroundTest(true);
}

using ViewPainterTest = PaintControllerPaintTest;

INSTANTIATE_PAINT_TEST_SUITE_P(ViewPainterTest);

TEST_P(ViewPainterTest, DocumentBackgroundWithScroll) {
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar { display: none }</style>
    <div style='height: 5000px'></div>
  )HTML");

  auto* scroll_hit_test_data = MakeGarbageCollected<HitTestData>();
  scroll_hit_test_data->scroll_hit_test_rect = gfx::Rect(0, 0, 800, 600);
  scroll_hit_test_data->scroll_translation =
      GetLayoutView().FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test_data->scrolling_contents_cull_rect =
      gfx::Rect(0, 0, 800, 4600);
  // The scroll hit test should be before the scrolled contents to ensure the
  // hit test does not prevent the background squashing with the scrolling
  // contents.
  EXPECT_THAT(
      GetPersistentData().GetPaintChunks()[0],
      IsPaintChunk(
          0, 0,
          PaintChunk::Id(GetLayoutView().Id(), DisplayItem::kScrollHitTest),
          GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
          scroll_hit_test_data, gfx::Rect(0, 0, 800, 600)));
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM));
  EXPECT_THAT(ContentPaintChunks(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON));
}

TEST_P(ViewPainterTest, FrameScrollHitTestProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #child { width: 100px; height: 2000px; background: green; }
    </style>
    <div id='child'></div>
  )HTML");

  auto& child = *GetLayoutObjectByElementId("child");

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(child.Id(), kBackgroundType)));

  const auto& paint_chunks = GetPersistentData().GetPaintChunks();
  auto* scroll_hit_test_data = MakeGarbageCollected<HitTestData>();
  scroll_hit_test_data->scroll_translation =
      GetLayoutView().FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test_data->scroll_hit_test_rect = gfx::Rect(0, 0, 800, 600);
  // The scroll hit test should be before the scrolled contents to ensure the
  // hit test does not prevent the background squashing with the scrolling
  // contents.
  const auto& scroll_hit_test_chunk = paint_chunks[0];
  const auto& contents_chunk = paint_chunks[1];
  EXPECT_THAT(
      scroll_hit_test_chunk,
      IsPaintChunk(
          0, 0,
          PaintChunk::Id(GetLayoutView().Id(), DisplayItem::kScrollHitTest),
          GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
          scroll_hit_test_data));
  EXPECT_THAT(contents_chunk, VIEW_SCROLLING_BACKGROUND_CHUNK(2, nullptr));

  // The scroll hit test should not be scrolled and should not be clipped.
  const auto& scroll_hit_test_transform =
      ToUnaliased(scroll_hit_test_chunk.properties.Transform());
  EXPECT_EQ(nullptr, scroll_hit_test_transform.ScrollNode());
  const auto& scroll_hit_test_clip =
      ToUnaliased(scroll_hit_test_chunk.properties.Clip());
  EXPECT_EQ(gfx::RectF(InfiniteIntRect()),
            scroll_hit_test_clip.PaintClipRect().Rect());

  // The scrolled contents should be scrolled and clipped.
  const auto& contents_transform =
      ToUnaliased(contents_chunk.properties.Transform());
  const auto* contents_scroll = contents_transform.ScrollNode();
  EXPECT_EQ(gfx::Rect(0, 0, 800, 2000), contents_scroll->ContentsRect());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), contents_scroll->ContainerRect());
  const auto& contents_clip = ToUnaliased(contents_chunk.properties.Clip());
  EXPECT_EQ(gfx::RectF(0, 0, 800, 600), contents_clip.PaintClipRect().Rect());

  // The scroll hit test paint chunk maintains a reference to a scroll offset
  // translation node and the contents should be scrolled by this node.
  EXPECT_EQ(&contents_transform,
            scroll_hit_test_chunk.hit_test_data->scroll_translation);
}

TEST_P(ViewPainterTest, TouchActionRect) {
  SetPreferCompositingToLCDText(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      html {
        background: radial-gradient(
          circle at 100px 100px, blue, transparent 200px) fixed;
        touch-action: pinch-zoom;
      }
      body {
        margin: 0;
      }
    </style>
    <div id='child' style='width: 10px; height: 100px; touch-action: none'>
    </div>
    <div id='forcescroll' style='width: 0; height: 2900px;'></div>
  )HTML");

  GetFrame().DomWindow()->scrollBy(0, 100);
  UpdateAllLifecyclePhasesForTest();

  auto* view = &GetLayoutView();
  auto non_scrolling_properties =
      view->FirstFragment().LocalBorderBoxProperties();
  auto* view_hit_test_data = MakeGarbageCollected<HitTestData>();
  view_hit_test_data->touch_action_rects = {
      {gfx::Rect(0, 0, 800, 600), TouchAction::kPinchZoom}};
  auto* html = GetDocument().documentElement()->GetLayoutBox();
  auto scrolling_properties = view->FirstFragment().ContentsProperties();
  auto* scrolling_hit_test_data = MakeGarbageCollected<HitTestData>();
  scrolling_hit_test_data->touch_action_rects = {
      {gfx::Rect(0, 0, 800, 3000), TouchAction::kPinchZoom},
      {gfx::Rect(0, 0, 10, 100), TouchAction::kNone}};

  auto* scroll_hit_test_data = MakeGarbageCollected<HitTestData>();
  scroll_hit_test_data->scroll_translation =
      GetLayoutView().FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test_data->scroll_hit_test_rect = gfx::Rect(0, 0, 800, 600);
  EXPECT_THAT(
      GetPersistentData().GetPaintChunks()[0],
      IsPaintChunk(
          0, 1, PaintChunk::Id(view->Layer()->Id(), DisplayItem::kLayerChunk),
          non_scrolling_properties, view_hit_test_data,
          gfx::Rect(0, 0, 800, 600)));
  EXPECT_THAT(GetPersistentData().GetPaintChunks()[1],
              IsPaintChunk(
                  1, 1, PaintChunk::Id(view->Id(), DisplayItem::kScrollHitTest),
                  non_scrolling_properties, scroll_hit_test_data,
                  gfx::Rect(0, 0, 800, 600)));
  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(IsPaintChunk(
          1, 1,
          RuntimeEnabledFeatures::HitTestOpaquenessEnabled()
              ? PaintChunk::Id(view->GetScrollableArea()
                                   ->GetScrollingBackgroundDisplayItemClient()
                                   .Id(),
                               DisplayItem::kDocumentBackground)
              : PaintChunk::Id(html->Layer()->Id(), DisplayItem::kLayerChunk),
          scrolling_properties, scrolling_hit_test_data,
          gfx::Rect(0, 0, 800, 3000))));
}

}  // namespace
}  // namespace blink
