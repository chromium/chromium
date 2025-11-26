// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/overscroll/overscroll_area_tracker.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

class OverscrollAreaTrackerTest : public testing::Test,
                                  ScopedCSSOverscrollGesturesForTest {
 public:
  OverscrollAreaTrackerTest() : ScopedCSSOverscrollGesturesForTest(true) {}

  void SetUp() override {
    dummy_page_holder_ =
        std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  void SetInnerHTML(const char* html) {
    GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(html);
  }

  OverscrollAreaTracker* OverscrollAreaTrackerById(const char* id) {
    if (auto* element = GetDocument().getElementById(AtomicString(id))) {
      return element->OverscrollAreaTracker();
    }
    return nullptr;
  }

  using OverscrollMember = OverscrollAreaTracker::OverscrollMember;
  const VectorOf<OverscrollMember>& GetOverscrollMembers(
      OverscrollAreaTracker* tracker) {
    return tracker->overscroll_members_;
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(OverscrollAreaTrackerTest, OverscrollAreaCreated) {
  SetInnerHTML(R"HTML(
    <div id=one overscrollcontainer>
      <div id=two>
        <div id=three OverScrollContainer></div>
      </div>
    </div>
  )HTML");

  EXPECT_TRUE(OverscrollAreaTrackerById("one"));
  EXPECT_FALSE(OverscrollAreaTrackerById("two"));
  EXPECT_TRUE(OverscrollAreaTrackerById("three"));
}

TEST_F(OverscrollAreaTrackerTest, AddOverscrollAreaPopulatedManually) {
  SetInnerHTML(R"HTML(
    <div id="container" overscrollcontainer>
      <div id="menu"></div>
      <button id="button"></button>
    </div>
  )HTML");

  auto* area_tracker = OverscrollAreaTrackerById("container");
  ASSERT_TRUE(area_tracker);

  Element* menu = GetDocument().getElementById(AtomicString("menu"));
  Element* button = GetDocument().getElementById(AtomicString("button"));

  area_tracker->AddOverscroll(menu, button);

  const auto& members = GetOverscrollMembers(area_tracker);
  EXPECT_EQ(members.size(), 1u);
  EXPECT_EQ(members[0]->overscroll_element, menu);
  EXPECT_EQ(members[0]->activator, button);
}

}  // namespace blink
