// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/invalidation/style_invalidator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class StyleInvalidatorTest : public testing::Test {
 protected:
  void SetUp() override {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(StyleInvalidatorTest, SkipDisplayNone) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="root">
      <div style="display:none">
        <div class="a"></div>
        <div class="a"></div>
      </div>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  PendingInvalidations pending;
  {
    InvalidationLists lists;
    scoped_refptr<InvalidationSet> set = DescendantInvalidationSet::Create();
    set->AddClass(AtomicString("a"));
    lists.descendants.push_back(set);
    pending.ScheduleInvalidationSetsForNode(
        lists, *GetDocument().getElementById(AtomicString("root")));
  }

  StyleInvalidator invalidator(pending.GetPendingInvalidationMap());
  invalidator.Invalidate(GetDocument(), GetDocument().body());

  EXPECT_FALSE(GetDocument().GetStyleEngine().NeedsStyleRecalc());
}

TEST_F(StyleInvalidatorTest, SkipDisplayNoneClearPendingNth) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="none" style="display:none">
      <div class="a"></div>
      <div class="a"></div>
    </div>
    <div id="descendant">
      <div class="a"></div>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  PendingInvalidations pending;
  {
    InvalidationLists lists;
    scoped_refptr<InvalidationSet> set = NthSiblingInvalidationSet::Create();
    set->AddClass(AtomicString("a"));
    lists.siblings.push_back(set);
    pending.ScheduleInvalidationSetsForNode(
        lists, *GetDocument().getElementById(AtomicString("none")));
  }
  {
    InvalidationLists lists;
    scoped_refptr<InvalidationSet> set = DescendantInvalidationSet::Create();
    set->AddClass(AtomicString("a"));
    lists.descendants.push_back(set);
    pending.ScheduleInvalidationSetsForNode(
        lists, *GetDocument().getElementById(AtomicString("descendant")));
  }

  StyleInvalidator invalidator(pending.GetPendingInvalidationMap());
  invalidator.Invalidate(GetDocument(), GetDocument().body());

  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("none"))
                   ->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("descendant"))
                  ->ChildNeedsStyleRecalc());
}

}  // namespace blink
