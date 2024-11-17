// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

class LayoutBoxModelObjectTest : public RenderingTest,
                                 public PaintTestConfigurations {
 protected:
  LayoutBoxModelObject* GetLayoutBoxModelObjectByElementId(const char* id) {
    return To<LayoutBoxModelObject>(GetLayoutObjectByElementId(id));
  }

  bool HasStickyLayer(const PaintLayerScrollableArea* scrollable_area,
                      const LayoutBoxModelObject* sticky) {
    for (const auto& fragment :
         scrollable_area->GetLayoutBox()->PhysicalFragments()) {
      if (auto* sticky_descendants = fragment.StickyDescendants()) {
        if (sticky_descendants->Contains(sticky)) {
          return true;
        }
      }
    }
    return false;
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(LayoutBoxModelObjectTest);

// This test doesn't need to be a parameterized test.
TEST_P(LayoutBoxModelObjectTest, LocalCaretRectForEmptyElementVertical) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body {
      font: 10px Ahem;
    }
    .target {
      padding: 1px 3px 5px 7px;
      block-size: 40px;
      inline-size: 33px;
    }
    #target-rl {
      writing-mode: vertical-rl;
    }
    #target-lr {
      writing-mode: vertical-lr;
    }
    </style>
    <div id='target-rl' class="target"></div>
    <div id='target-lr' class="target"></div>

    <div style="writing-mode:vertical-rl;">
    <br>
    <span id="target-inline-rl" class="target"></span>
    </div>

    <div style="writing-mode:vertical-lr;">
    <br>
    <span id="target-inline-lr" class="target"></span>
    </div>
  })HTML");

  constexpr LayoutUnit kPaddingTop = LayoutUnit(1);
  constexpr LayoutUnit kPaddingRight = LayoutUnit(3);
  constexpr LayoutUnit kPaddingLeft = LayoutUnit(7);
  constexpr LayoutUnit kFontHeight = LayoutUnit(10);
  constexpr LayoutUnit kCaretWidth = LayoutUnit(1);

  {
    auto* rl = GetLayoutBoxByElementId("target-rl");
    EXPECT_EQ(PhysicalRect(rl->Size().width - kPaddingRight - kFontHeight,
                           kPaddingTop, kFontHeight, kCaretWidth),
              rl->LocalCaretRect(0));
  }
  {
    auto* lr = GetLayoutBoxByElementId("target-lr");
    EXPECT_EQ(PhysicalRect(kPaddingLeft, kPaddingTop, kFontHeight, kCaretWidth),
              lr->LocalCaretRect(0));
  }
  {
    auto* inline_rl =
        To<LayoutInline>(GetLayoutObjectByElementId("target-inline-rl"));
    EXPECT_EQ(PhysicalRect(LayoutUnit(), kPaddingTop - kCaretWidth, kFontHeight,
                           kCaretWidth),
              inline_rl->LocalCaretRect(0));
  }
  {
    auto* inline_lr =
        To<LayoutInline>(GetLayoutObjectByElementId("target-inline-lr"));
    EXPECT_EQ(PhysicalRect(kFontHeight, kPaddingTop - kCaretWidth, kFontHeight,
                           kCaretWidth),
              inline_lr->LocalCaretRect(0));
  }
}

TEST_P(LayoutBoxModelObjectTest, BorderAndPaddingLogicalLeftRight) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .target {
      border-color: red;
      border-style: solid;
      border-width: 2px 4px 11px 13px;
      padding: 1px 3px 5px 7px;
      block-size: 40px;
      inline-size: 33px;
    }
    #target-htb {
      writing-mode: horizontal-tb;
    }
    #target-vrl {
      writing-mode: vertical-rl;
    }
    #target-vlr {
      writing-mode: vertical-lr;
    }
    #target-srl {
      writing-mode: sideways-rl;
    }
    #target-slr {
      writing-mode: sideways-lr;
    }
    </style>
    <div id='target-htb' class="target"></div>
    <div id='target-vrl' class="target"></div>
    <div id='target-vlr' class="target"></div>
    <div id='target-srl' class="target"></div>
    <div id='target-slr' class="target"></div>
  })HTML");

  constexpr LayoutUnit kTop = LayoutUnit(2 + 1);
  constexpr LayoutUnit kRight = LayoutUnit(4 + 3);
  constexpr LayoutUnit kBottom = LayoutUnit(11 + 5);
  constexpr LayoutUnit kLeft = LayoutUnit(13 + 7);

  {
    auto* target = GetLayoutBoxByElementId("target-htb");
    EXPECT_EQ(kLeft, target->BorderAndPaddingInlineStart());
    EXPECT_EQ(kRight, target->BorderAndPaddingInlineEnd());
  }
  {
    auto* target = GetLayoutBoxByElementId("target-vrl");
    EXPECT_EQ(kTop, target->BorderAndPaddingInlineStart());
    EXPECT_EQ(kBottom, target->BorderAndPaddingInlineEnd());
  }
  {
    auto* target = GetLayoutBoxByElementId("target-vlr");
    EXPECT_EQ(kTop, target->BorderAndPaddingInlineStart());
    EXPECT_EQ(kBottom, target->BorderAndPaddingInlineEnd());
  }
  {
    auto* target = GetLayoutBoxByElementId("target-srl");
    EXPECT_EQ(kTop, target->BorderAndPaddingInlineStart());
    EXPECT_EQ(kBottom, target->BorderAndPaddingInlineEnd());
  }
  {
    auto* target = GetLayoutBoxByElementId("target-slr");
    EXPECT_EQ(kBottom, target->BorderAndPaddingInlineStart());
    EXPECT_EQ(kTop, target->BorderAndPaddingInlineEnd());
  }
}

// Verifies that the sticky constraints are correctly computed.
TEST_P(LayoutBoxModelObjectTest, StickyPositionConstraints) {
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
      gfx::PointF(scrollable_area->ScrollOffsetInt().x(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  ASSERT_EQ(scroller->Layer(),
            sticky->Layer()->ContainingScrollContainerLayer());

  const auto* constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky));
  ASSERT_EQ(0.f, constraints->top_inset->ToFloat());

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(gfx::Rect(15, 115, 170, 370),
            ToEnclosingRect(
                constraints->scroll_container_relative_containing_block_rect));
  ASSERT_EQ(
      gfx::Rect(15, 115, 100, 100),
      ToEnclosingRect(constraints->scroll_container_relative_sticky_box_rect));

  // The sticky constraining rect also doesn't include the border offset.
  ASSERT_EQ(gfx::Rect(0, 0, 400, 100),
            ToEnclosingRect(constraints->constraining_rect));
}

// Verifies that the sticky constraints are correctly computed in right to left.
TEST_P(LayoutBoxModelObjectTest, StickyPositionVerticalRLConstraints) {
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
      gfx::PointF(scrollable_area->ScrollOffsetInt().x(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  ASSERT_EQ(scroller->Layer(),
            sticky->Layer()->ContainingScrollContainerLayer());

  const auto* constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky));

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(gfx::Rect(215, 115, 170, 370),
            ToEnclosingRect(
                constraints->scroll_container_relative_containing_block_rect));
  ASSERT_EQ(
      gfx::Rect(285, 115, 100, 100),
      ToEnclosingRect(constraints->scroll_container_relative_sticky_box_rect));

  // The sticky constraining rect also doesn't include the border offset.
  ASSERT_EQ(gfx::Rect(0, 0, 400, 100),
            ToEnclosingRect(constraints->constraining_rect));
}

// Verifies that the sticky constraints are correctly computed for inline.
TEST_P(LayoutBoxModelObjectTest, StickyPositionInlineConstraints) {
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
      gfx::PointF(scrollable_area->ScrollOffsetInt().x(), 50));
  EXPECT_EQ(50.f, scrollable_area->ScrollPosition().y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");


  EXPECT_EQ(scroller->Layer(),
            sticky->Layer()->ContainingScrollContainerLayer());

  const auto* constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky));
  EXPECT_EQ(10.f, constraints->top_inset->ToFloat());

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  EXPECT_EQ(gfx::Rect(0, 100, 200, 400),
            ToEnclosingRect(
                constraints->scroll_container_relative_containing_block_rect));
  EXPECT_EQ(
      gfx::Rect(0, 100, 10, 10),
      ToEnclosingRect(constraints->scroll_container_relative_sticky_box_rect));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            ToEnclosingRect(constraints->constraining_rect));
}

// Verifies that the sticky constraints are correctly computed for sticky with
// writing mode.
TEST_P(LayoutBoxModelObjectTest, StickyPositionVerticalRLInlineConstraints) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 50));
  EXPECT_EQ(50.f, scrollable_area->ScrollPosition().y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");

  EXPECT_EQ(scroller->Layer(),
            sticky->Layer()->ContainingScrollContainerLayer());

  const auto* constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky));
  EXPECT_EQ(10.f, constraints->top_inset->ToFloat());

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  EXPECT_EQ(gfx::Rect(2000, 100, 200, 400),
            ToEnclosingRect(
                constraints->scroll_container_relative_containing_block_rect));
  EXPECT_EQ(
      gfx::Rect(2190, 100, 10, 10),
      ToEnclosingRect(constraints->scroll_container_relative_sticky_box_rect));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            ToEnclosingRect(constraints->constraining_rect));
}

// Verifies that the sticky constraints are not affected by transforms
TEST_P(LayoutBoxModelObjectTest, StickyPositionTransforms) {
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
      gfx::PointF(scrollable_area->ScrollOffsetInt().x(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  ASSERT_EQ(scroller->Layer(),
            sticky->Layer()->ContainingScrollContainerLayer());

  const auto* constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky));
  ASSERT_EQ(0.f, constraints->top_inset->ToFloat());

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(gfx::Rect(15, 115, 170, 370),
            ToEnclosingRect(
                constraints->scroll_container_relative_containing_block_rect));
  ASSERT_EQ(
      gfx::Rect(15, 115, 100, 100),
      ToEnclosingRect(constraints->scroll_container_relative_sticky_box_rect));
}

// Verifies that the sticky constraints are correctly computed.
TEST_P(LayoutBoxModelObjectTest, StickyPositionPercentageStyles) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  ASSERT_EQ(scroller->Layer(),
            sticky->Layer()->ContainingScrollContainerLayer());

  const auto* constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky));
  ASSERT_EQ(0.f, constraints->top_inset->ToFloat());

  if (RuntimeEnabledFeatures::LayoutIgnoreMarginsForStickyEnabled()) {
    ASSERT_EQ(
        gfx::Rect(25, 125, 200, 350),
        ToEnclosingRect(
            constraints->scroll_container_relative_containing_block_rect));
  } else {
    ASSERT_EQ(
        gfx::Rect(25, 145, 200, 330),
        ToEnclosingRect(
            constraints->scroll_container_relative_containing_block_rect));
  }
  ASSERT_EQ(
      gfx::Rect(25, 145, 100, 100),
      ToEnclosingRect(constraints->scroll_container_relative_sticky_box_rect));
}

// Verifies that the sticky constraints are correct when the sticky position
// container is also the ancestor scroller.
TEST_P(LayoutBoxModelObjectTest, StickyPositionContainerIsScroller) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  ASSERT_EQ(scroller->Layer(),
            sticky->Layer()->ContainingScrollContainerLayer());

  const auto* constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky));
  ASSERT_EQ(gfx::Rect(0, 0, 400, 1100),
            ToEnclosingRect(
                constraints->scroll_container_relative_containing_block_rect));
  ASSERT_EQ(
      gfx::Rect(0, 0, 100, 100),
      ToEnclosingRect(constraints->scroll_container_relative_sticky_box_rect));
}

// Verifies that the sticky constraints are correct when the sticky position
// object has an anonymous containing block.
TEST_P(LayoutBoxModelObjectTest, StickyPositionAnonymousContainer) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().y());
  auto* sticky = GetLayoutBoxModelObjectByElementId("sticky");
  ASSERT_EQ(scroller->Layer(),
            sticky->Layer()->ContainingScrollContainerLayer());

  const auto* constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky));

  ASSERT_EQ(gfx::Rect(15, 115, 170, 370),
            ToEnclosingRect(
                constraints->scroll_container_relative_containing_block_rect));
  ASSERT_EQ(
      gfx::Rect(15, 165, 100, 100),
      ToEnclosingRect(constraints->scroll_container_relative_sticky_box_rect));
}

TEST_P(LayoutBoxModelObjectTest, StickyPositionTableContainers) {
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

  const auto* constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky));

  EXPECT_EQ(gfx::Rect(0, 0, 50, 100),
            ToEnclosingRect(
                constraints->scroll_container_relative_containing_block_rect));
  EXPECT_EQ(
      gfx::Rect(0, 50, 50, 50),
      ToEnclosingRect(constraints->scroll_container_relative_sticky_box_rect));
}

// Tests that when a non-layer changes size it invalidates the constraints for
// sticky position elements within the same scroller.
TEST_P(LayoutBoxModelObjectTest, StickyPositionConstraintInvalidation) {
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

  const auto* constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky));

  EXPECT_EQ(
      25.f,
      constraints->scroll_container_relative_sticky_box_rect.X().ToFloat());
  To<HTMLElement>(target->GetNode())->classList().Add(AtomicString("hide"));
  // After updating layout we should have the updated position.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(50.f, sticky->StickyConstraints()
                      ->scroll_container_relative_sticky_box_rect.X()
                      .ToFloat());
}

TEST_P(LayoutBoxModelObjectTest, StickyPositionStatusChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: auto; height: 200px; }
      #sticky { position: sticky; top: 0; }
    </style>
    <div id='scroller'>
      <div id='sticky'></div>
      <div style='height: 500px'></div>
    </div>
  )HTML");
  auto* scrollable_area =
      GetLayoutBoxModelObjectByElementId("scroller")->GetScrollableArea();
  auto* sticky = GetElementById("sticky");
  const auto* sticky_box = sticky->GetLayoutBox();
  auto* sticky_layer = sticky_box->Layer();
  ASSERT_TRUE(sticky_layer);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky_box));
  EXPECT_TRUE(sticky_box->StickyConstraints());

  // Change top to auto which effectively makes the object no longer sticky
  // constrained and removed from the scrollable area's sticky constraints map.
  sticky->setAttribute(html_names::kStyleAttr, AtomicString("top: auto"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(sticky_box->StyleRef().HasStickyConstrainedPosition());
  ASSERT_EQ(sticky_layer, sticky_box->Layer());
  EXPECT_FALSE(HasStickyLayer(scrollable_area, sticky_box));
  EXPECT_FALSE(sticky_box->StickyConstraints());

  // Change top back to 0. |sticky| should be back to sticky constrained.
  sticky->setAttribute(html_names::kStyleAttr, g_empty_atom);
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(sticky_box->StyleRef().HasStickyConstrainedPosition());
  ASSERT_EQ(sticky_layer, sticky_box->Layer());
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky_box));
  EXPECT_TRUE(sticky_box->StickyConstraints());

  // Change position to relative. The sticky layer should be removed from the
  // scrollable area's sticky constraints map.
  sticky->setAttribute(html_names::kStyleAttr,
                       AtomicString("position: relative"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  ASSERT_EQ(sticky_layer, sticky_box->Layer());
  EXPECT_FALSE(HasStickyLayer(scrollable_area, sticky_box));
  EXPECT_FALSE(sticky_box->StickyConstraints());

  // Change position back to sticky.
  sticky->setAttribute(html_names::kStyleAttr, g_empty_atom);
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  ASSERT_EQ(sticky_layer, sticky_box->Layer());
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky_box));
  EXPECT_TRUE(sticky_box->StickyConstraints());

  // Change position to static, which removes the layer. There should be no
  // dangling pointer in the sticky constraints map.
  sticky->setAttribute(html_names::kStyleAttr,
                       AtomicString("position: static"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  ASSERT_FALSE(sticky_box->Layer());
  EXPECT_FALSE(HasStickyLayer(scrollable_area, sticky_box));
  EXPECT_FALSE(sticky_box->StickyConstraints());

  // Change position back to sticky.
  sticky->setAttribute(html_names::kStyleAttr, g_empty_atom);
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky_box));
  EXPECT_TRUE(sticky_box->StickyConstraints());

  // Remove the layout object. There should be no dangling pointer in the
  // sticky constraints map.
  sticky->setAttribute(html_names::kStyleAttr, AtomicString("display: none"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  ASSERT_FALSE(sticky->GetLayoutObject());
  EXPECT_FALSE(HasStickyLayer(scrollable_area, sticky_box));
}

// Verifies that the correct sticky-box shifting ancestor is found when
// computing the sticky constraints. Any such ancestor is the first sticky
// element between you and your containing block (exclusive).
//
// In most cases, this pointer should be null since your parent is normally your
// containing block. However there are cases where this is not true, including
// inline blocks and tables. The latter is currently irrelevant since only table
// cells can be sticky in CSS2.1, but we can test the former.
TEST_P(LayoutBoxModelObjectTest,
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
  LayoutBoxModelObject* sticky_outer_inline =
      GetLayoutBoxModelObjectByElementId("stickyOuterInline");
  LayoutBoxModelObject* unanchored_sticky =
      GetLayoutBoxModelObjectByElementId("unanchoredSticky");
  LayoutBoxModelObject* sticky_inner_inline =
      GetLayoutBoxModelObjectByElementId("stickyInnerInline");

  PaintLayerScrollableArea* scrollable_area =
      sticky_outer_div->ContainingScrollContainerLayer()->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);

  ASSERT_TRUE(
      HasStickyLayer(scrollable_area, sticky_outer_div->GetLayoutBox()));
  auto* outer_div_constraints =
      sticky_outer_div->GetLayoutObject().StickyConstraints();
  ASSERT_TRUE(outer_div_constraints);

  ASSERT_TRUE(HasStickyLayer(scrollable_area, sticky_outer_inline));
  auto* outer_inline_constraints = sticky_outer_inline->StickyConstraints();
  ASSERT_TRUE(outer_inline_constraints);

  ASSERT_FALSE(HasStickyLayer(scrollable_area, unanchored_sticky));
  EXPECT_FALSE(unanchored_sticky->StickyConstraints());

  ASSERT_TRUE(HasStickyLayer(scrollable_area, sticky_inner_inline));
  auto* inner_inline_constraints = sticky_inner_inline->StickyConstraints();
  ASSERT_TRUE(inner_inline_constraints);

  // The outer block element trivially has no sticky-box shifting ancestor.
  EXPECT_FALSE(outer_div_constraints->nearest_sticky_layer_shifting_sticky_box);

  // Neither does the outer inline element, as its parent element is also its
  // containing block.
  EXPECT_FALSE(
      outer_inline_constraints->nearest_sticky_layer_shifting_sticky_box);

  // However the inner inline element does have a sticky-box shifting ancestor,
  // as its containing block is the ancestor block element, above its ancestor
  // sticky element.
  EXPECT_EQ(sticky_outer_inline,
            inner_inline_constraints->nearest_sticky_layer_shifting_sticky_box);
}

// Verifies that the correct containing-block shifting ancestor is found when
// computing the sticky constraints. Any such ancestor is the first sticky
// element between your containing block (inclusive) and your ancestor overflow
// layer (exclusive).
TEST_P(LayoutBoxModelObjectTest,
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

  LayoutBox* scroller = GetLayoutBoxByElementId("scroller");
  LayoutBox* sticky_parent = GetLayoutBoxByElementId("stickyParent");
  LayoutBox* sticky_child = GetLayoutBoxByElementId("stickyChild");
  LayoutBox* sticky_nested_child = GetLayoutBoxByElementId("stickyNestedChild");

  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ASSERT_FALSE(HasStickyLayer(scrollable_area, scroller));
  EXPECT_TRUE(HasStickyLayer(GetLayoutView().GetScrollableArea(), scroller));
  EXPECT_TRUE(scroller->StickyConstraints());

  ASSERT_TRUE(HasStickyLayer(scrollable_area, sticky_parent));
  auto* parent_constraints = sticky_parent->StickyConstraints();
  ASSERT_TRUE(parent_constraints);

  ASSERT_TRUE(HasStickyLayer(scrollable_area, sticky_child));
  auto* child_constraints = sticky_child->StickyConstraints();
  ASSERT_TRUE(child_constraints);

  ASSERT_TRUE(HasStickyLayer(scrollable_area, sticky_nested_child));
  auto* nested_child_constraints = sticky_nested_child->StickyConstraints();
  ASSERT_TRUE(nested_child_constraints);

  // The outer <div> should not detect the scroller as its containing-block
  // shifting ancestor.
  EXPECT_FALSE(
      parent_constraints->nearest_sticky_layer_shifting_containing_block);

  // Both inner children should detect the parent <div> as their
  // containing-block shifting ancestor. They skip past unanchored sticky
  // because it will never have a non-zero offset.
  EXPECT_EQ(sticky_parent,
            child_constraints->nearest_sticky_layer_shifting_containing_block);
  EXPECT_EQ(
      sticky_parent,
      nested_child_constraints->nearest_sticky_layer_shifting_containing_block);
}

// Verifies that the correct containing-block shifting ancestor is found when
// computing the sticky constraints, in the case where the overflow ancestor is
// the page itself. This is a special-case version of the test above, as we
// often treat the root page as special when it comes to scroll logic. It should
// not make a difference for containing-block shifting ancestor calculations.
TEST_P(LayoutBoxModelObjectTest,
       StickyPositionFindsCorrectContainingBlockShiftingAncestorRoot) {
  SetBodyInnerHTML(R"HTML(
    <style>#stickyParent { position: sticky; top: 0;}
    #stickyGrandchild { position: sticky; top: 0;}</style>
    <div id='stickyParent'><div><div id='stickyGrandchild'></div></div>
    </div>
  )HTML");

  LayoutBox* sticky_parent = GetLayoutBoxByElementId("stickyParent");
  LayoutBox* sticky_grandchild = GetLayoutBoxByElementId("stickyGrandchild");

  PaintLayerScrollableArea* scrollable_area =
      sticky_parent->Layer()
          ->ContainingScrollContainerLayer()
          ->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);

  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky_parent));
  EXPECT_TRUE(sticky_parent->StickyConstraints());

  ASSERT_TRUE(HasStickyLayer(scrollable_area, sticky_grandchild));
  auto* grandchild_constraints = sticky_grandchild->StickyConstraints();
  ASSERT_TRUE(grandchild_constraints);

  // The grandchild sticky should detect the parent as its containing-block
  // shifting ancestor.
  EXPECT_EQ(
      sticky_parent,
      grandchild_constraints->nearest_sticky_layer_shifting_containing_block);
}

// Verifies that the correct containing-block shifting ancestor is found when
// computing the sticky constraints, in the case of tables. Tables are unusual
// because the containing block for all table elements is the <table> itself, so
// we have to skip over elements to find the correct ancestor.
TEST_P(LayoutBoxModelObjectTest,
       StickyPositionFindsCorrectContainingBlockShiftingAncestorTable) {
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { overflow-y: scroll; }
    #stickyOuter { position: sticky; top: 0;}
    #stickyTh { position: sticky; top: 0;}</style>
    <div id='scroller'><div id='stickyOuter'><table><thead><tr>
    <th id='stickyTh'></th></tr></thead></table></div></div>
  )HTML");

  LayoutBox* scroller = GetLayoutBoxByElementId("scroller");
  LayoutBox* sticky_outer = GetLayoutBoxByElementId("stickyOuter");
  LayoutBox* sticky_th = GetLayoutBoxByElementId("stickyTh");

  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ASSERT_FALSE(HasStickyLayer(scrollable_area, scroller));
  EXPECT_FALSE(scroller->StickyConstraints());

  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky_outer));
  EXPECT_TRUE(sticky_outer->StickyConstraints());

  ASSERT_TRUE(HasStickyLayer(scrollable_area, sticky_th));
  auto* th_constraints = sticky_th->StickyConstraints();
  ASSERT_TRUE(th_constraints);

  // The table cell should detect the outer <div> as its containing-block
  // shifting ancestor.
  EXPECT_EQ(sticky_outer,
            th_constraints->nearest_sticky_layer_shifting_containing_block);
}

// Verifies that the calculated position:sticky offsets are correct when we have
// a simple case of nested sticky elements.
TEST_P(LayoutBoxModelObjectTest, StickyPositionNested) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().y());

  // Both the parent and child sticky divs are attempting to place themselves at
  // the top of the scrollable area. To achieve this the parent must offset on
  // the y-axis against its starting position. The child is offset relative to
  // its parent so should not move at all.
  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), sticky_child->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct when the
// child has a larger edge constraint value than the parent.
TEST_P(LayoutBoxModelObjectTest, StickyPositionChildHasLargerTop) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().y());

  // The parent is attempting to place itself at the top of the scrollable area,
  // whilst the child is attempting to be 25 pixels from the top. To achieve
  // this both must offset on the y-axis against their starting positions, but
  // note the child is offset relative to the parent.
  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), sticky_child->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct when the
// child has a smaller edge constraint value than the parent.
TEST_P(LayoutBoxModelObjectTest, StickyPositionParentHasLargerTop) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().y());

  // The parent is attempting to place itself 25 pixels from the top of the
  // scrollable area, whilst the child is attempting to be at the top. However,
  // the child must stay contained within the parent, so it should be pushed
  // down to the same height. As always, the child offset is relative.
  EXPECT_EQ(PhysicalOffset(0, 75), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), sticky_child->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct when the
// child has a large enough edge constraint value to push outside of its parent.
TEST_P(LayoutBoxModelObjectTest, StickyPositionChildPushingOutsideParent) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().y());

  // The parent is attempting to place itself at the top of the scrollable area,
  // whilst the child is attempting to be 50 pixels from the top. However, there
  // is only 25 pixels of space for the child to move into, so it should be
  // capped by that offset. As always, the child offset is relative.
  EXPECT_EQ(PhysicalOffset(0, 50), sticky_parent->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), sticky_child->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of triple nesting. Triple (or more) nesting must be tested as the grandchild
// sticky must correct both its sticky box constraint rect and its containing
// block constaint rect.
TEST_P(LayoutBoxModelObjectTest, StickyPositionTripleNestedDiv) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 100));
  ASSERT_EQ(100.0, scrollable_area->ScrollPosition().y());

  // The grandparent and parent divs are attempting to place themselves at the
  // top of the scrollable area. The child div is attempting to place itself at
  // an offset of 25 pixels to the top of the scrollable area. The result of
  // this sticky offset calculation is quite simple, but internally the child
  // offset has to offset both its sticky box constraint rect and its containing
  // block constraint rect.
  EXPECT_EQ(PhysicalOffset(0, 50), outmost_sticky->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), middle_sticky->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), inner_sticky->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of tables. Tables are special as the containing block for table elements is
// always the root level <table>.
TEST_P(LayoutBoxModelObjectTest, StickyPositionNestedStickyTable) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 150));
  ASSERT_EQ(150.0, scrollable_area->ScrollPosition().y());

  // All sticky elements are attempting to stick to the top of the scrollable
  // area. For the root sticky div, this requires an offset. All the other
  // descendant sticky elements are positioned relatively so don't need offset.
  EXPECT_EQ(PhysicalOffset(0, 100), sticky_div->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), sticky_th->StickyPositionOffset());

  // If we now scroll to the point where the overall sticky div starts to move,
  // the table headers should continue to stick to the top of the scrollable
  // area until they run out of <table> space to move in.

  scrollable_area->ScrollToAbsolutePosition(
      gfx::PointF(scrollable_area->ScrollPosition().x(), 275));
  ASSERT_EQ(275.0, scrollable_area->ScrollPosition().y());

  EXPECT_EQ(PhysicalOffset(0, 200), sticky_div->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), sticky_th->StickyPositionOffset());

  // Finally, if we scroll so that the table is off the top of the page, the
  // sticky header should travel as far as it can (i.e. the table height) then
  // move off the top with it.
  scrollable_area->ScrollToAbsolutePosition(
      gfx::PointF(scrollable_area->ScrollPosition().x(), 350));
  ASSERT_EQ(350.0, scrollable_area->ScrollPosition().y());

  EXPECT_EQ(PhysicalOffset(0, 200), sticky_div->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 100), sticky_th->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// where a particular position:sticky element is used both as a sticky-box
// shifting ancestor as well as a containing-block shifting ancestor.
//
// This is a rare case that can be replicated by nesting tables so that a sticky
// cell contains another table that has sticky elements. See the HTML below.
TEST_P(LayoutBoxModelObjectTest, StickyPositionComplexTableNesting) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 150));
  ASSERT_EQ(150.0, scrollable_area->ScrollPosition().y());

  EXPECT_EQ(PhysicalOffset(0, 100), outer_sticky_th->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), inner_sticky_th->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of nested inline elements.
TEST_P(LayoutBoxModelObjectTest, StickyPositionNestedInlineElements) {
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
      gfx::PointF(scrollable_area->ScrollPosition().x(), 50));
  ASSERT_EQ(50.0, scrollable_area->ScrollPosition().y());

  EXPECT_EQ(PhysicalOffset(0, 0), outer_inline->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), inner_inline->StickyPositionOffset());
}

// Verifies that the calculated position:sticky offsets are correct in the case
// of an intermediate position:fixed element.
TEST_P(LayoutBoxModelObjectTest, StickyPositionNestedFixedPos) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { width: 0; height: 0; }
      body { margin: 0; }
      #scroller { height: 200px; width: 100px; overflow-y: auto; }
      #outerSticky { position: sticky; top: 0; height: 50px; }
      #fixedDiv { position: fixed; top: 0; left: 300px; height: 1000px;
                  width: 100px; }
      #innerStickyTop { position: sticky; top: 25px; height: 25px; }
      #innerStickyBottom { position: sticky; bottom: 25px; height: 25px; }
      .padding { height: 600px; }
    </style>
    <div id='scroller'>
      <div id='outerSticky'>
        <div id='fixedDiv'>
          <div id='innerStickyTop'></div>
          <div class='padding'></div>
          <div id='innerStickyBottom'></div>
        </div>
      </div>
      <div class='padding'></div>
    </div>
    <div class='padding'></div>
  )HTML");

  // The view size is set by the base class. This test depends on it.
  ASSERT_EQ(PhysicalSize(800, 600), GetLayoutView().Size());

  auto* outer_sticky = GetLayoutBoxModelObjectByElementId("outerSticky");
  auto* inner_sticky_top = GetLayoutBoxModelObjectByElementId("innerStickyTop");
  auto* inner_sticky_bottom =
      GetLayoutBoxModelObjectByElementId("innerStickyBottom");

  auto* view_scrollable_area = GetLayoutView().GetScrollableArea();
  auto* scroller = GetLayoutBoxModelObjectByElementId("scroller");
  auto* scroller_scrollable_area = scroller->GetScrollableArea();

  // outerSticky is contained by the scroller.
  ASSERT_FALSE(HasStickyLayer(view_scrollable_area, outer_sticky));
  bool is_fixed_to_view = false;
  ASSERT_EQ(
      scroller->Layer(),
      outer_sticky->Layer()->ContainingScrollContainerLayer(&is_fixed_to_view));
  ASSERT_FALSE(is_fixed_to_view);
  ASSERT_TRUE(HasStickyLayer(scroller_scrollable_area, outer_sticky));

  // innerSticky* are not contained by the scroller, but by the LayoutView
  ASSERT_TRUE(HasStickyLayer(view_scrollable_area, inner_sticky_top));
  ASSERT_EQ(GetLayoutView().Layer(),
            inner_sticky_top->Layer()->ContainingScrollContainerLayer(
                &is_fixed_to_view));
  ASSERT_TRUE(is_fixed_to_view);
  ASSERT_FALSE(HasStickyLayer(scroller_scrollable_area, inner_sticky_top));
  ASSERT_TRUE(HasStickyLayer(view_scrollable_area, inner_sticky_top));
  ASSERT_EQ(GetLayoutView().Layer(),
            inner_sticky_bottom->Layer()->ContainingScrollContainerLayer(
                &is_fixed_to_view));
  ASSERT_TRUE(is_fixed_to_view);
  ASSERT_FALSE(HasStickyLayer(scroller_scrollable_area, inner_sticky_top));
  ASSERT_TRUE(HasStickyLayer(view_scrollable_area, inner_sticky_top));

  // innerSticky* should not detect the outer one as any sort of ancestor.
  auto* inner_constraints_top = inner_sticky_top->StickyConstraints();
  ASSERT_TRUE(inner_constraints_top);
  EXPECT_FALSE(inner_constraints_top->nearest_sticky_layer_shifting_sticky_box);
  EXPECT_FALSE(
      inner_constraints_top->nearest_sticky_layer_shifting_containing_block);
  auto* inner_constraints_bottom = inner_sticky_bottom->StickyConstraints();
  ASSERT_TRUE(inner_constraints_bottom);
  EXPECT_FALSE(
      inner_constraints_bottom->nearest_sticky_layer_shifting_sticky_box);
  EXPECT_FALSE(
      inner_constraints_bottom->nearest_sticky_layer_shifting_containing_block);

  // Scroll the scroller down.
  scroller_scrollable_area->ScrollToAbsolutePosition(
      gfx::PointF(scroller_scrollable_area->ScrollPosition().x(), 100));
  ASSERT_EQ(100.0, scroller_scrollable_area->ScrollPosition().y());

  EXPECT_EQ(PhysicalOffset(0, 100), outer_sticky->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), inner_sticky_top->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, -75),
            inner_sticky_bottom->StickyPositionOffset());

  // Scroll the page down. No StickyPositionOffset() should change because
  // none of the sticky elements scroll with the view.
  view_scrollable_area->ScrollToAbsolutePosition(
      gfx::PointF(view_scrollable_area->ScrollPosition().x(), 100));
  ASSERT_EQ(100.0, view_scrollable_area->ScrollPosition().y());

  EXPECT_EQ(PhysicalOffset(0, 100), outer_sticky->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, 25), inner_sticky_top->StickyPositionOffset());
  EXPECT_EQ(PhysicalOffset(0, -75),
            inner_sticky_bottom->StickyPositionOffset());
}

TEST_P(LayoutBoxModelObjectTest, InvalidatePaintLayerOnStackedChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .stacked { background: red; position: relative; height: 2000px; }
      .non-stacked { all: inherit }
    </style>
    <div style='height: 100px; backface-visibility: hidden'>
      <div id='target' class='stacked'></div>
    </div>
  )HTML");

  auto* target_element = GetElementById("target");
  auto* target = target_element->GetLayoutBoxModelObject();
  auto* parent = target->Parent();
  auto* original_compositing_container =
      target->Layer()->CompositingContainer();
  EXPECT_FALSE(target->IsStackingContext());
  EXPECT_TRUE(target->IsStacked());
  EXPECT_FALSE(parent->IsStacked());
  EXPECT_NE(parent, original_compositing_container->GetLayoutObject());

  target_element->setAttribute(html_names::kClassAttr,
                               AtomicString("non-stacked"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  EXPECT_FALSE(target->IsStacked());
  EXPECT_TRUE(target->Layer()->SelfNeedsRepaint());
  EXPECT_TRUE(original_compositing_container->DescendantNeedsRepaint());
  auto* new_compositing_container = target->Layer()->CompositingContainer();
  EXPECT_EQ(parent, new_compositing_container->GetLayoutObject());

  UpdateAllLifecyclePhasesForTest();
  target_element->setAttribute(html_names::kClassAttr, AtomicString("stacked"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  EXPECT_TRUE(target->IsStacked());
  EXPECT_TRUE(target->Layer()->SelfNeedsRepaint());
  EXPECT_TRUE(new_compositing_container->DescendantNeedsRepaint());
  EXPECT_EQ(original_compositing_container,
            target->Layer()->CompositingContainer());
}

TEST_P(LayoutBoxModelObjectTest, BackfaceVisibilityChange) {
  AtomicString base_style(
      "width: 100px; height: 100px; background: blue; position: absolute");
  SetBodyInnerHTML("<div id='target' style='" + base_style + "'></div>");

  auto* target = GetElementById("target");
  auto* target_layer =
      To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer();
  ASSERT_NE(nullptr, target_layer);
  EXPECT_FALSE(target_layer->SelfNeedsRepaint());

  target->setAttribute(
      html_names::kStyleAttr,
      AtomicString(base_style + "; backface-visibility: hidden"));
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

TEST_P(LayoutBoxModelObjectTest, ChangingFilterWithWillChange) {
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
  auto* target = GetElementById("target");
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("filter: grayscale(1)"));
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

TEST_P(LayoutBoxModelObjectTest, ChangingWillChangeFilter) {
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
  auto* target = GetElementById("target");
  target->classList().Add(AtomicString("willChange"));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());

  // A lifecycle update should clear dirty bits.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());

  // Removing will-change: filter should check for paint invalidation and remove
  // the PaintLayer.
  target->classList().Remove(AtomicString("willChange"));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());
}

TEST_P(LayoutBoxModelObjectTest, ChangingBackdropFilterWithWillChange) {
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
  auto* target = GetElementById("target");
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("backdrop-filter: grayscale(1)"));
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

TEST_P(LayoutBoxModelObjectTest, ChangingWillChangeBackdropFilter) {
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
  auto* target = GetElementById("target");
  target->classList().Add(AtomicString("willChange"));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());

  // A lifecycle update should clear dirty bits.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());

  // Removing will-change: backdrop-filter should check for paint invalidation
  // and remove the PaintLayer.
  target->classList().Remove(AtomicString("willChange"));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(To<LayoutBoxModelObject>(target->GetLayoutObject())->Layer());
}

TEST_P(LayoutBoxModelObjectTest, UpdateStackingContextForOption) {
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

  auto* option_element = GetElementById("opt");
  auto* option_layout = option_element->GetLayoutObject();
  ASSERT_TRUE(option_layout);
  EXPECT_TRUE(option_layout->IsStackingContext());
  EXPECT_TRUE(option_layout->StyleRef().HasCurrentOpacityAnimation());
}

TEST_P(LayoutBoxModelObjectTest,
       StickyParentContainStrictChangeOverflowProperty) {
  SetBodyInnerHTML(R"HTML(
    <style>html, body { contain: strict; }</style>
    <div id="sticky" style="position: sticky; top: 1px"></div>
  )HTML");

  auto* sticky = GetLayoutBoxByElementId("sticky");
  auto* constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_EQ(&GetLayoutView(),
            &constraints->containing_scroll_container_layer->GetLayoutObject());

  GetDocument().body()->setAttribute(html_names::kStyleAttr,
                                     AtomicString("overflow: hidden"));
  UpdateAllLifecyclePhasesForTest();
  constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_EQ(GetDocument().body()->GetLayoutObject(),
            &constraints->containing_scroll_container_layer->GetLayoutObject());

  GetDocument().body()->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  constraints = sticky->StickyConstraints();
  ASSERT_TRUE(constraints);
  EXPECT_EQ(&GetLayoutView(),
            &constraints->containing_scroll_container_layer->GetLayoutObject());
}

TEST_P(LayoutBoxModelObjectTest, RemoveStickyUnderContain) {
  SetBodyInnerHTML(R"HTML(
    <div id="contain" style="contain: strict; width: 100px; height: 2000px">
      <div id="parent">
        <div id="sticky" style="top: 100px; position: sticky">STICKY</div>
      </div>
    </div>
  )HTML");

  auto* scrollable_area = GetLayoutView().GetScrollableArea();
  auto* sticky = GetLayoutBoxByElementId("sticky");
  EXPECT_TRUE(HasStickyLayer(scrollable_area, sticky));

  GetElementById("parent")->remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(HasStickyLayer(scrollable_area, sticky));

  // This should not crash.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(LayoutBoxModelObjectTest, ChangeStickyStatusUnderContain) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { contain: strict; height: 2000px; }
    </style>
    <div id="target"></div>
  )HTML");

  auto* target = GetElementById("target");
  EXPECT_FALSE(target->GetLayoutBox()->StickyConstraints());

  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("top: 1px; position: sticky"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(target->GetLayoutBox()->StickyConstraints());
  GetLayoutView().GetScrollableArea()->ScrollToAbsolutePosition(
      gfx::PointF(0, 50));

  target->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetLayoutBox()->StickyConstraints());

  // This should not crash.
  GetLayoutView().GetScrollableArea()->ScrollToAbsolutePosition(
      gfx::PointF(0, 100));
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(LayoutBoxModelObjectTest, ChangeStickyStatusKeepLayerUnderContain) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { contain: strict; height: 2000px; }
      #target { opacity: 0.9; }
    </style>
    <div id="target"></div>
  )HTML");

  auto* target = GetElementById("target");
  EXPECT_FALSE(target->GetLayoutBox()->StickyConstraints());

  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("top: 1px; position: sticky"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(target->GetLayoutBox()->StickyConstraints());
  GetLayoutView().GetScrollableArea()->ScrollToAbsolutePosition(
      gfx::PointF(0, 50));

  target->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetLayoutBox()->StickyConstraints());

  // This should not crash.
  GetLayoutView().GetScrollableArea()->ScrollToAbsolutePosition(
      gfx::PointF(0, 100));
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(LayoutBoxModelObjectTest,
       RemoveStickyStatusInNestedStickyElementsWithContain) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body, #container, #child {
        contain: strict;
        position: sticky;
        bottom: 0;
        height: 2000px;
      }
    </style>
    <div id="container">
      <div id="child"></div>
    </div>
  )HTML");

  auto* body = GetDocument().body()->GetLayoutBox();
  auto* container_element = GetElementById("container");
  auto* container = container_element->GetLayoutBoxModelObject();
  auto* child = GetLayoutBoxModelObjectByElementId("child");

  ASSERT_TRUE(body->StickyConstraints());
  ASSERT_TRUE(container->StickyConstraints());
  auto* child_constraints = child->StickyConstraints();
  ASSERT_TRUE(child_constraints);
  EXPECT_EQ(
      container,
      child_constraints->nearest_sticky_layer_shifting_containing_block.Get());

  GetLayoutView().GetScrollableArea()->ScrollToAbsolutePosition(
      gfx::PointF(0, 50));

  container_element->setAttribute(html_names::kStyleAttr,
                                  AtomicString("position: relative"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  ASSERT_TRUE(body->StickyConstraints());
  ASSERT_FALSE(container->StickyConstraints());
  child_constraints = child->StickyConstraints();
  ASSERT_TRUE(child_constraints);
  EXPECT_EQ(
      body,
      child_constraints->nearest_sticky_layer_shifting_containing_block.Get());

  // This should not crash.
  GetLayoutView().GetScrollableArea()->ScrollToAbsolutePosition(
      gfx::PointF(0, 0));
  UpdateAllLifecyclePhasesForTest();
}

}  // namespace blink
