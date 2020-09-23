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
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

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

  // Initially there should be no hit test data because there is no touch
  // action.
  const auto& scrolling_client = ViewScrollingBackgroundClient();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));
  PaintChunk::Id root_chunk_id(scrolling_client, kDocumentBackgroundType);
  auto root_chunk_properties =
      GetLayoutView().FirstFragment().ContentsProperties();
  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, root_chunk_id, root_chunk_properties)));

  // Add a touch action to parent and ensure that hit test data are created
  // for both the parent and the visible child.
  auto* parent_element = GetElementById("parent");
  parent_element->setAttribute(html_names::kClassAttr, "touchActionNone");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));
  HitTestData hit_test_data;
  hit_test_data.touch_action_rects = {{IntRect(0, 0, 100, 100)},
                                      {IntRect(0, 0, 200, 25)}};
  EXPECT_THAT(RootPaintController().PaintChunks(),
              ElementsAre(IsPaintChunk(0, 1, root_chunk_id,
                                       root_chunk_properties, &hit_test_data)));

  // Remove the touch action from parent and ensure no hit test data are left.
  parent_element->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));
  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, root_chunk_id, root_chunk_properties)));
}

TEST_F(BlockPainterTouchActionTest, TouchActionRectSubsequenceCaching) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #stacking-context {
        position: absolute;
        z-index: 1;
      }
      #touchaction {
        width: 100px;
        height: 100px;
        touch-action: none;
      }
    </style>
    <div id='stacking-context'>
      <div id='touchaction'></div>
    </div>
  )HTML");

  const auto& scrolling_client = ViewScrollingBackgroundClient();
  const auto* touchaction = GetLayoutObjectByElementId("touchaction");
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));

  const auto& hit_test_client =
      *ToLayoutBox(GetLayoutObjectByElementId("stacking-context"))->Layer();
  EXPECT_SUBSEQUENCE(hit_test_client, 1, 2);

  PaintChunk::Id root_chunk_id(scrolling_client, kDocumentBackgroundType);
  auto root_chunk_properties =
      GetLayoutView().FirstFragment().ContentsProperties();

  PaintChunk::Id hit_test_chunk_id(hit_test_client, DisplayItem::kLayerChunk);
  auto hit_test_chunk_properties = touchaction->EnclosingLayer()
                                       ->GetLayoutObject()
                                       .FirstFragment()
                                       .ContentsProperties();
  HitTestData hit_test_data;
  hit_test_data.touch_action_rects = {{IntRect(0, 0, 100, 100)}};

  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(
          IsPaintChunk(0, 1, root_chunk_id, root_chunk_properties),
          IsPaintChunk(1, 1, hit_test_chunk_id, hit_test_chunk_properties,
                       &hit_test_data, IntRect(0, 0, 100, 100))));

  // Trigger a repaint with the whole HTML subsequence cached.
  GetLayoutView().Layer()->SetNeedsRepaint();
  EXPECT_TRUE(PaintWithoutCommit());
  EXPECT_EQ(1, NumCachedNewItems());
  CommitAndFinishCycle();

  EXPECT_SUBSEQUENCE(hit_test_client, 1, 2);

  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(
          IsPaintChunk(0, 1, root_chunk_id, root_chunk_properties),
          IsPaintChunk(1, 1, hit_test_chunk_id, hit_test_chunk_properties,
                       &hit_test_data, IntRect(0, 0, 100, 100))));
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
  auto* sibling_element = GetElementById("sibling");
  const auto* sibling = sibling_element->GetLayoutObject();
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType),
                          IsSameId(sibling, kBackgroundType)));

  PaintChunk::Id root_chunk_id(scrolling_client, kDocumentBackgroundType);
  auto root_chunk_properties =
      GetLayoutView().FirstFragment().ContentsProperties();

  HitTestData hit_test_data;
  hit_test_data.touch_action_rects = {{IntRect(0, 0, 100, 100)}};

  EXPECT_THAT(RootPaintController().PaintChunks(),
              ElementsAre(IsPaintChunk(0, 2, root_chunk_id,
                                       root_chunk_properties, &hit_test_data)));

  sibling_element->setAttribute(html_names::kStyleAttr, "background: green;");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(PaintWithoutCommit());
  // Only the background display item of the sibling should be invalidated.
  EXPECT_EQ(1, NumCachedNewItems());
  CommitAndFinishCycle();

  EXPECT_THAT(RootPaintController().PaintChunks(),
              ElementsAre(IsPaintChunk(0, 2, root_chunk_id,
                                       root_chunk_properties, &hit_test_data)));
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
  auto& non_scroller_paint_controller = RootPaintController();
  auto& scroller_paint_controller = scroller->GetScrollableArea()
                                        ->Layer()
                                        ->GraphicsLayerBacking()
                                        ->GetPaintController();
  EXPECT_THAT(scroller_paint_controller.GetDisplayItemList(),
              ElementsAre(IsSameId(&scroller_client, kBackgroundType)));
  HitTestData hit_test_data;
  hit_test_data.touch_action_rects = {{IntRect(0, 0, 100, 400)},
                                      {IntRect(0, 0, 10, 400)}};
  EXPECT_THAT(
      scroller_paint_controller.PaintChunks(),
      ElementsAre(IsPaintChunk(
          0, 1, PaintChunk::Id(*scroller, kScrollingBackgroundChunkType),
          scroller->FirstFragment().ContentsProperties(), &hit_test_data)));

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
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));

  PaintChunk::Id hit_test_chunk_id(*touchaction->EnclosingLayer(),
                                   kNonScrollingBackgroundChunkType);
  HitTestData hit_test_data;
  hit_test_data.touch_action_rects = {{IntRect(0, 0, 100, 100)}};

  EXPECT_THAT(RootPaintController().PaintChunks(),
              ElementsAre(IsPaintChunk(0, 1, root_chunk_id,
                                       root_chunk_properties, &hit_test_data)));

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

  // Initially there should be no hit test data because there are no event
  // handlers.
  const auto& scrolling_client = ViewScrollingBackgroundClient();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));

  // Add an event listener to parent and ensure that hit test data are created
  // for both the parent and child.
  BlockPainterMockEventListener* callback =
      MakeGarbageCollected<BlockPainterMockEventListener>();
  auto* parent_element = GetElementById("parent");
  parent_element->addEventListener(event_type_names::kTouchstart, callback);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));
  HitTestData hit_test_data;
  hit_test_data.touch_action_rects = {{IntRect(0, 0, 100, 100)},
                                      {IntRect(0, 0, 200, 50)}};
  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(
          0, 1, PaintChunk::Id(scrolling_client, kDocumentBackgroundType),
          GetLayoutView().FirstFragment().ContentsProperties(), &hit_test_data,
          IntRect(0, 0, 800, 600))));

  // Remove the event handler from parent and ensure no hit test data are left.
  parent_element->RemoveAllEventListeners();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));
  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(
          0, 1, PaintChunk::Id(scrolling_client, kDocumentBackgroundType),
          GetLayoutView().FirstFragment().ContentsProperties(), nullptr,
          IntRect(0, 0, 800, 600))));
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
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));
  HitTestData hit_test_data;
  hit_test_data.touch_action_rects = {{IntRect(0, 0, 100, 100)},
                                      {IntRect(0, 0, 200, 50)}};
  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(
          0, 1, PaintChunk::Id(scrolling_client, kDocumentBackgroundType),
          GetLayoutView().FirstFragment().ContentsProperties(), &hit_test_data,
          IntRect(0, 0, 800, 600))));

  auto* child_element = GetElementById("child");
  child_element->setAttribute("style", "background: blue;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType),
                  IsSameId(child_element->GetLayoutObject(), kBackgroundType)));
  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(
          0, 2, PaintChunk::Id(scrolling_client, kDocumentBackgroundType),
          GetLayoutView().FirstFragment().ContentsProperties(), &hit_test_data,
          IntRect(0, 0, 800, 600))));
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
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&scrolling_client, kDocumentBackgroundType)));

  HitTestData scroller_touch_action_hit_test_data;
  scroller_touch_action_hit_test_data.touch_action_rects = {
      {IntRect(0, 0, 100, 100)}};
  HitTestData scroll_hit_test_data;
  scroll_hit_test_data.scroll_translation =
      scroller->FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test_data.scroll_hit_test_rect = IntRect(0, 0, 100, 100);
  HitTestData scrolled_hit_test_data;
  scrolled_hit_test_data.touch_action_rects = {{IntRect(0, 0, 200, 50)}};

  const auto& paint_chunks = RootPaintController().PaintChunks();
  EXPECT_THAT(
      paint_chunks,
      ElementsAre(
          IsPaintChunk(
              0, 1, PaintChunk::Id(scrolling_client, kDocumentBackgroundType),
              GetLayoutView().FirstFragment().ContentsProperties(), nullptr,
              IntRect(0, 0, 800, 600)),
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*scroller->Layer(), DisplayItem::kLayerChunk),
              scroller->FirstFragment().LocalBorderBoxProperties(),
              &scroller_touch_action_hit_test_data, IntRect(0, 0, 100, 100)),
          IsPaintChunk(1, 1,
                       PaintChunk::Id(*scroller, DisplayItem::kScrollHitTest),
                       scroller->FirstFragment().LocalBorderBoxProperties(),
                       &scroll_hit_test_data, IntRect(0, 0, 100, 100)),
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*scroller, kClippedContentsBackgroundChunkType),
              scroller->FirstFragment().ContentsProperties(),
              &scrolled_hit_test_data, IntRect(0, 0, 200, 50))));

  const auto& scroller_paint_chunk = paint_chunks[{0, 1}];
  // The hit test rect for the scroller itself should not be scrolled.
  EXPECT_FALSE(
      ToUnaliased(scroller_paint_chunk.properties.Transform()).ScrollNode());

  const auto& scrolled_paint_chunk = paint_chunks[{0, 3}];
  // The hit test rect for the scrolled contents should be scrolled.
  EXPECT_TRUE(
      ToUnaliased(scrolled_paint_chunk.properties.Transform()).ScrollNode());
}

}  // namespace blink
