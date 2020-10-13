// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/view_painter.h"

#include <gtest/gtest.h>
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

using testing::ElementsAre;

namespace blink {

class ViewPainterFixedBackgroundTest : public PaintControllerPaintTest {
 protected:
  void RunFixedBackgroundTest(bool prefer_compositing_to_lcd_text);
};

INSTANTIATE_PAINT_TEST_SUITE_P(ViewPainterFixedBackgroundTest);

void ViewPainterFixedBackgroundTest::RunFixedBackgroundTest(
    bool prefer_compositing_to_lcd_text) {
  if (prefer_compositing_to_lcd_text) {
    Settings* settings = GetDocument().GetFrame()->GetSettings();
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }
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
  frame_view->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);

  const DisplayItem* background_display_item = nullptr;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    const auto& display_items = RootPaintController().GetDisplayItemList();
    const auto& background_client = prefer_compositing_to_lcd_text
                                        ? GetLayoutView()
                                        : ViewScrollingBackgroundClient();
    background_display_item = &display_items[0];
    EXPECT_THAT(*background_display_item,
                IsSameId(&background_client, DisplayItem::kDocumentBackground));
  } else {
    // If we prefer compositing to LCD text, the fixed background should go in a
    // different layer from the scrolling content; otherwise, it should go in
    // the same layer (i.e., the scrolling contents layer).
    if (prefer_compositing_to_lcd_text) {
      const auto& display_items = GetLayoutView()
                                      .Layer()
                                      ->GraphicsLayerBacking(&GetLayoutView())
                                      ->GetPaintController()
                                      .GetDisplayItemList();
      EXPECT_THAT(display_items,
                  ElementsAre(IsSameId(&GetLayoutView(),
                                       DisplayItem::kDocumentBackground)));
      background_display_item = &display_items[0];
    } else {
      const auto& display_items = RootPaintController().GetDisplayItemList();
      EXPECT_THAT(display_items,
                  ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM));
      background_display_item = &display_items[0];
    }
  }

  sk_sp<const PaintRecord> record =
      static_cast<const DrawingDisplayItem*>(background_display_item)
          ->GetPaintRecord();
  ASSERT_EQ(record->size(), 2u);
  cc::PaintOpBuffer::Iterator it(record.get());
  ASSERT_EQ((*++it)->GetType(), cc::PaintOpType::DrawRect);

  // This is the dest_rect_ calculated by BackgroundImageGeometry. For a fixed
  // background in scrolling contents layer, its location is the scroll offset.
  SkRect rect = static_cast<const cc::DrawRectOp*>(*it)->rect;
  if (prefer_compositing_to_lcd_text) {
    EXPECT_EQ(SkRect::MakeXYWH(0, 0, 800, 600), rect);
  } else {
    EXPECT_EQ(SkRect::MakeXYWH(scroll_offset.Width(), scroll_offset.Height(),
                               800, 600),
              rect);
  }
}

TEST_P(ViewPainterFixedBackgroundTest, DocumentFixedBackgroundLowDPI) {
  RunFixedBackgroundTest(false);
}

TEST_P(ViewPainterFixedBackgroundTest, DocumentFixedBackgroundHighDPI) {
  RunFixedBackgroundTest(true);
}

using ViewPainterTest = PaintControllerPaintTest;

INSTANTIATE_PAINT_TEST_SUITE_P(ViewPainterTest);

TEST_P(ViewPainterTest, DocumentBackgroundWithScroll) {
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar { display: none }</style>
    <div style='height: 5000px'></div>
  )HTML");

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    HitTestData scroll_hit_test_data;
    scroll_hit_test_data.scroll_translation =
        GetLayoutView().FirstFragment().PaintProperties()->ScrollTranslation();
    scroll_hit_test_data.scroll_hit_test_rect = IntRect(0, 0, 800, 600);
    // The scroll hit test should be before the scrolled contents to ensure the
    // hit test does not prevent the background squashing with the scrolling
    // contents.
    EXPECT_THAT(
        RootPaintController().PaintChunks()[0],
        IsPaintChunk(
            0, 0, PaintChunk::Id(GetLayoutView(), DisplayItem::kScrollHitTest),
            GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
            &scroll_hit_test_data, IntRect(0, 0, 800, 600)));
  }
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM));
  EXPECT_THAT(ContentPaintChunks(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON));
}

TEST_P(ViewPainterTest, FrameScrollHitTestProperties) {
  // This test depends on the CompositeAfterPaint behavior of painting solid
  // color backgrounds into both the non-scrolled and scrolled spaces.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;
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
                          IsSameId(&child, kBackgroundType)));

  const auto& paint_chunks = RootPaintController().PaintChunks();
  HitTestData scroll_hit_test_data;
  scroll_hit_test_data.scroll_translation =
      GetLayoutView().FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test_data.scroll_hit_test_rect = IntRect(0, 0, 800, 600);
  // The scroll hit test should be before the scrolled contents to ensure the
  // hit test does not prevent the background squashing with the scrolling
  // contents.
  const auto& scroll_hit_test_chunk = paint_chunks[0];
  const auto& contents_chunk = paint_chunks[1];
  EXPECT_THAT(
      scroll_hit_test_chunk,
      IsPaintChunk(0, 0,
                   PaintChunk::Id(GetLayoutView(), DisplayItem::kScrollHitTest),
                   GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
                   &scroll_hit_test_data));
  EXPECT_THAT(contents_chunk, VIEW_SCROLLING_BACKGROUND_CHUNK(2, nullptr));

  // The scroll hit test should not be scrolled and should not be clipped.
  const auto& scroll_hit_test_transform =
      ToUnaliased(scroll_hit_test_chunk.properties.Transform());
  EXPECT_EQ(nullptr, scroll_hit_test_transform.ScrollNode());
  const auto& scroll_hit_test_clip =
      ToUnaliased(scroll_hit_test_chunk.properties.Clip());
  EXPECT_EQ(FloatRect(LayoutRect::InfiniteIntRect()),
            scroll_hit_test_clip.UnsnappedClipRect().Rect());

  // The scrolled contents should be scrolled and clipped.
  const auto& contents_transform =
      ToUnaliased(contents_chunk.properties.Transform());
  const auto* contents_scroll = contents_transform.ScrollNode();
  EXPECT_EQ(IntSize(800, 2000), contents_scroll->ContentsSize());
  EXPECT_EQ(IntRect(0, 0, 800, 600), contents_scroll->ContainerRect());
  const auto& contents_clip = ToUnaliased(contents_chunk.properties.Clip());
  EXPECT_EQ(FloatRect(0, 0, 800, 600),
            contents_clip.UnsnappedClipRect().Rect());

  // The scroll hit test paint chunk maintains a reference to a scroll offset
  // translation node and the contents should be scrolled by this node.
  EXPECT_EQ(&contents_transform,
            scroll_hit_test_chunk.hit_test_data->scroll_translation);
}

class ViewPainterTouchActionRectTest : public ViewPainterTest {
 public:
  void SetUp() override {
    ViewPainterTest::SetUp();
    Settings* settings = GetDocument().GetFrame()->GetSettings();
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(ViewPainterTouchActionRectTest);

TEST_P(ViewPainterTouchActionRectTest, TouchActionRectScrollingContents) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      html {
        background: lightblue;
        touch-action: none;
      }
      body {
        margin: 0;
      }
    </style>
    <div id='forcescroll' style='width: 0; height: 3000px;'></div>
  )HTML");

  GetFrame().DomWindow()->scrollBy(0, 100);
  UpdateAllLifecyclePhasesForTest();

  HitTestData view_hit_test_data;
  view_hit_test_data.touch_action_rects = {{IntRect(0, 0, 800, 3000)},
                                           {IntRect(0, 0, 800, 3000)},
                                           {IntRect(0, 0, 800, 3000)}};

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    HitTestData non_scrolling_hit_test_data;
    non_scrolling_hit_test_data.touch_action_rects = {
        {IntRect(0, 0, 800, 600)}};
    HitTestData scroll_hit_test_data;
    scroll_hit_test_data.scroll_translation =
        GetLayoutView().FirstFragment().PaintProperties()->ScrollTranslation();
    scroll_hit_test_data.scroll_hit_test_rect = IntRect(0, 0, 800, 600);
    EXPECT_THAT(
        RootPaintController().PaintChunks()[0],
        IsPaintChunk(
            0, 0,
            PaintChunk::Id(*GetLayoutView().Layer(), DisplayItem::kLayerChunk),
            GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
            &non_scrolling_hit_test_data, IntRect(0, 0, 800, 600)));
    EXPECT_THAT(
        RootPaintController().PaintChunks()[1],
        IsPaintChunk(
            0, 0, PaintChunk::Id(GetLayoutView(), DisplayItem::kScrollHitTest),
            GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
            &scroll_hit_test_data, IntRect(0, 0, 800, 600)));
  }
  EXPECT_THAT(ContentPaintChunks(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK(
                  1, &view_hit_test_data, IntRect(0, 0, 800, 3000))));
}

TEST_P(ViewPainterTouchActionRectTest, TouchActionRectNonScrollingContents) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      html {
         background: radial-gradient(
          circle at 100px 100px, blue, transparent 200px) fixed;
        touch-action: none;
      }
      body {
        margin: 0;
      }
    </style>
    <div id='forcescroll' style='width: 0; height: 3000px;'></div>
  )HTML");

  GetFrame().DomWindow()->scrollBy(0, 100);
  UpdateAllLifecyclePhasesForTest();

  auto* view = &GetLayoutView();
  auto non_scrolling_properties =
      view->FirstFragment().LocalBorderBoxProperties();
  HitTestData view_hit_test_data;
  view_hit_test_data.touch_action_rects = {{IntRect(0, 0, 800, 600)}};
  auto* html = GetDocument().documentElement()->GetLayoutBox();
  auto scrolling_properties = view->FirstFragment().ContentsProperties();
  HitTestData scrolling_hit_test_data;
  scrolling_hit_test_data.touch_action_rects = {{IntRect(0, 0, 800, 3000)},
                                                {IntRect(0, 0, 800, 3000)}};
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    HitTestData scroll_hit_test_data;
    scroll_hit_test_data.scroll_translation =
        GetLayoutView().FirstFragment().PaintProperties()->ScrollTranslation();
    scroll_hit_test_data.scroll_hit_test_rect = IntRect(0, 0, 800, 600);
    EXPECT_THAT(
        RootPaintController().PaintChunks()[0],
        IsPaintChunk(0, 1,
                     PaintChunk::Id(*view->Layer(), DisplayItem::kLayerChunk),
                     non_scrolling_properties, &view_hit_test_data,
                     IntRect(0, 0, 800, 600)));
    EXPECT_THAT(
        RootPaintController().PaintChunks()[1],
        IsPaintChunk(1, 1, PaintChunk::Id(*view, DisplayItem::kScrollHitTest),
                     non_scrolling_properties, &scroll_hit_test_data,
                     IntRect(0, 0, 800, 600)));
    EXPECT_THAT(
        ContentPaintChunks(),
        ElementsAre(IsPaintChunk(
            1, 1, PaintChunk::Id(*html->Layer(), DisplayItem::kLayerChunk),
            scrolling_properties, &scrolling_hit_test_data,
            IntRect(0, 0, 800, 3000))));
  } else {
    auto& non_scrolling_paint_controller =
        view->Layer()->GraphicsLayerBacking(view)->GetPaintController();
    EXPECT_THAT(
        non_scrolling_paint_controller.PaintChunks(),
        ElementsAre(IsPaintChunk(
            0, 1, PaintChunk::Id(*view->Layer(), DisplayItem::kLayerChunk),
            non_scrolling_properties, &view_hit_test_data,
            IntRect(0, 0, 800, 600))));
    EXPECT_THAT(
        ContentPaintChunks(),
        ElementsAre(IsPaintChunk(
            0, 0, PaintChunk::Id(*html->Layer(), DisplayItem::kLayerChunk),
            scrolling_properties, &scrolling_hit_test_data,
            IntRect(0, 0, 800, 3000))));
  }
}

}  // namespace blink
