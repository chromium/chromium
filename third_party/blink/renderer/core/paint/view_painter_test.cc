// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/view_painter.h"

#include <gtest/gtest.h>
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_hit_test_display_item.h"

using testing::ElementsAre;

namespace blink {

class ViewPainterTest : public PaintControllerPaintTest {
 protected:
  void RunFixedBackgroundTest(bool prefer_compositing_to_lcd_text);
};

INSTANTIATE_PAINT_TEST_SUITE_P(ViewPainterTest);

void ViewPainterTest::RunFixedBackgroundTest(
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
  layout_viewport->SetScrollOffset(scroll_offset, kUserScroll);
  frame_view->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);

  const DisplayItem* background_display_item = nullptr;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    const auto& display_items = RootPaintController().GetDisplayItemList();
    if (prefer_compositing_to_lcd_text) {
      EXPECT_THAT(
          display_items,
          ElementsAre(IsSameId(&GetLayoutView(), kDocumentBackgroundType),
                      IsSameId(&GetLayoutView(), DisplayItem::kScrollHitTest)));
      background_display_item = &display_items[0];
    } else {
      EXPECT_THAT(
          display_items,
          ElementsAre(IsSameId(&GetLayoutView(), DisplayItem::kScrollHitTest),
                      IsSameId(&ViewScrollingBackgroundClient(),
                               kDocumentBackgroundType)));
      background_display_item = &display_items[1];
    }
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
      EXPECT_THAT(
          display_items,
          ElementsAre(IsSameId(&GetLayoutView(), kDocumentBackgroundType)));
      background_display_item = &display_items[0];
    } else {
      const auto& display_items = RootPaintController().GetDisplayItemList();
      EXPECT_THAT(display_items,
                  ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                       kDocumentBackgroundType)));
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

TEST_P(ViewPainterTest, DocumentFixedBackgroundLowDPI) {
  RunFixedBackgroundTest(false);
}

TEST_P(ViewPainterTest, DocumentFixedBackgroundHighDPI) {
  RunFixedBackgroundTest(true);
}

using ViewPainterScrollHitTestTest = PaintControllerPaintTest;

INSTANTIATE_SCROLL_HIT_TEST_SUITE_P(ViewPainterScrollHitTestTest);

TEST_P(ViewPainterScrollHitTestTest, DocumentBackgroundWithScroll) {
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar { display: none }</style>
    <div style='height: 5000px'></div>
  )HTML");

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // The scroll hit test should be before the scrolled contents to ensure the
    // hit test does not prevent the background squashing with the scrolling
    // contents.
    EXPECT_THAT(
        RootPaintController().GetDisplayItemList(),
        ElementsAre(IsSameId(&GetLayoutView(), DisplayItem::kScrollHitTest),
                    IsSameId(&ViewScrollingBackgroundClient(),
                             kDocumentBackgroundType)));
    HitTestData scroll_hit_test_data;
    const auto& scrolling_contents_properties =
        GetLayoutView().FirstFragment().ContentsProperties();
    scroll_hit_test_data.SetScrollHitTest(
        &scrolling_contents_properties.Transform(), IntRect(0, 0, 800, 600));
    EXPECT_THAT(
        RootPaintController().PaintChunks(),
        ElementsAre(
            IsPaintChunk(
                0, 1,
                PaintChunk::Id(GetLayoutView(), DisplayItem::kScrollHitTest),
                GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
                scroll_hit_test_data),
            IsPaintChunk(1, 2,
                         PaintChunk::Id(ViewScrollingBackgroundClient(),
                                        kDocumentBackgroundType),
                         scrolling_contents_properties)));
  } else {
    // Because the frame composited scrolls, no scroll hit test display item is
    // needed.
    EXPECT_THAT(RootPaintController().GetDisplayItemList(),
                ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                     kDocumentBackgroundType)));
    EXPECT_THAT(RootPaintController().PaintChunks(),
                ElementsAre(IsPaintChunk(
                    0, 1,
                    PaintChunk::Id(ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType),
                    GetLayoutView().FirstFragment().ContentsProperties())));
  }
}

TEST_P(ViewPainterScrollHitTestTest, FrameScrollHitTestProperties) {
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

  auto& html =
      To<LayoutBlock>(*GetDocument().documentElement()->GetLayoutObject());
  auto& child = *GetLayoutObjectByElementId("child");

  // The scroll hit test should be before the scrolled contents to ensure the
  // hit test does not prevent the background squashing with the scrolling
  // contents.
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&GetLayoutView(), kScrollHitTestType),
                          IsSameId(&ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType),
                          IsSameId(&child, kBackgroundType)));

  const auto& paint_chunks = RootPaintController().PaintChunks();
  const auto& view_contents_properties =
      GetLayoutView().FirstFragment().ContentsProperties();
  HitTestData scroll_hit_test_data;
  scroll_hit_test_data.SetScrollHitTest(&view_contents_properties.Transform(),
                                        IntRect(0, 0, 800, 600));
  EXPECT_THAT(
      paint_chunks,
      ElementsAre(
          IsPaintChunk(
              0, 1,
              PaintChunk::Id(GetLayoutView(), DisplayItem::kScrollHitTest),
              GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
              scroll_hit_test_data),
          IsPaintChunk(1, 2,
                       PaintChunk::Id(ViewScrollingBackgroundClient(),
                                      kDocumentBackgroundType),
                       view_contents_properties),
          IsPaintChunk(2, 3,
                       PaintChunk::Id(*html.Layer(),
                                      kNonScrollingContentsBackgroundChunkType),
                       html.FirstFragment().ContentsProperties())));

  // The scroll hit test should not be scrolled and should not be clipped.
  const auto& scroll_hit_test_chunk = RootPaintController().PaintChunks()[0];
  const auto& scroll_hit_test_transform =
      scroll_hit_test_chunk.properties.Transform();
  EXPECT_EQ(nullptr, scroll_hit_test_transform.ScrollNode());
  const auto& scroll_hit_test_clip = scroll_hit_test_chunk.properties.Clip();
  EXPECT_EQ(FloatRect(LayoutRect::InfiniteIntRect()),
            scroll_hit_test_clip.ClipRect().Rect());

  // The scrolled contents should be scrolled and clipped.
  const auto& contents_chunk = RootPaintController().PaintChunks()[2];
  const auto& contents_transform = contents_chunk.properties.Transform();
  const auto* contents_scroll = contents_transform.ScrollNode();
  EXPECT_EQ(IntSize(800, 2000), contents_scroll->ContentsSize());
  EXPECT_EQ(IntRect(0, 0, 800, 600), contents_scroll->ContainerRect());
  const auto& contents_clip = contents_chunk.properties.Clip();
  EXPECT_EQ(FloatRect(0, 0, 800, 600), contents_clip.ClipRect().Rect());

  // The scroll hit test display item maintains a reference to a scroll offset
  // translation node and the contents should be scrolled by this node.
  const auto& scroll_hit_test_display_item =
      static_cast<const ScrollHitTestDisplayItem&>(
          RootPaintController()
              .GetDisplayItemList()[scroll_hit_test_chunk.begin_index]);
  EXPECT_EQ(&contents_transform,
            scroll_hit_test_display_item.scroll_offset_node());
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

  const auto& scrolling_client = ViewScrollingBackgroundClient();
  auto scrolling_properties =
      GetLayoutView().FirstFragment().ContentsProperties();
  HitTestData view_hit_test_data;
  view_hit_test_data.touch_action_rects.emplace_back(
      LayoutRect(0, 0, 800, 3000));

  auto* html =
      To<LayoutBlock>(GetDocument().documentElement()->GetLayoutObject());
  HitTestData html_hit_test_data;
  html_hit_test_data.touch_action_rects.emplace_back(
      LayoutRect(0, 0, 800, 3000));
  html_hit_test_data.touch_action_rects.emplace_back(
      LayoutRect(0, 0, 800, 3000));

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    HitTestData non_scrolling_hit_test_data;
    non_scrolling_hit_test_data.touch_action_rects.emplace_back(
        LayoutRect(0, 0, 800, 600));
    HitTestData scroll_hit_test_data;
    scroll_hit_test_data.SetScrollHitTest(&scrolling_properties.Transform(),
                                          IntRect(0, 0, 800, 600));
    EXPECT_THAT(
        RootPaintController().PaintChunks(),
        ElementsAre(
            IsPaintChunk(
                0, 1,
                PaintChunk::Id(*GetLayoutView().Layer(),
                               DisplayItem::kLayerChunkBackground),
                GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
                non_scrolling_hit_test_data),
            IsPaintChunk(
                1, 2,
                PaintChunk::Id(GetLayoutView(), DisplayItem::kScrollHitTest),
                GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
                scroll_hit_test_data),
            IsPaintChunk(
                2, 4, PaintChunk::Id(scrolling_client, kDocumentBackgroundType),
                scrolling_properties, view_hit_test_data),
            IsPaintChunk(4, 6,
                         PaintChunk::Id(*html->Layer(),
                                        kNonScrollingBackgroundChunkType),
                         scrolling_properties, html_hit_test_data)));
  } else {
    EXPECT_THAT(
        RootPaintController().PaintChunks(),
        ElementsAre(
            IsPaintChunk(
                0, 2, PaintChunk::Id(scrolling_client, kDocumentBackgroundType),
                scrolling_properties, view_hit_test_data),
            IsPaintChunk(2, 4,
                         PaintChunk::Id(*html->Layer(),
                                        kNonScrollingBackgroundChunkType),
                         scrolling_properties, html_hit_test_data)));
  }
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
  view_hit_test_data.touch_action_rects.emplace_back(
      LayoutRect(0, 0, 800, 600));
  auto* html =
      To<LayoutBlock>(GetDocument().documentElement()->GetLayoutObject());
  auto scrolling_properties = view->FirstFragment().ContentsProperties();
  HitTestData scrolling_hit_test_data;
  scrolling_hit_test_data.touch_action_rects.emplace_back(
      LayoutRect(0, 0, 800, 3000));
  scrolling_hit_test_data.touch_action_rects.emplace_back(
      LayoutRect(0, 0, 800, 3000));
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    HitTestData scroll_hit_test_data;
    scroll_hit_test_data.SetScrollHitTest(&scrolling_properties.Transform(),
                                          IntRect(0, 0, 800, 600));
    EXPECT_THAT(
        RootPaintController().PaintChunks(),
        ElementsAre(
            IsPaintChunk(0, 2,
                         PaintChunk::Id(*view->Layer(),
                                        DisplayItem::kLayerChunkBackground),
                         non_scrolling_properties, view_hit_test_data),
            IsPaintChunk(2, 3,
                         PaintChunk::Id(*view, DisplayItem::kScrollHitTest),
                         non_scrolling_properties, scroll_hit_test_data),
            IsPaintChunk(3, 5,
                         PaintChunk::Id(*html->Layer(),
                                        kNonScrollingBackgroundChunkType),
                         scrolling_properties, scrolling_hit_test_data)));
  } else {
    auto& non_scrolling_paint_controller =
        view->Layer()->GraphicsLayerBacking(view)->GetPaintController();
    EXPECT_THAT(
        non_scrolling_paint_controller.PaintChunks(),
        ElementsAre(IsPaintChunk(
            0, 2,
            PaintChunk::Id(*view->Layer(), kNonScrollingBackgroundChunkType),
            non_scrolling_properties, view_hit_test_data)));
    EXPECT_THAT(
        RootPaintController().PaintChunks(),
        ElementsAre(IsPaintChunk(
            0, 2,
            PaintChunk::Id(*html->Layer(), kNonScrollingBackgroundChunkType),
            scrolling_properties, scrolling_hit_test_data)));
  }
}

}  // namespace blink
