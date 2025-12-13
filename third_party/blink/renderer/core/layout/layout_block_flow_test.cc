// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

class LayoutBlockFlowTest : public RenderingTest {};

// crbug.com/1253159.  We had a bug that a legacy IFC LayoutBlockFlow didn't
// call RecalcVisualOverflow() for children.
TEST_F(LayoutBlockFlowTest, RecalcInlineChildrenScrollableOverflow) {
  SetBodyInnerHTML(R"HTML(
<style>
kbd { float: right; }
var { column-count: 17179869184; }
</style>
<kbd id="kbd">
<var>
<svg>
<text id="text">B B
)HTML");
  LayoutBlockFlow* kbd = To<LayoutBlockFlow>(GetLayoutObjectByElementId("kbd"));
  // The parent should be NG.
  ASSERT_TRUE(kbd->Parent()->IsLayoutBlockFlow());
  ASSERT_TRUE(kbd->CreatesNewFormattingContext());
  UpdateAllLifecyclePhasesForTest();
  GetElementById("text")->setAttribute(AtomicString("font-size"),
                                       AtomicString("100"));
  UpdateAllLifecyclePhasesForTest();
  // The test passes if no DCHECK failure in ink_overflow.cc.
}

TEST_F(LayoutBlockFlowTest, IsInsideMulticol) {
  SetBodyInnerHTML(R"HTML(
<div id="outer">
  <div id="inner" style="columns:3;">
    <div id="container">
      <div id="child"></div>
    </div>
  </div>
</div>
)HTML");

  Element* outer_elm = GetElementById("outer");
  ASSERT_TRUE(outer_elm);
  Element* inner_elm = GetElementById("inner");
  ASSERT_TRUE(inner_elm);

  const LayoutObject* outer = GetLayoutObjectByElementId("outer");
  ASSERT_TRUE(outer);
  const LayoutObject* inner = GetLayoutObjectByElementId("inner");
  ASSERT_TRUE(inner);
  const LayoutObject* container = GetLayoutObjectByElementId("container");
  ASSERT_TRUE(container);
  const LayoutObject* child = GetLayoutObjectByElementId("child");
  ASSERT_TRUE(child);

  EXPECT_FALSE(outer->IsInsideMulticol());
  EXPECT_FALSE(inner->IsInsideMulticol());
  EXPECT_TRUE(container->IsInsideMulticol());
  EXPECT_TRUE(child->IsInsideMulticol());

  // Turn off multicol for #inner
  inner_elm->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "columns", "auto", "", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(outer->IsInsideMulticol());
  EXPECT_FALSE(inner->IsInsideMulticol());
  EXPECT_FALSE(container->IsInsideMulticol());
  EXPECT_FALSE(child->IsInsideMulticol());

  // Turn multicol for #inner back on
  inner_elm->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "columns", "3", "", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(outer->IsInsideMulticol());
  EXPECT_FALSE(inner->IsInsideMulticol());
  EXPECT_TRUE(container->IsInsideMulticol());
  EXPECT_TRUE(child->IsInsideMulticol());

  // Turn on multicol for #outer
  outer_elm->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "columns", "3", "", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(outer->IsInsideMulticol());
  EXPECT_TRUE(inner->IsInsideMulticol());
  EXPECT_TRUE(container->IsInsideMulticol());
  EXPECT_TRUE(child->IsInsideMulticol());

  // Turn off multicol for #inner
  inner_elm->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "columns", "auto", "", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(outer->IsInsideMulticol());
  EXPECT_TRUE(inner->IsInsideMulticol());
  EXPECT_TRUE(container->IsInsideMulticol());
  EXPECT_TRUE(child->IsInsideMulticol());
}

TEST_F(LayoutBlockFlowTest, AnonymousContainer) {
  SetBodyInnerHTML(R"HTML(
  <div id="multicol" style="columns:3;"><span id="child1"></span><span id="child2"></span></div>
)HTML");

  Element* multicol_elm = GetElementById("multicol");
  ASSERT_TRUE(multicol_elm);
  const auto* multicol =
      DynamicTo<LayoutBlockFlow>(GetLayoutObjectByElementId("multicol"));
  ASSERT_TRUE(multicol);
  const LayoutObject* child1 = GetLayoutObjectByElementId("child1");
  ASSERT_TRUE(child1);
  const LayoutObject* child2 = GetLayoutObjectByElementId("child2");
  ASSERT_TRUE(child2);

  // The children are inline. Need an anonymous wrapper.
  const auto* anonymous_wrapper =
      DynamicTo<LayoutBlockFlow>(multicol->FirstChild());
  ASSERT_TRUE(anonymous_wrapper);
  EXPECT_TRUE(anonymous_wrapper->IsAnonymousBlockFlow());
  EXPECT_EQ(anonymous_wrapper->NextSibling(), nullptr);
  ASSERT_TRUE(anonymous_wrapper->FirstChild());
  ASSERT_TRUE(anonymous_wrapper->FirstChild()->NextSibling());
  EXPECT_EQ(anonymous_wrapper->FirstChild(), child1);
  EXPECT_EQ(anonymous_wrapper->FirstChild()->NextSibling(), child2);
  EXPECT_EQ(anonymous_wrapper->FirstChild()->NextSibling()->NextSibling(),
            nullptr);

  // Turn off multicol.
  multicol_elm->style()->setProperty(GetDocument().GetExecutionContext(),
                                     "columns", "auto", "",
                                     ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(multicol->FirstChild(), child1);
  ASSERT_TRUE(multicol->FirstChild()->NextSibling());
  EXPECT_EQ(multicol->FirstChild()->NextSibling(), child2);
  EXPECT_EQ(multicol->FirstChild()->NextSibling()->NextSibling(), nullptr);

  // Turn multicol back on.
  multicol_elm->style()->setProperty(GetDocument().GetExecutionContext(),
                                     "columns", "3", "", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  anonymous_wrapper = DynamicTo<LayoutBlockFlow>(multicol->FirstChild());
  ASSERT_TRUE(anonymous_wrapper);
  EXPECT_TRUE(anonymous_wrapper->IsAnonymousBlockFlow());
  EXPECT_EQ(anonymous_wrapper->NextSibling(), nullptr);
  EXPECT_EQ(anonymous_wrapper->FirstChild(), child1);
  ASSERT_TRUE(anonymous_wrapper->FirstChild());
  ASSERT_TRUE(anonymous_wrapper->FirstChild()->NextSibling());
  EXPECT_EQ(anonymous_wrapper->FirstChild()->NextSibling(), child2);
  EXPECT_EQ(anonymous_wrapper->FirstChild()->NextSibling()->NextSibling(),
            nullptr);
}

TEST_F(LayoutBlockFlowTest, NoAnonymousContainer) {
  SetBodyInnerHTML(R"HTML(
  <div id="multicol" style="columns:3;">
    <div id="child1"></div>
    <div id="child2"></div>
  </div>
)HTML");

  const auto* multicol =
      DynamicTo<LayoutBlockFlow>(GetLayoutObjectByElementId("multicol"));
  ASSERT_TRUE(multicol);
  const LayoutObject* child1 = GetLayoutObjectByElementId("child1");
  ASSERT_TRUE(child1);
  const LayoutObject* child2 = GetLayoutObjectByElementId("child2");
  ASSERT_TRUE(child2);

  // The children are blocks. No need for an anonymous wrapper.
  EXPECT_EQ(multicol->FirstChild(), child1);
  EXPECT_EQ(multicol->FirstChild()->NextSibling(), child2);
  EXPECT_EQ(multicol->FirstChild()->NextSibling()->NextSibling(), nullptr);
}

}  // anonymous namespace
}  // namespace blink
