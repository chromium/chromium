// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/block_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_hit_test_display_item.h"

using testing::ElementsAre;

namespace blink {

using BlockPainterTest = PaintControllerPaintTest;

INSTANTIATE_PAINT_TEST_SUITE_P(BlockPainterTest);

TEST_P(BlockPainterTest, OverflowRectForCullRectTesting) {
  SetBodyInnerHTML(R"HTML(
    <div id='scroller' style='width: 50px; height: 50px; overflow: scroll'>
      <div style='width: 50px; height: 5000px'></div>
    </div>
  )HTML");
  auto* scroller = To<LayoutBlock>(GetLayoutObjectByElementId("scroller"));
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(PhysicalRect(0, 0, 50, 5000),
              BlockPainter(*scroller).OverflowRectForCullRectTesting(false));
  } else {
    EXPECT_EQ(PhysicalRect(0, 0, 50, 50),
              BlockPainter(*scroller).OverflowRectForCullRectTesting(false));
  }
}

TEST_P(BlockPainterTest, OverflowRectCompositedScrollingForCullRectTesting) {
  SetBodyInnerHTML(R"HTML(
    <div id='scroller' style='width: 50px; height: 50px; overflow: scroll; will-change: transform'>
      <div style='width: 50px; height: 5000px'></div>
    </div>
  )HTML");
  auto* scroller = To<LayoutBlock>(GetLayoutObjectByElementId("scroller"));
  EXPECT_EQ(PhysicalRect(0, 0, 50, 5000),
            BlockPainter(*scroller).OverflowRectForCullRectTesting(false));
}

// TODO(pdr): These touch action tests should be run for all paint test
// parameters (using INSTANTIATE_PAINT_TEST_SUITE_P) but they are currently
// run without flags (i.e., stable configuration).
class BlockPainterTouchActionTest : public PaintControllerPaintTestBase {};

TEST_F(BlockPainterTouchActionTest, TouchActionRectsWithoutPaint) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #parent { width: 100px; height: 100px; }
      .touchActionNone { touch-action: none; }
      #childVisible { width: 200px; height: 25px; }
      #childHidden { width: 200px; height: 30px; visibility: hidden; }
      #childDisplayNone { width: 200px; height: 30px; display: none; }
    </style>
    <div id='parent'>
      <div id='childVisible'></div>
      <div id='childHidden'></div>
    </div>
  )HTML");

  // Initially there should be no hit test display items because there is no
  // touch action.
  const auto& scrolling_client = ViewScrollingBackgroundClient();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));

  // Add a touch action to parent and ensure that hit test display items are
  // created for both the parent and the visible child.
  auto* parent_element = GetElementById("parent");
  parent_element->setAttribute(html_names::kClassAttr, "touchActionNone");
  UpdateAllLifecyclePhasesForTest();
  auto* parent = GetLayoutObjectByElementId("parent");
  auto* child_visible = GetLayoutObjectByElementId("childVisible");
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType),
                          IsSameId(parent, DisplayItem::kHitTest),
                          IsSameId(child_visible, DisplayItem::kHitTest)));

  // Remove the touch action from parent and ensure no hit test display items
  // are left.
  parent_element->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));
}

TEST_F(BlockPainterTouchActionTest, TouchActionRectSubsequenceCaching) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #touchaction {
        width: 100px;
        height: 100px;
        touch-action: none;
      }
      #sibling {
        width: 100px;
        height: 100px;
        background: blue;
      }
    </style>
    <div id='touchaction'></div>
  )HTML");

  const auto& scrolling_client = ViewScrollingBackgroundClient();
  const auto* touchaction = GetLayoutObjectByElementId("touchaction");
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType),
                          IsSameId(touchaction, DisplayItem::kHitTest)));

  const auto& hit_test_client = *touchaction->EnclosingLayer();
  EXPECT_SUBSEQUENCE(hit_test_client, 1, 2);

  PaintChunk::Id root_chunk_id(scrolling_client, kDocumentBackgroundType);
  auto root_chunk_properties =
      GetLayoutView().FirstFragment().ContentsProperties();

  PaintChunk::Id hit_test_chunk_id(hit_test_client,
                                   kNonScrollingBackgroundChunkType);
  auto hit_test_chunk_properties = touchaction->EnclosingLayer()
                                       ->GetLayoutObject()
                                       .FirstFragment()
                                       .ContentsProperties();
  HitTestData hit_test_data;
  hit_test_data.touch_action_rects.emplace_back(LayoutRect(0, 0, 100, 100));

  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, root_chunk_id, root_chunk_properties),
                  IsPaintChunk(1, 2, hit_test_chunk_id,
                               hit_test_chunk_properties, hit_test_data)));

  // Trigger a repaint with the whole HTML subsequence cached.
  GetLayoutView().Layer()->SetNeedsRepaint();
  EXPECT_TRUE(PaintWithoutCommit());
  EXPECT_EQ(2, NumCachedNewItems());
  CommitAndFinishCycle();

  EXPECT_SUBSEQUENCE(hit_test_client, 1, 2);

  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, root_chunk_id, root_chunk_properties),
                  IsPaintChunk(1, 2, hit_test_chunk_id,
                               hit_test_chunk_properties, hit_test_data)));
}

TEST_F(BlockPainterTouchActionTest, TouchActionRectPaintCaching) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #touchaction {
        width: 100px;
        height: 100px;
        touch-action: none;
      }
      #sibling {
        width: 100px;
        height: 100px;
        background: blue;
      }
    </style>
    <div id='touchaction'></div>
    <div id='sibling'></div>
  )HTML");

  const auto& scrolling_client = ViewScrollingBackgroundClient();
  const auto* touchaction = GetLayoutObjectByElementId("touchaction");
  auto* sibling_element = GetElementById("sibling");
  const auto* sibling = sibling_element->GetLayoutObject();
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType),
                          IsSameId(touchaction, DisplayItem::kHitTest),
                          IsSameId(sibling, kBackgroundType)));

  PaintChunk::Id root_chunk_id(scrolling_client, kDocumentBackgroundType);
  auto root_chunk_properties =
      GetLayoutView().FirstFragment().ContentsProperties();

  PaintChunk::Id hit_test_chunk_id(*touchaction->EnclosingLayer(),
                                   kNonScrollingBackgroundChunkType);
  auto hit_test_chunk_properties = touchaction->EnclosingLayer()
                                       ->GetLayoutObject()
                                       .FirstFragment()
                                       .ContentsProperties();
  HitTestData hit_test_data;
  hit_test_data.touch_action_rects.emplace_back(LayoutRect(0, 0, 100, 100));

  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, root_chunk_id, root_chunk_properties),
                  IsPaintChunk(1, 3, hit_test_chunk_id,
                               hit_test_chunk_properties, hit_test_data)));

  sibling_element->setAttribute(html_names::kStyleAttr, "background: green;");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(PaintWithoutCommit());
  // Only the background display item of the sibling should be invalidated.
  EXPECT_EQ(2, NumCachedNewItems());
  CommitAndFinishCycle();

  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, root_chunk_id, root_chunk_properties),
                  IsPaintChunk(1, 3, hit_test_chunk_id,
                               hit_test_chunk_properties, hit_test_data)));
}

TEST_F(BlockPainterTouchActionTest, TouchActionRectScrollingContents) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #scroller {
        width: 100px;
        height: 100px;
        overflow: scroll;
        touch-action: none;
        will-change: transform;
        background-color: blue;
      }
      #child {
        width: 10px;
        height: 400px;
      }
    </style>
    <div id='scroller'>
      <div id='child'></div>
    </div>
  )HTML");

  const auto& root_client = GetLayoutView()
                                .GetScrollableArea()
                                ->GetScrollingBackgroundDisplayItemClient();
  auto* scroller_element = GetElementById("scroller");
  auto* scroller = ToLayoutBoxModelObject(scroller_element->GetLayoutObject());
  const auto& scroller_client =
      scroller->GetScrollableArea()->GetScrollingBackgroundDisplayItemClient();
  auto* child_element = GetElementById("child");
  auto* child = child_element->GetLayoutObject();
  auto& non_scroller_paint_controller = RootPaintController();
  auto& scroller_paint_controller = scroller->GetScrollableArea()
                                        ->Layer()
                                        ->GraphicsLayerBacking()
                                        ->GetPaintController();
  EXPECT_THAT(scroller_paint_controller.GetDisplayItemList(),
              ElementsAre(IsSameId(&scroller_client, kBackgroundType),
                          IsSameId(&scroller_client, DisplayItem::kHitTest),
                          IsSameId(child, DisplayItem::kHitTest)));
  HitTestData hit_test_data;
  hit_test_data.touch_action_rects.emplace_back(LayoutRect(0, 0, 100, 400));
  hit_test_data.touch_action_rects.emplace_back(LayoutRect(0, 0, 10, 400));
  EXPECT_THAT(
      scroller_paint_controller.PaintChunks(),
      ElementsAre(IsPaintChunk(
          0, 3, PaintChunk::Id(*scroller, kScrollingBackgroundChunkType),
          scroller->FirstFragment().ContentsProperties(), hit_test_data)));

  EXPECT_THAT(non_scroller_paint_controller.GetDisplayItemList(),
              ElementsAre(IsSameId(&root_client, kDocumentBackgroundType)));
  EXPECT_THAT(non_scroller_paint_controller.PaintChunks(),
              ElementsAre(IsPaintChunk(
                  0, 1, PaintChunk::Id(root_client, kDocumentBackgroundType),
                  GetLayoutView().FirstFragment().ContentsProperties())));
}

TEST_F(BlockPainterTouchActionTest, TouchActionRectPaintChunkChanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #touchaction {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id='touchaction'></div>
  )HTML");

  const auto& scrolling_client = ViewScrollingBackgroundClient();
  auto* touchaction_element = GetElementById("touchaction");
  auto* touchaction = touchaction_element->GetLayoutObject();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));

  PaintChunk::Id root_chunk_id(scrolling_client, kDocumentBackgroundType);
  auto root_chunk_properties =
      GetLayoutView().FirstFragment().ContentsProperties();

  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, root_chunk_id, root_chunk_properties)));

  touchaction_element->setAttribute(html_names::kStyleAttr,
                                    "touch-action: none;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType),
                          IsSameId(touchaction, DisplayItem::kHitTest)));

  PaintChunk::Id hit_test_chunk_id(*touchaction->EnclosingLayer(),
                                   kNonScrollingBackgroundChunkType);
  auto hit_test_chunk_properties = touchaction->EnclosingLayer()
                                       ->GetLayoutObject()
                                       .FirstFragment()
                                       .ContentsProperties();
  HitTestData hit_test_data;
  hit_test_data.touch_action_rects.emplace_back(LayoutRect(0, 0, 100, 100));

  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, root_chunk_id, root_chunk_properties),
                  IsPaintChunk(1, 2, hit_test_chunk_id,
                               hit_test_chunk_properties, hit_test_data)));

  touchaction_element->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));
  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, root_chunk_id, root_chunk_properties)));
}

namespace {
class BlockPainterMockEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) final {}
};
}  // namespace

TEST_F(BlockPainterTouchActionTest, TouchHandlerRectsWithoutPaint) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #parent { width: 100px; height: 100px; }
      #child { width: 200px; height: 50px; }
    </style>
    <div id='parent'>
      <div id='child'></div>
    </div>
  )HTML");

  // Initially there should be no hit test display items because there are no
  // event handlers.
  const auto& scrolling_client = ViewScrollingBackgroundClient();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));

  // Add an event listener to parent and ensure that hit test display items are
  // created for both the parent and child.
  BlockPainterMockEventListener* callback =
      MakeGarbageCollected<BlockPainterMockEventListener>();
  auto* parent_element = GetElementById("parent");
  parent_element->addEventListener(event_type_names::kTouchstart, callback);
  UpdateAllLifecyclePhasesForTest();

  auto* parent = GetLayoutObjectByElementId("parent");
  auto* child = GetLayoutObjectByElementId("child");
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType),
                          IsSameId(parent, DisplayItem::kHitTest),
                          IsSameId(child, DisplayItem::kHitTest)));

  // Remove the event handler from parent and ensure no hit test display items
  // are left.
  parent_element->RemoveAllEventListeners();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));
}

TEST_F(BlockPainterTouchActionTest, TouchActionRectsAcrossPaintChanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #parent { width: 100px; height: 100px; touch-action: none; }
      #child { width: 200px; height: 50px; }
    </style>
    <div id='parent'>
      <div id='child'></div>
    </div>
  )HTML");

  const auto& scrolling_client = ViewScrollingBackgroundClient();
  auto* parent = GetLayoutObjectByElementId("parent");
  auto* child = GetLayoutObjectByElementId("child");
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType),
                          IsSameId(parent, DisplayItem::kHitTest),
                          IsSameId(child, DisplayItem::kHitTest)));

  auto* child_element = GetElementById("parent");
  child_element->setAttribute("style", "background: blue;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType),
                          IsSameId(parent, kBackgroundType),
                          IsSameId(parent, DisplayItem::kHitTest),
                          IsSameId(child, DisplayItem::kHitTest)));
}

TEST_F(BlockPainterTouchActionTest, ScrolledHitTestChunkProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #scroller {
        width: 100px;
        height: 100px;
        overflow: scroll;
        touch-action: none;
      }
      #child {
        width: 200px;
        height: 50px;
        touch-action: none;
      }
    </style>
    <div id='scroller'>
      <div id='child'></div>
    </div>
  )HTML");

  const auto& scrolling_client = ViewScrollingBackgroundClient();
  const auto* scroller =
      To<LayoutBlock>(GetLayoutObjectByElementId("scroller"));
  const auto* child = GetLayoutObjectByElementId("child");
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType),
                          IsSameId(scroller, DisplayItem::kHitTest),
                          IsSameId(scroller, DisplayItem::kScrollHitTest),
                          IsSameId(child, DisplayItem::kHitTest)));

  HitTestData scroller_touch_action_hit_test_data;
  scroller_touch_action_hit_test_data.touch_action_rects.emplace_back(
      LayoutRect(0, 0, 100, 100));
  const auto& scrolling_contents_properties =
      scroller->FirstFragment().ContentsProperties();
  HitTestData scroll_hit_test_data;
  scroll_hit_test_data.SetScrollHitTest(
      &scrolling_contents_properties.Transform(), IntRect(0, 0, 100, 100));
  HitTestData scrolled_hit_test_data;
  scrolled_hit_test_data.touch_action_rects.emplace_back(
      LayoutRect(0, 0, 200, 50));

  const auto& paint_chunks = RootPaintController().PaintChunks();
  EXPECT_THAT(
      paint_chunks,
      ElementsAre(
          IsPaintChunk(
              0, 1, PaintChunk::Id(scrolling_client, kDocumentBackgroundType),
              GetLayoutView().FirstFragment().ContentsProperties()),
          IsPaintChunk(1, 2,
                       PaintChunk::Id(*scroller->Layer(),
                                      kNonScrollingBackgroundChunkType),
                       scroller->FirstFragment().LocalBorderBoxProperties(),
                       scroller_touch_action_hit_test_data),
          IsPaintChunk(2, 3,
                       PaintChunk::Id(*scroller, DisplayItem::kScrollHitTest),
                       scroller->FirstFragment().LocalBorderBoxProperties(),
                       scroll_hit_test_data),
          IsPaintChunk(
              3, 4,
              PaintChunk::Id(*scroller, kScrollingContentsBackgroundChunkType),
              scroller->FirstFragment().ContentsProperties(),
              scrolled_hit_test_data)));

  const auto& scroller_paint_chunk = paint_chunks[1];
  EXPECT_EQ(IntRect(0, 0, 100, 100), scroller_paint_chunk.bounds);
  // The hit test rect for the scroller itself should not be scrolled.
  EXPECT_FALSE(scroller_paint_chunk.properties.Transform().ScrollNode());

  const auto& scrolled_paint_chunk = paint_chunks[3];
  EXPECT_EQ(IntRect(0, 0, 200, 50), scrolled_paint_chunk.bounds);
  // The hit test rect for the scrolled contents should be scrolled.
  EXPECT_TRUE(scrolled_paint_chunk.properties.Transform().ScrollNode());
}

}  // namespace blink
