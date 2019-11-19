// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_inputs_updater.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class CompositingInputsUpdaterTest : public RenderingTest {
 public:
  CompositingInputsUpdaterTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

// Tests that transitioning a sticky away from an ancestor overflow layer that
// does not have a scrollable area does not crash.
//
// See http://crbug.com/467721#c14
TEST_F(CompositingInputsUpdaterTest,
       ChangingAncestorOverflowLayerAwayFromNonScrollableDoesNotCrash) {
  // The setup for this test is quite complex. We need UpdateRecursive to
  // transition directly from a non-scrollable ancestor overflow layer to a
  // scrollable one.
  //
  // To achieve this both scrollers must always have a PaintLayer (achieved by
  // making them positioned), and the previous ancestor overflow must change
  // from being scrollable to non-scrollable (achieved by setting its overflow
  // property to visible at the same time as we change the inner scroller.)
  SetBodyInnerHTML(R"HTML(
    <style>#outerScroller { position: relative; overflow: scroll;
    height: 500px; width: 100px; }
    #innerScroller { position: relative; height: 100px; }
    #sticky { position: sticky; top: 0; height: 50px; width: 50px; }
    #padding { height: 200px; }</style>
    <div id='outerScroller'><div id='innerScroller'><div id='sticky'></div>
    <div id='padding'></div></div></div>
  )HTML");

  LayoutBoxModelObject* outer_scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("outerScroller"));
  LayoutBoxModelObject* inner_scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("innerScroller"));
  LayoutBoxModelObject* sticky =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky"));

  // Both scrollers must always have a layer.
  EXPECT_TRUE(outer_scroller->Layer());
  EXPECT_TRUE(inner_scroller->Layer());

  // The inner 'scroller' starts as non-overflow, so the sticky element's
  // ancestor overflow layer should be the outer scroller.
  EXPECT_TRUE(outer_scroller->GetScrollableArea());
  EXPECT_FALSE(inner_scroller->GetScrollableArea());
  EXPECT_TRUE(
      outer_scroller->GetScrollableArea()->GetStickyConstraintsMap().Contains(
          sticky->Layer()));
  EXPECT_EQ(sticky->Layer()->AncestorOverflowLayer(), outer_scroller->Layer());

  // Now make the outer scroller non-scrollable (i.e. overflow: visible), and
  // the inner scroller into an actual scroller.
  To<Element>(outer_scroller->GetNode())
      ->SetInlineStyleProperty(CSSPropertyID::kOverflow, "visible");
  To<Element>(inner_scroller->GetNode())
      ->SetInlineStyleProperty(CSSPropertyID::kOverflow, "scroll");

  // Before we update compositing inputs, validate that the current ancestor
  // overflow no longer has a scrollable area.
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_FALSE(sticky->Layer()->AncestorOverflowLayer()->GetScrollableArea());
  EXPECT_EQ(sticky->Layer()->AncestorOverflowLayer(), outer_scroller->Layer());

  UpdateAllLifecyclePhasesForTest();

  // Both scrollers must still have a layer.
  EXPECT_TRUE(outer_scroller->Layer());
  EXPECT_TRUE(inner_scroller->Layer());

  // However now the sticky is hanging off the inner scroller - and most
  // importantly we didnt crash when we switched ancestor overflow layers.
  EXPECT_TRUE(inner_scroller->GetScrollableArea());
  EXPECT_TRUE(
      inner_scroller->GetScrollableArea()->GetStickyConstraintsMap().Contains(
          sticky->Layer()));
  EXPECT_EQ(sticky->Layer()->AncestorOverflowLayer(), inner_scroller->Layer());
}

TEST_F(CompositingInputsUpdaterTest, UnclippedAndClippedRectsUnderScroll) {
  SetBodyInnerHTML(R"HTML(
    <div id=clip style="overflow: hidden; position: relative">
      <div id=target style="transform: translateZ(0); width: 200px; height: 200px; background: lightgray"></div>
     </div>
     <div style="position: relative; width: 20px; height: 3000px"></div>
  )HTML");

  LayoutBoxModelObject* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"));

  GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, 25),
                                                   kUserScroll);
  GetDocument()
      .View()
      ->GetLayoutView()
      ->Layer()
      ->SetNeedsCompositingInputsUpdate();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(IntRect(8, 8, 200, 200),
            target->Layer()->ClippedAbsoluteBoundingBox());
  EXPECT_EQ(IntRect(8, 8, 200, 200),
            target->Layer()->UnclippedAbsoluteBoundingBox());
}

TEST_F(CompositingInputsUpdaterTest,
       UnclippedAndClippedRectsUnderScrollFixedPos) {
  SetBodyInnerHTML(R"HTML(
    <div id=clip style="position: fixed; overflow: hidden;">
      <div id=target style=" transform: translateZ(0); width: 200px; height: 200px; background: lightgray"></div>
     </div>
     <div style="position: relative; width: 20px; height: 3000px"></div>
  )HTML");

  LayoutBoxModelObject* target =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"));

  GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, 25),
                                                   kUserScroll);
  GetDocument()
      .View()
      ->GetLayoutView()
      ->Layer()
      ->SetNeedsCompositingInputsUpdate();

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(IntRect(8, 8, 200, 200),
            target->Layer()->ClippedAbsoluteBoundingBox());
  EXPECT_EQ(IntRect(8, 8, 200, 200),
            target->Layer()->UnclippedAbsoluteBoundingBox());
}

TEST_F(CompositingInputsUpdaterTest, ClipPathAncestor) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent" style="clip-path: circle(100%)">
      <div id="child" style="width: 20px; height: 20px; will-change: transform">
        <div id="grandchild" style="position: relative";
      </div>
    </div>
  )HTML");

  PaintLayer* parent =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("parent"))->Layer();
  PaintLayer* child =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("child"))->Layer();
  PaintLayer* grandchild =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("grandchild"))->Layer();

  EXPECT_EQ(nullptr, parent->ClipPathAncestor());
  EXPECT_EQ(parent, child->ClipPathAncestor());
  EXPECT_EQ(parent, grandchild->ClipPathAncestor());
}

TEST_F(CompositingInputsUpdaterTest, MaskAncestor) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent" style="-webkit-mask-image: linear-gradient(black, white);">
      <div id="child" style="width: 20px; height: 20px; will-change: transform">
        <div id="grandchild" style="position: relative"></div>
      </div>
    </div>
  )HTML");

  PaintLayer* parent =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("parent"))->Layer();
  PaintLayer* child =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("child"))->Layer();
  PaintLayer* grandchild =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("grandchild"))->Layer();

  EXPECT_EQ(nullptr, parent->MaskAncestor());
  EXPECT_EQ(parent, child->MaskAncestor());
  EXPECT_EQ(parent, grandchild->MaskAncestor());
}

TEST_F(CompositingInputsUpdaterTest, LayoutContainmentLayer) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent" style="contain: layout">
      <div id="child" style="width: 20px; height: 20px; will-change: transform">
        <div id="grandchild" style="contain: layout; position: relative">
          <div id="greatgrandchild" style="position: relative"></div>
        </div>
      </div>
    </div>
  )HTML");

  PaintLayer* parent =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("parent"))->Layer();
  PaintLayer* child =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("child"))->Layer();
  PaintLayer* grandchild =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("grandchild"))->Layer();
  PaintLayer* greatgrandchild =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("greatgrandchild"))
          ->Layer();

  EXPECT_EQ(parent, parent->NearestContainedLayoutLayer());
  EXPECT_EQ(parent, child->NearestContainedLayoutLayer());
  EXPECT_EQ(grandchild, grandchild->NearestContainedLayoutLayer());
  EXPECT_EQ(grandchild, greatgrandchild->NearestContainedLayoutLayer());
}

}  // namespace blink
