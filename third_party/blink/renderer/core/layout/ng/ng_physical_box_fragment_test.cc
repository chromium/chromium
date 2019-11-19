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
                ->CurrentFragment();
  }

  const NGPhysicalBoxFragment& GetPhysicalBoxFragmentByElementId(
      const char* id) {
    auto* layout_object = To<LayoutBlockFlow>(GetLayoutObjectByElementId(id));
    DCHECK(layout_object);
    const NGPhysicalBoxFragment* fragment = layout_object->CurrentFragment();
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
  EXPECT_TRUE(fragment->IsBlockFormattingContextRoot());
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
  EXPECT_TRUE(fragment->IsBlockFormattingContextRoot());
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
  EXPECT_TRUE(fragment->IsBlockFormattingContextRoot());
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
  EXPECT_TRUE(fragment->IsBlockFormattingContextRoot());
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

}  // namespace blink
