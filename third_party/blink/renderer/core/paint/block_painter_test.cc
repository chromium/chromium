// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/block_painter.h"

#include <gtest/gtest.h>
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_hit_test_display_item.h"

namespace blink {

using BlockPainterTest = PaintControllerPaintTest;

INSTANTIATE_SPV2_TEST_CASE_P(BlockPainterTest);

TEST_P(BlockPainterTest, ScrollHitTestProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0 }
      #container { width: 200px; height: 200px;
                  overflow: scroll; background: blue; }
      #child { width: 100px; height: 300px; background: green; }
    </style>
    <div id='container'>
      <div id='child'></div>
    </div>
  )HTML");

  auto& container = *GetLayoutObjectByElementId("container");
  auto& child = *GetLayoutObjectByElementId("child");

  // The scroll hit test should be after the container background but before the
  // scrolled contents.
  EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
                      TestDisplayItem(container, kBackgroundType),
                      TestDisplayItem(container, kScrollHitTestType),
                      TestDisplayItem(child, kBackgroundType));
  const auto& paint_chunks =
      RootPaintController().GetPaintArtifact().PaintChunks();
  EXPECT_EQ(4u, paint_chunks.size());
  const auto& root_chunk = RootPaintController().PaintChunks()[0];
  EXPECT_EQ(GetLayoutView().Layer(), &root_chunk.id.client);
  const auto& container_chunk = RootPaintController().PaintChunks()[1];
  EXPECT_EQ(ToLayoutBoxModelObject(container).Layer(),
            &container_chunk.id.client);
  // The container's scroll hit test.
  const auto& scroll_hit_test_chunk = RootPaintController().PaintChunks()[2];
  EXPECT_EQ(&container, &scroll_hit_test_chunk.id.client);
  EXPECT_EQ(kScrollHitTestType, scroll_hit_test_chunk.id.type);
  // The scrolled contents.
  const auto& contents_chunk = RootPaintController().PaintChunks()[3];
  EXPECT_EQ(&container, &contents_chunk.id.client);

  // The document should not scroll so there should be no scroll offset
  // transform.
  auto* root_transform = root_chunk.properties.Transform();
  EXPECT_EQ(nullptr, root_transform->ScrollNode());

  // The container's background chunk should not scroll and therefore should use
  // the root transform. Its local transform is actually a paint offset
  // transform.
  auto* container_transform = container_chunk.properties.Transform()->Parent();
  EXPECT_EQ(root_transform, container_transform);
  EXPECT_EQ(nullptr, container_transform->ScrollNode());

  // The scroll hit test should not be scrolled and should not be clipped.
  // Its local transform is actually a paint offset transform.
  auto* scroll_hit_test_transform =
      scroll_hit_test_chunk.properties.Transform()->Parent();
  EXPECT_EQ(nullptr, scroll_hit_test_transform->ScrollNode());
  EXPECT_EQ(root_transform, scroll_hit_test_transform);
  auto* scroll_hit_test_clip = scroll_hit_test_chunk.properties.Clip();
  EXPECT_EQ(FloatRect(0, 0, 800, 600), scroll_hit_test_clip->ClipRect().Rect());

  // The scrolled contents should be scrolled and clipped.
  auto* contents_transform = contents_chunk.properties.Transform();
  auto* contents_scroll = contents_transform->ScrollNode();
  EXPECT_EQ(IntSize(200, 300), contents_scroll->ContentsSize());
  EXPECT_EQ(IntRect(0, 0, 200, 200), contents_scroll->ContainerRect());
  auto* contents_clip = contents_chunk.properties.Clip();
  EXPECT_EQ(FloatRect(0, 0, 200, 200), contents_clip->ClipRect().Rect());

  // The scroll hit test display item maintains a reference to a scroll offset
  // translation node and the contents should be scrolled by this node.
  const auto& scroll_hit_test_display_item =
      static_cast<const ScrollHitTestDisplayItem&>(
          RootPaintController()
              .GetDisplayItemList()[scroll_hit_test_chunk.begin_index]);
  EXPECT_EQ(contents_transform,
            &scroll_hit_test_display_item.scroll_offset_node());
}

TEST_P(BlockPainterTest, FrameScrollHitTestProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #child { width: 100px; height: 2000px; background: green; }
    </style>
    <div id='child'></div>
  )HTML");

  auto& html = *GetDocument().documentElement()->GetLayoutObject();
  auto& child = *GetLayoutObjectByElementId("child");

  // The scroll hit test should be after the document background but before the
  // scrolled contents.
  EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
                      TestDisplayItem(GetLayoutView(), kScrollHitTestType),
                      TestDisplayItem(child, kBackgroundType));

  const auto& paint_chunks =
      RootPaintController().GetPaintArtifact().PaintChunks();
  EXPECT_EQ(3u, paint_chunks.size());
  const auto& root_chunk = RootPaintController().PaintChunks()[0];
  EXPECT_EQ(GetLayoutView().Layer(), &root_chunk.id.client);
  const auto& scroll_hit_test_chunk = RootPaintController().PaintChunks()[1];
  EXPECT_EQ(&GetLayoutView(), &scroll_hit_test_chunk.id.client);
  EXPECT_EQ(kScrollHitTestType, scroll_hit_test_chunk.id.type);
  // The scrolled contents.
  const auto& contents_chunk = RootPaintController().PaintChunks()[2];
  EXPECT_EQ(ToLayoutBoxModelObject(html).Layer(), &contents_chunk.id.client);

  // The scroll hit test should not be scrolled and should not be clipped.
  auto* scroll_hit_test_transform =
      scroll_hit_test_chunk.properties.Transform();
  EXPECT_EQ(nullptr, scroll_hit_test_transform->ScrollNode());
  auto* scroll_hit_test_clip = scroll_hit_test_chunk.properties.Clip();
  EXPECT_EQ(FloatRect(LayoutRect::InfiniteIntRect()),
            scroll_hit_test_clip->ClipRect().Rect());

  // The scrolled contents should be scrolled and clipped.
  auto* contents_transform = contents_chunk.properties.Transform();
  auto* contents_scroll = contents_transform->ScrollNode();
  EXPECT_EQ(IntSize(800, 2000), contents_scroll->ContentsSize());
  EXPECT_EQ(IntRect(0, 0, 800, 600), contents_scroll->ContainerRect());
  auto* contents_clip = contents_chunk.properties.Clip();
  EXPECT_EQ(FloatRect(0, 0, 800, 600), contents_clip->ClipRect().Rect());

  // The scroll hit test display item maintains a reference to a scroll offset
  // translation node and the contents should be scrolled by this node.
  const auto& scroll_hit_test_display_item =
      static_cast<const ScrollHitTestDisplayItem&>(
          RootPaintController()
              .GetDisplayItemList()[scroll_hit_test_chunk.begin_index]);
  EXPECT_EQ(contents_transform,
            &scroll_hit_test_display_item.scroll_offset_node());
}

class BlockPainterTestWithPaintTouchAction
    : public PaintControllerPaintTestBase,
      private ScopedPaintTouchActionRectsForTest {
 public:
  BlockPainterTestWithPaintTouchAction()
      : PaintControllerPaintTestBase(),
        ScopedPaintTouchActionRectsForTest(true) {}
};

TEST_F(BlockPainterTestWithPaintTouchAction, TouchActionRectsWithoutPaint) {
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
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 1,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType));

  // Add a touch action to parent and ensure that hit test display items are
  // created for both the parent and the visible child.
  auto* parent_element = GetElementById("parent");
  parent_element->setAttribute(HTMLNames::classAttr, "touchActionNone");
  GetDocument().View()->UpdateAllLifecyclePhases();
  auto* parent = GetLayoutObjectByElementId("parent");
  auto* childVisible = GetLayoutObjectByElementId("childVisible");
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 3,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType),
      TestDisplayItem(*parent, DisplayItem::kHitTest),
      TestDisplayItem(*childVisible, DisplayItem::kHitTest));

  // Remove the touch action from parent and ensure no hit test display items
  // are left.
  parent_element->removeAttribute(HTMLNames::classAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 1,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType));
}

TEST_F(BlockPainterTestWithPaintTouchAction, TouchActionRectPaintCaching) {
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
  auto* touchaction_element = GetElementById("touchaction");
  auto* touchaction = touchaction_element->GetLayoutObject();
  auto* sibling_element = GetElementById("sibling");
  auto* sibling = sibling_element->GetLayoutObject();
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 3,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType),
      TestDisplayItem(*touchaction, DisplayItem::kHitTest),
      TestDisplayItem(*sibling, kBackgroundType));

  {
    const auto& paint_chunks =
        RootPaintController().GetPaintArtifact().PaintChunks();
    EXPECT_EQ(paint_chunks.size(), 2u);
    auto& background_chunk = paint_chunks[0];
    EXPECT_EQ(nullptr, background_chunk.GetHitTestData());
    auto& hit_test_chunk = paint_chunks[1];
    DCHECK(hit_test_chunk.GetHitTestData());
    EXPECT_EQ(1u, hit_test_chunk.GetHitTestData()->touch_action_rects.size());
    auto& touch_action_rect =
        hit_test_chunk.GetHitTestData()->touch_action_rects[0];
    EXPECT_EQ(LayoutRect(0, 0, 100, 100), touch_action_rect.rect);
    EXPECT_EQ(TouchAction::kTouchActionNone,
              touch_action_rect.whitelisted_touch_action);
  }

  sibling_element->setAttribute(HTMLNames::styleAttr, "background: green;");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(PaintWithoutCommit());
  // Only the background display item of the sibling should be invalidated.
  EXPECT_EQ(2, NumCachedNewItems());
  CommitAndFinishCycle();

  {
    const auto& paint_chunks =
        RootPaintController().GetPaintArtifact().PaintChunks();
    EXPECT_EQ(paint_chunks.size(), 2u);
    auto& background_chunk = paint_chunks[0];
    EXPECT_EQ(nullptr, background_chunk.GetHitTestData());
    auto& hit_test_chunk = paint_chunks[1];
    DCHECK(hit_test_chunk.GetHitTestData());
    EXPECT_EQ(1u, hit_test_chunk.GetHitTestData()->touch_action_rects.size());
    auto& touch_action_rect =
        hit_test_chunk.GetHitTestData()->touch_action_rects[0];
    EXPECT_EQ(LayoutRect(0, 0, 100, 100), touch_action_rect.rect);
    EXPECT_EQ(TouchAction::kTouchActionNone,
              touch_action_rect.whitelisted_touch_action);
  }
}

TEST_F(BlockPainterTestWithPaintTouchAction, TouchActionRectScrollingContents) {
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
  LayoutBoxModelObject* scroller =
      static_cast<LayoutBoxModelObject*>(scroller_element->GetLayoutObject());
  auto* child_element = GetElementById("child");
  auto* child = child_element->GetLayoutObject();
  auto& non_scroller_paint_controller = RootPaintController();
  auto& scroller_paint_controller = scroller->GetScrollableArea()
                                        ->Layer()
                                        ->GraphicsLayerBacking()
                                        ->GetPaintController();
  EXPECT_DISPLAY_LIST(
      scroller_paint_controller.GetDisplayItemList(), 3,
      TestDisplayItem(scroller->GetScrollableArea()
                          ->GetScrollingBackgroundDisplayItemClient(),
                      kBackgroundType),
      TestDisplayItem(*scroller, DisplayItem::kHitTest),
      TestDisplayItem(*child, DisplayItem::kHitTest));
  EXPECT_DISPLAY_LIST(non_scroller_paint_controller.GetDisplayItemList(), 1,
                      TestDisplayItem(root_client, kDocumentBackgroundType));

  {
    const auto& paint_chunks =
        scroller_paint_controller.GetPaintArtifact().PaintChunks();
    EXPECT_EQ(paint_chunks.size(), 1u);
    auto& hit_test_chunk = paint_chunks[0];
    DCHECK(hit_test_chunk.GetHitTestData());
    EXPECT_EQ(2u, hit_test_chunk.GetHitTestData()->touch_action_rects.size());
    {
      auto& touch_action_rect =
          hit_test_chunk.GetHitTestData()->touch_action_rects[0];
      EXPECT_EQ(LayoutRect(0, 0, 100, 400), touch_action_rect.rect);
      EXPECT_EQ(TouchAction::kTouchActionNone,
                touch_action_rect.whitelisted_touch_action);
    }
    {
      auto& touch_action_rect =
          hit_test_chunk.GetHitTestData()->touch_action_rects[1];
      EXPECT_EQ(LayoutRect(0, 0, 10, 400), touch_action_rect.rect);
      EXPECT_EQ(TouchAction::kTouchActionNone,
                touch_action_rect.whitelisted_touch_action);
    }
  }
  {
    const auto& paint_chunks =
        non_scroller_paint_controller.GetPaintArtifact().PaintChunks();
    EXPECT_EQ(paint_chunks.size(), 1u);
    auto& hit_test_chunk = paint_chunks[0];
    EXPECT_FALSE(hit_test_chunk.GetHitTestData());
  }
}

TEST_F(BlockPainterTestWithPaintTouchAction, TouchActionRectPaintChunkChanges) {
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
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 1,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType));

  {
    const auto& paint_chunks =
        RootPaintController().GetPaintArtifact().PaintChunks();
    EXPECT_EQ(paint_chunks.size(), 1u);
    EXPECT_EQ(nullptr, paint_chunks[0].GetHitTestData());
  }

  touchaction_element->setAttribute(HTMLNames::styleAttr,
                                    "touch-action: none;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 2,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType),
      TestDisplayItem(*touchaction, DisplayItem::kHitTest));

  {
    const auto& paint_chunks =
        RootPaintController().GetPaintArtifact().PaintChunks();
    EXPECT_EQ(paint_chunks.size(), 2u);
    auto& background_chunk = paint_chunks[0];
    EXPECT_EQ(nullptr, background_chunk.GetHitTestData());
    auto& hit_test_chunk = paint_chunks[1];
    DCHECK(hit_test_chunk.GetHitTestData());
    EXPECT_EQ(1u, hit_test_chunk.GetHitTestData()->touch_action_rects.size());
    auto& touch_action_rect =
        hit_test_chunk.GetHitTestData()->touch_action_rects[0];
    EXPECT_EQ(LayoutRect(0, 0, 100, 100), touch_action_rect.rect);
    EXPECT_EQ(TouchAction::kTouchActionNone,
              touch_action_rect.whitelisted_touch_action);
  }

  touchaction_element->removeAttribute(HTMLNames::styleAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 1,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType));
  {
    const auto& paint_chunks =
        RootPaintController().GetPaintArtifact().PaintChunks();
    EXPECT_EQ(paint_chunks.size(), 1u);
    EXPECT_EQ(nullptr, paint_chunks[0].GetHitTestData());
  }
}

namespace {
class BlockPainterMockEventListener final : public EventListener {
 public:
  BlockPainterMockEventListener() : EventListener(kCPPEventListenerType) {}

  bool operator==(const EventListener& other) const final {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event*) final {}
};
}  // namespace

TEST_F(BlockPainterTestWithPaintTouchAction, TouchHandlerRectsWithoutPaint) {
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
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 1,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType));

  // Add an event listener to parent and ensure that hit test display items are
  // created for both the parent and child.
  BlockPainterMockEventListener* callback = new BlockPainterMockEventListener();
  auto* parent_element = GetElementById("parent");
  parent_element->addEventListener(EventTypeNames::touchstart, callback);
  GetDocument().View()->UpdateAllLifecyclePhases();
  auto* parent = GetLayoutObjectByElementId("parent");
  auto* child = GetLayoutObjectByElementId("child");
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 3,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType),
      TestDisplayItem(*parent, DisplayItem::kHitTest),
      TestDisplayItem(*child, DisplayItem::kHitTest));

  // Remove the event handler from parent and ensure no hit test display items
  // are left.
  parent_element->RemoveAllEventListeners();
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 1,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType));
}

TEST_F(BlockPainterTestWithPaintTouchAction,
       TouchActionRectsAcrossPaintChanges) {
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
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 3,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType),
      TestDisplayItem(*parent, DisplayItem::kHitTest),
      TestDisplayItem(*child, DisplayItem::kHitTest));

  auto* child_element = GetElementById("parent");
  child_element->setAttribute("style", "background: blue;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 4,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType),
      TestDisplayItem(*parent, kBackgroundType),
      TestDisplayItem(*parent, DisplayItem::kHitTest),
      TestDisplayItem(*child, DisplayItem::kHitTest));
}

TEST_F(BlockPainterTestWithPaintTouchAction, ScrolledHitTestChunkProperties) {
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
  auto* scroller = GetLayoutObjectByElementId("scroller");
  auto* child = GetLayoutObjectByElementId("child");
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 3,
      TestDisplayItem(scrolling_client, kDocumentBackgroundType),
      TestDisplayItem(*scroller, DisplayItem::kHitTest),
      TestDisplayItem(*child, DisplayItem::kHitTest));

  const auto& paint_chunks =
      RootPaintController().GetPaintArtifact().PaintChunks();
  EXPECT_EQ(3u, paint_chunks.size());

  const auto& scroller_paint_chunk = RootPaintController().PaintChunks()[1];
  EXPECT_EQ(ToLayoutBoxModelObject(scroller)->Layer(),
            &scroller_paint_chunk.id.client);
  EXPECT_EQ(FloatRect(0, 0, 100, 100), scroller_paint_chunk.bounds);
  // The hit test rect for the scroller itself should not be scrolled.
  EXPECT_FALSE(scroller_paint_chunk.properties.Transform()->ScrollNode());

  const auto& scrolled_paint_chunk = RootPaintController().PaintChunks()[2];
  EXPECT_EQ(scroller, &scrolled_paint_chunk.id.client);
  EXPECT_EQ(FloatRect(0, 0, 200, 50), scrolled_paint_chunk.bounds);
  // The hit test rect for the scrolled contents should be scrolled.
  EXPECT_TRUE(scrolled_paint_chunk.properties.Transform()->ScrollNode());
}

}  // namespace blink
