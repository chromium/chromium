// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class PhysicalBoxFragmentTest : public RenderingTest {
 public:
  const PhysicalBoxFragment& GetBodyFragment() const {
    return *To<LayoutBlockFlow>(GetDocument().body()->GetLayoutObject())
                ->GetPhysicalFragment(0);
  }

  const PhysicalBoxFragment& GetPhysicalBoxFragmentByElementId(const char* id) {
    auto* layout_object = GetLayoutBoxByElementId(id);
    DCHECK(layout_object);
    const PhysicalBoxFragment* fragment = layout_object->GetPhysicalFragment(0);
    DCHECK(fragment);
    return *fragment;
  }
};

TEST_F(PhysicalBoxFragmentTest, FloatingDescendantsInlineChlidren) {
  SetBodyInnerHTML(R"HTML(
    <div id="hasfloats">
      text
      <div style="float: left"></div>
    </div>
    <div id="nofloats">
      text
    </div>
  )HTML");

  const auto& has_floats = GetPhysicalBoxFragmentByElementId("hasfloats");
  EXPECT_TRUE(has_floats.HasFloatingDescendantsForPaint());
  const auto& no_floats = GetPhysicalBoxFragmentByElementId("nofloats");
  EXPECT_FALSE(no_floats.HasFloatingDescendantsForPaint());
}

TEST_F(PhysicalBoxFragmentTest, FloatingDescendantsBlockChlidren) {
  SetBodyInnerHTML(R"HTML(
    <div id="hasfloats">
      <div></div>
      <div style="float: left"></div>
    </div>
    <div id="nofloats">
      <div></div>
    </div>
  )HTML");

  const auto& has_floats = GetPhysicalBoxFragmentByElementId("hasfloats");
  EXPECT_TRUE(has_floats.HasFloatingDescendantsForPaint());
  const auto& no_floats = GetPhysicalBoxFragmentByElementId("nofloats");
  EXPECT_FALSE(no_floats.HasFloatingDescendantsForPaint());
}

// HasFloatingDescendantsForPaint() should be set for each inline formatting
// context and should not be propagated across inline formatting context.
TEST_F(PhysicalBoxFragmentTest, FloatingDescendantsInlineBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id="nofloats">
      text
      <span id="hasfloats" style="display: inline-block">
        <div style="float: left"></div>
      </span>
    </div>
  )HTML");

  const auto& has_floats = GetPhysicalBoxFragmentByElementId("hasfloats");
  EXPECT_TRUE(has_floats.HasFloatingDescendantsForPaint());
  const auto& no_floats = GetPhysicalBoxFragmentByElementId("nofloats");
  EXPECT_FALSE(no_floats.HasFloatingDescendantsForPaint());
}

// HasFloatingDescendantsForPaint() should be set even if it crosses a block
// formatting context.
TEST_F(PhysicalBoxFragmentTest, FloatingDescendantsBlockFormattingContext) {
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

  const auto& has_floats = GetPhysicalBoxFragmentByElementId("hasfloats");
  EXPECT_TRUE(has_floats.HasFloatingDescendantsForPaint());

  const auto& has_floats_2 = GetPhysicalBoxFragmentByElementId("hasfloats2");
  EXPECT_TRUE(has_floats_2.HasFloatingDescendantsForPaint());
}

TEST_F(PhysicalBoxFragmentTest, ReplacedBlock) {
  SetBodyInnerHTML(R"HTML(
    <img id="target" style="display: block">
  )HTML");
  const PhysicalBoxFragment& body = GetBodyFragment();
  const PhysicalFragment& fragment = *body.Children().front();
  EXPECT_EQ(fragment.Type(), PhysicalFragment::kFragmentBox);
  // |LayoutReplaced| sets |IsAtomicInlineLevel()| even when it is block-level.
  // crbug.com/567964
  EXPECT_FALSE(fragment.IsAtomicInline());
  EXPECT_EQ(fragment.GetBoxType(), PhysicalFragment::kBlockFlowRoot);
}

TEST_F(PhysicalBoxFragmentTest, IsFragmentationContextRoot) {
  SetBodyInnerHTML(R"HTML(
    <div id="multicol" style="columns:3;">
      <div id="child"></div>
    </div>
  )HTML");

  const auto& multicol = GetPhysicalBoxFragmentByElementId("multicol");
  EXPECT_TRUE(multicol.IsFragmentationContextRoot());

  // There should be one column.
  EXPECT_EQ(multicol.Children().size(), 1u);
  const auto& column = To<PhysicalBoxFragment>(*multicol.Children()[0]);
  EXPECT_TRUE(column.IsColumnBox());
  EXPECT_FALSE(column.IsFragmentationContextRoot());

  const auto& child = GetPhysicalBoxFragmentByElementId("child");
  EXPECT_FALSE(child.IsFragmentationContextRoot());
}

TEST_F(PhysicalBoxFragmentTest, IsFragmentationContextRootNested) {
  SetBodyInnerHTML(R"HTML(
    <div id="outer" style="columns:3;">
      <div id="foo">
        <div id="inner" style="columns:3;">
          <div id="bar"></div>
        </div>
      </div>
    </div>
  )HTML");

  const auto& outer = GetPhysicalBoxFragmentByElementId("outer");
  EXPECT_TRUE(outer.IsFragmentationContextRoot());

  EXPECT_EQ(outer.Children().size(), 1u);
  const auto& outer_column = To<PhysicalBoxFragment>(*outer.Children()[0]);
  EXPECT_TRUE(outer_column.IsColumnBox());
  EXPECT_FALSE(outer_column.IsFragmentationContextRoot());

  const auto& foo = GetPhysicalBoxFragmentByElementId("foo");
  EXPECT_FALSE(foo.IsFragmentationContextRoot());

  const auto& inner = GetPhysicalBoxFragmentByElementId("inner");
  EXPECT_TRUE(inner.IsFragmentationContextRoot());

  EXPECT_EQ(inner.Children().size(), 1u);
  const auto& inner_column = To<PhysicalBoxFragment>(*inner.Children()[0]);
  EXPECT_TRUE(inner_column.IsColumnBox());
  EXPECT_FALSE(inner_column.IsFragmentationContextRoot());

  const auto& bar = GetPhysicalBoxFragmentByElementId("bar");
  EXPECT_FALSE(bar.IsFragmentationContextRoot());
}

TEST_F(PhysicalBoxFragmentTest, IsFragmentationContextRootFieldset) {
  SetBodyInnerHTML(R"HTML(
    <fieldset id="fieldset" style="columns:3;">
      <legend id="legend"></legend>
      <div id="child"></div>
    </fieldset>
  )HTML");

  const auto& fieldset = GetPhysicalBoxFragmentByElementId("fieldset");
  EXPECT_FALSE(fieldset.IsFragmentationContextRoot());

  // There should be a legend and an anonymous fieldset wrapper fragment.
  ASSERT_EQ(fieldset.Children().size(), 2u);

  const auto& legend = To<PhysicalBoxFragment>(*fieldset.Children()[0]);
  EXPECT_EQ(To<Element>(legend.GetNode())->GetIdAttribute(), "legend");
  EXPECT_FALSE(legend.IsFragmentationContextRoot());

  // The multicol container is established by the anonymous content wrapper, not
  // the actual fieldset.
  const auto& wrapper = To<PhysicalBoxFragment>(*fieldset.Children()[1]);
  EXPECT_FALSE(wrapper.GetNode());
  EXPECT_TRUE(wrapper.IsFragmentationContextRoot());

  EXPECT_EQ(wrapper.Children().size(), 1u);
  const auto& column = To<PhysicalBoxFragment>(*wrapper.Children()[0]);
  EXPECT_TRUE(column.IsColumnBox());

  const auto& child = GetPhysicalBoxFragmentByElementId("child");
  EXPECT_FALSE(child.IsFragmentationContextRoot());
}

TEST_F(PhysicalBoxFragmentTest, MayHaveDescendantAboveBlockStart) {
  SetBodyInnerHTML(R"HTML(
    <div id="container2">
      <div id="container">
        <div style="height: 100px"></div>
        <div style="height: 100px; margin-top: -200px"></div>
      </div>
    </div>
  )HTML");
  const auto& container = GetPhysicalBoxFragmentByElementId("container");
  EXPECT_TRUE(container.MayHaveDescendantAboveBlockStart());
  const auto& container2 = GetPhysicalBoxFragmentByElementId("container2");
  EXPECT_TRUE(container2.MayHaveDescendantAboveBlockStart());
}

TEST_F(PhysicalBoxFragmentTest, MayHaveDescendantAboveBlockStartBlockInInline) {
  SetBodyInnerHTML(R"HTML(
    <div id="container2">
      <div id="container">
        <span>
          <div style="height: 100px"></div>
          <div style="height: 100px; margin-top: -200px"></div>
        </span>
      </div>
    </div>
  )HTML");
  const auto& container = GetPhysicalBoxFragmentByElementId("container");
  EXPECT_TRUE(container.MayHaveDescendantAboveBlockStart());
  const auto& container2 = GetPhysicalBoxFragmentByElementId("container2");
  EXPECT_TRUE(container2.MayHaveDescendantAboveBlockStart());
}

TEST_F(PhysicalBoxFragmentTest, OverflowClipMarginVisualBox) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        width: 200px;
        height: 50px;
        column-count: 2;
      }

      .container {
        width: 50px;
        height: 50px;
        border: 5px solid grey;
        padding: 5px;
        overflow: clip;
        overflow-clip-margin: content-box 15px;
      }

      .content {
        width: 100px;
        height: 200px;
      }
    </style>
    <div class="container" id="test">
      <div class="content" style="background:blue"></div>
    </div>
  )HTML");

  auto* layout_box = GetLayoutBoxByElementId("test");
  ASSERT_EQ(layout_box->PhysicalFragmentCount(), 2u);

  const PhysicalOffset zero_offset;

  EXPECT_EQ(
      layout_box->GetPhysicalFragment(0)->InkOverflowRect(),
      PhysicalRect(zero_offset, PhysicalSize(LayoutUnit(75), LayoutUnit(35))));
  EXPECT_EQ(
      layout_box->GetPhysicalFragment(1)->InkOverflowRect(),
      PhysicalRect(zero_offset, PhysicalSize(LayoutUnit(75), LayoutUnit(40))));

  GetElementById("test")->SetInlineStyleProperty(
      CSSPropertyID::kOverflowClipMargin, "padding-box 15px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      layout_box->GetPhysicalFragment(0)->InkOverflowRect(),
      PhysicalRect(zero_offset, PhysicalSize(LayoutUnit(80), LayoutUnit(35))));
  EXPECT_EQ(
      layout_box->GetPhysicalFragment(1)->InkOverflowRect(),
      PhysicalRect(zero_offset, PhysicalSize(LayoutUnit(80), LayoutUnit(45))));

  GetElementById("test")->SetInlineStyleProperty(
      CSSPropertyID::kOverflowClipMargin, "border-box 15px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      layout_box->GetPhysicalFragment(0)->InkOverflowRect(),
      PhysicalRect(zero_offset, PhysicalSize(LayoutUnit(85), LayoutUnit(35))));
  EXPECT_EQ(
      layout_box->GetPhysicalFragment(1)->InkOverflowRect(),
      PhysicalRect(zero_offset, PhysicalSize(LayoutUnit(85), LayoutUnit(50))));
}

TEST_F(PhysicalBoxFragmentTest, CloneWithPostLayoutFragments) {
  SetHtmlInnerHTML(R"HTML(<frameset id="fs"></frameset>)HTML");
  const auto& fragment = GetPhysicalBoxFragmentByElementId("fs");
  EXPECT_TRUE(fragment.GetFrameSetLayoutData());
  const auto* clone =
      PhysicalBoxFragment::CloneWithPostLayoutFragments(fragment);
  EXPECT_TRUE(clone->GetFrameSetLayoutData());
}

}  // namespace blink
