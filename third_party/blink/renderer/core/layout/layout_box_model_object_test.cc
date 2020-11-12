// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutBoxModelObjectTest : public RenderingTest {
 protected:
  LayoutBoxModelObject* GetLayoutBoxModelObjectByElementId(const char* id) {
    return To<LayoutBoxModelObject>(GetLayoutObjectByElementId(id));
  }
};

// Verifies that the sticky constraints are correctly computed.
TEST_F(LayoutBoxModelObjectTest, StickyPositionConstraints) {
  SetBodyInnerHTML(R"HTML(
    <style>#sticky { position: sticky; top: 0; width: 100px; height: 100px;
    }
    #container { box-sizing: border-box; position: relative; top: 100px;
    height: 400px; width: 200px; padding: 10px; border: 5px solid black; }
    #scroller { width: 400px; height: 100px; overflow: auto;
    position: relative; top: 200px; border: 2px solid black; }
    .spacer { height: 1000px; }</style>
    <div id='scroller'><div id='container'><div
    id='sticky'></div></div><div class='spacer'></div></div>
  )HTML");
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollOffsetInt().Width(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().Y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  sticky->UpdateStickyPositionConstraints();
  ASSERT_EQ(scroller->Layer(), sticky->Layer()->AncestorScrollContainerLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollable_area->GetStickyConstraintsMap().at(sticky->Layer());
  ASSERT_EQ(0.f, constraints.top_offset);

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(IntRect(15, 115, 170, 370),
            EnclosingIntRect(
                constraints.scroll_container_relative_containing_block_rect));
  ASSERT_EQ(
      IntRect(15, 115, 100, 100),
      EnclosingIntRect(constraints.scroll_container_relative_sticky_box_rect));

  // The sticky constraining rect also doesn't include the border offset.
  ASSERT_EQ(IntRect(0, 0, 400, 100),
            EnclosingIntRect(sticky->ComputeStickyConstrainingRect()));
}

// Verifies that the sticky constraints are correctly computed in right to left.
TEST_F(LayoutBoxModelObjectTest, StickyPositionVerticalRLConstraints) {
  SetBodyInnerHTML(R"HTML(
    <style> html { -webkit-writing-mode: vertical-rl; }
    #sticky { position: sticky; top: 0; width: 100px; height: 100px;
    }
    #container { box-sizing: border-box; position: relative; top: 100px;
    height: 400px; width: 200px; padding: 10px; border: 5px solid black; }
    #scroller { width: 400px; height: 100px; overflow: auto;
    position: relative; top: 200px; border: 2px solid black; }
    .spacer { height: 1000px; }</style>
    <div id='scroller'><div id='container'><div
    id='sticky'></div></div><div class='spacer'></div></div>
  )HTML");
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollOffsetInt().Width(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().Y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  sticky->UpdateStickyPositionConstraints();
  ASSERT_EQ(scroller->Layer(), sticky->Layer()->AncestorScrollContainerLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollable_area->GetStickyConstraintsMap().at(sticky->Layer());
  ASSERT_EQ(0.f, constraints.top_offset);

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(IntRect(215, 115, 170, 370),
            EnclosingIntRect(
                constraints.scroll_container_relative_containing_block_rect));
  ASSERT_EQ(
      IntRect(285, 115, 100, 100),
      EnclosingIntRect(constraints.scroll_container_relative_sticky_box_rect));

  // The sticky constraining rect also doesn't include the border offset.
  ASSERT_EQ(IntRect(0, 0, 400, 100),
            EnclosingIntRect(sticky->ComputeStickyConstrainingRect()));
}

// Verifies that the sticky constraints are correctly computed for inline.
TEST_F(LayoutBoxModelObjectTest, StickyPositionInlineConstraints) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      .scroller { overflow: scroll; width: 100px; height: 100px; top: 100px;
          position: absolute; }
      .container { position: relative; top: 100px; height: 400px;
        width: 200px; }
      .sticky_box { width: 10px; height: 10px; top: 10px; position: sticky; }
      .inline { display: inline-block; }
      .spacer { height: 2000px; }
    </style>
    <div class='scroller' id='scroller'>
      <div class='container'>
        <div class='inline sticky_box' id='sticky'></div>
      </div>
      <div class='spacer'></div>
    </div>
  )HTML");

  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollOffsetInt().Width(), 50));
  EXPECT_EQ(50.f, scrollable_area->ScrollPosition().Y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");

  sticky->UpdateStickyPositionConstraints();

  EXPECT_EQ(scroller->Layer(), sticky->Layer()->AncestorScrollContainerLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollable_area->GetStickyConstraintsMap().at(sticky->Layer());

  EXPECT_EQ(10.f, constraints.top_offset);

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  EXPECT_EQ(IntRect(0, 100, 200, 400),
            EnclosingIntRect(
                constraints.scroll_container_relative_containing_block_rect));
  EXPECT_EQ(
      IntRect(0, 100, 10, 10),
      EnclosingIntRect(constraints.scroll_container_relative_sticky_box_rect));
  EXPECT_EQ(IntRect(0, 0, 100, 100),
            EnclosingIntRect(sticky->ComputeStickyConstrainingRect()));
}

// Verifies that the sticky constraints are correctly computed for sticky with
// writing mode.
TEST_F(LayoutBoxModelObjectTest, StickyPositionVerticalRLInlineConstraints) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      .scroller { writing-mode: vertical-rl; overflow: scroll; width: 100px;
          height: 100px; top: 100px; position: absolute; }
      .container { position: relative; top: 100px; height: 400px;
        width: 200px; }
      .sticky_box { width: 10px; height: 10px; top: 10px; position: sticky; }
      .inline { display: inline-block; }
      .spacer { width: 2000px; height: 2000px; }
    </style>
    <div class='scroller' id='scroller'>
      <div class='container'>
        <div class='inline sticky_box' id='sticky'></div>
      </div>
      <div class='spacer'></div>
    </div>
  )HTML");
  // Initial layout:
  // 0---------------2000----2200
  // -----spacer-----
  //                 container---
  //                 ----2100----
  //                     scroller
  //                     ----2190
  //                         sticky
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 50));
  EXPECT_EQ(50.f, scrollable_area->ScrollPosition().Y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");

  sticky->UpdateStickyPositionConstraints();

  EXPECT_EQ(scroller->Layer(), sticky->Layer()->AncestorScrollContainerLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollable_area->GetStickyConstraintsMap().at(sticky->Layer());

  EXPECT_EQ(10.f, constraints.top_offset);

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  EXPECT_EQ(IntRect(2000, 100, 200, 400),
            EnclosingIntRect(
                constraints.scroll_container_relative_containing_block_rect));
  EXPECT_EQ(
      IntRect(2190, 100, 10, 10),
      EnclosingIntRect(constraints.scroll_container_relative_sticky_box_rect));
  EXPECT_EQ(IntRect(0, 0, 100, 100),
            EnclosingIntRect(sticky->ComputeStickyConstrainingRect()));
}

// Verifies that the sticky constraints are not affected by transforms
TEST_F(LayoutBoxModelObjectTest, StickyPositionTransforms) {
  SetBodyInnerHTML(R"HTML(
    <style>#sticky { position: sticky; top: 0; width: 100px; height: 100px;
    transform: scale(2); transform-origin: top left; }
    #container { box-sizing: border-box; position: relative; top: 100px;
    height: 400px; width: 200px; padding: 10px; border: 5px solid black;
    transform: scale(2); transform-origin: top left; }
    #scroller { height: 100px; overflow: auto; position: relative; top:
    200px; }
    .spacer { height: 1000px; }</style>
    <div id='scroller'><div id='container'><div
    id='sticky'></div></div><div class='spacer'></div></div>
  )HTML");
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollOffsetInt().Width(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().Y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  sticky->UpdateStickyPositionConstraints();
  ASSERT_EQ(scroller->Layer(), sticky->Layer()->AncestorScrollContainerLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollable_area->GetStickyConstraintsMap().at(sticky->Layer());
  ASSERT_EQ(0.f, constraints.top_offset);

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(IntRect(15, 115, 170, 370),
            EnclosingIntRect(
                constraints.scroll_container_relative_containing_block_rect));
  ASSERT_EQ(
      IntRect(15, 115, 100, 100),
      EnclosingIntRect(constraints.scroll_container_relative_sticky_box_rect));
}

// Verifies that the sticky constraints are correctly computed.
TEST_F(LayoutBoxModelObjectTest, StickyPositionPercentageStyles) {
  SetBodyInnerHTML(R"HTML(
    <style>#sticky { position: sticky; margin-top: 10%; top: 0; width:
    100px; height: 100px; }
    #container { box-sizing: border-box; position: relative; top: 100px;
    height: 400px; width: 250px; padding: 5%; border: 5px solid black; }
    #scroller { width: 400px; height: 100px; overflow: auto; position:
    relative; top: 200px; }
    .spacer { height: 1000px; }</style>
    <div id='scroller'><div id='container'><div
    id='sticky'></div></div><div class='spacer'></div></div>
  )HTML");
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().Y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  sticky->UpdateStickyPositionConstraints();
  ASSERT_EQ(scroller->Layer(), sticky->Layer()->AncestorScrollContainerLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollable_area->GetStickyConstraintsMap().at(sticky->Layer());
  ASSERT_EQ(0.f, constraints.top_offset);

  ASSERT_EQ(IntRect(25, 145, 200, 330),
            EnclosingIntRect(
                constraints.scroll_container_relative_containing_block_rect));
  ASSERT_EQ(
      IntRect(25, 145, 100, 100),
      EnclosingIntRect(constraints.scroll_container_relative_sticky_box_rect));
}

// Verifies that the sticky constraints are correct when the sticky position
// container is also the ancestor scroller.
TEST_F(LayoutBoxModelObjectTest, StickyPositionContainerIsScroller) {
  SetBodyInnerHTML(R"HTML(
    <style>#sticky { position: sticky; top: 0; width: 100px; height: 100px;
    }
    #scroller { height: 100px; width: 400px; overflow: auto; position:
    relative; top: 200px; border: 2px solid black; }
    .spacer { height: 1000px; }</style>
    <div id='scroller'><div id='sticky'></div><div
    class='spacer'></div></div>
  )HTML");
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().Y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  sticky->UpdateStickyPositionConstraints();
  ASSERT_EQ(scroller->Layer(), sticky->Layer()->AncestorScrollContainerLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollable_area->GetStickyConstraintsMap().at(sticky->Layer());
  ASSERT_EQ(IntRect(0, 0, 400, 1100),
            EnclosingIntRect(
                constraints.scroll_container_relative_containing_block_rect));
  ASSERT_EQ(
      IntRect(0, 0, 100, 100),
      EnclosingIntRect(constraints.scroll_container_relative_sticky_box_rect));
}

// Verifies that the sticky constraints are correct when the sticky position
// object has an anonymous containing block.
TEST_F(LayoutBoxModelObjectTest, StickyPositionAnonymousContainer) {
  SetBodyInnerHTML(R"HTML(
    <style>#sticky { display: inline-block; position: sticky; top: 0;
    width: 100px; height: 100px; }
    #container { box-sizing: border-box; position: relative; top: 100px;
    height: 400px; width: 200px; padding: 10px; border: 5px solid black; }
    #scroller { height: 100px; overflow: auto; position: relative; top:
    200px; }
    .header { height: 50px; }
    .spacer { height: 1000px; }</style>
    <div id='scroller'><div id='container'><div class='header'></div><div
    id='sticky'></div></div><div class='spacer'></div></div>
  )HTML");
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().Y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  sticky->UpdateStickyPositionConstraints();
  ASSERT_EQ(scroller->Layer(), sticky->Layer()->AncestorScrollContainerLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollable_area->GetStickyConstraintsMap().at(sticky->Layer());
  ASSERT_EQ(IntRect(15, 115, 170, 370),
            EnclosingIntRect(
                constraints.scroll_container_relative_containing_block_rect));
  ASSERT_EQ(
      IntRect(15, 165, 100, 100),
      EnclosingIntRect(constraints.scroll_container_relative_sticky_box_rect));
}

TEST_F(LayoutBoxModelObjectTest, StickyPositionTableContainers) {
  SetBodyInnerHTML(R"HTML(
    <style> td, th { height: 50px; width: 50px; }
    #sticky { position: sticky; left: 0; will-change: transform; }
    table {border: none; }
    #scroller { overflow: auto; }
    </style>
    <div id='scroller'>
    <table cellspacing='0' cellpadding='0'>
        <thead><tr><td></td></tr></thead>
        <tr><td id='sticky'></td></tr>
    </table></div>
  )HTML");
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  sticky->UpdateStickyPositionConstraints();
  const StickyPositionScrollingConstraints& constraints =
      scrollable_area->GetStickyConstraintsMap().at(sticky->Layer());
  EXPECT_EQ(IntRect(0, 0, 50, 100),
            EnclosingIntRect(
                constraints.scroll_container_relative_containing_block_rect));
  EXPECT_EQ(
      IntRect(0, 50, 50, 50),
      EnclosingIntRect(constraints.scroll_container_relative_sticky_box_rect));
}

// Tests that when a non-layer changes size it invalidates the constraints for
// sticky position elements within the same scroller.
TEST_F(LayoutBoxModelObjectTest, StickyPositionConstraintInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #scroller { overflow: auto; display: flex; width: 200px; }
    #target { width: 50px; }
    #sticky { position: sticky; top: 0; }
    .container { width: 100px; margin-left: auto; margin-right: auto; }
    .hide { display: none; }
    </style>
    <div id='scroller'>
      <div style='flex: 1'>
        <div class='container'><div id='sticky'></div>
      </div>
    </div>
    <div class='spacer' id='target'></div>
    </div>
  )HTML");
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  auto* target = GetLayoutBoxModelObjectByElementId("target");
  EXPECT_TRUE(
      scrollable_area->GetStickyConstraintsMap().Contains(sticky->Layer()));
  EXPECT_EQ(25.f, scrollable_area->GetStickyConstraintsMap()
                      .at(sticky->Layer())
                      .scroll_container_relative_sticky_box_rect.X());
  To<HTMLElement>(target->GetNode())->classList().Add("hide");
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  // Layout should invalidate the sticky constraints of the sticky element and
  // mark it as needing a compositing inputs update.
  EXPECT_FALSE(
      scrollable_area->GetStickyConstraintsMap().Contains(sticky->Layer()));
  EXPECT_TRUE(sticky->Layer()->NeedsCompositingInputsUpdate());

  // After updating compositing inputs we should have the updated position.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(50.f, scrollable_area->GetStickyConstraintsMap()
                      .at(sticky->Layer())
                      .scroll_container_relative_sticky_box_rect.X());
}

// Verifies that the correct sticky-box shifting ancestor is found when
// computing the sticky constraints. Any such ancestor is the first sticky
// element between you and your containing block (exclusive).
//
// In most cases, this pointer should be null since your parent is normally your
// containing block. However there are cases where this is not true, including
// inline blocks and tables. The latter is currently irrelevant since only table
// cells can be sticky in CSS2.1, but we can test the former.
TEST_F(LayoutBoxModelObjectTest,
       StickyPositionFindsCorrectStickyBoxShiftingAncestor) {
  SetBodyInnerHTML(R"HTML(
    <style>#stickyOuterDiv { position: sticky; top: 0;}
    #stickyOuterInline { position: sticky; top: 0; display: inline; }
    #unanchoredSticky { position: sticky; display: inline; }
    .inline { display: inline; }
    #stickyInnerInline { position: sticky; top: 0; display: inline;
    }</style>
    <div id='stickyOuterDiv'>
      <div id='stickyOuterInline'>
       <div id='unanchoredSticky'>
          <div class='inline'>
            <div id='stickyInnerInline'></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  PaintLayer* sticky_outer_div = GetPaintLayerByElementId("stickyOuterDiv");
  PaintLayer* sticky_outer_inline =
      GetPaintLayerByElementId("stickyOuterInline");
  PaintLayer* unanchored_sticky = GetPaintLayerByElementId("unanchoredSticky");
  PaintLayer* sticky_inner_inline =
      GetPaintLayerByElementId("stickyInnerInline");

  PaintLayerScrollableArea* scrollable_area =
      sticky_outer_div->AncestorScrollContainerLayer()->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  StickyConstraintsMap constraints_map =
      scrollable_area->GetStickyConstraintsMap();

  ASSERT_TRUE(constraints_map.Contains(sticky_outer_div));
  ASSERT_TRUE(constraints_map.Contains(sticky_outer_inline));
  ASSERT_FALSE(constraints_map.Contains(unanchored_sticky));
  ASSERT_TRUE(constraints_map.Contains(sticky_inner_inline));

  // The outer block element trivially has no sticky-box shifting ancestor.
  EXPECT_FALSE(constraints_map.at(sticky_outer_div)
                   .nearest_sticky_layer_shifting_sticky_box);

  // Neither does the outer inline element, as its parent element is also its
  // containing block.
  EXPECT_FALSE(constraints_map.at(sticky_outer_inline)
                   .nearest_sticky_layer_shifting_sticky_box);

  // However the inner inline element does have a sticky-box shifting ancestor,
  // as its containing block is the ancestor block element, above its ancestor
  // sticky element.
  EXPECT_EQ(sticky_outer_inline, constraints_map.at(sticky_inner_inline)
                                     .nearest_sticky_layer_shifting_sticky_box);
}

// Verifies that the correct containing-block shifting ancestor is found when
// computing the sticky constraints. Any such ancestor is the first sticky
// element between your containing block (inclusive) and your ancestor overflow
// layer (exclusive).
TEST_F(LayoutBoxModelObjectTest,
       StickyPositionFindsCorrectContainingBlockShiftingAncestor) {
  // We make the scroller itself sticky in order to check that elements do not
  // detect it as their containing-block shifting ancestor.
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { overflow-y: scroll; position: sticky; top: 0;}
    #stickyParent { position: sticky; top: 0;}
    #stickyChild { position: sticky; top: 0;}
    #unanchoredSticky { position: sticky; }
    #stickyNestedChild { position: sticky; top: 0;}</style>
    <div id='scroller'>
      <div id='stickyParent'>
        <div id='unanchoredSticky'>
          <div id='stickyChild'></div>
          <div><div id='stickyNestedChild'></div></div>
        </div>
      </div>
    </div>
  )HTML");

  PaintLayer* scroller = GetPaintLayerByElementId("scroller");
  PaintLayer* sticky_parent = GetPaintLayerByElementId("stickyParent");
  PaintLayer* sticky_child = GetPaintLayerByElementId("stickyChild");
  PaintLayer* sticky_nested_child =
      GetPaintLayerByElementId("stickyNestedChild");

  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  StickyConstraintsMap constraints_map =
      scrollable_area->GetStickyConstraintsMap();

  ASSERT_FALSE(constraints_map.Contains(scroller));
  ASSERT_TRUE(constraints_map.Contains(sticky_parent));
  ASSERT_TRUE(constraints_map.Contains(sticky_child));
  ASSERT_TRUE(constraints_map.Contains(sticky_nested_child));

  // The outer <div> should not detect the scroller as its containing-block
  // shifting ancestor.
  EXPECT_FALSE(constraints_map.at(sticky_parent)
                   .nearest_sticky_layer_shifting_containing_block);

  // Both inner children should detect the parent <div> as their
  // containing-block shifting ancestor. They skip past unanchored sticky
  // because it will never have a non-zero offset.
  EXPECT_EQ(sticky_parent, constraints_map.at(sticky_child)
                               .nearest_sticky_layer_shifting_containing_block);
  EXPECT_EQ(sticky_parent, constraints_map.at(sticky_nested_child)
                               .nearest_sticky_layer_shifting_containing_block);
}

// Verifies that the correct containing-block shifting ancestor is found when
// computing the sticky constraints, in the case where the overflow ancestor is
// the page itself. This is a special-case version of the test above, as we
// often treat the root page as special when it comes to scroll logic. It should
// not make a difference for containing-block shifting ancestor calculations.
TEST_F(LayoutBoxModelObjectTest,
       StickyPositionFindsCorrectContainingBlockShiftingAncestorRoot) {
  SetBodyInnerHTML(R"HTML(
    <style>#stickyParent { position: sticky; top: 0;}
    #stickyGrandchild { position: sticky; top: 0;}</style>
    <div id='stickyParent'><div><div id='stickyGrandchild'></div></div>
    </div>
  )HTML");

  PaintLayer* sticky_parent = GetPaintLayerByElementId("stickyParent");
  PaintLayer* sticky_grandchild = GetPaintLayerByElementId("stickyGrandchild");

  PaintLayerScrollableArea* scrollable_area =
      sticky_parent->AncestorScrollContainerLayer()->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  StickyConstraintsMap constraints_map =
      scrollable_area->GetStickyConstraintsMap();

  ASSERT_TRUE(constraints_map.Contains(sticky_parent));
  ASSERT_TRUE(constraints_map.Contains(sticky_grandchild));

  // The grandchild sticky should detect the parent as its containing-block
  // shifting ancestor.
  EXPECT_EQ(sticky_parent, constraints_map.at(sticky_grandchild)
                               .nearest_sticky_layer_shifting_containing_block);
}

// Verifies that the correct containing-block shifting ancestor is found when
// computing the sticky constraints, in the case of tables. Tables are unusual
// because the containing block for all table elements is the <table> itself, so
// we have to skip over elements to find the correct ancestor.
TEST_F(LayoutBoxModelObjectTest,
       StickyPositionFindsCorrectContainingBlockShiftingAncestorTable) {
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { overflow-y: scroll; }
    #stickyOuter { position: sticky; top: 0;}
    #stickyTh { position: sticky; top: 0;}</style>
    <div id='scroller'><div id='stickyOuter'><table><thead><tr>
    <th id='stickyTh'></th></tr></thead></table></div></div>
  )HTML");

  PaintLayer* scroller = GetPaintLayerByElementId("scroller");
  PaintLayer* sticky_outer = GetPaintLayerByElementId("stickyOuter");
  PaintLayer* sticky_th = GetPaintLayerByElementId("stickyTh");

  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  StickyConstraintsMap constraints_map =
      scrollable_area->GetStickyConstraintsMap();

  ASSERT_FALSE(constraints_map.Contains(scroller));
  ASSERT_TRUE(constraints_map.Contains(sticky_outer));
  ASSERT_TRUE(constraints_map.Contains(sticky_th));

  // The table cell should detect the outer <div> as its containing-block
  // shifting ancestor.
  EXPECT_EQ(sticky_outer, constraints_map.at(sticky_th)
                              .nearest_sticky_layer_shifting_containing_block);
}

// Verifies that the calculated position:sticky offsets are correct when we have
// a simple case of nested sticky elements.
TEST_F(LayoutBoxModelObjectTest, StickyPositionNested) {
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { height: 100px; width: 100px; overflow-y: auto; }
    #prePadding { height: 50px }
    #stickyParent { position: sticky; top: 0; height: 50px; }
    #stickyChild { position: sticky; top: 0; height: 25px; }
    #postPadding { height: 200px }</style>
    <div id='scroller'><div id='prePadding'></div><div id='stickyParent'>
    <div id='stickyChild'></div></div><div id='postPadding'></div></div>
  )HTML");

  auto* sticky_parent = GetLayoutBoxModelObjectByElementId("stickyParent");
  auto* sticky_child = GetLayoutBoxModelObjectByElementId("stickyChild");

  // Scroll the page down.
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().Y());

  // Both the parent and child sticky divs are attempting to place themselves at
  // the top of the scrollable area. To achieve this the parent must offset on
  // the y-axis against its starting position. The child is offset relative to
  // its parent so should not move at all.
  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), sticky_child->StickyPositionOffset());

  sticky_parent->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());

  sticky_child->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), sticky_child->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct when the
// child has a larger edge constraint value than the parent.
TEST_F(LayoutBoxModelObjectTest, StickyPositionChildHasLargerTop) {
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { height: 100px; width: 100px; overflow-y: auto; }
    #prePadding { height: 50px }
    #stickyParent { position: sticky; top: 0; height: 50px; }
    #stickyChild { position: sticky; top: 25px; height: 25px; }
    #postPadding { height: 200px }</style>
    <div id='scroller'><div id='prePadding'></div><div id='stickyParent'>
    <div id='stickyChild'></div></div><div id='postPadding'></div></div>
  )HTML");

  auto* sticky_parent = GetLayoutBoxModelObjectByElementId("stickyParent");
  auto* sticky_child = GetLayoutBoxModelObjectByElementId("stickyChild");

  // Scroll the page down.
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().Y());

  // The parent is attempting to place itself at the top of the scrollable area,
  // whilst the child is attempting to be 25 pixels from the top. To achieve
  // this both must offset on the y-axis against their starting positions, but
  // note the child is offset relative to the parent.
  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), sticky_child->StickyPositionOffset());

  sticky_parent->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());

  sticky_child->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), sticky_child->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct when the
// child has a smaller edge constraint value than the parent.
TEST_F(LayoutBoxModelObjectTest, StickyPositionParentHasLargerTop) {
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { height: 100px; width: 100px; overflow-y: auto; }
    #prePadding { height: 50px }
    #stickyParent { position: sticky; top: 25px; height: 50px; }
    #stickyChild { position: sticky; top: 0; height: 25px; }
    #postPadding { height: 200px }</style>
    <div id='scroller'><div id='prePadding'></div><div id='stickyParent'>
    <div id='stickyChild'></div></div><div id='postPadding'></div></div>
  )HTML");

  auto* sticky_parent = GetLayoutBoxModelObjectByElementId("stickyParent");
  auto* sticky_child = GetLayoutBoxModelObjectByElementId("stickyChild");

  // Scroll the page down.
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().Y());

  // The parent is attempting to place itself 25 pixels from the top of the
  // scrollable area, whilst the child is attempting to be at the top. However,
  // the child must stay contained within the parent, so it should be pushed
  // down to the same height. As always, the child offset is relative.
  EXPECT_EQ(PhysicalOffset(0, 75), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), sticky_child->StickyPositionOffset());

  sticky_parent->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 75), sticky_parent->StickyPositionOffset());

  sticky_child->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 75), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), sticky_child->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct when the
// child has a large enough edge constraint value to push outside of its parent.
TEST_F(LayoutBoxModelObjectTest, StickyPositionChildPushingOutsideParent) {
  SetBodyInnerHTML(R"HTML(
    <style> #scroller { height: 100px; width: 100px; overflow-y: auto; }
    #prePadding { height: 50px; }
    #stickyParent { position: sticky; top: 0; height: 50px; }
    #stickyChild { position: sticky; top: 50px; height: 25px; }
    #postPadding { height: 200px }</style>
    <div id='scroller'><div id='prePadding'></div><div id='stickyParent'>
    <div id='stickyChild'></div></div><div id='postPadding'></div></div>
  )HTML");

  auto* sticky_parent = GetLayoutBoxModelObjectByElementId("stickyParent");
  auto* sticky_child = GetLayoutBoxModelObjectByElementId("stickyChild");

  // Scroll the page down.
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().Y());

  // The parent is attempting to place itself at the top of the scrollable area,
  // whilst the child is attempting to be 50 pixels from the top. However, there
  // is only 25 pixels of space for the child to move into, so it should be
  // capped by that offset. As always, the child offset is relative.
  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), sticky_child->StickyPositionOffset());

  sticky_parent->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());

  sticky_child->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), sticky_child->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of triple nesting. Triple (or more) nesting must be tested as the grandchild
// sticky must correct both its sticky box constraint rect and its containing
// block constaint rect.
TEST_F(LayoutBoxModelObjectTest, StickyPositionTripleNestedDiv) {
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { height: 200px; width: 100px; overflow-y: auto; }
    #prePadding { height: 50px; }
    #outmostSticky { position: sticky; top: 0; height: 100px; }
    #middleSticky { position: sticky; top: 0; height: 75px; }
    #innerSticky { position: sticky; top: 25px; height: 25px; }
    #postPadding { height: 400px }</style>
    <div id='scroller'><div id='prePadding'></div><div id='outmostSticky'>
    <div id='middleSticky'><div id='innerSticky'></div></div></div>
    <div id='postPadding'></div></div>
  )HTML");

  auto* outmost_sticky = GetLayoutBoxModelObjectByElementId("outmostSticky");
  auto* middle_sticky = GetLayoutBoxModelObjectByElementId("middleSticky");
  auto* inner_sticky = GetLayoutBoxModelObjectByElementId("innerSticky");

  // Scroll the page down.
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().Y());

  // The grandparent and parent divs are attempting to place themselves at the
  // top of the scrollable area. The child div is attempting to place itself at
  // an offset of 25 pixels to the top of the scrollable area. The result of
  // this sticky offset calculation is quite simple, but internally the child
  // offset has to offset both its sticky box constraint rect and its containing
  // block constraint rect.
  EXPECT_EQ(PhysicalOffset(0, 50), outmost_sticky->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), middle_sticky->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), inner_sticky->StickyPositionOffset());

  outmost_sticky->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 50), outmost_sticky->StickyPositionOffset());

  middle_sticky->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 50), outmost_sticky->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), middle_sticky->StickyPositionOffset());

  inner_sticky->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 50), outmost_sticky->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), middle_sticky->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), inner_sticky->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of tables. Tables are special as the containing block for table elements is
// always the root level <table>.
TEST_F(LayoutBoxModelObjectTest, StickyPositionNestedStickyTable) {
  SetBodyInnerHTML(R"HTML(
    <style>table { border-collapse: collapse; }
    td, th { height: 25px; width: 25px; padding: 0; }
    #scroller { height: 100px; width: 100px; overflow-y: auto; }
    #prePadding { height: 50px; }
    #stickyDiv { position: sticky; top: 0; height: 200px; }
    #stickyTh { position: sticky; top: 0; }
    #postPadding { height: 200px; }</style>
    <div id='scroller'><div id='prePadding'></div><div id='stickyDiv'>
    <table><thead><tr><th id='stickyTh'></th></tr></thead><tbody><tr><td>
    </td></tr><tr><td></td></tr><tr><td></td></tr><tr><td></td></tr></tbody>
    </table></div><div id='postPadding'></div></div>
  )HTML");

  auto* sticky_div = GetLayoutBoxModelObjectByElementId("stickyDiv");
  auto* sticky_th = GetLayoutBoxModelObjectByElementId("stickyTh");

  // Scroll the page down.
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 150));
  ASSERT_EQ(150.0, scrollable_area->ScrollPosition().Y());

  // All sticky elements are attempting to stick to the top of the scrollable
  // area. For the root sticky div, this requires an offset. All the other
  // descendant sticky elements are positioned relatively so don't need offset.
  EXPECT_EQ(PhysicalOffset(0, 100), sticky_div->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), sticky_th->StickyPositionOffset());

  // If we now scroll to the point where the overall sticky div starts to move,
  // the table headers should continue to stick to the top of the scrollable
  // area until they run out of <table> space to move in.

  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 275));
  ASSERT_EQ(275.0, scrollable_area->ScrollPosition().Y());

  EXPECT_EQ(PhysicalOffset(0, 200), sticky_div->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), sticky_th->StickyPositionOffset());

  // Finally, if we scroll so that the table is off the top of the page, the
  // sticky header should travel as far as it can (i.e. the table height) then
  // move off the top with it.
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 350));
  ASSERT_EQ(350.0, scrollable_area->ScrollPosition().Y());

  EXPECT_EQ(PhysicalOffset(0, 200), sticky_div->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 100), sticky_th->StickyPositionOffset());

  sticky_div->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 200), sticky_div->StickyPositionOffset());

  sticky_th->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 200), sticky_div->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 100), sticky_th->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// where a particular position:sticky element is used both as a sticky-box
// shifting ancestor as well as a containing-block shifting ancestor.
//
// This is a rare case that can be replicated by nesting tables so that a sticky
// cell contains another table that has sticky elements. See the HTML below.
TEST_F(LayoutBoxModelObjectTest, StickyPositionComplexTableNesting) {
  SetBodyInnerHTML(R"HTML(
    <style>table { border-collapse: collapse; }
    td, th { height: 25px; width: 25px; padding: 0; }
    #scroller { height: 100px; width: 100px; overflow-y: auto; }
    #prePadding { height: 50px; }
    #outerStickyTh { height: 50px; position: sticky; top: 0; }
    #innerStickyTh { position: sticky; top: 25px; }
    #postPadding { height: 200px; }</style>
    <div id='scroller'><div id='prePadding'></div>
    <table><thead><tr><th id='outerStickyTh'><table><thead><tr>
    <th id='innerStickyTh'></th></tr></thead><tbody><tr><td></td></tr>
    </tbody></table></th></tr></thead><tbody><tr><td></td></tr><tr><td></td>
    </tr><tr><td></td></tr><tr><td></td></tr></tbody></table>
    <div id='postPadding'></div></div>
  )HTML");

  auto* outer_sticky_th = GetLayoutBoxModelObjectByElementId("outerStickyTh");
  auto* inner_sticky_th = GetLayoutBoxModelObjectByElementId("innerStickyTh");

  // Scroll the page down.
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 150));
  ASSERT_EQ(150.0, scrollable_area->ScrollPosition().Y());

  EXPECT_EQ(PhysicalOffset(0, 100), outer_sticky_th->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), inner_sticky_th->StickyPositionOffset());

  outer_sticky_th->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 100), outer_sticky_th->StickyPositionOffset());

  inner_sticky_th->UpdateStickyPositionConstraints();

  EXPECT_EQ(PhysicalOffset(0, 100), outer_sticky_th->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), inner_sticky_th->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of nested inline elements.
TEST_F(LayoutBoxModelObjectTest, StickyPositionNestedInlineElements) {
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { width: 100px; height: 100px; overflow-y: scroll; }
    #paddingBefore { height: 50px; }
    #outerInline { display: inline; position: sticky; top: 0; }
    #unanchoredSticky { position: sticky; display: inline; }
    .inline {display: inline;}
    #innerInline { display: inline; position: sticky; top: 25px; }
    #paddingAfter { height: 200px; }</style>
    <div id='scroller'>
      <div id='paddingBefore'></div>
      <div id='outerInline'>
        <div id='unanchoredSticky'>
          <div class='inline'>
            <div id='innerInline'></div>
          </div>
        </div>
      </div>
      <div id='paddingAfter'></div>
    </div>
  )HTML");

  auto* outer_inline = GetLayoutBoxModelObjectByElementId("outerInline");
  auto* inner_inline = GetLayoutBoxModelObjectByElementId("innerInline");

  // Scroll the page down.
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().Y());

  EXPECT_EQ(PhysicalOffset(0, 0), outer_inline->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), inner_inline->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of an intermediate position:fixed element.
TEST_F(LayoutBoxModelObjectTest, StickyPositionNestedFixedPos) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }
    #scroller { height: 200px; width: 100px; overflow-y: auto; }
    #outerSticky { position: sticky; top: 0; height: 50px; }
    #fixedDiv { position: fixed; top: 0; left: 300px; height: 100px;
    width: 100px; }
    #innerSticky { position: sticky; top: 25px; height: 25px; }
    #padding { height: 400px }</style>
    <div id='scroller'><div id='outerSticky'><div id='fixedDiv'>
    <div id='innerSticky'></div></div></div><div id='padding'></div></div>
  )HTML");

  auto* outer_sticky = GetLayoutBoxModelObjectByElementId("outerSticky");
  auto* inner_sticky = GetLayoutBoxModelObjectByElementId("innerSticky");

  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();

  StickyConstraintsMap constraints_map =
      scrollable_area->GetStickyConstraintsMap();
  ASSERT_TRUE(constraints_map.Contains(outer_sticky->Layer()));
  ASSERT_TRUE(constraints_map.Contains(inner_sticky->Layer()));

  // The inner sticky should not detect the outer one as any sort of ancestor.
  EXPECT_FALSE(constraints_map.at(inner_sticky->Layer())
                   .nearest_sticky_layer_shifting_sticky_box);
  EXPECT_FALSE(constraints_map.at(inner_sticky->Layer())
                   .nearest_sticky_layer_shifting_containing_block);

  // Scroll the page down.
  scrollable_area->ScrollToAbsolutePosition(
      FloatPoint(scrollable_area->ScrollPosition().X(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().Y());

  // TODO(smcgruer): Until http://crbug.com/686164 is fixed, the sticky position
  // offset of the inner sticky stays 75 instead of 25.
  // the constraints here before calculations will be correct.
  EXPECT_EQ(PhysicalOffset(0, 100), outer_sticky->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 75), inner_sticky->StickyPositionOffset());
}

TEST_F(LayoutBoxModelObjectTest, InvalidatePaintLayerOnStackedChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .stacked { background: red; position: relative; height: 2000px; }
      .non-stacked { all: inherit }
    </style>
    <div style='height: 100px; backface-visibility: hidden'>
      <div id='target' class='stacked'></div>
    </div>
  )HTML");

  auto* target_element = GetDocument().getElementById("target");
  auto* target = target_element->GetLayoutBoxModelObject();
  auto* parent = target->Parent();
  auto* original_compositing_container =
      target->Layer()->CompositingContainer();
  EXPECT_FALSE(target->IsStackingContext());
  EXPECT_TRUE(target->IsStacked());
  EXPECT_FALSE(parent->IsStacked());
  EXPECT_NE(parent, original_compositing_container->GetLayoutObject());

  target_element->setAttribute(html_names::kClassAttr, "non-stacked");
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  EXPECT_FALSE(target->IsStacked());
  EXPECT_TRUE(target->Layer()->SelfNeedsRepaint());
  EXPECT_TRUE(original_compositing_container->DescendantNeedsRepaint());
  auto* new_compositing_container = target->Layer()->CompositingContainer();
  EXPECT_EQ(parent, new_compositing_container->GetLayoutObject());

  UpdateAllLifecyclePhasesForTest();
  target_element->setAttribute(html_names::kClassAttr, "stacked");
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  EXPECT_TRUE(target->IsStacked());
  EXPECT_TRUE(target->Layer()->SelfNeedsRepaint());
  EXPECT_TRUE(new_compositing_container->DescendantNeedsRepaint());
  EXPECT_EQ(original_compositing_container,
            target->Layer()->CompositingContainer());
}

// Tests that when a sticky object is removed from the root scroller it
// correctly clears its viewport constrained position: https://crbug.com/755307.
TEST_F(LayoutBoxModelObjectTest, StickyRemovedFromRootScrollableArea) {
  SetBodyInnerHTML(R"HTML(
    <style>
    body { height: 5000px; }
    #scroller { height: 100px; }
    #sticky { position: sticky; top: 0; height: 50px; width: 50px; }
    </style>
    <div id='scroller'>
      <div id='sticky'></div>
      </div>
  )HTML");

  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");

  // The 'scroller' starts as as a non-scroll container, so the sticky
  // element's ancestor overflow layer should be the outer scroller.
  EXPECT_TRUE(sticky->Layer()->AncestorScrollContainerLayer()->IsRootLayer());

  // We need the sticky element to not be a PaintLayer child of the scroller,
  // so that it is later reparented under the scroller's PaintLayer
  EXPECT_FALSE(scroller->Layer());

  // Now make the scroller into an actual scroller. This will reparent the
  // sticky element to be a child of the scroller, and will set its previous
  // overflow layer to nullptr.
  To<Element>(scroller->GetNode())
      ->SetInlineStyleProperty(CSSPropertyID::kOverflow, "scroll");
  UpdateAllLifecyclePhasesForTest();

  // The sticky element should no longer be viewport constrained.
  EXPECT_FALSE(GetDocument().View()->HasViewportConstrainedObjects());

  // Making the scroller have visible overflow but still have a PaintLayer
  // (in this case by making it position: relative) will cause us to need to
  // recompute the sticky element's ancestor overflow layer.
  To<Element>(scroller->GetNode())
      ->SetInlineStyleProperty(CSSPropertyID::kPosition, "relative");
  To<Element>(scroller->GetNode())
      ->SetInlineStyleProperty(CSSPropertyID::kOverflow, "visible");

  // Now try to scroll to the sticky element, this used to crash.
  GetDocument().GetFrame()->DomWindow()->scrollTo(0, 500);
}

TEST_F(LayoutBoxModelObjectTest, BackfaceVisibilityChange) {
  AtomicString base_style =
      "width: 100px; height: 100px; background: blue; position: absolute";
  SetBodyInnerHTML("<div id='target' style='" + base_style + "'></div>");

  auto* target = GetDocument().getElementById("target");
  auto* target_layer =
      To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer();
  ASSERT_NE(nullptr, target_layer);
  EXPECT_FALSE(target_layer->SelfNeedsRepaint());

  target->setAttribute(html_names::kStyleAttr,
                       base_style + "; backface-visibility: hidden");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(target_layer->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target_layer->SelfNeedsRepaint());

  target->setAttribute(html_names::kStyleAttr, base_style);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(target_layer->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target_layer->SelfNeedsRepaint());
}

TEST_F(LayoutBoxModelObjectTest, ChangingFilterWithWillChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        width: 100px;
        height: 100px;
        will-change: filter;
      }
    </style>
    <div id="target"></div>
  )HTML");

  // Adding a filter should not need to check for paint invalidation because
  // will-change: filter is present.
  auto* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kStyleAttr, "filter: grayscale(1)");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());

  // Removing a filter should not need to check for paint invalidation because
  // will-change: filter is present.
  target->removeAttribute(html_names::kStyleAttr);
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
}

TEST_F(LayoutBoxModelObjectTest, ChangingWillChangeFilter) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .willChange {
        will-change: filter;
      }
      #filter {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id="target"></div>
  )HTML");

  // Adding will-change: filter should check for paint invalidation and create
  // a PaintLayer.
  auto* target = GetDocument().getElementById("target");
  target->classList().Add("willChange");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());

  // A lifecycle update should clear dirty bits.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());

  // Removing will-change: filter should check for paint invalidation and remove
  // the PaintLayer.
  target->classList().Remove("willChange");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());
}

TEST_F(LayoutBoxModelObjectTest, ChangingBackdropFilterWithWillChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        width: 100px;
        height: 100px;
        will-change: backdrop-filter;
      }
    </style>
    <div id="target"></div>
  )HTML");

  // Adding a backdrop-filter should not need to check for paint invalidation
  // because will-change: backdrop-filter is present.
  auto* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kStyleAttr, "backdrop-filter: grayscale(1)");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());

  // Removing a backdrop-filter should not need to check for paint invalidation
  // because will-change: backdrop-filter is present.
  target->removeAttribute(html_names::kStyleAttr);
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
}

TEST_F(LayoutBoxModelObjectTest, ChangingWillChangeBackdropFilter) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .willChange {
        will-change: backdrop-filter;
      }
      #filter {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id="target"></div>
  )HTML");

  // Adding will-change: backdrop-filter should check for paint invalidation and
  // create a PaintLayer.
  auto* target = GetDocument().getElementById("target");
  target->classList().Add("willChange");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());

  // A lifecycle update should clear dirty bits.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());

  // Removing will-change: backdrop-filter should check for paint invalidation
  // and remove the PaintLayer.
  target->classList().Remove("willChange");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());
}

TEST_F(LayoutBoxModelObjectTest, UpdateStackingContextForOption) {
  // We do not create LayoutObject for option elements inside multiple selects
  // on platforms where DelegatesMenuListRendering() returns true like Android.
  if (LayoutTheme::GetTheme().DelegatesMenuListRendering())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes op {
        0% { opacity: 0 }
        100% { opacity: 1 }
      }
      option {
        animation: op 0.001s;
      }
    </style>
    <select multiple size=1>
      <option id=opt>PASS</option>
    </select>
  )HTML");

  auto* option_element = GetDocument().getElementById("opt");
  auto* option_layout = option_element->GetLayoutObject();
  ASSERT_TRUE(option_layout);
  EXPECT_TRUE(option_layout->IsStackingContext());
  EXPECT_TRUE(option_layout->StyleRef().HasCurrentOpacityAnimation());
}

// Tests that contain: layout changes cause compositing inputs update.
TEST_F(LayoutBoxModelObjectTest,
       LayoutContainmentChangeCausesCompositingInputsUpdate) {
  SetBodyInnerHTML(R"HTML(
    <style>
    /* ensure we retain the paint layer after removing .contained class. */
    div { position: relative; }
    .contained { contain: layout; }
    </style>
    <div id=target class=contained></div>
    <div id=unrelated class=contained></div>
  )HTML");
  auto* target = GetLayoutBoxModelObjectByElementId("target");
  ASSERT_TRUE(target->Layer());

  EXPECT_FALSE(target->Layer()->NeedsCompositingInputsUpdate());
  EXPECT_EQ(target->Layer(), target->Layer()->NearestContainedLayoutLayer());

  To<HTMLElement>(target->GetNode())->classList().Remove("contained");
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  EXPECT_TRUE(target->Layer()->NeedsCompositingInputsUpdate());

  // After updating compositing inputs we should have no contained layer
  // ancestors.
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(target->Layer());
  EXPECT_FALSE(target->Layer()->NeedsCompositingInputsUpdate());
  EXPECT_EQ(nullptr, target->Layer()->NearestContainedLayoutLayer());
}
}  // namespace blink
