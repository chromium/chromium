// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

class NGPhysicalBoxFragmentTest : public NGLayoutTest {
 public:
  NGPhysicalBoxFragmentTest() : NGLayoutTest() {}

  const NGPhysicalBoxFragment& GetBodyFragment() const {
    return *To<LayoutBlockFlow>(GetDocument().body()->GetLayoutObject())
                ->GetPhysicalFragment(0);
  }

  const NGPhysicalBoxFragment& GetPhysicalBoxFragmentByElementId(
      const char* id) {
    auto* layout_object = To<LayoutBlockFlow>(GetLayoutObjectByElementId(id));
    DCHECK(layout_object);
    const NGPhysicalBoxFragment* fragment =
        layout_object->GetPhysicalFragment(0);
    DCHECK(fragment);
    return *fragment;
  }
};

TEST_F(NGPhysicalBoxFragmentTest, FloatingDescendantsInlineChlidren) {
  SetBodyInnerHTML(R"HTML(
    <div id="hasfloats">
      text
      <div style="float: left"></div>
    </div>
    <div id="nofloats">
      text
    </div>
  )HTML");

  const NGPhysicalBoxFragment& has_floats =
      GetPhysicalBoxFragmentByElementId("hasfloats");
  EXPECT_TRUE(has_floats.HasFloatingDescendantsForPaint());
  const NGPhysicalBoxFragment& no_floats =
      GetPhysicalBoxFragmentByElementId("nofloats");
  EXPECT_FALSE(no_floats.HasFloatingDescendantsForPaint());
}

TEST_F(NGPhysicalBoxFragmentTest, FloatingDescendantsBlockChlidren) {
  SetBodyInnerHTML(R"HTML(
    <div id="hasfloats">
      <div></div>
      <div style="float: left"></div>
    </div>
    <div id="nofloats">
      <div></div>
    </div>
  )HTML");

  const NGPhysicalBoxFragment& has_floats =
      GetPhysicalBoxFragmentByElementId("hasfloats");
  EXPECT_TRUE(has_floats.HasFloatingDescendantsForPaint());
  const NGPhysicalBoxFragment& no_floats =
      GetPhysicalBoxFragmentByElementId("nofloats");
  EXPECT_FALSE(no_floats.HasFloatingDescendantsForPaint());
}

// HasFloatingDescendantsForPaint() should be set for each inline formatting
// context and should not be propagated across inline formatting context.
TEST_F(NGPhysicalBoxFragmentTest, FloatingDescendantsInlineBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id="nofloats">
      text
      <span id="hasfloats" style="display: inline-block">
        <div style="float: left"></div>
      </span>
    </div>
  )HTML");

  const NGPhysicalBoxFragment& has_floats =
      GetPhysicalBoxFragmentByElementId("hasfloats");
  EXPECT_TRUE(has_floats.HasFloatingDescendantsForPaint());
  const NGPhysicalBoxFragment& no_floats =
      GetPhysicalBoxFragmentByElementId("nofloats");
  EXPECT_FALSE(no_floats.HasFloatingDescendantsForPaint());
}

// HasFloatingDescendantsForPaint() should be set even if it crosses a block
// formatting context.
TEST_F(NGPhysicalBoxFragmentTest, FloatingDescendantsBlockFormattingContext) {
  SetBodyInnerHTML(R"HTML(
    <div id="hasfloats">
      <div style="display: flow-root">
        <div style="float: left"></div>
      </div>
    </div>
    <div id="hasfloats2" style="position: relative">
      <div style="position: absolute">
        <div style="float: left"></div>
      </div>
    </div>
  )HTML");

  const NGPhysicalBoxFragment& has_floats =
      GetPhysicalBoxFragmentByElementId("hasfloats");
  EXPECT_TRUE(has_floats.HasFloatingDescendantsForPaint());

  const NGPhysicalBoxFragment& has_floats_2 =
      GetPhysicalBoxFragmentByElementId("hasfloats2");
  EXPECT_TRUE(has_floats_2.HasFloatingDescendantsForPaint());
}

// TODO(layout-dev): Design more straightforward way to ensure old layout
// instead of using |contenteditable|.

// Tests that a normal old layout root box fragment has correct box type.
TEST_F(NGPhysicalBoxFragmentTest, DISABLED_NormalLegacyLayoutRoot) {
  SetBodyInnerHTML("<div contenteditable>X</div>");
  const NGPhysicalFragment* fragment =
      GetBodyFragment().Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kNormalBox, fragment->BoxType());
  EXPECT_TRUE(fragment->IsLegacyLayoutRoot());
  EXPECT_TRUE(fragment->IsFormattingContextRoot());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
// Tests that a float old layout root box fragment has correct box type.
TEST_F(NGPhysicalBoxFragmentTest, DISABLED_FloatLegacyLayoutRoot) {
  SetBodyInnerHTML("<span contenteditable style='float:left'>X</span>foo");
  const NGPhysicalFragment* fragment =
      GetBodyFragment().Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kFloating, fragment->BoxType());
  EXPECT_TRUE(fragment->IsLegacyLayoutRoot());
  EXPECT_TRUE(fragment->IsFormattingContextRoot());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
// Tests that an inline block old layout root box fragment has correct box type.
TEST_F(NGPhysicalBoxFragmentTest, DISABLED_InlineBlockLegacyLayoutRoot) {
  SetBodyInnerHTML(
      "<span contenteditable style='display:inline-block'>X</span>foo");
  const auto* line_box = To<NGPhysicalContainerFragment>(
      GetBodyFragment().Children().front().get());
  const NGPhysicalFragment* fragment = line_box->Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kAtomicInline, fragment->BoxType());
  EXPECT_TRUE(fragment->IsLegacyLayoutRoot());
  EXPECT_TRUE(fragment->IsFormattingContextRoot());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
// Tests that an out-of-flow positioned old layout root box fragment has correct
// box type.
TEST_F(NGPhysicalBoxFragmentTest,
       DISABLED_OutOfFlowPositionedLegacyLayoutRoot) {
  SetBodyInnerHTML(
      "<style>body {position: absolute}</style>"
      "<div contenteditable style='position: absolute'>X</div>");
  const NGPhysicalFragment* fragment =
      GetBodyFragment().Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kOutOfFlowPositioned, fragment->BoxType());
  EXPECT_TRUE(fragment->IsLegacyLayoutRoot());
  EXPECT_TRUE(fragment->IsFormattingContextRoot());
}

TEST_F(NGPhysicalBoxFragmentTest, ReplacedBlock) {
  SetBodyInnerHTML(R"HTML(
    <img id="target" style="display: block">
  )HTML");
  const NGPhysicalBoxFragment& body = GetBodyFragment();
  const NGPhysicalFragment& fragment = *body.Children().front();
  EXPECT_EQ(fragment.Type(), NGPhysicalFragment::kFragmentBox);
  // |LayoutReplaced| sets |IsAtomicInlineLevel()| even when it is block-level.
  // crbug.com/567964
  EXPECT_FALSE(fragment.IsAtomicInline());
  EXPECT_EQ(fragment.BoxType(), NGPhysicalFragment::kBlockFlowRoot);
}

TEST_F(NGPhysicalBoxFragmentTest, OffsetFromFirstFragmentColumnBox) {
  ScopedLayoutNGBlockFragmentationForTest enable_ng_block_fragmentation(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #columns {
      column-width: 100px;
      column-gap: 10px;
      column-fill: auto;
      width: 320px;
      height: 500px;
    }
    </style>
    <div id="columns" style="background: blue">
      <div id="block" style="height: 1500px"></div>
    </div>
  )HTML");
  const auto* columns = GetLayoutBoxByElementId("columns");
  const auto* flow_thread = To<LayoutBox>(columns->SlowFirstChild());
  EXPECT_EQ(flow_thread->PhysicalFragmentCount(), 3u);
  const NGPhysicalBoxFragment* fragment0 = flow_thread->GetPhysicalFragment(0);
  EXPECT_EQ(fragment0->OffsetFromFirstFragment(), PhysicalOffset());
  const NGPhysicalBoxFragment* fragment1 = flow_thread->GetPhysicalFragment(1);
  EXPECT_EQ(fragment1->OffsetFromFirstFragment(), PhysicalOffset(110, 0));
  const NGPhysicalBoxFragment* fragment2 = flow_thread->GetPhysicalFragment(2);
  EXPECT_EQ(fragment2->OffsetFromFirstFragment(), PhysicalOffset(220, 0));

  // Check running another layout does not crash.
  GetElementById("block")->appendChild(GetDocument().createTextNode("a"));
  RunDocumentLifecycle();
}

TEST_F(NGPhysicalBoxFragmentTest, OffsetFromFirstFragmentFloat) {
  ScopedLayoutNGBlockFragmentationForTest enable_ng_block_fragmentation(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #columns {
      column-width: 100px;
      column-gap: 10px;
      column-fill: auto;
      width: 320px;
      height: 500px;
    }
    #float {
      float: left;
      width: 50px;
      height: 500px;
      background: orange;
    }
    </style>
    <div id="columns" style="background: blue">
      <!-- A spacer to make `target` start at 2nd column. -->
      <div style="height: 800px"></div>
      <div id="float"></div>
      Text
    </div>
  )HTML");
  const auto* target = GetLayoutBoxByElementId("float");
  EXPECT_EQ(target->PhysicalFragmentCount(), 2u);
  const NGPhysicalBoxFragment* fragment0 = target->GetPhysicalFragment(0);
  EXPECT_EQ(fragment0->OffsetFromFirstFragment(), PhysicalOffset());
  const NGPhysicalBoxFragment* fragment1 = target->GetPhysicalFragment(1);
  EXPECT_EQ(fragment1->OffsetFromFirstFragment(), PhysicalOffset(110, -300));
}

TEST_F(NGPhysicalBoxFragmentTest, OffsetFromFirstFragmentNested) {
  ScopedLayoutNGBlockFragmentationForTest enable_ng_block_fragmentation(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
    }
    #outer-columns {
      column-width: 100px;
      column-gap: 10px;
      column-fill: auto;
      width: 320px;
      height: 500px;
    }
    #inner-columns {
      column-width: 45px;
      column-gap: 10px;
      column-fill: auto;
      width: 100px;
      height: 800px;
    }
    </style>
    <div id="outer-columns" style="background: blue">
      <!-- A spacer to make `inner-columns` start at 2nd column. -->
      <div style="height: 700px"></div>
      <div id="inner-columns" style="height: 800px; background: purple">
        <!-- A spacer to make `target` start at 2nd column. -->
        <div style="height: 400px"></div>
        <div id="target" style="background: orange; height: 1000px"></div>
      </div>
    </div>
  )HTML");
  const auto* target = GetLayoutBoxByElementId("target");
  EXPECT_EQ(target->PhysicalFragmentCount(), 3u);
  const NGPhysicalBoxFragment* fragment0 = target->GetPhysicalFragment(0);
  EXPECT_EQ(fragment0->OffsetFromFirstFragment(), PhysicalOffset());
  const NGPhysicalBoxFragment* fragment1 = target->GetPhysicalFragment(1);
  EXPECT_EQ(fragment1->OffsetFromFirstFragment(), PhysicalOffset(55, -300));
  const NGPhysicalBoxFragment* fragment2 = target->GetPhysicalFragment(2);
  EXPECT_EQ(fragment2->OffsetFromFirstFragment(), PhysicalOffset(110, -300));
}

}  // namespace blink
