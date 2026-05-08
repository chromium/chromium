// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/invalidation/pending_invalidations.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class PendingInvalidationsTest : public testing::Test {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  StyleEngine& GetStyleEngine() { return GetDocument().GetStyleEngine(); }
  PendingInvalidations& GetPendingNodeInvalidations() {
    return GetDocument().GetStyleEngine().GetPendingNodeInvalidations();
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void PendingInvalidationsTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
}

TEST_F(PendingInvalidationsTest, ScheduleOnDocumentNode) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='d'></div><i id='i'></i><span></span>");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  scoped_refptr<DescendantInvalidationSet> set =
      DescendantInvalidationSet::Create();
  set->AddTagName(AtomicString("div"));
  set->AddTagName(AtomicString("span"));

  InvalidationLists lists;
  lists.descendants.push_back(set);
  GetPendingNodeInvalidations().ScheduleInvalidationSetsForNode(lists,
                                                                GetDocument());

  EXPECT_TRUE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());

  GetStyleEngine().InvalidateStyle();

  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleRecalc());
  EXPECT_TRUE(GetStyleEngine().NeedsStyleRecalc());

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(2u, after_count - before_count);
}

TEST_F(PendingInvalidationsTest, DescendantInvalidationOnDisplayNone) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #a { display: none }
      .a .b { color: green }
    </style>
    <div id="a">
      <div class="b"></div>
      <div class="b"></div>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // We skip scheduling descendant invalidations on display:none elements.
  GetDocument()
      .getElementById(AtomicString("a"))
      ->setAttribute(html_names::kClassAttr, AtomicString("a"));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

// Regression test for https://crbug.com/40257823.
// With a style rule using :not() and the subsequent-sibling combinator (~),
// adding and removing an element should not leave an orphaned entry in the
// pending invalidation map.
TEST_F(PendingInvalidationsTest, NoLeakForNotWithSubsequentSibling) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>:not(.x) ~ * { color: red; }</style>
    <div id="container"></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_TRUE(container);

  // Add a button as the only child (no nextSibling).
  Element* button = GetDocument().CreateRawElement(html_names::kButtonTag);
  container->AppendChild(button);

  // The pending invalidation map may have entries for the container (from
  // sibling invalidation being rescheduled as descendant invalidation). Flush
  // style to clear all pending state.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Remove the button.
  container->RemoveChild(button);

  // After removal, the button should not remain in the pending invalidation
  // map.
  EXPECT_TRUE(
      GetPendingNodeInvalidations().GetPendingInvalidationMap().find(button) ==
      GetPendingNodeInvalidations().GetPendingInvalidationMap().end());
}

// Verify that Nth sibling invalidation sets are still scheduled on a last child
// even when it has no nextSibling.
TEST_F(PendingInvalidationsTest, NthSetsScheduledOnLastSibling) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='parent'><div id='only'></div></div>");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  Element* only = GetDocument().getElementById(AtomicString("only"));
  ASSERT_TRUE(only);
  ASSERT_FALSE(only->nextSibling());

  // Create an NthSiblingInvalidationSet.
  scoped_refptr<NthSiblingInvalidationSet> nth_set =
      NthSiblingInvalidationSet::Create();

  InvalidationLists lists;
  lists.siblings.push_back(nth_set);
  GetPendingNodeInvalidations().ScheduleInvalidationSetsForNode(lists, *only);

  // The Nth set should have been scheduled and the node should be in the map.
  EXPECT_TRUE(only->NeedsStyleInvalidation());
  EXPECT_TRUE(
      GetPendingNodeInvalidations().GetPendingInvalidationMap().find(only) !=
      GetPendingNodeInvalidations().GetPendingInvalidationMap().end());
}

}  // namespace blink
