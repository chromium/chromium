// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/block_layout_algorithm.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/tag_collection.h"
#include "third_party/blink/renderer/core/layout/base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

using testing::ElementsAre;
using testing::Pointee;

class BlockLayoutAlgorithmTest : public BaseLayoutAlgorithmTest {
 protected:
  void SetUp() override { BaseLayoutAlgorithmTest::SetUp(); }

  const PhysicalBoxFragment* GetHtmlPhysicalFragment() const {
    const auto* layout_box =
        To<LayoutBox>(GetDocument()
                          .getElementsByTagName(AtomicString("html"))
                          ->item(0)
                          ->GetLayoutObject());
    return To<PhysicalBoxFragment>(
        &layout_box->GetSingleCachedLayoutResult()->GetPhysicalFragment());
  }

  MinMaxSizes RunComputeMinMaxSizes(BlockNode node) {
    // The constraint space is not used for min/max computation, but we need
    // it to create the algorithm.
    ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        LogicalSize(LayoutUnit(), LayoutUnit()));
    FragmentGeometry fragment_geometry = CalculateInitialFragmentGeometry(
        space, node, /* break_token */ nullptr, /* is_intrinsic */ true);

    BlockLayoutAlgorithm algorithm({node, fragment_geometry, space});
    return algorithm.ComputeMinMaxSizes(MinMaxSizesFloatInput()).sizes;
  }

  const LayoutResult* RunCachedLayoutResult(const ConstraintSpace& space,
                                            const BlockNode& node) {
    LayoutCacheStatus cache_status;
    std::optional<FragmentGeometry> initial_fragment_geometry;
    return To<LayoutBlockFlow>(node.GetLayoutBox())
        ->CachedLayoutResult(space, nullptr, nullptr, nullptr,
                             &initial_fragment_geometry, &cache_status);
  }

  String DumpFragmentTree(const PhysicalBoxFragment* fragment) {
    PhysicalFragment::DumpFlags flags =
        PhysicalFragment::DumpHeaderText | PhysicalFragment::DumpSubtree |
        PhysicalFragment::DumpIndentation | PhysicalFragment::DumpOffset |
        PhysicalFragment::DumpSize;

    return fragment->DumpFragmentTree(flags);
  }

  template <typename UpdateFunc>
  void UpdateStyleForElement(Element* element, const UpdateFunc& update) {
    auto* layout_object = element->GetLayoutObject();
    ComputedStyleBuilder builder(layout_object->StyleRef());
    update(builder);
    layout_object->SetStyle(builder.TakeStyle(),
                            LayoutObject::ApplyStyleChanges::kNo);
    layout_object->SetNeedsLayout("");
    UpdateAllLifecyclePhasesForTest();
  }
};

TEST_F(BlockLayoutAlgorithmTest, FixedSize) {
  SetBodyInnerHTML(R"HTML(
    <div id="box" style="width:30px; height:40px"></div>
  )HTML");

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), kIndefiniteSize));

  BlockNode box(GetLayoutBoxByElementId("box"));

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(box, space);

  EXPECT_EQ(PhysicalSize(30, 40), fragment->Size());
}

TEST_F(BlockLayoutAlgorithmTest, Caching) {
  // The inner element exists so that "simplified" layout logic isn't invoked.
  SetBodyInnerHTML(R"HTML(
    <div id="box" style="width:30px; height:40%;">
      <div style="height: 100%;"></div>
    </div>
  )HTML");

  AdvanceToLayoutPhase();
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)));

  auto* block_flow = To<LayoutBlockFlow>(GetLayoutObjectByElementId("box"));
  BlockNode node(block_flow);

  const LayoutResult* result = node.Layout(space, nullptr);
  EXPECT_EQ(PhysicalSize(30, 40), result->GetPhysicalFragment().Size());

  // Test pointer-equal constraint space.
  result = RunCachedLayoutResult(space, node);
  EXPECT_NE(result, nullptr);

  // Test identical, but not pointer-equal, constraint space.
  space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)));
  result = RunCachedLayoutResult(space, node);
  EXPECT_NE(result, nullptr);

  // Test different constraint space.
  space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(100)));
  result = RunCachedLayoutResult(space, node);
  EXPECT_NE(result, nullptr);

  // Test a different constraint space that will actually result in a different
  // sized fragment.
  space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(200)));
  result = RunCachedLayoutResult(space, node);
  EXPECT_EQ(result, nullptr);

  // Test layout invalidation
  block_flow->SetNeedsLayout("");
  result = RunCachedLayoutResult(space, node);
  EXPECT_EQ(result, nullptr);
}

TEST_F(BlockLayoutAlgorithmTest, MinInlineSizeCaching) {
  SetBodyInnerHTML(R"HTML(
    <div id="box" style="min-width:30%; width: 10px; height:40px;"></div>
  )HTML");

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)));

  auto* block_flow = To<LayoutBlockFlow>(GetLayoutObjectByElementId("box"));
  BlockNode node(block_flow);

  const LayoutResult* result = node.Layout(space, nullptr);
  EXPECT_EQ(PhysicalSize(30, 40), result->GetPhysicalFragment().Size());

  // Test pointer-equal constraint space.
  result = RunCachedLayoutResult(space, node);
  EXPECT_NE(result, nullptr);

  // Test identical, but not pointer-equal, constraint space.
  space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)));
  result = RunCachedLayoutResult(space, node);
  EXPECT_NE(result, nullptr);

  // Test different constraint space.
  space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(200)));
  result = RunCachedLayoutResult(space, node);
  EXPECT_NE(result, nullptr);

  // Test a different constraint space that will actually result in a different
  // size.
  space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(100)));
  result = RunCachedLayoutResult(space, node);
  EXPECT_EQ(result, nullptr);
}

TEST_F(BlockLayoutAlgorithmTest, PercentageBlockSizeQuirkDescendantsCaching) {
  // Quirks mode triggers the interesting parent-child %-resolution behavior.
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);

  SetBodyInnerHTML(R"HTML(
    <div id="container" style="display: flow-root; width: 100px; height: 100px;">
      <div id="box1"></div>
      <div id="box2">
        <div style="height: 20px;"></div>
        <div style="height: 20px;"></div>
      </div>
      <div id="box3">
        <div style="height: 20px;"></div>
        <div style="height: 50%;"></div>
      </div>
      <div id="box4">
        <div style="height: 20px;"></div>
        <div style="display: flex;"></div>
      </div>
      <div id="box5">
        <div style="height: 20px;"></div>
        <div style="display: flex; height: 50%;"></div>
      </div>
      <div id="box6" style="position: relative;">
        <div style="position: absolute; width: 10px; height: 100%;"></div>
      </div>
      <div id="box7">
        <img />
      </div>
      <div id="box8">
        <img style="height: 100%;" />
      </div>
    </div>
  )HTML");

  auto create_space = [&](auto size) -> ConstraintSpace {
    ConstraintSpaceBuilder builder(
        WritingMode::kHorizontalTb,
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        /* is_new_formatting_context */ false);
    builder.SetAvailableSize(size);
    builder.SetPercentageResolutionSize(size);
    builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
    return builder.ToConstraintSpace();
  };

  ConstraintSpace space100 =
      create_space(LogicalSize(LayoutUnit(100), LayoutUnit(100)));
  ConstraintSpace space200 =
      create_space(LogicalSize(LayoutUnit(100), LayoutUnit(200)));

  auto run_test = [&](auto id) -> const LayoutResult* {
    // Grab the box under test.
    auto* box = To<LayoutBlockFlow>(GetLayoutObjectByElementId(id));
    BlockNode node(box);

    // Check that we have a cache hit with space100.
    const LayoutResult* result = RunCachedLayoutResult(space100, node);
    EXPECT_NE(result, nullptr);

    // Return the result of the cache with space200.
    return RunCachedLayoutResult(space200, node);
  };

  // Test 1: No descendants.
  EXPECT_NE(run_test("box1"), nullptr);

  // Test 2: No %-height descendants.
  EXPECT_NE(run_test("box2"), nullptr);

  // Test 3: A %-height descendant.
  EXPECT_EQ(run_test("box3"), nullptr);

  // Test 4: A flexbox (legacy descendant), which doesn't use the quirks mode
  // behavior.
  EXPECT_NE(run_test("box4"), nullptr);

  // Test 5: A flexbox (legacy descendant), which doesn't use the quirks mode
  // behavior, but is %-sized.
  EXPECT_EQ(run_test("box5"), nullptr);

  // Test 6: An OOF positioned descentant which has a %-height, should not
  // count as a percentage descendant.
  EXPECT_NE(run_test("box6"), nullptr);

  // Test 7: A replaced element (legacy descendant), shouldn't use the quirks
  // mode behavior.
  EXPECT_NE(run_test("box7"), nullptr);

  // Test 8: A replaced element (legacy descendant), shouldn't use the quirks
  // mode behavior, but is %-sized.
  EXPECT_EQ(run_test("box8"), nullptr);
}

TEST_F(BlockLayoutAlgorithmTest, LineOffsetCaching) {
  SetBodyInnerHTML(R"HTML(
    <div id="container" style="display: flow-root; width: 300px; height: 100px;">
      <div id="box1" style="width: 100px; margin: 0 auto 0 auto;"></div>
    </div>
  )HTML");

  auto create_space = [&](auto size, auto bfc_offset) -> ConstraintSpace {
    ConstraintSpaceBuilder builder(
        WritingMode::kHorizontalTb,
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        /* is_new_formatting_context */ false);
    builder.SetAvailableSize(size);
    builder.SetPercentageResolutionSize(size);
    builder.SetBfcOffset(bfc_offset);
    return builder.ToConstraintSpace();
  };

  ConstraintSpace space200 =
      create_space(LogicalSize(LayoutUnit(300), LayoutUnit(100)),
                   BfcOffset(LayoutUnit(50), LayoutUnit()));

  const LayoutResult* result = nullptr;
  auto* box1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("box1"));

  // Ensure we get a cached layout result, even if our BFC line-offset changed.
  result = RunCachedLayoutResult(space200, BlockNode(box1));
  EXPECT_NE(result, nullptr);
}

// Verifies that two children are laid out with the correct size and position.
TEST_F(BlockLayoutAlgorithmTest, LayoutBlockChildren) {
  SetBodyInnerHTML(R"HTML(
    <div id="container" style="width: 30px">
      <div style="height: 20px">
      </div>
      <div style="height: 30px; margin-top: 5px; margin-bottom: 20px">
      </div>
    </div>
  )HTML");
  const int kWidth = 30;
  const int kHeight1 = 20;
  const int kHeight2 = 30;
  const int kMarginTop = 5;

  BlockNode container(GetLayoutBoxByElementId("container"));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), kIndefiniteSize));

  const PhysicalBoxFragment* fragment =
      RunBlockLayoutAlgorithm(container, space);

  EXPECT_EQ(LayoutUnit(kWidth), fragment->Size().width);
  EXPECT_EQ(LayoutUnit(kHeight1 + kHeight2 + kMarginTop),
            fragment->Size().height);
  EXPECT_EQ(PhysicalFragment::kFragmentBox, fragment->Type());
  ASSERT_EQ(fragment->Children().size(), 2UL);

  const PhysicalFragmentLink& first_child = fragment->Children()[0];
  EXPECT_EQ(kHeight1, first_child->Size().height);
  EXPECT_EQ(0, first_child.Offset().top);

  const PhysicalFragmentLink& second_child = fragment->Children()[1];
  EXPECT_EQ(kHeight2, second_child->Size().height);
  EXPECT_EQ(kHeight1 + kMarginTop, second_child.Offset().top);
}

// Verifies that a child is laid out correctly if it's writing mode is different
// from the parent's one.
TEST_F(BlockLayoutAlgorithmTest, LayoutBlockChildrenWithWritingMode) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #div2 {
        width: 50px;
        height: 50px;
        margin-left: 100px;
        writing-mode: horizontal-tb;
      }
    </style>
    <div id="container">
      <div id="div1" style="writing-mode: vertical-lr;">
        <div id="div2">
        </div>
      </div>
    </div>
  )HTML");
  const int kHeight = 50;
  const int kMarginLeft = 100;

  BlockNode container(GetLayoutBoxByElementId("container"));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(500), LayoutUnit(500)));
  const PhysicalBoxFragment* fragment =
      RunBlockLayoutAlgorithm(container, space);

  const PhysicalFragmentLink& child = fragment->Children()[0];
  const PhysicalFragmentLink& child2 =
      static_cast<const PhysicalBoxFragment*>(child.get())->Children()[0];

  EXPECT_EQ(kHeight, child2->Size().height);
  EXPECT_EQ(0, child2.Offset().top);
  EXPECT_EQ(kMarginLeft, child2.Offset().left);
}

// Verifies that floats are positioned at the top of the first child that can
// determine its position after margins collapsed.
TEST_F(BlockLayoutAlgorithmTest, CollapsingMarginsCase1WithFloats) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 200px;
          width: 200px;
          margin-top: 10px;
          padding: 0 7px;
          background-color: red;
        }
        #first-child {
          margin-top: 20px;
          height: 10px;
          background-color: blue;
        }
        #float-child-left {
          float: left;
          height: 10px;
          width: 10px;
          padding: 10px;
          margin: 10px;
          background-color: green;
        }
        #float-child-right {
          float: right;
          height: 30px;
          width: 30px;
          background-color: pink;
        }
      </style>
      <div id='container'>
        <div id='float-child-left'></div>
        <div id='float-child-right'></div>
        <div id='first-child'></div>
      </div>
    )HTML");

  const auto* fragment = GetHtmlPhysicalFragment();
  ASSERT_EQ(fragment->Children().size(), 1UL);

  PhysicalOffset body_offset = fragment->Children()[0].Offset();
  auto* body_fragment = To<PhysicalBoxFragment>(fragment->Children()[0].get());
  // 20 = max(first child's margin top, containers's margin top)
  int body_top_offset = 20;
  EXPECT_THAT(LayoutUnit(body_top_offset), body_offset.top);
  // 8 = body's margin
  int body_left_offset = 8;
  EXPECT_THAT(LayoutUnit(body_left_offset), body_offset.left);
  ASSERT_EQ(1UL, body_fragment->Children().size());

  auto* container_fragment =
      To<PhysicalBoxFragment>(body_fragment->Children()[0].get());
  PhysicalOffset container_offset = body_fragment->Children()[0].Offset();

  // 0 = collapsed with body's margin
  EXPECT_THAT(LayoutUnit(0), container_offset.top);
  ASSERT_EQ(3UL, container_fragment->Children().size());

  PhysicalOffset child_offset = container_fragment->Children()[2].Offset();

  // 0 = collapsed with container's margin
  EXPECT_THAT(LayoutUnit(0), child_offset.top);
}

// Verifies the collapsing margins case for the next pairs:
// - bottom margin of box and top margin of its next in-flow following sibling.
// - top and bottom margins of a box that does not establish a new block
//   formatting context and that has zero computed 'min-height', zero or 'auto'
//   computed 'height', and no in-flow children
TEST_F(BlockLayoutAlgorithmTest, CollapsingMarginsCase2WithFloats) {
  SetBodyInnerHTML(R"HTML(
      <style>
      #first-child {
        background-color: red;
        height: 50px;
        margin-bottom: 20px;
      }
      #float-between-empties {
        background-color: green;
        float: left;
        height: 30px;
        width: 30px;
      }
      #float-between-nonempties {
        background-color: lightgreen;
        float: left;
        height: 40px;
        width: 40px;
      }
      #float-top-align {
        background-color: seagreen;
        float: left;
        height: 50px;
        width: 50px;
      }
      #second-child {
        background-color: blue;
        height: 50px;
        margin-top: 10px;
      }
      </style>
      <div id='first-child'>
        <div id='empty1' style='margin-bottom: -15px'></div>
        <div id='float-between-empties'></div>
        <div id='empty2'></div>
      </div>
      <div id='float-between-nonempties'></div>
      <div id='second-child'>
        <div id='float-top-align'></div>
        <div id='empty3'></div>
        <div id='empty4' style='margin-top: -30px'></div>
      </div>
      <div id='empty5'></div>
    )HTML");

  const auto* fragment = GetHtmlPhysicalFragment();
  auto* body_fragment = To<PhysicalBoxFragment>(fragment->Children()[0].get());
  PhysicalOffset body_offset = fragment->Children()[0].Offset();
  // -7 = empty1's margin(-15) + body's margin(8)
  EXPECT_THAT(LayoutUnit(-7), body_offset.top);
  ASSERT_EQ(4UL, body_fragment->Children().size());

  FragmentChildIterator iterator(body_fragment);
  PhysicalOffset offset;
  iterator.NextChild(&offset);
  EXPECT_THAT(LayoutUnit(), offset.top);

  iterator.NextChild(&offset);
  // 70 = first_child's height(50) + first child's margin-bottom(20)
  EXPECT_THAT(offset.top, LayoutUnit(70));
  EXPECT_THAT(offset.left, LayoutUnit(0));

  iterator.NextChild(&offset);
  // 40 = first_child's height(50) - margin's collapsing result(10)
  EXPECT_THAT(LayoutUnit(40), offset.top);

  iterator.NextChild(&offset);
  // 90 = first_child's height(50) + collapsed margins(-10) +
  // second child's height(50)
  EXPECT_THAT(LayoutUnit(90), offset.top);

  // ** Verify layout tree **
  Element* first_child = GetElementById("first-child");
  // -7 = body_top_offset
  EXPECT_EQ(-7, first_child->OffsetTop());
}

// Verifies the collapsing margins case for the next pair:
// - bottom margin of a last in-flow child and bottom margin of its parent if
//   the parent has 'auto' computed height
TEST_F(BlockLayoutAlgorithmTest, CollapsingMarginsCase3) {
  SetBodyInnerHTML(R"HTML(
      <style>
       #container {
         margin-bottom: 20px;
       }
       #child {
         margin-bottom: 200px;
         height: 50px;
       }
      </style>
      <div id='container'>
        <div id='child'></div>
      </div>
    )HTML");

  const PhysicalBoxFragment* body_fragment = nullptr;
  const PhysicalBoxFragment* container_fragment = nullptr;
  const PhysicalBoxFragment* child_fragment = nullptr;
  const PhysicalBoxFragment* fragment = nullptr;
  auto run_test = [&](const Length& container_height) {
    UpdateStyleForElement(GetElementById("container"),
                          [&](ComputedStyleBuilder& builder) {
                            builder.SetHeight(container_height);
                          });
    fragment = GetHtmlPhysicalFragment();
    ASSERT_EQ(1UL, fragment->Children().size());
    body_fragment = To<PhysicalBoxFragment>(fragment->Children()[0].get());
    container_fragment =
        To<PhysicalBoxFragment>(body_fragment->Children()[0].get());
    ASSERT_EQ(1UL, container_fragment->Children().size());
    child_fragment =
        To<PhysicalBoxFragment>(container_fragment->Children()[0].get());
  };

  // height == auto
  run_test(Length::Auto());
  // Margins are collapsed with the result 200 = std::max(20, 200)
  // The fragment size 258 == body's margin 8 + child's height 50 + 200
  EXPECT_EQ(PhysicalSize(800, 258), fragment->Size());

  // height == fixed
  run_test(Length::Fixed(50));
  // Margins are not collapsed, so fragment still has margins == 20.
  // The fragment size 78 == body's margin 8 + child's height 50 + 20
  EXPECT_EQ(PhysicalSize(800, 78), fragment->Size());
}

// Verifies that 2 adjoining margins are not collapsed if there is padding or
// border that separates them.
TEST_F(BlockLayoutAlgorithmTest, CollapsingMarginsCase4) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          margin: 30px 0px;
          width: 200px;
        }
        #child {
         margin: 200px 0px;
          height: 50px;
          background-color: blue;
        }
      </style>
      <div id='container'>
        <div id='child'></div>
      </div>
    )HTML");

  PhysicalOffset body_offset;
  PhysicalOffset container_offset;
  PhysicalOffset child_offset;
  const PhysicalBoxFragment* fragment = nullptr;
  auto run_test = [&](const Length& container_padding_top) {
    UpdateStyleForElement(GetElementById("container"),
                          [&](ComputedStyleBuilder& builder) {
                            builder.SetPaddingTop(container_padding_top);
                          });
    fragment = GetHtmlPhysicalFragment();
    ASSERT_EQ(1UL, fragment->Children().size());
    const auto* body_fragment =
        To<PhysicalBoxFragment>(fragment->Children()[0].get());
    body_offset = fragment->Children()[0].Offset();
    const auto* container_fragment =
        To<PhysicalBoxFragment>(body_fragment->Children()[0].get());
    container_offset = body_fragment->Children()[0].Offset();
    ASSERT_EQ(1UL, container_fragment->Children().size());
    child_offset = container_fragment->Children()[0].Offset();
  };

  // with padding
  run_test(Length::Fixed(20));
  // 500 = child's height 50 + 2xmargin 400 + paddint-top 20 +
  // container's margin 30
  EXPECT_EQ(PhysicalSize(800, 500), fragment->Size());
  // 30 = max(body's margin 8, container margin 30)
  EXPECT_EQ(LayoutUnit(30), body_offset.top);
  // 220 = container's padding top 20 + child's margin
  EXPECT_EQ(LayoutUnit(220), child_offset.top);

  // without padding
  run_test(Length::Fixed(0));
  // 450 = 2xmax(body's margin 8, container's margin 30, child's margin 200) +
  //       child's height 50
  EXPECT_EQ(PhysicalSize(800, 450), fragment->Size());
  // 200 = (body's margin 8, container's margin 30, child's margin 200)
  EXPECT_EQ(LayoutUnit(200), body_offset.top);
  // 0 = collapsed margins
  EXPECT_EQ(LayoutUnit(0), child_offset.top);
}

// Verifies that margins of 2 adjoining blocks with different writing modes
// get collapsed.
TEST_F(BlockLayoutAlgorithmTest, CollapsingMarginsCase5) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          margin-top: 10px;
          writing-mode: vertical-lr;
        }
        #vertical {
          margin-right: 90px;
          background-color: red;
          height: 70px;
          width: 30px;
        }
        #horizontal {
         background-color: blue;
          margin-left: 100px;
          writing-mode: horizontal-tb;
          height: 60px;
          width: 30px;
        }
      </style>
      <div id='container'>
        <div id='vertical'></div>
        <div id='horizontal'></div>
      </div>
    )HTML");
  const auto* fragment = GetHtmlPhysicalFragment();

  // body
  auto* body_fragment = To<PhysicalBoxFragment>(fragment->Children()[0].get());
  PhysicalOffset body_offset = fragment->Children()[0].Offset();
  // 10 = std::max(body's margin 8, container's margin top)
  int body_top_offset = 10;
  EXPECT_THAT(body_offset.top, LayoutUnit(body_top_offset));
  int body_left_offset = 8;
  EXPECT_THAT(body_offset.left, LayoutUnit(body_left_offset));

  // height = 70. std::max(vertical height's 70, horizontal's height's 60)
  ASSERT_EQ(PhysicalSize(784, 70), body_fragment->Size());
  ASSERT_EQ(1UL, body_fragment->Children().size());

  // container
  auto* container_fragment =
      To<PhysicalBoxFragment>(body_fragment->Children()[0].get());
  PhysicalOffset container_offset = body_fragment->Children()[0].Offset();
  // Container's margins are collapsed with body's fragment.
  EXPECT_THAT(container_offset.top, LayoutUnit());
  EXPECT_THAT(container_offset.left, LayoutUnit());
  ASSERT_EQ(2UL, container_fragment->Children().size());

  // vertical
  PhysicalOffset vertical_offset = container_fragment->Children()[0].Offset();
  EXPECT_THAT(vertical_offset.top, LayoutUnit());
  EXPECT_THAT(vertical_offset.left, LayoutUnit());

  // horizontal
  PhysicalOffset orizontal_offset = container_fragment->Children()[1].Offset();
  EXPECT_THAT(orizontal_offset.top, LayoutUnit());
  // 130 = vertical's width 30 +
  //       std::max(vertical's margin right 90, horizontal's margin-left 100)
  EXPECT_THAT(orizontal_offset.left, LayoutUnit(130));
}

// Verifies that margins collapsing logic works with Layout Inline.
TEST_F(BlockLayoutAlgorithmTest, CollapsingMarginsWithText) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
          margin: 10px;
        }
        p {
          margin: 20px;
        }
      </style>
      <p>Some text</p>
    )HTML");
  const auto* html_fragment = GetHtmlPhysicalFragment();

  const auto* body_fragment =
      To<PhysicalBoxFragment>(html_fragment->Children()[0].get());
  PhysicalOffset body_offset = html_fragment->Children()[0].Offset();
  // 20 = std::max(body's margin, p's margin)
  EXPECT_THAT(body_offset, PhysicalOffset(10, 20));

  PhysicalOffset p_offset = body_fragment->Children()[0].Offset();
  // Collapsed margins with result = 0.
  EXPECT_THAT(p_offset, PhysicalOffset(20, 0));
}

// Verifies that the margin strut of a child with a different writing mode does
// not get used in the collapsing margins calculation.
TEST_F(BlockLayoutAlgorithmTest, CollapsingMarginsCase6) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #div1 {
        margin-bottom: 10px;
        width: 10px;
        height: 60px;
        writing-mode: vertical-rl;
      }
      #div2 { margin-left: -20px; width: 10px; }
      #div3 { margin-top: 40px; height: 60px; }
    </style>
    <div id="container" style="width:500px;height:500px">
      <div id="div1">
         <div id="div2">vertical</div>
      </div>
      <div id="div3"></div>
    </div>
  )HTML");
  const int kHeight = 60;
  const int kMarginBottom = 10;
  const int kMarginTop = 40;

  BlockNode container(GetLayoutBoxByElementId("container"));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(500), LayoutUnit(500)));
  const PhysicalBoxFragment* fragment =
      RunBlockLayoutAlgorithm(container, space);

  ASSERT_EQ(fragment->Children().size(), 2UL);

  const PhysicalFragment* child1 = fragment->Children()[0].get();
  PhysicalOffset child1_offset = fragment->Children()[0].Offset();
  EXPECT_EQ(0, child1_offset.top);
  EXPECT_EQ(kHeight, child1->Size().height);

  PhysicalOffset child2_offset = fragment->Children()[1].Offset();
  EXPECT_EQ(kHeight + std::max(kMarginBottom, kMarginTop), child2_offset.top);
}

// Verifies that a child with clearance - which does nothing - still shifts its
// parent's offset.
TEST_F(BlockLayoutAlgorithmTest, CollapsingMarginsCase7) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      outline: solid purple 1px;
      width: 200px;
    }
    #zero {
      outline: solid red 1px;
      margin-top: 10px;
    }
    #float {
      background: yellow;
      float: right;
      width: 20px;
      height: 20px;
    }
    #inflow {
      background: blue;
      clear: left;
      height: 20px;
      margin-top: 20px;
    }
    </style>
    <div id="zero">
      <div id="float"></div>
    </div>
    <div id="inflow"></div>
  )HTML");

  const auto* fragment = GetHtmlPhysicalFragment();
  FragmentChildIterator iterator(fragment);

  // body
  PhysicalOffset offset;
  const PhysicalBoxFragment* child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(200, 20), child->Size());
  EXPECT_EQ(PhysicalOffset(8, 20), offset);

  // #zero
  iterator.SetParent(child);
  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(200, 0), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 0), offset);

  // #inflow
  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(200, 20), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 0), offset);
}

// An empty block level element (with margins collapsing through it) has
// non-trivial behavior with margins collapsing.
TEST_F(BlockLayoutAlgorithmTest, CollapsingMarginsEmptyBlockWithClearance) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      position: relative;
      outline: solid purple 1px;
      display: flow-root;
      width: 200px;
    }
    #float {
      background: orange;
      float: left;
      width: 50px;
      height: 50px;
    }
    #zero {
      outline: solid red 1px;
      clear: left;
    }
    #abs {
      background: cyan;
      position: absolute;
      width: 20px;
      height: 20px;
    }
    #inflow {
      background: green;
      height: 20px;
    }
    </style>
    <div id="float"></div>
    <div id="zero-top"></div>
    <div id="zero">
      <!-- This exists to produce complex margin struts. -->
      <div id="zero-inner"></div>
    </div>
    <div id="abs"></div>
    <div id="inflow"></div>
  )HTML");

  const LayoutBlockFlow* zero;
  const LayoutBlockFlow* abs;
  const LayoutBlockFlow* inflow;
  auto run_test = [&](const Length& zero_top_margin_bottom,
                      const Length& zero_inner_margin_top,
                      const Length& zero_inner_margin_bottom,
                      const Length& zero_margin_bottom,
                      const Length& inflow_margin_top) {
    // Set the style of the elements we care about.
    UpdateStyleForElement(GetElementById("zero-top"),
                          [&](ComputedStyleBuilder& builder) {
                            builder.SetMarginBottom(zero_top_margin_bottom);
                          });
    UpdateStyleForElement(GetElementById("zero-inner"),
                          [&](ComputedStyleBuilder& builder) {
                            builder.SetMarginTop(zero_inner_margin_top);
                            builder.SetMarginBottom(zero_inner_margin_bottom);
                          });
    UpdateStyleForElement(GetElementById("zero"),
                          [&](ComputedStyleBuilder& builder) {
                            builder.SetMarginBottom(zero_margin_bottom);
                          });
    UpdateStyleForElement(GetElementById("inflow"),
                          [&](ComputedStyleBuilder& builder) {
                            builder.SetMarginTop(inflow_margin_top);
                          });
    UpdateAllLifecyclePhasesForTest();

    LayoutBlockFlow* child;
    // #float
    child = To<LayoutBlockFlow>(GetLayoutObjectByElementId("float"));
    EXPECT_EQ(PhysicalSize(LayoutUnit(50), LayoutUnit(50)), child->Size());
    EXPECT_EQ(PhysicalOffset(0, 0), child->PhysicalLocation());

    // We need to manually test the position of #zero, #abs, #inflow.
    zero = To<LayoutBlockFlow>(GetLayoutObjectByElementId("zero"));
    inflow = To<LayoutBlockFlow>(GetLayoutObjectByElementId("inflow"));
    abs = To<LayoutBlockFlow>(GetLayoutObjectByElementId("abs"));
  };

  // Base case of no margins.
  run_test(
      /* #zero-top margin-bottom */ Length::Fixed(0),
      /* #zero-inner margin-top */ Length::Fixed(0),
      /* #zero-inner margin-bottom */ Length::Fixed(0),
      /* #zero margin-bottom */ Length::Fixed(0),
      /* #inflow margin-top */ Length::Fixed(0));

  // #zero, #abs, #inflow should all be positioned at the float.
  EXPECT_EQ(LayoutUnit(50), zero->PhysicalLocation().top);
  EXPECT_EQ(LayoutUnit(50), abs->PhysicalLocation().top);
  EXPECT_EQ(LayoutUnit(50), inflow->PhysicalLocation().top);

  // A margin strut which resolves to -50 (-70 + 20) adjusts the position of
  // #zero to the float clearance.
  run_test(
      /* #zero-top margin-bottom */ Length::Fixed(0),
      /* #zero-inner margin-top */ Length::Fixed(-60),
      /* #zero-inner margin-bottom */ Length::Fixed(20),
      /* #zero margin-bottom */ Length::Fixed(-70),
      /* #inflow margin-top */ Length::Fixed(50));

  // #zero is placed at the float, the margin strut is at:
  // 90 = (50 - (-60 + 20)).
  EXPECT_EQ(LayoutUnit(50), zero->PhysicalLocation().top);

  // #abs estimates its position with the margin strut:
  // 40 = (90 + (-70 + 20)).
  EXPECT_EQ(LayoutUnit(40), abs->PhysicalLocation().top);

  // #inflow has similar behavior to #abs, but includes its margin.
  // 70 = (90 + (-70 + 50))
  EXPECT_EQ(LayoutUnit(70), inflow->PhysicalLocation().top);

  // A margin strut which resolves to 60 (-10 + 70) means that #zero doesn't
  // get adjusted to clear the float, and we have normal behavior.
  //
  // NOTE: This case below has wildly different results on different browsers,
  // we may have to change the behavior here in the future for web compat.
  run_test(
      /* #zero-top margin-bottom */ Length::Fixed(0),
      /* #zero-inner margin-top */ Length::Fixed(70),
      /* #zero-inner margin-bottom */ Length::Fixed(-10),
      /* #zero margin-bottom */ Length::Fixed(-20),
      /* #inflow margin-top */ Length::Fixed(80));

  // #zero is placed at 60 (-10 + 70).
  EXPECT_EQ(LayoutUnit(60), zero->PhysicalLocation().top);

  // #abs estimates its position with the margin strut:
  // 50 = (0 + (-20 + 70)).
  EXPECT_EQ(LayoutUnit(50), abs->PhysicalLocation().top);

  // #inflow has similar behavior to #abs, but includes its margin.
  // 60 = (0 + (-20 + 80))
  EXPECT_EQ(LayoutUnit(60), inflow->PhysicalLocation().top);

  // #zero-top produces a margin which needs to be ignored, as #zero is
  // affected by clearance, it needs to have layout performed again, starting
  // with an empty margin strut.
  run_test(
      /* #zero-top margin-bottom */ Length::Fixed(30),
      /* #zero-inner margin-top */ Length::Fixed(20),
      /* #zero-inner margin-bottom */ Length::Fixed(-10),
      /* #zero margin-bottom */ Length::Fixed(0),
      /* #inflow margin-top */ Length::Fixed(25));

  // #zero is placed at the float, the margin strut is at:
  // 40 = (50 - (-10 + 20)).
  EXPECT_EQ(LayoutUnit(50), zero->PhysicalLocation().top);

  // The margin strut is now disjoint, this is placed at:
  // 55 = (40 + (-10 + 25))
  EXPECT_EQ(LayoutUnit(55), inflow->PhysicalLocation().top);
}

// Tests that when auto margins are applied to a new formatting context, they
// are applied within the layout opportunity.
TEST_F(BlockLayoutAlgorithmTest, NewFormattingContextAutoMargins) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container { width: 200px; direction: rtl; display: flow-root; }
        #float { width: 100px; height: 60px; background: hotpink; float: left; }
        #newfc { direction: rtl; width: 50px; height: 20px; background: green; overflow: hidden; }
      </style>
      <div id="container">
        <div id="float"></div>
        <div id="newfc" style="margin-right: auto;"></div>
        <div id="newfc" style="margin-left: auto; margin-right: auto;"></div>
        <div id="newfc" style="margin-left: auto;"></div>
      </div>
    )HTML");

  const auto* fragment =
      &To<PhysicalBoxFragment>(GetLayoutBoxByElementId("container")
                                   ->GetSingleCachedLayoutResult()
                                   ->GetPhysicalFragment());

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:200x60
    offset:0,0 size:100x60
    offset:100,0 size:50x20
    offset:125,20 size:50x20
    offset:150,40 size:50x20
)DUMP";
  EXPECT_EQ(expectation, DumpFragmentTree(fragment));
}

// Verifies that a box's size includes its borders and padding, and that
// children are positioned inside the content box.
TEST_F(BlockLayoutAlgorithmTest, BorderAndPadding) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #div1 {
        width: 100px;
        height: 100px;
        border-style: solid;
        border-width: 1px 2px 3px 4px;
        padding: 5px 6px 7px 8px;
      }
    </style>
    <div id="container">
      <div id="div1">
         <div id="div2"></div>
      </div>
    </div>
  )HTML");
  const int kWidth = 100;
  const int kHeight = 100;
  const int kBorderTop = 1;
  const int kBorderRight = 2;
  const int kBorderBottom = 3;
  const int kBorderLeft = 4;
  const int kPaddingTop = 5;
  const int kPaddingRight = 6;
  const int kPaddingBottom = 7;
  const int kPaddingLeft = 8;

  BlockNode container(GetLayoutBoxByElementId("container"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize));

  const PhysicalBoxFragment* fragment =
      RunBlockLayoutAlgorithm(container, space);

  ASSERT_EQ(fragment->Children().size(), 1UL);

  // div1
  const PhysicalFragment* child = fragment->Children()[0].get();
  EXPECT_EQ(kBorderLeft + kPaddingLeft + kWidth + kPaddingRight + kBorderRight,
            child->Size().width);
  EXPECT_EQ(kBorderTop + kPaddingTop + kHeight + kPaddingBottom + kBorderBottom,
            child->Size().height);

  ASSERT_TRUE(child->IsBox());
  ASSERT_EQ(static_cast<const PhysicalBoxFragment*>(child)->Children().size(),
            1UL);

  PhysicalOffset div2_offset =
      static_cast<const PhysicalBoxFragment*>(child)->Children()[0].Offset();
  EXPECT_EQ(kBorderTop + kPaddingTop, div2_offset.top);
  EXPECT_EQ(kBorderLeft + kPaddingLeft, div2_offset.left);
}

TEST_F(BlockLayoutAlgorithmTest, PercentageResolutionSize) {
  SetBodyInnerHTML(R"HTML(
    <div id="container" style="width: 30px; padding-left: 10px">
      <div id="div1" style="width: 40%"></div>
    </div>
  )HTML");
  const int kPaddingLeft = 10;
  const int kWidth = 30;

  BlockNode container(GetLayoutBoxByElementId("container"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), kIndefiniteSize));
  const PhysicalBoxFragment* fragment =
      RunBlockLayoutAlgorithm(container, space);

  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), fragment->Size().width);
  EXPECT_EQ(PhysicalFragment::kFragmentBox, fragment->Type());
  ASSERT_EQ(fragment->Children().size(), 1UL);

  const PhysicalFragment* child = fragment->Children()[0].get();
  EXPECT_EQ(LayoutUnit(12), child->Size().width);
}

// A very simple auto margin case. We rely on the tests in length_utils_test
// for the more complex cases; just make sure we handle auto at all here.
TEST_F(BlockLayoutAlgorithmTest, AutoMargin) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #first { width: 10px; margin-left: auto; margin-right: auto; }
    </style>
    <div id="container" style="width: 30px; padding-left: 10px">
      <div id="first">
      </div>
    </div>
  )HTML");
  const int kPaddingLeft = 10;
  const int kWidth = 30;
  const int kChildWidth = 10;

  BlockNode container(GetLayoutBoxByElementId("container"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), kIndefiniteSize));
  const PhysicalBoxFragment* fragment =
      RunBlockLayoutAlgorithm(container, space);

  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), fragment->Size().width);
  EXPECT_EQ(PhysicalFragment::kFragmentBox, fragment->Type());
  ASSERT_EQ(1UL, fragment->Children().size());

  const PhysicalFragment* child = fragment->Children()[0].get();
  PhysicalOffset child_offset = fragment->Children()[0].Offset();
  EXPECT_EQ(LayoutUnit(kChildWidth), child->Size().width);
  EXPECT_EQ(LayoutUnit(kPaddingLeft + 10), child_offset.left);
  EXPECT_EQ(LayoutUnit(0), child_offset.top);
}

// Verifies that floats can be correctly positioned if they are inside of nested
// empty blocks.
TEST_F(BlockLayoutAlgorithmTest, PositionFloatInsideEmptyBlocks) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 300px;
          width: 300px;
          outline: blue solid;
        }
        #empty1 {
          margin: 20px;
          padding: 0 20px;
        }
        #empty2 {
          margin: 15px;
          padding: 0 15px;
        }
        #left-float {
          float: left;
          height: 5px;
          width: 5px;
          padding: 10px;
          margin: 10px;
          background-color: green;
        }
        #right-float {
          float: right;
          height: 15px;
          width: 15px;
          margin: 15px 10px;
          background-color: red;
        }
      </style>
      <div id='container'>
        <div id='empty1'>
          <div id='empty2'>
            <div id='left-float'></div>
            <div id='right-float'></div>
          </div>
        </div>
      </div>
    )HTML");

  const auto* fragment = GetHtmlPhysicalFragment();
  const auto* body_fragment =
      To<PhysicalBoxFragment>(fragment->Children()[0].get());
  PhysicalOffset body_offset = fragment->Children()[0].Offset();
  FragmentChildIterator iterator(body_fragment);
  // 20 = std::max(empty1's margin, empty2's margin, body's margin)
  int body_top_offset = 20;
  EXPECT_THAT(body_offset.top, LayoutUnit(body_top_offset));
  ASSERT_EQ(1UL, body_fragment->Children().size());

  const auto* container_fragment = iterator.NextChild();
  ASSERT_EQ(1UL, container_fragment->Children().size());

  iterator.SetParent(container_fragment);
  PhysicalOffset offset;
  const auto* empty1_fragment = iterator.NextChild(&offset);
  // 0, vertical margins got collapsed
  EXPECT_THAT(offset.top, LayoutUnit());
  // 20 empty1's margin
  EXPECT_THAT(offset.left, LayoutUnit(20));
  ASSERT_EQ(empty1_fragment->Children().size(), 1UL);

  iterator.SetParent(empty1_fragment);
  const auto* empty2_fragment = iterator.NextChild(&offset);
  // 0, vertical margins got collapsed
  EXPECT_THAT(LayoutUnit(), offset.top);
  // 35 = empty1's padding(20) + empty2's padding(15)
  EXPECT_THAT(offset.left, LayoutUnit(35));

  offset = empty2_fragment->Children()[0].offset;
  // inline 25 = left float's margin(10) + empty2's padding(15).
  // block 10 = left float's margin
  EXPECT_THAT(offset, PhysicalOffset(25, 10));

  offset = empty2_fragment->Children()[1].offset;
  // inline offset 140 = right float's margin(10) + right float offset(140)
  // block offset 15 = right float's margin
  LayoutUnit right_float_offset = LayoutUnit(140);
  EXPECT_THAT(offset, PhysicalOffset(LayoutUnit(10) + right_float_offset,
                                     LayoutUnit(15)));

  // ** Verify layout tree **
  Element* left_float = GetElementById("left-float");
  // 88 = body's margin(8) +
  // empty1's padding and margin + empty2's padding and margins + float's
  // padding
  EXPECT_THAT(left_float->OffsetLeft(), 88);
  // 30 = body_top_offset(collapsed margins result) + float's padding
  EXPECT_THAT(left_float->OffsetTop(), body_top_offset + 10);
}

// Verifies that left/right floating and regular blocks can be positioned
// correctly by the algorithm.
TEST_F(BlockLayoutAlgorithmTest, PositionFloatFragments) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 200px;
          width: 200px;
        }
        #left-float {
          background-color: red;
          float: left;
          height: 30px;
          width: 30px;
        }
        #left-wide-float {
          background-color: greenyellow;
          float: left;
          height: 30px;
          width: 180px;
        }
        #regular {
          width: 40px;
          height: 40px;
          background-color: green;
        }
        #right-float {
          background-color: cyan;
          float: right;
          width: 50px;
          height: 50px;
        }
        #left-float-with-margin {
          background-color: black;
          float: left;
          height: 120px;
          margin: 10px;
          width: 120px;
        }
      </style>
      <div id='container'>
        <div id='left-float'></div>
        <div id='left-wide-float'></div>
        <div id='regular'></div>
        <div id='right-float'></div>
        <div id='left-float-with-margin'></div>
      </div>
      )HTML");

  const auto* fragment = GetHtmlPhysicalFragment();

  // ** Verify LayoutNG fragments and the list of positioned floats **
  ASSERT_EQ(1UL, fragment->Children().size());
  const auto* body_fragment =
      To<PhysicalBoxFragment>(fragment->Children()[0].get());
  PhysicalOffset body_offset = fragment->Children()[0].Offset();
  EXPECT_THAT(LayoutUnit(8), body_offset.top);

  FragmentChildIterator iterator(body_fragment);
  const auto* container_fragment = iterator.NextChild();
  ASSERT_EQ(5UL, container_fragment->Children().size());

  // ** Verify layout tree **
  Element* left_float = GetElementById("left-float");
  // 8 = body's margin-top
  EXPECT_EQ(8, left_float->OffsetTop());

  iterator.SetParent(container_fragment);
  PhysicalOffset offset;
  iterator.NextChild(&offset);
  EXPECT_THAT(LayoutUnit(), offset.top);

  Element* left_wide_float = GetElementById("left-wide-float");
  // left-wide-float is positioned right below left-float as it's too wide.
  // 38 = left_float_block_offset 8 +
  //      left-float's height 30
  EXPECT_EQ(38, left_wide_float->OffsetTop());

  iterator.NextChild(&offset);
  // 30 = left-float's height.
  EXPECT_THAT(LayoutUnit(30), offset.top);

  Element* regular = GetElementById("regular");
  // regular_block_offset = body's margin-top 8
  EXPECT_EQ(8, regular->OffsetTop());

  iterator.NextChild(&offset);
  EXPECT_THAT(LayoutUnit(), offset.top);

  Element* right_float = GetElementById("right-float");
  // 158 = body's margin-left 8 + container's width 200 - right_float's width 50
  // it's positioned right after our left_wide_float
  // 68 = left_wide_float_block_offset 38 + left-wide-float's height 30
  EXPECT_EQ(158, right_float->OffsetLeft());
  EXPECT_EQ(68, right_float->OffsetTop());

  iterator.NextChild(&offset);
  // 60 = right_float_block_offset(68) - body's margin(8)
  EXPECT_THAT(LayoutUnit(60), offset.top);
  // 150 = right_float_inline_offset(158) - body's margin(8)
  EXPECT_THAT(LayoutUnit(150), offset.left);

  Element* left_float_with_margin = GetElementById("left-float-with-margin");
  // 18 = body's margin(8) + left-float-with-margin's margin(10)
  EXPECT_EQ(18, left_float_with_margin->OffsetLeft());
  // 78 = left_wide_float_block_offset 38 + left-wide-float's height 30 +
  //      left-float-with-margin's margin(10)
  EXPECT_EQ(78, left_float_with_margin->OffsetTop());

  iterator.NextChild(&offset);
  // 70 = left_float_with_margin_block_offset(78) - body's margin(8)
  EXPECT_THAT(LayoutUnit(70), offset.top);
  // 10 = left_float_with_margin_inline_offset(18) - body's margin(8)
  EXPECT_THAT(LayoutUnit(10), offset.left);
}

// Verifies that NG block layout algorithm respects "clear" CSS property.
TEST_F(BlockLayoutAlgorithmTest, PositionFragmentsWithClear) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 200px;
          width: 200px;
        }
        #float-left {
          background-color: red;
          float: left;
          height: 30px;
          width: 30px;
        }
        #float-right {
          background-color: blue;
          float: right;
          height: 170px;
          width: 40px;
        }
        #clearance {
          background-color: yellow;
          height: 60px;
          width: 60px;
          margin: 20px;
        }
        #block {
          margin: 40px;
          background-color: black;
          height: 60px;
          width: 60px;
        }
        #adjoining-clearance {
          background-color: green;
          clear: left;
          height: 20px;
          width: 20px;
          margin: 30px;
        }
      </style>
      <div id='container'>
        <div id='float-left'></div>
        <div id='float-right'></div>
        <div id='clearance'></div>
        <div id='block'></div>
        <div id='adjoining-clearance'></div>
      </div>
    )HTML");

  PhysicalOffset clerance_offset;
  PhysicalOffset body_offset;
  PhysicalOffset container_offset;
  PhysicalOffset block_offset;
  PhysicalOffset adjoining_clearance_offset;
  auto run_with_clearance = [&](EClear clear_value) {
    UpdateStyleForElement(
        GetElementById("clearance"),
        [&](ComputedStyleBuilder& builder) { builder.SetClear(clear_value); });
    const auto* fragment = GetHtmlPhysicalFragment();
    ASSERT_EQ(1UL, fragment->Children().size());
    const auto* body_fragment =
        To<PhysicalBoxFragment>(fragment->Children()[0].get());
    body_offset = fragment->Children()[0].Offset();
    const auto* container_fragment =
        To<PhysicalBoxFragment>(body_fragment->Children()[0].get());
    ASSERT_EQ(5UL, container_fragment->Children().size());
    container_offset = body_fragment->Children()[0].Offset();
    clerance_offset = container_fragment->Children()[2].Offset();
    block_offset = container_fragment->Children()[3].Offset();
    adjoining_clearance_offset = container_fragment->Children()[4].Offset();
  };

  // clear: none
  run_with_clearance(EClear::kNone);
  // 20 = std::max(body's margin 8, clearance's margins 20)
  EXPECT_EQ(LayoutUnit(20), body_offset.top);
  EXPECT_EQ(LayoutUnit(0), container_offset.top);
  // 0 = collapsed margins
  EXPECT_EQ(LayoutUnit(0), clerance_offset.top);
  // 100 = clearance's height 60 +
  //       std::max(clearance's margins 20, block's margins 40)
  EXPECT_EQ(LayoutUnit(100), block_offset.top);
  // 200 = 100 + block's height 60 + max(adjoining_clearance's margins 30,
  //                                     block's margins 40)
  EXPECT_EQ(LayoutUnit(200), adjoining_clearance_offset.top);

  // clear: right
  run_with_clearance(EClear::kRight);
  // 8 = body's margin. This doesn't collapse its margins with 'clearance' block
  // as it's not an adjoining block to body.
  EXPECT_EQ(LayoutUnit(8), body_offset.top);
  EXPECT_EQ(LayoutUnit(0), container_offset.top);
  // 170 = float-right's height
  EXPECT_EQ(LayoutUnit(170), clerance_offset.top);
  // 270 = float-right's height + clearance's height 60 +
  //       max(clearance's margin 20, block margin 40)
  EXPECT_EQ(LayoutUnit(270), block_offset.top);
  // 370 = block's offset 270 + block's height 60 +
  //       std::max(block's margin 40, adjoining_clearance's margin 30)
  EXPECT_EQ(LayoutUnit(370), adjoining_clearance_offset.top);

  // clear: left
  run_with_clearance(EClear::kLeft);
  // 8 = body's margin. This doesn't collapse its margins with 'clearance' block
  // as it's not an adjoining block to body.
  EXPECT_EQ(LayoutUnit(8), body_offset.top);
  EXPECT_EQ(LayoutUnit(0), container_offset.top);
  // 30 = float_left's height
  EXPECT_EQ(LayoutUnit(30), clerance_offset.top);
  // 130 = float_left's height + clearance's height 60 +
  //       max(clearance's margin 20, block margin 40)
  EXPECT_EQ(LayoutUnit(130), block_offset.top);
  // 230 = block's offset 130 + block's height 60 +
  //       std::max(block's margin 40, adjoining_clearance's margin 30)
  EXPECT_EQ(LayoutUnit(230), adjoining_clearance_offset.top);

  // clear: both
  // same as clear: right
  run_with_clearance(EClear::kBoth);
  EXPECT_EQ(LayoutUnit(8), body_offset.top);
  EXPECT_EQ(LayoutUnit(0), container_offset.top);
  EXPECT_EQ(LayoutUnit(170), clerance_offset.top);
  EXPECT_EQ(LayoutUnit(270), block_offset.top);
  EXPECT_EQ(LayoutUnit(370), adjoining_clearance_offset.top);
}

// Verifies that we compute the right min and max-content size.
TEST_F(BlockLayoutAlgorithmTest, ComputeMinMaxContent) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="first-child" style="width: 20px"></div>
      <div id="second-child" style="width: 30px"></div>
    </div>
  )HTML");

  const int kSecondChildWidth = 30;

  BlockNode container(GetLayoutBoxByElementId("container"));

  MinMaxSizes sizes = RunComputeMinMaxSizes(container);
  EXPECT_EQ(kSecondChildWidth, sizes.min_size);
  EXPECT_EQ(kSecondChildWidth, sizes.max_size);
}

TEST_F(BlockLayoutAlgorithmTest, ComputeMinMaxContentFloats) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f1 { float: left; width: 20px; }
      #f2 { float: left; width: 30px; }
      #f3 { float: right; width: 40px; }
    </style>
    <div id="container">
      <div id="f1"></div>
      <div id="f2"></div>
      <div id="f3"></div>
    </div>
  )HTML");

  BlockNode container(GetLayoutBoxByElementId("container"));

  MinMaxSizes sizes = RunComputeMinMaxSizes(container);
  EXPECT_EQ(LayoutUnit(40), sizes.min_size);
  EXPECT_EQ(LayoutUnit(90), sizes.max_size);
}

TEST_F(BlockLayoutAlgorithmTest, ComputeMinMaxContentFloatsClearance) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f1 { float: left; width: 20px; }
      #f2 { float: left; width: 30px; }
      #f3 { float: right; width: 40px; clear: left; }
    </style>
    <div id="container">
      <div id="f1"></div>
      <div id="f2"></div>
      <div id="f3"></div>
    </div>
  )HTML");

  BlockNode container(GetLayoutBoxByElementId("container"));

  MinMaxSizes sizes = RunComputeMinMaxSizes(container);
  EXPECT_EQ(LayoutUnit(40), sizes.min_size);
  EXPECT_EQ(LayoutUnit(50), sizes.max_size);
}

TEST_F(BlockLayoutAlgorithmTest, ComputeMinMaxContentNewFormattingContext) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f1 { float: left; width: 20px; }
      #f2 { float: left; width: 30px; }
      #fc { display: flex; width: 40px; margin-left: 60px; }
    </style>
    <div id="container">
      <div id="f1"></div>
      <div id="f2"></div>
      <div id="fc"></div>
    </div>
  )HTML");

  BlockNode container(GetLayoutBoxByElementId("container"));

  MinMaxSizes sizes = RunComputeMinMaxSizes(container);
  EXPECT_EQ(LayoutUnit(100), sizes.min_size);
  EXPECT_EQ(LayoutUnit(100), sizes.max_size);
}

TEST_F(BlockLayoutAlgorithmTest,
       ComputeMinMaxContentNewFormattingContextNegativeMargins) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f1 { float: left; width: 20px; }
      #f2 { float: left; width: 30px; }
      #fc { display: flex; width: 40px; margin-left: -20px; }
    </style>
    <div id="container">
      <div id="f1"></div>
      <div id="f2"></div>
      <div id="fc"></div>
    </div>
  )HTML");

  BlockNode container(GetLayoutBoxByElementId("container"));

  MinMaxSizes sizes = RunComputeMinMaxSizes(container);
  EXPECT_EQ(LayoutUnit(30), sizes.min_size);
  EXPECT_EQ(LayoutUnit(70), sizes.max_size);
}

TEST_F(BlockLayoutAlgorithmTest,
       ComputeMinMaxContentSingleNewFormattingContextNegativeMargins) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #fc { display: flex; width: 20px; margin-left: -40px; }
    </style>
    <div id="container">
      <div id="fc"></div>
    </div>
  )HTML");

  BlockNode container(GetLayoutBoxByElementId("container"));

  MinMaxSizes sizes = RunComputeMinMaxSizes(container);
  EXPECT_EQ(LayoutUnit(), sizes.min_size);
  EXPECT_EQ(LayoutUnit(), sizes.max_size);
}

// Tests that we correctly handle shrink-to-fit
TEST_F(BlockLayoutAlgorithmTest, ShrinkToFit) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="first-child" style="width: 20px"></div>
      <div id="second-child" style="width: 30px"></div>
    </div>
  )HTML");
  const int kWidthChild2 = 30;

  BlockNode container(GetLayoutBoxByElementId("container"));

  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ false);
  const PhysicalBoxFragment* fragment =
      RunBlockLayoutAlgorithm(container, space);

  EXPECT_EQ(LayoutUnit(kWidthChild2), fragment->Size().width);
}

// Verifies that we position empty blocks and floats correctly inside of the
// block that establishes new BFC.
TEST_F(BlockLayoutAlgorithmTest, PositionEmptyBlocksInNewBfc) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        overflow: hidden;
      }
      #empty-block1 {
        margin: 8px;
      }
      #left-float {
        float: left;
        background: red;
        height: 20px;
        width: 10px;
        margin: 15px;
      }
      #empty-block2 {
        margin-top: 50px;
      }
    </style>
    <div id="container">
      <div id="left-float"></div>
      <div id="empty-block1"></div>
      <div id="empty-block2"></div>
    </div>
  )HTML");

  const auto* html_fragment = GetHtmlPhysicalFragment();
  auto* body_fragment =
      To<PhysicalBoxFragment>(html_fragment->Children()[0].get());
  auto* container_fragment =
      To<PhysicalBoxFragment>(body_fragment->Children()[0].get());
  PhysicalOffset empty_block1_offset =
      container_fragment->Children()[1].Offset();
  // empty-block1's margin == 8
  EXPECT_THAT(empty_block1_offset, PhysicalOffset(8, 8));

  PhysicalOffset empty_block2_offset =
      container_fragment->Children()[2].Offset();
  // empty-block2's margin == 50
  EXPECT_THAT(empty_block2_offset, PhysicalOffset(0, 50));
}

// Verifies that we can correctly position blocks with clearance and
// intruding floats.
TEST_F(BlockLayoutAlgorithmTest,
       PositionBlocksWithClearanceAndIntrudingFloats) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    body { margin: 80px; }
    #left-float {
      background: green;
      float: left;
      width: 50px;
      height: 50px;
    }
    #right-float {
      background: red;
      float: right;
      margin: 0 80px 0 10px;
      width: 50px;
      height: 80px;
    }
    #block1 {
      outline: purple solid;
      height: 30px;
      margin: 130px 0 20px 0;
    }
    #zero {
     margin-top: 30px;
    }
    #container-clear {
      clear: left;
      outline: orange solid;
    }
    #clears-right {
      clear: right;
      height: 20px;
      background: lightblue;
    }
    </style>

    <div id="left-float"></div>
    <div id="right-float"></div>
    <div id="block1"></div>
    <div id="container-clear">
      <div id="zero"></div>
      <div id="clears-right"></div>
    </div>
  )HTML");

  const auto* html_fragment = GetHtmlPhysicalFragment();
  auto* body_fragment =
      To<PhysicalBoxFragment>(html_fragment->Children()[0].get());
  ASSERT_EQ(4UL, body_fragment->Children().size());

  // Verify #container-clear block
  auto* container_clear_fragment =
      To<PhysicalBoxFragment>(body_fragment->Children()[3].get());
  PhysicalOffset container_clear_offset = body_fragment->Children()[3].Offset();
  // 60 = block1's height 30 + std::max(block1's margin 20, zero's margin 30)
  EXPECT_THAT(PhysicalOffset(0, 60), container_clear_offset);
  Element* container_clear = GetElementById("container-clear");
  // 190 = block1's margin 130 + block1's height 30 +
  //       std::max(block1's margin 20, zero's margin 30)
  EXPECT_THAT(container_clear->OffsetTop(), 190);

  // Verify #clears-right block
  ASSERT_EQ(2UL, container_clear_fragment->Children().size());
  PhysicalOffset clears_right_offset =
      container_clear_fragment->Children()[1].Offset();
  // 20 = right-float's block end offset (130 + 80) -
  //      container_clear->offsetTop() 190
  EXPECT_THAT(PhysicalOffset(0, 20), clears_right_offset);
}

// Tests that a block won't fragment if it doesn't reach the fragmentation line.
TEST_F(BlockLayoutAlgorithmTest, NoFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          height: 200px;
        }
      </style>
      <div id='container'></div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  BlockNode node(GetLayoutBoxByElementId("container"));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  // We should only have one 150x200 fragment with no fragmentation.
  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(150, 200), fragment->Size());
  ASSERT_FALSE(fragment->GetBreakToken());
}

// Tests that a block will fragment if it reaches the fragmentation line.
TEST_F(BlockLayoutAlgorithmTest, SimpleFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          height: 300px;
        }
      </style>
      <div id='container'></div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  BlockNode node(GetLayoutBoxByElementId("container"));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(150, 200), fragment->Size());
  EXPECT_TRUE(fragment->GetBreakToken());

  fragment = RunBlockLayoutAlgorithm(node, space, fragment->GetBreakToken());
  EXPECT_EQ(PhysicalSize(150, 100), fragment->Size());
  ASSERT_FALSE(fragment->GetBreakToken());
}

// Tests that children inside the same block formatting context fragment when
// reaching a fragmentation line.
TEST_F(BlockLayoutAlgorithmTest, InnerChildrenFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          padding-top: 20px;
        }
        #child1 {
          height: 200px;
          margin-bottom: 20px;
        }
        #child2 {
          height: 100px;
          margin-top: 20px;
        }
      </style>
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
      </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  BlockNode node(GetLayoutBoxByElementId("container"));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(150, 200), fragment->Size());
  EXPECT_TRUE(fragment->GetBreakToken());

  FragmentChildIterator iterator(To<PhysicalBoxFragment>(fragment));
  PhysicalOffset offset;
  const PhysicalBoxFragment* child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(150, 180), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 20), offset);

  EXPECT_FALSE(iterator.NextChild());

  fragment = RunBlockLayoutAlgorithm(node, space, fragment->GetBreakToken());
  EXPECT_EQ(PhysicalSize(150, 140), fragment->Size());
  ASSERT_FALSE(fragment->GetBreakToken());

  iterator.SetParent(To<PhysicalBoxFragment>(fragment));
  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(150, 20), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 0), offset);

  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(150, 100), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 40), offset);

  EXPECT_FALSE(iterator.NextChild());
}

// Tests that children which establish new formatting contexts fragment
// correctly.
TEST_F(BlockLayoutAlgorithmTest, InnerFormattingContextChildrenFragmentation) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          padding-top: 20px;
        }
        #child1 {
          height: 200px;
          margin-bottom: 20px;
          contain: paint;
        }
        #child2 {
          height: 100px;
          margin-top: 20px;
          contain: paint;
        }
      </style>
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
      </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  BlockNode node(GetLayoutBoxByElementId("container"));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(150, 200), fragment->Size());
  EXPECT_TRUE(fragment->GetBreakToken());

  FragmentChildIterator iterator(To<PhysicalBoxFragment>(fragment));
  PhysicalOffset offset;
  const PhysicalBoxFragment* child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(150, 180), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 20), offset);

  EXPECT_FALSE(iterator.NextChild());

  fragment = RunBlockLayoutAlgorithm(node, space, fragment->GetBreakToken());
  EXPECT_EQ(PhysicalSize(150, 140), fragment->Size());
  ASSERT_FALSE(fragment->GetBreakToken());

  iterator.SetParent(To<PhysicalBoxFragment>(fragment));
  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(150, 20), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 0), offset);

  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(150, 100), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 40), offset);

  EXPECT_FALSE(iterator.NextChild());
}

// Tests that children inside a block container will fragment if the container
// doesn't reach the fragmentation line.
TEST_F(BlockLayoutAlgorithmTest, InnerChildrenFragmentationSmallHeight) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          padding-top: 20px;
          height: 50px;
        }
        #child1 {
          height: 200px;
          margin-bottom: 20px;
        }
        #child2 {
          height: 100px;
          margin-top: 20px;
        }
      </style>
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
      </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  BlockNode node(GetLayoutBoxByElementId("container"));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(150, 70), fragment->Size());
  EXPECT_TRUE(fragment->GetBreakToken());

  FragmentChildIterator iterator(To<PhysicalBoxFragment>(fragment));
  PhysicalOffset offset;
  const PhysicalBoxFragment* child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(150, 180), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 20), offset);

  EXPECT_FALSE(iterator.NextChild());

  fragment = RunBlockLayoutAlgorithm(node, space, fragment->GetBreakToken());
  EXPECT_EQ(PhysicalSize(150, 0), fragment->Size());
  ASSERT_FALSE(fragment->GetBreakToken());

  iterator.SetParent(To<PhysicalBoxFragment>(fragment));
  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(150, 20), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 0), offset);

  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(150, 100), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 40), offset);

  EXPECT_FALSE(iterator.NextChild());
}

// Tests that float children fragment correctly inside a parallel flow.
TEST_F(BlockLayoutAlgorithmTest, DISABLED_FloatFragmentationParallelFlows) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        width: 150px;
        height: 50px;
        display: flow-root;
      }
      #float1 {
        width: 50px;
        height: 200px;
        float: left;
      }
      #float2 {
        width: 75px;
        height: 250px;
        float: right;
        margin: 10px;
      }
    </style>
    <div id='container'>
      <div id='float1'></div>
      <div id='float2'></div>
    </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(150);

  BlockNode node(To<LayoutBlockFlow>(GetLayoutObjectByElementId("container")));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(150, 50), fragment->Size());
  EXPECT_TRUE(fragment->GetBreakToken());

  FragmentChildIterator iterator(To<PhysicalBoxFragment>(fragment));

  // First fragment of float1.
  PhysicalOffset offset;
  const auto* child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(50, 150), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 0), offset);

  // First fragment of float2.
  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(75, 150), child->Size());
  EXPECT_EQ(PhysicalOffset(65, 10), offset);

  space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  fragment = RunBlockLayoutAlgorithm(node, space, fragment->GetBreakToken());
  EXPECT_EQ(PhysicalSize(150, 0), fragment->Size());
  ASSERT_FALSE(fragment->GetBreakToken());

  iterator.SetParent(To<PhysicalBoxFragment>(fragment));

  // Second fragment of float1.
  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(50, 50), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 0), offset);

  // Second fragment of float2.
  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(75, 100), child->Size());
  EXPECT_EQ(PhysicalOffset(65, 0), offset);
}

// Tests that float children don't fragment if they aren't in the same writing
// mode as their parent.
TEST_F(BlockLayoutAlgorithmTest, FloatFragmentationOrthogonalFlows) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        width: 150px;
        height: 60px;
        display: flow-root;
      }
      #float1 {
        width: 100px;
        height: 50px;
        float: left;
      }
      #float2 {
        width: 60px;
        height: 200px;
        float: right;
        writing-mode: vertical-rl;
      }
    </style>
    <div id='container'>
      <div id='float1'></div>
      <div id='float2'></div>
    </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(150);

  BlockNode node(To<LayoutBlockFlow>(GetLayoutObjectByElementId("container")));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true, kFragmentainerSpaceAvailable);

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(150, 60), fragment->Size());
  ASSERT_FALSE(fragment->GetBreakToken());

  const auto* float2 = fragment->Children()[1].fragment.Get();

  // float2 should only have one fragment.
  EXPECT_EQ(PhysicalSize(60, 200), float2->Size());
  ASSERT_TRUE(float2->IsBox());
  const BreakToken* break_token =
      To<PhysicalBoxFragment>(float2)->GetBreakToken();
  EXPECT_FALSE(break_token);
}

// Tests that a float child inside a zero height block fragments correctly.
TEST_F(BlockLayoutAlgorithmTest, DISABLED_FloatFragmentationZeroHeight) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        width: 150px;
        height: 50px;
        display: flow-root;
      }
      #float {
        width: 75px;
        height: 200px;
        float: left;
        margin: 10px;
      }
    </style>
    <div id='container'>
      <div id='zero'>
        <div id='float'></div>
      </div>
    </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(150);

  BlockNode node(To<LayoutBlockFlow>(GetLayoutObjectByElementId("container")));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(150, 50), fragment->Size());
  EXPECT_TRUE(fragment->GetBreakToken());

  FragmentChildIterator iterator(To<PhysicalBoxFragment>(fragment));
  const auto* child = iterator.NextChild();

  // First fragment of float.
  iterator.SetParent(child);
  PhysicalOffset offset;
  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(75, 150), child->Size());
  EXPECT_EQ(PhysicalOffset(10, 10), offset);

  space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      node.CreatesNewFormattingContext(), kFragmentainerSpaceAvailable);

  fragment = RunBlockLayoutAlgorithm(node, space, fragment->GetBreakToken());
  EXPECT_EQ(PhysicalSize(150, 0), fragment->Size());
  ASSERT_FALSE(fragment->GetBreakToken());

  iterator.SetParent(To<PhysicalBoxFragment>(fragment));
  child = iterator.NextChild();

  // Second fragment of float.
  iterator.SetParent(child);
  child = iterator.NextChild();
  EXPECT_EQ(PhysicalSize(75, 50), child->Size());
  // TODO(ikilpatrick): Don't include the block-start margin of a float which
  // has fragmented.
  // EXPECT_EQ(PhysicalOffset(10, 0),
  // child->Offset());
}

// Verifies that we correctly position a new FC block with the Layout
// Opportunity iterator.
TEST_F(BlockLayoutAlgorithmTest, NewFcBlockWithAdjoiningFloatCollapsesMargins) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        width: 200px; outline: solid purple 1px;
      }
      #float {
        float: left; width: 100px; height: 30px; background: red;
      }
      #new-fc {
        contain: paint; margin-top: 20px; background: purple;
        height: 50px;
      }
    </style>
    <div id="container">
      <div id="float"></div>
      <div id="new-fc"></div>
    </div>
  )HTML");

  PhysicalOffset body_offset;
  PhysicalOffset new_fc_offset;

  auto run_test = [&](const Length& block_width) {
    UpdateStyleForElement(
        GetElementById("new-fc"),
        [&](ComputedStyleBuilder& builder) { builder.SetWidth(block_width); });
    const auto* fragment = GetHtmlPhysicalFragment();
    ASSERT_EQ(1UL, fragment->Children().size());
    const auto* body_fragment =
        To<PhysicalBoxFragment>(fragment->Children()[0].get());
    const auto* container_fragment =
        To<PhysicalBoxFragment>(body_fragment->Children()[0].get());
    ASSERT_EQ(2UL, container_fragment->Children().size());
    body_offset = fragment->Children()[0].Offset();
    new_fc_offset = container_fragment->Children()[1].Offset();
  };

  // #new-fc is small enough to fit on the same line with #float.
  run_test(Length::Fixed(80));
  // 100 = float's width, 0 = no margin collapsing
  EXPECT_THAT(new_fc_offset, PhysicalOffset(100, 0));
  // 8 = body's margins, 20 = new-fc's margin top(20) collapses with
  // body's margin(8)
  EXPECT_THAT(body_offset, PhysicalOffset(8, 20));

  // #new-fc is too wide to be positioned on the same line with #float
  run_test(Length::Fixed(120));
  // 30 = #float's height
  EXPECT_THAT(new_fc_offset, PhysicalOffset(0, 30));
  // 8 = body's margins, no margin collapsing
  EXPECT_THAT(body_offset, PhysicalOffset(8, 8));
}

TEST_F(BlockLayoutAlgorithmTest, NewFcAvoidsFloats) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        width: 200px;
      }
      #float {
        float: left; width: 100px; height: 30px; background: red;
      }
      #fc {
        width: 150px; height: 120px; display: flow-root;
      }
    </style>
    <div id="container">
      <div id="float"></div>
      <div id="fc"></div>
    </div>
  )HTML");

  BlockNode node(To<LayoutBlockFlow>(GetLayoutObjectByElementId("container")));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize));

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(200, 150), fragment->Size());

  FragmentChildIterator iterator(To<PhysicalBoxFragment>(fragment));

  PhysicalOffset offset;
  const PhysicalBoxFragment* child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(100, 30), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 0), offset);

  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(150, 120), child->Size());
  EXPECT_EQ(PhysicalOffset(0, 30), offset);
}

TEST_F(BlockLayoutAlgorithmTest, ZeroBlockSizeAboveEdge) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container { width: 200px; display: flow-root; }
      #inflow { width: 50px; height: 50px; background: red; margin-top: -70px; }
      #zero { width: 70px; margin: 10px 0 30px 0; }
    </style>
    <div id="container">
      <div id="inflow"></div>
      <div id="zero"></div>
    </div>
  )HTML");

  BlockNode node(To<LayoutBlockFlow>(GetLayoutObjectByElementId("container")));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(200, 10), fragment->Size());

  FragmentChildIterator iterator(To<PhysicalBoxFragment>(fragment));

  PhysicalOffset offset;
  const PhysicalBoxFragment* child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(50, 50), child->Size());
  EXPECT_EQ(PhysicalOffset(0, -70), offset);

  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(70, 0), child->Size());
  EXPECT_EQ(PhysicalOffset(0, -10), offset);
}

TEST_F(BlockLayoutAlgorithmTest, NewFcFirstChildIsZeroBlockSize) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container { width: 200px; display: flow-root; }
      #zero1 { width: 50px; margin-top: -30px; margin-bottom: 10px; }
      #zero2 { width: 70px; margin-top: 20px; margin-bottom: -40px; }
      #inflow { width: 90px; height: 20px; margin-top: 30px; }
    </style>
    <div id="container">
      <div id="zero1"></div>
      <div id="zero2"></div>
      <div id="inflow"></div>
    </div>
  )HTML");

  BlockNode node(To<LayoutBlockFlow>(GetLayoutObjectByElementId("container")));
  ConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  const PhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(node, space);
  EXPECT_EQ(PhysicalSize(200, 10), fragment->Size());

  FragmentChildIterator iterator(To<PhysicalBoxFragment>(fragment));

  PhysicalOffset offset;
  const PhysicalBoxFragment* child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(50, 0), child->Size());
  EXPECT_EQ(PhysicalOffset(0, -30), offset);

  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(70, 0), child->Size());
  EXPECT_EQ(PhysicalOffset(0, -10), offset);

  child = iterator.NextChild(&offset);
  EXPECT_EQ(PhysicalSize(90, 20), child->Size());
  EXPECT_EQ(PhysicalOffset(0, -10), offset);
}

// This test assumes that tables are not yet implemented in LayoutNG.
TEST_F(BlockLayoutAlgorithmTest, RootFragmentOffsetInsideLegacy) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div style="display:table-cell;">
      <div id="innerNGRoot" style="margin-top:10px; margin-left:20px;"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  const LayoutObject* innerNGRoot = GetLayoutObjectByElementId("innerNGRoot");

  ASSERT_TRUE(innerNGRoot->IsLayoutNGObject());
  const PhysicalBoxFragment* fragment =
      CurrentFragmentFor(To<LayoutBlockFlow>(innerNGRoot));

  ASSERT_TRUE(fragment);
  // TODO(crbug.com/781241: Re-enable when we calculate inline offset at
  // the right time.
  // EXPECT_EQ(PhysicalOffset(20, 10), fragment->Offset());
}

TEST_F(BlockLayoutAlgorithmTest, LayoutRubyTextCrash) {
  // crbug.com/1102186. This test passes if no DCHECK failure.
  SetBodyInnerHTML(R"HTML(
    <ruby>base<rt style="writing-mode:vertical-rl">annotation</ruby>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(BlockLayoutAlgorithmTest, HandleTextControlPlaceholderCrash) {
  // crbug.com/1209025 and crbug.com/1342608. This test passes if no crash.
  SetBodyInnerHTML(R"HTML(
<style>
input::first-line {
 color: red;
}
#num::-webkit-textfield-decoration-container {
 position: absolute;
}
</style>
<input id="i1" readonly>
<input id="num" type="number" placeholder="foo">)HTML");
  UpdateAllLifecyclePhasesForTest();
  auto* input = GetElementById("i1");
  input->setAttribute(html_names::kPlaceholderAttr, AtomicString("z"));
  UpdateAllLifecyclePhasesForTest();
}

}  // namespace
}  // namespace blink
