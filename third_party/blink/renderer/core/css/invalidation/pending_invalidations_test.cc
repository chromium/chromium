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
  GetDocument().body()->setInnerHTML(
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
  GetDocument().body()->setInnerHTML(R"HTML(
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

}  // namespace blink
