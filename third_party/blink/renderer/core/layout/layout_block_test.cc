// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using ::testing::MatchesRegex;

namespace blink {

class LayoutBlockTest : public RenderingTest {};

TEST_F(LayoutBlockTest, LayoutNameCalledWithNullStyle) {
  scoped_refptr<ComputedStyle> style =
      GetDocument().GetStyleResolver().CreateComputedStyle();
  LayoutObject* obj = LayoutBlockFlow::CreateAnonymous(&GetDocument(), style,
                                                       LegacyLayout::kAuto);
  obj->SetStyle(nullptr, LayoutObject::ApplyStyleChanges::kNo);
  EXPECT_FALSE(obj->Style());
  EXPECT_THAT(obj->DecoratedName().Ascii(),
              MatchesRegex("LayoutN?G?BlockFlow \\(anonymous\\)"));
  obj->Destroy();
}

TEST_F(LayoutBlockTest, WidthAvailableToChildrenChanged) {
  USE_NON_OVERLAY_SCROLLBARS();

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id='list' style='overflow-y:auto; width:150px; height:100px'>
      <div style='height:20px'>Item</div>
      <div style='height:20px'>Item</div>
      <div style='height:20px'>Item</div>
      <div style='height:20px'>Item</div>
      <div style='height:20px'>Item</div>
      <div style='height:20px'>Item</div>
    </div>
  )HTML");
  Element* list_element = GetDocument().getElementById("list");
  ASSERT_TRUE(list_element);
  auto* list_box = list_element->GetLayoutBox();
  Element* item_element = ElementTraversal::FirstChild(*list_element);
  ASSERT_TRUE(item_element);
  ASSERT_GT(list_box->ComputeScrollbars().HorizontalSum(), 0);
  ASSERT_EQ(item_element->OffsetWidth(),
            150 - list_box->ComputeScrollbars().HorizontalSum());

  DummyExceptionStateForTesting exception_state;
  list_element->style()->setCSSText(GetDocument().GetExecutionContext(),
                                    "width:150px;height:100px;",
                                    exception_state);
  ASSERT_FALSE(exception_state.HadException());
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(list_box->ComputeScrollbars().HorizontalSum(), 0);
  ASSERT_EQ(item_element->OffsetWidth(), 150);
}

TEST_F(LayoutBlockTest, OverflowWithTransformAndPerspective) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width: 100px; height: 100px; overflow: scroll;
        perspective: 100px;'>
      <div style='transform: rotateY(-45deg); width: 140px; height: 100px'>
      </div>
    </div>
  )HTML");
  auto* scroller = GetLayoutBoxByElementId("target");
  EXPECT_EQ(187.625, scroller->LayoutOverflowRect().Width().ToFloat());
}

TEST_F(LayoutBlockTest, NestedInlineVisualOverflow) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 0; height: 0">
      <span style="font-size: 10px/10px">
        <img style="margin-left: -15px; width: 40px; height: 40px">
      </span>
    </div>
  )HTML");

  auto* target = GetLayoutBoxByElementId("target");
  EXPECT_EQ(LayoutRect(-15, 0, 40, 40), target->VisualOverflowRect());
  EXPECT_EQ(PhysicalRect(-15, 0, 40, 40), target->PhysicalVisualOverflowRect());
}

TEST_F(LayoutBlockTest, NestedInlineVisualOverflowVerticalRL) {
  SetBodyInnerHTML(R"HTML(
    <div style="width: 100px; writing-mode: vertical-rl">
      <div id="target" style="width: 0; height: 0">
        <span style="font-size: 10px/10px">
          <img style="margin-right: -15px; width: 40px; height: 40px">
        </span>
      </div>
    </div>
  )HTML");

  auto* target = GetLayoutBoxByElementId("target");
  EXPECT_EQ(LayoutRect(-15, 0, 40, 40), target->VisualOverflowRect());
  EXPECT_EQ(PhysicalRect(-25, 0, 40, 40), target->PhysicalVisualOverflowRect());
}

TEST_F(LayoutBlockTest, ContainmentStyleChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { display: block }
    </style>
    <div id=target style="contain:strict">
      <div>
        <div>
          <div id=contained style="position: fixed"></div>
          <div></div>
        <div>
      </div>
    </div>
  )HTML");

  Element* target_element = GetDocument().getElementById("target");
  auto* target = To<LayoutBlockFlow>(target_element->GetLayoutObject());
  auto* contained = GetLayoutBoxByElementId("contained");
  if (target->IsLayoutNGObject()) {
    EXPECT_TRUE(target->GetSingleCachedLayoutResult()
                    ->PhysicalFragment()
                    .HasOutOfFlowFragmentChild());
  } else {
    EXPECT_TRUE(target->PositionedObjects()->Contains(contained));
  }

  // Remove layout containment. This should cause |contained| to now be
  // in the positioned objects set for the LayoutView, not |target|.
  target_element->setAttribute(html_names::kStyleAttr, "contain:style");
  UpdateAllLifecyclePhasesForTest();
  if (target->IsLayoutNGObject()) {
    EXPECT_FALSE(target->GetSingleCachedLayoutResult()
                     ->PhysicalFragment()
                     .HasOutOfFlowFragmentChild());
  } else {
    EXPECT_FALSE(target->PositionedObjects());
  }
  const LayoutView* view = GetDocument().GetLayoutView();
  if (view->IsLayoutNGObject()) {
    EXPECT_TRUE(view->GetSingleCachedLayoutResult()
                    ->PhysicalFragment()
                    .HasOutOfFlowFragmentChild());
  } else {
    EXPECT_TRUE(view->PositionedObjects()->Contains(contained));
  }
}

}  // namespace blink
